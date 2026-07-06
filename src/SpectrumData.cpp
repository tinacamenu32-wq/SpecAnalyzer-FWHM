#include "SpectrumData.h"
#include <limits>

SpectrumData::SpectrumData() = default;

void SpectrumData::computeRange() const
{
    if (m_rangeCached || points.isEmpty())
        return;

    m_wlMin = std::numeric_limits<double>::max();
    m_wlMax = std::numeric_limits<double>::lowest();
    m_intMin = std::numeric_limits<double>::max();
    m_intMax = std::numeric_limits<double>::lowest();

    for (const auto &p : points) {
        if (p.x() < m_wlMin) m_wlMin = p.x();
        if (p.x() > m_wlMax) m_wlMax = p.x();
        if (p.y() < m_intMin) m_intMin = p.y();
        if (p.y() > m_intMax) m_intMax = p.y();
    }
    m_rangeCached = true;
}

double SpectrumData::wavelengthMin() const { computeRange(); return m_wlMin; }
double SpectrumData::wavelengthMax() const { computeRange(); return m_wlMax; }
double SpectrumData::intensityMin()  const { computeRange(); return m_intMin; }
double SpectrumData::intensityMax()  const { computeRange(); return m_intMax; }
