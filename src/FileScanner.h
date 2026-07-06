#pragma once

#include <QStringList>

/// 递归扫描 csv 文件的工具
class FileScanner {
public:
    /// 递归查找 rootDir 及其子目录下的所有 *.csv 文件，按名称排序
    static QStringList discoverCsvFiles(const QString &rootDir);
};
