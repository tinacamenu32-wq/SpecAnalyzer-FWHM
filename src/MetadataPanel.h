#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVector>
#include <QPointF>
#include "SpectrumData.h"
#include "SpectralAnalyzer.h"

/// 底部面板：文件元数据 + 去本底 + 峰值过滤 + 谱线分析
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
    /// 获取基线扣除参数
    SpectralAnalyzer::BLParams blParams() const;

    /// 峰值过滤是否启用
    bool isFilterEnabled() const { return m_chkFilterEnable && m_chkFilterEnable->isChecked(); }
    double filterMinIntensity() const { return m_spinMinIntensity ? m_spinMinIntensity->value() : 0; }
    double filterMinFWHM() const { return m_spinMinFWHM ? m_spinMinFWHM->value() : 0; }
    double filterMaxFWHM() const { return m_spinMaxFWHM ? m_spinMaxFWHM->value() : 9999; }

    void clear();

signals:
    void sgParamsChanged();
    void blParamsChanged();
    void filterChanged();
    void peakExportRequested();

private slots:
    void onSGSettingsChanged();
    void onBLSettingsChanged();

private:
    void setupUi();
    void applyFilter();
    void updateComboBox();

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
    QCheckBox *m_chkSGEnable = nullptr;
    QSpinBox  *m_spinWindow  = nullptr;
    QSpinBox  *m_spinOrder   = nullptr;

    // --- 去本底 ---
    QCheckBox      *m_chkBLEnable  = nullptr;
    QDoubleSpinBox *m_spinLambda   = nullptr;
    QDoubleSpinBox *m_spinP        = nullptr;

    // --- 峰值过滤 ---
    QCheckBox      *m_chkFilterEnable = nullptr;
    QDoubleSpinBox *m_spinMinIntensity = nullptr;
    QDoubleSpinBox *m_spinMinFWHM  = nullptr;
    QDoubleSpinBox *m_spinMaxFWHM  = nullptr;

    // --- 谱线分析 ---
    QLabel    *m_lblPeakCount = nullptr;
    QComboBox *m_cmbLine      = nullptr;

    // --- 数据 ---
    QVector<QPointF>       m_spectrumPoints;
    QVector<DetectedPeak>   m_allPeaks;           // 检测到的所有峰
    QVector<double>         m_allComputedValues;  // 所有峰的半高宽
    QVector<DetectedPeak>   m_detectedPeaks;      // 过滤后的峰
    QVector<double>         m_peakComputedValues; // 过滤后的半高宽
};
