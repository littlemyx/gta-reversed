#include "StdInc.h"
#include "InteriorGroup_c.h"

#include "Interior_c.h"
#include "InteriorManager_c.h"
#include "PopCycle.h"
#include "ScriptsForBrains.h"
#include "Interior/TaskInteriorBeInHouse.h"
#include "Interior/TaskInteriorBeInShop.h"
#include "Interior/TaskInteriorBeInOffice.h"
#include "Interior/TaskInteriorShopKeeper.h"

void InteriorGroup_c::InjectHooks() {
    RH_ScopedClass(InteriorGroup_c);
    RH_ScopedCategory("Interior");

    //RH_ScopedInstall(Constructor, 0x597FE0, { .Reversed = false });
    //RH_ScopedInstall(Destructor, 0x597FF0, { .Reversed = false });

    RH_ScopedInstall(Init, 0x5947E0);
    RH_ScopedInstall(Update, 0x5968E0);
    RH_ScopedInstall(SetupPeds, 0x596890);
    RH_ScopedInstall(UpdatePeds, 0x596830);
    RH_ScopedInstall(SetupHousePeds, 0x5965E0);
    RH_ScopedInstall(SetupPaths, 0x595590);
    RH_ScopedInstall(ArePathsLoaded, 0x595380);
    RH_ScopedInstall(Setup, 0x595320);
    RH_ScopedInstall(Exit, 0x595290);
    RH_ScopedInstall(ContainsInteriorType, 0x595250);
    RH_ScopedInstall(CalcIsVisible, 0x595200);
    RH_ScopedInstall(DereferenceAnims, 0x595160);
    RH_ScopedInstall(ReferenceAnims, 0x5950D0);
    RH_ScopedInstall(UpdateOfficePeds, 0x594E90);
    RH_ScopedInstall(RemovePed, 0x594E30);
    RH_ScopedInstall(SetupShopPeds, 0x594C10);
    RH_ScopedInstall(SetupOfficePeds, 0x594BF0);
    RH_ScopedInstall(GetEntity, 0x594BD0);
    RH_ScopedInstall(GetPed, 0x594B90);
    RH_ScopedInstall(FindClosestInteriorInfo, 0x594A50);
    RH_ScopedInstall(FindInteriorInfo, 0x594970);
    RH_ScopedInstall(GetNumInteriorInfos, 0x594920);
    RH_ScopedInstall(GetRandomInterior, 0x5948C0);
    RH_ScopedInstall(AddInterior, 0x594840);
}

namespace {
// Members of `g_interiorMan` (private)
auto& s_ArePedsEnabled        = StaticRef<bool>(0xBB3DC2);                    // InteriorManager_c::m_ArePedsEnabled
auto& s_InteriorPedsAliveState = StaticRef<std::array<bool, 16>>(0xBB3D9C);   // InteriorManager_c::m_InteriorPedsAliveState

// `(int)(rand() * (1 / 32768.f) * n)` - Same as `RandBelow` in `Interior_c.cpp` (named differently so unity builds don't break)
int32 GroupRandBelow(int32 n) {
    return static_cast<int32>(static_cast<float>(CGeneral::GetRandomNumber()) * (1.f / 32768.f) * static_cast<float>(n));
}

// Position of the entity (made by transforming (0,0,0) with its matrix; allocates the matrix if needed)
CVector GetEntityOrigin(CEntity* entity) {
    if (!entity->m_matrix) {
        entity->AllocateMatrix();
        entity->m_placement.UpdateMatrix(entity->m_matrix);
    }
    return entity->m_matrix->TransformPoint(CVector{});
}

// Sets up the freshly created `ped` as an interior ped with the given task
void SetupInteriorPed(CPed* ped, CTask* task) {
    ped->SetCharCreatedBy(PED_MISSION);
    ped->GetIntelligence()->SetPedDecisionMakerType(7);
    ped->GetIntelligence()->GetTaskManager().SetTask(task, TASK_PRIMARY_DEFAULT, false);
}
} // namespace

// 0x5947E0
void InteriorGroup_c::Init(CEntity* entity, int32 id) {
    std::ranges::fill(m_interiors, nullptr);
    std::ranges::fill(m_peds, nullptr);
    std::ranges::fill(m_pedsToRemove, nullptr);
    m_numInteriors        = 0;
    m_pathSetupComplete   = false;
    m_updatePeds          = false;
    m_isVisible           = false;
    m_lastIsVisible       = false;
    m_animBlockReferenced = false;
    m_pEntity             = entity;
    m_groupId             = static_cast<uint8>(id);
}

