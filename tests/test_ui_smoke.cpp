#include "MainWindow.h"
#include "UiTheme.h"
#include <QApplication>
#include <QLineEdit>
#include <QTabWidget>
#include <QDebug>
#include <QDir>
#include <cmath>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    UiTheme::apply(app);
    MainWindow window;
    window.show();
    app.processEvents();
    auto *files = window.findChild<FileListPanel *>();
    auto *chart = window.findChild<SpectrumChartView *>();
    auto *metadata = window.findChild<MetadataPanel *>();
    if (!files || !chart || !metadata) return 1;
    auto *search = files->findChild<QLineEdit *>("fileSearch");
    auto *list = files->findChild<QListWidget *>();
    if (!files || !chart || !metadata || !search || !list) return 1;
    if(metadata->findChild<QCheckBox *>("includeApproximateRsd")) return 13;
    const QString output = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    if (!output.isEmpty()) {
        QDir().mkpath(output);
        window.grab().save(output + "/interface-empty.png");
    }
    files->addFiles({QStringLiteral("/preview/岩样_A_001.csv"), QStringLiteral("/preview/岩样_B_002.csv"), QStringLiteral("/preview/岩样_C_003.csv")});
    search->setText(QStringLiteral("B_002"));
    if (!list->item(0)->isHidden() || list->item(1)->isHidden() || !list->item(2)->isHidden()) return 2;
    search->clear();
    if (list->item(0)->isHidden()) return 3;
    list->blockSignals(true);
    list->setCurrentRow(0);
    list->blockSignals(false);
    SpectrumData data;
    data.fileName = QStringLiteral("岩样_A_001.csv");
    data.metadata.integrationTime = 10;
    data.metadata.averagingCount = 5;
    data.metadata.laserVoltage = 650;
    data.metadata.triggerMode = QStringLiteral("外部触发");
    for (int i = 0; i < 2400; ++i) {
        const double x = 350 + i * .2;
        double y = 80 + 18 * std::sin(x * .5);
        for (auto peak : {QPointF(393.6, 6200), QPointF(422.7, 2400), QPointF(588.6, 4100), QPointF(656.3, 1800), QPointF(766.5, 3300)})
            y += peak.y() * std::exp(-.5 * std::pow((x - peak.x()) / 1.5, 2));
        data.points.append(QPointF(x, y));
    }
    metadata->setMetadata(data.metadata, data.fileName, data.pointCount());
    chart->setSpectrumData(data);
    metadata->setSpectrumPoints(data.points);
    chart->setPeakMarkers(metadata->detectedPeaks());
    if (metadata->detectedPeaks().isEmpty()) return 4;
    for (auto *check : metadata->findChildren<QCheckBox *>()) {
        if (check->text().contains(QStringLiteral("S-G"))) {
            check->setChecked(false);
            if (metadata->sgParams().enabled) return 5;
            check->setChecked(true);
        }
    }
    auto *combo = metadata->findChild<QComboBox *>();
    if (!combo || !metadata->findChild<QLabel *>("peakDetails")->text().contains(QStringLiteral("峰值面积"))) return 6;
    const QString visibleDetails=metadata->findChild<QLabel *>("peakDetails")->text();
    if(visibleDetails.contains(QStringLiteral("近似")) || visibleDetails.contains(QStringLiteral("拟合")) || visibleDetails.contains(QStringLiteral("积分区间")) || visibleDetails.contains(QLatin1Char('\n'))) return 14;
    if(combo->itemData(0,Qt::ToolTipRole).toString().contains(QStringLiteral("切分"))) return 15;
    const auto allCandidates=SpectralAnalyzer::detectPeaks(data.points);
    const auto expectedAreas=SpectralAnalyzer::analyzePeaks(data.points, allCandidates);
    if(!metadata->findChild<QLabel *>("peakDetails")->text().contains(QString::number(expectedAreas[0].computedValue,'f',4))) return 10;
    for(auto *spin:metadata->findChildren<QDoubleSpinBox *>()) {
        if(spin->toolTip()==QStringLiteral("过滤掉强度低于此值的噪声峰")) {
            const double before=spin->value();
            spin->setValue(5000);
            if(metadata->detectedPeaks().size()!=1) return 11;
            if(!metadata->findChild<QLabel *>("peakDetails")->text().contains(QString::number(expectedAreas[0].computedValue,'f',4))) return 12;
            spin->setValue(before);
        }
    }
    app.processEvents();
    if (!output.isEmpty()) window.grab().save(output + "/interface-preview.png");
    auto *tabs = metadata->findChild<QTabWidget *>();
    if (!tabs || tabs->count() != 3) return 9;
    tabs->setCurrentIndex(1);
    app.processEvents();
    if (!output.isEmpty()) window.grab().save(output + "/interface-processing.png");
    tabs->setCurrentIndex(2);
    app.processEvents();
    auto *details = metadata->findChild<QWidget *>("acquisitionDetails");
    if (!details || !details->isVisible()) return 16;
    for (auto *button : metadata->findChildren<QAbstractButton *>())
        if (button->text() == QStringLiteral("采集参数")) return 17;
    if (!output.isEmpty()) window.grab().save(output + "/interface-file-details.png");
    tabs->setCurrentIndex(0);
    app.processEvents();
    window.resize(1100, 700);
    app.processEvents();
    if (files->width() < 230 || chart->width() < 420 || metadata->width() < 310) return 7;
    if (!output.isEmpty()) window.grab().save(output + "/interface-compact.png");
    chart->clear();
    if (!chart->findChild<QLabel *>("chartEmptyState")->isVisible()) return 8;
    qInfo() << "UI smoke checks passed: empty state, file search, parameters, peaks, compact layout";
    return 0;
}
