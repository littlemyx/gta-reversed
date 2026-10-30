#include "StdInc.h"

bool CPointList::Empty() const noexcept {
    return m_Count == 0;
}

void CPointList::AddPoint(CVector pt) noexcept {
    if (m_Count >= m_Points.size()) {
        return;
    }
    m_Points[m_Count++] = pt;
}

CVector CPointList::GetCentroid() const noexcept {
    CVector sum{};
    for (const auto& pt : GetPoints()) {
        sum += pt;
    }
    return ExeDivide(sum, (float)m_Count); // exe 0x69B2C2..: sum * (1.0f / n), as in operator/(CVector, float) 0x4119D0
}
