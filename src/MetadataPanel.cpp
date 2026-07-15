#include "MetadataPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QScrollArea>
#include <QGroupBox>

MetadataPanel::MetadataPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void MetadataPanel::setupUi()
{
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *contentWidget = new QWidget();
    auto *outerLayout = new QVBoxLayout(contentWidget);
    outerLayout->setContentsMargins(8, 4, 8, 8);
    outerLayout->setSpacing(4);

    QFont boldFont;
    boldFont.setBold(true);
    boldFont.setPointSize(11);

    // ===== 文件信息 =====
    auto *titleLabel = new QLabel("文件信息", contentWidget);
    titleLabel->setFont(boldFont);
    outerLayout->addWidget(titleLabel);

    auto *separator1 = new QFrame(contentWidget);
    separator1->setFrameShape(QFrame::HLine);
    separator1->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator1);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(2);

    m_lblFileName    = new QLabel("-", contentWidget);
    m_lblDataPoints   = new QLabel("-", contentWidget);
    m_lblLaserVoltage = new QLabel("-", contentWidget);
    m_lblLaserFreq    = new QLabel("-", contentWidget);
    m_lblLaserDivider = new QLabel("-", contentWidget);
    m_lblIntegTime    = new QLabel("-", contentWidget);
    m_lblAveraging    = new QLabel("-", contentWidget);
    m_lblTriggerMode  = new QLabel("-", contentWidget);
    m_lblTriggerDelay = new QLabel("-", contentWidget);

    form->addRow("文件名:",       m_lblFileName);
    form->addRow("数据点数:",     m_lblDataPoints);
    form->addRow("激光器电压:",   m_lblLaserVoltage);
    form->addRow("激光器频率:",   m_lblLaserFreq);
    form->addRow("激光器分频:",   m_lblLaserDivider);
    form->addRow("积分时间:",     m_lblIntegTime);
    form->addRow("平均次数:",     m_lblAveraging);
    form->addRow("触发模式:",     m_lblTriggerMode);
    form->addRow("触发延迟:",     m_lblTriggerDelay);

    outerLayout->addLayout(form);

    // ===== S-G 平滑 =====
    auto *sgTitle = new QLabel("Savitzky-Golay 平滑", contentWidget);
    sgTitle->setFont(boldFont);
    outerLayout->addWidget(sgTitle);
    auto *sepSG = new QFrame(contentWidget);
    sepSG->setFrameShape(QFrame::HLine); sepSG->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(sepSG);

    auto *sgLayout = new QHBoxLayout();
    m_chkSGEnable = new QCheckBox("启用", contentWidget);
    m_chkSGEnable->setChecked(true);
    sgLayout->addWidget(m_chkSGEnable);
    sgLayout->addWidget(new QLabel("窗口:", contentWidget));
    m_spinWindow = new QSpinBox(contentWidget);
    m_spinWindow->setRange(3, 9999); m_spinWindow->setSingleStep(2); m_spinWindow->setValue(7);
    sgLayout->addWidget(m_spinWindow);
    auto *lblW = new QLabel("越大越平滑", contentWidget);
    lblW->setStyleSheet("color: #888; font-size: 10px;"); sgLayout->addWidget(lblW);
    sgLayout->addWidget(new QLabel("阶数:", contentWidget));
    m_spinOrder = new QSpinBox(contentWidget);
    m_spinOrder->setRange(1, 99); m_spinOrder->setValue(2);
    sgLayout->addWidget(m_spinOrder);
    auto *lblO = new QLabel("越高越保峰形", contentWidget);
    lblO->setStyleSheet("color: #888; font-size: 10px;"); sgLayout->addWidget(lblO);
    sgLayout->addStretch();
    outerLayout->addLayout(sgLayout);

    // ===== 去本底 =====
    auto *blTitle = new QLabel("去本底 (ALS)", contentWidget);
    blTitle->setFont(boldFont);
    outerLayout->addWidget(blTitle);

    auto *separator1c = new QFrame(contentWidget);
    separator1c->setFrameShape(QFrame::HLine);
    separator1c->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator1c);

    auto *blLayout = new QHBoxLayout();
    m_chkBLEnable = new QCheckBox("启用", contentWidget);
    m_chkBLEnable->setChecked(true);
    blLayout->addWidget(m_chkBLEnable);

    blLayout->addWidget(new QLabel("平滑度:", contentWidget));
    m_spinLambda = new QDoubleSpinBox(contentWidget);
    m_spinLambda->setRange(1, 1e12);
    m_spinLambda->setDecimals(0);
    m_spinLambda->setValue(1e5);
    blLayout->addWidget(m_spinLambda);

    auto *lblLamHint = new QLabel("越大越平", contentWidget);
    lblLamHint->setStyleSheet("color: #888; font-size: 10px;");
    blLayout->addWidget(lblLamHint);

    blLayout->addWidget(new QLabel("不对称性:", contentWidget));
    m_spinP = new QDoubleSpinBox(contentWidget);
    m_spinP->setRange(0.0001, 0.5);
    m_spinP->setDecimals(4);
    m_spinP->setSingleStep(0.001);
    m_spinP->setValue(0.01);
    blLayout->addWidget(m_spinP);

    auto *lblPHint = new QLabel("越小越贴底", contentWidget);
    lblPHint->setStyleSheet("color: #888; font-size: 10px;");
    blLayout->addWidget(lblPHint);

    blLayout->addStretch();
    outerLayout->addLayout(blLayout);

    // ===== 峰值过滤 =====
    auto *filterTitle = new QLabel("峰值过滤", contentWidget);
    filterTitle->setFont(boldFont);
    outerLayout->addWidget(filterTitle);

    auto *separator1d = new QFrame(contentWidget);
    separator1d->setFrameShape(QFrame::HLine);
    separator1d->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator1d);

    auto *filterLayout = new QHBoxLayout();
    m_chkFilterIntensity = new QCheckBox("强度", contentWidget);
    m_chkFilterIntensity->setChecked(true);
    filterLayout->addWidget(m_chkFilterIntensity);
    filterLayout->addWidget(new QLabel("≥:", contentWidget));
    m_spinMinIntensity = new QDoubleSpinBox(contentWidget);
    m_spinMinIntensity->setRange(0, 1e12);
    m_spinMinIntensity->setDecimals(0);
    m_spinMinIntensity->setValue(1200);
    m_spinMinIntensity->setToolTip("过滤掉强度低于此值的噪声峰");
    filterLayout->addWidget(m_spinMinIntensity);

    m_chkFilterFWHM = new QCheckBox("半高宽", contentWidget);
    m_chkFilterFWHM->setChecked(true);
    filterLayout->addWidget(m_chkFilterFWHM);
    filterLayout->addWidget(new QLabel(":", contentWidget));
    m_spinMinFWHM = new QDoubleSpinBox(contentWidget);
    m_spinMinFWHM->setRange(0, 1e12);
    m_spinMinFWHM->setDecimals(4);
    m_spinMinFWHM->setValue(0.0001);
    m_spinMinFWHM->setToolTip("最小半高宽");
    filterLayout->addWidget(m_spinMinFWHM);
    filterLayout->addWidget(new QLabel("~", contentWidget));
    m_spinMaxFWHM = new QDoubleSpinBox(contentWidget);
    m_spinMaxFWHM->setRange(0, 1e12);
    m_spinMaxFWHM->setDecimals(4);
    m_spinMaxFWHM->setValue(10000);
    m_spinMaxFWHM->setToolTip("最大半高宽");
    filterLayout->addWidget(m_spinMaxFWHM);

    filterLayout->addStretch();
    outerLayout->addLayout(filterLayout);

    // ===== 谱线分析 =====
    auto *analysisTitle = new QLabel("谱线分析", contentWidget);
    analysisTitle->setFont(boldFont);
    outerLayout->addWidget(analysisTitle);

    auto *separator2 = new QFrame(contentWidget);
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator2);

    m_lblPeakCount = new QLabel("未检测到峰值", contentWidget);
    m_lblPeakCount->setStyleSheet("color: #5f6368; font-size: 11px;");
    outerLayout->addWidget(m_lblPeakCount);

    auto *lineLayout = new QHBoxLayout();
    lineLayout->addWidget(new QLabel("目标谱线:", contentWidget));
    m_cmbLine = new QComboBox(contentWidget);
    m_cmbLine->setMinimumWidth(280);
    lineLayout->addWidget(m_cmbLine, 1);
    auto *btnExportPeaks = new QPushButton("导出所选峰值", contentWidget);
    lineLayout->addWidget(btnExportPeaks);
    outerLayout->addLayout(lineLayout);

    outerLayout->addStretch();

    scrollArea->setWidget(contentWidget);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    // 信号连接
    connect(m_chkSGEnable, &QCheckBox::toggled, this, &MetadataPanel::onSGSettingsChanged);
    connect(m_spinWindow, QOverload<int>::of(&QSpinBox::valueChanged), this, &MetadataPanel::onSGSettingsChanged);
    connect(m_spinOrder, QOverload<int>::of(&QSpinBox::valueChanged), this, &MetadataPanel::onSGSettingsChanged);
    connect(m_chkBLEnable, &QCheckBox::toggled, this, &MetadataPanel::onBLSettingsChanged);
    connect(m_spinLambda, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MetadataPanel::onBLSettingsChanged);
    connect(m_spinP, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MetadataPanel::onBLSettingsChanged);
    connect(m_chkFilterIntensity, &QCheckBox::toggled, this, &MetadataPanel::applyFilter);
    connect(m_chkFilterFWHM, &QCheckBox::toggled, this, &MetadataPanel::applyFilter);
    connect(m_spinMinIntensity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MetadataPanel::applyFilter);
    connect(m_spinMinFWHM, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MetadataPanel::applyFilter);
    connect(m_spinMaxFWHM, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MetadataPanel::applyFilter);
    connect(btnExportPeaks, &QPushButton::clicked, this, &MetadataPanel::peakExportRequested);
}

