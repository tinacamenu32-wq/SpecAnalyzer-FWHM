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

// Qt5/Qt6 兼容层
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define EVENT_POS(event) (event)->position()
#else
#define EVENT_POS(event) ((event)->type() == QEvent::Wheel ? \
    static_cast<QWheelEvent*>(event)->posF() : \
    static_cast<QMouseEvent*>(event)->localPos())
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
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    setupChart();
    layout->addWidget(m_chartView);
}

void SpectrumChartView::setupChart()
{
    m_series = new QLineSeries(this);
    m_series->setName("光谱强度");

    // 峰值标记散点
    m_markers = new QScatterSeries(this);
    m_markers->setMarkerSize(6);
    m_markers->setColor(Qt::red);
    m_markers->setBorderColor(Qt::darkRed);

    m_chart = new QChart();
    m_chart->addSeries(m_series);
    m_chart->addSeries(m_markers);
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
    m_markers->attachAxis(m_axisX);
    m_markers->attachAxis(m_axisY);

    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);

    m_chartView = new ZoomableChartView(m_chart, this);

    // 缩放/平移/复位后更新标签位置
    connect(m_chartView, &ZoomableChartView::viewChanged, this, &SpectrumChartView::updatePeakLabels);
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
    m_chart->setTitle(QString("光谱图 — %1").arg(data.fileName));
}

void SpectrumChartView::clear()
{
    clearPeakLabels();
    m_series->clear();
    m_markers->clear();
    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);
    m_chart->setTitle("光谱图");
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

        // 数据坐标 → 场景坐标
        QPointF scenePt = m_chart->mapToPosition(dataPt);
        // 转换为图表视图坐标系
        QPointF viewPt = m_chartView->mapFromScene(scenePt);

        auto *label = new QGraphicsTextItem(QString::number(i + 1));
        QFont f = label->font();
        f.setPointSize(8);
        f.setBold(true);
        label->setFont(f);
        label->setDefaultTextColor(Qt::red);
        label->setPos(scenePt);
        label->setZValue(100);

        m_chart->scene()->addItem(label);
        m_peakLabels.append(label);
    }

    m_updatingLabels = false;
}
