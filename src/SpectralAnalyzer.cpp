#include "SpectralAnalyzer.h"
#include <cmath>
#include <limits>
#include <numeric>

// ========== Savitzky-Golay 平滑 ==========

/// 用高斯消元法解 Ax = b，返回 x
static QVector<double> solveLinearSystem(QVector<QVector<double>> A, QVector<double> b)
{
    const int n = A.size();
    for (int col = 0, row = 0; col < n && row < n; ++col) {
        // 选主元
        int pivot = row;
        for (int i = row + 1; i < n; ++i) {
            if (std::abs(A[i][col]) > std::abs(A[pivot][col]))
                pivot = i;
        }
        if (std::abs(A[pivot][col]) < 1e-12) continue;
        std::swap(A[row], A[pivot]);
        std::swap(b[row], b[pivot]);

        // 消元
        for (int i = row + 1; i < n; ++i) {
            double factor = A[i][col] / A[row][col];
            for (int j = col; j < n; ++j)
                A[i][j] -= factor * A[row][j];
            b[i] -= factor * b[row];
        }
        ++row;
    }

    // 回代
    QVector<double> x(n, 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double sum = b[i];
        for (int j = i + 1; j < n; ++j)
            sum -= A[i][j] * x[j];
        x[i] = (std::abs(A[i][i]) > 1e-12) ? sum / A[i][i] : 0.0;
    }
    return x;
}

static QVector<double> computeSGCoeffs(int windowSize, int polyOrder)
{
    if (windowSize % 2 == 0) windowSize += 1; // 确保奇数
    if (polyOrder >= windowSize) polyOrder = windowSize - 1;

    const int halfWin = windowSize / 2;
    const int nCols   = polyOrder + 1;

    // 构建设计矩阵 A: A[i][j] = (i - halfWin)^j
    QVector<QVector<double>> A(windowSize, QVector<double>(nCols));
    for (int i = 0; i < windowSize; ++i) {
        double x = i - halfWin;
        double xp = 1.0;
        for (int j = 0; j < nCols; ++j) {
            A[i][j] = xp;
            xp *= x;
        }
    }

    // 计算 A^T A (nCols × nCols)
    QVector<QVector<double>> ATA(nCols, QVector<double>(nCols, 0.0));
    for (int i = 0; i < nCols; ++i)
        for (int j = 0; j < nCols; ++j)
            for (int k = 0; k < windowSize; ++k)
                ATA[i][j] += A[k][i] * A[k][j];

    // 对于每个窗口位置，我们需要系数向量 c 使得 smoothed = sum(c_k * y_k)
    // c 是 A * (A^T A)^{-1} 的中心行（halfWin 行）
    // 等价于求解 (A^T A) c_raw = e0（只关心中心点）
    QVector<double> e0(nCols, 0.0);
    e0[0] = 1.0; // e0 = [1, 0, 0, ...], 提取第 0 阶系数（平滑值，非导数）

    QVector<double> rawCoeffs = solveLinearSystem(ATA, e0);

    // 完整系数: c[i] = rawCoeffs · A[i]  (A[i] 是第 i 行的设计向量)
    QVector<double> coeffs(windowSize);
    for (int i = 0; i < windowSize; ++i) {
        double sum = 0.0;
        for (int j = 0; j < nCols; ++j)
            sum += rawCoeffs[j] * A[i][j];
        coeffs[i] = sum;
    }
    return coeffs;
}

