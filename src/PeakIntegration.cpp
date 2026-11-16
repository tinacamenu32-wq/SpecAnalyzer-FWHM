#include "SpectralAnalyzer.h"
#include <cmath>
#include <numeric>
#include <QStringList>

namespace {
constexpr int maxComponents = 8;
struct Group { int first, last, left, right; };
struct Gaussian { double amplitude, center, sigma; };

// Exact positive area of a line segment, including its interpolated zero crossing.
double positiveSegment(double x0, double y0, double x1, double y1)
{
    const double dx = x1 - x0;
    if (y0 >= 0 && y1 >= 0) return .5 * (y0 + y1) * dx;
    if (y0 <= 0 && y1 <= 0) return 0;
    const double fraction = -y0 / (y1 - y0);
    return y0 > 0 ? .5 * y0 * dx * fraction : .5 * y1 * dx * (1 - fraction);
}

double baseline(const QVector<QPointF> &points, int left, int right, double x)
{
    const auto &a = points[left];
    const auto &b = points[right];
    return a.y() + (b.y() - a.y()) * (x - a.x()) / (b.x() - a.x());
}

// Integrate the same group-wide baseline over an arbitrary subinterval. Clip the
// data segment at a fractional cut before finding its zero crossing.
double integratePartition(const QVector<QPointF> &points, const Group &group,
                          double left, double right)
{
    double area = 0;
    for (int i=group.left; i<group.right; ++i) {
        const auto &a=points[i]; const auto &b=points[i+1];
        const double lo=std::max(left,a.x()), hi=std::min(right,b.x());
        if (hi<=lo) continue;
        const double h0=a.y()-baseline(points,group.left,group.right,a.x());
        const double h1=b.y()-baseline(points,group.left,group.right,b.x());
        const double y0=h0+(h1-h0)*(lo-a.x())/(b.x()-a.x());
        const double y1=h0+(h1-h0)*(hi-a.x())/(b.x()-a.x());
        area+=positiveSegment(lo,y0,hi,y1);
    }
    return area;
}

void approximateGroup(const QVector<QPointF> &points, const QVector<DetectedPeak> &peaks,
                      const Group &group, double noise, const QString &fitReason,
                      double fitError, const QVector<int> &order,
                      QVector<SpectralLineResult> &results)
{
    const int count=group.last-group.first+1;
    QVector<double> boundaries {points[group.left].x()};
    QVector<QString> methods;
    auto height=[&](int i) {
        return points[i].y()-baseline(points,group.left,group.right,points[i].x());
    };
    for(int k=group.first; k<group.last; ++k) {
        const int left=peaks[k].index, right=peaks[k+1].index;
        if (right<=left) {
            // Coincident samples do not establish two distinct peak positions.
            for(int j=group.first;j<=group.last;++j) {
                auto &r=results[order[j]];
                r.areaStatus=r.errorMsg=QStringLiteral("未可靠分离：重复峰位，无法切分估算");
            }
            return;
        }
        int valley=-1;
        const double depth=std::max(noise,std::max(0.0,std::min(height(left),height(right)))*.005);
        for(int i=left+1;i<right;++i) {
            const double h=height(i);
            if(h<=height(i-1) && h<=height(i+1) &&
               std::min(height(left),height(right))-h>depth &&
               (valley<0 || h<height(valley))) valley=i;
        }
        boundaries.append(valley>=0 ? points[valley].x()
                                   : .5*(points[left].x()+points[right].x()));
        methods.append(valley>=0 ? QStringLiteral("谷底切分") : QStringLiteral("中点切分"));
    }
    boundaries.append(points[group.right].x());
    QString reason=fitReason;
    if(reason.startsWith(QStringLiteral("未可靠分离："))) reason.remove(0,QStringLiteral("未可靠分离：").size());
    for(int k=0;k<count;++k) {
        auto &r=results[order[group.first+k]];
        r.fitted=false;
        r.approximate=true;
        r.fitRelativeError=fitError;
        r.integrationLeft=boundaries[k];r.integrationRight=boundaries[k+1];
        const double area=integratePartition(points,group,boundaries[k],boundaries[k+1]);
        r.valid=std::isfinite(area) && area>0;
        if(!r.valid) {
            r.areaStatus=r.errorMsg=QStringLiteral("近似切分后无有效正面积");
            continue;
        }
        r.computedValue=r.peakArea=area;
        r.errorMsg.clear();
        QStringList cuts;
        if(k>0) cuts.append(methods[k-1]);
        if(k<methods.size()) cuts.append(methods[k]);
        cuts.removeDuplicates();
        r.areaStatus=QStringLiteral("近似：%1；共用峰群基线；拟合未通过：%2")
                         .arg(cuts.join(QLatin1Char('/')),reason);
        if(r.truncated) r.areaStatus+=QStringLiteral("；触及数据边界，可能截断");
    }
}

double median(QVector<double> values)
{
    if (values.isEmpty()) return 0;
    const int mid = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + mid, values.end());
    return values[mid];
}

