#include "MetadataPanel.h"

#include <QVBoxLayout>
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
    // 用 QScrollArea 包裹，方便内容多时滚动
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *contentWidget = new QWidget();
    auto *outerLayout = new QVBoxLayout(contentWidget);
    outerLayout->setContentsMargins(8, 4, 8, 8);
    outerLayout->setSpacing(4);

    // ===== 文件信息 =====
    auto *titleLabel = new QLabel("文件信息", contentWidget);
    QFont boldFont = titleLabel->font();
    boldFont.setBold(true);
    boldFont.setPointSize(11);
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

    auto *separator1b = new QFrame(contentWidget);
    separator1b->setFrameShape(QFrame::HLine);
    separator1b->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator1b);

    auto *sgLayout = new QHBoxLayout();
    m_chkSGEnable = new QCheckBox("启用", contentWidget);
    m_chkSGEnable->setChecked(true);
    sgLayout->addWidget(m_chkSGEnable);

    sgLayout->addWidget(new QLabel("窗口:", contentWidget));
    m_spinWindow = new QSpinBox(contentWidget);
    m_spinWindow->setRange(3, 31);
    m_spinWindow->setSingleStep(2);
    m_spinWindow->setValue(7);
    m_spinWindow->setToolTip("窗口点数(奇数): 越大去噪越强，但会丢失细节");
    sgLayout->addWidget(m_spinWindow);

    auto *lblWinHint = new QLabel("越大越平滑", contentWidget);
    lblWinHint->setStyleSheet("color: #888; font-size: 10px;");
    sgLayout->addWidget(lblWinHint);

    sgLayout->addWidget(new QLabel("阶数:", contentWidget));
    m_spinOrder = new QSpinBox(contentWidget);
    m_spinOrder->setRange(1, 5);
    m_spinOrder->setValue(2);
    m_spinOrder->setToolTip("多项式阶数: 越高越能保留峰形细节");
    sgLayout->addWidget(m_spinOrder);

    auto *lblOrderHint = new QLabel("越高越保峰形", contentWidget);
    lblOrderHint->setStyleSheet("color: #888; font-size: 10px;");
    sgLayout->addWidget(lblOrderHint);

    sgLayout->addStretch();
    outerLayout->addLayout(sgLayout);

    // ===== 谱线分析 =====
    auto *analysisTitle = new QLabel("谱线分析", contentWidget);
    analysisTitle->setFont(boldFont);
    outerLayout->addWidget(analysisTitle);

    auto *separator2 = new QFrame(contentWidget);
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator2);

    // 检测到的峰数量提示
    m_lblPeakCount = new QLabel("未检测到峰值", contentWidget);
    m_lblPeakCount->setStyleSheet("color: #5f6368; font-size: 11px;");
    outerLayout->addWidget(m_lblPeakCount);

    // 谱线选择（由自动检测的峰值填充）
    auto *lineLayout = new QHBoxLayout();
    auto *lblLine = new QLabel("目标谱线:", contentWidget);
    m_cmbLine = new QComboBox(contentWidget);
    m_cmbLine->setMinimumWidth(200);
    lineLayout->addWidget(lblLine);
    lineLayout->addWidget(m_cmbLine, 1);
    outerLayout->addLayout(lineLayout);

    // 计算结果
    auto *resultForm = new QFormLayout();
    resultForm->setLabelAlignment(Qt::AlignRight);
    resultForm->setHorizontalSpacing(10);
    resultForm->setVerticalSpacing(2);

    m_lblPeakWl   = new QLabel("-", contentWidget);
    m_lblPeakInt  = new QLabel("-", contentWidget);
    m_lblFwhm     = new QLabel("-", contentWidget);
    m_lblHalfPeak = new QLabel("-", contentWidget);
    m_lblResult   = new QLabel("-", contentWidget);

    // 高亮结果显示
    QFont resultFont = m_lblResult->font();
    resultFont.setBold(true);
    resultFont.setPointSize(resultFont.pointSize() + 1);
    m_lblResult->setFont(resultFont);
    m_lblResult->setStyleSheet("color: #1a73e8;");

    resultForm->addRow("峰值波长:",  m_lblPeakWl);
    resultForm->addRow("峰值强度:",  m_lblPeakInt);
    resultForm->addRow("半峰全宽:",  m_lblFwhm);
    resultForm->addRow("半峰强度:",  m_lblHalfPeak);
    resultForm->addRow("半高宽:", m_lblResult);

    outerLayout->addLayout(resultForm);
    outerLayout->addStretch();

    scrollArea->setWidget(contentWidget);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    // 信号连接
    connect(m_cmbLine, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MetadataPanel::onLineSelected);
    connect(m_chkSGEnable, &QCheckBox::toggled, this, &MetadataPanel::onSGSettingsChanged);
    connect(m_spinWindow, QOverload<int>::of(&QSpinBox::valueChanged), this, &MetadataPanel::onSGSettingsChanged);
    connect(m_spinOrder, QOverload<int>::of(&QSpinBox::valueChanged), this, &MetadataPanel::onSGSettingsChanged);
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
    // 确保奇数窗口
    if (m_spinWindow->value() % 2 == 0)
        m_spinWindow->setValue(m_spinWindow->value() + 1);
    emit sgParamsChanged();
}