QVector<QPointF> SpectralAnalyzer::savitzkyGolay(const QVector<QPointF> &points,
                                                    const SGParams &params)
{
    if (!params.enabled || points.size() < params.windowSize)
        return points;

    const int n = points.size();

    // 只对 Y 值做平滑
    QVector<double> y(n);
    for (int i = 0; i < n; ++i)
        y[i] = points[i].y();

    // 计算中间的卷积系数
    QVector<double> coeffs = computeSGCoeffs(params.windowSize, params.polyOrder);
    const int halfWin = params.windowSize / 2;

    QVector<double> smoothed(n, 0.0);

    for (int i = 0; i < n; ++i) {
        if (i < halfWin || i >= n - halfWin) {
            // 边缘：用当前点附近的非对称窗口重新计算系数
            int localHalf = std::min({i, n - 1 - i, halfWin});
            int localSize = 2 * localHalf + 1;
            QVector<double> localCoeffs = computeSGCoeffs(localSize, std::min(params.polyOrder, localSize - 1));
            int localHalfW = localSize / 2;

            double sum = 0.0;
            for (int j = -localHalfW; j <= localHalfW; ++j)
                sum += localCoeffs[j + localHalfW] * y[i + j];
            smoothed[i] = sum;
        } else {
            // 中心区域：直接卷积
            double sum = 0.0;
            for (int j = -halfWin; j <= halfWin; ++j)
                sum += coeffs[j + halfWin] * y[i + j];
            smoothed[i] = sum;
        }
    }

    // 构建输出点（波长不变，强度取平滑值）
    QVector<QPointF> result(n);
    for (int i = 0; i < n; ++i)
        result[i] = QPointF(points[i].x(), smoothed[i]);

    return result;
}

// ========== ALS 基线扣除 ==========

/// 求解 pentadiagonal SPD 系统 Mx = b（带半宽 2 的 Cholesky）
/// a[i]=M[i][i-2], b[i]=M[i][i-1], c[i]=M[i][i], M symmetric
static QVector<double> solvePenta(const QVector<double> &a,
                                   const QVector<double> &b,
                                   const QVector<double> &c,
                                   const QVector<double> &rhs)
{
    const int n = c.size();
    // 存储 L 因子: l2[i] = L[i][i-2], l1[i] = L[i][i-1], d[i] = D[i]
    QVector<double> l2(n, 0.0), l1(n, 0.0), d(n, 0.0);

    // Cholesky 分解 L D L^T
    for (int i = 0; i < n; ++i) {
        double cbar = c[i];

        if (i >= 1)
            cbar -= l1[i] * l1[i] * d[i - 1];
        if (i >= 2)
            cbar -= l2[i] * l2[i] * d[i - 2];

        if (cbar <= 0.0) cbar = 1e-12;
        d[i] = cbar;

        // L[i+1][i]
        if (i + 1 < n) {
            double val = b[i + 1];
            if (i >= 1)
                val -= l2[i + 1] * l1[i] * d[i - 1];
            l1[i + 1] = val / d[i];
        }
        // L[i+2][i]
        if (i + 2 < n) {
            l2[i + 2] = a[i + 2] / d[i];
        }
    }

    // 前代 L y = rhs
    QVector<double> y(n);
    for (int i = 0; i < n; ++i) {
        double sum = rhs[i];
        if (i >= 1) sum -= l1[i] * y[i - 1];
        if (i >= 2) sum -= l2[i] * y[i - 2];
        y[i] = sum;
    }

    // 缩放 D z = y 和回代 L^T x = z
    QVector<double> x(n);
    for (int i = n - 1; i >= 0; --i) {
        double zi = y[i] / d[i];
        if (i + 1 < n) zi -= l1[i + 1] * x[i + 1];
        if (i + 2 < n) zi -= l2[i + 2] * x[i + 2];
        x[i] = zi;
    }

    return x;
}

