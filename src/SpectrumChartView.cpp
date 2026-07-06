#include "SpectrumChartView.h"
#include "SpectrumData.h"

#include <QVBoxLayout>

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
    // 创建系列
    m_series = new QLineSeries(this);
    m_series->setName("光谱强度");

    // 创建图表
    m_chart = new QChart();
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->setTitle("光谱图");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);

    // 创建坐标轴
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

    // 设置默认范围（无数据时）
    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);

    // 创建 ChartView
    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setRubberBand(QChartView::RectangleRubberBand); // 支持框选放大
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

    m_axisX->setRange(data.wavelengthMin() - wlPad,  data.wavelengthMax() + wlPad);
    m_axisY->setRange(data.intensityMin()   - intPad, data.intensityMax()   + intPad);

    m_chart->setTitle(QString("光谱图 — %1").arg(data.fileName));
}

void SpectrumChartView::clear()
{
    m_series->clear();
    m_axisX->setRange(0, 1000);
    m_axisY->setRange(0, 1000);
    m_chart->setTitle("光谱图");
}
