#pragma once

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class SpectrumData;

/// 中央光谱图表面板
class SpectrumChartView : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumChartView(QWidget *parent = nullptr);

    void setSpectrumData(const SpectrumData &data);
    void clear();

private:
    void setupChart();

    QChartView   *m_chartView = nullptr;
    QChart       *m_chart     = nullptr;
    QLineSeries  *m_series    = nullptr;
    QValueAxis   *m_axisX     = nullptr;
    QValueAxis   *m_axisY     = nullptr;
};
