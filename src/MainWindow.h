#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QMap>

#include "FileListPanel.h"
#include "SpectrumChartView.h"
#include "MetadataPanel.h"
#include "SpectrumData.h"

/// 应用主窗口
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    /// 启动时加载初始文件列表（只加入列表，不解析）
    void loadInitialFiles(const QStringList &filePaths);

private slots:
    void onFileSelected(const QString &filePath);
    void onExportResults();

private:
    void setupUi();
    void ensureFileParsed(const QString &filePath);

    // 左侧
    FileListPanel *m_fileListPanel = nullptr;

    // 右侧
    SpectrumChartView *m_chartView      = nullptr;
    MetadataPanel     *m_metadataPanel  = nullptr;

    // 布局
    QSplitter *m_mainSplitter  = nullptr;
    QSplitter *m_rightSplitter = nullptr;

    // 缓存
    QMap<QString, SpectrumData> m_dataCache;
};