// 0x5968E0
void InteriorGroup_c::Update() {
    CalcIsVisible();
    if (!m_pathSetupComplete) {
        SetupPaths();
    }
    if (m_pathSetupComplete && !m_updatePeds) {
        SetupPeds();
    }
    if (m_updatePeds) {
        UpdatePeds();
    }
    ReferenceAnims();
}

// 0x594840
void InteriorGroup_c::AddInterior(Interior_c* interior) {
    for (auto& slot : m_interiors) {
        if (!slot) {
            slot = interior;
            m_numInteriors++;
            return;
        }
    }
}

// 0x596890
void InteriorGroup_c::SetupPeds() {
    if (!m_EnEx || !s_ArePedsEnabled) {
        return;
    }
    switch (m_groupType) {
    case 0: SetupHousePeds();  break;
    case 1: SetupShopPeds();   break;
    case 2: SetupOfficePeds(); break;
    }
    m_updatePeds = true;
}

// 0x596830
void InteriorGroup_c::UpdatePeds() {
    if (!m_EnEx || !s_ArePedsEnabled) {
        return;
    }

    for (auto& ped : m_pedsToRemove) {
        if (ped) {
            if (ped->IsPointerValid()) {
                RemovePed(ped);
            }
            ped = nullptr;
        }
    }

    if (m_groupType == 2) {
        UpdateOfficePeds();
    }
}

// 0x5965E0
void InteriorGroup_c::SetupHousePeds() {
    CStreaming::StreamPedsForInterior(0);
    CStreaming::LoadAllRequestedModels(false);

    m_numPeds = 0;

    const auto pos = GetEntityOrigin(m_pEntity);

    int32 numPeds = (GroupRandBelow(100) <= 50) + 1;

    // Chance to create gang members instead
    bool        isGang  = false;
    ePedType    gangType = static_cast<ePedType>(0);
    if (CPopCycle::m_pCurrZoneInfo) {
        gangType = CPopCycle::PickGangToCreateMembersOf();
        if (gangType != static_cast<ePedType>(0) && CPopulation::ChooseGangOccupation(static_cast<eGangID>(gangType - PED_TYPE_GANG1)) >= 0) {
            isGang = true;
            if (GroupRandBelow(100) <= 15) {
                numPeds = 3 + (CGeneral::GetRandomNumber() >= 0x3FFF);
            } else {
                isGang = false;
            }
        }
    }

    for (int32 i = 0; i < numPeds; i++) {
        eModelID modelId;
        if (isGang) {
            modelId = CPopulation::ChooseGangOccupation(static_cast<eGangID>(gangType - PED_TYPE_GANG1));
        } else if (numPeds == 1) {
            modelId = static_cast<eModelID>(CStreaming::FindMIPedSlotForInterior(CGeneral::GetRandomNumber() < 0x3FFF ? 0 : 1));
        } else {
            modelId = static_cast<eModelID>(CStreaming::FindMIPedSlotForInterior(i));
        }

        const auto pedIdx = m_numPeds;
        auto* const ped   = CPopulation::AddPed(CModelInfo::GetPedModelInfo(modelId)->m_nPedType, modelId, pos, false);
        m_peds[pedIdx]    = ped;
        if (ped) {
            m_numPeds++;
            CTheScripts::ScriptsForBrains.StartOrRequestNewStreamedScriptBrainWithThisName("house", ped, 3);
            SetupInteriorPed(ped, new CTaskInteriorBeInHouse(this));
        }
        if (!s_InteriorPedsAliveState[i]) {
            RemovePed(m_peds[pedIdx]);
        }
    }
}

