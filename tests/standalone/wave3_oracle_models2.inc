// ORACLE-W3-MODELS part 2: CVehicleModelInfo rows (included by wave3_oracle_models.inc). See there for the fixture description.
namespace w3m {
static void SetWeather(Rng& r) { const uint32_t a = r.below(20), b = r.below(20); std::memcpy(&CWeather::OldWeatherType, &a, sizeof CWeather::OldWeatherType); std::memcpy(&CWeather::NewWeatherType, &b, sizeof CWeather::NewWeatherType); }
static void BuildAtomicPlugin(Rng& r) {   // atomic with the visibility plugin at a fixed offset (flags are the u16 at +2 of the plugin data)
    BuildMi(r);
    StaticRef<int32_t>(0x8D608C) = 0x50;   // CVisibilityPlugins::ms_atomicPluginOffset
}
static void BuildColour(Rng& r) {   // ChooseVehicleColour: player in focus (maybe in a vehicle with this model), cheats
    BuildMi(r);
    auto* vi = reinterpret_cast<CVehicleModelInfo*>(P(kMi));
    vi->m_nNumColorVariations = (uint8)(r.below(5) == 0 ? 0 : 1 + r.below(8));
    vi->m_nLastColorVariation = (uint8)(r.below(2) ? r.below(8) : r.below(256));
    for (int i = 0; i < 8; ++i) { vi->m_anPrimaryColors[i] = (uint8)r.below(r.below(4) ? 3 : 256); vi->m_anSecondaryColors[i] = (uint8)r.below(r.below(4) ? 3 : 256); vi->m_anTertiaryColors[i] = (uint8)r.below(3); vi->m_anQuaternaryColors[i] = (uint8)r.below(3); }
    CCheat::m_aCheatsActive[CHEAT_BLACK_TRAFFIC] = r.below(8) == 0;
    CCheat::m_aCheatsActive[CHEAT_PINK_TRAFFIC] = r.below(8) == 0;
    // the player: in focus 0; a vehicle fixture whose model index leads back to this info and whose colours match one of the variations
    CWorld::PlayerInFocus = 0;
    auto& pl = CWorld::Players[0];
    const int mode = r.below(4);   // 0 no ped, 1 ped on foot, 2 in a vehicle of another model, 3 in a vehicle of this model
    pl.m_pPed = mode == 0 ? nullptr : reinterpret_cast<CPlayerPed*>(P(kPed));
    if (mode) {
        auto* ped = reinterpret_cast<CPlayerPed*>(P(kPed));
        ped->bInVehicle = mode >= 2;
        ped->m_pVehicle = reinterpret_cast<CVehicle*>(P(kVeh));
        auto* veh = reinterpret_cast<CVehicle*>(P(kVeh));
        const int mdl = 400 + (int)r.below(100);
        veh->m_nModelIndex = (uint16)mdl;
        reinterpret_cast<void**>(kModelPtrs)[mdl] = mode == 3 ? P(kMi) : P(kMi2);
        const int v = r.below(8);
        veh->m_nPrimaryColor = vi->m_anPrimaryColors[v]; veh->m_nSecondaryColor = vi->m_anSecondaryColors[v]; veh->m_nTertiaryColor = vi->m_anTertiaryColors[v]; veh->m_nQuaternaryColor = vi->m_anQuaternaryColors[v];
        if (r.below(3) == 0) veh->m_nPrimaryColor = (uint8)r.below(256);
    }
}
} // namespace w3m

