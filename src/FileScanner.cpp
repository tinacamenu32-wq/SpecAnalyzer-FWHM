#include "FileScanner.h"

#include <QDirIterator>
#include <QCollator>
#include <algorithm>

QStringList FileScanner::discoverCsvFiles(const QString &rootDir)
{
    QStringList files;
    QDirIterator it(rootDir, {"*.csv"}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files.append(it.next());
    }

    // 自然排序
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(),
              [&collator](const QString &a, const QString &b) {
                  return collator.compare(a, b) < 0;
              });

    return files;
}
