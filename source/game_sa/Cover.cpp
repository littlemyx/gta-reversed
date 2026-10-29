#include "StdInc.h"

#include "Cover.h"
#include "Fx/FxFtol.h"

void CCover::InjectHooks() {
    RH_ScopedClass(CCover);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x698710);
    RH_ScopedInstall(RemoveCoverPointIfEntityLost, 0x698DB0);
    RH_ScopedInstall(RemoveCoverPointsForThisEntity, 0x698740);
    RH_ScopedInstall(ShouldThisBuildingHaveItsCoverPointsCreated, 0x699230);
    RH_ScopedInstall(Update, 0x6997E0);
    RH_ScopedInstall(AddCoverPoint, 0x698F30);
    RH_ScopedInstall(CalculateHorizontalSize, 0x6987F0);
    RH_ScopedInstall(DoLineCheckWithinObject, 0x698990);
    RH_ScopedInstall(DoesCoverPointStillProvideCover, 0x698DD0);
    RH_ScopedInstall(Find2HighestPoints, 0x6988E0);
    RH_ScopedInstall(FindAndReserveCoverPoint, 0x6992B0);
    RH_ScopedInstall(FindCoordinatesCoverPoint, 0x699570);
    RH_ScopedInstall(FindCoverPointsForThisBuilding, 0x699120);
    RH_ScopedInstall(FindDirFromVector, 0x698D40, {.Locked = true});
    RH_ScopedInstall(FindVectorFromDir, 0x698D60, {.Locked = true});
    RH_ScopedInstall(FindVectorFromFirstToMissingVertex, 0x698790);
}

// 0x698710
void CCover::Init() {
    ZoneScoped;

    m_NumPoints = 0;
    m_ListOfProcessedBuildings.Flush();
    // 0x698710: only the type byte of each point is cleared (the entity / ped pointers and their references stay as they are)
    for (auto& cpt : m_Points) {
        cpt.ClearType();
    }
}

// 0x698DB0
void CCover::RemoveCoverPointIfEntityLost(CCoverPoint& cpt) {
    if (cpt.GetType() > CCoverPoint::eType::NONE && cpt.GetType() < CCoverPoint::eType::NUM) {
        if (!cpt.GetCoverEntity()) {
            cpt.Remove();
        }
    }
}

// 0x698740
void CCover::RemoveCoverPointsForThisEntity(CEntity* entity) {
    for (auto& cpt : m_Points) {
        if (cpt.GetType() == CCoverPoint::eType::NONE) {
            continue;
        }
        if (cpt.GetCoverEntity() != entity) {
            continue;
        }
        cpt.Remove();
    }
}

// 0x699230
bool CCover::ShouldThisBuildingHaveItsCoverPointsCreated(CBuilding* building) {
    CVector pos;
    building->GetBoundCentre(pos);
    // 0x699230: sqrt((dz^2 + dy^2) + dx^2) < r + 30 (strict; the port's x,y,z squared sum with <= differed in rounding / the equal case / NaN)
    return IsPointInSphereExeSqrt(
        pos,
        FindPlayerCoors(),
        building->GetColModel()->GetBoundRadius() + 30.0f
    );
}

