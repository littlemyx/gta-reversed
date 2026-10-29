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

// Code from 0x699570, translated line by line (x87, PC24 in game mode)
bool CCoverPoint::FindCoordinatesCoverPoint(CPed* ped, const CVector& targetPos, CVector& out) const noexcept {
    // 0x699577 - the ped has to be one of the covered peds
    if (!notsa::contains(GetCoveredPeds(), ped)) {
        return false;
    }
    switch (GetType()) {
    case eType::OBJECT: { // 0x6996F2
        CEntity* const entity = m_CoverEntity.Get();
        if (!entity) {
            return false;
        }
        const CColModel* const col = CModelInfo::GetModelInfo((int16)entity->m_nModelIndex)->m_pColModel; // 0x6996FD - movsx word [esi+0x22]
        const CBoundingBox&    bb  = col->m_boundBox;

        // 0x69970B - `fld max.x; fcomp max.y; test ah, 0x41; jne` => y if (x <= y or unordered) else x; then `fadd 0.4f` and a store to a float
        const float maxXY = bb.m_vecMax.x > bb.m_vecMax.y ? bb.m_vecMax.x : bb.m_vecMax.y;
        const float dist  = (float)((x87d)maxXY + 0.4f); // 0x858EE8

        // 0x699739 - (target - entity) in 2D, stored to floats, z = 0, normalised in 3D (CVector::Normalise 0x59C910)
        const CVector entityPos = entity->GetPosition();
        CVector       dir{
            (float)((x87d)targetPos.x - entityPos.x),
            (float)((x87d)targetPos.y - entityPos.y),
            0.f
        };
        dir.Normalise();

        // 0x699763 - entity pos - dir * dist. The products of x / y stay unrounded on the x87 stack, the one of z is stored to a float
        const x87d mulX = (x87d)dir.x * dist;
        const x87d mulY = (x87d)dir.y * dist;
        const float  mulZ = (float)((x87d)dir.z * dist);
        out.x             = (float)((x87d)entityPos.x - mulX);
        out.y             = (float)((x87d)entityPos.y - mulY);
        out.z             = (float)((x87d)entityPos.z - mulZ);
        return true;
    }
    case eType::VEHICLE: { // 0x6995D4
        CEntity* const entity = m_CoverEntity.Get();
        if (!entity) {
            return false;
        }
        CPointList points{}; // 0x699F00
        CColModel* const col = CModelInfo::GetModelInfo((int16)entity->m_nModelIndex)->m_pColModel; // 0x6995E8
        CFormation::FindCoverPointsBehindBox(
            &points,
            targetPos,
            entity->m_matrix, // the exe passes the raw pointer
            col->m_boundSphere.m_vecCenter,
            col->m_boundBox.m_vecMin,
            col->m_boundBox.m_vecMax,
            1000000.f // 0x49742400
        );
        if (points.m_Count != 2) { // 0x699635
            return false;
        }

        // 0x69963E - squared 2D distances of the two points to the PED (not to the target): the first as a float that is then square-rooted, the second not rounded
        const CVector pedPos = ped->GetPosition();
        const x87d  d0x    = (x87d)points.m_Points[0].x - pedPos.x;
        const x87d  d0y    = (x87d)points.m_Points[0].y - pedPos.y;
        const float   d0Sq   = (float)(d0y * d0y + d0x * d0x);
        const x87d  d1x    = (x87d)points.m_Points[1].x - pedPos.x;
        const x87d  d1y    = (x87d)points.m_Points[1].y - pedPos.y;
        const x87d  r0     = x87::sqrt(d0Sq);
        const x87d  r1     = x87::sqrt(d1y * d1y + d1x * d1x);
        out = r1 > r0 ? points.m_Points[0] : points.m_Points[1]; // 0x6996A1 - `fcompp; test ah, 0x41; jne` => the second point unless r1 > r0
        return true;
    }
    case eType::POINTONMAP: { // 0x6995AF
        out = m_PointOnMap;
        return true;
    }
    default: // 0x6997D4 - NONE (or any other value)
        return false;
    }
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
        // 0x699839: sqrt(x*x + y*y + z*z) compared with 0.01: kept when <= 0.01 (the port: squared >= 0.01^2 removed, i.e. the equal case and the NaN case differed)
        if (!GetCoverEntity() || !(GetCoverEntity()->AsVehicle()->GetMoveSpeed().Magnitude() <= 0.01f)) {
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
    NOTSA_UNREACHABLE_RET(false);
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
    NOTSA_UNREACHABLE_RET({});
}

void CCoverPoint::InjectHooks() {
    RH_ScopedClass(CCoverPoint);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(CanAccommodateAnotherPed, 0x698E70);
    RH_ScopedInstall(ReleaseCoverPointForPed, 0x698EF0);
    RH_ScopedInstall(ReserveCoverPointForPed, 0x698EB0);
}