SpectralAnalyzer::SGParams MetadataPanel::sgParams() const
{
    SpectralAnalyzer::SGParams p;
    p.enabled    = m_chkSGEnable->isChecked();
    p.windowSize = m_spinWindow->value();
    p.polyOrder  = m_spinOrder->value();
    return p;
}

void MetadataPanel::onSGSettingsChanged()
{
    if (m_spinWindow->value() % 2 == 0)
        m_spinWindow->setValue(m_spinWindow->value() + 1);
    emit sgParamsChanged();
}

SpectralAnalyzer::BLParams MetadataPanel::blParams() const
{
    SpectralAnalyzer::BLParams p;
    p.enabled = m_chkBLEnable->isChecked();
    p.lambda  = m_spinLambda->value();
    p.p       = m_spinP->value();
    return p;
}

void MetadataPanel::onBLSettingsChanged()
{
    emit blParamsChanged();
}

void MetadataPanel::setSpectrumPoints(const QVector<QPointF> &points)
{
    m_spectrumPoints = points;

    if (points.isEmpty()) {
        m_allPeaks.clear();
        m_allComputedValues.clear();
        m_cmbLine->clear();
        m_lblPeakCount->setText("无光谱数据");
        return;
    }

    // 检测所有峰
    m_allPeaks = SpectralAnalyzer::detectPeaks(points);
    if (m_allPeaks.isEmpty()) {
        m_allComputedValues.clear();
        m_cmbLine->clear();
        m_lblPeakCount->setText("未检测到显著峰值");
        return;
    }

    // 预计算所有峰的半高宽
    m_allComputedValues.resize(m_allPeaks.size());
    for (int i = 0; i < m_allPeaks.size(); ++i) {
        auto result = SpectralAnalyzer::analyzePeak(points, m_allPeaks[i]);
        m_allComputedValues[i] = result.computedValue;
    }

    applyFilter();
}

