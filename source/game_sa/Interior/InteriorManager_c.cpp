#include "StdInc.h"

#include "InteriorManager_c.h"
#include "FurnitureManager_c.h"

#include <TaskTypes/TaskSimpleCarDrive.h>

void InteriorManager_c::InjectHooks() {
    RH_ScopedClass(InteriorManager_c);
    RH_ScopedCategory("Interior");

    RH_ScopedGlobalInstall(AddSameGroupEffectInfos, 0x598430);
    RH_ScopedGlobalInstall(AreAnimsLoaded, 0x5980F0);
    RH_ScopedInstall(PruneVisibleEffects, 0x598A60);
    RH_ScopedInstall(Exit, 0x598010);
    RH_ScopedInstall(GetVisibleEffects, 0x598D80);
    RH_ScopedInstall(IsInteriorEffectVisible, 0x598690);
    RH_ScopedInstall(GetPedsInterior, 0x598620);
    RH_ScopedInstall(ReturnInteriorToPool, 0x5984B0);
    RH_ScopedInstall(GetInteriorFromPool, 0x5984A0);
    RH_ScopedInstall(GetVectorsInterior, 0x5983D0);
    RH_ScopedInstall(SetStealableObjectStolen, 0x598390);
    RH_ScopedOverloadedInstall(FindStealableObjectId, "1", 0x598360,  int32(InteriorManager_c::*)(CEntity *) const);
    RH_ScopedOverloadedInstall(FindStealableObjectId, "2", 0x5982F0,  int32(InteriorManager_c::*)(int32, int32, CVector) const);
    RH_ScopedInstall(HasInteriorHadStealDataSetup, 0x5982B0);
    RH_ScopedInstall(IsGroupActive, 0x598280);
    RH_ScopedInstall(GetPedsInteriorGroup, 0x598240);
    RH_ScopedInstall(SetEntryExitPtr, 0x598180);
    RH_ScopedInstall(GetBoundingBox, 0x598090);
    RH_ScopedInstall(ActivatePeds, 0x598080);
    RH_ScopedInstall(Update, 0x598F50);
    RH_ScopedInstall(Init, 0x5C0500);
}

// 0x5C0500
void InteriorManager_c::Init() {
    m_IsFrozen       = false;
    m_IsActive       = true;
    m_ArePedsEnabled = true;
    for (auto& i : m_Interiors) {
        m_InteriorPool.AddItem(&i);
    }
    for (auto&& [i, g] : rngv::enumerate(m_InteriorGroups)) {
        g.m_id = (uint8)i;
        m_InteriorGroupPool.AddItem(&g);
    }
    g_furnitureMan.Init();
    m_EnEx          = nullptr;
    m_InteriorCount = 0;
    m_ObjectCount   = 0;
    rng::fill(m_InteriorPedsAliveState, true);
    m_TimeLastPickupsGenerated = -180'000; // exe 0x5C05D7: 0xFFFD40E0 (was UINT32_MAX, which delayed the first pickup generation by 3 minutes of game time)
}