// Pivoted elimination for the small (<=24 unknowns) LM normal system.
bool solve(QVector<QVector<double>> a, QVector<double> rhs, QVector<double> &answer,
           double minimumPivotRatio = 1e-13)
{
    const int n = rhs.size();
    double largest = 0;
    for (const auto &row : a) for (double v : row) largest = std::max(largest, std::abs(v));
    if (!(largest > 0)) return false;
    for (int i = 0; i < n; ++i) {
        int pivot = i;
        for (int k = i + 1; k < n; ++k)
            if (std::abs(a[k][i]) > std::abs(a[pivot][i])) pivot = k;
        if (std::abs(a[pivot][i]) < largest * minimumPivotRatio) return false;
        a.swapItemsAt(i, pivot);
        rhs.swapItemsAt(i, pivot);
        for (int k = i + 1; k < n; ++k) {
            const double factor = a[k][i] / a[i][i];
            for (int j = i + 1; j < n; ++j) a[k][j] -= factor * a[i][j];
            rhs[k] -= factor * rhs[i];
        }
    }
    answer.fill(0, n);
    for (int i = n - 1; i >= 0; --i) {
        double value = rhs[i];
        for (int j = i + 1; j < n; ++j) value -= a[i][j] * answer[j];
        answer[i] = value / a[i][i];
        if (!std::isfinite(answer[i])) return false;
    }
    return true;
}

struct FitData {
    QVector<double> x, y, weights, seeds, lowCenter, highCenter;
    double minSigma;
};

void constrain(QVector<double> &params, const FitData &data)
{
    for (int k = 0; k < data.seeds.size(); ++k) {
        params[3*k] = std::clamp(params[3*k], std::log(1e-7), std::log(3.0));
        params[3*k+1] = std::clamp(params[3*k+1], data.lowCenter[k], data.highCenter[k]);
        params[3*k+2] = std::clamp(params[3*k+2], std::log(data.minSigma), std::log(.5));
    }
}

double evaluate(const QVector<double> &params, const FitData &data,
                QVector<QVector<double>> *normal = nullptr, QVector<double> *rhs = nullptr)
{
    const int dim = params.size();
    if (normal) normal->fill(QVector<double>(dim, 0), dim);
    if (rhs) rhs->fill(0, dim);
    double cost = 0;
    for (int i = 0; i < data.x.size(); ++i) {
        double prediction = 0;
        QVector<double> jac(dim, 0);
        for (int k = 0; k < dim / 3; ++k) {
            const double amp = std::exp(params[3*k]);
            const double sigma = std::exp(params[3*k+2]);
            const double distance = (data.x[i] - params[3*k+1]) / sigma;
            const double value = amp * std::exp(-.5 * distance * distance);
            prediction += value;
            jac[3*k] = value; // derivative wrt log amplitude
            jac[3*k+1] = value * distance / sigma;
            jac[3*k+2] = value * distance * distance;
        }
        const double residual = data.y[i] - prediction;
        const double weight = data.weights[i];
        cost += weight * residual * residual;
        if (!normal || !rhs) continue;
        for (int j = 0; j < dim; ++j) {
            (*rhs)[j] += weight * jac[j] * residual;
            for (int k = 0; k <= j; ++k) (*normal)[j][k] += weight * jac[j] * jac[k];
        }
    }
    if (normal) for (int j = 0; j < dim; ++j)
        for (int k = 0; k < j; ++k) (*normal)[k][j] = (*normal)[j][k];
    return cost;
}

