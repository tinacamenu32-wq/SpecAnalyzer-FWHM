#pragma once

#include "SpectrumData.h"
#include <QString>
#include <optional>

/// 静态 CSV 解析器：处理 UTF-8 BOM + 双重编码修复 + 中文元数据 + 谱线数据
class CsvParser {
public:
    /// 解析一个 CSV 文件，失败返回 std::nullopt
    static std::optional<SpectrumData> parse(const QString &filePath,
                                             QString *errorMessage = nullptr);

private:
    static QByteArray stripBom(const QByteArray &rawData);

    static bool parseMetadata(const QStringList &headerFields,
                              const QStringList &valueFields,
                              SpectrumMetadata &outMeta,
                              QString &outError);

    static QVector<QPointF> parseDataRows(const QStringList &nonBlankLines,
                                          QString &outError);
};
