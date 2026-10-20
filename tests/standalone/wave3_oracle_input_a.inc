// ORACLE-W3-INPUT rows A: Pad.cpp (see wave3_oracle_input.inc). The pad world is the whole pad / keyboard / mouse block of the exe (0xB72CB0..0xB73700), both pads randomised.
static void TestW3InputA() {
    using namespace w2; using namespace w3i;
    const auto padOf = [](unsigned i) -> CPad* { return &CPad::Pads[i & 1]; };
    // ---- statics
    ROW("CPad::ClearKeyBoardHistory", 0x53F1E0, 60) {
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { CPad::ClearKeyBoardHistory(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x53F1E0)(); return 0; });
    } ENDROW;
    ROW("CPad::ClearMouseHistory", 0x541BD0, 60) {
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { CPad::ClearMouseHistory(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x541BD0)(); return 0; });
    } ENDROW;
    ROW("CPad::Initialise", 0x541D90, 100) {
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { CPad::Initialise(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x541D90)(); return 0; });
    } ENDROW;
    // ---- members that touch only the pad (and the timer)
    ROW("CPad::SetTouched", 0x53F200, 200) {
        const unsigned i = r.below(2);
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { padOf(i)->SetTouched(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x53F200, padOf(i)); return 0; });
    } ENDROW;
    ROW("CPad::GetTouchedTimeDelta", 0x53F210, 200) {
        const unsigned i = r.below(2);
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { return padOf(i)->GetTouchedTimeDelta(); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x53F210, padOf(i)); });
    } ENDROW;
    ROW("CPad::StopShaking", 0x53FB50, 50) {
        const unsigned i = r.below(2); const int16_t arg = (int16_t)r.u32();
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { padOf(i)->StopShaking(arg); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x53FB50, padOf(i), (int)arg); return 0; });
    } ENDROW;
    ROW("CPad::ProcessPCSpecificStuff", 0x53FB40, 50) {
        const unsigned i = r.below(2);
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { padOf(i)->ProcessPCSpecificStuff(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x53FB40, padOf(i)); return 0; });
    } ENDROW;
    ROW("CPad::Clear", 0x541A70, 300) {
        const unsigned i = r.below(2); const bool a = r.below(2) != 0, b = r.below(2) != 0;
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { padOf(i)->Clear(a, b); return 0; }, [&]() -> uint64_t { ExeThis<void, int, int>(0x541A70, padOf(i), (int)a, (int)b); return 0; });
    } ENDROW;
    ROW("CPad::Update", 0x541C40, 1500) {
        const unsigned i = r.below(2);
        Tu tu([](Rng& rr) { PadWorld(rr); });
        return UCase(r, d, [&]() -> uint64_t { padOf(i)->Update((int32_t)i); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x541C40, padOf(i), (int)i); return 0; });
    } ENDROW;
    // ---- reconcile two controller states (the function-local state lives at 0xB736C0 in the exe)
    ROW("CPad::ReconcileTwoControllersInput", 0x53F530, 3000) {
        const unsigned i = r.below(2);
        Tu tu([](Rng& rr) {
            PadWorld(rr);
            FillState(rr, SAt<CControllerState>(oStA)); FillState(rr, SAt<CControllerState>(oStB));
            if (rr.below(4) == 0) SAt<CControllerState>(oStB) = SAt<CControllerState>(oStA);                                  // equal inputs
            if (rr.below(4) == 0) { auto& a = SAt<CControllerState>(oStA); auto& b = SAt<CControllerState>(oStB); b.LeftStickX = (int16_t)-a.LeftStickX; b.RightStickY = (int16_t)-a.RightStickY; b.LeftStickY = a.LeftStickY ? a.LeftStickY : 0; }   // opposite axes
        });
        CControllerState* out = reinterpret_cast<CControllerState*>(S(oOut));
        return UCase(r, d, [&]() -> uint64_t { return U(&padOf(i)->ReconcileTwoControllersInput(*out, SAt<CControllerState>(oStA), SAt<CControllerState>(oStB))) - U(out); },
            [&]() -> uint64_t { return (uint64_t)(uintptr_t)ExeThis<void*, void*, void*, void*>(0x53F530, padOf(i), out, S(oStA), S(oStB)) - U(out); });
    } ENDROW;
    // ---- shaking by a train (player vehicle is a train; camera position and the point random)
    ROW("CPad::StartShake_Train", 0x53FA70, 2000) {
        const unsigned i = r.below(2);
        const bool inVeh = r.below(6) != 0, train = r.below(5) != 0, vib = r.below(5) != 0, cut = r.below(8) == 0;
        const CVector2D pt = r.below(4) == 0 ? CVector2D{ GF(r, 80.f), GF(r, 80.f) } : CVector2D{ GF(r, 1000.f), GF(r, 1000.f) };
        const CVector cam = r.below(3) ? CVector{ pt.x + GF(r, 90.f), pt.y + GF(r, 90.f), GF(r, 40.f) } : CVector{ GF(r, 1000.f), GF(r, 1000.f), GF(r, 100.f) };   // mostly around the point (4900 = 70 squared), z random: the exe measures in the plane
        Tu tu([&](Rng& rr) {
            PadWorld(rr); PlayerWorld(rr, inVeh, true, train);
            FrontEndMenuManager.m_PrefsUseVibration = vib; CCutsceneMgr::ms_running = cut;
            TheCamera.m_matrix = nullptr; TheCamera.m_placement.m_vPosn = cam;
        });
        // the exe takes the point BY VALUE (two floats on the stack, ret 8)
        return UCase(r, d, [&]() -> uint64_t { padOf(i)->StartShake_Train(pt); return 0; }, [&]() -> uint64_t { ExeThis<void, float, float>(0x53FA70, padOf(i), pt.x, pt.y); return 0; });
    } ENDROW;
    // ---- ped dependent stick queries
    {
        struct PedFn { const char* nm; uint32_t addr; int16_t (CPad::*pf)(CPed*) const; };
        static const PedFn fns[] = {
            { "CPad::GetPedWalkLeftRight(ped)", 0x540DC0, &CPad::GetPedWalkLeftRight },
            { "CPad::GetPedWalkUpDown(ped)", 0x540E20, &CPad::GetPedWalkUpDown },
            { "CPad::AimWeaponLeftRight", 0x541040, &CPad::AimWeaponLeftRight },
            { "CPad::AimWeaponUpDown", 0x5410C0, &CPad::AimWeaponUpDown },
        };
        for (const auto& fn : fns) {
            const std::string nm = std::string(fn.nm) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", fn.addr); return std::string(b); }();
            RunScaledI(nm, [&](Rng& r, std::string& d) -> bool {
                const unsigned i = r.below(2); const bool nullPed = r.below(6) == 0, inVeh = r.below(2) != 0;
                Tu tu([&](Rng& rr) { PadWorld(rr); PlayerWorld(rr, inVeh, rr.below(2) != 0, false); });
                CPed* ped = nullPed ? nullptr : reinterpret_cast<CPed*>(S(oPed));
                return UCase(r, d, [&]() -> uint64_t { return (uint64_t)(uint16_t)(padOf(i)->*fn.pf)(ped); },
                    [&]() -> uint64_t { return (uint64_t)(ExeThis<uint32_t, void*>(fn.addr, padOf(i), ped) & 0xFFFF); });
            }, 3000);
        }
        struct PedFn2 { const char* nm; uint32_t addr; int16_t (CPad::*pf)(CPed*) noexcept; };
        static const PedFn2 fns2[] = {
            { "CPad::LookAroundLeftRight(ped)", 0x540BD0, &CPad::LookAroundLeftRight },
            { "CPad::LookAroundUpDown(ped)", 0x540CC0, &CPad::LookAroundUpDown },
        };
        for (const auto& fn : fns2) {
            const std::string nm = std::string(fn.nm) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", fn.addr); return std::string(b); }();
            RunScaledI(nm, [&](Rng& r, std::string& d) -> bool {
                const unsigned i = r.below(2); const bool nullPed = r.below(6) == 0, inVeh = r.below(2) != 0;
                Tu tu([&](Rng& rr) { PadWorld(rr); PlayerWorld(rr, inVeh, rr.below(2) != 0, false); });
                CPed* ped = nullPed ? nullptr : reinterpret_cast<CPed*>(S(oPed));
                return UCase(r, d, [&]() -> uint64_t { return (uint64_t)(uint16_t)(padOf(i)->*fn.pf)(ped); },
                    [&]() -> uint64_t { return (uint64_t)(ExeThis<uint32_t, void*>(fn.addr, padOf(i), ped) & 0xFFFF); });
            }, 3000);
        }
    }
    // ---- key pressed (reads the new keyboard state, writes the key code out)
    ROW("GetCurrentKeyPressed", 0x541490, 3000) {
        Tu tu([](Rng& rr) {
            PadWorld(rr);
            // one or a few keys down, like a real keyboard state (all keys random would always find the first one)
            if (rr.below(3)) { auto* k = reinterpret_cast<uint8_t*>(&CPad::NewKeyState); std::memset(k, 0, sizeof(CKeyboardState)); const int n = (int)rr.below(3) + 1; for (int j = 0; j < n; ++j) k[rr.below(sizeof(CKeyboardState))] = 1; }
        });
        int32_t* out = reinterpret_cast<int32_t*>(S(oOut));
        const uint32_t init = r.u32();
        return UCase(r, d, [&]() -> uint64_t { *out = (int32_t)init; GetCurrentKeyPressed(*reinterpret_cast<RsKeyCodes*>(out)); return 0; },
            [&]() -> uint64_t { *out = (int32_t)init; oracle::Fn<void __cdecl(void*)>(0x541490)(out); return 0; });
    } ENDROW;
}