bool fitGaussians(const QVector<QPointF> &points, const QVector<DetectedPeak> &peaks,
                  const Group &group, QVector<Gaussian> &components, double &relativeError,
                  QString &reason)
{
    const int count = group.last - group.first + 1;
    if (count > maxComponents || group.right - group.left + 1 < 5 * count) {
        reason = QStringLiteral("未可靠分离：峰数过多或采样点不足");
        return false;
    }
    const double origin = points[group.left].x();
    const double span = points[group.right].x() - origin;
    double scale = 0, minStep = span;
    FitData data;
    for (int i = group.left; i <= group.right; ++i) {
        data.x.append((points[i].x() - origin) / span);
        data.y.append(std::max(0.0, points[i].y() - baseline(points, group.left, group.right, points[i].x())));
        scale = std::max(scale, data.y.last());
        if (i > group.left) minStep = std::min(minStep, points[i].x() - points[i-1].x());
        const double dx = i == group.left ? points[i+1].x()-points[i].x()
                          : i == group.right ? points[i].x()-points[i-1].x()
                          : points[i+1].x()-points[i-1].x();
        data.weights.append(.5 * dx / span);
    }
    if (!(scale > 0)) { reason = QStringLiteral("局部基线以上无有效峰信号"); return false; }
    for (auto &y : data.y) y /= scale;
    data.minSigma = std::max(minStep / span * .75, 1e-6);
    if (data.minSigma >= .25) { reason = QStringLiteral("未可靠分离：采样过稀"); return false; }
    for (int k = group.first; k <= group.last; ++k)
        data.seeds.append((points[peaks[k].index].x() - origin) / span);
    for (int k = 0; k < count; ++k) {
        const double seed = data.seeds[k];
        const double leftGap = k ? seed-data.seeds[k-1] : (count > 1 ? data.seeds[1]-seed : 1);
        const double rightGap = k+1 < count ? data.seeds[k+1]-seed : leftGap;
        if (leftGap <= 0 || rightGap <= 0) { reason = QStringLiteral("未可靠分离：重复峰位"); return false; }
        // Permit a shoulder's actual component center to move away from its detected bump,
        // but never permit two component identities to exchange order.
        data.lowCenter.append(k ? .5*(data.seeds[k-1]+seed) : 0.0);
        data.highCenter.append(k+1<count ? .5*(seed+data.seeds[k+1]) : 1.0);
    }
    double bestCost = std::numeric_limits<double>::infinity();
    QVector<double> bestParams;
    bool bestConverged = false;
    for (double widthFactor : {.55, 1.0, 1.7}) {
        QVector<double> params;
        for (int k = 0; k < count; ++k) {
            const int index = peaks[group.first+k].index;
            const double amp = std::max(.01, (points[index].y()-baseline(points, group.left, group.right, points[index].x())) / scale);
            const double gap = k+1 < count ? data.seeds[k+1]-data.seeds[k] : data.seeds[k]-data.seeds[k-1];
            params << std::log(amp) << data.seeds[k] << std::log(std::max(data.minSigma * 2, gap * .55 * widthFactor));
        }
        constrain(params, data);
        double cost = evaluate(params, data), damping = .001;
        bool converged = false;
        for (int iteration = 0; iteration < 180; ++iteration) {
            QVector<QVector<double>> matrix;
            QVector<double> gradient, step;
            evaluate(params, data, &matrix, &gradient);
            double norm = 0;
            for (double v : gradient) norm = std::max(norm, std::abs(v));
            if (norm < 1e-9 || cost < 1e-14) { converged = true; break; }
            for (int j = 0; j < params.size(); ++j) matrix[j][j] += damping * std::max(matrix[j][j], 1e-7);
            if (!solve(matrix, gradient, step)) break;
            QVector<double> proposal = params;
            for (int j = 0; j < params.size(); ++j) proposal[j] += step[j];
            constrain(proposal, data);
            const double nextCost = evaluate(proposal, data);
            if (nextCost < cost) {
                const double improvement = cost - nextCost;
                params = proposal;
                cost = nextCost;
                damping = std::max(1e-10, damping * .3);
                if (improvement < 1e-12 * (1 + cost)) { converged = true; break; }
            } else {
                damping *= 8;
                if (damping > 1e12) break;
            }
        }
        if (converged && cost < bestCost) {
            bestCost = cost; bestParams = params; bestConverged = true;
        }
    }
    if (!bestConverged) { reason = QStringLiteral("未可靠分离：高斯联合拟合不收敛"); return false; }
    double signalEnergy = 0;
    for (int i=0; i<data.y.size(); ++i) signalEnergy += data.weights[i]*data.y[i]*data.y[i];
    relativeError = std::sqrt(bestCost / std::max(signalEnergy, 1e-15));
    if (relativeError > .05) {
        reason = QStringLiteral("未可靠分离：高斯峰形不匹配（相对RMS %1%）").arg(relativeError*100, 0, 'f', 1);
        return false;
    }
    // Reject nearly singular fits: a good summed curve alone does not establish
    // that the individual component areas can be identified.
    QVector<QVector<double>> normal;
    QVector<double> gradient, check;
    evaluate(bestParams, data, &normal, &gradient);
    if (!solve(normal, QVector<double>(normal.size(), 1), check, 1e-8)) {
        reason = QStringLiteral("未可靠分离：组分参数退化"); return false;
    }
    for (int k = 0; k < count; ++k) {
        Gaussian g {std::exp(bestParams[3*k])*scale,
                    origin + bestParams[3*k+1]*span,
                    std::exp(bestParams[3*k+2])*span};
        if (g.amplitude < scale * .005 || g.sigma <= minStep * .8 || g.sigma >= span * .49) {
            reason = QStringLiteral("未可靠分离：组分过弱或峰宽触及约束"); return false;
        }
        for (const auto &previous : components) {
            if (g.center - previous.center < .5 * std::min(g.sigma, previous.sigma)) {
                reason = QStringLiteral("未可靠分离：两个组分无法区分"); return false;
            }
        }
        components.append(g);
    }
    return true;
}
} // namespace

