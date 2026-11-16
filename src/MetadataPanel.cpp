#include "MetadataPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>
#include <QScrollArea>
#include <QGroupBox>
#include <QToolButton>
#include <QTabWidget>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QScreen>
#include <QGuiApplication>

namespace {
class PeakDetailsDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height() + 10, 36));
        return size;
    }

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        const QString details = index.data(Qt::ToolTipRole).toString();
        if (!details.isEmpty()) option->text = details;
    }
};

class PeakComboBox : public QComboBox {
public:
    explicit PeakComboBox(QWidget *parent) : QComboBox(parent)
    {
        setItemDelegate(new PeakDetailsDelegate(this));
        QFont popupFont = font();
        popupFont.setPointSizeF(qMax(13.0, popupFont.pointSizeF()));
        view()->setFont(popupFont);
    }

    void showPopup() override
    {
        int popupWidth = width();
        for (int i = 0; i < count(); ++i)
            popupWidth = qMax(popupWidth,
                view()->fontMetrics().horizontalAdvance(itemData(i, Qt::ToolTipRole).toString()) + 48);
        auto *screen = QGuiApplication::screenAt(mapToGlobal(rect().center()));
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (screen) popupWidth = qMin(popupWidth, screen->availableGeometry().width() - 24);
        view()->setMinimumWidth(popupWidth);
        QComboBox::showPopup();
    }
};
}

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
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *contentWidget = new QWidget();
    contentWidget->setObjectName("inspectorContent");
    auto *outerLayout = new QVBoxLayout(contentWidget);
    outerLayout->setContentsMargins(2, 6, 8, 12);
    outerLayout->setSpacing(14);
    auto *title = new QLabel(QStringLiteral("分析设置"), contentWidget);
    title->setProperty("role", "section");
    outerLayout->addWidget(title);
    auto *description = new QLabel(QStringLiteral("调整参数，实时更新光谱与峰值"), contentWidget);
    description->setProperty("role", "secondary");
    outerLayout->addWidget(description);
    auto makeGroup = [contentWidget, outerLayout](const QString &title) {
        auto *group = new QGroupBox(title, contentWidget);
        outerLayout->addWidget(group);
        auto *layout = new QVBoxLayout(group);
        layout->setContentsMargins(12, 14, 12, 12);
        layout->setSpacing(10);
        return layout;
    };
    auto makeForm = [](QVBoxLayout *layout) {
        auto *form = new QFormLayout();
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setHorizontalSpacing(12);
        form->setVerticalSpacing(8);
        layout->addLayout(form);
        return form;
    };

    auto *sgLayout = makeGroup(QStringLiteral("光谱平滑"));
    m_chkSGEnable = new QCheckBox(QStringLiteral("启用 S-G 平滑"), contentWidget);
    m_chkSGEnable->setChecked(true);
    sgLayout->addWidget(m_chkSGEnable);
    auto *sgForm = makeForm(sgLayout);
    m_spinWindow = new QSpinBox(contentWidget);
    m_spinWindow->setRange(3, 9999);
    m_spinWindow->setSingleStep(2);
    m_spinWindow->setValue(7);
    m_spinWindow->setToolTip(QStringLiteral("奇数窗口；窗口越大，曲线越平滑"));
    sgForm->addRow(QStringLiteral("窗口大小"), m_spinWindow);
    m_spinOrder = new QSpinBox(contentWidget);
    m_spinOrder->setRange(1, 99);
    m_spinOrder->setValue(2);
    sgForm->addRow(QStringLiteral("多项式阶数"), m_spinOrder);
    auto *sgHint = new QLabel(QStringLiteral("窗口越大越平滑，阶数越高越保留峰形。"), contentWidget);
    sgHint->setProperty("role", "secondary");
    sgHint->setWordWrap(true);
    sgLayout->addWidget(sgHint);
    connect(m_chkSGEnable, &QCheckBox::toggled, m_spinWindow, &QWidget::setEnabled);
    connect(m_chkSGEnable, &QCheckBox::toggled, m_spinOrder, &QWidget::setEnabled);

    auto *blLayout = makeGroup(QStringLiteral("基线校正"));
    m_chkBLEnable = new QCheckBox(QStringLiteral("启用 ALS 去本底"), contentWidget);
    m_chkBLEnable->setChecked(true);
    blLayout->addWidget(m_chkBLEnable);
    auto *blForm = makeForm(blLayout);
    m_spinLambda = new QDoubleSpinBox(contentWidget);
    m_spinLambda->setRange(1, 1e12);
    m_spinLambda->setDecimals(0);
    m_spinLambda->setValue(1e5);
    m_spinLambda->setToolTip(QStringLiteral("数值越大，估计的基线越平滑"));
    blForm->addRow(QStringLiteral("平滑度 λ"), m_spinLambda);
    m_spinP = new QDoubleSpinBox(contentWidget);
    m_spinP->setRange(0.0001, 0.5);
    m_spinP->setDecimals(4);
    m_spinP->setSingleStep(0.001);
    m_spinP->setValue(0.01);
    m_spinP->setToolTip(QStringLiteral("数值越小，基线越贴近峰底"));
    blForm->addRow(QStringLiteral("不对称性 p"), m_spinP);
    connect(m_chkBLEnable, &QCheckBox::toggled, m_spinLambda, &QWidget::setEnabled);
    connect(m_chkBLEnable, &QCheckBox::toggled, m_spinP, &QWidget::setEnabled);

    auto *filterLayout = makeGroup(QStringLiteral("峰值过滤"));
    m_chkFilterIntensity = new QCheckBox(QStringLiteral("按强度过滤"), contentWidget);
    m_chkFilterIntensity->setChecked(true);
    filterLayout->addWidget(m_chkFilterIntensity);
    auto *intensityForm = makeForm(filterLayout);
    m_spinMinIntensity = new QDoubleSpinBox(contentWidget);
    m_spinMinIntensity->setRange(0, 1e12);
    m_spinMinIntensity->setDecimals(0);
    m_spinMinIntensity->setValue(1200);
    m_spinMinIntensity->setToolTip(QStringLiteral("过滤掉强度低于此值的噪声峰"));
    intensityForm->addRow(QStringLiteral("最低强度"), m_spinMinIntensity);
    m_chkFilterFWHM = new QCheckBox(QStringLiteral("按净峰积分面积过滤"), contentWidget);
    m_chkFilterFWHM->setChecked(true);
    filterLayout->addWidget(m_chkFilterFWHM);
    auto *areaForm = makeForm(filterLayout);
    m_spinMinFWHM = new QDoubleSpinBox(contentWidget);
    m_spinMinFWHM->setRange(0, 1e12);
    m_spinMinFWHM->setDecimals(4);
    m_spinMinFWHM->setValue(0.0001);
    m_spinMinFWHM->setToolTip(QStringLiteral("最小净峰积分面积"));
    areaForm->addRow(QStringLiteral("最小面积"), m_spinMinFWHM);
    m_spinMaxFWHM = new QDoubleSpinBox(contentWidget);
    m_spinMaxFWHM->setRange(0, 1e12);
    m_spinMaxFWHM->setDecimals(4);
    m_spinMaxFWHM->setValue(1e12);
    m_spinMaxFWHM->setToolTip(QStringLiteral("最大净峰积分面积"));
    areaForm->addRow(QStringLiteral("最大面积"), m_spinMaxFWHM);
    connect(m_chkFilterIntensity, &QCheckBox::toggled, m_spinMinIntensity, &QWidget::setEnabled);
    connect(m_chkFilterFWHM, &QCheckBox::toggled, m_spinMinFWHM, &QWidget::setEnabled);
    connect(m_chkFilterFWHM, &QCheckBox::toggled, m_spinMaxFWHM, &QWidget::setEnabled);

    auto *analysisLayout = makeGroup(QStringLiteral("谱线与导出"));
    m_lblPeakCount = new QLabel(QStringLiteral("导入光谱后自动检测峰值"), contentWidget);
    m_lblPeakCount->setWordWrap(true);
    m_lblPeakCount->setProperty("role", "secondary");
    analysisLayout->addWidget(m_lblPeakCount);
    m_cmbLine = new PeakComboBox(contentWidget);
    m_cmbLine->setMinimumWidth(0);
    m_cmbLine->setMinimumContentsLength(12);
    m_cmbLine->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_cmbLine->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_cmbLine->setAccessibleName(QStringLiteral("检测到的谱线"));
    analysisLayout->addWidget(m_cmbLine);
    m_lblPeakDetails = new QLabel(QStringLiteral("选择谱线以查看强度、半高宽与面积"), contentWidget);
    m_lblPeakDetails->setObjectName("peakDetails");
    m_lblPeakDetails->setWordWrap(true);
    m_lblPeakDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);
    analysisLayout->addWidget(m_lblPeakDetails);
    auto *btnExportPeaks = new QPushButton(QStringLiteral("导出所选峰值表"), contentWidget);
    btnExportPeaks->setProperty("role", "primary");
    btnExportPeaks->setEnabled(false);
    analysisLayout->addWidget(btnExportPeaks);
    connect(this, &MetadataPanel::filterChanged, btnExportPeaks, [this, btnExportPeaks]() {
        btnExportPeaks->setEnabled(!m_detectedPeaks.isEmpty());
    });
    connect(m_cmbLine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        updatePeakDetails(index);
    });

    auto *fileLayout = makeGroup(QStringLiteral("文件信息"));
    m_lblFileName = new QLabel(QStringLiteral("尚未选择文件"), contentWidget);
    m_lblFileName->setObjectName("fileName");
    m_lblFileName->setWordWrap(true);
    m_lblFileName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileLayout->addWidget(m_lblFileName);
    auto *detailsButton = new QToolButton(contentWidget);
    detailsButton->setText(QStringLiteral("采集参数"));
    detailsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    detailsButton->setArrowType(Qt::RightArrow);
    detailsButton->setCheckable(true);
    fileLayout->addWidget(detailsButton);
    auto *details = new QWidget(contentWidget);
    auto *form = new QFormLayout(details);
    form->setContentsMargins(0, 4, 0, 0);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setVerticalSpacing(8);
    m_lblDataPoints = new QLabel("-", details);
    m_lblLaserVoltage = new QLabel("-", details);
    m_lblLaserFreq = new QLabel("-", details);
    m_lblLaserDivider = new QLabel("-", details);
    m_lblIntegTime = new QLabel("-", details);
    m_lblAveraging = new QLabel("-", details);
    m_lblTriggerMode = new QLabel("-", details);
    m_lblTriggerDelay = new QLabel("-", details);
    form->addRow(QStringLiteral("数据点数"), m_lblDataPoints);
    form->addRow(QStringLiteral("激光器电压"), m_lblLaserVoltage);
    form->addRow(QStringLiteral("激光器频率"), m_lblLaserFreq);
    form->addRow(QStringLiteral("激光器分频"), m_lblLaserDivider);
    form->addRow(QStringLiteral("积分时间"), m_lblIntegTime);
    form->addRow(QStringLiteral("平均次数"), m_lblAveraging);
    form->addRow(QStringLiteral("触发模式"), m_lblTriggerMode);
    form->addRow(QStringLiteral("触发延迟"), m_lblTriggerDelay);
    fileLayout->addWidget(details);
    details->hide();
    connect(detailsButton, &QToolButton::toggled, details, &QWidget::setVisible);
    connect(detailsButton, &QToolButton::toggled, detailsButton, [detailsButton](bool open) {
        detailsButton->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });
    // Keep frequent analysis controls in view; advanced processing and acquisition
    // details each have their own page, instead of one long inspector.
    auto *tabs = new QTabWidget(contentWidget);
    tabs->setDocumentMode(true);
    auto addPage = [outerLayout, tabs](const QString &name, const QList<QVBoxLayout *> &groups) {
        auto *page = new QWidget(tabs);
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 8, 0, 0);
        pageLayout->setSpacing(14);
        for (auto *groupLayout : groups) {
            auto *group = groupLayout->parentWidget();
            outerLayout->removeWidget(group);
            pageLayout->addWidget(group);
        }
        pageLayout->addStretch();
        tabs->addTab(page, name);
    };
    addPage(QStringLiteral("峰值分析"), {filterLayout, analysisLayout});
    addPage(QStringLiteral("预处理"), {sgLayout, blLayout});
    addPage(QStringLiteral("文件信息"), {fileLayout});
    outerLayout->addWidget(tabs);
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
        m_allResults.clear();
        m_peakResults.clear();
        m_allComputedValues.clear();
        m_detectedPeaks.clear();
        m_cmbLine->clear();
        updatePeakDetails(-1);
        emit filterChanged();
        m_lblPeakCount->setText("无光谱数据");
        return;
    }

    // 检测所有峰
    m_allPeaks = SpectralAnalyzer::detectPeaks(points);
    if (m_allPeaks.isEmpty()) {
        m_allResults.clear();
        m_peakResults.clear();
        m_allComputedValues.clear();
        m_detectedPeaks.clear();
        m_cmbLine->clear();
        updatePeakDetails(-1);
        emit filterChanged();
        m_lblPeakCount->setText("未检测到显著峰值");
        return;
    }

    // 在任何面积/强度过滤之前联合分析，避免过滤改变重叠峰组分。
    m_allResults = SpectralAnalyzer::analyzePeaks(points, m_allPeaks);
    // 预计算所有峰的净峰积分面积
    m_allComputedValues.resize(m_allPeaks.size());
    m_allFwhmValues.resize(m_allPeaks.size());
    for (int i = 0; i < m_allPeaks.size(); ++i) {
        const auto &result = m_allResults[i];
        m_allComputedValues[i] = result.computedValue;
        m_allFwhmValues[i] = result.fwhm;
    }

    applyFilter();
}