// 0x595590
void InteriorGroup_c::SetupPaths() {
    if (!ArePathsLoaded()) {
        return;
    }

    auto& paths = ThePaths;
    paths.StartNewInterior(static_cast<int8>(m_id));

    const auto AddNode = [&](const CVector& pos, bool dontWander = false) {
        return paths.AddNodeToNewInterior(pos.x, pos.y, pos.z, dontWander, -1, -1, -1, -1, -1, -1);
    };

    // 1. Add nodes for all goto points
    int32 baseNode[8]{}; // Index of the first node of each interior
    int32 linkIdx = 0;
    int32 total   = 0;
    for (size_t slot = 0; slot < std::size(m_interiors); slot++) {
        auto* const interior = m_interiors[slot];
        if (!interior) {
            continue;
        }
        baseNode[slot] = total;
        const int32 count = interior->m_gotoPtsCount;
        if (count > 2) {
            AddNode(interior->m_gotoPts[0].Pos);
            AddNode(interior->m_gotoPts[1].Pos);
            paths.AddInteriorLink(linkIdx + 1, total);
            linkIdx += 2;
            for (int32 k = 2; k < count; k++) {
                AddNode(interior->m_gotoPts[k].Pos);
                paths.AddInteriorLink(linkIdx, total + (k - 2));
                if (k & 1) {
                    paths.AddInteriorLink(linkIdx, total + (k - 1));
                }
                linkIdx++;
            }
        }
        // NOTE: Unlike `linkIdx` this is increased even if no nodes were added
        total += count;
    }

    // 2. Add nodes for the exits, and link them to the goto points (and exterior nodes)
    // NOTSA: The original has room for only 16 entries
    constexpr size_t MAX_UNLINKED = 32;
    CVector          unlinkedPos[MAX_UNLINKED];
    int32            unlinkedNode[MAX_UNLINKED];
    bool             unlinkedDone[MAX_UNLINKED]{};
    int32            numUnlinked = 0;

    int32 cur = total; // Index of the next node to be added (`linkIdx` is always `cur - 1` from now on)
    for (size_t slot = 0; slot < std::size(m_interiors); slot++) {
        auto* const interior = m_interiors[slot];
        if (!interior) {
            continue;
        }
        const auto& box     = *interior->m_box;
        const int8  starts[4] = { box.m_door, box.m_lDoorStart, box.m_tDoorStart, box.m_rDoorStart };

        bool  hasPosition = false;
        int32 insideNode[4]{};
        for (int32 e = 0; e < 4; e++) {
            if (starts[e] < 0) {
                continue;
            }
            auto&      exitPt = interior->m_exitPts[e];
            const auto addr   = AddNode(exitPt.PosInside);
            for (const auto idx : exitPt.GotoPtIdx) {
                if (idx != -1) {
                    paths.AddInteriorLink(cur, idx + baseNode[slot]);
                }
            }
            if (!hasPosition) {
                interior->m_position    = exitPt.PosInside;
                interior->m_nodeAddress = addr;
                hasPosition             = true;
            }

            insideNode[e]           = cur;
            const int32 outsideNode = cur + 1;
            AddNode(exitPt.PosOutside, e == 0 && m_groupType != 0); // NOTE: Only the first exit considers the group type
            paths.AddInteriorLink(outsideNode, cur);

            const auto ext = paths.FindNearestExteriorNodeToInteriorNode(outsideNode);
            if (ext.m_wAreaId == 0xFFFF) {
                // No exterior node nearby, try to link it to another exit later
                unlinkedPos[numUnlinked]  = exitPt.PosOutside;
                unlinkedNode[numUnlinked] = outsideNode;
                numUnlinked++;
            } else {
                const CVector nodePos = paths.m_pPathNodes[ext.m_wAreaId][ext.m_wNodeId].GetPosition();
                interior->m_exteriorNodeAddress = ext;
                interior->m_exteriorNodePos     = nodePos;
                if (m_groupType == 0) {
                    const auto dx = nodePos.x - exitPt.PosOutside.x;
                    const auto dy = nodePos.y - exitPt.PosOutside.y;
                    const auto dz = nodePos.z - exitPt.PosOutside.z;
                    // NOTE: The order of the additions is the same as in the original (it differs for the 1st exit)
                    const auto distSq = e == 0 ? dy * dy + dz * dz + dx * dx : dx * dx + dz * dz + dy * dy;
                    if (distSq <= 9.f) { // NOTE: `<=`, not `<`
                        paths.AddInteriorLinkToExternalNode(outsideNode, ext);
                    }
                }
            }
            cur += 2;
        }

        // If there are no goto points, link the exits directly
        if (interior->m_gotoPtsCount == 0) {
            if (box.m_door >= 0) {
                if (box.m_lDoorStart >= 0) { paths.AddInteriorLink(insideNode[0], insideNode[1]); }
                if (box.m_tDoorStart >= 0) { paths.AddInteriorLink(insideNode[0], insideNode[2]); }
                if (box.m_rDoorStart >= 0) { paths.AddInteriorLink(insideNode[0], insideNode[3]); }
            }
            if (box.m_tDoorStart >= 0) {
                if (box.m_lDoorStart >= 0) { paths.AddInteriorLink(insideNode[2], insideNode[1]); }
                if (box.m_rDoorStart >= 0) { paths.AddInteriorLink(insideNode[2], insideNode[3]); }
            }
        }
    }

    // 3. Link the exits that have no exterior node to the closest other such exit
    for (int32 i = 0; i < numUnlinked; i++) {
        if (unlinkedDone[i]) {
            continue;
        }
        int32 best     = -1;
        float bestDist = 1000000000.f;
        for (int32 j = i + 1; j < numUnlinked; j++) {
            if (unlinkedDone[j]) {
                continue;
            }
            const auto dx   = unlinkedPos[i].x - unlinkedPos[j].x;
            const auto dy   = unlinkedPos[i].y - unlinkedPos[j].y;
            const auto dz   = unlinkedPos[i].z - unlinkedPos[j].z;
            const auto dist = dy * dy + dz * dz + dx * dx; // NOTE: Same order of additions as in the original
            if (dist < bestDist) {
                best     = j;
                bestDist = dist;
            }
        }
        if (bestDist >= 3.f) {
            paths.RemoveLinksToNewInteriorNode(unlinkedNode[i]); // 0x44DF60
        } else {
            unlinkedDone[i]    = true;
            unlinkedDone[best] = true;
            paths.AddInteriorLink(unlinkedNode[i], unlinkedNode[best]);
        }
    }

    paths.CompleteNewInterior(nullptr); // 0x452270
    m_pathSetupComplete = true;
}

