/// 临时测试：验证二阶导数法找峰效果
/// 编译：cd build && cmake --build . && cd ..
/// 运行：./build/test_peaks 7.4日数据/4_20260704_113835_257.csv
#include "src/SpectralAnalyzer.h"
#include "src/CsvParser.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        qDebug() << "Usage: test_peaks <csv_file>";
        return 1;
    }

    QString path = argv[1];
    if (!QFileInfo::exists(path)) {
        qDebug() << "File not found:" << path;
        return 1;
    }

    // 1. 解析 CSV
    auto optData = CsvParser::parse(path);
    if (!optData.has_value()) {
        qDebug() << "Failed to parse CSV";
        return 1;
    }
    SpectrumData data = optData.value();
    if (data.points.isEmpty()) {
        qDebug() << "No data points in CSV";
        return 1;
    }
    qDebug() << "Loaded" << data.points.size() << "points from" << path;

    // 2. 跑 pipeline: SG 平滑 + ALS 基线扣除 + 峰值检测
    SpectralAnalyzer::SGParams sg;
    sg.enabled = true; sg.windowSize = 7; sg.polyOrder = 2;

    SpectralAnalyzer::BLParams bl;
    bl.enabled = true; bl.lambda = 1e5; bl.p = 0.01; bl.niter = 10;

    auto smoothed = SpectralAnalyzer::savitzkyGolay(data.points, sg);
    auto baselineRemoved = SpectralAnalyzer::subtractBaseline(smoothed, bl);

    // 3. 只跑局部极大值法（旧算法）
    auto peaksOld = SpectralAnalyzer::detectPeaks(baselineRemoved, 0.0, 5);
    // 过滤掉 from2ndDeriv 的，只看局部极大值
    QVector<DetectedPeak> onlyLocal;
    for (const auto &p : peaksOld) {
        if (!p.from2ndDeriv) onlyLocal.append(p);
    }

    // 4. 跑二阶导数法
    auto peaks2nd = SpectralAnalyzer::detectPeaks2ndDeriv(baselineRemoved, 0.0, 7, 2);

    qDebug() << "\n========== 结果对比 ==========";
    qDebug() << "局部极大值法找到:" << onlyLocal.size() << "个峰";
    qDebug() << "二阶导数法找到:" << peaks2nd.size() << "个峰";
    qDebug() << "综合结果(去重后):" << peaksOld.size() << "个峰";

    // 5. 列出二阶导数法新发现的峰（不在局部极大值结果中的）
    qDebug() << "\n--- 二阶导数法新增的峰(肩峰/隐藏峰) ---";
    int newCount = 0;
    for (const auto &p2 : peaks2nd) {
        bool found = false;
        for (const auto &p1 : onlyLocal) {
            if (std::abs(p1.index - p2.index) <= 5) { found = true; break; }
        }
        if (!found) {
            auto result = SpectralAnalyzer::analyzePeak(baselineRemoved, p2);
            qDebug().noquote()
                << QString("  λ=%1 nm  I=%2  FWHM=%3 nm  Area=%4  heightAboveBL=%5")
                       .arg(p2.wavelength, 8, 'f', 2)
                       .arg(p2.intensity, 6, 'f', 0)
                       .arg(result.fwhm, 0, 'f', 4)
                       .arg(result.peakArea, 0, 'f', 2)
                       .arg(result.heightAboveBaseline, 0, 'f', 0);
            newCount++;
        }
    }
    qDebug() << "二阶导数法新增了" << newCount << "个峰";

    // 6. 列出前 15 个最强的峰
    qDebug() << "\n--- Top 15 最强峰 ---";
    auto sorted = peaksOld;
    std::sort(sorted.begin(), sorted.end(),
              [](const DetectedPeak &a, const DetectedPeak &b) { return a.intensity > b.intensity; });
    for (int i = 0; i < std::min(15, (int)sorted.size()); ++i) {
        const auto &p = sorted[i];
        auto result = SpectralAnalyzer::analyzePeak(baselineRemoved, p);
        QString tag = p.from2ndDeriv ? "[二阶导数]" : "[局部极大]";
        qDebug().noquote()
            << QString("  #%1 %2 λ=%3 nm  I=%4  FWHM=%5 nm  prominence=%6")
                   .arg(i + 1, 2)
                   .arg(tag)
                   .arg(p.wavelength, 8, 'f', 2)
                   .arg(p.intensity, 6, 'f', 0)
                   .arg(result.fwhm, 0, 'f', 4)
                   .arg(p.prominence, 0, 'f', 0);
    }

    return 0;
}
