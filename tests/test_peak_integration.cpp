#include "SpectralAnalyzer.h"
#include <QDebug>
#include <cmath>

static int failures = 0;
static void require(bool condition, const char *message)
{
    if (!condition) { qCritical() << message; ++failures; }
}
static QVector<QPointF> gaussianData(const QVector<double> &centers, const QVector<double> &amps,
                                     const QVector<double> &widths, double low=-10, double high=10, int n=2001)
{
    QVector<QPointF> data;
    for(int i=0;i<n;++i) {
        double x=low+(high-low)*i/(n-1), y=30+1.7*x;
        for(int k=0;k<centers.size();++k) y+=amps[k]*std::exp(-.5*std::pow((x-centers[k])/widths[k],2));
        data.append(QPointF(x,y));
    }
    return data;
}
static QVector<DetectedPeak> seeds(const QVector<QPointF> &data, const QVector<double> &centers)
{
    QVector<DetectedPeak> out;
    for(double center:centers) {
        int index=0;
        for(int i=1;i<data.size();++i) if(std::abs(data[i].x()-center)<std::abs(data[index].x()-center)) index=i;
        DetectedPeak p; p.index=index;p.wavelength=data[index].x();p.intensity=data[index].y();p.prominence=0;
        out.append(p);
    }
    return out;
}
int main()
{
    const double root2pi=std::sqrt(2*std::acos(-1.0));
    for(auto centers : {QVector<double>{0}, QVector<double>{-4,4}, QVector<double>{-1.2,1.2}, QVector<double>{-2,0,2}}) {
        QVector<double> amps(centers.size(),100), widths(centers.size(),1);
        auto data=gaussianData(centers,amps,widths);
        auto peaks=seeds(data,centers);
        auto results=SpectralAnalyzer::analyzePeaks(data,peaks);
        for(int k=0;k<results.size();++k) {
            const auto &r=results[k];
            qInfo() << centers << k << r.valid << r.computedValue << r.areaStatus << r.integrationLeft << r.integrationRight;
            require(r.valid,"Expected valid integral");
            require(std::abs(r.peakArea-100*root2pi)/(100*root2pi)<.04,"Known Gaussian net area deviates >4%");
            require(r.computedValue==r.peakArea,"Area aliases disagree");
            require(!r.truncated,"Complete Gaussian should not be marked truncated");
            if(centers.size()>1 && centers.last()-centers.first()<5) require(r.fitted,"Overlapping peaks must be fitted jointly");
        }
        // Caller ordering must not affect component identity or area.
        std::reverse(peaks.begin(),peaks.end());
        const auto reversed=SpectralAnalyzer::analyzePeaks(data,peaks);
        for(int k=0;k<results.size();++k) require(std::abs(results[k].computedValue-reversed[results.size()-1-k].computedValue)<1e-7,"Order-dependent component areas");
    }
    {
        auto data=gaussianData({-.4,.4},{10000,8000},{.3,.3},-5,5,2001);
        const auto detected=SpectralAnalyzer::detectPeaks(data);
        require(detected.size()==2,"End-to-end detector did not resolve two maxima");
        const auto results=SpectralAnalyzer::analyzePeaks(data,detected);
        for(int k=0;k<results.size();++k) {
            require(results[k].valid && results[k].fitted,"End-to-end overlapping peak fit failed");
            require(std::abs(results[k].computedValue-(k==0?10000:8000)*.3*root2pi)/((k==0?10000:8000)*.3*root2pi)<.04,"End-to-end overlap integral incorrect");
            const auto single=SpectralAnalyzer::analyzePeak(data,detected[k]);
            require(std::abs(single.computedValue-results[k].computedValue)<1e-6,"Single-peak API disagrees with batch API");
        }
    }
    {
        auto data=gaussianData({-1.3,1.3},{100,70},{.7,1.3});
        const auto result=SpectralAnalyzer::analyzePeaks(data,seeds(data,{-1.3,1.3}));
        for(int k=0;k<2;++k) {
            const double expected=(k==0?100*.7:70*1.3)*root2pi;
            require(result[k].valid && std::abs(result[k].computedValue-expected)/expected<.05,"Unequal-width component area incorrect");
        }
    }
    {
        auto data=gaussianData({0},{100},{1});
        for(auto &point:data) {point.setY(point.y()-1.7*point.x());point.setX(point.x()+500);}
        const auto r=SpectralAnalyzer::analyzePeaks(data,seeds(data,{500}))[0];
        require(std::abs(r.fwhm-2.35482)<.002,"Flat-baseline Gaussian FWHM regression");
    }
    // Asymmetric shoulder, known individual components.
    {
        auto data=gaussianData({-.8,1},{100,40},{1,1});
        const auto r=SpectralAnalyzer::analyzePeaks(data,seeds(data,{-.8,1}));
        for(int k=0;k<2;++k) {
            qInfo() << "shoulder" << k << r[k].valid << r[k].computedValue << r[k].areaStatus;
            require(r[k].valid && r[k].fitted,"Shoulder fit failed");
            require(std::abs(r[k].computedValue-(k==0?100:40)*root2pi)/((k==0?100:40)*root2pi)<.06,"Shoulder area incorrect");
        }
    }
    // Exact clipping at interpolated zero crossings, not max-at-nodes trapezoids.
    {
        QVector<QPointF> data{{0,0},{1,-2},{2,2},{3,5},{4,2},{5,-2},{6,0}};
        auto result=SpectralAnalyzer::analyzePeaks(data,seeds(data,{3}))[0];
        require(result.valid && std::abs(result.computedValue-8)<1e-10,"Positive-part segment integral incorrect");
    }
    {
        QVector<QPointF> data{{0,0},{.5,-2},{2,2},{3,5},{4.5,2},{7,-2},{8,0}};
        auto result=SpectralAnalyzer::analyzePeaks(data,seeds(data,{3}))[0];
        require(result.valid && std::abs(result.computedValue-10.75)<1e-10,"Nonuniform wavelength spacing not integrated correctly");
    }
    {
        auto data=gaussianData({-.02,.02},{100,100},{1,1});
        const auto result=SpectralAnalyzer::analyzePeaks(data,seeds(data,{-.02,.02}));
        require(result[0].valid && result[1].valid && result[0].approximate && result[1].approximate && !result[0].fitted,"Indistinguishable fitted components must use marked approximations");
    }
    {
        auto data=gaussianData({0},{100},{1},-.5,5);
        auto result=SpectralAnalyzer::analyzePeaks(data,seeds(data,{0}))[0];
        require(result.truncated,"Truncated peak not flagged");
    }
    {
        auto data=gaussianData({0},{100},{1});auto p=seeds(data,{0,0});
        const auto result=SpectralAnalyzer::analyzePeaks(data,p);
        require(!result[0].valid && !result[1].valid && std::isnan(result[0].computedValue),"Duplicate components must not have fabricated areas");
        auto bad=data;bad[1].setX(bad[0].x());
        require(!SpectralAnalyzer::analyzePeaks(bad,{p[0]})[0].valid,"Duplicate wavelengths accepted");
        bad=data;bad[1].setY(std::numeric_limits<double>::quiet_NaN());
        require(!SpectralAnalyzer::analyzePeaks(bad,{p[0]})[0].valid,"NaN input accepted");
        QVector<QPointF> few{{0,0},{1,4},{2,3},{3,4},{4,0}};
        const auto sparse=SpectralAnalyzer::analyzePeaks(few,seeds(few,{1,3}));
        require(sparse[0].valid && sparse[1].valid && sparse[0].approximate && sparse[1].approximate,"Coarse but integrable peaks need marked partition fallback");
    }
    // Fit is deliberately underdetermined with <5 samples per component.
    // Fallback must conserve observed positive area without claiming a fit.
    {
        QVector<QPointF> data;
        const QVector<double> y{0,1,5,4,3,4,5,1,0};
        for(int i=0;i<y.size();++i) data.append(QPointF(i,y[i]));
        auto peaks=seeds(data,{2,6});
        const auto r=SpectralAnalyzer::analyzePeaks(data,peaks);
        for(const auto &area:r) {
            require(area.valid && area.approximate && !area.fitted,"Valley fallback missing approximate flag");
            require(area.areaStatus.contains(QStringLiteral("近似：谷底切分")),"Valley method not disclosed");
            require(area.errorMsg.isEmpty(),"Usable approximation still has an error");
            require(std::abs(area.computedValue-11.5)<1e-12,"Valley partition area wrong");
        }
        require(r[0].integrationRight==4 && r[1].integrationLeft==4,"Shared valley boundary wrong");
        std::reverse(peaks.begin(),peaks.end());
        const auto reverse=SpectralAnalyzer::analyzePeaks(data,peaks);
        require(reverse[1].integrationLeft==r[0].integrationLeft,"Fallback depends on caller order");
    }
    // A sloped baseline must be shared, not re-fitted through each cut. The
    // midpoint x=2.5 is between data samples, so skipping partial cells fails.
    {
        QVector<QPointF> data;
        const QVector<double> y{0,8,7,6,5,1,0};
        for(int i=0;i<y.size();++i) data.append(QPointF(i,10+2*i+y[i]));
        const auto r=SpectralAnalyzer::analyzePeaks(data,seeds(data,{1,4}));
        for(const auto &area:r) require(area.valid && area.approximate && area.areaStatus.contains(QStringLiteral("中点切分")),"Midpoint fallback not marked");
        require(r[0].integrationRight==2.5 && r[1].integrationLeft==2.5,"Fractional cut location incorrect");
        require(std::abs(r[0].computedValue-14.875)<1e-12 && std::abs(r[1].computedValue-12.125)<1e-12,"Fractional midpoint/baseline integral incorrect");
        require(std::abs(r[0].computedValue+r[1].computedValue-27)<1e-12,"Midpoint partitions do not conserve group net area");
    }
    {
        QVector<QPointF> data;
        const QVector<double> y{0,1,6,4,3,4,6,5.8,5.5,5.2,5,1,0};
        for(int i=0;i<y.size();++i) data.append(QPointF(i,y[i]));
        const auto r=SpectralAnalyzer::analyzePeaks(data,seeds(data,{2,6,10}));
        double total=0;
        for(const auto &area:r) {require(area.valid&&area.approximate,"Mixed-cut group not approximated");total+=area.computedValue;}
        require(std::abs(total-46.5)<1e-10,"Mixed partitions lose or repeat area");
        require(r[1].areaStatus.contains(QStringLiteral("谷底切分/中点切分")),"Mixed boundary methods not disclosed");
    }
    return failures ? 1 : 0;
}
