// ORACLE-C-VEH-2: hand rows of the former "kills the test process" family (the cause was SafeSEH: the exe's C++ EH frames on the stack were rejected by the OS when a fault unwound through them;
// wave3_vehicle_oracle_test is linked /SAFESEH:NO now) whose generated rows were not good enough. Included by wave3_oracle_veh.inc (namespace w3 helpers, g_post).
static void TestW3VehHand3() {
    using namespace w2; using namespace w3;
    // 0x6A38A0: with the car node frames null CAutomobile::OpenDoor returns at once (the port's RwFrame is librw's, the exe's RW 3.7: no shared fake frame), so the row compares the timers and the
    // decisions (door missing / closed / getting-in flags), which is where the port differed. Times are set around the 500 ms window edge.
    Run("CAutomobile::ProcessAutoBusDoors 0x6A38A0", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t* v) {
            auto* au = reinterpret_cast<CAutomobile*>(v);
            for (auto*& n : au->m_aCarNodes) n = nullptr;
            const uint32 now = 100000 + rr.below(1000000);
            CTimer::m_snTimeInMilliseconds = now; *reinterpret_cast<uint32_t*>(0xB7CB84) = now;   // the exe's clock (0x6A38A4 reads 0xB7CB84)
            switch (rr.below(6)) { case 0: au->m_nBusDoorTimerEnd = 0; break; case 1: au->m_nBusDoorTimerEnd = now - rr.below(3000); break; case 2: au->m_nBusDoorTimerEnd = now + 500 + rr.below(5) - 2; break;
                                   case 3: au->m_nBusDoorTimerEnd = now + rr.below(600); break; case 4: au->m_nBusDoorTimerEnd = now + rr.below(1000000); break; default: au->m_nBusDoorTimerEnd = now; break; }
            au->m_nBusDoorTimerStart = rr.below(2) ? 0 : rr.u32();
            au->m_nGettingInFlags = (uint8)(rr.below(4) ? rr.below(8) : rr.u32());
            for (int i = 0; i < 2; ++i) {   // doors 2 / 3 (left / right front): present or missing, closed or open
                au->m_damageManager.SetDoorStatus(i ? DOOR_RIGHT_FRONT : DOOR_LEFT_FRONT, (eDoorStatus)(rr.below(4) == 0 ? (int)DAMSTATE_NOTPRESENT : (int)rr.below(3)));
                auto& dr = au->m_doors[i ? DOOR_RIGHT_FRONT : DOOR_LEFT_FRONT];
                dr.m_closedAngle = rr.f01(); dr.m_angle = rr.below(2) ? dr.m_closedAngle : rr.f01();
            }
        };
        // the end time is compared relative to each side's own clock
        auto rel = [](void* p, uint32 clk) { auto* au = static_cast<CAutomobile*>(p); if (au->m_nBusDoorTimerEnd) au->m_nBusDoorTimerEnd -= clk; };
        return W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { static_cast<CAutomobile*>(p)->CAutomobile::ProcessAutoBusDoors(); rel(p, CTimer::m_snTimeInMilliseconds); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*)>(0x6A38A0)(p); rel(p, *reinterpret_cast<uint32_t*>(0xB7CB84)); return 0; }, false, false, "CAutomobile");
    }, 600);
    // 0x6B7150: the product CMatrix temp leaves the padding words of m_mLeanMatrix (+0xC / 0x1C / 0x2C / 0x3C) at stack garbage in the exe and at the debug fill in the port (never read): zeroed after each call
    Run("CBike::CalculateLeanMatrix 0x6B7150", [&](Rng& r, std::string& d) {
        g_post = [&](Rng& rr, uint8_t* v) {
            auto* bk = reinterpret_cast<CBike*>(v);
            bk->m_bLeanMatrixCalculated = rr.below(5) == 0;
            switch (rr.below(6)) { case 0: bk->m_RideAnimData.LeanAngle = 0.f; break; case 1: bk->m_RideAnimData.LeanAngle = (rr.f01() - 0.5f) * 3.2f; break; default: bk->m_RideAnimData.LeanAngle = (rr.f01() - 0.5f) * 1.2f; break; }
        };
        auto clearPad = [](void* p) { for (size_t o : { 0xCu, 0x1Cu, 0x2Cu, 0x3Cu }) std::memset(static_cast<uint8_t*>(p) + offsetof(CBike, m_mLeanMatrix) + o, 0, 4); };
        return W3FuncCase(r, d, sizeof(CBike), {}, [&](void* p) -> uint64_t { static_cast<CBike*>(p)->CBike::CalculateLeanMatrix(); clearPad(p); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*)>(0x6B7150)(p); clearPad(p); return 0; }, false, false, "CBike");
    }, 600);
    // 0x6D6640: the vehicle side of the collision pre-check (every vehicle collision). colPhysical is a pool ped / vehicle / object of the reference world; the links between the two
    // (ignored-collision entity, attached-to, knocked-off-bike ped) are set in a share of the cases so that the early-out branches are reached.
    Run("CVehicle::SpecialEntityPreCollisionStuff 0x6D6640", [&](Rng& r, std::string& d) {
        CPhysical* col = static_cast<CPhysical*>(GEntN(r));
        const bool ign = r.below(2);
        const uint32 link = r.below(8);
        g_post = [&](Rng& rr, uint8_t* v) {
            auto* me = reinterpret_cast<CVehicle*>(v);
            std::memset(g_out, 0, 8);
            FillGarbage(rr, g_mi2, sizeof g_mi2);   // the collider's model info (IsSwayInWind1 / smashable / temp-collision flags are read from it)
            CModelInfo::ms_modelInfoPtrs[col->m_nModelIndex] = reinterpret_cast<CBaseModelInfo*>(g_mi2);
            switch (link) { case 0: me->m_pEntityIgnoredCollision = col; break; case 1: col->m_pEntityIgnoredCollision = me; break; case 2: me->m_pAttachedTo = col; break; case 3: col->m_pAttachedTo = me; break;
                            case 4: if (col->GetIsTypePed()) { static_cast<CPed*>(col)->bKnockedOffBike = true; static_cast<CPed*>(col)->m_pVehicle = me; } break; default: break; }
            if (rr.below(3) == 0) { me->physicalFlags.bDisableCollisionForce = true; col->physicalFlags.bDisableCollisionForce = true; }
        };
        auto pack = []() -> uint64_t { return (uint64_t)g_out[0] | ((uint64_t)g_out[1] << 8) | ((uint64_t)g_out[2] << 16) | ((uint64_t)g_out[3] << 24); };
        { char t[96]; std::snprintf(t, sizeof t, " {link %u ign %d coltype %d}", link, (int)ign, (int)col->GetType()); d += t; }
        auto*& miSlot = CModelInfo::ms_modelInfoPtrs[col->m_nModelIndex]; auto* const miOld = miSlot;
        const bool ok = W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { static_cast<CVehicle*>(p)->CVehicle::SpecialEntityPreCollisionStuff(col, ign, *reinterpret_cast<bool*>(g_out), *reinterpret_cast<bool*>(g_out + 1), *reinterpret_cast<bool*>(g_out + 2), *reinterpret_cast<bool*>(g_out + 3)); return pack(); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, int, void*, void*, void*, void*)>(0x6D6640)(p, 0, col, ign, g_out, g_out + 1, g_out + 2, g_out + 3); return pack(); }, false, false, "CVehicle");
        miSlot = miOld; return ok;
    }, 3000);
}
