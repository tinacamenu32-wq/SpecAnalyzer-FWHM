#include "SpectrumChartView.h"
#include "SpectrumData.h"

#include <QVBoxLayout>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QValueAxis>
#include <QtCharts/QScatterSeries>

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
    QPointF chartPos = chart()->mapToValue(event->position());

    const double factor = (event->angleDelta().y() > 0) ? 1.08 : (1.0 / 1.08);

    // 滚轮 → X 轴，Shift+滚轮 → Y 轴
    double fx = (event->modifiers() & Qt::ShiftModifier) ? 1.0 : factor;
    double fy = (event->modifiers() & Qt::ShiftModifier) ? factor : 1.0;

    applyZoom(chartPos, fx, fy);
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
        m_lastPanPos  = event->position();
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
                      - chart()->mapToValue(event->position());

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

        m_lastPanPos = event->position();
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
}

// ==================== SpectrumChartView ====================

SpectrumChartView::SpectrumChartView(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    setupChart();
    layout->addWidget(m_chartView);
}

void SpectrumChartView::setupChart()
{
    m_series = new QLineSeries(this);
    m_series->setName("光谱强度");

    m_chart = new QChart();
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->setTitle("光谱图");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

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

    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);

    m_chartView = new ZoomableChartView(m_chart, this);
}

void SpectrumChartView::setSpectrumData(const SpectrumData &data)
{
    if (!data.isValid())
        return;

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
    m_chart->setTitle(QString("光谱图 — %1").arg(data.fileName));
}

void SpectrumChartView::clear()
{
    m_series->clear();
    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);
    m_chart->setTitle("光谱图");
}
