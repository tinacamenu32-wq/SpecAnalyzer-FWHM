#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVector>
#include <QPointF>
#include "SpectrumData.h"
#include "SpectralAnalyzer.h"

/// 底部面板：文件元数据 + 谱线分析
class MetadataPanel : public QWidget {
    Q_OBJECT
public:
    explicit MetadataPanel(QWidget *parent = nullptr);

    void setMetadata(const SpectrumMetadata &meta,
                     const QString &fileName,
                     int dataPointCount);

    /// 设置当前光谱数据（自动检测峰值并填充下拉框）
    void setSpectrumPoints(const QVector<QPointF> &points);

    /// 获取当前检测到的所有峰值
    const QVector<DetectedPeak> &detectedPeaks() const { return m_detectedPeaks; }

    void clear();

private slots:
    void onLineSelected(int index);

private:
    void setupUi();
    void calculateAndDisplay();

    // --- 元数据 ---
    QLabel *m_lblFileName      = nullptr;
    QLabel *m_lblDataPoints     = nullptr;
    QLabel *m_lblLaserVoltage   = nullptr;
    QLabel *m_lblLaserFreq      = nullptr;
    QLabel *m_lblLaserDivider   = nullptr;
    QLabel *m_lblIntegTime      = nullptr;
    QLabel *m_lblAveraging      = nullptr;
    QLabel *m_lblTriggerMode    = nullptr;
    QLabel *m_lblTriggerDelay   = nullptr;

    // --- 谱线分析 ---
    QLabel    *m_lblPeakCount = nullptr;  // 检测到的峰数量
    QComboBox *m_cmbLine      = nullptr;
    QLabel    *m_lblPeakWl    = nullptr;
    QLabel    *m_lblPeakInt   = nullptr;
    QLabel    *m_lblFwhm      = nullptr;
    QLabel    *m_lblHalfPeak  = nullptr;
    QLabel    *m_lblResult    = nullptr;

    // --- 数据 ---
    QVector<QPointF>      m_spectrumPoints;
    QVector<DetectedPeak>  m_detectedPeaks;
    SpectralLineResult     m_lastResult;
};
