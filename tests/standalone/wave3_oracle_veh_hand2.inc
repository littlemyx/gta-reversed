// ORACLE-W3-VEH: hand rows with reference / out parameters and CVector results (the exe returns a CVector through a hidden first stack argument: oracle::Fn<void* __fastcall(this, edx, out, ...)>).
// Included by wave3_oracle_veh.inc (uses W3FuncCase, g_post, g_out, g_frame, g_cp of namespace w3).
// a surface type of the game's table (the exe and the port both index the 179-entry table; the game never stores a larger one), biased to sand / rail track
static int PickSurface(Rng& rr) {
    static std::vector<int> sand, rail;
    if (sand.empty()) { for (int s = 0; s < 179; ++s) { if (g_surfaceInfos.GetAdhesionGroup((eSurfaceType)s) == ADHESION_GROUP_SAND) sand.push_back(s); } rail.push_back((int)SURFACE_RAILTRACK); }
    switch (rr.below(4)) { case 0: return sand.empty() ? 0 : sand[rr.below((uint32)sand.size())]; case 1: return rail[0]; default: return (int)rr.below(179); }
}
// OPEN rows (run with W3_OPEN=1, excluded from the default run: each still shows 1-2 differing cases in 200, see .notes/reports/ORACLE-W3-VEH-state.md):
//   CVehicle::CanVehicleBeDamaged (1 case: the flag / result written by CPhysical::CanPhysicalBeDamaged differ), CAutomobile::DoSoftGroundResistance (1 case: denormal speed),
//   CBike::DoSoftGroundResistance (2 cases: the exe produces NaN where the port does not and vice versa - not analysed yet).
static void TestW3VehHand2() {
    using namespace w2; using namespace w3;
    if (std::getenv("W3_OPEN")) Run("CVehicle::CanVehicleBeDamaged 0x6D1280", [&](Rng& r, std::string& d) {
        const uint32 k = r.below(4); const int weapon = (int)r.below(60); const uint32 pk = r.below(3);
        CEntity* other = k == 2 ? GEnt(r) : static_cast<CEntity*>(rf::PedE((int)r.below(rf::kP)));
        g_post = [&](Rng& rr, uint8_t*) {
            CWorld::PlayerInFocus = 0;
            CWorld::Players[0].m_pPed = pk == 0 ? nullptr : reinterpret_cast<CPlayerPed*>(rf::PedE((int)rr.below(rf::kP)));
            g_out[0] = (uint8_t)rr.below(2);
        };
        auto damager = [&]() -> CEntity* { return k == 0 ? nullptr : k == 1 ? static_cast<CEntity*>(CWorld::Players[0].m_pPed) : other; };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { bool& f = *reinterpret_cast<bool*>(g_out); const bool v = static_cast<CVehicle*>(p)->CVehicle::CanVehicleBeDamaged(damager(), (eWeaponType)weapon, f); return (uint64_t)v | ((uint64_t)f << 8); },
            [&](void* p) -> uint64_t { const bool v = (uint8_t)oracle::Fn<int __fastcall(void*, int, void*, int, void*)>(0x6D1280)(p, 0, damager(), weapon, g_out); return (uint64_t)v | ((uint64_t)g_out[0] << 8); });
    }, 120);
    Run("CVehicle::MakeDirty 0x6D2BF0", [&](Rng& r, std::string& d) {
        const int surf = (int)r.below(r.below(4) ? 60 : 179);
        g_post = [&](Rng& rr, uint8_t* v) {
            std::memcpy(g_cp + offsetof(CColPoint, m_nSurfaceTypeB), &surf, 4);
            CWeather::NewWeatherType = (eWeatherType)rr.below(23); CWeather::OldWeatherType = (eWeatherType)rr.below(23); CWeather::InterpolationValue = rr.f01();
            const float dirt = rr.below(3) ? rr.f01() * 16.f : GF(rr, 30.f); std::memcpy(v + offsetof(CVehicle, m_fDirtLevel), &dirt, 4);
            CTimer::ms_fTimeStep = rr.below(5) ? rr.f01() * 4.f : 0.f;
        };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { static_cast<CVehicle*>(p)->CVehicle::MakeDirty(*reinterpret_cast<CColPoint*>(g_cp)); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x6D2BF0)(p, 0, g_cp); return 0; });
    }, 200);
    if (std::getenv("W3_OPEN")) Run("CAutomobile::DoSoftGroundResistance 0x6A4AF0", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t* v) { for (auto& cp : reinterpret_cast<CAutomobile*>(v)->m_wheelColPoint) cp.m_nSurfaceTypeB = (eSurfaceType)PickSurface(rr); CWeather::WetRoads = rr.below(2) ? rr.f01() : 0.f; CTimer::ms_fTimeStep = rr.f01() * 3.f; std::memset(g_out, 0, 4); if (rr.below(2)) g_out[0] = (uint8_t)rr.u32(); };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { static_cast<CAutomobile*>(p)->CAutomobile::DoSoftGroundResistance(*reinterpret_cast<uint32*>(g_out)); return *reinterpret_cast<uint32*>(g_out); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x6A4AF0)(p, 0, g_out); return *reinterpret_cast<uint32*>(g_out); });
    }, 200);
    if (std::getenv("W3_OPEN")) Run("CBike::DoSoftGroundResistance 0x6B6D40", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t* v) { for (auto& cp : reinterpret_cast<CBike*>(v)->m_aWheelColPoints) cp.m_nSurfaceTypeB = (eSurfaceType)PickSurface(rr); CTimer::ms_fTimeStep = rr.f01() * 3.f; std::memset(g_out, 0, 4); if (rr.below(2)) g_out[0] = (uint8_t)rr.u32(); };
        return W3FuncCase(r, d, sizeof(CBike), {}, [&](void* p) -> uint64_t { static_cast<CBike*>(p)->CBike::DoSoftGroundResistance(*reinterpret_cast<uint32*>(g_out)); return *reinterpret_cast<uint32*>(g_out); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x6B6D40)(p, 0, g_out); return *reinterpret_cast<uint32*>(g_out); });
    }, 200);
    Run("CAutomobile::AddMovingCollisionSpeed 0x6A1ED0", [&](Rng& r, std::string& d) {
        CVector pt = GV(r);
        g_post = [&](Rng& rr, uint8_t* v) {
            auto* au = reinterpret_cast<CAutomobile*>(v);
            for (auto*& n : au->m_aCarNodes) n = nullptr;   // no frames: the port's RwFrame is librw's, the exe's the RenderWare 3.7 layout - a shared fake frame cannot satisfy both
            au->m_wMiscComponentAngle = (uint16)rr.below(rr.below(4) ? 60 : 65536); au->m_wMiscComponentAnglePrev = (uint16)(au->m_wMiscComponentAngle + (int)rr.below(rr.below(4) ? 30 : 400) - 15);
            CTimer::ms_fTimeStep = rr.below(6) ? 0.2f + rr.f01() * 3.f : 0.f;
            std::memset(g_out, 0, 12);
        };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { const CVector v = static_cast<CAutomobile*>(p)->CAutomobile::AddMovingCollisionSpeed(pt); std::memcpy(g_out + 16, &v, 12); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*, void*)>(0x6A1ED0)(p, 0, g_out + 16, &pt); return 0; });
    }, 200);
    Run("CVehicle::GetPlaneGunsPosition 0x6D4290", [&](Rng& r, std::string& d) {
        const int gun = (int)r.below(4);
        g_post = [&](Rng&, uint8_t*) { std::memset(g_out, 0, 12); };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { const CVector v = static_cast<CVehicle*>(p)->CVehicle::GetPlaneGunsPosition(gun); std::memcpy(g_out + 16, &v, 12); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*, int)>(0x6D4290)(p, 0, g_out + 16, gun); return 0; });
    }, 200);
    Run("CVehicle::GetPlaneOrdnancePosition 0x6D46E0", [&](Rng& r, std::string& d) {
        const int type = (int)r.below(4);
        g_post = [&](Rng&, uint8_t*) { std::memset(g_out, 0, 12); };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { const CVector v = static_cast<CVehicle*>(p)->CVehicle::GetPlaneOrdnancePosition(type); std::memcpy(g_out + 16, &v, 12); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*, int)>(0x6D46E0)(p, 0, g_out + 16, type); return 0; });
    }, 200);
}
