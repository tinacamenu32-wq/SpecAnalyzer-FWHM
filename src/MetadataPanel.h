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

    /// 设置当前光谱数据（用于谱线分析计算）
    void setSpectrumPoints(const QVector<QPointF> &points);

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
    QComboBox *m_cmbLine      = nullptr;
    QLabel    *m_lblPeakWl    = nullptr;
    QLabel    *m_lblPeakInt   = nullptr;
    QLabel    *m_lblFwhm      = nullptr;
    QLabel    *m_lblHalfPeak  = nullptr;
    QLabel    *m_lblResult    = nullptr;

    // --- 数据 ---
    QVector<QPointF> m_spectrumPoints;
    SpectralLineResult m_lastResult;
};
