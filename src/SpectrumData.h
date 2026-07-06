#pragma once

#include <QString>
#include <QVector>
#include <QPointF>

/// 光谱元数据
struct SpectrumMetadata {
    double  laserVoltage     = 0;   // 激光器电压(V)
    double  laserFrequency   = 0;   // 激光器频率(Hz)
    int     laserDivider     = 0;   // 激光器分频
    double  integrationTime  = 0;   // 光谱仪积分时间(ms)
    int     averagingCount   = 0;   // 光谱仪平均次数
    QString triggerMode;            // 触发模式
    double  triggerDelay    = 0;    // 触发延迟(us)
};

/// 一个 CSV 文件解析后的完整数据
class SpectrumData {
public:
    SpectrumData();

    QString             filePath;   // 完整绝对路径
    QString             fileName;   // 展示用的文件名
    SpectrumMetadata    metadata;
    QVector<QPointF>    points;     // (波长nm, 强度) 数据对

    bool isValid() const { return !points.isEmpty(); }
    int  pointCount() const { return points.size(); }

    double wavelengthMin() const;
    double wavelengthMax() const;
    double intensityMin() const;
    double intensityMax() const;

private:
    mutable bool m_rangeCached = false;
    mutable double m_wlMin = 0, m_wlMax = 0;
    mutable double m_intMin = 0, m_intMax = 0;
    void computeRange() const;
};
