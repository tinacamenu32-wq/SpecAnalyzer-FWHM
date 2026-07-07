#include "MainWindow.h"
#include "CsvParser.h"
#include "SpectralAnalyzer.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QTextStream>
#include <QProgressDialog>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QScrollArea>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("SpecAnalyzer - 光谱分析器");
    resize(1280, 800);

    // 中央 splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);

    // ---- 左侧：文件列表 ----
    m_fileListPanel = new FileListPanel(this);
    m_fileListPanel->setMinimumWidth(200);

    // ---- 右侧：图表 + 元数据 ----
    m_rightSplitter = new QSplitter(Qt::Vertical, this);

    m_chartView = new SpectrumChartView(this);
    m_metadataPanel = new MetadataPanel(this);

    m_rightSplitter->addWidget(m_chartView);
    m_rightSplitter->addWidget(m_metadataPanel);
    m_rightSplitter->setStretchFactor(0, 85); // 图表占 85%
    m_rightSplitter->setStretchFactor(1, 15); // 元数据占 15%

    // 组装到大 splitter
    m_mainSplitter->addWidget(m_fileListPanel);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setStretchFactor(0, 1); // 左侧
    m_mainSplitter->setStretchFactor(1, 4); // 右侧占 80%
    m_mainSplitter->setSizes({250, 1030});

    // 信号连接
    connect(m_fileListPanel, &FileListPanel::fileSelected,
            this, &MainWindow::onFileSelected);
    connect(m_fileListPanel, &FileListPanel::exportRequested,
            this, &MainWindow::onExportResults);

    // 状态栏
    statusBar()->showMessage("就绪");
}

void MainWindow::loadInitialFiles(const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        statusBar()->showMessage("未发现 CSV 文件");
        return;
    }

    m_fileListPanel->addFiles(filePaths);
    statusBar()->showMessage(QString("已发现 %1 个 CSV 文件").arg(filePaths.size()));

    // 自动选中第一个文件
    if (m_fileListPanel->currentFilePath().isEmpty()) {
        auto *listWidget = m_fileListPanel->findChild<QListWidget *>();
        if (listWidget && listWidget->count() > 0) {
            listWidget->setCurrentRow(0);
            emit m_fileListPanel->fileSelected(filePaths.first());
        }
    }
}

void MainWindow::ensureFileParsed(const QString &filePath)
{
    if (m_dataCache.contains(filePath) && m_dataCache[filePath].isValid())
        return;

    QString error;
    auto result = CsvParser::parse(filePath, &error);
    if (result.has_value()) {
        m_dataCache[filePath] = result.value();
    } else {
        // 缓存一个空的无效数据，标记解析失败
        SpectrumData empty;
        empty.filePath = filePath;
        empty.fileName = QFileInfo(filePath).fileName();
        m_dataCache[filePath] = empty;
    }
}

void MainWindow::onFileSelected(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    statusBar()->showMessage(QString("正在加载: %1").arg(filePath));

    // 检查缓存
    if (m_dataCache.contains(filePath) && m_dataCache[filePath].isValid()) {
        const SpectrumData &data = m_dataCache[filePath];
        m_chartView->setSpectrumData(data);
        m_metadataPanel->setMetadata(data.metadata, data.fileName, data.pointCount());
        m_metadataPanel->setSpectrumPoints(data.points);
        statusBar()->showMessage(QString("已显示: %1 (%2 个数据点)")
                                     .arg(data.fileName)
                                     .arg(data.pointCount()));
        return;
    }

    // 解析（首次）
    QString error;
    auto result = CsvParser::parse(filePath, &error);
    if (!result.has_value()) {
        QMessageBox::warning(this, "解析失败",
                             QString("无法解析文件:\n%1\n\n错误: %2").arg(filePath, error));
        statusBar()->showMessage("解析失败");
        return;
    }

    // 缓存并展示
    SpectrumData data = result.value();
    m_dataCache[filePath] = data;

    m_chartView->setSpectrumData(data);
    m_metadataPanel->setMetadata(data.metadata, data.fileName, data.pointCount());
    m_metadataPanel->setSpectrumPoints(data.points);
    statusBar()->showMessage(QString("已显示: %1 (%2 个数据点)")
                                 .arg(data.fileName)
                                 .arg(data.pointCount()));
}

