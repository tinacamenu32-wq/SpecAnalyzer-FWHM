#include "CsvExport.h"
#include <QBuffer>
#include <QDebug>

int main()
{
    // Escaped expected text is independent of the compiler's source encoding.
    const QString fileName = QStringLiteral("\u6587\u4ef6\u540d");
    const QString columns = QStringLiteral(",\u6ce2\u957f(nm),\u5f3a\u5ea6,\u534a\u9ad8\u5bbd\u539f\u59cb\u503c(nm),\u534a\u9ad8\u5bbd");
    for (int peaks : {0, 1, 3}) {
        QString csv;
        QTextStream stream(&csv);
        CsvExport::writeHeader(stream, peaks);
        stream.flush();
        if (csv != fileName + columns.repeated(peaks) + QLatin1Char('\n')) {
            qCritical() << "Chinese CSV header mismatch:" << csv;
            return 1;
        }

        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        QTextStream output(&buffer);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        output.setEncoding(QStringConverter::Utf8);
#else
        output.setCodec("UTF-8");
#endif
        output << QChar(0xFEFF) << csv;
        output.flush();
        if (bytes != QByteArray::fromHex("efbbbf") + csv.toUtf8()) {
            qCritical() << "CSV is not UTF-8 with BOM";
            return 1;
        }
    }
    qInfo() << "Chinese CSV headers and UTF-8 BOM verified";
    return 0;
}
