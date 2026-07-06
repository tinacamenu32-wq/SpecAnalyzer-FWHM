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

SpectralLineResult SpectralAnalyzer::analyze(const QVector<QPointF> &points,
                                              double targetWl,
                                              const QString &label,
                                              double windowNm)
{
    SpectralLineResult result;
    result.label = label;
    result.targetWl = targetWl;

    if (points.isEmpty()) {
        result.errorMsg = "光谱数据为空";
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
    // 左侧半峰波长：从 peak 向左搜索
    double leftWl = findHalfCrossing(points, startIdx, peakIdx, result.halfMax, -1);
    // 右侧半峰波长：从 peak 向右搜索
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

double SpectralAnalyzer::findHalfCrossing(const QVector<QPointF> &points,
                                           int startIdx, int endIdx,
                                           double halfValue,
                                           int direction)
{
    // direction: -1 = 向左搜索, +1 = 向右搜索
    // 在 startIdx..endIdx 范围内找强度穿越 halfValue 的位置

    if (direction < 0) {
        // 向左：从 endIdx 向 startIdx 搜索
        for (int i = endIdx; i > startIdx; --i) {
            double y1 = points[i - 1].y();
            double y2 = points[i].y();
            // 检查是否穿过 halfValue
            if ((y1 <= halfValue && y2 >= halfValue) ||
                (y1 >= halfValue && y2 <= halfValue)) {
                // 线性插值
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