void MetadataPanel::applyFilter()
{
    // 未启用任何过滤则显示全部
    if (!m_chkFilterIntensity->isChecked() && !m_chkFilterFWHM->isChecked()) {
        m_detectedPeaks = m_allPeaks;
        m_peakComputedValues = m_allComputedValues;
        m_peakFwhmValues = m_allFwhmValues;
        m_peakResults = m_allResults;
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
    m_peakFwhmValues.clear();
    m_peakResults.clear();
    for (int i = 0; i < m_allPeaks.size(); ++i) {
        const auto &p = m_allPeaks[i];
        double v = m_allComputedValues[i];
        if (filterIntensity && p.intensity < minInt) continue;
        if (filterFWHM && m_allResults[i].valid && (v < minFWHM || v > maxFWHM)) continue;
        m_detectedPeaks.append(p);
        m_peakComputedValues.append(v);
        m_peakFwhmValues.append(m_allFwhmValues[i]);
        m_peakResults.append(m_allResults[i]);
    }

    updateComboBox();
}

void MetadataPanel::updateComboBox()
{
    m_cmbLine->blockSignals(true);
    m_cmbLine->clear();
    for (int i = 0; i < m_detectedPeaks.size(); ++i) {
        const auto &p = m_detectedPeaks[i];
        QString text = QString("#%1  %2 nm  强度:%3  半高宽原始值:%4 nm  净峰积分面积:%5")
                           .arg(i + 1, 2)
                           .arg(p.wavelength, 8, 'f', 2)
                           .arg(p.intensity, 0, 'f', 0)
                           .arg(m_peakFwhmValues[i], 0, 'f', 4)
                           .arg(m_peakResults[i].valid ? QString::number(m_peakComputedValues[i], 'f', 4) : QStringLiteral("不可用"));
        text += QStringLiteral("  %1").arg(m_peakResults[i].areaStatus);
        m_cmbLine->addItem(QStringLiteral("#%1  ·  %2 nm").arg(i + 1).arg(p.wavelength, 0, 'f', 2), i);
        m_cmbLine->setItemData(i, text, Qt::ToolTipRole);
    }
    m_cmbLine->blockSignals(false);
    updatePeakDetails(m_cmbLine->currentIndex());

    if (!m_chkFilterIntensity->isChecked() && !m_chkFilterFWHM->isChecked()) {
        m_lblPeakCount->setText(QString("检测到 %1 个峰值 (未过滤)").arg(m_allPeaks.size()));
    } else if (m_detectedPeaks.isEmpty()) {
        m_lblPeakCount->setText(QString("无符合过滤条件的峰值 (共检测到 %1 个)").arg(m_allPeaks.size()));
    } else {
        m_lblPeakCount->setText(QString("检测到 %1 个峰值 (过滤后 %2 个)")
                                    .arg(m_allPeaks.size())
                                    .arg(m_detectedPeaks.size()));
    }

    int invalidCount = 0;
    for (const auto &r : m_peakResults) if (!r.valid) ++invalidCount;
    if (invalidCount) m_lblPeakCount->setText(m_lblPeakCount->text() + QStringLiteral("；%1 个面积不可用").arg(invalidCount));
    emit filterChanged();
}

void MetadataPanel::updatePeakDetails(int index)
{
    if (index < 0 || index >= m_detectedPeaks.size()) {
        m_lblPeakDetails->setText(QStringLiteral("暂无符合条件的谱线"));
        m_cmbLine->setToolTip(QString());
        return;
    }
    m_cmbLine->setToolTip(m_cmbLine->itemData(index, Qt::ToolTipRole).toString());
    const auto &result = m_peakResults[index];
    const QString area = result.valid ? QString::number(result.computedValue, 'f', 4) : QStringLiteral("不可用");
    m_lblPeakDetails->setText(QStringLiteral("强度  %1\n半高宽  %2 nm\n净峰积分面积  %3\n积分区间  %4 – %5 nm\n%6")
        .arg(m_detectedPeaks[index].intensity, 0, 'f', 2)
        .arg(m_peakFwhmValues[index], 0, 'f', 4)
        .arg(area).arg(result.integrationLeft, 0, 'f', 3)
        .arg(result.integrationRight, 0, 'f', 3).arg(result.areaStatus));
}

void MetadataPanel::setMetadata(const SpectrumMetadata &meta,
                                 const QString &fileName,
                                 int dataPointCount)
{
    m_lblFileName->setText(fileName);
    m_lblFileName->setToolTip(fileName);
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
    m_lblFileName->setText(QStringLiteral("尚未选择文件"));
    m_lblFileName->setToolTip(QString());
    m_lblDataPoints->setText("-");
    m_lblLaserVoltage->setText("-");
    m_lblLaserFreq->setText("-");
    m_lblLaserDivider->setText("-");
    m_lblIntegTime->setText("-");
    m_lblAveraging->setText("-");
    m_lblTriggerMode->setText("-");
    m_lblTriggerDelay->setText("-");

    m_allPeaks.clear();
    m_allResults.clear();
    m_peakResults.clear();
    m_allComputedValues.clear();
    m_allFwhmValues.clear();
    m_detectedPeaks.clear();
    m_cmbLine->clear();
    updatePeakDetails(-1);
    emit filterChanged();
    m_lblPeakCount->setText("未检测到峰值");
    m_spectrumPoints.clear();
}
