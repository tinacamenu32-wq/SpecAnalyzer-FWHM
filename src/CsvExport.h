#pragma once

#include <QString>
#include <QTextStream>

namespace CsvExport {

inline void writeHeader(QTextStream &stream, int peakCount)
{
    // Qt 5 QTextStream treats const char* as Latin-1. Use Unicode strings.
    stream << QStringLiteral("文件名");
    for (int i = 0; i < peakCount; ++i)
        stream << QStringLiteral(",波长(nm),强度,半高宽原始值(nm),半高宽");
    stream << '\n';
}

} // namespace CsvExport
