#include "SpectralAnalyzer.h"
#include <cmath>
#include <limits>

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
