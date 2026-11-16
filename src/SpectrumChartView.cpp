#include "SpectrumChartView.h"
#include "SpectrumData.h"
#include "SpectralAnalyzer.h"

#include <QVBoxLayout>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QValueAxis>
#include <QtCharts/QScatterSeries>
#include <QGraphicsScene>
#include <QFont>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStackedLayout>

// Qt5/Qt6 兼容层
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define EVENT_POS(event) (event)->position()
#else
#define EVENT_POS(event) ((event)->type() == QEvent::Wheel ? \
    ((QWheelEvent*)(event))->posF() : \
    ((QMouseEvent*)(event))->localPos())
#endif

// ==================== ZoomableChartView ====================

ZoomableChartView::ZoomableChartView(QChart *chart, QWidget *parent)
    : QChartView(chart, parent)
{
    setRenderHint(QPainter::Antialiasing);
    setMouseTracking(true);
}

void ZoomableChartView::setOriginalRange(double xMin, double xMax, double yMin, double yMax)
{
    m_origXMin = xMin;
    m_origXMax = xMax;
    m_origYMin = yMin;
    m_origYMax = yMax;
    m_origSet  = true;
}

void ZoomableChartView::resetZoom()
{
    if (!m_origSet) return;
    chart()->zoomReset();
    auto axes = chart()->axes();
    for (auto *axis : axes) {
        if (auto *valAxis = qobject_cast<QValueAxis *>(axis)) {
            if (axis->orientation() == Qt::Horizontal)
                valAxis->setRange(m_origXMin, m_origXMax);
            else
                valAxis->setRange(m_origYMin, m_origYMax);
        }
    }
}

// ========== 滚轮缩放 ==========

void ZoomableChartView::wheelEvent(QWheelEvent *event)
{
    QPointF chartPos = chart()->mapToValue(EVENT_POS(event));

    const double factor = (event->angleDelta().y() > 0) ? 1.03 : (1.0 / 1.03);

    // 滚轮 → X 轴，Shift+滚轮 → Y 轴
    double fx = (event->modifiers() & Qt::ShiftModifier) ? 1.0 : factor;
    double fy = (event->modifiers() & Qt::ShiftModifier) ? factor : 1.0;

    applyZoom(chartPos, fx, fy);
    emit viewChanged();
    event->accept();
}

void ZoomableChartView::applyZoom(const QPointF &center, double factorX, double factorY)
{
    auto axes = chart()->axes();
    for (auto *axis : axes) {
        auto *valAxis = qobject_cast<QValueAxis *>(axis);
        if (!valAxis) continue;

        bool isX = (axis->orientation() == Qt::Horizontal);
        double f = isX ? factorX : factorY;
        if (std::abs(f - 1.0) < 1e-9) continue;

        double c = isX ? center.x() : center.y();
        double lo = valAxis->min();
        double hi = valAxis->max();

        double newLo = c - (c - lo) / f;
        double newHi = c + (hi - c) / f;

        if (newHi - newLo > 0.0001)
            valAxis->setRange(newLo, newHi);
    }
}

// ========== 鼠标拖拽平移 ==========

void ZoomableChartView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPanning   = true;
        m_lastPanPos  = EVENT_POS(event);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QChartView::mousePressEvent(event);
}

void ZoomableChartView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning) {
        // 计算鼠标移动在图表坐标系中的位移
        QPointF delta = chart()->mapToValue(m_lastPanPos)
                      - chart()->mapToValue(EVENT_POS(event));

        // 平移所有坐标轴
        auto axes = chart()->axes();
        for (auto *axis : axes) {
            auto *valAxis = qobject_cast<QValueAxis *>(axis);
            if (!valAxis) continue;

            double shift = (axis->orientation() == Qt::Horizontal) ? delta.x() : delta.y();
            double lo = valAxis->min() + shift;
            double hi = valAxis->max() + shift;
            valAxis->setRange(lo, hi);
        }

        m_lastPanPos = EVENT_POS(event);
        emit viewChanged();
        event->accept();
        return;
    }
    QChartView::mouseMoveEvent(event);
}

