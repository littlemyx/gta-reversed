// ORACLE-W3-SCRIPTS rows B: CTheScripts (TheScripts.cpp), statics (cdecl). ROW / CC / Tw are defined in wave3_oracle_scripts.inc.
// Entities come from the reference world of logic_oracle_refs.inc (rf::: fake ped / vehicle / object pools at the exe's pool globals, buildings, loose entities); which one is picked is decided in the
// row body (indices), the tweak that runs after the fixture is built turns the indices into pointers.
namespace w3s {
constexpr uintptr_t kScriptsArray = 0xA8B430;   // CTheScripts' TU-static ScriptsArray (70 x CRunningScript, 0xE0 bytes)
static CRunningScript* GScr(int k) { return reinterpret_cast<CRunningScript*>(kScriptsArray + (size_t)k * 0xE0); }
template<class T> static void Garbage(Rng& r, T& obj) { FillGarbage(r, &obj, sizeof(obj)); }
static CBuilding* BldP(int i) { return reinterpret_cast<CBuilding*>(rf::BldE(i)); }
static CEntity* AnyEnt(int i) { return rf::All[(size_t)i]; }
// the script-space head chunks: chunk 0 'global vars' at offset 0, chunk 1 'used objects', chunk 2 'script file info' (the exe reads the 2nd and 3rd by position)
static void WriteHeadChunks(Rng& r, uint32_t usedObjects, uint32_t missions) {
    uint8_t* ss = &CTheScripts::ScriptSpace[0];
    auto Hdr = [&](uint32_t off, uint32_t next, uint8_t idx) { ss[off] = 2; ss[off + 1] = 0; ss[off + 2] = 1; std::memcpy(ss + off + 3, &next, 4); ss[off + 7] = idx; };
    const uint32_t o1 = 8 + (uint32_t)r.below(40) * 4, o2 = o1 + 12 + usedObjects * 24 + (uint32_t)r.below(5) * 4, o3 = o2 + 0x18 + missions * 4 + (uint32_t)r.below(5) * 4;
    Hdr(0, o1, 's'); Hdr(o1, o2, 0); Hdr(o2, o3, 1);
    std::memcpy(ss + o1 + 8, &usedObjects, 4);
    for (uint32_t i = 0; i < usedObjects; ++i) for (int k = 0; k < 24; ++k) ss[o1 + 12 + i * 24 + k] = r.below(4) ? (uint8_t)('A' + r.below(26)) : (uint8_t)r.u32();
    uint8_t* f = ss + o2 + 8;
    const uint32_t main = r.u32() & 0xFFFFF, large = r.u32() & 0xFFFFF, locals = r.u32() & 0xFFF;
    const uint16_t excl = (uint16_t)r.below(missions + 1);
    std::memcpy(f, &main, 4); std::memcpy(f + 4, &large, 4); std::memcpy(f + 8, &missions, 2); std::memcpy(f + 10, &excl, 2); std::memcpy(f + 12, &locals, 4);
    for (uint32_t i = 0; i < missions; ++i) { const uint32_t v = r.u32() & 0xFFFFFF; std::memcpy(f + 16 + i * 4, &v, 4); }
}
// idle / active script lists over the ScriptsArray: the nodes are a random permutation; `want` (>= 0) is forced into the idle list with probability 2/3
static void MakeScriptLists(Rng& rr, int idleMin, int want) {
    FillGarbage(rr, GScr(0), 70 * 0xE0);
    int p[70]; for (int i = 0; i < 70; ++i) p[i] = i;
    for (int i = 69; i > 0; --i) std::swap(p[i], p[rr.below(i + 1)]);
    const int nIdle = idleMin + rr.below(41 - idleMin), nAct = rr.below(70 - nIdle + 1);
    if (want >= 0 && want < 70 && nIdle > 0 && rr.below(3)) { for (int i = 0; i < 70; ++i) if (p[i] == want) { std::swap(p[i], p[rr.below(nIdle)]); break; } }
    for (int i = 0; i < 70; ++i) { GScr(i)->m_pNext = nullptr; GScr(i)->m_pPrev = nullptr; }
    auto Chain = [&](int from, int n) { for (int i = 0; i < n; ++i) { GScr(p[from + i])->m_pPrev = i ? GScr(p[from + i - 1]) : nullptr; GScr(p[from + i])->m_pNext = i + 1 < n ? GScr(p[from + i + 1]) : nullptr; } };
    Chain(0, nIdle); Chain(nIdle, nAct);
    CTheScripts::pIdleScripts = nIdle ? GScr(p[0]) : nullptr;
    CTheScripts::pActiveScripts = nAct ? GScr(p[nIdle]) : nullptr;
}
static void WriteRaw32(void* p, size_t off, uint32_t v) { std::memcpy(static_cast<uint8_t*>(p) + off, &v, 4); }
} // namespace w3s

