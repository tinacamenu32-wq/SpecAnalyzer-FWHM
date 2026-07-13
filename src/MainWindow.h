#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QMap>

#include "FileListPanel.h"
#include "SpectrumChartView.h"
#include "MetadataPanel.h"
#include "SpectrumData.h"
#include "SpectralAnalyzer.h"

/// 应用主窗口
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void loadInitialFiles(const QStringList &filePaths);

private slots:
    void onFileSelected(const QString &filePath);
    void onExportResults();
    void onBLParamsChanged();
    void onSGParamsChanged();
    void onFilterChanged();
    void onPeakBasedExport();

private:
    void setupUi();
    void ensureFileParsed(const QString &filePath);
    QVector<QPointF> processPipeline(const QVector<QPointF> &rawPoints);
    void reprocessAll();
    void refreshCurrentFile();

    FileListPanel *m_fileListPanel = nullptr;
    SpectrumChartView *m_chartView      = nullptr;
    MetadataPanel     *m_metadataPanel  = nullptr;

    QSplitter *m_mainSplitter  = nullptr;
    QSplitter *m_rightSplitter = nullptr;

    QMap<QString, SpectrumData> m_dataCache;
    QMap<QString, QVector<QPointF>> m_processedCache;

    SpectralAnalyzer::SGParams m_sgParams;
    SpectralAnalyzer::BLParams m_blParams;
    QString m_currentFilePath;
};
