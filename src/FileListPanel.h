#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

/// 左侧面板：文件列表 + 导入按钮
class FileListPanel : public QWidget {
    Q_OBJECT
public:
    explicit FileListPanel(QWidget *parent = nullptr);

    void addFiles(const QStringList &filePaths);
    void clearFiles();

    QString currentFilePath() const;
    QStringList allFilePaths() const;

signals:
    void fileSelected(const QString &filePath);
    void exportRequested();

private slots:
    void onImportClicked();
    void onImportFolderClicked();
    void onExportClicked();
    void onRemoveClicked();
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget  *m_fileList     = nullptr;
    QPushButton  *m_importBtn    = nullptr;
    QPushButton  *m_importDirBtn = nullptr;
    QPushButton  *m_exportBtn    = nullptr;
    QPushButton  *m_removeBtn    = nullptr;
    QVBoxLayout  *m_layout       = nullptr;
};
