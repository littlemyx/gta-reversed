#include "StdInc.h"
#include "CoverPoint.h"

// notsa
CCoverPoint::CCoverPoint(eType type, eUsage usage, Dir dir, CEntity* coverEntity, const CVector* pos) :
    m_Type{ type },
    m_Usage{ usage },
    m_Dir{ dir }
{
    assert(type == eType::POINTONMAP || !!pos != !!coverEntity && "Only either `pos` or `coverEntity` must be set");

    if (pos) {
        m_PointOnMap = *pos;
    }
    if (coverEntity) {
        if (!coverEntity->GetIsTypeBuilding() || coverEntity->GetIplIndex() != 0) {
            m_CoverEntity = coverEntity;
        }
    }
}

// 0x698E70 (Typo in name fixed, it was `CanAccomodateAnotherPed`!)
bool CCoverPoint::CanAccommodateAnotherPed() const noexcept {
    switch (GetType()) {
    case CCoverPoint::eType::NONE:
        return false;
    case CCoverPoint::eType::VEHICLE:
        return notsa::IsFixBugs()
            ? rng::all_of(GetCoveredPeds(), notsa::IsNull{})
            : !m_CoveredPeds[0] || m_CoveredPeds[1];
    default:
        return !m_CoveredPeds[0];
    }
}

// 0x698EF0
void CCoverPoint::ReleaseCoverPointForPed(CPed* ped) {
    // Only the slots that hold `ped` are cleared (the exe compares each slot with the argument)
    for (auto& covered : m_CoveredPeds) {
        if (covered.Get() == ped) {
            covered = (CPed*)nullptr; // CleanUpOldReference (0x571A00) if it was set
        }
    }
}

// 0x698EB0
void CCoverPoint::ReserveCoverPointForPed(CPed* ped) {
    // The exe walks the slots in order: stops if one already holds `ped`, else takes the first free one
    for (auto& covered : m_CoveredPeds) {
        if (covered.Get() == ped) {
            return;
        }
        if (!covered.Get()) {
            covered = ped; // RegisterReference (0x571B70)
            return;
        }
    }
}

// Code from 0x699570
bool CCoverPoint::FindCoordinatesCoverPoint(CPed* ped, const CVector& targetPos, CVector& out) const noexcept {
    if (!notsa::contains(GetCoveredPeds(), ped)) {
        return false;
    }
    switch (GetType()) {
    case eType::OBJECT: { // 0x6996F2
        if (!m_CoverEntity) {
            return false;
        }
        const auto* const bb    = &m_CoverEntity->GetColModel()->GetBoundingBox();
        const auto        dir2D = (CVector2D{ targetPos } - m_CoverEntity->GetPosition2D()).Normalized();
        out                     = m_CoverEntity->GetPosition() - CVector{ dir2D } * (std::max(bb->m_vecMax.x, bb->m_vecMax.y) + 0.4f);
        return true;
    }
    case eType::VEHICLE: { // 0x6995E3
        CPointList points{};
        CFormation::FindCoverPointsBehindBox(
            &points,
            targetPos,
            &m_CoverEntity->GetMatrix(),
            m_CoverEntity->GetColModel()->GetBoundCenter(),
            m_CoverEntity->GetColModel()->GetBoundingBox().m_vecMin,
            m_CoverEntity->GetColModel()->GetBoundingBox().m_vecMax,
            1000000.f
        );
        if (points.m_Count != 2) {
            return false;
        }
        out = (points.m_Points[0] - targetPos).SquaredMagnitude() < (points.m_Points[1] - targetPos).SquaredMagnitude() // Use closest point
            ? points.m_Points[0]
            : points.m_Points[1];
        return true;
    }
    case eType::POINTONMAP: { // 0x6995BB
        out = m_PointOnMap;
        return true;
    }
    }
    NOTSA_UNREACHABLE();
}

// notsa
void CCoverPoint::Remove() {
    m_Type        = eType::NONE;
    m_CoverEntity = nullptr;
    VERIFY(CCover::m_NumPoints-- > 0);
}

// notsa, code from 0x699800 - 0x699892
void CCoverPoint::Update() {
    if (CanBeRemoved()) {
        Remove();
    }
}

// notsa, code part of `CCoverPoint::Update`
bool CCoverPoint::CanBeRemoved() const {
    switch (GetType()) {
    case CCoverPoint::eType::OBJECT: { // 0x699850
        if (!GetCoverEntity() || GetCoverEntity()->GetUp().z < 0.9f) {
            return true;
        }
        return false;
    }
    case CCoverPoint::eType::VEHICLE: { // 0x699819
        if (!GetCoverEntity() || GetCoverEntity()->AsVehicle()->GetMoveSpeed().SquaredMagnitude() >= sq(0.01f)) {
            return true;
        }
        return false;
    }
    case CCoverPoint::eType::POINTONMAP: { // 0x699815
        if (!GetCoverEntity()) {
            return true;
        }
        return false;
    }
    case CCoverPoint::eType::NONE:
        return false; /* Already removed, no need to remove again */
    }
    NOTSA_UNREACHABLE();
}

// notsa
CVector CCoverPoint::GetPos() const {
    switch (GetType()) {
    case eType::OBJECT:
    case eType::VEHICLE:
        return GetCoverEntity()->GetPosition();
    case eType::POINTONMAP:
        return GetPointOnMap();
    }
    NOTSA_UNREACHABLE();
}

void CCoverPoint::InjectHooks() {
    RH_ScopedClass(CCoverPoint);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(CanAccommodateAnotherPed, 0x698E70);
    RH_ScopedInstall(ReleaseCoverPointForPed, 0x698EF0);
    RH_ScopedInstall(ReserveCoverPointForPed, 0x698EB0);
}