// 0x6997E0
void CCover::Update() {
    ZoneScoped;

    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    rng::for_each(m_Points, &CCoverPoint::Update);

    /* Add new cover points for vehicles/objects */
    switch (CTimer::GetFrameCounter() % 32) {
    case 26: { // 0x6998B5
        for (auto& veh : NOTSA_EXE_OR(GetVehiclePool()->GetAllValidReverse(), GetVehiclePool()->GetAllValid())) {
            if (!veh.IsAutomobile()) {
                continue;
            }
            // 0x699909: speed (sqrt of x*x+y*y+z*z) < 0.005 (0x858B4C; the port had `squared > 0.05^2`, a 10x higher limit)
            if (!(veh.GetMoveSpeed().Magnitude() < 0.005f)) {
                continue;
            }
            if (!veh.vehicleFlags.bDoesProvideCover) {
                continue;
            }
            if (!((FindPlayerCoors() - veh.GetPosition()).Magnitude2D() < 25.f)) { // 0x69994D: sqrt(dy*dy + dx*dx) < 25 (NaN skips)
                continue;
            }
            AddCoverPoint(CCoverPoint::eType::VEHICLE, &veh, nullptr, CCoverPoint::eUsage::LOWCOVER, CCoverPoint::Dir{ (uint8)0xFF }); // push 0xFF
        }

        return;
    }
    case 28: { // 0x6999B5
        for (auto& obj : NOTSA_EXE_OR(GetObjectPool()->GetAllValidReverse(), GetObjectPool()->GetAllValid())) {
            if (obj.GetUp().z <= 0.95f) {
                continue;
            }
            if (!obj.CanBeUsedToTakeCoverBehind()) {
                continue;
            }
            if (!((FindPlayerCoors() - obj.GetPosition()).Magnitude2D() < 25.f)) { // 0x699A1E: sqrt(dy*dy + dx*dx) < 25 (NaN skips)
                continue;
            }
            AddCoverPoint(CCoverPoint::eType::OBJECT, &obj, nullptr, CCoverPoint::eUsage::LOWCOVER, CCoverPoint::Dir{ (uint8)0xFF }); // push 0xFF
        }

        return;
    }
    }

    /* Process buildings too */
    if (CTimer::GetFrameCounter() % 8 == 5) { // 0x699A71
        // BUG:
        // This implementation may've accessed objects after they've been destroyed (use-after-free
        // I've modified it so that it uses references that get null'd automatically
        for (CPtrNodeDoubleLink<CBuilding*> *it = m_ListOfProcessedBuildings.GetNode(), *next{}; it; it = next) {
            next            = it->Next;
            auto* const obj = it->Item;

#ifdef NOTSA_STANDALONE_RUN
            // The exe reads buildings that were streamed out meanwhile (pool slot memory stays intact there); stale entries are dropped instead
            if (obj && !GetBuildingPool()->IsObjectValid(obj)) {
                RemoveCoverPointsForThisEntity(obj);
                m_ListOfProcessedBuildings.DeleteNode(it);
                continue;
            }
#endif
            if (!notsa::IsFixBugs() || obj) { // If fixbugs the reference may've got cleared
                if (ShouldThisBuildingHaveItsCoverPointsCreated(obj)) {
                    continue;
                }
                RemoveCoverPointsForThisEntity(obj);
                if (notsa::IsFixBugs()) { // FIXBUGS: Use-after-free
                    CEntity::SafeCleanUpRef(reinterpret_cast<CEntity*&>(it->Item));
                }
            }
            m_ListOfProcessedBuildings.DeleteNode(it);
        }

        if (!FindPlayerVehicle()) { // 0x699AE1
            CWorld::AdvanceCurrentScanCode();
            // 0x699AF1: the exe clamps the sector bounds to [0, 119] (min >= 0, max <= 119); the port's unclamped GetSectorX / GetSectorY gave the
            // sectors 120 / -1 beyond the map edge, which `GetSector` aliases to the neighbouring row's sectors
            const CRect rect{ FindPlayerCoors(), 30.f };
            const int32 minX = std::max(0, CWorld::GetSectorX(rect.left)), minY = std::max(0, CWorld::GetSectorY(rect.bottom));
            const int32 maxX = std::min(MAX_SECTORS_X - 1, CWorld::GetSectorX(rect.right)), maxY = std::min(MAX_SECTORS_Y - 1, CWorld::GetSectorY(rect.top));
            if (minX <= maxX && minY <= maxY) { CWorld::IterateSectors(minX, minY, maxX, maxY, [&](int32 x, int32 y) {
                for (auto* const obj : CWorld::GetSector(x, y).Buildings) {
                    if (obj->IsScanCodeCurrent()) {
                        continue;
                    }
                    obj->SetCurrentScanCode();
                    if (!ShouldThisBuildingHaveItsCoverPointsCreated(obj)) {
                        continue;
                    }
                    if (m_ListOfProcessedBuildings.IsMemberOfList(obj)) {
                        continue;
                    }
                    FindCoverPointsForThisBuilding(obj);
                    auto* const link = m_ListOfProcessedBuildings.AddItem(obj);
                    if (notsa::IsFixBugs()) { // FIXBUGS: Use-after-free
                        CEntity::SafeRegisterRef(reinterpret_cast<CEntity*&>(link->Item));
                    }
                }
                return true;
            }); }
        }
    }
}

