#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVector>
#include <QPointF>
#include "SpectrumData.h"
#include "SpectralAnalyzer.h"

/// 底部面板：文件元数据 + 谱线分析 + S-G 平滑
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

    /// 获取 S-G 平滑参数
    SpectralAnalyzer::SGParams sgParams() const;

    void clear();

signals:
    void sgParamsChanged();

private slots:
    void onLineSelected(int index);
    void onSGSettingsChanged();

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

    // --- S-G 平滑 ---
    QCheckBox *m_chkSGEnable  = nullptr;
    QSpinBox  *m_spinWindow   = nullptr;
    QSpinBox  *m_spinOrder    = nullptr;

    // --- 谱线分析 ---
    QLabel    *m_lblPeakCount = nullptr;
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