QVector<SpectralLineResult> SpectralAnalyzer::analyzePeaks(const QVector<QPointF> &points,
                                                          const QVector<DetectedPeak> &input)
{
    QVector<SpectralLineResult> results(input.size());
    if (input.isEmpty()) return results;
    for (const auto &p : points) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) {
            for (auto &r : results) r.errorMsg = r.areaStatus = QStringLiteral("无效数据：包含非有限数值");
            return results;
        }
    }
    for (int i = 1; i < points.size(); ++i) if (points[i].x() <= points[i-1].x()) {
        for (auto &r : results) r.errorMsg = r.areaStatus = QStringLiteral("无效数据：波长必须严格递增");
        return results;
    }
    QVector<int> order;
    for (int i = 0; i < input.size(); ++i) {
        if (input[i].index < 0 || input[i].index >= points.size()) {
            results[i].errorMsg = results[i].areaStatus = QStringLiteral("无效峰值索引");
        } else order.append(i);
    }
    if (order.isEmpty()) return results;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return input[a].index < input[b].index; });
    QVector<DetectedPeak> peaks;
    for (int index : order) {
        DetectedPeak p = input[index];
        p.wavelength = points[p.index].x(); p.intensity = points[p.index].y();
        peaks.append(p);
        results[index] = analyzePeakWidth(points, p);
        results[index].valid = false;
    }
    const int count = peaks.size(), n = points.size();
    if (n < 5) {
        for (auto &r : results) r.errorMsg = r.areaStatus = QStringLiteral("采样点不足，无法确定完整峰");
        return results;
    }
    QVector<int> split(count+1);
    split[0] = 0; split[count] = n-1;
    for (int k = 1; k < count; ++k) {
        int valley = peaks[k-1].index;
        for (int i = peaks[k-1].index; i <= peaks[k].index; ++i)
            if (points[i].y() < points[valley].y()) valley = i;
        split[k] = valley;
    }
    QVector<Group> groups;
    for (int k = 0; k < count; ++k) groups.append({k,k,split[k],split[k+1]});
    // Merge shallow-valley or shoulder neighbors before any display/export filter.
    bool changed = true;
    while (changed) {
        changed = false;
        for (int k = 0; k+1 < groups.size(); ++k) {
            const auto a = groups[k], b = groups[k+1];
            const int valley = split[b.first];
            if (a.left >= b.right) continue;
            const double av = points[valley].y()-baseline(points,a.left,b.right,points[valley].x());
            const auto &p = points[peaks[a.last].index];
            const auto &q = points[peaks[b.first].index];
            const double height = std::min(p.y()-baseline(points,a.left,b.right,p.x()), q.y()-baseline(points,a.left,b.right,q.x()));
            if (valley == peaks[a.last].index || valley == peaks[b.first].index || av > .1 * std::max(0.0,height)) {
                groups[k] = {a.first,b.last,a.left,b.right};
                groups.removeAt(k+1); changed = true; break;
            }
        }
    }
    for (auto group : groups) {
        if (group.left >= group.right) {
            for (int k=group.first;k<=group.last;++k)
                results[order[k]].areaStatus = results[order[k]].errorMsg = QStringLiteral("无法确定积分边界");
            continue;
        }
        const int initialLeft=group.left, initialRight=group.right;
        double height=0;
        for (int k=group.first;k<=group.last;++k) {
            const auto &p=points[peaks[k].index];
            height=std::max(height,p.y()-baseline(points,initialLeft,initialRight,p.x()));
        }
        // Robust estimate of point noise; baseline curvature/peaks occupy only a
        // minority of second differences in a normal spectrum.
        QVector<double> deviations;
        for (int i=initialLeft+1;i<initialRight;++i) {
            const double fraction=(points[i].x()-points[i-1].x())/(points[i+1].x()-points[i-1].x());
            deviations.append(std::abs(points[i].y()-(points[i-1].y()+(points[i+1].y()-points[i-1].y())*fraction)));
        }
        const double tailFraction = group.first == group.last ? .001 : .00005;
        const double noise = deviations.size() >= 31 ? median(deviations)/.826 : 0;
        const double tolerance=std::max(height*tailFraction, std::min(3*noise, height*.02));
        // Stop only after three consecutive near-background samples, never at a
        // lone noisy dip. Baseline is re-established through the chosen endpoints.
        int run=0;
        for (int i=peaks[group.first].index-1;i>=initialLeft;--i) {
            const double h=points[i].y()-baseline(points,initialLeft,initialRight,points[i].x());
            run = h <= tolerance ? run+1 : 0;
            if (run>=3) { group.left=i+2; break; }
        }
        run=0;
        for (int i=peaks[group.last].index+1;i<=initialRight;++i) {
            const double h=points[i].y()-baseline(points,initialLeft,initialRight,points[i].x());
            run = h <= tolerance ? run+1 : 0;
            if (run>=3) { group.right=i-2; break; }
        }
        const bool truncated = group.left==0 || group.right==n-1;
        for (int k=group.first;k<=group.last;++k) {
            auto &r=results[order[k]];
            r.integrationLeft=points[group.left].x(); r.integrationRight=points[group.right].x();
            r.truncated=truncated;
        }
        if (group.right-group.left<2 || height<=0) {
            for(int k=group.first;k<=group.last;++k)
                results[order[k]].errorMsg=results[order[k]].areaStatus=QStringLiteral("局部基线以上无足够有效数据");
            continue;
        }
        if (group.first==group.last) {
            double area=0;
            for(int i=group.left;i<group.right;++i) {
                const auto &a=points[i]; const auto &b=points[i+1];
                area+=positiveSegment(a.x(),a.y()-baseline(points,group.left,group.right,a.x()),
                                      b.x(),b.y()-baseline(points,group.left,group.right,b.x()));
            }
            auto &r=results[order[group.first]];
            r.computedValue=r.peakArea=area;
            r.valid=std::isfinite(area) && area>0;
            r.areaStatus=truncated ? QStringLiteral("净面积直接积分；触及数据边界，可能截断") : QStringLiteral("净面积直接积分");
            if (!r.valid) r.errorMsg=r.areaStatus=QStringLiteral("局部基线以上无有效峰面积");
        } else {
            QVector<Gaussian> components;
            double fitError=0;
            QString reason;
            const bool success=fitGaussians(points,peaks,group,components,fitError,reason);
            if (!success) {
                approximateGroup(points,peaks,group,noise,reason,fitError,order,results);
                continue;
            }
            for(int k=group.first;k<=group.last;++k) {
                auto &r=results[order[k]];
                r.fitted=true; r.fitRelativeError=fitError;
                const auto g=components[k-group.first];
                // Integrate the nonnegative fitted component over observed support;
                // do not silently extrapolate missing peak tails outside the data.
                const double root2=std::sqrt(2.0);
                const double area=g.amplitude*g.sigma*std::sqrt(std::acos(-1.0)/2.0)*
                    (std::erf((r.integrationRight-g.center)/(root2*g.sigma))-
                     std::erf((r.integrationLeft-g.center)/(root2*g.sigma)));
                r.computedValue=r.peakArea=area; r.valid=std::isfinite(area)&&area>0;
                r.areaStatus=QStringLiteral("%1峰高斯联合拟合；相对RMS %2%")
                    .arg(group.last-group.first+1).arg(fitError*100,0,'f',2);
                if (truncated) r.areaStatus+=QStringLiteral("；触及数据边界，可能截断");
            }
        }
    }
    return results;
}

SpectralLineResult SpectralAnalyzer::analyzePeak(const QVector<QPointF> &points,
                                               const DetectedPeak &peak)
{
    if (points.isEmpty() || peak.index<0 || peak.index>=points.size()) {
        SpectralLineResult r; r.errorMsg=r.areaStatus=QStringLiteral("无效峰值索引"); return r;
    }
    auto peaks=detectPeaks(points);
    int match=-1;
    for(int i=0;i<peaks.size();++i) if(peaks[i].index==peak.index) {match=i;break;}
    if(match<0) {match=peaks.size();peaks.append(peak);}
    return analyzePeaks(points,peaks)[match];
}
