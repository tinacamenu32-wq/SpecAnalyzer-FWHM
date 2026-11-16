#include "MainWindow.h"
#include "CsvParser.h"
#include "SpectralAnalyzer.h"
#include "CsvExport.h"

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
#include <QFrame>
#include <QSet>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("SpecAnalyzer · 光谱分析"));
    resize(1440, 920);
    setMinimumSize(1100, 700);

    auto *workspace = new QWidget(this);
    workspace->setObjectName("workspace");
    auto *layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(16, 8, 16, 0);
    layout->setSpacing(12);
    auto *header = new QFrame(workspace);
    header->setObjectName("appHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(6, 6, 6, 6);
    auto *heading = new QLabel(QStringLiteral("光谱分析"), header);
    heading->setProperty("role", "title");
    headerLayout->addWidget(heading);
    auto *subtitle = new QLabel(QStringLiteral("SpecAnalyzer / FWHM"), header);
    subtitle->setProperty("role", "secondary");
    headerLayout->addWidget(subtitle);
    headerLayout->addStretch();
    auto *badge = new QLabel(QStringLiteral("净峰积分面积分析"), header);
    badge->setProperty("role", "badge");
    headerLayout->addWidget(badge);
    layout->addWidget(header);

    m_mainSplitter = new QSplitter(Qt::Horizontal, workspace);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(10);
    m_fileListPanel = new FileListPanel(m_mainSplitter);
    m_fileListPanel->setMinimumWidth(230);
    m_chartView = new SpectrumChartView(m_mainSplitter);
    m_chartView->setMinimumWidth(420);
    m_metadataPanel = new MetadataPanel(m_mainSplitter);
    m_metadataPanel->setMinimumWidth(310);
    m_mainSplitter->addWidget(m_fileListPanel);
    m_mainSplitter->addWidget(m_chartView);
    m_mainSplitter->addWidget(m_metadataPanel);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);
    m_mainSplitter->setSizes({250, 790, 350});
    layout->addWidget(m_mainSplitter, 1);
    setCentralWidget(workspace);

    // 信号连接
    connect(m_fileListPanel, &FileListPanel::fileSelected,
            this, &MainWindow::onFileSelected);
    connect(m_fileListPanel, &FileListPanel::exportRequested,
            this, &MainWindow::onExportResults);
    connect(m_metadataPanel, &MetadataPanel::sgParamsChanged,
            this, &MainWindow::onSGParamsChanged);
    connect(m_metadataPanel, &MetadataPanel::blParamsChanged,
            this, &MainWindow::onBLParamsChanged);
    connect(m_metadataPanel, &MetadataPanel::filterChanged,
            this, &MainWindow::onFilterChanged);
    connect(m_metadataPanel, &MetadataPanel::peakExportRequested,
            this, &MainWindow::onPeakBasedExport);

    // 状态栏
    statusBar()->showMessage(QStringLiteral("就绪 · 导入 CSV 文件开始分析"));
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

    m_currentFilePath = filePath;
    statusBar()->showMessage(QString("正在加载: %1").arg(filePath));

    // 检查缓存
    if (m_dataCache.contains(filePath) && m_dataCache[filePath].isValid()) {
        refreshCurrentFile();
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

    // 缓存原始数据并处理
    SpectrumData data = result.value();
    m_dataCache[filePath] = data;
    m_processedCache[filePath] = processPipeline(data.points);
    refreshCurrentFile();
}

void MainWindow::onSGParamsChanged()
{
    m_sgParams = m_metadataPanel->sgParams();
    reprocessAll();
}

void MainWindow::onBLParamsChanged()
{
    m_blParams = m_metadataPanel->blParams();
    reprocessAll();
}

void MainWindow::onFilterChanged()
{
    // 峰值过滤条件改变，刷新图表标记
    if (!m_currentFilePath.isEmpty()) {
        m_chartView->setPeakMarkers(m_metadataPanel->detectedPeaks());
    }
}

void MainWindow::reprocessAll()
{
    for (auto it = m_dataCache.begin(); it != m_dataCache.end(); ++it) {
        if (it.value().isValid())
            m_processedCache[it.key()] = processPipeline(it.value().points);
    }
    refreshCurrentFile();
}

QVector<QPointF> MainWindow::processPipeline(const QVector<QPointF> &rawPoints)
{
    QVector<QPointF> processed = rawPoints;
    processed = SpectralAnalyzer::savitzkyGolay(processed, m_sgParams);
    processed = SpectralAnalyzer::subtractBaseline(processed, m_blParams);
    return processed;
}

