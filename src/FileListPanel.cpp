#include "FileListPanel.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QDirIterator>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QSet>
#include <QLineEdit>
#include <QAbstractItemModel>

FileListPanel::FileListPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("fileSidebar");
    setAttribute(Qt::WA_StyledBackground, true);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(16, 20, 16, 16);
    m_layout->setSpacing(12);
    auto *titleRow = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("光谱文件"), this);
    title->setProperty("role", "section");
    auto *count = new QLabel(QStringLiteral("0"), this);
    count->setProperty("role", "badge");
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(count);
    m_layout->addLayout(titleRow);

    m_importBtn = new QPushButton(QStringLiteral("＋ 导入 CSV"), this);
    m_importBtn->setProperty("role", "primary");
    m_importBtn->setToolTip(QStringLiteral("选择一个或多个光谱 CSV 文件"));
    m_layout->addWidget(m_importBtn);
    m_importDirBtn = new QPushButton(QStringLiteral("导入文件夹…"), this);
    m_layout->addWidget(m_importDirBtn);
    auto *search = new QLineEdit(this);
    search->setObjectName("fileSearch");
    search->setPlaceholderText(QStringLiteral("搜索文件名"));
    search->setClearButtonEnabled(true);
    search->setAccessibleName(QStringLiteral("搜索光谱文件"));
    m_layout->addWidget(search);
    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileList->setTextElideMode(Qt::ElideMiddle);
    m_fileList->setAccessibleName(QStringLiteral("光谱文件列表"));
    m_layout->addWidget(m_fileList, 1);
    auto *empty = new QLabel(QStringLiteral("还没有光谱文件\n导入 CSV 或整个文件夹"), this);
    empty->setProperty("role", "secondary");
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    m_layout->addWidget(empty);
    auto updateFiles = [this, count, empty, search]() {
        count->setText(QString::number(m_fileList->count()));
        int visible = 0;
        for (int i = 0; i < m_fileList->count(); ++i) {
            auto *item = m_fileList->item(i);
            item->setHidden(!item->text().contains(search->text(), Qt::CaseInsensitive));
            if (!item->isHidden()) ++visible;
        }
        count->setText(search->text().isEmpty() ? QString::number(m_fileList->count())
                       : QStringLiteral("%1 / %2").arg(visible).arg(m_fileList->count()));
        empty->setText(m_fileList->count() == 0
            ? QStringLiteral("还没有光谱文件\n导入 CSV 或整个文件夹")
            : QStringLiteral("没有匹配的文件\n试试其他关键词"));
        empty->setVisible(visible == 0);
        m_removeBtn->setEnabled(m_fileList->count() > 0);
        m_exportBtn->setEnabled(m_fileList->count() > 0);
    };
    m_exportBtn = new QPushButton(QStringLiteral("导出峰值表"), this);
    m_exportBtn->setProperty("role", "primary");
    m_layout->addWidget(m_exportBtn);
    m_removeBtn = new QPushButton(QStringLiteral("移除文件…"), this);
    m_removeBtn->setToolTip(QStringLiteral("仅从列表移除，不会删除原始文件"));
    m_layout->addWidget(m_removeBtn);
    connect(search, &QLineEdit::textChanged, this, updateFiles);
    connect(m_fileList->model(), &QAbstractItemModel::rowsInserted, this, updateFiles);
    connect(m_fileList->model(), &QAbstractItemModel::rowsRemoved, this, updateFiles);
    connect(m_fileList->model(), &QAbstractItemModel::modelReset, this, updateFiles);
    updateFiles();

    // 信号连接
    connect(m_importBtn, &QPushButton::clicked, this, &FileListPanel::onImportClicked);
    connect(m_importDirBtn, &QPushButton::clicked, this, &FileListPanel::onImportFolderClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &FileListPanel::onExportClicked);
    connect(m_removeBtn, &QPushButton::clicked, this, &FileListPanel::onRemoveClicked);
    connect(m_fileList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item, QListWidgetItem *) { onItemClicked(item); });
}

FileListPanel::~FileListPanel()
{
    // Model reset can fire while child widgets are being destroyed. Stop callbacks
    // before the count label and action buttons are deleted.
    disconnect(m_fileList->model(), nullptr, this, nullptr);
    disconnect(m_fileList, nullptr, this, nullptr);
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

void FileListPanel::onRemoveClicked()
{
    if (m_fileList->count() == 0) return;

    QDialog dlg(this);
    dlg.setWindowTitle("选择要移除的文件");
    dlg.resize(400, 300);
    auto *dlgLayout = new QVBoxLayout(&dlg);
    auto *selectAllBtn = new QPushButton("全选", &dlg);
    auto *deselectAllBtn = new QPushButton("取消全选", &dlg);
    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(selectAllBtn); topLayout->addWidget(deselectAllBtn); topLayout->addStretch();
    dlgLayout->addLayout(topLayout);

    QListWidget *listWidget = new QListWidget(&dlg);
    QVector<QCheckBox*> checkBoxes;
    for (int i = 0; i < m_fileList->count(); ++i) {
        auto *item = new QListWidgetItem();
        auto *cb = new QCheckBox(m_fileList->item(i)->text());
        cb->setChecked(true);
        cb->setProperty("row", i);
        listWidget->addItem(item);
        listWidget->setItemWidget(item, cb);
        checkBoxes.append(cb);
    }
    dlgLayout->addWidget(listWidget);

    connect(selectAllBtn, &QPushButton::clicked, &dlg, [&]() { for (auto *cb : checkBoxes) cb->setChecked(true); });
    connect(deselectAllBtn, &QPushButton::clicked, &dlg, [&]() { for (auto *cb : checkBoxes) cb->setChecked(false); });

    auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText("移除");
    btnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    dlgLayout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    // 从后往前删，避免索引错位
    QSet<int> toRemove;
    for (auto *cb : checkBoxes)
        if (cb->isChecked()) toRemove.insert(cb->property("row").toInt());

    for (int i = m_fileList->count() - 1; i >= 0; --i)
        if (toRemove.contains(i))
            delete m_fileList->takeItem(i);
}

void FileListPanel::onItemClicked(QListWidgetItem *item)
{
    if (item) {
        QString path = item->data(Qt::UserRole).toString();
        emit fileSelected(path);
    }
}