// 0x595380
bool InteriorGroup_c::ArePathsLoaded() {
    auto* const entity = m_pEntity;
    const auto& bb     = CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundingBox();
    const auto& mn     = bb.m_vecMin;
    const auto& mx     = bb.m_vecMax;

    const CVector corners[8] = {
        { mn.x, mn.y, mn.z }, { mn.x, mx.y, mn.z }, { mx.x, mx.y, mn.z }, { mx.x, mn.y, mn.z },
        { mn.x, mn.y, mx.z }, { mn.x, mx.y, mx.z }, { mx.x, mx.y, mx.z }, { mx.x, mn.y, mx.z },
    };

    CVector lo{ 999999.f, 999999.f, 999999.f };
    CVector hi{ -999999.f, -999999.f, -999999.f };
    for (const auto& corner : corners) {
        if (!entity->m_matrix) {
            entity->AllocateMatrix();
            entity->m_placement.UpdateMatrix(entity->m_matrix);
        }
        const auto pt = entity->m_matrix->TransformPoint(corner);
        lo.x = std::min(lo.x, pt.x);
        lo.y = std::min(lo.y, pt.y);
        lo.z = std::min(lo.z, pt.z);
        hi.x = std::max(hi.x, pt.x);
        hi.y = std::max(hi.y, pt.y);
        hi.z = std::max(hi.z, pt.z);
    }
    return ThePaths.AreNodesLoadedForArea(lo.x, hi.x, lo.y, hi.y);
}

// 0x595320
void InteriorGroup_c::Setup() {
    if (ContainsInteriorType(2)) {
        m_groupType = static_cast<uint8>(eInteriorGroupType::HOUSE);
    } else if (ContainsInteriorType(0) || ContainsInteriorType(6)) {
        m_groupType = static_cast<uint8>(eInteriorGroupType::SHOP);
    } else {
        // NOTE: -1 if there's no office
        m_groupType = ContainsInteriorType(1) ? static_cast<uint8>(eInteriorGroupType::OFFICE) : static_cast<uint8>(-1);
    }
    ReferenceAnims();
}

// 0x595290
void InteriorGroup_c::Exit() {
    for (auto* const interior : m_interiors) {
        if (interior) {
            interior->Exit();
            g_interiorMan.ReturnInteriorToPool(interior);
        }
    }

    for (size_t i = 0; i < std::size(m_peds); i++) {
        auto*& ped = m_peds[i];
        if (!ped) {
            continue;
        }
        if (!ped->IsPointerValid()) {
            s_InteriorPedsAliveState[i] = false;
        } else {
            if (!ped->IsAlive()) {
                s_InteriorPedsAliveState[i] = false;
            }
            CPopulation::RemovePed(ped);
        }
        ped = nullptr;
    }
    m_numPeds = 0;

    ThePaths.RemoveInterior(static_cast<int8>(m_id));
    DereferenceAnims();
}