void MainWindow::refreshCurrentFile()
{
    if (m_currentFilePath.isEmpty()) return;
    if (!m_dataCache.contains(m_currentFilePath)) return;

    const SpectrumData &data = m_dataCache[m_currentFilePath];
    QVector<QPointF> processed = m_processedCache.value(m_currentFilePath);

    SpectrumData displayData = data;
    displayData.points = processed;
    m_chartView->setSpectrumData(displayData);
    m_metadataPanel->setMetadata(data.metadata, data.fileName, data.pointCount());
    m_metadataPanel->setSpectrumPoints(processed);
    m_chartView->setPeakMarkers(m_metadataPanel->detectedPeaks());

    QStringList tags;
    if (m_sgParams.enabled) tags << "S-G";
    if (m_blParams.enabled) tags << "去本底";
    QString tag = tags.isEmpty() ? "原始" : tags.join("+");
    statusBar()->showMessage(QString("已显示: %1 (%2 个数据点, %3)")
                                 .arg(data.fileName)
                                 .arg(data.pointCount())
                                 .arg(tag));
}

void MainWindow::onExportResults()
{
    QStringList allPaths = m_fileListPanel->allFilePaths();
    if (allPaths.isEmpty()) {
        QMessageBox::information(this, "导出", "没有已加载的文件。");
        return;
    }

    // --- 文件选择 ---
    QDialog dlg(this);
    dlg.setWindowTitle("选择要导出的文件");
    dlg.resize(450, 350);
    auto *dlgLayout = new QVBoxLayout(&dlg);
    auto *selectAllBtn = new QPushButton("全选", &dlg);
    auto *deselectAllBtn = new QPushButton("取消全选", &dlg);
    auto *topBtnLayout = new QHBoxLayout();
    topBtnLayout->addWidget(selectAllBtn); topBtnLayout->addWidget(deselectAllBtn); topBtnLayout->addStretch();
    dlgLayout->addLayout(topBtnLayout);
    QListWidget *listWidget = new QListWidget(&dlg);
    QVector<QCheckBox*> checkBoxes;
    for (const QString &path : allPaths) {
        auto *item = new QListWidgetItem();
        auto *cb = new QCheckBox(QFileInfo(path).fileName());
        cb->setChecked(true); cb->setProperty("filePath", path);
        listWidget->addItem(item); listWidget->setItemWidget(item, cb);
        checkBoxes.append(cb);
    }
    dlgLayout->addWidget(listWidget);
    connect(selectAllBtn, &QPushButton::clicked, &dlg, [&]() { for (auto *cb : checkBoxes) cb->setChecked(true); });
    connect(deselectAllBtn, &QPushButton::clicked, &dlg, [&]() { for (auto *cb : checkBoxes) cb->setChecked(false); });
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttonBox->button(QDialogButtonBox::Ok)->setText("导出");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");
    dlgLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    QStringList filePaths;
    for (auto *cb : checkBoxes)
        if (cb->isChecked()) filePaths.append(cb->property("filePath").toString());
    filePaths.removeDuplicates();
    if (filePaths.isEmpty()) { QMessageBox::information(this, "导出", "没有选中任何文件。"); return; }

    QString savePath = QFileDialog::getSaveFileName(this, "导出峰值表", QString(), "CSV 文件 (*.csv)");
    if (savePath.isEmpty()) return;

    QProgressDialog progress("正在分析...", "取消", 0, 2 * filePaths.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    // 收集过滤条件
    bool filterIntensity = m_metadataPanel->isFilterIntensityEnabled();
    bool filterFWHM = m_metadataPanel->isFilterFWHMEnabled();
    double minInt = m_metadataPanel->filterMinIntensity();
    double minFH = m_metadataPanel->filterMinFWHM();
    double maxFH = m_metadataPanel->filterMaxFWHM();

    // Analyze all components before filtering, once per file.
    QMap<QString, QVector<DetectedPeak>> peaksByFile;
    QMap<QString, QVector<SpectralLineResult>> resultsByFile;
    struct WavelengthGroup { double sum; int count; QSet<QString> files; };
    QVector<WavelengthGroup> wavelengthGroups;
    for (int fileIndex=0; fileIndex<filePaths.size(); ++fileIndex) {
        const QString &path=filePaths[fileIndex];
        progress.setValue(fileIndex);
        progress.setLabelText(QStringLiteral("联合分析：%1").arg(QFileInfo(path).fileName()));
        if (progress.wasCanceled()) return;
        QApplication::processEvents();
        ensureFileParsed(path);
        if (!m_dataCache[path].isValid()) continue;
        if (!m_processedCache.contains(path))
            m_processedCache[path] = processPipeline(m_dataCache[path].points);
        const auto peaks = SpectralAnalyzer::detectPeaks(m_processedCache[path]);
        const auto results = SpectralAnalyzer::analyzePeaks(m_processedCache[path], peaks);
        peaksByFile[path] = peaks;
        resultsByFile[path] = results;
        for (int i=0; i<peaks.size(); ++i) {
            const auto &p=peaks[i]; const auto &r=results[i];
            if (filterIntensity && p.intensity<minInt) continue;
            if (filterFWHM && r.valid && (r.computedValue<minFH || r.computedValue>maxFH)) continue;
            // Never merge two distinct components from the same spectrum into
            // one wavelength column, even when their separation is <0.5 nm.
            int best=-1; double distance=.5;
            for (int g=0; g<wavelengthGroups.size(); ++g) {
                const auto &group=wavelengthGroups[g];
                const double d=std::abs(group.sum/group.count-p.wavelength);
                if (!group.files.contains(path) && d<distance) {best=g;distance=d;}
            }
            if (best<0) wavelengthGroups.append({p.wavelength,1,QSet<QString>{path}});
            else {wavelengthGroups[best].sum+=p.wavelength; ++wavelengthGroups[best].count; wavelengthGroups[best].files.insert(path);}
        }
    }
    QVector<double> groupWls;
    for(const auto &g:wavelengthGroups) groupWls.append(g.sum/g.count);
    std::sort(groupWls.begin(),groupWls.end());

    if (groupWls.isEmpty()) { QMessageBox::information(this, "导出", "未检测到符合过滤条件的峰值。"); return; }

    // 逐文件匹配
    QString csv;
    QTextStream stream(&csv);
    CsvExport::writeHeader(stream, groupWls.size());

    int totalFound = 0;
    QVector<QVector<double>> fwhmValues(groupWls.size());

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progress.wasCanceled()) break;
        const QString &path = filePaths[i];
        progress.setValue(filePaths.size() + i);
        progress.setLabelText(QStringLiteral("写入结果：%1").arg(QFileInfo(path).fileName()));
        QApplication::processEvents();

        const SpectrumData &data = m_dataCache[path];
        stream << CsvExport::quoted(QFileInfo(path).fileName());
        if (!data.isValid()) {
            for (int j = 0; j < groupWls.size(); ++j) CsvExport::writeMissing(stream, QStringLiteral("文件解析失败"));
            stream << "\n"; continue;
        }
        if (!m_processedCache.contains(path))
            m_processedCache[path] = processPipeline(data.points);
        const auto filePeaks = peaksByFile.value(path);
        const auto fileResults = resultsByFile.value(path);
        const auto matches = CsvExport::matchPeaks(groupWls, filePeaks);
        for (int j=0; j<groupWls.size(); ++j) {
            const int match=matches[j];
            if (match<0) {CsvExport::writeMissing(stream);continue;}
            const auto &p=filePeaks[match]; const auto &result=fileResults[match];
            if (filterIntensity && p.intensity<minInt) {CsvExport::writeMissing(stream,QStringLiteral("未通过强度过滤"));continue;}
            if (filterFWHM && result.valid && (result.computedValue<minFH || result.computedValue>maxFH)) {
                CsvExport::writeMissing(stream,QStringLiteral("未通过面积过滤"));continue;
            }
            CsvExport::writePeak(stream,p,result);
            if (result.valid && !result.truncated) fwhmValues[j].append(result.computedValue);
            ++totalFound;
        }
        stream << "\n";
    }

    // Invalid fits and partial peaks do not enter RSD statistics.
    CsvExport::writeRsd(stream, fwhmValues);

    progress.setValue(2 * filePaths.size());
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", QString("无法写入文件:\n%1").arg(savePath));
        return;
    }
    if (!CsvExport::writeUtf8(file, csv)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("写入文件失败，请检查磁盘空间和权限。"));
        return;
    }
    file.close();

    statusBar()->showMessage(QString("已导出 %1 个文件 → %2").arg(filePaths.size()).arg(savePath));
    QMessageBox::information(this, "导出完成", QString("成功导出 %1 个文件，共 %2 条记录。").arg(filePaths.size()).arg(totalFound));
}