void ZoomableChartView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isPanning) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QChartView::mouseReleaseEvent(event);
}

void ZoomableChartView::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    resetZoom();
    emit viewChanged();
}

// ==================== SpectrumChartView ====================

SpectrumChartView::SpectrumChartView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("chartPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 14);
    layout->setSpacing(8);
    auto *toolbar = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("光谱曲线"), this);
    title->setProperty("role", "section");
    toolbar->addWidget(title);
    toolbar->addStretch();
    auto *reset = new QPushButton(QStringLiteral("复位视图"), this);
    reset->setObjectName("resetChartView");
    reset->setToolTip(QStringLiteral("恢复完整波长与强度范围，也可双击图表"));
    toolbar->addWidget(reset);
    layout->addLayout(toolbar);
    m_sourceLabel = new QLabel(QStringLiteral("选择左侧文件以查看光谱"), this);
    m_sourceLabel->setProperty("role", "secondary");
    m_sourceLabel->setWordWrap(true);
    layout->addWidget(m_sourceLabel);
    setupChart();
    auto *canvas = new QWidget(this);
    auto *stack = new QStackedLayout(canvas);
    stack->setStackingMode(QStackedLayout::StackAll);
    stack->setContentsMargins(0, 0, 0, 0);
    stack->addWidget(m_chartView);
    m_emptyState = new QLabel(QStringLiteral("让光谱数据清晰可见\n\n导入 CSV，然后选择一个文件开始分析"), canvas);
    m_emptyState->setObjectName("chartEmptyState");
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setProperty("role", "secondary");
    m_emptyState->setAttribute(Qt::WA_TransparentForMouseEvents);
    stack->addWidget(m_emptyState);
    stack->setCurrentWidget(m_emptyState);
    layout->addWidget(canvas, 1);
    auto *hint = new QLabel(QStringLiteral("滚轮缩放波长 · Shift + 滚轮缩放强度 · 拖动平移 · 双击复位"), this);
    hint->setProperty("role", "secondary");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    connect(reset, &QPushButton::clicked, this, [this]() {
        m_chartView->resetZoom();
        updatePeakLabels();
    });
}

void SpectrumChartView::setupChart()
{
    m_series = new QLineSeries(this);
    m_series->setName("光谱强度");

    // 峰值标记散点
    m_markers = new QScatterSeries(this);
    m_markers->setMarkerSize(6);
    m_markers->setColor(QColor("#ee8157"));
    m_markers->setBorderColor(QColor("#d7643c"));

    m_chart = new QChart();
    m_chart->addSeries(m_series);
    m_chart->addSeries(m_markers);
    m_chart->legend()->hide();
    m_chart->setBackgroundBrush(Qt::white);
    m_chart->setBackgroundRoundness(0);
    m_chart->setDropShadowEnabled(false);
    m_chart->setMargins(QMargins(0, 12, 0, 0));
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_series->setPen(QPen(QColor("#007aff"), 1.8));

    m_axisX = new QValueAxis(this);
    m_axisX->setTitleText("波长 (nm)");
    m_axisX->setLabelFormat("%.0f");

    m_axisY = new QValueAxis(this);
    m_axisY->setTitleText("强度");
    m_axisY->setLabelFormat("%.0f");

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisX);
    m_series->attachAxis(m_axisY);
    m_markers->attachAxis(m_axisX);
    m_markers->attachAxis(m_axisY);

    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);

    for (auto *axis : {m_axisX, m_axisY}) {
        axis->setGridLinePen(QPen(QColor("#edf0f4"), 1));
        axis->setLinePen(QPen(QColor("#d9dfe7"), 1));
        axis->setLabelsColor(QColor("#758093"));
        axis->setTitleBrush(QColor("#556174"));
    }
    m_chartView = new ZoomableChartView(m_chart, this);
    m_chartView->setFrameShape(QFrame::NoFrame);

    // 缩放/平移/复位后更新标签位置
    connect(m_chartView, &ZoomableChartView::viewChanged, this, &SpectrumChartView::updatePeakLabels);
    connect(m_chart, &QChart::plotAreaChanged, this, [this](const QRectF &) { updatePeakLabels(); });
}

