#pragma once

#include <QPointF>
#include <QVector>
#include <QString>

/// 谱线分析结果
struct SpectralLineResult {
    QString  label;          // 如 "Na 589.0 nm"
    double   targetWl;       // 目标波长 (nm)
    double   foundPeakWl;    // 实际找到的峰值波长 (nm)
    double   peakIntensity;  // 峰值强度
    double   halfMax;        // 半峰强度 = peakIntensity / 2
    double   fwhm;           // 半峰全宽 (nm)
    double   computedValue;  // FWHM × (peak/2)
    bool     valid = false;
    QString  errorMsg;
};

/// 光谱分析器：找峰 + 计算 FWHM × (peak/2)
class SpectralAnalyzer {
public:
    /// 预定义的谱线列表
    struct LineDef {
        QString label;
        double  wavelength;
    };

    static const QVector<LineDef> &predefinedLines();

    /// 对给定光谱数据，分析目标波长附近的峰
    /// @param points  光谱数据点
    /// @param targetWl 目标波长
    /// @param windowNm 搜索窗口半径 (nm)，默认 ±0.5 nm
    static SpectralLineResult analyze(const QVector<QPointF> &points,
                                      double targetWl,
                                      const QString &label,
                                      double windowNm = 0.5);

private:
    /// 线性插值找到强度穿过 halfValue 的波长
    static double findHalfCrossing(const QVector<QPointF> &points,
                                   int startIdx, int endIdx,
                                   double halfValue,
                                   int direction);
};