// notsa - allocate new point, increments `NumPoints`
CCoverPoint* CCover::GetFree() {
    for (auto& cpt : m_Points) {
        if (cpt.GetType() == CCoverPoint::eType::NONE) {
            m_NumPoints++;
            return &cpt;
        }
    }
    return nullptr;
}

// 0x698F30
// NOTE: Original function didn't return the (possibly) created point, we do
CCoverPoint* CCover::AddCoverPoint(CCoverPoint::eType type, CEntity* coverEntity, const CVector* pos, CCoverPoint::eUsage usage, CCoverPoint::Dir dir) {
    // `POINTONMAP` is tied to a building, thus both variables are supposed to be set
    assert(type == CCoverPoint::eType::POINTONMAP || !!pos != !!coverEntity && "Only either `position` or `coverEntity` must be set"); 

    if (m_NumPoints >= m_Points.size()) {
        return nullptr;
    }

    // Check if there's already a cover point nearby
    // If so, we might not need to allocate a new one
    for (auto& cpt : m_Points) {
        // 0x698F5D
        if (cpt.GetType() == type) {
            switch (cpt.GetType()) {
            case CCoverPoint::eType::OBJECT:
            case CCoverPoint::eType::VEHICLE: {
                if (coverEntity == cpt.GetCoverEntity()) {
                    return &cpt;
                }
                break;
            }
            case CCoverPoint::eType::POINTONMAP: {
                if (*pos == cpt.GetPointOnMap()) {
                    return &cpt;
                }
                break;
            }
            }
        }

        // 0x698F9F - BUG: `GetPointOnMap()` only works if the type is `POINTONMAP`
        if (notsa::IsFixBugs() ? cpt.GetType() != CCoverPoint::eType::POINTONMAP : cpt.GetType() == CCoverPoint::eType::NONE) {
            continue;
        }

        const CVector atPos = coverEntity
            ? coverEntity->GetPosition()
            : *pos;
        // 0x698FDD: dx, dy, dz = cpt - at (x87, the sum is dz*dz + dy*dy + dx*dx) and the exe compares the SQUARE ROOT (fsqrt) with 0.8 / 2.0
        const x87d dx = (x87d)cpt.GetPointOnMap().x - (x87d)atPos.x;
        const x87d dy = (x87d)cpt.GetPointOnMap().y - (x87d)atPos.y;
        const x87d dz = (x87d)cpt.GetPointOnMap().z - (x87d)atPos.z;
        const x87d distToCpt = x87::sqrt(dz * dz + dy * dy + dx * dx);

        // 0x69901E
        if (distToCpt < (double)0.8f) {
            return &cpt;
        }

        // 0x69903C
        if (distToCpt < (double)2.f) {
            // 0x699051: byte arithmetic (`sub al, [dir]; cmp al, 0x20; jb; cmp al, 0xDF; ja`): within 32 / 256 of a full turn on either side
            const uint8 d = (uint8)(cpt.GetDir().Raw() - dir.Raw());
            if (d < 0x20 || d > 0xDF) {
                return &cpt;
            }
        }
    }

    // 0x699071
    return new (GetFree()) CCoverPoint{ // May return null
        type,
        usage,
        dir,
        coverEntity,
        pos
    };
}

// 0x6987F0
float CCover::CalculateHorizontalSize(CColTriangle* triangle, CVector* vertPositions) {
    NOTSA_UNREACHABLE_RET(0.f, "Unused"); //return plugin::CallAndReturn<float, 0x6987F0, CColTriangle*, CVector*>(triangle, vertPositions);
}

// 0x698990
bool CCover::DoLineCheckWithinObject(CColTriangle* triangle, int32 a2, CVector* a3, CVector* a4, CVector a5, CVector a6) {
    NOTSA_UNREACHABLE_RET(false, "Unused"); // return plugin::CallAndReturn<bool, 0x698990, CColTriangle*, int32, CVector*, CVector*, CVector, CVector>(triangle, a2, a3, a4, a5, a6);
}