QVector<QPointF> SpectralAnalyzer::computeBaseline(const QVector<QPointF> &points,
                                                      const BLParams &params)
{
    const int n = points.size();
    QVector<QPointF> baseline(n);

    if (n < 5) {
        for (int i = 0; i < n; ++i)
            baseline[i] = QPointF(points[i].x(), points[i].y());
        return baseline;
    }

    // 提取 Y 值
    QVector<double> y(n);
    for (int i = 0; i < n; ++i)
        y[i] = points[i].y();

    // 构建 D^T D 的固定部分（不随迭代改变）
    // 对五对角矩阵，只存下三角（子对角线）:
    // a[i] = M[i][i-2] (i≥2), b[i] = M[i][i-1] (i≥1), c[i] = M[i][i]
    QVector<double> a_dtd(n, 0.0), b_dtd(n, 0.0), c_dtd(n, 0.0);

    const double lam = params.lambda;

    // Row 0: D^T D[0] = [1, -2, 1]
    c_dtd[0] = lam;
    // b_dtd[0] = 0 (no subdiagonal)

    // Row 1: D^T D[1] = [-2, 5, -4, 1]
    b_dtd[1] = -2.0 * lam;
    c_dtd[1] =  5.0 * lam;
    // a_dtd[1] = 0

    // Interior rows 2..n-3: D^T D[i] = [1, -4, 6, -4, 1]
    for (int i = 2; i < n - 2; ++i) {
        a_dtd[i] = lam;
        b_dtd[i] = -4.0 * lam;
        c_dtd[i] =  6.0 * lam;
    }

    // Row n-2: D^T D[n-2] = [1, -4, 5, -2]
    if (n > 2) {
        a_dtd[n - 2] = lam;
        b_dtd[n - 2] = -4.0 * lam;
        c_dtd[n - 2] =  5.0 * lam;
    }

    // Row n-1: D^T D[n-1] = [1, -2, 1]
    if (n > 1) {
        a_dtd[n - 1] = lam;
        b_dtd[n - 1] = -2.0 * lam;
        c_dtd[n - 1] = lam;
    }

    // 初始基线 = 数据的最小二乘平滑近似（y 本身）
    QVector<double> z = y;

    // ALS 迭代
    for (int iter = 0; iter < params.niter; ++iter) {
        QVector<double> a_m(n), b_m(n), c_m(n), rhs(n);

        for (int i = 0; i < n; ++i) {
            double w = (y[i] > z[i]) ? params.p : (1.0 - params.p);
            rhs[i] = w * y[i];
            c_m[i] = w + c_dtd[i];
            a_m[i] = a_dtd[i];  // 对 i<2 天然为 0
            b_m[i] = b_dtd[i];  // 对 i<1 天然为 0
        }

        z = solvePenta(a_m, b_m, c_m, rhs);
    }

    for (int i = 0; i < n; ++i)
        baseline[i] = QPointF(points[i].x(), z[i]);

    return baseline;
}

QVector<QPointF> SpectralAnalyzer::subtractBaseline(const QVector<QPointF> &points,
                                                       const BLParams &params)
{
    if (!params.enabled || points.isEmpty())
        return points;

    QVector<QPointF> baseline = computeBaseline(points, params);
    QVector<QPointF> result(points.size());
    for (int i = 0; i < points.size(); ++i)
        result[i] = QPointF(points[i].x(), points[i].y() - baseline[i].y());

    return result;
}

// ========== 预定义谱线 ==========

const QVector<SpectralAnalyzer::LineDef> &SpectralAnalyzer::predefinedLines()
{
    static const QVector<LineDef> lines = {
        {"Na  589.0 nm", 589.0},
        {"Na  589.6 nm", 589.6},
        {"K   766.5 nm", 766.5},
        {"K   769.9 nm", 769.9},
        {"Ca  393.4 nm", 393.4},
        {"Ca  396.8 nm", 396.8},
        {"Ca  422.7 nm", 422.7},
    };
    return lines;
}

// ========== 自动峰值检测 ==========

