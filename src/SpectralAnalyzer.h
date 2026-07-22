#pragma once

#include <QPointF>
#include <QVector>
#include <QString>
#include <algorithm>

/// 自动检测到的峰值
struct DetectedPeak {
    double wavelength;     // 峰值波长 (nm)
    double intensity;      // 峰值强度
    double prominence;     // 峰显著度
    int    index;          // 在原始数据中的索引
    double peakArea = 0;        // 峰面积（局部基线以上的积分）
    double localBaselineY = 0;  // 峰位置处的局部基线高度
    bool   from2ndDeriv = false; // 是否由二阶导数法检测到
};

/// 谱线分析结果
struct SpectralLineResult {
    QString  label;          // 如 "589.02 nm"
    double   targetWl;       // 目标波长 (nm)
    double   foundPeakWl;    // 实际找到的峰值波长 (nm)
    double   peakIntensity;  // 峰值强度
    double   halfMax;        // 半峰强度 = peakIntensity / 2
    double   fwhm;           // 半峰全宽 (nm)
    double   computedValue;  // FWHM × (peak/2)
    double   peakArea = 0;          // 峰面积（局部基线以上的积分）
    double   heightAboveBaseline = 0; // 局部基线以上的峰高
    bool     valid = false;
    QString  errorMsg;
};

/// 光谱分析器：自动找峰 + 计算 FWHM × (peak/2)
class SpectralAnalyzer {
public:
    /// 预定义的谱线列表（保留兼容）
    struct LineDef {
        QString label;
        double  wavelength;
    };

    /// Savitzky-Golay 平滑参数
    struct SGParams {
        int windowSize = 7;   // 窗口大小（奇数），默认 7
        int polyOrder  = 2;   // 多项式阶数，默认 2（与 Origin 一致）
        bool enabled   = true;  // 是否启用平滑
    };

    /// 基线扣除参数（ALS 算法）
    struct BLParams {
        double lambda = 1e5;   // 平滑度，越大基线越平滑 (典型 1e2 ~ 1e9)
        double p      = 0.01;  // 不对称性，峰上方点权重 (典型 0.001 ~ 0.1)
        int    niter  = 10;    // 迭代次数
        bool   enabled = true; // 是否启用去本底
    };

    static const QVector<LineDef> &predefinedLines();

    /// Savitzky-Golay 平滑滤波
    /// @param points  光谱数据点（只平滑强度，波长不变）
    /// @param params  平滑参数
    /// @return 平滑后的数据点
    static QVector<QPointF> savitzkyGolay(const QVector<QPointF> &points,
                                            const SGParams &params);

    /// 自动检测光谱数据中的所有峰值（综合局部极大值 + 二阶导数法）
    /// @param points          光谱数据点
    /// @param minProminence   最小峰显著度阈值，<=0 则自动计算（最大强度的 2%）
    /// @param neighborhoodSize 局部极大值检测的邻域大小（单侧点数）
    /// @return 检测到的峰值列表，按波长升序排列
    static QVector<DetectedPeak> detectPeaks(const QVector<QPointF> &points,
                                              double minProminence = 0.0,
                                              int neighborhoodSize = 5);

    /// 二阶导数法检测峰值（参照 Origin Peak Analyzer）
    /// 可检测到局部极大值法漏掉的肩峰和隐藏峰
    /// @param points      光谱数据点
    /// @param minHeight   最小峰高阈值，<=0 则自动计算（最大强度的 1%）
    /// @param smoothWindow SG 平滑窗口大小（用于导数计算）
    /// @param polyOrder   多项式阶数
    /// @return 检测到的峰值列表
    static QVector<DetectedPeak> detectPeaks2ndDeriv(const QVector<QPointF> &points,
                                                       double minHeight = 0.0,
                                                       int smoothWindow = 7,
                                                       int polyOrder = 2);

    /// 计算 SG 平滑导数（0=平滑值, 1=一阶导数, 2=二阶导数）
    /// @param points      光谱数据点
    /// @param windowSize  SG 窗口大小
    /// @param polyOrder   多项式阶数
    /// @param derivativeOrder 导数阶数 (0/1/2)
    /// @return 导数值数组（与输入等长）
    static QVector<double> computeSGDerivative(const QVector<QPointF> &points,
                                                int windowSize,
                                                int polyOrder,
                                                int derivativeOrder);

    /// 计算两谷底之间的局部基线高度（线性插值）
    /// @param points    光谱数据点
    /// @param leftIdx   左谷底索引
    /// @param rightIdx  右谷底索引
    /// @param targetX   目标波长
    /// @return 目标波长处的基线高度
    static double computeLocalBaseline(const QVector<QPointF> &points,
                                        int leftIdx,
                                        int rightIdx,
                                        double targetX);

    /// 对指定的峰值计算 FWHM 等指标
    /// @param points  光谱数据点
    /// @param peak    待分析的峰值
    static SpectralLineResult analyzePeak(const QVector<QPointF> &points,
                                           const DetectedPeak &peak);

    /// 基线扣除（ALS 非对称最小二乘法）
    /// @param points 光谱数据点
    /// @param params 基线参数
    /// @return 扣除基线后的数据点（波长不变，强度减去基线）
    static QVector<QPointF> subtractBaseline(const QVector<QPointF> &points,
                                               const BLParams &params);

    /// 获取计算出的基线（用于可视化等）
    static QVector<QPointF> computeBaseline(const QVector<QPointF> &points,
                                              const BLParams &params);

    /// 对给定光谱数据，分析目标波长附近的峰（保留兼容）
    /// @param points  光谱数据点
    /// @param targetWl 目标波长
    /// @param windowNm 搜索窗口半径 (nm)，默认 ±0.5 nm
    static SpectralLineResult analyze(const QVector<QPointF> &points,
                                      double targetWl,
                                      const QString &label,
                                      double windowNm = 0.5);

private:
    /// 计算峰的显著度（topographic prominence）
    static double computeProminence(const QVector<QPointF> &points,
                                     int peakIdx,
                                     int leftBoundary,
                                     int rightBoundary);

    /// 找到峰值两侧的谷底（局部最小值）作为 FWHM 搜索边界
    static void findValleyBounds(const QVector<QPointF> &points,
                                  int peakIdx,
                                  int &leftBound,
                                  int &rightBound);

    /// 线性插值找到强度穿过 halfValue 的波长
    static double findHalfCrossing(const QVector<QPointF> &points,
                                   int startIdx, int endIdx,
                                   double halfValue,
                                   int direction);

};
