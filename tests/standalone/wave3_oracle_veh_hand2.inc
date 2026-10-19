// ORACLE-W3-VEH: hand rows with reference / out parameters and CVector results (the exe returns a CVector through a hidden first stack argument: oracle::Fn<void* __fastcall(this, edx, out, ...)>).
// Included by wave3_oracle_veh.inc (uses W3FuncCase, g_post, g_out, g_frame, g_cp of namespace w3).
// a surface type of the game's table (the exe and the port both index the 179-entry table; the game never stores a larger one), biased to sand / rail track
static int PickSurface(Rng& rr) {
    static std::vector<int> sand, rail;
    if (sand.empty()) { for (int s = 0; s < 179; ++s) { if (g_surfaceInfos.GetAdhesionGroup((eSurfaceType)s) == ADHESION_GROUP_SAND) sand.push_back(s); } rail.push_back((int)SURFACE_RAILTRACK); }
    switch (rr.below(4)) { case 0: return sand.empty() ? 0 : sand[rr.below((uint32)sand.size())]; case 1: return rail[0]; default: return (int)rr.below(179); }
}
// W3-VEH-OPEN: three quarters of the soft-ground cases use game-range motion (speed ~ m/frame, mass 200..5000, wheels at 0..1) instead of garbage floats: garbage saturates (inf / NaN / denormal) and hides one-ulp rounding differences
static void RealMotion(Rng& rr, uint8_t* v, float* ratios, int n) {
    auto* p = reinterpret_cast<CPhysical*>(v);
    if (rr.below(4) == 0) {   // edge values: the compare / spill behaviour of the x87 (NaN, inf, float limits, denormals, the 0.3 threshold)
        static const float e[] = { 0.f, -0.f, 1e-20f, -1e-20f, 1e-40f, -1e-40f, 1e20f, -1e20f, 3e38f, -3e38f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 0.3f, 0.29f, 0.31f };
        auto pick = [&] { return e[rr.below((uint32)std::size(e))]; };
        const bool mix = rr.below(2);
        p->m_vecMoveSpeed = CVector(pick(), mix ? (rr.f01() - 0.5f) * 2.f : pick(), pick());
        if (rr.below(2)) p->m_fMass = std::fabs(pick()) + (rr.below(2) ? 1.f : 0.f);
        p->physicalFlags.bInfiniteMass = false; p->physicalFlags.bDisableMoveForce = false;
        for (int i = 0; i < n; ++i) ratios[i] = rr.below(3) ? rr.f01() : 1.f;
        return;
    }
    if (rr.below(4) == 0) return;
    const float sc = rr.below(3) == 0 ? 0.2f : rr.below(2) ? 2.f : 30.f;
    p->m_vecMoveSpeed = CVector((rr.f01() - 0.5f) * sc, (rr.f01() - 0.5f) * sc, rr.below(3) ? (rr.f01() - 0.5f) * sc * 0.2f : 0.f);
    p->m_fMass = 200.f + rr.f01() * 4800.f;
    p->physicalFlags.bInfiniteMass = false; p->physicalFlags.bDisableMoveForce = false; p->physicalFlags.bDisableZ = rr.below(4) == 0;
    for (int i = 0; i < n; ++i) ratios[i] = rr.below(3) ? rr.f01() : 1.f;
}
// W3-VEH-OPEN: the three former OPEN rows (CanVehicleBeDamaged, both DoSoftGroundResistance) are fixed in the port and un-gated, see .notes/reports/W3-VEH-OPEN.md.
static void TestW3VehHand2() {
    using namespace w2; using namespace w3;
    Run("CVehicle::CanVehicleBeDamaged 0x6D1280", [&](Rng& r, std::string& d) {
        const uint32 k = r.below(4); const int weapon = (int)(r.below(3) == 0 ? (r.below(2) ? 16 + r.below(6) : 35 + r.below(6)) : r.below(8) ? r.below(56) : r.below(70)); const uint32 pk = r.below(3);
        CEntity* other = k == 2 ? GEnt(r) : static_cast<CEntity*>(rf::PedE((int)r.below(rf::kP)));
        g_post = [&](Rng& rr, uint8_t* v) {
            // status PLAYER (0) in half of the cases, health at the 250 edge / NaN (the `health < 250` compare is unordered-sensitive)
            if (rr.below(2)) v[0x36] &= 7;
            { static const float hv[] = { 249.99f, 250.f, 250.01f, 0.f, -1.f, 1000.f }; float h = rr.below(4) ? hv[rr.below(6)] : std::numeric_limits<float>::quiet_NaN(); if (rr.below(3) == 0) h = GF(rr, 1000.f); std::memcpy(v + offsetof(CVehicle, m_fHealth), &h, 4); }
            CWorld::PlayerInFocus = 0;
            CWorld::Players[0].m_pPed = pk == 0 ? nullptr : reinterpret_cast<CPlayerPed*>(rf::PedE((int)rr.below(rf::kP)));
            g_out[0] = (uint8_t)rr.below(2);
        };
        auto damager = [&]() -> CEntity* { return k == 0 ? nullptr : k == 1 ? static_cast<CEntity*>(CWorld::Players[0].m_pPed) : other; };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { bool& f = *reinterpret_cast<bool*>(g_out); const bool v = static_cast<CVehicle*>(p)->CVehicle::CanVehicleBeDamaged(damager(), (eWeaponType)weapon, f); return (uint64_t)v | ((uint64_t)f << 8); },
            [&](void* p) -> uint64_t { const bool v = (uint8_t)oracle::Fn<int __fastcall(void*, int, void*, int, void*)>(0x6D1280)(p, 0, damager(), weapon, g_out); return (uint64_t)v | ((uint64_t)g_out[0] << 8); });
    }, 3000);
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
    Run("CAutomobile::DoSoftGroundResistance 0x6A4AF0", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t* v) { RealMotion(rr, v, reinterpret_cast<CAutomobile*>(v)->m_fWheelsSuspensionCompression.data(), 4); for (auto& cp : reinterpret_cast<CAutomobile*>(v)->m_wheelColPoint) cp.m_nSurfaceTypeB = (eSurfaceType)PickSurface(rr); CWeather::WetRoads = rr.below(2) ? rr.f01() : 0.f; CTimer::ms_fTimeStep = rr.f01() * 3.f; std::memset(g_out, 0, 4); if (rr.below(2)) g_out[0] = (uint8_t)rr.u32(); };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { static_cast<CAutomobile*>(p)->CAutomobile::DoSoftGroundResistance(*reinterpret_cast<uint32*>(g_out)); return *reinterpret_cast<uint32*>(g_out); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x6A4AF0)(p, 0, g_out); return *reinterpret_cast<uint32*>(g_out); });
    }, 3000);
    Run("CBike::DoSoftGroundResistance 0x6B6D40", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t* v) { RealMotion(rr, v, reinterpret_cast<CBike*>(v)->m_aWheelRatios.data(), 4); if (rr.below(3)) reinterpret_cast<CBike*>(v)->m_GasPedal = rr.f01(); for (auto& cp : reinterpret_cast<CBike*>(v)->m_aWheelColPoints) cp.m_nSurfaceTypeB = (eSurfaceType)PickSurface(rr); CTimer::ms_fTimeStep = rr.f01() * 3.f; std::memset(g_out, 0, 4); if (rr.below(2)) g_out[0] = (uint8_t)rr.u32(); };
        return W3FuncCase(r, d, sizeof(CBike), {}, [&](void* p) -> uint64_t { static_cast<CBike*>(p)->CBike::DoSoftGroundResistance(*reinterpret_cast<uint32*>(g_out)); return *reinterpret_cast<uint32*>(g_out); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x6B6D40)(p, 0, g_out); return *reinterpret_cast<uint32*>(g_out); });
    }, 3000);
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
    // RenderWare callback with only plain-struct accesses (RwObject flags at +2: the same offset in librw and RW 3.7); CVehicle::SetCompAlphaCB is `static` (Entity.h) = not linkable from a test
    Run("CBoat::GetBoatAtomicObjectCB 0x6F00D0", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t*) { std::memset(g_out, 0, 8); if (rr.below(2)) g_frame[2] |= 4; else g_frame[2] &= ~4; };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void*) -> uint64_t { return (uint64_t)(uintptr_t)GetBoatAtomicObjectCB(reinterpret_cast<RwObject*>(g_frame), g_out) == (uint64_t)(uintptr_t)g_frame; },
            [&](void*) -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void*(void*, void*)>(0x6F00D0)(g_frame, g_out) == (uint64_t)(uintptr_t)g_frame; });
    }, 200);
}