QVector<DetectedPeak> SpectralAnalyzer::detectPeaks(const QVector<QPointF> &points,
                                                      double minProminence,
                                                      int neighborhoodSize)
{
    QVector<DetectedPeak> peaks;
    const int n = points.size();
    if (n < 2 * neighborhoodSize + 1) return peaks;

    // ---- 自动计算最小显著度阈值 ----
    if (minProminence <= 0.0) {
        double maxInt = 0.0;
        for (const auto &p : points) {
            if (p.y() > maxInt) maxInt = p.y();
        }
        // 默认：最大强度的 2%
        minProminence = maxInt * 0.02;
        if (minProminence < 1e-12) minProminence = 1e-12;
    }

    // ---- 第 1 步：找出所有局部极大值 ----
    for (int i = neighborhoodSize; i < n - neighborhoodSize; ++i) {
        double yi = points[i].y();
        bool isMax = true;
        for (int j = i - neighborhoodSize; j <= i + neighborhoodSize; ++j) {
            if (j == i) continue;
            if (points[j].y() >= yi) {
                isMax = false;
                break;
            }
        }
        if (isMax) {
            DetectedPeak p;
            p.wavelength = points[i].x();
            p.intensity  = yi;
            p.prominence = 0.0;
            p.index      = i;
            peaks.append(p);
        }
    }

    if (peaks.isEmpty()) return peaks;

    // ---- 第 2 步：计算每个峰的 Topographic Prominence ----
    // 先按索引排序
    std::sort(peaks.begin(), peaks.end(),
              [](const DetectedPeak &a, const DetectedPeak &b) { return a.index < b.index; });

    for (int pi = 0; pi < peaks.size(); ++pi) {
        DetectedPeak &peak = peaks[pi];

        // 左侧边界：左边第一个比当前峰更高的峰，或数据起点
        int leftBoundary = 0;
        for (int j = pi - 1; j >= 0; --j) {
            if (peaks[j].intensity > peak.intensity) {
                leftBoundary = peaks[j].index;
                break;
            }
        }

        // 右侧边界：右边第一个比当前峰更高的峰，或数据终点
        int rightBoundary = n - 1;
        for (int j = pi + 1; j < peaks.size(); ++j) {
            if (peaks[j].intensity > peak.intensity) {
                rightBoundary = peaks[j].index;
                break;
            }
        }

        peak.prominence = computeProminence(points, peak.index, leftBoundary, rightBoundary);
    }

    // ---- 第 3 步：按显著度过滤 ----
    QVector<DetectedPeak> filtered;
    for (const auto &p : peaks) {
        if (p.prominence >= minProminence) {
            filtered.append(p);
        }
    }

    // ---- 第 4 步：按波长升序排列 ----
    std::sort(filtered.begin(), filtered.end(),
              [](const DetectedPeak &a, const DetectedPeak &b) { return a.wavelength < b.wavelength; });

    return filtered;
}

double SpectralAnalyzer::computeProminence(const QVector<QPointF> &points,
                                            int peakIdx,
                                            int leftBoundary,
                                            int rightBoundary)
{
    double leftMin  = points[peakIdx].y();
    double rightMin = points[peakIdx].y();

    for (int i = leftBoundary; i <= peakIdx; ++i) {
        if (points[i].y() < leftMin) leftMin = points[i].y();
    }
    for (int i = peakIdx; i <= rightBoundary; ++i) {
        if (points[i].y() < rightMin) rightMin = points[i].y();
    }

    double referenceHeight = std::max(leftMin, rightMin);
    return points[peakIdx].y() - referenceHeight;
}

// ========== 对检测到的峰计算 FWHM ==========

void SpectralAnalyzer::findValleyBounds(const QVector<QPointF> &points,
                                         int peakIdx,
                                         int &leftBound,
                                         int &rightBound)
{
    const int n = points.size();
    leftBound  = 0;
    rightBound = n - 1;

    // 向左找谷底
    for (int i = peakIdx - 1; i > 1; --i) {
        if (points[i].y() <= points[i - 1].y() && points[i].y() <= points[i + 1].y()) {
            leftBound = i;
            break;
        }
    }

    // 向右找谷底
    for (int i = peakIdx + 1; i < n - 1; ++i) {
        if (points[i].y() <= points[i - 1].y() && points[i].y() <= points[i + 1].y()) {
            rightBound = i;
            break;
        }
    }
}

SpectralLineResult SpectralAnalyzer::analyzePeak(const QVector<QPointF> &points,
                                                   const DetectedPeak &peak)
{
    SpectralLineResult result;
    result.label         = QString("%1 nm").arg(peak.wavelength, 0, 'f', 2);
    result.targetWl      = peak.wavelength;
    result.foundPeakWl   = peak.wavelength;
    result.peakIntensity = peak.intensity;
    result.halfMax       = peak.intensity / 2.0;

    if (points.isEmpty() || peak.index < 0 || peak.index >= points.size()) {
        result.errorMsg = QStringLiteral("无效的峰值索引");
        return result;
    }

    // 找到峰两侧的谷底边界
    int leftBound, rightBound;
    findValleyBounds(points, peak.index, leftBound, rightBound);

    // 在谷底范围内搜索半峰穿越点
    double leftWl  = findHalfCrossing(points, leftBound, peak.index,     result.halfMax, -1);
    double rightWl = findHalfCrossing(points, peak.index,   rightBound,  result.halfMax,  1);

    // 找不到半峰穿越点时，用谷底横坐标兜底
    if (leftWl < 0)
        leftWl = points[leftBound].x();
    if (rightWl < 0)
        rightWl = points[rightBound].x();

    result.fwhm          = rightWl - leftWl;
    result.computedValue = result.fwhm * result.halfMax;  // FWHM × (peak/2)
    result.valid         = true;

    return result;
}