void MetadataPanel::setSpectrumPoints(const QVector<QPointF> &points)
{
    m_spectrumPoints = points;

    // 断开信号避免填充过程中触发计算
    m_cmbLine->blockSignals(true);
    m_cmbLine->clear();

    if (points.isEmpty()) {
        m_detectedPeaks.clear();
        m_lblPeakCount->setText("无光谱数据");
        m_cmbLine->blockSignals(false);
        return;
    }

    // 自动检测所有峰值
    m_detectedPeaks = SpectralAnalyzer::detectPeaks(points);

    if (m_detectedPeaks.isEmpty()) {
        m_lblPeakCount->setText("未检测到显著峰值");
        m_cmbLine->blockSignals(false);
        // 清除结果显示
        m_lblPeakWl->setText("-");
        m_lblPeakInt->setText("-");
        m_lblFwhm->setText("-");
        m_lblHalfPeak->setText("-");
        m_lblResult->setText("-");
        return;
    }

    // 填充下拉框：显示波长 + 强度概览
    for (int i = 0; i < m_detectedPeaks.size(); ++i) {
        const auto &p = m_detectedPeaks[i];
        QString text = QString("%1 nm  (强度: %2)")
                           .arg(p.wavelength, 8, 'f', 2)
                           .arg(p.intensity, 0, 'f', 0);
        m_cmbLine->addItem(text, i);  // 存储峰值在 m_detectedPeaks 中的索引
    }

    m_lblPeakCount->setText(QString("自动检测到 %1 个峰值").arg(m_detectedPeaks.size()));

    m_cmbLine->blockSignals(false);

    // 自动选中第一个（波长最小），触发计算
    if (m_cmbLine->count() > 0) {
        m_cmbLine->setCurrentIndex(0);
        calculateAndDisplay();
    }
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

    // 清除峰值列表
    m_cmbLine->blockSignals(true);
    m_cmbLine->clear();
    m_cmbLine->blockSignals(false);
    m_detectedPeaks.clear();
    m_lblPeakCount->setText("未检测到峰值");

    // 清除分析结果
    m_lblPeakWl->setText("-");
    m_lblPeakInt->setText("-");
    m_lblFwhm->setText("-");
    m_lblHalfPeak->setText("-");
    m_lblResult->setText("-");

    m_spectrumPoints.clear();
}

void MetadataPanel::onLineSelected(int /*index*/)
{
    if (!m_spectrumPoints.isEmpty() && !m_detectedPeaks.isEmpty()) {
        calculateAndDisplay();
    }
}

void MetadataPanel::calculateAndDisplay()
{
    if (m_spectrumPoints.isEmpty() || m_detectedPeaks.isEmpty()) {
        return;
    }

    int comboIdx = m_cmbLine->currentIndex();
    if (comboIdx < 0) return;

    int peakIdx = m_cmbLine->currentData().toInt();
    if (peakIdx < 0 || peakIdx >= m_detectedPeaks.size()) return;

    const DetectedPeak &peak = m_detectedPeaks[peakIdx];
    m_lastResult = SpectralAnalyzer::analyzePeak(m_spectrumPoints, peak);

    if (m_lastResult.valid) {
        m_lblPeakWl->setText(QString("%1 nm").arg(m_lastResult.foundPeakWl, 0, 'f', 2));
        m_lblPeakInt->setText(QString::number(m_lastResult.peakIntensity, 'f', 2));
        m_lblFwhm->setText(QString("%1 nm").arg(m_lastResult.fwhm, 0, 'f', 4));
        m_lblHalfPeak->setText(QString::number(m_lastResult.halfMax, 'f', 2));
        m_lblResult->setText(QString::number(m_lastResult.computedValue, 'f', 4));
        m_lblResult->setStyleSheet("color: #1a73e8;");
    } else {
        m_lblPeakWl->setText(m_lastResult.errorMsg);
        m_lblPeakInt->setText("-");
        m_lblFwhm->setText("-");
        m_lblHalfPeak->setText("-");
        m_lblResult->setText(m_lastResult.errorMsg);
        m_lblResult->setStyleSheet("color: #d93025;");
    }
}
