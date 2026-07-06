#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

/// 左侧面板：文件列表 + 导入 / 导出按钮
class FileListPanel : public QWidget {
    Q_OBJECT
public:
    explicit FileListPanel(QWidget *parent = nullptr);

    void addFiles(const QStringList &filePaths);
    void clearFiles();

    /// 当前选中文件的完整路径，没选中返回空串
    QString currentFilePath() const;

    /// 获取列表中所有文件路径
    QStringList allFilePaths() const;

signals:
    void fileSelected(const QString &filePath);
    void exportRequested();

private slots:
    void onImportClicked();
    void onImportFolderClicked();
    void onExportClicked();
    void onItemClicked(QListWidgetItem *item);

private:
    QListWidget  *m_fileList     = nullptr;
    QPushButton  *m_importBtn    = nullptr;
    QPushButton  *m_importDirBtn = nullptr;
    QPushButton  *m_exportBtn    = nullptr;
    QVBoxLayout  *m_layout       = nullptr;
};
