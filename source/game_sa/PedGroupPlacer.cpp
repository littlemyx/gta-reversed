#include "StdInc.h"

#include "PedGroupPlacer.h"
#include "PedGroups.h"
#include "Population.h"
#include "PedPlacement.h"
#include "VisibilityPlugins.h"
#include "TaskComplexBeInGroup.h"
#include "TaskComplexWanderGang.h"
#include "TaskComplexFollowLeaderInFormation.h"
#include "PedGroupDefaultTaskAllocators.h"

void CPedGroupPlacer::InjectHooks() {
    RH_ScopedClass(CPedGroupPlacer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(PlaceFormationGroup, 0x5FC9B0);
    RH_ScopedInstall(PlaceChatGroup, 0x5FCE80);
    RH_ScopedInstall(PlaceRandomGroup, 0x5FD330);
    RH_ScopedInstall(PlaceGroup, 0x5FD810);
}

namespace {
// [Shared by all `Place*Group` functions]
// `true` if it's okay to create peds around `origin` (they would be out of the player's sight, and the area is clear)
bool CanPlaceGroupAt(const CVector& origin, float radius) {
    if (TheCamera.IsSphereVisible(origin, radius)) {
        const auto* const player = FindPlayerPed(-1);
        const auto distToPlayer  = (origin - player->GetPosition()).Magnitude2D();
        if (distToPlayer < CPopulation::PedCreationDistMultiplier() * 42.5f) { // 0x86C850
            return false;
        }
    }
    return CPedPlacement::IsPositionClearForPed(origin, radius, -1, nullptr, true, true, true);
}

// Common to chat/random groups: Places peds in a circle around `origin`.
// Returns the number of peds placed (the placed peds are stored in `outPeds`)
int32 PlacePedsInCircle(ePedType type, uint32 numOfPeds, const CVector& origin, float stepAngle, float radius, std::array<CPed*, TOTAL_PED_GROUP_MEMBERS>& outPeds) {
    int32   numPlaced = 0;
    CVector firstPos{};
    for (int32 i = 0; i < (int32)numOfPeds; i++) {
        const auto angleJitter  = ((float)rand() * RAND_MAX_FLOAT_RECIPROCAL * 0.4f - 0.2f) * stepAngle;
        const auto radiusJitter = ((float)rand() * RAND_MAX_FLOAT_RECIPROCAL * 0.4f - 0.2f) * radius;
        const auto angle        = (float)i * stepAngle + angleJitter;
        const auto dist         = radiusJitter + radius;
        const auto offsetX      = std::cosf(angle) * dist;
        const auto offsetY      = std::sinf(angle) * dist;

        bool       foundGround{};
        auto       groundZ = CWorld::FindGroundZFor3DCoord(CVector{ offsetX + origin.x, offsetY + origin.y, origin.z + 1.0f }, &foundGround, nullptr) + 1.0f;
        if (!foundGround) {
            continue;
        }
        if (groundZ < origin.z) {
            groundZ = origin.z;
        }
        const CVector pos{ offsetX + origin.x, offsetY + origin.y, groundZ };
        if (i == 0) {
            firstPos = pos;
        }

        const auto model = CPopulation::ChooseGangOccupation((eGangID)((int32)type - 7));
        auto* const mi   = CModelInfo::GetModelInfo(model);
        if (!mi->m_pRwObject) {
            continue;
        }

        // Make sure the position is clear (ignoring the peds we've already placed)
        std::array<CEntity*, 9> hitEntities{};
        CPedPlacement::IsPositionClearForPed(pos, mi->GetColModel()->GetBoundRadius(), (int32)hitEntities.size(), hitEntities.data(), true, true, true);
        bool isBlocked = false;
        for (auto* const hit : hitEntities) {
            if (hit && std::find(outPeds.begin(), outPeds.begin() + numPlaced, hit) == outPeds.begin() + numPlaced) {
                isBlocked = true;
                break;
            }
        }

        bool isLOSClear, isSameHeight;
        if (i == 0) {
            isLOSClear   = true;
            isSameHeight = true;
        } else {
            isLOSClear   = CWorld::GetIsLineOfSightClear(pos, firstPos, true, false, false, false, false, false, false);
            isSameHeight = std::abs(pos.z - firstPos.z) < 1.0f;
        }
        if (isBlocked || !isLOSClear || !isSameHeight) {
            continue;
        }

        auto* const ped = CPopulation::AddPed(type, model, pos, false);
        if (!ped) {
            // NOTSA: The original code calls `RemovePed(nullptr)` here (which would crash)
            continue;
        }
        outPeds[numPlaced++] = ped;

        const auto heading = CGeneral::GetRadianAngleBetweenPoints(origin.x, origin.y, pos.x, pos.y);
        ped->m_fCurrentRotation = heading;
        ped->m_fAimingRotation  = heading;
        CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);
    }
    return numPlaced;
}

void SetTaskBeInGroup(CPed* ped, int32 groupId, bool isLeader) {
    ped->GetTaskManager().SetTask(new CTaskComplexBeInGroup(groupId, isLeader), TASK_PRIMARY_PRIMARY, false);
}
}; // namespace