// 0x698DD0
bool CCover::DoesCoverPointStillProvideCover(CCoverPoint* cpt, CVector pos) {
    if (!cpt) {
        return false;
    }
    RemoveCoverPointIfEntityLost(*cpt);
    switch (cpt->GetType()) {
    case CCoverPoint::eType::NONE:
        return false;
    case CCoverPoint::eType::POINTONMAP: {
        if (FindVectorFromDir(cpt->GetDir()).Dot(pos - cpt->GetPointOnMap()) > 0.f) {
            return true;
        }
        return false;
    }
    }
    return true;
}

// 0x6988E0
void CCover::Find2HighestPoints(CColTriangle* triangle, CVector* vertPositions, int32& outPoint1, int32& outPoint2) {
    // 0x6988E0: all three branches write both outputs (the stack arguments are read after pops that move esp: the second store goes to outPoint2)
    const auto a = triangle->m_vertIndices[0], b = triangle->m_vertIndices[1], c = triangle->m_vertIndices[2];
    const float za = vertPositions[a].z, zb = vertPositions[b].z, zc = vertPositions[c].z;
    if (za < zb && za < zc) { // A is lowest => B, C highest
        outPoint1 = b;
        outPoint2 = c;
        return;
    }
    if (zb < za && zb < zc) { // B is lowest => A, C highest
        outPoint1 = a;
        outPoint2 = c;
        return;
    }
    outPoint1 = a; // C lowest (or ties) => A, B highest
    outPoint2 = b;
}

