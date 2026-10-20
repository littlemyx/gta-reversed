// ORACLE-W3-AI rows: PedIntelligence.cpp (classes A + B). Two peds in the scratch block with intelligences, acquaintance masks, scanner arrays and interesting-entity slots.
static void TestW3AiPed() {
    using namespace w2; using namespace w3a;
    const auto pedA = [] { return reinterpret_cast<CPed*>(S(oPedA)); };
    const auto pedB = [] { return reinterpret_cast<CPed*>(S(oPedB)); };
    const auto intA = [] { return reinterpret_cast<CPedIntelligence*>(S(oIntA)); };
    const auto world = [](Rng& rr) {
        PedWorld(rr, oPedA, oIntA, 0); PedWorld(rr, oPedB, oIntB, 1);
        // peds that matter to the acquaintance tests: types / masks with shared bits so both answers occur
        if (rr.below(2)) { auto* a = reinterpret_cast<CPed*>(S(oPedA)); auto* b = reinterpret_cast<CPed*>(S(oPedB)); const uint32_t bit = 1u << ((int)b->m_nPedType & 31); for (auto& m : a->m_acquaintance.m_acquaintances) if (rr.below(3) == 0) m |= bit; }
        if (rr.below(8) == 0) { auto* a = reinterpret_cast<CPed*>(S(oPedA)); auto* b = reinterpret_cast<CPed*>(S(oPedB)); b->m_nPedType = a->m_nPedType; }
    };
    ROW("CPedIntelligence::GetPedEntities", 0x4893E0, 40) {
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { return U(intA()->GetPedEntities()); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x4893E0, intA()); });
    } ENDROW;
    ROW("CPedIntelligence::SetHearingRange", 0x600BE0, 80) {
        const float v = Fl(r, 100.f, 6);
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { intA()->SetHearingRange(v); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x600BE0, intA(), v); return 0; });
    } ENDROW;
    ROW("CPedIntelligence::SetSeeingRange", 0x600BF0, 80) {
        const float v = Fl(r, 100.f, 6);
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { intA()->SetSeeingRange(v); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x600BF0, intA(), v); return 0; });
    } ENDROW;
    ROW("CPedIntelligence::GetEffectInUse", 0x6018D0, 40) {
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { return U(intA()->GetEffectInUse()); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x6018D0, intA()); });
    } ENDROW;
    ROW("CPedIntelligence::SetEffectInUse", 0x6018E0, 40) {
        const uint32_t v = r.u32();
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { intA()->SetEffectInUse(reinterpret_cast<C2dEffect*>(uintptr_t(v))); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t>(0x6018E0, intA(), v); return 0; });
    } ENDROW;
    ROW("CPedIntelligence::FindRespectedFriendInInformRange", 0x600CF0, 1500) {
        Tu tu([&](Rng& rr) {
            world(rr);
            auto* in = intA();
            in->m_nDmNumPedsToScan = rr.below(5) == 0 ? rr.below(17) : 3;
            in->m_fDmRadius = rr.below(8) == 0 ? Fl(rr, 100.f, 3) : rr.f01() * 40.f;
            // the respected mask contains the type of the scanned ped in about half of the cases
            if (rr.below(2)) pedA()->m_acquaintance.m_nRespect |= 1u << ((int)pedB()->m_nPedType & 31);
            for (auto& e : in->m_pedScanner.m_apEntities) e = rr.below(3) == 0 ? nullptr : (rr.below(8) == 0 ? reinterpret_cast<CEntity*>(pedA()) : reinterpret_cast<CEntity*>(pedB()));
            // positions close together half of the time so the radius test goes both ways
            if (rr.below(2) && pedB()->m_matrix && pedA()->m_matrix) { pedB()->m_matrix->GetPosition() = pedA()->m_matrix->GetPosition() + CVector((rr.f01() - 0.5f) * 40.f, (rr.f01() - 0.5f) * 40.f, (rr.f01() - 0.5f) * 10.f); }
        });
        return UCase(r, d, [&]() -> uint64_t { return B8(intA()->FindRespectedFriendInInformRange()); }, [&]() -> uint64_t { return B8(ExeThis<bool>(0x600CF0, intA())); });
    } ENDROW;
    ROW("CPedIntelligence::IsFriendlyWith", 0x601BC0, 1000) {
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { return B8(intA()->IsFriendlyWith(*pedB())); }, [&]() -> uint64_t { return B8(ExeThis<bool, void*>(0x601BC0, intA(), pedB())); });
    } ENDROW;
    ROW("CPedIntelligence::IsThreatenedBy", 0x601C30, 1000) {
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { return B8(intA()->IsThreatenedBy(*pedB())); }, [&]() -> uint64_t { return B8(ExeThis<bool, void*>(0x601C30, intA(), pedB())); });
    } ENDROW;
    ROW("CPedIntelligence::Respects", 0x601C90, 1000) {
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { return B8(intA()->Respects(pedB())); }, [&]() -> uint64_t { return B8(ExeThis<bool, void*>(0x601C90, intA(), pedB())); });
    } ENDROW;
    ROW("CPedIntelligence::AreFriends", 0x601D10, 1000) {
        Tu tu([&](Rng& rr) { world(rr); });
        return UCase(r, d, [&]() -> uint64_t { return B8(CPedIntelligence::AreFriends(*pedA(), *pedB())); }, [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*, void*)>(0x601D10)(pedA(), pedB())); });
    } ENDROW;
    ROW("CPedIntelligence::RecordEventForScript", 0x602050, 1500) {
        const int32_t id = r.below(4) == 0 ? (int32_t)r.below(0x40) : (r.below(3) == 0 ? 0x20 : (int32_t)r.below(0x23)), pr = r.below(8) == 0 ? (int32_t)r.below(300) - 20 : (int32_t)r.below(6);
        Tu tu([&](Rng& rr) { world(rr); if (rr.below(2)) intA()->m_nEventPriority = (uint8_t)rr.below(6); });
        return UCase(r, d, [&]() -> uint64_t { intA()->RecordEventForScript(id, pr); return 0; }, [&]() -> uint64_t { ExeThis<void, int, int>(0x602050, intA(), id, pr); return 0; });
    } ENDROW;
    ROW("CPedIntelligence::HasInterestingEntites", 0x602080, 300) {
        Tu tu([&](Rng& rr) { world(rr); for (auto& e : intA()->m_apInterestingEntities) e = rr.below(2) ? nullptr : reinterpret_cast<CEntity*>(S(oPedB)); });
        return UCase(r, d, [&]() -> uint64_t { return B8(intA()->HasInterestingEntites()); }, [&]() -> uint64_t { return B8(ExeThis<bool>(0x602080, intA())); });
    } ENDROW;
    ROW("CPedIntelligence::IsInterestingEntity", 0x6020A0, 500) {
        int pick = (int)r.below(5);
        Tu tu([&](Rng& rr) { world(rr); for (auto& e : intA()->m_apInterestingEntities) e = rr.below(3) == 0 ? nullptr : reinterpret_cast<CEntity*>(rr.below(2) ? S(oPedB) : S(oVehA)); });
        return UCase(r, d, [&]() -> uint64_t { CEntity* a = pick == 0 ? nullptr : pick == 1 ? reinterpret_cast<CEntity*>(S(oPedB)) : pick == 2 ? reinterpret_cast<CEntity*>(S(oVehA)) : reinterpret_cast<CEntity*>(S(oObjA)); return B8(intA()->IsInterestingEntity(a)); },
                                  [&]() -> uint64_t { void* a = pick == 0 ? nullptr : pick == 1 ? S(oPedB) : pick == 2 ? S(oVehA) : S(oObjA); return B8(ExeThis<bool, void*>(0x6020A0, intA(), a)); });
    } ENDROW;
    ROW("CPedIntelligence::RemoveAllInterestingEntities", 0x602320, 400) {
        Tu tu([&](Rng& rr) {
            world(rr);
            // the slots hold reference-world entities, registered (CEntity::RegisterReference) in most cases so that CleanUpOldReference finds the node
            for (auto& e : intA()->m_apInterestingEntities) {
                e = rr.below(4) == 0 ? nullptr : rf::All[rr.below((uint32)(rf::kP + rf::kV + rf::kO))];
                if (e && rr.below(4) != 0) e->RegisterReference(&e);
            }
        }, true);
        return UCase(r, d, [&]() -> uint64_t { intA()->RemoveAllInterestingEntities(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x602320, intA()); return 0; });
    } ENDROW;
    ROW("CPedIntelligence::IncrementAngerAtPlayer", 0x421050, 1500) {
        const uint8_t anger = (uint8_t)(r.below(3) ? r.below(40) : r.below(256));
        Tu tu([&](Rng& rr) {
            world(rr);
            CTimer::m_snTimeInMilliseconds = rr.below(3) ? rr.below(2000000) : rr.u32();
            auto& ms = intA()->m_mentalState;
            auto* b = reinterpret_cast<uint8_t*>(&ms.m_AngerTimer);
            b[8] &= 1; b[9] &= 1;   // the started / stopped flags of the timer are bools
            if (rr.below(2)) *reinterpret_cast<uint32_t*>(b) = CTimer::m_snTimeInMilliseconds - rr.below(7000);
            if (rr.below(2)) *reinterpret_cast<uint32_t*>(b + 4) = rr.below(8000);
        });
        return UCase(r, d, [&]() -> uint64_t { intA()->IncrementAngerAtPlayer(anger); return 0; }, [&]() -> uint64_t { ExeThis<void, int>(0x421050, intA(), (int)anger); return 0; });
    } ENDROW;
    ROW("CPedIntelligence::CanSeeEntityWithLights", 0x605550, 2000) {
        const int kind = (int)r.below(8);   // 0: vehicle (not a ped), 1: non-player ped, else the player-type ped
        Tu tu([&](Rng& rr) {
            world(rr);
            auto* b = pedB();
            SAt<void*>(oPedB + 0x480) = rr.below(2) ? nullptr : S(oVehA);   // m_pVehicle (CPhysical::GetLightingTotal reads the vehicle's flag byte +0x86)
            if (kind == 0) { reinterpret_cast<uint8_t*>(b)[0x36] = (reinterpret_cast<uint8_t*>(b)[0x36] & 0xF8) | 2; }
            else if (kind == 1) b->m_nPedType = PED_TYPE_CIVMALE;
            else b->m_nPedType = rr.below(4) ? PED_TYPE_PLAYER1 : PED_TYPE_PLAYER2;
            // physical lighting fields (CPhysical::GetLightingTotal): brightness floats in 0..1.5
            for (size_t o = 0x130; o < 0x150; o += 4) *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(b) + o) = rr.f01() * 1.5f;
            if (rr.below(2) && b->m_matrix && pedA()->m_matrix) b->m_matrix->GetPosition() = pedA()->m_matrix->GetPosition() + CVector((rr.f01() - 0.5f) * 80.f, (rr.f01() - 0.5f) * 80.f, (rr.f01() - 0.5f) * 10.f);
        });
        return UCase(r, d, [&]() -> uint64_t { return RF(intA()->CanSeeEntityWithLights(reinterpret_cast<CEntity*>(pedB()), 0)); },
                                  [&]() -> uint64_t { return RF(ExeThis<float, void*, int>(0x605550, intA(), pedB(), 0)); });
    } ENDROW;
    ROW("CPedIntelligence::GetActivePrimaryTask", 0x4B85B0, 400) {
        Tu tu([&](Rng& rr) { world(rr); for (auto& t : intA()->m_TaskMgr.m_aPrimaryTasks) t = rr.below(2) ? nullptr : reinterpret_cast<CTask*>(S(oOut + 4 * rr.below(32))); });
        return UCase(r, d, [&]() -> uint64_t { return U(intA()->GetActivePrimaryTask()); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x4B85B0, intA()); });
    } ENDROW;
}
