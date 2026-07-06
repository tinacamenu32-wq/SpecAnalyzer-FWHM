#include "FileListPanel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QDirIterator>

FileListPanel::FileListPanel(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 8, 8, 8);

    // 标题
    auto *titleLabel = new QLabel("文件列表", this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    m_layout->addWidget(titleLabel);

    // 文件列表
    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layout->addWidget(m_fileList);

    // 按钮行 1
    auto *btnLayout1 = new QHBoxLayout();
    m_importBtn = new QPushButton("导入文件...", this);
    m_importDirBtn = new QPushButton("导入文件夹...", this);
    btnLayout1->addWidget(m_importBtn);
    btnLayout1->addWidget(m_importDirBtn);
    m_layout->addLayout(btnLayout1);

    // 按钮行 2
    auto *btnLayout2 = new QHBoxLayout();
    m_exportBtn = new QPushButton("导出结果", this);
    btnLayout2->addWidget(m_exportBtn);
    m_layout->addLayout(btnLayout2);

    // 信号连接
    connect(m_importBtn, &QPushButton::clicked, this, &FileListPanel::onImportClicked);
    connect(m_importDirBtn, &QPushButton::clicked, this, &FileListPanel::onImportFolderClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &FileListPanel::onExportClicked);
    connect(m_fileList, &QListWidget::itemClicked, this, &FileListPanel::onItemClicked);
}

void FileListPanel::addFiles(const QStringList &filePaths)
{
    for (const QString &path : filePaths) {
        QFileInfo fi(path);
        auto *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_fileList->addItem(item);
    }
}

void FileListPanel::clearFiles()
{
    m_fileList->clear();
}

QString FileListPanel::currentFilePath() const
{
    auto *item = m_fileList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

QStringList FileListPanel::allFilePaths() const
{
    QStringList paths;
    for (int i = 0; i < m_fileList->count(); ++i) {
        QString path = m_fileList->item(i)->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            paths.append(path);
    }
    return paths;
}

void FileListPanel::onImportClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        "选择光谱 CSV 文件",
        QString(),
        "CSV 文件 (*.csv);;所有文件 (*)");

    if (!files.isEmpty())
        addFiles(files);
}

void FileListPanel::onImportFolderClicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "选择包含 CSV 文件的文件夹",
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir.isEmpty())
        return;

    QStringList csvFiles;
    QDirIterator it(dir, {"*.csv"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        csvFiles.append(it.next());
    }

    if (csvFiles.isEmpty()) {
        QMessageBox::information(this, "提示", "所选文件夹及其子文件夹中未发现 CSV 文件。");
        return;
    }

    addFiles(csvFiles);
}

void FileListPanel::onExportClicked()
{
    emit exportRequested();
}

void FileListPanel::onItemClicked(QListWidgetItem *item)
{
    if (item) {
        QString path = item->data(Qt::UserRole).toString();
        emit fileSelected(path);
    }
}
