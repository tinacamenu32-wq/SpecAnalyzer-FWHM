#pragma once

#include "SpectralAnalyzer.h"
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QIODevice>
#include <cmath>

namespace CsvExport {
constexpr int ColumnsPerPeak = 7;
constexpr int AreaColumn = 3; // zero-based within each peak's columns

inline QString quoted(QString text)
{
    text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + text + QLatin1Char('"');
}

inline void writeHeader(QTextStream &stream, int peakCount)
{
    stream << QStringLiteral("文件名");
    for (int i = 0; i < peakCount; ++i)
        stream << QStringLiteral(",波长(nm),强度,半高宽原始值(nm),净峰积分面积,积分左边界(nm),积分右边界(nm),面积计算状态");
    stream << '\n';
}

inline void writeMissing(QTextStream &stream, const QString &status = QStringLiteral("未匹配到峰"))
{
    for (int i=0; i<ColumnsPerPeak; ++i) stream << ',';
    stream << quoted(status);
}

inline void writePeak(QTextStream &stream, const DetectedPeak &peak, const SpectralLineResult &result)
{
    stream << ',' << QString::number(peak.wavelength, 'f', 2)
           << ',' << QString::number(peak.intensity, 'f', 2)
           << ',' << QString::number(result.fwhm, 'f', 4) << ',';
    if (result.valid && std::isfinite(result.computedValue))
        stream << QString::number(result.computedValue, 'f', 4);
    stream << ',';
    if (result.integrationRight > result.integrationLeft) stream << QString::number(result.integrationLeft, 'f', 4);
    stream << ',';
    if (result.integrationRight > result.integrationLeft) stream << QString::number(result.integrationRight, 'f', 4);
    stream << ',' << quoted(result.areaStatus);
}

inline void writeRsd(QTextStream &stream, const QVector<QVector<double>> &areas)
{
    stream << QStringLiteral("RSD(%)");
    for (const auto &values : areas) {
        QStringList columns;
        for (int k=0; k<ColumnsPerPeak; ++k) columns.append(QString());
        QVector<double> valid;
        for (double v : values) if (std::isfinite(v) && v > 0) valid.append(v);
        if (valid.size() >= 2) {
            double sum=0; for(double v:valid) sum+=v;
            const double mean=sum/valid.size();
            double squares=0; for(double v:valid) squares+=(v-mean)*(v-mean);
            columns[AreaColumn]=QString::number(std::sqrt(squares/(valid.size()-1))/mean*100, 'f', 2);
        }
        stream << ',' << columns.join(QLatin1Char(','));
    }
    stream << '\n';
}

inline bool writeUtf8(QIODevice &device, const QString &csv)
{
    const QByteArray bytes=QByteArray::fromHex("efbbbf")+csv.toUtf8();
    return device.write(bytes)==bytes.size();
}

// A detected peak can belong to at most one target column in an exported row.
inline QVector<int> matchPeaks(const QVector<double> &targets, const QVector<DetectedPeak> &peaks,
                              double tolerance=.5)
{
    struct Pair { int target, peak; double distance; };
    QVector<Pair> candidates;
    for(int t=0;t<targets.size();++t) for(int p=0;p<peaks.size();++p) {
        const double distance=std::abs(targets[t]-peaks[p].wavelength);
        if(distance<tolerance) candidates.append({t,p,distance});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const Pair &a,const Pair &b){return a.distance<b.distance;});
    QVector<int> result(targets.size(),-1);
    QVector<bool> used(peaks.size(),false);
    for(const auto &p:candidates) if(result[p.target]<0&&!used[p.peak]) {result[p.target]=p.peak;used[p.peak]=true;}
    return result;
}
} // namespace CsvExport