void SpectrumChartView::setSpectrumData(const SpectrumData &data)
{
    if (!data.isValid())
        return;

    clearPeakLabels();
    m_series->replace(data.points);

    double wlPad  = (data.wavelengthMax() - data.wavelengthMin()) * 0.02;
    double intPad = (data.intensityMax()   - data.intensityMin())  * 0.05;
    if (wlPad  < 0.001) wlPad  = 1.0;
    if (intPad < 0.001) intPad = 1.0;

    double xMin = data.wavelengthMin() - wlPad;
    double xMax = data.wavelengthMax() + wlPad;
    double yMin = data.intensityMin()   - intPad;
    double yMax = data.intensityMax()   + intPad;

    m_axisX->setRange(xMin, xMax);
    m_axisY->setRange(yMin, yMax);

    m_chartView->setOriginalRange(xMin, xMax, yMin, yMax);
    m_sourceLabel->setText(data.fileName);
    m_sourceLabel->setToolTip(data.fileName);
    m_emptyState->hide();
}

void SpectrumChartView::clear()
{
    clearPeakLabels();
    m_series->clear();
    m_markers->clear();
    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);
    m_sourceLabel->setText(QStringLiteral("选择左侧文件以查看光谱"));
    m_sourceLabel->setToolTip(QString());
    m_emptyState->show();
}

// ========== 峰值编号标记 ==========

void SpectrumChartView::clearPeakLabels()
{
    for (auto *label : m_peakLabels) {
        if (label->scene())
            label->scene()->removeItem(label);
        delete label;
    }
    m_peakLabels.clear();
    m_peakPositions.clear();
}

void SpectrumChartView::setPeakMarkers(const QVector<DetectedPeak> &peaks)
{
    clearPeakLabels();
    m_markers->clear();

    if (peaks.isEmpty()) return;

    // 添加散点标记
    for (const auto &p : peaks)
        m_markers->append(p.wavelength, p.intensity);

    // 记录峰坐标用于标签定位
    for (const auto &p : peaks)
        m_peakPositions.append(QPointF(p.wavelength, p.intensity));

    updatePeakLabels();
}

void SpectrumChartView::updatePeakLabels()
{
    if (m_updatingLabels) return; // 防止递归
    m_updatingLabels = true;

    // 清除旧标签
    for (auto *label : m_peakLabels) {
        if (label->scene())
            label->scene()->removeItem(label);
        delete label;
    }
    m_peakLabels.clear();

    if (m_peakPositions.isEmpty()) { m_updatingLabels = false; return; }

    // 计算标签偏移：Y轴范围的一小部分
    double yRange = m_axisY->max() - m_axisY->min();
    double yOffset = yRange * 0.03;

    for (int i = 0; i < m_peakPositions.size(); ++i) {
        QPointF dataPt(m_peakPositions[i].x(), m_peakPositions[i].y() + yOffset);

        // Labels use chart-local coordinates and follow plot resizing.
        const QPointF chartPt = m_chart->mapToPosition(dataPt, m_series);
        auto *label = new QGraphicsTextItem(QString::number(i + 1), m_chart);
        QFont f = label->font();
        f.setPointSize(8);
        f.setBold(true);
        label->setFont(f);
        label->setDefaultTextColor(QColor("#c45b36"));
        label->setPos(chartPt.x() - label->boundingRect().width() / 2,
                      chartPt.y() - label->boundingRect().height());
        label->setVisible(m_chart->plotArea().contains(m_chart->mapToPosition(m_peakPositions[i], m_series)));
        label->setZValue(100);

        m_peakLabels.append(label);
    }

    m_updatingLabels = false;
}