// 0x6992B0
// Translated line by line from the exe (x87, PC24 in game mode). Per point: drop it if its entity is gone (the exe inlines 0x698DB0), skip points that cannot take another
// ped, load the position of the point into locals (Note: for a type outside 1..3 the exe keeps the position of the PREVIOUS point in its stack slots, so `cptPos` lives
// outside of the loop), POINTONMAP points must face the target, the ped must be within 4 units vertically and 15 units horizontally, then the point with the smallest
// ped distance (a float; strictly smaller wins) is remembered. For an attacker the point may be at most 4 units farther from the target than the ped is.
CCoverPoint* CCover::FindAndReserveCoverPoint(CPed* ped, const CVector& targetPos, bool isForAttack) {
    constexpr float kDirStep = 0.02454369328916073f; // 0x859BBC - 2pi/256 (as float)

    float   bestDist = 99999.9f; // 0x85999C - the best distance is kept on the x87 stack across the loop
    int32   bestIdx  = -1;
    CVector cptPos{};            // [esp+0x20]

    for (int32 i = 0; i < (int32)m_Points.size(); i++) {
        CCoverPoint& cpt = m_Points[i];

        // 0x6992D8 - CCover::RemoveCoverPointIfEntityLost, inlined
        RemoveCoverPointIfEntityLost(cpt);
        if (cpt.GetType() == CCoverPoint::eType::NONE) {
            continue;
        }

        // 0x6992FA - Can accommodate another ped? (VEHICLE: `!ped[0] || ped[1]`, everything else `!ped[0]`)
        const auto coveredPeds = cpt.GetCoveredPeds();
        if (cpt.GetType() == CCoverPoint::eType::VEHICLE) {
            if (coveredPeds[0] && !coveredPeds[1]) {
                continue;
            }
        } else if (coveredPeds[0]) {
            continue;
        }

        // 0x699316 - Position of the point
        switch (cpt.GetType()) {
        case CCoverPoint::eType::OBJECT:
        case CCoverPoint::eType::VEHICLE:
            cptPos = cpt.GetCoverEntity()->GetPosition(); // 0x699342
            break;
        case CCoverPoint::eType::POINTONMAP: { // 0x69932B
            cptPos = cpt.GetPointOnMap();

            // 0x69936B - The point has to face the target: dot((target - pos), (-sin(dir), cos(dir), 0)) > 0
            // `sin` stays unrounded on the x87 stack (and is negated), `cos` is stored to a float, `dz` is multiplied by 0.0
            const x87d dx    = (x87d)targetPos.x - cpt.GetPointOnMap().x;
            const x87d dy    = (x87d)targetPos.y - cpt.GetPointOnMap().y;
            const x87d dz    = (x87d)targetPos.z - cpt.GetPointOnMap().z;
            const x87d angle = (x87d)cpt.GetDir().Raw() * kDirStep;
            const x87d nsinX = X87R(-x87::sin(angle) * dx);
            const float  cosF  = (float)x87::cos(angle);
            const x87d dyCos = dy * cosF;
            const x87d dot   = (dyCos + nsinX) + dz * (x87d)0.0;
            if (!(dot > 0.0)) {
                continue;
            }
            break;
        }
        default: // outside of 1..3: `cptPos` keeps the value of the previous point
            break;
        }

        // 0x6993C5 - The ped has to be less than 4 units above / below the point
        const CVector pedPos = ped->GetPosition();
        const x87d  pedDz  = (x87d)pedPos.z - cptPos.z;
        const x87d  absDz  = pedDz < 0.0 ? -pedDz : pedDz;
        if (!(absDz < 4.0)) {
            continue;
        }

        // 0x69941A - and less than 15 units away (2D). The distance is also stored as a float (it is the value that is compared below)
        const x87d pedDx    = (x87d)pedPos.x - cptPos.x;
        const x87d pedDy    = (x87d)pedPos.y - cptPos.y;
        const x87d cptDist  = x87::sqrt(pedDy * pedDy + pedDx * pedDx);
        const float  cptDistF = (float)cptDist;
        if (!(cptDist < 15.0)) {
            continue;
        }

        // 0x699456 - Distance (float) between the ped and the target, and (not rounded) between the point and the target
        const x87d pedTgtDx  = (x87d)pedPos.x - targetPos.x;
        const x87d pedTgtDy  = (x87d)pedPos.y - targetPos.y;
        const float  pedToTgt  = (float)x87::sqrt(pedTgtDy * pedTgtDy + pedTgtDx * pedTgtDx);
        const x87d cptTgtDx  = (x87d)cptPos.x - targetPos.x;
        const x87d cptTgtDy  = (x87d)cptPos.y - targetPos.y;
        const x87d cptToTgt  = x87::sqrt(cptTgtDy * cptTgtDy + cptTgtDx * cptTgtDx);

        // 0x6994A1
        if (!(cptDistF < bestDist)) {
            continue;
        }
        if (isForAttack && !(cptToTgt < (x87d)pedToTgt + 4.0f)) { // 0x6994CF
            continue;
        }
        bestDist = cptDistF;
        bestIdx  = i;
    }

    // 0x69950E
    if (bestIdx < 0) {
        return nullptr;
    }
    CCoverPoint* const best = &m_Points[bestIdx];
    best->ReserveCoverPointForPed(ped); // 0x699519 - the first slot that already holds `ped` stops the search, else the first free slot gets it (RegisterReference)
    return best;
}

// 0x699570
bool CCover::FindCoordinatesCoverPoint(const CCoverPoint& cpt, CPed* ped, const CVector& position, CVector& outCoordinates) {
    return cpt.FindCoordinatesCoverPoint(ped, position, outCoordinates);
}

// 0x699120
void CCover::FindCoverPointsForThisBuilding(CBuilding* building) {
    auto* mi = CModelInfo::GetModelInfo(building->GetModelIndex());
    for (int32 i = 0; i < mi->m_n2dfxCount; ++i) {
        if (auto* const fx = notsa::dyn_cast<C2dEffectCoverPoint>(mi->Get2dEffect(i))) {
            const auto dir = building->GetMatrix().TransformVector(CVector{fx->m_DirOfCover, 0.f});
            const auto pos = building->GetMatrix().TransformPoint(fx->m_Pos);
            CCover::AddCoverPoint(
                CCoverPoint::eType::POINTONMAP,
                building,
                &pos,
                fx->m_Usage,
                x87::atan2(-dir.x, dir.y)
            );
        }
    }
}

