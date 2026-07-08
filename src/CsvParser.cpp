#include "CsvParser.h"

#include <QFile>
#include <QFileInfo>
#include <QStringView>
#include <QDebug>
#include <QRegularExpression>

QByteArray CsvParser::stripBom(const QByteArray &raw)
{
    if (raw.size() >= 3
        && static_cast<unsigned char>(raw[0]) == 0xEF
        && static_cast<unsigned char>(raw[1]) == 0xBB
        && static_cast<unsigned char>(raw[2]) == 0xBF)
    {
        return raw.mid(3);
    }
    return raw;
}

/// 检测并修复双重 UTF-8 编码
/// 场景：中文字符被 UTF-8 → Latin-1 误解码 → 再次 UTF-8 编码，导致乱码
static QString fixDoubleEncoding(const QString &maybeGarbled)
{
    // 尝试 latin-1 → utf-8 反向修复
    QByteArray latin1Bytes = maybeGarbled.toLatin1();
    if (latin1Bytes.isEmpty())
        return maybeGarbled;

    // 检查修复后的文本是否包含中文字符
    QString fixed = QString::fromUtf8(latin1Bytes);
    if (fixed.contains(QRegularExpression("[\\x{4e00}-\\x{9fff}]")))
        return fixed;

    return maybeGarbled;
}

bool CsvParser::parseMetadata(const QStringList &headerFields,
                               const QStringList &valueFields,
                               SpectrumMetadata &outMeta,
                               QString &outError)
{
    if (headerFields.size() < 7 || valueFields.size() < 7) {
        outError = QString("元数据字段不足 (期望7, 实际头%1/值%2)")
                       .arg(headerFields.size()).arg(valueFields.size());
        return false;
    }

    bool ok = false;
    outMeta.laserVoltage    = valueFields[0].toDouble(&ok); if (!ok) { outError = "激光器电压解析失败"; return false; }
    outMeta.laserFrequency  = valueFields[1].toDouble(&ok); if (!ok) { outError = "激光器频率解析失败"; return false; }
    outMeta.laserDivider    = valueFields[2].toInt(&ok);    if (!ok) { outError = "激光器分频解析失败"; return false; }
    outMeta.integrationTime = valueFields[3].toDouble(&ok); if (!ok) { outError = "积分时间解析失败"; return false; }
    outMeta.averagingCount  = valueFields[4].toInt(&ok);    if (!ok) { outError = "平均次数解析失败"; return false; }
    outMeta.triggerMode     = valueFields[5];
    outMeta.triggerDelay    = valueFields[6].toDouble(&ok); if (!ok) { outError = "触发延迟解析失败"; return false; }

    return true;
}

QVector<QPointF> CsvParser::parseDataRows(const QStringList &nonBlankLines,
                                           QString &outError)
{
    QVector<QPointF> points;
    // nonBlankLines[0]=元数据头, [1]=元数据值, [2]=数据头"Wavelength(nm),Stitched_Intensity", [3+]=数据
    if (nonBlankLines.size() < 4) {
        outError = "数据行不足";
        return points;
    }

    int dataStartIdx = 3; // skip: metadata header, metadata values, data header
    int dataCount = nonBlankLines.size() - dataStartIdx;
    points.reserve(dataCount);

    for (int i = dataStartIdx; i < nonBlankLines.size(); ++i) {
        const QString &line = nonBlankLines[i];
        int commaIdx = line.indexOf(',');
        if (commaIdx < 0)
            continue;

        bool ok1 = false, ok2 = false;
        double wl        = QStringView(line).left(commaIdx).toDouble(&ok1);
        double intensity = QStringView(line).mid(commaIdx + 1).toDouble(&ok2);

        if (ok1 && ok2)
            points.append(QPointF(wl, intensity));
    }

    if (points.isEmpty())
        outError = "未能解析到任何有效数据行";

    return points;
}

std::optional<SpectrumData> CsvParser::parse(const QString &filePath,
                                              QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QString("无法打开文件: %1").arg(filePath);
        return std::nullopt;
    }

    QByteArray raw = file.readAll();
    file.close();

    // 1. 去除 UTF-8 BOM 并修复双重 UTF-8 编码
    QByteArray cleanData = stripBom(raw);
    QString content = fixDoubleEncoding(QString::fromUtf8(cleanData));

    // 2. 分行并处理 \r\n
    QStringList rawLines = content.split('\n');
    QStringList lines;
    lines.reserve(rawLines.size());
    for (const QString &line : rawLines) {
        QString trimmed = line;
        if (trimmed.endsWith('\r'))
            trimmed.chop(1);
        if (!trimmed.isEmpty())
            lines.append(trimmed);
    }

    if (lines.size() < 4) {
        if (errorMessage)
            *errorMessage = "文件内容不完整（少于4行非空行）";
        return std::nullopt;
    }

    // 3. 解析元数据（前两行非空：元数据头 + 元数据值）
    QStringList metaHeaders = lines[0].split(',');
    QStringList metaValues  = lines[1].split(',');

    SpectrumData data;
    data.filePath = filePath;
    data.fileName = QFileInfo(filePath).fileName();

    QString err;
    if (!parseMetadata(metaHeaders, metaValues, data.metadata, err)) {
        if (errorMessage) *errorMessage = err;
        return std::nullopt;
    }

    // 4. 解析数据行
    data.points = parseDataRows(lines, err);
    if (data.points.isEmpty()) {
        if (errorMessage) *errorMessage = err;
        return std::nullopt;
    }

    return data;
}