// 0x5FC9B0
bool CPedGroupPlacer::PlaceFormationGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType unused) {
    const auto groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    if (!CanPlaceGroupAt(origin, 3.0f)) {
        return false;
    }

    bool foundGround{};
    auto groundZ = CWorld::FindGroundZFor3DCoord(CVector{ origin.x, origin.y, origin.z + 1.0f }, &foundGround, nullptr) + 1.0f;
    if (!foundGround) {
        return false;
    }
    const auto z = groundZ >= origin.z ? groundZ : origin.z;

    const auto model = CPopulation::ChooseGangOccupation((eGangID)((int32)type - 7));
    if (!CModelInfo::GetModelInfo(model)->m_pRwObject) {
        return false;
    }

    auto* const leader = CPopulation::AddPed(type, model, CVector{ origin.x, origin.y, z }, false);
    if (!leader) {
        return false;
    }

    std::array<CPed*, TOTAL_PED_GROUP_MEMBERS> peds{ leader };
    int32 numPlaced = 1;
    for (int32 i = 1; i < (int32)numOfPeds; i++) {
        const auto followerModel = CPopulation::ChooseGangOccupation((eGangID)((int32)type - 7));
        if (!CModelInfo::GetModelInfo(followerModel)->m_pRwObject) {
            continue;
        }

        auto* const ped = CPopulation::AddPed(type, followerModel, CVector{ origin.x, origin.y, z }, false);
        if (!ped) {
            continue;
        }

        const auto& offsets = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets[i];
        ped->SetPosn(leader->GetPosition() + CVector{ offsets.x, offsets.y, 0.0f });

        const auto pedPos = ped->GetPosition();
        bool       foundPedGround{};
        auto       pedGroundZ = CWorld::FindGroundZFor3DCoord(CVector{ pedPos.x, pedPos.y, pedPos.z + 1.0f }, &foundPedGround, nullptr) + 1.0f;
        const auto pedZ       = pedPos.z <= pedGroundZ ? pedGroundZ : pedPos.z;
        if (!foundPedGround) {
            CPopulation::RemovePed(ped);
            continue;
        }
        if (!(std::abs(pedZ - leader->GetPosition().z) <= 1.0f)) {
            CPopulation::RemovePed(ped);
            continue;
        }
        if (!CWorld::GetIsLineOfSightClear(CVector{ pedPos.x, pedPos.y, pedZ }, leader->GetPosition(), true, false, false, false, false, false, false)) {
            CPopulation::RemovePed(ped);
            continue;
        }

        ped->SetPosn(pedPos.x, pedPos.y, pedZ);
        peds[numPlaced++] = ped;
        CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);
    }

    auto& group = CPedGroups::GetGroup(groupId);
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::FOLLOW_ANY_MEANS));
    group.GetMembership().SetLeader(peds[0]);
    group.GetMembership().Process();
    group.GetIntelligence().Process();
    SetTaskBeInGroup(peds[0], groupId, true);

    for (int32 i = 1; i < numPlaced; i++) {
        group.GetMembership().AddFollower(peds[i]);
        group.GetMembership().Process();
        group.GetIntelligence().Process();
        SetTaskBeInGroup(peds[i], groupId, false);
    }
    return true;
}