void MainWindow::onExportResults()
{
    QStringList allPaths = m_fileListPanel->allFilePaths();
    if (allPaths.isEmpty()) {
        QMessageBox::information(this, "导出", "没有已加载的文件。");
        return;
    }

    // --- 弹出文件选择对话框 ---
    QDialog dlg(this);
    dlg.setWindowTitle("选择要导出的文件");
    dlg.resize(450, 350);

    auto *dlgLayout = new QVBoxLayout(&dlg);

    auto *selectAllBtn = new QPushButton("全选", &dlg);
    auto *deselectAllBtn = new QPushButton("取消全选", &dlg);
    auto *topBtnLayout = new QHBoxLayout();
    topBtnLayout->addWidget(selectAllBtn);
    topBtnLayout->addWidget(deselectAllBtn);
    topBtnLayout->addStretch();
    dlgLayout->addLayout(topBtnLayout);

    QListWidget *listWidget = new QListWidget(&dlg);
    QVector<QCheckBox*> checkBoxes;
    for (const QString &path : allPaths) {
        QString name = QFileInfo(path).fileName();
        auto *item = new QListWidgetItem();
        auto *cb = new QCheckBox(name);
        cb->setChecked(true);
        cb->setProperty("filePath", path);
        listWidget->addItem(item);
        listWidget->setItemWidget(item, cb);
        checkBoxes.append(cb);
    }
    dlgLayout->addWidget(listWidget);

    connect(selectAllBtn, &QPushButton::clicked, &dlg, [&]() {
        for (auto *cb : checkBoxes) cb->setChecked(true);
    });
    connect(deselectAllBtn, &QPushButton::clicked, &dlg, [&]() {
        for (auto *cb : checkBoxes) cb->setChecked(false);
    });

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttonBox->button(QDialogButtonBox::Ok)->setText("导出");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
    dlgLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // 收集勾选的文件
    QStringList filePaths;
    for (auto *cb : checkBoxes) {
        if (cb->isChecked())
            filePaths.append(cb->property("filePath").toString());
    }

    if (filePaths.isEmpty()) {
        QMessageBox::information(this, "导出", "没有选中任何文件。");
        return;
    }

    // 选择保存位置
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "导出所有峰值 FWHM 分析结果",
        QString(),
        "CSV 文件 (*.csv)");
    if (savePath.isEmpty())
        return;

    // 进度对话框
    QProgressDialog progress("正在分析峰值...", "取消", 0, filePaths.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    // 构建 CSV 内容（宽格式：每文件一行，列 = 各峰波长+FWHM）
    QString csv;
    QTextStream stream(&csv);

    // 先收集所有文件的峰值信息（用于构建统一表头）
    struct PeakInfo {
        double wavelength;
        double intensity;
        double computedValue; // FWHM × 半峰
    };
    QVector<QPair<QString, QVector<PeakInfo>>> allResults; // (fileName, peaks)

    int totalPeaks = 0;
    int maxPeakCount = 0;

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progress.wasCanceled())
            break;

        const QString &path = filePaths[i];
        progress.setLabelText(QString("正在分析: %1").arg(QFileInfo(path).fileName()));
        progress.setValue(i);
        QApplication::processEvents();

        ensureFileParsed(path);
        const SpectrumData &data = m_dataCache[path];
        QString fileName = QFileInfo(path).fileName();

        QVector<PeakInfo> peakInfos;
        if (data.isValid()) {
            QVector<DetectedPeak> peaks = SpectralAnalyzer::detectPeaks(data.points);
            for (const auto &peak : peaks) {
                auto result = SpectralAnalyzer::analyzePeak(data.points, peak);
                PeakInfo info;
                info.wavelength    = peak.wavelength;
                info.intensity     = peak.intensity;
                info.computedValue = result.computedValue;
                peakInfos.append(info);
            }
            totalPeaks += peaks.size();
        }

        allResults.append({fileName, peakInfos});
        if (peakInfos.size() > maxPeakCount)
            maxPeakCount = peakInfos.size();
    }

    // 写表头
    stream << "文件名";
    for (int j = 0; j < maxPeakCount; ++j) {
        stream << QString(",峰值%1_波长(nm),峰值%1_强度,峰值%1_半高宽").arg(j + 1);
    }
    stream << "\n";

    // 写数据行：每文件一行
    for (const auto &[fileName, peakInfos] : allResults) {
        stream << "\"" << fileName << "\"";
        if (peakInfos.isEmpty()) {
            for (int j = 0; j < maxPeakCount; ++j)
                stream << ",,,";
        } else {
            for (const auto &info : peakInfos) {
                stream << "," << QString::number(info.wavelength, 'f', 2);
                stream << "," << QString::number(info.intensity, 'f', 2);
                stream << "," << QString::number(info.computedValue, 'f', 4);
            }
            // 补齐不足 maxPeakCount 的列
            for (int j = peakInfos.size(); j < maxPeakCount; ++j)
                stream << ",,,";
        }
        stream << "\n";
    }

    int successCount = allResults.size();

    progress.setValue(filePaths.size());

    // 写入文件
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败",
                             QString("无法写入文件:\n%1").arg(savePath));
        return;
    }

    QTextStream fileStream(&file);
    fileStream.setEncoding(QStringConverter::Utf8);
    fileStream << QChar(0xFEFF);
    fileStream << csv;
    file.close();

    statusBar()->showMessage(QString("已导出 %1 个文件共 %2 个峰值 → %3")
                                 .arg(successCount)
                                 .arg(totalPeaks)
                                 .arg(savePath));
    QMessageBox::information(this, "导出完成",
                             QString("成功导出 %1 个文件，共 %2 个峰值的 FWHM 分析结果。")
                                 .arg(successCount)
                                 .arg(totalPeaks));
}