// 0x598F50
bool InteriorManager_c::Update() {
    ZoneScoped;

    if (m_IsFrozen) {
        return false;
    }

    const auto plyr = FindPlayerPed();

    InteriorEffectInfo_t visibleIntFxBuf[32];
    const auto numVisibleIntFx = plyr->GetAreaCode() != eAreaCodes::AREA_CODE_NORMAL_WORLD && m_IsActive && !plyr->GetTaskManager().GetActiveTaskAs<CTaskSimpleCarDrive>()
        ? GetVisibleEffects(visibleIntFxBuf, std::size(visibleIntFxBuf))
        : 0;
    PruneVisibleEffects(visibleIntFxBuf, numVisibleIntFx, 8, 20.f);
    const auto visibleIntFx = visibleIntFxBuf | rng::views::take(numVisibleIntFx);

    const auto IsFxAssociatedWithGroup = [](InteriorEffectInfo_t& fx, InteriorGroup_c& g) {
        return fx.Entity == g.GetEntity() && fx.Effects[0]->m_groupId == g.GetId();
    };

    // Remove interiors that are associated with effects that aren't visible anymore
    for (auto it = m_InteriorGroupList.begin(); it != m_InteriorGroupList.end();) {
        auto& g = *it;
        it++; // `RemoveItem` below invalidates the iterator, so increment here
        if (rng::none_of(visibleIntFx, [&](InteriorEffectInfo_t& fx) {
            return IsFxAssociatedWithGroup(fx, g) && !fx.IsCulled;
        })) {
            g.Exit();
            m_InteriorGroupList.RemoveItem(&g);
            m_InteriorGroupPool.AddItem(&g);
        }
    }

    bool hasAddedAnyInteriors{};
    for (auto& ifx : visibleIntFx) {
        //> 0x599071 - Not visible?
        if (ifx.IsCulled) {
            continue;
        }

        //> 0x59909F - Check if there's an existing interior group for this effect
        if (rng::any_of(m_InteriorGroupList, [&](InteriorGroup_c& g) {
            return IsFxAssociatedWithGroup(ifx, g);
        })) {
            continue;
        }

        //> 0x5990B8 - Make an interior group for this effect
        const auto grp = m_InteriorGroupPool.RemoveHead();
        assert(grp);
        grp->Init(ifx.Entity, ifx.Effects[0]->m_groupId);
        grp->m_EnEx = m_EnEx;
        m_InteriorGroupList.AddItem(grp);

        //> 0x59910C - Create interiors for it
        for (auto k = 0u; k < ifx.NumFx; k++) {
            const auto i = m_InteriorPool.RemoveHead();
            if (!i) { // No more interiors to allocate
                break;
            }
            const auto& fxPos = ifx.Entity->GetPosition();

            i->m_box        = ifx.Effects[k];
            i->m_interiorId = NOTSA_FTOL(uint32, fxPos.x * fxPos.y * fxPos.z) + ifx.FxIds[k];
            i->m_areaCode   = ifx.Entity->GetAreaCode();
            i->m_pGroup     = grp;

            i->Init(ifx.Effects[k]->m_Pos);
            grp->AddInterior(i);

            hasAddedAnyInteriors = true;
        }

        grp->Setup();

        m_TimeLastPickupsGenerated = CTimer::GetTimeInMS();
    }

    // Update all created groups
    for (auto& g : m_InteriorGroupList) {
        g.Update();
    }

    return hasAddedAnyInteriors;
}

