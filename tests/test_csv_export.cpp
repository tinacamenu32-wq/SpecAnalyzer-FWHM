#include "CsvExport.h"
#include <QBuffer>
#include <QDebug>

int main()
{
    // Escaped expected text is independent of the compiler's source encoding.
    const QString fileName = QStringLiteral("\u6587\u4ef6\u540d");
    const QString columns = QStringLiteral(",\u6ce2\u957f(nm),\u5f3a\u5ea6,\u534a\u9ad8\u5bbd\u539f\u59cb\u503c(nm),\u51c0\u5cf0\u79ef\u5206\u9762\u79ef,\u79ef\u5206\u5de6\u8fb9\u754c(nm),\u79ef\u5206\u53f3\u8fb9\u754c(nm),\u9762\u79ef\u8ba1\u7b97\u72b6\u6001");
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
        if (!CsvExport::writeUtf8(buffer, csv)) return 2;
        if (bytes != QByteArray::fromHex("efbbbf") + csv.toUtf8()) {
            qCritical() << "CSV is not UTF-8 with BOM";
            return 1;
        }
    }
    {
        QString row;
        QTextStream stream(&row);
        stream << CsvExport::quoted(QStringLiteral("样品,\"A\".csv"));
        DetectedPeak peak; peak.wavelength=500;peak.intensity=100;
        SpectralLineResult result; result.fwhm=1.2;result.integrationLeft=499;result.integrationRight=501;
        result.areaStatus=QStringLiteral("拟合失败");
        CsvExport::writePeak(stream,peak,result);
        stream.flush();
        if (row.contains(QStringLiteral("nan")) || !row.contains(QStringLiteral(",1.2000,,499.0000,501.0000,"))) return 3;
        if (!row.startsWith(QStringLiteral("\"样品,\"\"A\"\".csv\""))) return 4;
    }
    {
        QString text; QTextStream stream(&text);
        CsvExport::writeRsd(stream, {{1,3,std::numeric_limits<double>::quiet_NaN()}, {5}});
        stream.flush();
        const auto cells=text.trimmed().split(QLatin1Char(','));
        if(cells.size()!=1+2*CsvExport::ColumnsPerPeak || cells[1+CsvExport::AreaColumn]!=QStringLiteral("70.71")) return 5;
        for(int i=1;i<cells.size();++i) if(i!=1+CsvExport::AreaColumn&&!cells[i].isEmpty()) return 6;
    }
    {
        QVector<DetectedPeak> peaks(2);peaks[0].wavelength=500;peaks[1].wavelength=500.2;
        const auto matches=CsvExport::matchPeaks({500.01,500.03},peaks);
        if(matches[0]<0 || matches[1]<0 || matches[0]==matches[1]) return 7;
    }
    {
        SpectralLineResult r;r.valid=true;r.computedValue=12;r.approximate=true;
        QVector<double> values;int count=0;
        CsvExport::collectRsd(r,false,values,count);
        if(!values.isEmpty()||count!=1) return 8;
        QString text;QTextStream stream(&text);
        CsvExport::writeRsd(stream,{values},{count},false);stream.flush();
        if(!text.contains(QStringLiteral("已排除 1 个近似面积"))) return 9;
        values.clear();count=0;
        CsvExport::collectRsd(r,true,values,count);
        if(values.size()!=1||values[0]!=12||count!=1) return 10;
        r.computedValue=16;
        CsvExport::collectRsd(r,true,values,count);
        text.clear();stream.seek(0);
        CsvExport::writeRsd(stream,{values},{count},true);stream.flush();
        if(!text.contains(QStringLiteral("含 2 个近似面积"))||!text.contains(QStringLiteral("20.20"))) return 11;
        r.truncated=true;
        CsvExport::collectRsd(r,true,values,count);
        if(values.size()!=2||count!=2) return 12;
    }
    qInfo() << "Chinese CSV headers and UTF-8 BOM verified";
    return 0;
}