static void TestW3ScriptsB() {
    using namespace w2; using namespace w3s;
    ROW("CTheScripts::InitialiseAllConnectLodObjects", 0x470960, 100) {
        Tw tw([](Rng& rr) { Garbage(rr, CTheScripts::ScriptConnectLodsObjects); });
        CC(CTheScripts::InitialiseAllConnectLodObjects(); return 0;, ExeC<void>(0x470960); return 0;);
    } ENDROW;
    ROW("CTheScripts::InitialiseConnectLodObjects", 0x470940, 200) {
        Tw tw([](Rng& rr) { Garbage(rr, CTheScripts::ScriptConnectLodsObjects); });
        const int i = r.below(10);
        CC(CTheScripts::InitialiseConnectLodObjects((uint16)i); return 0;, ExeC<void>(0x470940, i); return 0;);
    } ENDROW;
    ROW("CTheScripts::InitialiseSpecialAnimGroupsAttachedToCharModels", 0x474730, 100) {
        Tw tw([](Rng& rr) { Garbage(rr, CTheScripts::ScriptAttachedAnimGroups); });
        CC(CTheScripts::InitialiseSpecialAnimGroupsAttachedToCharModels(); return 0;, ExeC<void>(0x474730); return 0;);
    } ENDROW;
    ROW("CTheScripts::InitialiseSpecialAnimGroup", 0x474710, 200) {
        Tw tw([](Rng& rr) { Garbage(rr, CTheScripts::ScriptAttachedAnimGroups); });
        const int i = r.below(8);
        CC(CTheScripts::InitialiseSpecialAnimGroup((uint16)i); return 0;, ExeC<void>(0x474710, i); return 0;);
    } ENDROW;
    ROW("CTheScripts::ReadObjectNamesFromScript", 0x486720, 300) {
        const uint32_t n = r.below(4) ? r.below(40) : r.below(395);
        Tw tw([n](Rng& rr) { FillVars(rr); WriteHeadChunks(rr, n, rr.below(30)); Garbage(rr, CTheScripts::UsedObjectArray); CTheScripts::NumberOfUsedObjects = (uint16)rr.u32(); });
        CC(CTheScripts::ReadObjectNamesFromScript(); return 0;, ExeC<void>(0x486720); return 0;);
    } ENDROW;
    ROW("CTheScripts::ReadMultiScriptFileOffsetsFromScript", 0x4867C0, 300) {
        const uint32_t m = r.below(4) ? r.below(30) : r.below(201);
        Tw tw([m](Rng& rr) { FillVars(rr); WriteHeadChunks(rr, rr.below(20), m); Garbage(rr, CTheScripts::MultiScriptArray); });
        CC(CTheScripts::ReadMultiScriptFileOffsetsFromScript(); return 0;, ExeC<void>(0x4867C0); return 0;);
    } ENDROW;

    ROW("CTheScripts::AddToBuildingSwapArray", 0x481140, 3000) {
        const int bi = r.below(rf::kB);
        const int32_t oldM = r.below(20000), newM = r.below(4) == 0 ? -1 : (int32_t)r.below(20000);
        const int mode = r.below(3);   // 0: random array, 1: the building is in the array with new model != the call's, 2: ... with old model == the call's new model (cancels the swap)
        Tw tw([=](Rng& rr) {
            for (auto& s : CTheScripts::BuildingSwapArray) {
                if (rr.below(3)) s.Clear(); else { s.m_pCBuilding = BldP(rr.below(rf::kB)); s.m_nNewModelIndex = (int32)rr.below(20000); s.m_nOldModelIndex = rr.below(4) ? (int32)rr.below(20000) : -1; }
            }
            if (mode) {
                auto& s = CTheScripts::BuildingSwapArray[rr.below(25)];
                s.m_pCBuilding = BldP(bi); s.m_nNewModelIndex = (int32)rr.below(20000); s.m_nOldModelIndex = mode == 2 ? newM : (int32)rr.below(20000);
            }
        });
        CBuilding* b = BldP(bi);
        CC(CTheScripts::AddToBuildingSwapArray(b, oldM, newM); return 0;, ExeC<void>(0x481140, b, oldM, newM); return 0;);
    } ENDROW;
    ROW("CTheScripts::AddToInvisibilitySwapArray", 0x481200, 3000) {
        const int ei = r.below(rf::kB), mode = r.below(3);
        const int vis = r.below(2);
        Tw tw([=](Rng& rr) {
            for (auto& e : CTheScripts::InvisibilitySettingArray) e = rr.below(3) ? nullptr : rf::BldE(rr.below(rf::kB));
            if (mode == 1) CTheScripts::InvisibilitySettingArray[rr.below(20)] = rf::BldE(ei);
            if (mode == 2) for (auto& e : CTheScripts::InvisibilitySettingArray) if (!e) e = rf::BldE(rr.below(rf::kB));   // full
        });
        CEntity* e = rf::BldE(ei);
        CC(CTheScripts::AddToInvisibilitySwapArray(e, vis != 0); return 0;, ExeC<void>(0x481200, e, vis); return 0;);
    } ENDROW;
    ROW("CTheScripts::AddToListOfConnectedLodObjects", 0x470980, 3000) {
        const int i1 = r.below(rf::kO), i2 = r.below(rf::kO), mode = r.below(4);
        Tw tw([=](Rng& rr) {
            auto ref = [&](int i) { return (int32)GetObjectPool()->GetRef(reinterpret_cast<CObject*>(rf::ObjE(i))); };
            for (auto& l : CTheScripts::ScriptConnectLodsObjects) {
                if (rr.below(3) == 0) { l.a = -1; l.b = (int32)rr.u32(); } else { l.a = ref(rr.below(rf::kO)); l.b = ref(rr.below(rf::kO)); }
            }
            if (mode == 1) { auto& l = CTheScripts::ScriptConnectLodsObjects[rr.below(10)]; l.a = ref(i1); l.b = ref(i2); }
            if (mode == 2) for (auto& l : CTheScripts::ScriptConnectLodsObjects) if (l.a == -1) { l.a = ref(rr.below(rf::kO)); l.b = ref(rr.below(rf::kO)); }   // full table, no match unless random
            if (mode == 3) for (auto& l : CTheScripts::ScriptConnectLodsObjects) { l.a = -1; l.b = -1; }
        });
        CObject* o1 = reinterpret_cast<CObject*>(rf::ObjE(i1)); CObject* o2 = reinterpret_cast<CObject*>(rf::ObjE(i2));
        CC(CTheScripts::AddToListOfConnectedLodObjects(o1, o2); return 0;, ExeC<void>(0x470980, o1, o2); return 0;);
    } ENDROW;
    ROW("CTheScripts::AddToSwitchJumpTable", 0x470390, 600) {
        const uint16_t n = (uint16_t)r.below(75);   // 75 entries is the capacity: the 76th add writes over NumberOfEntriesInSwitchTable itself (hardened std::array asserts in the port)
        const int32_t val = (int32_t)r.u32(), lab = (int32_t)r.u32();
        Tw tw([n](Rng& rr) { Garbage(rr, CTheScripts::SwitchJumpTable); CTheScripts::NumberOfEntriesInSwitchTable = n; });
        CC(CTheScripts::AddToSwitchJumpTable(val, lab); return CTheScripts::NumberOfEntriesInSwitchTable;, ExeC<void>(0x470390, val, lab); return CTheScripts::NumberOfEntriesInSwitchTable;);
    } ENDROW;
    ROW("CTheScripts::AddToVehicleModelsBlockedByScript", 0x46B200, 1500) {
        const int mode = r.below(2);   // (no full-table mode: the exe writes slot 20 = SuppressedVehicleModels[0]; the port asserts / writes the same address in Release)
        const int32_t model = r.below(12) == 0 ? -1 : 400 + (int32_t)r.below(212);
        Tw tw([=](Rng& rr) {
            for (auto& m : CTheScripts::VehicleModelsBlockedByScript) m = rr.below(3) ? MODEL_INVALID : (eModelID)(400 + rr.below(212));
            if (mode == 1) CTheScripts::VehicleModelsBlockedByScript[rr.below(20)] = (eModelID)model;
            if (mode == 2) for (auto& m : CTheScripts::VehicleModelsBlockedByScript) if (m == MODEL_INVALID) m = (eModelID)(400 + rr.below(212));   // full: the exe writes slot 20 (SuppressedVehicleModels[0])
        });
        CC(CTheScripts::AddToVehicleModelsBlockedByScript((eModelID)model); return 0;, ExeC<void>(0x46B200, model); return 0;);
    } ENDROW;
    ROW("CTheScripts::AddToWaitingForScriptBrainArray", 0x46AB60, 3000) {
        const int ei = r.below((int)rf::All.size()), mode = r.below(4);
        const int16_t brain = (int16_t)(r.below(5) ? r.below(70) : (int)(int16_t)r.u32());
        Tw tw([=](Rng& rr) {
            for (auto& s : CTheScripts::EntitiesWaitingForScriptBrain) {   // raw stores: no reference registration for the fixture
                WriteRaw32(&s, 0, rr.below(2) ? 0 : P32(AnyEnt(rr.below((int)rf::All.size())))); WriteRaw32(&s, 4, rr.u32());
            }
            if (mode == 1) WriteRaw32(&CTheScripts::EntitiesWaitingForScriptBrain[rr.below(150)], 0, P32(AnyEnt(ei)));
            if (mode == 2) for (auto& s : CTheScripts::EntitiesWaitingForScriptBrain) if (!*reinterpret_cast<uint32_t*>(&s)) WriteRaw32(&s, 0, P32(AnyEnt(rr.below((int)rf::All.size()))));   // full
        });
        CEntity* e = AnyEnt(ei);
        CC(CTheScripts::AddToWaitingForScriptBrainArray(e, brain); return 0;, ExeC<void>(0x46AB60, e, (int)brain); return 0;);
    } ENDROW;
    ROW("CTheScripts::RemoveFromWaitingForScriptBrainArray", 0x46ABC0, 3000) {
        static const int16_t kIdx[] = { -1, 0, 1, 2, 5 };
        const int ei = r.below(3), bi = r.below(5);
        Tw tw([](Rng& rr) {
            for (auto& s : CTheScripts::EntitiesWaitingForScriptBrain) {
                const bool used = rr.below(3) == 0;
                WriteRaw32(&s, 0, used ? P32(AnyEnt(rr.below(3))) : 0); WriteRaw32(&s, 4, (uint32_t)(uint16_t)(used ? kIdx[rr.below(5)] : (int16_t)rr.u32()) | (rr.u32() << 16));
            }
        });
        CEntity* e = AnyEnt(ei);
        CC(CTheScripts::RemoveFromWaitingForScriptBrainArray(e, kIdx[bi]); return 0;, ExeC<void>(0x46ABC0, e, (int)kIdx[bi]); return 0;);
    } ENDROW;
    ROW("CTheScripts::CleanUpThisObject", 0x4866C0, 1000) {
        const bool nul = r.below(10) == 0;
        Tw tw([](Rng& rr) { Obj()->m_nObjectType = (uint8)(rr.below(2) ? (rr.below(2) ? 2 : 6) : rr.below(9)); CObject::nNoTempObjects = (uint16)rr.u32(); });
        CObject* o = nul ? nullptr : Obj();
        CC(CTheScripts::CleanUpThisObject(o); return 0;, ExeC<void>(0x4866C0, o); return 0;);
    } ENDROW;
    ROW("CTheScripts::ClearAllVehicleModelsBlockedByScript", 0x46A840, 100) {
        Tw tw([](Rng& rr) { Garbage(rr, CTheScripts::VehicleModelsBlockedByScript); });
        CC(CTheScripts::ClearAllVehicleModelsBlockedByScript(); return 0;, ExeC<void>(0x46A840); return 0;);
    } ENDROW;
    ROW("CTheScripts::ClearAllSuppressedCarModels", 0x46A7C0, 100) {
        Tw tw([](Rng& rr) { Garbage(rr, CTheScripts::SuppressedVehicleModels); });
        CC(CTheScripts::ClearAllSuppressedCarModels(); return 0;, ExeC<void>(0x46A7C0); return 0;);
    } ENDROW;
    ROW("CTheScripts::WipeLocalVariableMemoryForMissionScript", 0x464BB0, 100) {
        Tw tw([](Rng& rr) { FillVars(rr); });
        CC(CTheScripts::WipeLocalVariableMemoryForMissionScript(); return 0;, ExeC<void>(0x464BB0); return 0;);
    } ENDROW;
    ROW("CTheScripts::ReinitialiseSwitchStatementData", 0x470370, 100) {
        Tw tw([](Rng& rr) { CTheScripts::NumberOfEntriesStillToReadForSwitch = (uint16)rr.u32(); CTheScripts::ValueToCheckInSwitchStatement = (int32)rr.u32(); CTheScripts::SwitchDefaultExists = rr.below(2); CTheScripts::SwitchDefaultAddress = (int32)rr.u32(); CTheScripts::NumberOfEntriesInSwitchTable = (uint16)rr.u32(); });
        CC(CTheScripts::ReinitialiseSwitchStatementData(); return 0;, ExeC<void>(0x470370); return 0;);
    } ENDROW;
    ROW("CTheScripts::GetScriptIndexFromPointer", 0x464D20, 200) {
        const int k = r.below(70);
        CRunningScript* p = GScr(k);
        CC(return (uint32_t)CTheScripts::GetScriptIndexFromPointer(p);, return ExeC<uint32_t>(0x464D20, p););
    } ENDROW;
    ROW("CTheScripts::GetUniqueScriptThingIndex", 0x4810C0, 400) {
        const int grp = r.below(8);
        const int ty = r.below(2) ? 8 : (int)r.below(13);
        Tw tw([](Rng& rr) { Garbage(rr, CPedGroups::ScriptReferenceIndex); });
        CC(return (uint32_t)CTheScripts::GetUniqueScriptThingIndex(grp, (eScriptThingType)ty);, return ExeC<uint32_t>(0x4810C0, grp, ty););
    } ENDROW;
    ROW("CTheScripts::StartNewScript-last-idle", 0x464C20, 1500) {
        Tw tw([](Rng& rr) { MakeScriptLists(rr, 1, -1); });
        uint8_t* ip = reinterpret_cast<uint8_t*>(r.below(2) ? (r.u32() | 1) : (uint32_t)(uintptr_t)&CTheScripts::ScriptSpace[r.below(200000)]);
        CC(return P32(CTheScripts::StartNewScript(ip));, return ExeC<uint32_t>(0x464C20, ip););
    } ENDROW;
    ROW("CTheScripts::StartNewScript-indexed", 0x464C90, 2500) {
        const int idx = r.below(75);
        Tw tw([idx](Rng& rr) { MakeScriptLists(rr, rr.below(8) == 0 ? 0 : 1, idx); });
        uint8_t* ip = r.below(8) == 0 ? nullptr : reinterpret_cast<uint8_t*>(r.below(2) ? (r.u32() | 1) : (uint32_t)(uintptr_t)&CTheScripts::ScriptSpace[r.below(200000)]);
        CC(return P32(CTheScripts::StartNewScript(ip, (uint16)idx));, return ExeC<uint32_t>(0x464C90, ip, idx););
    } ENDROW;
    ROW("CTheScripts::StartTestScript", 0x464D40, 200) {
        Tw tw([](Rng& rr) { MakeScriptLists(rr, 1, -1); });
        CC(CTheScripts::StartTestScript(); return P32(CTheScripts::pActiveScripts);, ExeC<void>(0x464D40); return P32(CTheScripts::pActiveScripts););
    } ENDROW;
    ROW("CTheScripts::PrintListSizes", 0x4646D0, 200) {
        Tw tw([](Rng& rr) { MakeScriptLists(rr, 0, -1); });
        CC(CTheScripts::PrintListSizes(); return 0;, ExeC<void>(0x4646D0); return 0;);
    } ENDROW;
    ROW("CTheScripts::IsPlayerOnAMission", 0x464D50, 600) {
        Tw tw([](Rng& rr) {
            FillVars(rr);
            CTheScripts::OnAMissionFlag = rr.below(4) == 0 ? 0 : (int32)(rr.below(60000) & ~3u);
            if (CTheScripts::OnAMissionFlag && rr.below(2)) std::memcpy(&CTheScripts::ScriptSpace[CTheScripts::OnAMissionFlag], "\x01\0\0\0", 4);
        });
        CC(return B8(CTheScripts::IsPlayerOnAMission());, return B8(ExeC<bool>(0x464D50)););
    } ENDROW;
    ROW("CTheScripts::HasCarModelBeenSuppressed", 0x46A810, 1000) {
        const int mode = r.below(3);
        const int32_t model = r.below(10) == 0 ? -1 : 400 + (int32_t)r.below(212);
        Tw tw([=](Rng& rr) {
            for (auto& m : CTheScripts::SuppressedVehicleModels) m = rr.below(3) ? MODEL_INVALID : (eModelID)(400 + rr.below(212));
            if (mode == 1) CTheScripts::SuppressedVehicleModels[rr.below(40)] = (eModelID)model;
        });
        CC(return B8(CTheScripts::HasCarModelBeenSuppressed((eModelID)model));, return B8(ExeC<bool>(0x46A810, model)););
    } ENDROW;
    ROW("CTheScripts::HasVehicleModelBeenBlockedByScript", 0x46A890, 1000) {
        const int mode = r.below(3);
        const int32_t model = r.below(10) == 0 ? -1 : 400 + (int32_t)r.below(212);
        Tw tw([=](Rng& rr) {
            for (auto& m : CTheScripts::VehicleModelsBlockedByScript) m = rr.below(3) ? MODEL_INVALID : (eModelID)(400 + rr.below(212));
            if (mode == 1) CTheScripts::VehicleModelsBlockedByScript[rr.below(20)] = (eModelID)model;
        });
        CC(return B8(CTheScripts::HasVehicleModelBeenBlockedByScript((eModelID)model));, return B8(ExeC<bool>(0x46A890, model)););
    } ENDROW;
    ROW("CTheScripts::UndoEntityInvisibilitySettings", 0x4812D0, 300) {
        Tw tw([](Rng& rr) { for (auto& e : CTheScripts::InvisibilitySettingArray) e = rr.below(2) ? nullptr : Ent(rr.below(4)); });
        CC(CTheScripts::UndoEntityInvisibilitySettings(); return 0;, ExeC<void>(0x4812D0); return 0;);
    } ENDROW;
    ROW("CTheScripts::IsVehicleStopped", 0x4861F0, 1500) {
        Tw tw([](Rng& rr) { Veh()->m_fMovingSpeed = rr.below(3) ? GenF(rr, 0.1f, S_ZERO | S_TINY) : GenF(rr, 0.1f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF); });
        CC(return B8(CTheScripts::IsVehicleStopped(Veh()));, return B8(ExeC<bool>(0x4861F0, Veh())););
    } ENDROW;
    ROW("CTheScripts::IsPedStopped", 0x486110, 4000) {
        Tw tw([](Rng& rr) {
            CPed* p = Ped();
            p->m_nPedType = (ePedType)(2 + rr.below(20));          // never PLAYER1 / PLAYER2: the player branch queries animation associations of the ped's clump (none in this fixture)
            p->bInVehicle = rr.below(3) == 0;
            p->m_pVehicle = rr.below(8) ? Veh() : nullptr;
            p->bIsLanding = rr.below(6) == 0; p->bIsInTheAir = rr.below(6) == 0; p->bIsStanding = rr.below(5) != 0;
            p->m_nMoveState = (eMoveState)(rr.below(3) ? (int)rr.below(8) : (int)rr.below(70));   // 0x534: IsPedStandingInPlace = state < 4 (the port switches over the four values; negative garbage is no game state)
            const auto F = [&]() { return rr.below(2) ? 0.f : rr.below(3) ? GF(rr, 3.f) : GenF(rr, 3.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF); };
            p->m_vecAnimMovingShiftLocal = CVector2D(F(), F());
            Veh()->m_fMovingSpeed = rr.below(3) ? GenF(rr, 0.1f, S_ZERO | S_TINY) : GenF(rr, 0.1f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
        });
        CC(return B8(CTheScripts::IsPedStopped(Ped()));, return B8(ExeC<bool>(0x486110, Ped())););
    } ENDROW;
    ROW("CTheScripts::CheckStreamedScriptVersion", 0x464FF0, 50) {
        CC(return B8(CTheScripts::CheckStreamedScriptVersion(nullptr, "x"));, return B8(ExeC<bool>(0x464FF0, nullptr, "x")););
    } ENDROW;
    ROW("CTheScripts::UseSwitchJumpTable", 0x4703C0, 4000) {
        const int n = r.below(8) ? 1 + (int)r.below(12) : 1 + (int)r.below(74);
        const int mode = r.below(5);
        Tw tw([=](Rng& rr) {
            int32_t v = (int32_t)rr.below(200) - 100;
            for (int i = 0; i < n; ++i) { v += 1 + (int32_t)rr.below(rr.below(4) ? 3 : 1000); CTheScripts::SwitchJumpTable[i].m_nSwitchValue = v; CTheScripts::SwitchJumpTable[i].m_nSwitchLabelAddress = (int32_t)rr.u32(); }
            CTheScripts::NumberOfEntriesInSwitchTable = (uint16)n;
            CTheScripts::NumberOfEntriesStillToReadForSwitch = (uint16)rr.u32();
            CTheScripts::SwitchDefaultExists = rr.below(2); CTheScripts::SwitchDefaultAddress = (int32)rr.u32();
            const auto& e = CTheScripts::SwitchJumpTable[rr.below(n)];
            CTheScripts::ValueToCheckInSwitchStatement = mode <= 1 ? e.m_nSwitchValue : mode == 2 ? e.m_nSwitchValue + 1 : mode == 3 ? (int32_t)rr.u32() : CTheScripts::SwitchJumpTable[0].m_nSwitchValue - 1;
        });
        CC(CTheScripts::UseSwitchJumpTable(*reinterpret_cast<int32*>(Out())); return 0;, ExeC<void>(0x4703C0, Out()); return 0;);
    } ENDROW;
}