void MetadataPanel::applyFilter()
{
    // 未启用任何过滤则显示全部
    if (!m_chkFilterIntensity->isChecked() && !m_chkFilterFWHM->isChecked()) {
        m_detectedPeaks = m_allPeaks;
        m_peakComputedValues = m_allComputedValues;
        updateComboBox();
        return;
    }

    double minInt = m_spinMinIntensity->value();
    double minFWHM = m_spinMinFWHM->value();
    double maxFWHM = m_spinMaxFWHM->value();
    bool filterIntensity = m_chkFilterIntensity->isChecked();
    bool filterFWHM = m_chkFilterFWHM->isChecked();

    m_detectedPeaks.clear();
    m_peakComputedValues.clear();
    for (int i = 0; i < m_allPeaks.size(); ++i) {
        const auto &p = m_allPeaks[i];
        double v = m_allComputedValues[i];
        if (filterIntensity && p.intensity < minInt) continue;
        if (filterFWHM && (v < minFWHM || v > maxFWHM)) continue;
        m_detectedPeaks.append(p);
        m_peakComputedValues.append(v);
    }

    updateComboBox();
}

void MetadataPanel::updateComboBox()
{
    m_cmbLine->blockSignals(true);
    m_cmbLine->clear();
    for (int i = 0; i < m_detectedPeaks.size(); ++i) {
        const auto &p = m_detectedPeaks[i];
        QString text = QString("#%1  %2 nm  强度:%3  半高宽:%4")
                           .arg(i + 1, 2)
                           .arg(p.wavelength, 8, 'f', 2)
                           .arg(p.intensity, 0, 'f', 0)
                           .arg(m_peakComputedValues[i], 0, 'f', 4);
        m_cmbLine->addItem(text, i);
    }
    m_cmbLine->blockSignals(false);

    if (!m_chkFilterIntensity->isChecked() && !m_chkFilterFWHM->isChecked()) {
        m_lblPeakCount->setText(QString("检测到 %1 个峰值 (未过滤)").arg(m_allPeaks.size()));
    } else if (m_detectedPeaks.isEmpty()) {
        m_lblPeakCount->setText(QString("无符合过滤条件的峰值 (共检测到 %1 个)").arg(m_allPeaks.size()));
    } else {
        m_lblPeakCount->setText(QString("检测到 %1 个峰值 (过滤后 %2 个)")
                                    .arg(m_allPeaks.size())
                                    .arg(m_detectedPeaks.size()));
    }

    emit filterChanged();
}