// 0x595250
bool InteriorGroup_c::ContainsInteriorType(int32 type) {
    for (auto* const interior : m_interiors) {
        if (interior && static_cast<int32>(interior->m_box->m_type) == type) {
            return true;
        }
    }
    return false;
}

// 0x595200
void InteriorGroup_c::CalcIsVisible() {
    m_lastIsVisible = m_isVisible;
    m_isVisible     = false;
    for (int32 i = 0; i < m_numInteriors; i++) {
        if (m_interiors[i]->IsVisible()) {
            m_isVisible = true;
            break;
        }
    }
}

// 0x595160
void InteriorGroup_c::DereferenceAnims() {
    if (!m_animBlockReferenced) {
        return;
    }
    CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(GetAnimBlockName())); // 0x4D3FD0 (was `AddAnimBlockRef`)
    m_animBlockReferenced = false;
}

// 0x5950D0
void InteriorGroup_c::ReferenceAnims() {
    if (m_animBlockReferenced) {
        return;
    }
    const auto animBlkIdx = CAnimManager::GetAnimationBlockIndex(GetAnimBlockName());
    if (CStreaming::IsModelLoaded(IFPToModelId(animBlkIdx))) {
        CAnimManager::AddAnimBlockRef(animBlkIdx);
        m_animBlockReferenced = true;
    } else {
        CStreaming::RequestModel(IFPToModelId(animBlkIdx), STREAMING_KEEP_IN_MEMORY);
    }
}

// 0x594E90
void InteriorGroup_c::UpdateOfficePeds() {
    if (m_isVisible) {
        if (m_lastIsVisible) {
            return; // Peds were already created
        }

        const auto pos = GetEntityOrigin(m_pEntity);

        // Number of peds depends on the time
        int32 numDesks = GetNumInteriorInfos(6);
        int32 numPeds;
        if (CClock::GetIsTimeInRange(9, 18)) {
            numPeds = CGeneral::GetRandomNumberInRange(numDesks / 2, numDesks);
        } else if (CClock::GetIsTimeInRange(18, 22) || CClock::GetIsTimeInRange(6, 9)) {
            numPeds = CGeneral::GetRandomNumberInRange(0, numDesks / 2);
        } else {
            numPeds = 0;
        }
        if (numPeds >= 16) {
            numPeds = 16;
        }

        for (int32 i = 0; i < numPeds; i++) {
            const auto modelId = static_cast<eModelID>(CStreaming::FindMIPedSlotForInterior(GroupRandBelow(8)));
            const auto pedIdx  = m_numPeds;
            auto* const ped    = CPopulation::AddPed(CModelInfo::GetPedModelInfo(modelId)->m_nPedType, modelId, pos, false);
            m_peds[pedIdx]     = ped;
            if (ped) {
                m_numPeds++;
                SetupInteriorPed(ped, new CTaskInteriorBeInOffice(this));
            }
        }
    } else if (m_lastIsVisible) {
        // Not visible anymore, so remove all peds
        for (auto& ped : m_peds) {
            if (ped) {
                if (ped->IsPointerValid()) {
                    CPopulation::RemovePed(ped);
                }
                ped = nullptr;
            }
        }
        m_numPeds = 0;
    }
}

// 0x594E30
void InteriorGroup_c::RemovePed(CPed* ped) {
    for (auto& p : m_peds) {
        if (p && p == ped) {
            CPopulation::RemovePed(ped);
            p = nullptr;
            m_numPeds--;
            return;
        }
    }
}

// 0x594C10
void InteriorGroup_c::SetupShopPeds() {
    CStreaming::StreamPedsForInterior(1);
    CStreaming::LoadAllRequestedModels(false);

    m_numPeds = 0;

    // NOTE: `RandBelow(-3)` is in [-2, 0]
    const int32 numPeds = (2 - GroupRandBelow(-3)) * m_numInteriors + 1;
    for (int32 i = 0; i < numPeds; i++) {
        // The first ped is the shopkeeper
        const auto modelId = static_cast<eModelID>(CStreaming::FindMIPedSlotForInterior(i == 0 ? 0 : 1 - GroupRandBelow(-7)));
        const auto pedType = CModelInfo::GetPedModelInfo(modelId)->m_nPedType;

        auto* const interior = GetRandomInterior();
        int32       tileX, tileY;
        interior->GetRandomTile(3, &tileX, &tileY);
        CVector pos;
        interior->GetTileCentre(static_cast<float>(tileX), static_cast<float>(tileY), &pos);
        pos.z += 1.f;

        const auto pedIdx = m_numPeds;
        auto* const ped   = CPopulation::AddPed(pedType, modelId, pos, false);
        m_peds[pedIdx]    = ped;
        if (ped) {
            m_numPeds++;
            if (i == 0) {
                SetupInteriorPed(ped, new CTaskInteriorShopKeeper(this, false));
            } else {
                SetupInteriorPed(ped, new CTaskInteriorBeInShop(this));
            }
        }
    }
}