// 0x598A60
void InteriorManager_c::PruneVisibleEffects(InteriorEffectInfo_t* pIntFxInfos, size_t numInfos, size_t maxVisibleFxCount, float maxDist) {
    const std::span intFxInfos{pIntFxInfos, (size_t)numInfos};
    const float maxDistSq = maxDist * maxDist; // The exe keeps it as a float

    // Check if there's anything to cull at all (more fxs visible than allowed): the exe compares the SUM of the NumFx fields (signed)
    int32 totalFx{};
    for (const auto& intFxInfo : intFxInfos) {
        totalFx += (int32)intFxInfo.NumFx;
    }
    if (totalFx <= (int32)maxVisibleFxCount) {
        return;
    }

    //> 0x598AA3 - Calculate distances to camera
    for (auto& intFxInfo : intFxInfos) {
        intFxInfo.DistSq = sq(10'000.f);
        for (int32 k = 0; k < (int32)intFxInfo.NumFx; k++) {
            const auto fx = intFxInfo.Effects[k];
            intFxInfo.DistSq = std::min(
                intFxInfo.DistSq,
                CVector2D::DistSqr(intFxInfo.Entity->GetMatrix().TransformPoint(fx->m_Pos), TheCamera.GetPosition2D())
            );
        }
    }

    //> 0x598B6E - Sort by distance to camera: the exe uses an exchange sort (swap i and j whenever j is strictly smaller), which orders ties and NaNs differently from std::sort
    for (size_t i = 0; i + 1 < numInfos; i++) {
        for (size_t j = i + 1; j < numInfos; j++) {
            if (intFxInfos[j].DistSq < intFxInfos[i].DistSq) {
                std::swap(intFxInfos[i], intFxInfos[j]);
            }
        }
    }

    //> 0x598D4E - Cull excess effects
    int32 n{};
    for (auto& ifxi : intFxInfos) {
        if ((int32)ifxi.NumFx + n > (int32)maxVisibleFxCount || !(ifxi.DistSq <= maxDistSq)) {
            ifxi.IsCulled = true;
        } else {
            n += (int32)ifxi.NumFx;
        }
    }
}

// 0x598430
void InteriorManager_c::AddSameGroupEffectInfos(InteriorEffectInfo_t* ifxi, int32 a2) {
    if (ifxi->NumFx + 1 >= ifxi->Effects.size()) { // NOTSA
        return;
    }

    const auto mi = ifxi->Entity->GetModelInfo();
    for (size_t i = 0; i < mi->m_n2dfxCount; i++) {
        if (i == ifxi->FxIds[0]) { // Already stored
            continue;
        }

        const auto fx = notsa::dyn_cast_if_present<C2dEffectInterior>(mi->Get2dEffect(i));
        if (!fx || ifxi->Effects[0]->m_groupId != fx->m_groupId) {
            continue;
        }

        const auto j     = ifxi->NumFx++;
        ifxi->Effects[j] = fx;
        ifxi->FxIds[j]   = i;

        if (ifxi->NumFx >= ifxi->Effects.size()) { // Moved down here
            return;
        }
    }
}

// 0x5980F0
bool InteriorManager_c::AreAnimsLoaded(int32 animBlock) {
    animBlock = [&] {
        switch (animBlock) {
        case 0:  return CAnimManager::GetAnimationBlockIndex("int_house");
        case 1:  return CAnimManager::GetAnimationBlockIndex("int_shop");
        case 2:  return CAnimManager::GetAnimationBlockIndex("int_office");
        default: return animBlock;
        }
    }();
    return CAnimManager::GetAnimationBlock((AssocGroupId)animBlock)->IsLoaded;
}

// 0x598010
void InteriorManager_c::Exit() {
    for (auto& g : m_InteriorGroupList) {
        g.Exit();
    }
    m_InteriorGroupList.RemoveAll();
    m_InteriorPool.RemoveAll();
    m_InteriorGroupPool.RemoveAll();
    g_furnitureMan.Exit();
}

// 0x598D80
size_t InteriorManager_c::GetVisibleEffects(InteriorEffectInfo_t* intFxInfos, uint32 maxNumIntFxInfo) {
    if (!maxNumIntFxInfo) {
        return 0;
    }

    int16 objCount{};
    std::array<CEntity*, 2048> entitiesInRange; // `CBuilding*`
    CWorld::FindObjectsInRange(TheCamera.GetPosition(), 100.f, false, &objCount, (int16)entitiesInRange.size(), entitiesInRange.data(), true, false, false, false, false);

    size_t numIntFxInfo{};
    for (auto& e : entitiesInRange | rng::views::take(objCount)) {
        if (!e->GetRwObject() || !e->IsInCurrentArea()) {
            continue;
        }

        const auto mi = e->GetModelInfo();
        for (size_t i = 0; i < mi->m_n2dfxCount; i++) {
            const auto ifx = notsa::dyn_cast_if_present<C2dEffectInterior>(mi->Get2dEffect((int32)i));

            // We only care about visible interior effects
            if (!ifx || !IsInteriorEffectVisible(ifx, e)) {
                continue;
            }

            // Check if effect was already stored
            if (numIntFxInfo != 0) {
                if (rng::none_of(intFxInfos, intFxInfos + numIntFxInfo, [&](const InteriorEffectInfo_t& ifxi) {
                    return ifxi.Entity == e && ifxi.Effects[0]->m_groupId == ifx->m_groupId;
                })) {
                    continue;
                }
            }

            // Store the effect
            intFxInfos[numIntFxInfo++] = {
                .Entity   = e,
                .NumFx    = 1,
                .Effects  = { ifx },
                .FxIds    = { (uint32)i },
                .IsCulled = false,
            };

            if (numIntFxInfo >= maxNumIntFxInfo) {
                goto end; // bugfix: Stop looping if can't store more
            }
        }
    }
end:

    for (size_t i = numIntFxInfo; i --> 0;) {
        AddSameGroupEffectInfos(&intFxInfos[i], maxNumIntFxInfo);
    }

    return numIntFxInfo;
}

// 0x598690
bool InteriorManager_c::IsInteriorEffectVisible(C2dEffectInterior* effect, CEntity* entity) {
    // The exe builds the world-space axis aligned box of the eight transformed corners of the entity's collision bounding box (same as InteriorGroup_c::ArePathsLoaded)
    // and tests the camera, player 0 and (if present) player 1 against it, inclusive; NaN coordinates are outside
    const auto& bb = CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundingBox();
    const auto& mn = bb.m_vecMin;
    const auto& mx = bb.m_vecMax;

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

    const auto IsInside = [&](const CVector& p) {
        return p.x >= lo.x && p.x <= hi.x
            && p.y >= lo.y && p.y <= hi.y
            && p.z >= lo.z && p.z <= hi.z;
    };
    const bool camInside = IsInside(TheCamera.GetPosition());
    const bool p0Inside  = IsInside(FindPlayerPed(0)->GetPosition());
    const auto p1        = FindPlayerPed(1);
    const bool p1Inside  = p1 && IsInside(p1->GetPosition());
    return p1Inside || p0Inside || camInside;
}

// 0x598620
Interior_c* InteriorManager_c::GetPedsInterior(const CPed* ped) {
    // Only the interiors of the group the ped is registered in (GetPedsInteriorGroup), not of every active group
    const auto grp = GetPedsInteriorGroup(ped);
    if (!grp) {
        return nullptr;
    }
    for (auto* const i : grp->m_interiors) {
        if (i && i->IsPtInside(ped->GetPosition())) {
            return i;
        }
    }
    return nullptr;
}

// 0x5984B0
void InteriorManager_c::ReturnInteriorToPool(Interior_c* interior) {
    m_InteriorPool.AddItem(interior);
}

// 0x5984A0
Interior_c* InteriorManager_c::GetInteriorFromPool() {
    return m_InteriorPool.RemoveHead();
}

// 0x5983D0
Interior_c* InteriorManager_c::GetVectorsInterior(const CVector& pt) { // TODO: Name is shit, should be `GetInteriorOfPoint` or something similar
    for (auto& g : m_InteriorGroupList) {
        for (auto* const i : g.m_interiors) { // All 8 slots (not only the first m_numInteriors), null slots skipped
            if (i && i->IsPtInside(pt)) {
                return i;
            }
        }
    }
    return nullptr;
}

// 0x598390
void InteriorManager_c::SetStealableObjectStolen(CEntity* entity, uint8 isStolen) {
    if (const auto idx = FindStealableObjectId(entity); idx != -1) {
        m_Objects[idx].wasStolen = isStolen;
    }
}

// 0x598360
int32 InteriorManager_c::FindStealableObjectId(CEntity* entity) const {
    for (auto&& [i, v] : rngv::enumerate(GetObjects())) {
        if (v.entity == entity) {
            return i;
        }
    }
    return -1;
}

// 0x5982F0
int32 InteriorManager_c::FindStealableObjectId(int32 interiorId, int32 modelId, CVector point) const {
    for (auto&& [i, v] : rngv::enumerate(GetObjects())) {
        if (v.interiorId == interiorId && v.modelId == modelId && v.pos == point) {
            return i;
        }
    }
    return -1;
}

// 0x5982B0
bool InteriorManager_c::HasInteriorHadStealDataSetup(Interior_c* interior) const {
    return m_InteriorCount && notsa::contains(GetInteriorIds(), interior->m_interiorId);
}

// 0x598280
int8 InteriorManager_c::IsGroupActive(int32 groupType) const {
    return rng::any_of(m_InteriorGroupList, [&](const InteriorGroup_c& g) {
        return g.m_groupType == groupType;
    });
}

// 0x598240
InteriorGroup_c* InteriorManager_c::GetPedsInteriorGroup(const CPed* ped) {
    for (auto& grp : m_InteriorGroupList) {
        if (notsa::contains(grp.GetPeds(), ped)) {
            return &grp;
        }
    }
    return nullptr;
}

// 0x598180
void InteriorManager_c::SetEntryExitPtr(CEntryExit* enex) {
    if (enex->GetLinkedOrThis()->m_nArea == eAreaCodes::AREA_CODE_NORMAL_WORLD) {
        return;
    }
    // The exe resets the stealable-object state only when the rectangle changed (any component unequal / NaN), but always stores the pointer and the rectangle
    if (enex->m_recEntrance != m_EnExRect) {
        m_ObjectCount   = 0;
        m_InteriorCount = 0;

        rng::fill(m_InteriorPedsAliveState, true);
    }

    m_EnEx     = enex;
    m_EnExRect = enex->m_recEntrance;
}

// 0x598090
bool InteriorManager_c::GetBoundingBox(CEntity* entity, CVector* pos) {
    for (auto& grp : m_InteriorGroupList) {
        for (const auto i : grp.GetInteriors()) {
            if (!i) {
                continue;
            }
            if (const auto fe = i->GetFurnitureEntity(entity)) {
                return i->GetBoundingBox(fe, pos);
            }
        }
    }
    return false;
}

// 0x598080
void InteriorManager_c::ActivatePeds(bool enable) {
    m_ArePedsEnabled = enable;
}