void MainWindow::onPeakBasedExport()
{
    // 获取当前文件检测到的峰值
    const auto &peaks = m_metadataPanel->detectedPeaks();
    if (peaks.isEmpty()) {
        QMessageBox::information(this, "导出", "当前文件未检测到峰值。");
        return;
    }

    // ---- 第1步：选择要导出的峰值 ----
    QDialog peakDlg(this);
    peakDlg.setWindowTitle("选择要导出的峰值");
    peakDlg.resize(400, 350);

    auto *peakDlgLayout = new QVBoxLayout(&peakDlg);

    auto *peakSelectAll = new QPushButton("全选", &peakDlg);
    auto *peakDeselectAll = new QPushButton("取消全选", &peakDlg);
    auto *peakTopLayout = new QHBoxLayout();
    peakTopLayout->addWidget(peakSelectAll);
    peakTopLayout->addWidget(peakDeselectAll);
    peakTopLayout->addStretch();
    peakDlgLayout->addLayout(peakTopLayout);

    QListWidget *peakListWidget = new QListWidget(&peakDlg);
    QVector<QCheckBox*> peakCheckBoxes;
    for (int i = 0; i < peaks.size(); ++i) {
        const auto &p = peaks[i];
        auto *item = new QListWidgetItem();
        auto *cb = new QCheckBox(QString("#%1  %2 nm  强度:%3")
                                     .arg(i + 1).arg(p.wavelength, 0, 'f', 2).arg(p.intensity, 0, 'f', 0));
        cb->setChecked(true);
        cb->setProperty("peakIdx", i);
        peakListWidget->addItem(item);
        peakListWidget->setItemWidget(item, cb);
        peakCheckBoxes.append(cb);
    }
    peakDlgLayout->addWidget(peakListWidget);

    connect(peakSelectAll, &QPushButton::clicked, &peakDlg, [&]() {
        for (auto *cb : peakCheckBoxes) cb->setChecked(true);
    });
    connect(peakDeselectAll, &QPushButton::clicked, &peakDlg, [&]() {
        for (auto *cb : peakCheckBoxes) cb->setChecked(false);
    });

    auto *peakBtnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &peakDlg);
    peakBtnBox->button(QDialogButtonBox::Ok)->setText("下一步");
    peakBtnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    peakDlgLayout->addWidget(peakBtnBox);
    connect(peakBtnBox, &QDialogButtonBox::accepted, &peakDlg, &QDialog::accept);
    connect(peakBtnBox, &QDialogButtonBox::rejected, &peakDlg, &QDialog::reject);

    if (peakDlg.exec() != QDialog::Accepted) return;

    QVector<double> selectedWavelengths;
    for (auto *cb : peakCheckBoxes) {
        if (cb->isChecked()) {
            int idx = cb->property("peakIdx").toInt();
            selectedWavelengths.append(peaks[idx].wavelength);
        }
    }
    if (selectedWavelengths.isEmpty()) {
        QMessageBox::information(this, "导出", "未选择任何峰值。");
        return;
    }

    // ---- 第2步：选择要导出的文件（复用现有逻辑）----
    QStringList allPaths = m_fileListPanel->allFilePaths();
    QDialog fileDlg(this);
    fileDlg.setWindowTitle("选择要分析的文件");
    fileDlg.resize(450, 350);

    auto *fileDlgLayout = new QVBoxLayout(&fileDlg);
    auto *fileSelectAll = new QPushButton("全选", &fileDlg);
    auto *fileDeselectAll = new QPushButton("取消全选", &fileDlg);
    auto *fileTopLayout = new QHBoxLayout();
    fileTopLayout->addWidget(fileSelectAll);
    fileTopLayout->addWidget(fileDeselectAll);
    fileTopLayout->addStretch();
    fileDlgLayout->addLayout(fileTopLayout);

    QListWidget *fileListWidget = new QListWidget(&fileDlg);
    QVector<QCheckBox*> fileCheckBoxes;
    for (const QString &path : allPaths) {
        auto *item = new QListWidgetItem();
        auto *cb = new QCheckBox(QFileInfo(path).fileName());
        cb->setChecked(true);
        cb->setProperty("filePath", path);
        fileListWidget->addItem(item);
        fileListWidget->setItemWidget(item, cb);
        fileCheckBoxes.append(cb);
    }
    fileDlgLayout->addWidget(fileListWidget);
    connect(fileSelectAll, &QPushButton::clicked, &fileDlg, [&]() {
        for (auto *cb : fileCheckBoxes) cb->setChecked(true);
    });
    connect(fileDeselectAll, &QPushButton::clicked, &fileDlg, [&]() {
        for (auto *cb : fileCheckBoxes) cb->setChecked(false);
    });

    auto *fileBtnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &fileDlg);
    fileBtnBox->button(QDialogButtonBox::Ok)->setText("导出");
    fileBtnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    fileDlgLayout->addWidget(fileBtnBox);
    connect(fileBtnBox, &QDialogButtonBox::accepted, &fileDlg, &QDialog::accept);
    connect(fileBtnBox, &QDialogButtonBox::rejected, &fileDlg, &QDialog::reject);

    if (fileDlg.exec() != QDialog::Accepted) return;

    QStringList filePaths;
    for (auto *cb : fileCheckBoxes) {
        if (cb->isChecked())
            filePaths.append(cb->property("filePath").toString());
    }
    filePaths.removeDuplicates();
    if (filePaths.isEmpty()) {
        QMessageBox::information(this, "导出", "未选择任何文件。");
        return;
    }

    // ---- 选择保存位置 ----
    QString savePath = QFileDialog::getSaveFileName(this, "导出所选峰值分析结果", QString(), "CSV 文件 (*.csv)");
    if (savePath.isEmpty()) return;

    // ---- 第3步：逐文件分析 ----
    QProgressDialog progress("正在分析...", "取消", 0, filePaths.size(), this);
    progress.setWindowModality(Qt::WindowModal);

    QString csv;
    QTextStream stream(&csv);

    // 表头：文件名 + 每个峰七列（含区间和计算状态）
    CsvExport::writeHeader(stream, selectedWavelengths.size());

    int totalFound = 0;

    // 收集每个峰在所有文件中的净峰积分面积值，用于计算 RSD
    QVector<QVector<double>> fwhmValues(selectedWavelengths.size());

    for (int i = 0; i < filePaths.size(); ++i) {
        if (progress.wasCanceled()) break;
        const QString &path = filePaths[i];
        progress.setLabelText(QString("分析: %1").arg(QFileInfo(path).fileName()));
        progress.setValue(i);
        QApplication::processEvents();

        ensureFileParsed(path);
        const SpectrumData &data = m_dataCache[path];
        QString fileName = QFileInfo(path).fileName();

        stream << CsvExport::quoted(fileName);

        if (!data.isValid()) {
            for (int j = 0; j < selectedWavelengths.size(); ++j)
                CsvExport::writeMissing(stream, QStringLiteral("文件解析失败"));
            stream << "\n";
            continue;
        }

        if (!m_processedCache.contains(path))
            m_processedCache[path] = processPipeline(data.points);
        QVector<QPointF> pts = m_processedCache.value(path);
        QVector<DetectedPeak> filePeaks = SpectralAnalyzer::detectPeaks(pts);
        const auto fileResults = SpectralAnalyzer::analyzePeaks(pts, filePeaks);
        const auto matches = CsvExport::matchPeaks(selectedWavelengths, filePeaks);

        bool filterIntensity = m_metadataPanel->isFilterIntensityEnabled();
        bool filterFWHM = m_metadataPanel->isFilterFWHMEnabled();
        double minInt = m_metadataPanel->filterMinIntensity();
        double minFH = m_metadataPanel->filterMinFWHM();
        double maxFH = m_metadataPanel->filterMaxFWHM();

        for (int j=0; j<selectedWavelengths.size(); ++j) {
            const int match=matches[j];
            if (match<0) {CsvExport::writeMissing(stream);continue;}
            const auto &p=filePeaks[match]; const auto &result=fileResults[match];
            if (filterIntensity && p.intensity<minInt) {CsvExport::writeMissing(stream,QStringLiteral("未通过强度过滤"));continue;}
            if (filterFWHM && result.valid && (result.computedValue<minFH || result.computedValue>maxFH)) {
                CsvExport::writeMissing(stream,QStringLiteral("未通过面积过滤"));continue;
            }
            CsvExport::writePeak(stream,p,result);
            if (result.valid && !result.truncated) fwhmValues[j].append(result.computedValue);
            ++totalFound;
        }
        stream << "\n";
    }

    CsvExport::writeRsd(stream, fwhmValues);

    progress.setValue(filePaths.size());

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", QString("无法写入文件:\n%1").arg(savePath));
        return;
    }
    if (!CsvExport::writeUtf8(file, csv)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("写入文件失败，请检查磁盘空间和权限。"));
        return;
    }
    file.close();

    statusBar()->showMessage(QString("已导出 %1 条记录 → %2").arg(totalFound).arg(savePath));
    QMessageBox::information(this, "导出完成", QString("成功导出 %1 个文件，共 %2 个峰值匹配。").arg(filePaths.size()).arg(totalFound));
}
