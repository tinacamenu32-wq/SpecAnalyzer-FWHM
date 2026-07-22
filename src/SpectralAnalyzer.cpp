#include "SpectralAnalyzer.h"
#include <cmath>
#include <limits>
#include <numeric>

// ========== Savitzky-Golay 平滑 ==========

// 前向声明
static QVector<double> computeSGCoeffsDeriv(int windowSize, int polyOrder, int derivativeOrder);

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
    return computeSGCoeffsDeriv(windowSize, polyOrder, 0);
}

/// 通用 SG 系数：derivativeOrder=0 平滑, =1 一阶导数, =2 二阶导数
static QVector<double> computeSGCoeffsDeriv(int windowSize, int polyOrder, int derivativeOrder)
{
    if (windowSize % 2 == 0) windowSize += 1;
    if (polyOrder >= windowSize) polyOrder = windowSize - 1;
    if (derivativeOrder > polyOrder) derivativeOrder = polyOrder;

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

    // e_d: 提取第 d 阶导数（e_d[d] = d!, 其余为 0）
    QVector<double> ed(nCols, 0.0);
    ed[derivativeOrder] = 1.0;
    // 乘以 d!
    for (int k = 2; k <= derivativeOrder; ++k)
        ed[derivativeOrder] *= k;

    QVector<double> rawCoeffs = solveLinearSystem(ATA, ed);

    // 完整系数: c[i] = rawCoeffs · A[i]
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

// ========== SG 导数计算 ==========

QVector<double> SpectralAnalyzer::computeSGDerivative(const QVector<QPointF> &points,
                                                        int windowSize,
                                                        int polyOrder,
                                                        int derivativeOrder)
{
    const int n = points.size();
    QVector<double> deriv(n, 0.0);

    if (n < windowSize || derivativeOrder < 0 || derivativeOrder > 2)
        return deriv;

    // 计算导数系数
    QVector<double> coeffs = computeSGCoeffsDeriv(windowSize, polyOrder, derivativeOrder);
    const int halfWin = windowSize / 2;

    // 提取 Y 值
    QVector<double> y(n);
    for (int i = 0; i < n; ++i)
        y[i] = points[i].y();

    for (int i = 0; i < n; ++i) {
        if (i < halfWin || i >= n - halfWin) {
            // 边缘：用非对称窗口
            int localHalf = std::min({i, n - 1 - i, halfWin});
            int localSize = 2 * localHalf + 1;
            QVector<double> localCoeffs = computeSGCoeffsDeriv(
                localSize, std::min(polyOrder, localSize - 1), derivativeOrder);
            int localHalfW = localSize / 2;

            double sum = 0.0;
            for (int j = -localHalfW; j <= localHalfW; ++j)
                sum += localCoeffs[j + localHalfW] * y[i + j];
            deriv[i] = sum;
        } else {
            // 中心区域：直接卷积
            double sum = 0.0;
            for (int j = -halfWin; j <= halfWin; ++j)
                sum += coeffs[j + halfWin] * y[i + j];
            deriv[i] = sum;
        }
    }

    return deriv;
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

// ========== 二阶导数法峰值检测（参照 Origin Peak Analyzer）==========

QVector<DetectedPeak> SpectralAnalyzer::detectPeaks2ndDeriv(const QVector<QPointF> &points,
                                                               double minHeight,
                                                               int smoothWindow,
                                                               int polyOrder)
{
    QVector<DetectedPeak> peaks;
    const int n = points.size();
    if (n < smoothWindow + 3) return peaks;

    // ---- 自动最小峰高 ----
    if (minHeight <= 0.0) {
        double maxInt = 0.0;
        for (const auto &p : points) {
            if (p.y() > maxInt) maxInt = p.y();
        }
        minHeight = maxInt * 0.01; // 默认最大强度的 1%
        if (minHeight < 1e-12) minHeight = 1e-12;
    }

    // ---- 第 1 步：计算二阶导数 ----
    QVector<double> d2y = computeSGDerivative(points, smoothWindow, polyOrder, 2);

    // ---- 第 2 步：找到二阶导数为负的连续区域 ----
    struct NegRegion {
        int start, end;   // [start, end] 闭区间
        int minIdx;       // d2y 最小值位置
        double minD2Y;    // d2y 最小值
    };
    QVector<NegRegion> regions;

    int i = 0;
    while (i < n) {
        // 跳过非负区域
        while (i < n && d2y[i] >= 0.0) ++i;
        if (i >= n) break;

        int regionStart = i;
        double minD2 = d2y[i];
        int minIdx = i;

        // 扫描整个负区域
        while (i < n && d2y[i] < 0.0) {
            if (d2y[i] < minD2) {
                minD2 = d2y[i];
                minIdx = i;
            }
            ++i;
        }
        int regionEnd = i - 1;

        // 负区域至少 2 个连续点，且二阶导数足够负（排除噪声）
        if (regionEnd - regionStart >= 1 && minD2 < -10.0) {
            regions.append({regionStart, regionEnd, minIdx, minD2});
        }
    }

    if (regions.isEmpty()) return peaks;

    // ---- 第 3 步：在每个负区域内找峰 ----
    // 二阶导数最小值位置附近找原始数据的局部最大值
    const int searchHalf = smoothWindow / 2;

    for (const auto &region : regions) {
        int center = region.minIdx;

        // 在 center ± searchHalf 范围内找原始数据最高点
        int searchStart = std::max(0, center - searchHalf);
        int searchEnd   = std::min(n - 1, center + searchHalf);
        int peakIdx = searchStart;
        double peakY = points[searchStart].y();
        for (int j = searchStart; j <= searchEnd; ++j) {
            if (points[j].y() > peakY) {
                peakY = points[j].y();
                peakIdx = j;
            }
        }

        // 峰高不够则跳过
        if (peakY < minHeight) continue;

        // 确保是局部极大值（在 ±searchHalf 内）
        bool isLocalMax = true;
        for (int j = std::max(0, peakIdx - searchHalf);
             j <= std::min(n - 1, peakIdx + searchHalf); ++j) {
            if (j == peakIdx) continue;
            if (points[j].y() >= peakY) {
                isLocalMax = false;
                break;
            }
        }
        // 如果不是局部极大值，说明这是肩峰——仍保留（这正是一阶导数法的优势）
        // 但需要确保峰位置确实在负区域内
        if (peakIdx < region.start || peakIdx > region.end) {
            // 峰不在负区域内，跳过
            if (!isLocalMax) continue;
        }

        DetectedPeak p;
        p.wavelength = points[peakIdx].x();
        p.intensity  = peakY;
        p.prominence = 0.0; // 稍后计算
        p.index      = peakIdx;
        p.from2ndDeriv = true;
        peaks.append(p);
    }

    // ---- 第 4 步：去重（合并距离太近的峰）----
    if (peaks.size() > 1) {
        std::sort(peaks.begin(), peaks.end(),
                  [](const DetectedPeak &a, const DetectedPeak &b) { return a.index < b.index; });

        QVector<DetectedPeak> deduped;
        deduped.append(peaks[0]);
        for (int k = 1; k < peaks.size(); ++k) {
            const auto &prev = deduped.last();
            const auto &curr = peaks[k];
            // 如果和上一个峰距离太近（< 邻域窗口），保留强度高的
            if (curr.index - prev.index <= searchHalf) {
                if (curr.intensity > prev.intensity) {
                    deduped.last() = curr;
                }
            } else {
                deduped.append(curr);
            }
        }
        peaks = deduped;
    }

    return peaks;
}

// ========== 自动峰值检测（综合局部极大值 + 二阶导数法）==========

QVector<DetectedPeak> SpectralAnalyzer::detectPeaks(const QVector<QPointF> &points,
                                                      double minProminence,
                                                      int neighborhoodSize)
{
    const int n = points.size();
    if (n < 2 * neighborhoodSize + 1) return QVector<DetectedPeak>();

    // ---- 自动计算最小显著度阈值 ----
    double maxInt = 0.0;
    for (const auto &p : points) {
        if (p.y() > maxInt) maxInt = p.y();
    }
    if (minProminence <= 0.0) {
        minProminence = maxInt * 0.02;
        if (minProminence < 1e-12) minProminence = 1e-12;
    }

    // ===== 方法 1：局部极大值法 =====
    QVector<DetectedPeak> peaksLocal;
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
            p.from2ndDeriv = false;
            peaksLocal.append(p);
        }
    }

    // ===== 方法 2：二阶导数法（参照 Origin） =====
    // SG 窗口固定 7（与 Origin 默认一致），不受 neighborhoodSize 影响
    int sgWindow = 7;
    double minHeight2nd = maxInt * 0.005; // 更低阈值，捕捉肩峰
    QVector<DetectedPeak> peaks2nd = detectPeaks2ndDeriv(points, minHeight2nd, sgWindow, 2);

    // ===== 合并两个结果集 =====
    QVector<DetectedPeak> peaks;
    // 先用局部极大值的结果
    for (const auto &p : peaksLocal)
        peaks.append(p);

    // 添加二阶导数法找到的新峰（去重）
    // 使用波长容差而非索引容差，避免不同采样密度下的问题
    const double wlTolerance = 0.2; // nm，两个峰被视为同一峰的最大波长差
    for (const auto &p2 : peaks2nd) {
        bool isDuplicate = false;
        for (const auto &p1 : peaksLocal) {
            if (std::abs(p2.wavelength - p1.wavelength) <= wlTolerance) {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate) {
            peaks.append(p2);
        }
    }

    if (peaks.isEmpty()) return peaks;

    // ---- 计算每个峰的 Topographic Prominence ----
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

    // ---- 按显著度过滤（二阶导数峰额外用局部基线高度判断，避免漏掉肩峰）----
    double minProminence2nd   = minProminence * 0.25;
    double minHeightAboveBL   = maxInt * 0.005; // 肩峰：基线以上高度 > 0.5% 最大强度
    QVector<DetectedPeak> filtered;
    for (auto &p : peaks) {
        if (p.from2ndDeriv) {
            // 二阶导数峰：先计算局部基线高度
            int lb, rb;
            findValleyBounds(points, p.index, lb, rb);
            double blY = computeLocalBaseline(points, lb, rb, p.wavelength);
            double hAboveBL = p.intensity - blY;
            // 通过条件：prominence 达标 或 局部基线以上高度达标
            if (p.prominence >= minProminence2nd || hAboveBL >= minHeightAboveBL) {
                p.localBaselineY = blY;
                filtered.append(p);
            }
        } else {
            if (p.prominence >= minProminence) {
                filtered.append(p);
            }
        }
    }

    // ---- 按波长升序排列 ----
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

// ========== 局部基线 & 谷底查找 ==========

double SpectralAnalyzer::computeLocalBaseline(const QVector<QPointF> &points,
                                                int leftIdx,
                                                int rightIdx,
                                                double targetX)
{
    if (leftIdx < 0 || rightIdx >= points.size() || leftIdx >= rightIdx)
        return 0.0;

    double x1 = points[leftIdx].x();
    double y1 = points[leftIdx].y();
    double x2 = points[rightIdx].x();
    double y2 = points[rightIdx].y();

    if (std::abs(x2 - x1) < 1e-12)
        return y1;

    // 线性插值
    return y1 + (y2 - y1) * (targetX - x1) / (x2 - x1);
}

// ========== 对检测到的峰计算 FWHM ==========

void SpectralAnalyzer::findValleyBounds(const QVector<QPointF> &points,
                                         int peakIdx,
                                         int &leftBound,
                                         int &rightBound)
{
    const int n = points.size();
    double peakY = points[peakIdx].y();
    leftBound  = 0;
    rightBound = n - 1;

    // 向左找谷底：要求谷底深度至少为峰高的 10%（跳过微小波动）
    const double minDepth = peakY * 0.10;
    double bestLeftY = peakY;
    int bestLeftIdx = 0;
    for (int i = peakIdx - 1; i > 1; --i) {
        if (points[i].y() <= points[i - 1].y() && points[i].y() <= points[i + 1].y()) {
            // 检查深度是否够
            if (peakY - points[i].y() >= minDepth) {
                leftBound = i;
                break;
            }
            // 记录最深的谷
            if (points[i].y() < bestLeftY) {
                bestLeftY = points[i].y();
                bestLeftIdx = i;
            }
        }
    }
    // 如果没找到够深的谷，用搜索范围内最低点
    if (leftBound == 0 && bestLeftIdx > 0) {
        leftBound = bestLeftIdx;
    }

    // 向右找谷底：同样要求深度至少为峰高的 10%
    double bestRightY = peakY;
    int bestRightIdx = n - 1;
    for (int i = peakIdx + 1; i < n - 1; ++i) {
        if (points[i].y() <= points[i - 1].y() && points[i].y() <= points[i + 1].y()) {
            if (peakY - points[i].y() >= minDepth) {
                rightBound = i;
                break;
            }
            if (points[i].y() < bestRightY) {
                bestRightY = points[i].y();
                bestRightIdx = i;
            }
        }
    }
    if (rightBound == n - 1 && bestRightIdx < n - 1) {
        rightBound = bestRightIdx;
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

    if (points.isEmpty() || peak.index < 0 || peak.index >= points.size()) {
        result.errorMsg = QStringLiteral("无效的峰值索引");
        return result;
    }

    // ---- 1. 找到峰两侧的谷底边界 ----
    int leftBound, rightBound;
    findValleyBounds(points, peak.index, leftBound, rightBound);

    // ---- 2. 计算局部基线 ----
    double baselineAtPeak = computeLocalBaseline(points, leftBound, rightBound, peak.wavelength);
    double heightAboveBL = peak.intensity - baselineAtPeak;

    // 如果基线以上高度为负（异常情况），回退到 y=0 基线
    if (heightAboveBL <= 0.0) {
        heightAboveBL = peak.intensity;
        baselineAtPeak = 0.0;
        result.halfMax = peak.intensity / 2.0;
    } else {
        // 半峰高 = 基线 + 基线以上高度/2（参照 Origin 的 local baseline 方法）
        result.halfMax = baselineAtPeak + heightAboveBL / 2.0;
    }

    result.heightAboveBaseline = heightAboveBL;

    // ---- 3. 在谷底范围内搜索半峰穿越点 ----
    double leftWl  = findHalfCrossing(points, leftBound, peak.index,    result.halfMax, -1);
    double rightWl = findHalfCrossing(points, peak.index,  rightBound, result.halfMax,  1);

    // 找不到半峰穿越点时用谷底横坐标兜底
    if (leftWl < 0)
        leftWl = points[leftBound].x();
    if (rightWl < 0)
        rightWl = points[rightBound].x();

    result.fwhm          = rightWl - leftWl;
    result.computedValue = result.fwhm * (heightAboveBL / 2.0); // FWHM × 基线以上半高

    // ---- 4. 计算峰面积（梯形积分，仅积分局部基线以上的部分） ----
    double area = 0.0;
    for (int i = leftBound; i < rightBound; ++i) {
        // 对每个小区间，计算局部基线以上的高度
        double xLeft = points[i].x();
        double blLeft = computeLocalBaseline(points, leftBound, rightBound, xLeft);
        double hLeft = points[i].y() - blLeft;
        if (hLeft < 0.0) hLeft = 0.0;

        double xRight = points[i + 1].x();
        double blRight = computeLocalBaseline(points, leftBound, rightBound, xRight);
        double hRight = points[i + 1].y() - blRight;
        if (hRight < 0.0) hRight = 0.0;

        // 梯形面积 = (hLeft + hRight) * dx / 2
        area += (hLeft + hRight) * (xRight - xLeft) / 2.0;
    }
    result.peakArea = area;
    result.valid    = true;

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

    // 2.5 找谷底并计算局部基线
    int leftValley = startIdx, rightValley = endIdx;
    for (int i = peakIdx - 1; i > startIdx + 1; --i) {
        if (points[i].y() <= points[i - 1].y() && points[i].y() <= points[i + 1].y()) {
            leftValley = i;
            break;
        }
    }
    for (int i = peakIdx + 1; i < endIdx - 1; ++i) {
        if (points[i].y() <= points[i - 1].y() && points[i].y() <= points[i + 1].y()) {
            rightValley = i;
            break;
        }
    }

    double baselineAtPeak = computeLocalBaseline(points, leftValley, rightValley, points[peakIdx].x());
    double heightAboveBL = peakInt - baselineAtPeak;
    if (heightAboveBL <= 0.0) {
        heightAboveBL = peakInt;
        result.halfMax = peakInt / 2.0;
    } else {
        result.halfMax = baselineAtPeak + heightAboveBL / 2.0;
    }
    result.heightAboveBaseline = heightAboveBL;

    // 3. 找半峰全宽 (FWHM)
    double leftWl = findHalfCrossing(points, leftValley, peakIdx, result.halfMax, -1);
    double rightWl = findHalfCrossing(points, peakIdx, rightValley, result.halfMax, 1);

    if (leftWl < 0 || rightWl < 0) {
        result.errorMsg = QString("波长 %1 nm 处的峰无法计算半峰宽（基线可能过高或峰太窄）")
                              .arg(targetWl);
        return result;
    }

    result.fwhm = rightWl - leftWl;
    result.computedValue = result.fwhm * (heightAboveBL / 2.0);
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