// 0x594BF0
void InteriorGroup_c::SetupOfficePeds() {
    CStreaming::StreamPedsForInterior(2);
    CStreaming::LoadAllRequestedModels(false);
    m_numPeds = 0;
}

// 0x594BD0
CEntity* InteriorGroup_c::GetEntity() {
    return m_pEntity;
}

// 0x594B90
CPed* InteriorGroup_c::GetPed(int32 idx) {
    return m_peds[idx];
}

// 0x594A50
bool InteriorGroup_c::FindClosestInteriorInfo(int32 type, CVector point, float radius, InteriorInfo_t** outInfo, Interior_c** outInterior, float* outDistSq) {
    const auto radiusSq = radius * radius;

    InteriorInfo_t* bestInfo     = nullptr;
    Interior_c*     bestInterior = nullptr;
    float           bestDistSq   = 999999.f;
    for (auto* const interior : m_interiors) {
        if (!interior || !interior->IsPtInside(point)) {
            continue;
        }
        for (int32 i = 0; i < interior->m_interiorInfosCount; i++) {
            auto& info = interior->m_interiorInfos[i];
            if ((type == -1 || static_cast<int32>(info.Type.get()) == type) && !info.IsInUse) {
                const auto distSq = (point.x - info.Pos.x) * (point.x - info.Pos.x)
                                  + (point.y - info.Pos.y) * (point.y - info.Pos.y)
                                  + (point.z - info.Pos.z) * (point.z - info.Pos.z);
                if (distSq < radiusSq && distSq < bestDistSq) {
                    bestInfo     = &info;
                    bestInterior = interior;
                    bestDistSq   = distSq;
                }
            }
        }
    }
    if (!bestInfo) {
        return false;
    }
    *outInfo     = bestInfo;
    *outInterior = bestInterior;
    *outDistSq   = bestDistSq;
    return true;
}

// 0x594970
bool InteriorGroup_c::FindInteriorInfo(eInteriorInfoType infoType, InteriorInfo_t** outInfo, Interior_c** outInterior) {
    // NOTSA: The original has room for only 64 candidates
    InteriorInfo_t* infos[128];
    Interior_c*     interiors[128];
    int32           numCandidates = 0;
    for (auto* const interior : m_interiors) {
        if (!interior) {
            continue;
        }
        for (int32 i = 0; i < interior->m_interiorInfosCount; i++) {
            auto& info = interior->m_interiorInfos[i];
            if (info.Type == infoType && !info.IsInUse) {
                infos[numCandidates]     = &info;
                interiors[numCandidates] = interior;
                numCandidates++;
            }
        }
    }

    if (numCandidates > 0) {
        const auto idx = GroupRandBelow(numCandidates);
        *outInfo     = infos[idx];
        *outInterior = interiors[idx];
        return true;
    }
    *outInfo     = nullptr;
    *outInterior = nullptr;
    return false;
}

// 0x594920
int32 InteriorGroup_c::GetNumInteriorInfos(int32 type) {
    int32 n = 0;
    for (auto* const interior : m_interiors) {
        if (!interior) {
            continue;
        }
        for (int32 i = 0; i < interior->m_interiorInfosCount; i++) {
            if (static_cast<int32>(interior->m_interiorInfos[i].Type.get()) == type) {
                n++;
            }
        }
    }
    return n;
}

// 0x5948C0
Interior_c* InteriorGroup_c::GetRandomInterior() {
    const auto idx = GroupRandBelow(m_numInteriors);
    int32      n   = 0;
    for (auto* const interior : m_interiors) {
        if (interior) {
            if (n == idx) {
                return interior;
            }
            n++;
        }
    }
    return nullptr;
}

//! @notsa
const char* InteriorGroup_c::GetAnimBlockName() {
    switch ((eInteriorGroupType)m_groupType) {
    case eInteriorGroupType::HOUSE:  return "int_house";
    case eInteriorGroupType::SHOP:   return "int_shop";
    case eInteriorGroupType::OFFICE: return "int_office";
    default:                         NOTSA_UNREACHABLE();
    }
}
