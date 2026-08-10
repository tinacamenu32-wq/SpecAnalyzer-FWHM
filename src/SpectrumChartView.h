#pragma once

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>
#include <QPointF>
#include <QGraphicsTextItem>
#include <QVector>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// Qt5: Charts 类在 QtCharts 命名空间中
using namespace QtCharts;
#endif

class SpectrumData;
struct DetectedPeak;

/// 支持滚轮缩放 + 拖拽平移的自定义 ChartView
class ZoomableChartView : public QChartView {
    Q_OBJECT
public:
    explicit ZoomableChartView(QChart *chart, QWidget *parent = nullptr);

    /// 重置缩放为原始范围
    void resetZoom();

    /// 设置数据加载时的原始坐标范围
    void setOriginalRange(double xMin, double xMax, double yMin, double yMax);

signals:
    void viewChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void applyZoom(const QPointF &center, double factorX, double factorY);

    double m_origXMin = 0, m_origXMax = 1000;
    double m_origYMin = 0, m_origYMax = 1000;
    bool   m_origSet = false;

    bool    m_isPanning = false;
    QPointF m_lastPanPos;
};

/// 中央光谱图表面板
class SpectrumChartView : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumChartView(QWidget *parent = nullptr);

    void setSpectrumData(const SpectrumData &data);
    void clear();

    /// 在图上显示峰值编号标记
    void setPeakMarkers(const QVector<DetectedPeak> &peaks);

private slots:
    void updatePeakLabels();

private:
    void setupChart();
    void clearPeakLabels();

    ZoomableChartView *m_chartView = nullptr;
    QChart            *m_chart     = nullptr;
    QLineSeries       *m_series    = nullptr;
    QScatterSeries    *m_markers   = nullptr; // 峰值散点
    QValueAxis        *m_axisX     = nullptr;
    QValueAxis        *m_axisY     = nullptr;

    // 编号标签
    QVector<QGraphicsTextItem *> m_peakLabels;
    QVector<QPointF>             m_peakPositions; // 峰坐标 (wl, intensity)
    bool m_updatingLabels = false;
};