// 0x5FCE80
bool CPedGroupPlacer::PlaceChatGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType unused) {
    if (numOfPeds < 2) {
        return false;
    }

    const auto groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    const auto stepAngle = TWO_PI / (float)numOfPeds;
    const auto radius    = std::sqrt(0.5f / (1.0f - std::cosf(stepAngle)));
    if (!CanPlaceGroupAt(origin, radius)) {
        return false;
    }

    std::array<CPed*, TOTAL_PED_GROUP_MEMBERS> peds{};
    const auto numPlaced = PlacePedsInCircle(type, numOfPeds, origin, stepAngle, radius, peds);
    if (numPlaced < 1) {
        return false;
    }

    auto& group = CPedGroups::GetGroup(groupId);
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::STAND_STILL));
    group.GetMembership().SetLeader(peds[0]);
    group.GetMembership().Process();
    group.GetIntelligence().Process();
    SetTaskBeInGroup(peds[0], groupId, true);

    for (int32 i = 1; i < numPlaced; i++) {
        group.GetMembership().AddFollower(peds[i]);
        group.GetMembership().Process();
        group.GetIntelligence().Process();
        SetTaskBeInGroup(peds[i], groupId, false);
    }
    return true;
}

// 0x5FD330
bool CPedGroupPlacer::PlaceRandomGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType unused) {
    if (numOfPeds < 2) {
        return false;
    }

    const auto groupId = CPedGroups::AddGroup();
    if (groupId < 0) {
        return false;
    }

    const auto stepAngle = TWO_PI / (float)numOfPeds;
    const auto radius    = std::sqrt(0.5f / (1.0f - std::cosf(stepAngle)));
    if (!CanPlaceGroupAt(origin, radius)) {
        return false;
    }

    std::array<CPed*, TOTAL_PED_GROUP_MEMBERS> peds{};
    const auto numPlaced = PlacePedsInCircle(type, numOfPeds, origin, stepAngle, radius, peds);
    if (numPlaced < 1) {
        return false;
    }

    auto& group = CPedGroups::GetGroup(groupId);
    group.GetIntelligence().SetDefaultTaskAllocator(CPedGroupDefaultTaskAllocators::Get(ePedGroupDefaultTaskAllocatorType::RANDOM));
    for (int32 i = 0; i < numPlaced; i++) {
        if (i == 0) {
            group.GetMembership().SetLeader(peds[0]);
        } else {
            group.GetMembership().AddFollower(peds[i]);
        }
        group.GetMembership().Process();
        group.GetIntelligence().Process();

        const auto dir = (uint8)(int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f) * 8.0f);
        peds[i]->GetTaskManager().SetTask(new CTaskComplexWanderGang((eMoveState)4, dir, 5000, true, 0.5f), TASK_PRIMARY_DEFAULT, false);
        SetTaskBeInGroup(peds[i], groupId, false);
    }
    return true;
}

// 0x5FD810
bool CPedGroupPlacer::PlaceGroup(ePedType type, uint32 numOfPeds, const CVector& origin, ePedGroupDefaultTaskAllocatorType allocType) {
    using enum ePedGroupDefaultTaskAllocatorType;
    switch (allocType) {
    case FOLLOW_ANY_MEANS:
    case FOLLOW_LIMITED:   return PlaceFormationGroup(type, numOfPeds, origin, allocType);
    case STAND_STILL:
    case CHAT:             return PlaceChatGroup(type, numOfPeds, origin, allocType);
    case RANDOM:           return PlaceRandomGroup(type, numOfPeds, origin, allocType);
    default:               NOTSA_UNREACHABLE();
    }
}