// ========== 兼容旧接口：在目标波长窗口内分析 ==========

SpectralLineResult SpectralAnalyzer::analyze(const QVector<QPointF> &points,
                                              double targetWl,
                                              const QString &label,
                                              double windowNm)
{
    SpectralLineResult result;
    result.label = label;
    result.targetWl = targetWl;

    if (points.isEmpty()) {
        result.errorMsg = QStringLiteral("光谱数据为空");
        return result;
    }

    // 1. 收集窗口内的点 (wavelength in [targetWl - window, targetWl + window])
    int startIdx = -1, endIdx = -1;
    for (int i = 0; i < points.size(); ++i) {
        double wl = points[i].x();
        if (wl >= targetWl - windowNm && wl <= targetWl + windowNm) {
            if (startIdx < 0) startIdx = i;
            endIdx = i;
        } else if (wl > targetWl + windowNm) {
            break; // 已超出窗口
        }
    }

    if (startIdx < 0 || endIdx < 0 || startIdx >= endIdx) {
        result.errorMsg = QString("波长 %1 nm 附近 (±%2 nm) 未找到足够数据点")
                              .arg(targetWl).arg(windowNm);
        return result;
    }

    // 2. 找窗口内的最大值（峰值）
    int peakIdx = startIdx;
    double peakInt = points[startIdx].y();
    for (int i = startIdx; i <= endIdx; ++i) {
        if (points[i].y() > peakInt) {
            peakInt = points[i].y();
            peakIdx = i;
        }
    }

    result.foundPeakWl   = points[peakIdx].x();
    result.peakIntensity = peakInt;
    result.halfMax       = peakInt / 2.0;

    // 3. 找半峰全宽 (FWHM)
    double leftWl = findHalfCrossing(points, startIdx, peakIdx, result.halfMax, -1);
    double rightWl = findHalfCrossing(points, peakIdx, endIdx, result.halfMax, 1);

    if (leftWl < 0 || rightWl < 0) {
        result.errorMsg = QString("波长 %1 nm 处的峰无法计算半峰宽（基线可能过高或峰太窄）")
                              .arg(targetWl);
        return result;
    }

    result.fwhm = rightWl - leftWl;
    result.computedValue = result.fwhm * result.halfMax; // FWHM × (peak/2)
    result.valid = true;

    return result;
}

// ========== 半峰穿越点查找 ==========

double SpectralAnalyzer::findHalfCrossing(const QVector<QPointF> &points,
                                           int startIdx, int endIdx,
                                           double halfValue,
                                           int direction)
{
    if (direction < 0) {
        // 向左：从 endIdx 向 startIdx 搜索
        for (int i = endIdx; i > startIdx; --i) {
            double y1 = points[i - 1].y();
            double y2 = points[i].y();
            if ((y1 <= halfValue && y2 >= halfValue) ||
                (y1 >= halfValue && y2 <= halfValue)) {
                double x1 = points[i - 1].x();
                double x2 = points[i].x();
                if (std::abs(y2 - y1) < 1e-12)
                    return x1;
                return x1 + (x2 - x1) * (halfValue - y1) / (y2 - y1);
            }
        }
    } else {
        // 向右：从 startIdx 向 endIdx 搜索
        for (int i = startIdx; i < endIdx; ++i) {
            double y1 = points[i].y();
            double y2 = points[i + 1].y();
            if ((y1 <= halfValue && y2 >= halfValue) ||
                (y1 >= halfValue && y2 <= halfValue)) {
                double x1 = points[i].x();
                double x2 = points[i + 1].x();
                if (std::abs(y2 - y1) < 1e-12)
                    return x1;
                return x1 + (x2 - x1) * (halfValue - y1) / (y2 - y1);
            }
        }
    }

    return -1.0; // 未找到
}