static void TestW3ModelsVeh() {
    using namespace w2; using namespace w3m;
    auto VM = [] { return reinterpret_cast<CVehicleModelInfo*>(P(kMi)); };
    auto LU = [] { return reinterpret_cast<CVehicleModelInfo::CLinkedUpgradeList*>(P(kLU)); };
    // ---- virtual / simple CVehicleModelInfo members
    Run("CVehicleModelInfo::GetModelType 0x4C7650", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return (uint8_t)VM()->CVehicleModelInfo::GetModelType(); }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4C7650, P(kMi)); });
    }, 20);
    Run("CVehicleModelInfo::Init 0x4C7630", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { VM()->CVehicleModelInfo::Init(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C7630, P(kMi)); return 0; });
    }, 100);
    Run("CVehicleModelInfo::GetAnimFileIndex 0x4C7660", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)VM()->CVehicleModelInfo::GetAnimFileIndex(); }, [&]() -> uint64_t { return ExeT<uint32_t>(0x4C7660, P(kMi)); });
    }, 40);
    Run("CVehicleModelInfo::GetNumDoors 0x4C73C0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)VM()->GetNumDoors(); }, [&]() -> uint64_t { return ExeT<uint32_t>(0x4C73C0, P(kMi)); });
    }, 40);
    Run("CVehicleModelInfo::GetCustomCarPlateText 0x4C8970", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng& rr) { if (rr.below(3) == 0) At<uint8_t>(kMi + offsetof(CVehicleModelInfo, m_szPlateText)) = 0; };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return U(VM()->GetCustomCarPlateText()); }, [&]() -> uint64_t { return ExeT<uint32_t>(0x4C8970, P(kMi)); });
        g_tweak = nullptr;
        return ok;
    }, 100);
    Run("CVehicleModelInfo::IsUpgradeAvailable 0x4C8200", [&](Rng& r, std::string& d) {
        const int up = r.below(18);
        g_tweak = [&](Rng& rr) { for (int i = 0; i < 18; ++i) At<int32_t>(kVS + 0xB4 + i * 0x20 + 0x1C) = rr.below(3) == 0 ? -1 - (int)rr.below(2) : (int)rr.below(30); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return VM()->IsUpgradeAvailable((eVehicleUpgradePosn)up); }, [&]() -> uint64_t { return ExeT<uint8_t>(0x4C8200, P(kMi), up); });
        g_tweak = nullptr;
        return ok;
    }, 200);
    Run("CVehicleModelInfo::GetNumRemaps 0x4C86B0", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng& rr) { const int n = rr.below(5); for (int i = 0; i < 4; ++i) At<int16_t>(kMi + offsetof(CVehicleModelInfo, m_anRemapTxds) + 2 * i) = i < n ? (int16_t)rr.below(300) : (rr.below(3) ? (int16_t)-1 : (int16_t)rr.below(300)); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return (uint32_t)VM()->GetNumRemaps(); }, [&]() -> uint64_t { return ExeT<uint32_t>(0x4C86B0, P(kMi)); });
        g_tweak = nullptr;
        return ok;
    }, 200);
    Run("CVehicleModelInfo::AddRemap 0x4C86D0", [&](Rng& r, std::string& d) {
        const int txd = r.below(4) ? (int)r.below(300) : (int)r.u32();
        g_tweak = [&](Rng& rr) { const int n = rr.below(4); for (int i = 0; i < 4; ++i) At<int16_t>(kMi + offsetof(CVehicleModelInfo, m_anRemapTxds) + 2 * i) = i < n ? (int16_t)rr.below(300) : (int16_t)-1; };   // (a 5th remap writes past the array: the port asserts, the exe corrupts the next field)
        const bool ok = MCase(r, d, [&]() -> uint64_t { VM()->AddRemap(txd); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C86D0, P(kMi), txd); return 0; });
        g_tweak = nullptr;
        return ok;
    }, 200);
    Run("CVehicleModelInfo::DeleteVehicleColourTextures 0x4C8770", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { VM()->DeleteVehicleColourTextures(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C8770, P(kMi)); return 0; });
    }, 20);
    Run("CVehicleModelInfo::SetRenderPipelinesCB 0x4C88F4", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::SetRenderPipelinesCB(reinterpret_cast<RpAtomic*>(P(kAt)), P(kOut))) != 0; }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C88F4, P(kAt), P(kOut)) != 0; });   // the exe body is a bare `ret` (eax = whatever the caller left: non-zero = keep iterating); only "non-zero" is comparable
    }, 20);
    // ---- CLinkedUpgradeList
    Run("CLinkedUpgradeList::AddUpgradeLink 0x4C74B0", [&](Rng& r, std::string& d) {
        const int16_t a = r.below(4) ? (int16_t)r.below(30) : (int16_t)r.u32(), b = r.below(4) ? (int16_t)r.below(30) : (int16_t)r.u32();
        g_tweak = [&](Rng& rr) { At<uint32_t>(kLU + 0x78) = rr.below(30); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { LU()->AddUpgradeLink(a, b); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C74B0, P(kLU), (int)a, (int)b); return 0; });
        g_tweak = nullptr;
        return ok;
    }, 300);
    Run("CLinkedUpgradeList::FindOtherUpgrade 0x4C74D0", [&](Rng& r, std::string& d) {
        const int16_t up = (int16_t)r.below(8);
        g_tweak = [&](Rng& rr) { At<uint32_t>(kLU + 0x78) = rr.below(8) == 0 ? 0 : rr.below(31); for (int i = 0; i < 60; ++i) At<int16_t>(kLU + 2 * i) = (int16_t)(rr.below(5) ? rr.below(8) : rr.below(30)); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return (uint16_t)LU()->FindOtherUpgrade(up); }, [&]() -> uint64_t { return ExeT<uint16_t>(0x4C74D0, P(kLU), (int)up); });
        g_tweak = nullptr;
        return ok;
    }, 400);
    // ---- B: statics of the vehicle model info
    Run("CVehicleModelInfo::SetVehicleColour 0x4C84B0", [&](Rng& r, std::string& d) {
        const uint8_t a = (uint8_t)r.below(256), b = (uint8_t)r.below(256), c = (uint8_t)r.below(256), e = (uint8_t)r.below(256);
        return MCase(r, d, [&]() -> uint64_t { VM()->SetVehicleColour(a, b, c, e); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C84B0, P(kMi), (int)a, (int)b, (int)c, (int)e); return 0; });
    }, 100);
    Run("CVehicleModelInfo::SetupLightFlags 0x4C8C90", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { VM()->SetupLightFlags(reinterpret_cast<CVehicle*>(P(kVeh))); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4C8C90, P(kVeh)); return 0; });
    }, 200);
    Run("CVehicleModelInfo::AddWheelUpgrade 0x4C8700", [&](Rng& r, std::string& d) {
        const int ws = r.below(4); const int mdl = r.below(4) ? (int)(400 + r.below(200)) : (int)(int16_t)r.u32();
        return MCase(r, d, [&]() -> uint64_t { CVehicleModelInfo::AddWheelUpgrade(ws, mdl); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4C8700, ws, mdl); return 0; }, &BuildVehicleStatics);
    }, 200);
    Run("CVehicleModelInfo::GetNumWheelUpgrades 0x4C8740", [&](Rng& r, std::string& d) {
        const int ws = r.below(4);
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)CVehicleModelInfo::GetNumWheelUpgrades(ws); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C8740, ws); }, &BuildVehicleStatics);
    }, 100);
    Run("CVehicleModelInfo::GetWheelUpgrade 0x4C8750", [&](Rng& r, std::string& d) {
        const int ws = r.below(4), n = r.below(15);
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)CVehicleModelInfo::GetWheelUpgrade(ws, n); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C8750, ws, n); }, &BuildVehicleStatics);
    }, 200);
    Run("CVehicleModelInfo::GetMaximumNumberOfPassengersFromNumberOfDoors 0x4C89B0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)CVehicleModelInfo::GetMaximumNumberOfPassengersFromNumberOfDoors(g_modelId); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C89B0, g_modelId); }, &BuildPassengers);
    }, 600);
    Run("CVehicleModelInfo::ChooseVehicleColour 0x4C8500", [&](Rng& r, std::string& d) {
        const int shift = r.below(3) ? (int)r.below(12) : (int)r.below(300);
        uint8_t* o = P(kOut);
        return MCase(r, d, [&]() -> uint64_t { VM()->ChooseVehicleColour(o[0], o[1], o[2], o[3], shift); return 0; },
                           [&]() -> uint64_t { ExeT<void>(0x4C8500, P(kMi), o, o + 1, o + 2, o + 3, shift); return 0; }, &BuildColour);
    }, 800);
    // ---- components rules (weather globals)
    Run("::IsValidCompRule 0x4C7E10", [&](Rng& r, std::string& d) {
        const int rule = r.below(16);
        g_tweak = [&](Rng& rr) { SetWeather(rr); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return W3M_IsValidCompRule(rule); }, [&]() -> uint64_t { return ExeC<uint8_t>(0x4C7E10, rule); });
        g_tweak = nullptr;
        return ok;
    }, 400);
    Run("::CountCompsInRule 0x4C7F80", [&](Rng& r, std::string& d) {
        const int32_t comps = r.below(3) ? (int32_t)(r.u32() & 0xFFFF) : (int32_t)(r.u32() & 0x7FFFFFFF);   // callers pass 12-bit rules; a negative value never reaches 0 in the exe's loop (sar), the port's divide does
        return MCase(r, d, [&]() -> uint64_t { return (uint32_t)W3M_CountCompsInRule(comps); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7F80, comps); });
    }, 400);
    Run("::GetListOfComponentsNotUsedByRules 0x4C7E50", [&](Rng& r, std::string& d) {
        uint32_t comps = 0; for (int i = 0; i < 8; ++i) comps |= (uint32_t)(r.below(3) == 0 ? 0xF : r.below(6)) << (4 * i);   // component nibbles: 0..5 or 0xF (a larger index writes past the 6-entry list in both implementations)
        for (int i : { 3, 7 }) comps = (comps & ~(0xFu << (4 * i))) | ((uint32_t)r.below(4) << (4 * i));   // rule nibbles
        const int n = r.below(7);
        g_tweak = [&](Rng& rr) { SetWeather(rr); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return (uint32_t)W3M_GetListOfComponentsNotUsedByRules(comps, n, &At<int32_t>(kOut)); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7E50, comps, n, &At<int32_t>(kOut)); });
        g_tweak = nullptr;
        return ok;
    }, 600);
    // ---- material / atomic callbacks on raw RW-layout fixtures (colour alpha at +7, atomic flags byte at +2, visibility plugin at +0x50)
    Run("CVehicleModelInfo::HasAlphaMaterialCB 0x4C77C0", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng& rr) { if (rr.below(2)) At<uint8_t>(kMat + 7) = 255; };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::HasAlphaMaterialCB(reinterpret_cast<RpMaterial*>(P(kMat)), P(kOut))); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C77C0, P(kMat), P(kOut)); });
        g_tweak = nullptr;
        return ok;
    }, 200);
    Run("::RemoveWindowAlphaCB 0x4C83B0", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng& rr) { if (rr.below(2)) At<uint8_t>(kMat + 7) = 255; At<void*>(kOut) = P(kEntries); };
        const bool ok = MCase(r, d, [&]() -> uint64_t { return U(W3M_RemoveWindowAlphaCB(reinterpret_cast<RpMaterial*>(P(kMat)), P(kOut))); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C83B0, P(kMat), P(kOut)); });
        g_tweak = nullptr;
        return ok;
    }, 200);
    Run("CVehicleModelInfo::ResetEditableMaterials 0x4C8460", [&](Rng& r, std::string& d) {
        g_tweak = [&](Rng& rr) {   // gRestoreEntries (0xB4DBE8): (address, value) pairs ending with a null address; the addresses point into the fixture
            const int n = rr.below(12);
            for (int i = 0; i < 13; ++i) {
                StaticRef<uint32_t>(0xB4DBE8 + 8 * i) = i < n ? (uint32_t)(uintptr_t)P(kRest + 4 * rr.below(64)) : 0;
                StaticRef<uint32_t>(0xB4DBE8 + 8 * i + 4) = rr.u32();
            }
        };
        const bool ok = MCase(r, d, [&]() -> uint64_t { CVehicleModelInfo::ResetEditableMaterials(reinterpret_cast<RpClump*>(P(kAt))); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4C8460, P(kAt)); return 0; });
        g_tweak = nullptr;
        return ok;
    }, 200);
    Run("CVehicleModelInfo::SetAtomicFlagCB 0x4C7B90", [&](Rng& r, std::string& d) {
        void* flag = reinterpret_cast<void*>((uintptr_t)(r.below(2) ? (1u << r.below(16)) : r.u32()));
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::SetAtomicFlagCB(reinterpret_cast<RwObject*>(P(kAt)), flag)); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7B90, P(kAt), flag); }, &BuildAtomicPlugin);
    }, 300);
    Run("CVehicleModelInfo::ClearAtomicFlagCB 0x4C7BB0", [&](Rng& r, std::string& d) {
        void* flag = reinterpret_cast<void*>((uintptr_t)(r.below(2) ? (1u << r.below(16)) : r.u32()));
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::ClearAtomicFlagCB(reinterpret_cast<RwObject*>(P(kAt)), flag)); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7BB0, P(kAt), flag); }, &BuildAtomicPlugin);
    }, 300);
    Run("::GetOkAndDamagedAtomicCB 0x4C7BD0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(W3M_GetOkAndDamagedAtomicCB(reinterpret_cast<RwObject*>(P(kAt)), P(kOut))); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7BD0, P(kAt), P(kOut)); }, &BuildAtomicPlugin);
    }, 300);
    Run("CVehicleModelInfo::HideAllComponentsAtomicCB 0x4C7790", [&](Rng& r, std::string& d) {
        void* mask = reinterpret_cast<void*>((uintptr_t)(r.below(2) ? (1u << r.below(16)) : r.u32()));
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::HideAllComponentsAtomicCB(reinterpret_cast<RpAtomic*>(P(kAt)), mask)); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7790, P(kAt), mask); }, &BuildAtomicPlugin);
    }, 300);
}
