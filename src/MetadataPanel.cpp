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

    // ===== 谱线分析 =====
    auto *analysisTitle = new QLabel("谱线分析", contentWidget);
    analysisTitle->setFont(boldFont);
    outerLayout->addWidget(analysisTitle);

    auto *separator2 = new QFrame(contentWidget);
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    outerLayout->addWidget(separator2);

    // 谱线选择
    auto *lineLayout = new QHBoxLayout();
    auto *lblLine = new QLabel("目标谱线:", contentWidget);
    m_cmbLine = new QComboBox(contentWidget);
    for (const auto &def : SpectralAnalyzer::predefinedLines()) {
        m_cmbLine->addItem(def.label, def.wavelength);
    }
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
    resultForm->addRow("FWHM×半峰:", m_lblResult);

    outerLayout->addLayout(resultForm);
    outerLayout->addStretch();

    scrollArea->setWidget(contentWidget);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    // 信号连接
    connect(m_cmbLine, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MetadataPanel::onLineSelected);
}

void MetadataPanel::setSpectrumPoints(const QVector<QPointF> &points)
{
    m_spectrumPoints = points;
    // 如果之前选过谱线，自动重新计算
    if (m_cmbLine->currentIndex() >= 0 && !m_spectrumPoints.isEmpty()) {
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
    if (!m_spectrumPoints.isEmpty()) {
        calculateAndDisplay();
    }
}

void MetadataPanel::calculateAndDisplay()
{
    if (m_spectrumPoints.isEmpty()) {
        return;
    }

    int idx = m_cmbLine->currentIndex();
    if (idx < 0) return;

    double targetWl = m_cmbLine->currentData().toDouble();
    QString label = m_cmbLine->currentText();

    m_lastResult = SpectralAnalyzer::analyze(m_spectrumPoints, targetWl, label);

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