void MetadataPanel::setMetadata(const SpectrumMetadata &meta,
                                 const QString &fileName,
                                 int dataPointCount)
{
    m_lblFileName->setText(fileName);
    m_lblDataPoints->setText(QString::number(dataPointCount));
    m_lblLaserVoltage->setText(QString::number(meta.laserVoltage) + " V");
    m_lblLaserFreq->setText(QString::number(meta.laserFrequency) + " Hz");
    m_lblLaserDivider->setText(QString::number(meta.laserDivider));
    m_lblIntegTime->setText(QString::number(meta.integrationTime) + " ms");
    m_lblAveraging->setText(QString::number(meta.averagingCount));
    m_lblTriggerMode->setText(meta.triggerMode);
    m_lblTriggerDelay->setText(QString::number(meta.triggerDelay) + " us");
}

void MetadataPanel::clear()
{
    m_lblFileName->setText("-");
    m_lblDataPoints->setText("-");
    m_lblLaserVoltage->setText("-");
    m_lblLaserFreq->setText("-");
    m_lblLaserDivider->setText("-");
    m_lblIntegTime->setText("-");
    m_lblAveraging->setText("-");
    m_lblTriggerMode->setText("-");
    m_lblTriggerDelay->setText("-");

    m_allPeaks.clear();
    m_allComputedValues.clear();
    m_cmbLine->clear();
    m_lblPeakCount->setText("未检测到峰值");
    m_spectrumPoints.clear();
}