// 0x698D40 - unused
CCoverPoint::Dir CCover::FindDirFromVector(CVector dir) {
    // 0x698D40: atan2(-x, y) * 256 / 2pi on the x87 stack, truncated by _ftol2 (the product is NOT rounded to float first).
    static const uint32 K85A778 = 0x4222F983u;
    double res; // the lifted asm stores 8 bytes (fstp qword)
    const float a_x = dir.x;
    const float a_y = dir.y;
    const float a_z = dir.z;
#ifdef NOTSA_PORTABLE
#include "x87_lifted/Cover_FindDirFromVector.inc"
#else
    __asm {
        push dword ptr [a_z]
        push dword ptr [a_y]
        push dword ptr [a_x]
        call L_BODY
        add esp, 12
        fstp qword ptr [res]
        jmp L_END
    L_BODY:
        fld dword ptr [esp + 4]
        fchs
        fld dword ptr [esp + 8]
        fpatan
        fmul dword ptr [K85A778]
        ret
    L_END:
    }
#endif
    return CCoverPoint::Dir{ static_cast<uint8>(notsa::detail::Ftol(res)) }; // _ftol2 of the unrounded product, low byte
}

// 0x698D60
CVector CCover::FindVectorFromDir(CCoverPoint::Dir direction) {
    // 0x698D60: (-sin(a), cos(a), 0) with a = raw * 0x3CC90FDB (2pi / 256, a float product on the x87 stack), x87 code verbatim.
#ifdef NOTSA_PORTABLE
    float out[3]{}; // the name the lifted x87 code (x87_lifted/*.inc) refers to
#else
    float outV[3]{}; // not `out`: a reserved word inside MSVC inline assembly (C4405)
#endif
    const uint32 raw = std::bit_cast<uint8>(direction);
    static const uint32 K859BBC = 0x3CC90FDBu;

    const uint32 a_raw = raw;
#ifdef NOTSA_PORTABLE
#include "x87_lifted/Cover_FindVectorFromDir.inc"
#else
    __asm {
        push dword ptr [a_raw]
        lea eax, [outV]
        push eax
        call L_BODY
        add esp, 8
        jmp L_END
    L_BODY:
        sub esp, 0xc
        movzx eax, byte ptr [esp + 0x14]
        mov dword ptr [esp + 0x14], eax
        mov eax, dword ptr [esp + 0x10]
        mov ecx, eax
        fild dword ptr [esp + 0x14]
        mov dword ptr [esp + 8], 0
        fmul dword ptr [K859BBC]
        fld st(0)
        fsin
        fchs
        fstp dword ptr [esp]
        mov edx, dword ptr [esp]
        mov dword ptr [ecx], edx
        fcos
        fstp dword ptr [esp + 4]
        mov edx, dword ptr [esp + 4]
        mov dword ptr [ecx + 4], edx
        mov edx, dword ptr [esp + 8]
        mov dword ptr [ecx + 8], edx
        add esp, 0xc
        ret
    L_END:
    }
#endif
#ifdef NOTSA_PORTABLE
    return CVector{ out[0], out[1], out[2] };
#else
    return CVector{ outV[0], outV[1], outV[2] };
#endif
}

// unused
// 0x698790
CVector CCover::FindVectorFromFirstToMissingVertex(CColTriangle* triangle, int32* a3, CVector* vertPositions) {
    NOTSA_UNREACHABLE_RET({}, "Unused Function");

    //uint16 vertexIndex;
    //uint16 referenceIndex = *a3;
    //
    //// Is vertex missing ?
    //if ((triangle->vA != referenceIndex && triangle->vA != a3[1])) {
    //    vertexIndex = triangle->vA;
    //} else if (triangle->vB != referenceIndex && triangle->vB != a3[1]) {
    //    vertexIndex = triangle->vB;
    //} else {
    //    vertexIndex = triangle->vC;
    //}
    //
    //CVector vector;
    //vector.x = vertPositions[vertexIndex].x - vertPositions[referenceIndex].x;
    //vector.y = vertPositions[vertexIndex].y - vertPositions[referenceIndex].y;
    //vector.z = vertPositions[vertexIndex].z - vertPositions[referenceIndex].z;
    //
    //return vector;
}
