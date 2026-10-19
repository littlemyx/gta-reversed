// ORACLE-W3-SCRIPTS rows A: CRunningScript (RunningScript.cpp). ROW / CC / Tw are defined in wave3_oracle_scripts.inc. `this` is the scratch script Scr(0) (thiscall, ecx = this).
static void TestW3ScriptsA() {
    using namespace w2; using namespace w3s;
    CRunningScript* const S = Scr(0);

    ROW("CRunningScript::GetPointerToLocalVariable", 0x463CA0, 600) {
        // in-bounds locals only: the port indexes a std::array (hardened STL: out of range asserts), the exe just computes the address
        const bool mis = r.below(3) == 0; Tw tw([mis](Rng&) { Scr(0)->m_ThisMustBeTheOnlyMissionRunning = mis; });
        const int32_t loc = mis ? (int32_t)r.below(1024) : (int32_t)r.below(34);
        CC(return P32(S->GetPointerToLocalVariable(loc));, return P32(ExeT<void*>(0x463CA0, S, loc)););
    } ENDROW;
    ROW("CRunningScript::GetPointerToLocalArrayElement", 0x463CC0, 1500) {
        const bool mis = r.below(3) == 0; Tw tw([mis](Rng&) { Scr(0)->m_ThisMustBeTheOnlyMissionRunning = mis; });
        const int lim = mis ? 1024 : 34;
        const uint8_t sz = (uint8_t)(r.below(3) ? 1 : 1 + r.below(4));
        const uint16_t idx = (uint16_t)r.below(1 + (lim - 1) / sz);
        const int32_t base = (int32_t)r.below(lim - idx * sz);
        CC(return P32(S->GetPointerToLocalArrayElement(base, idx, sz));, return P32(ExeT<void*>(0x463CC0, S, base, (int)idx, (int)sz)););
    } ENDROW;
    ROW("CRunningScript::ReadArrayInformation", 0x463CF0, 1500) {
        Tw tw([](Rng& rr) {
            FillVars(rr);
            const bool m = rr.below(3) == 0; Scr(0)->m_ThisMustBeTheOnlyMissionRunning = m;
            Em e{ rr, Buf(), m }; e.Arr(rr.below(2));
            Scr(0)->m_IP = Buf();
        });
        const int32_t upd = r.below(4) == 0 ? (int32_t)r.below(5) : (int32_t)r.below(2);
        CC(S->ReadArrayInformation(upd, reinterpret_cast<uint16*>(Out()), reinterpret_cast<int32*>(Out() + 8)); return P32(S->m_IP);,
           ExeT<void>(0x463CF0, S, upd, Out(), Out() + 8); return P32(S->m_IP););
    } ENDROW;
    ROW("CRunningScript::ReadParametersForNewlyStartedScript", 0x464500, 2500) {
        Tw tw([](Rng& rr) {
            FillVars(rr);
            const bool m = rr.below(3) == 0; Scr(0)->m_ThisMustBeTheOnlyMissionRunning = m;
            Em e{ rr, Buf(), m };
            const int n = rr.below(4) == 0 ? (int)rr.below(24) : (int)rr.below(9);
            for (int i = 0; i < n; ++i) {
                if (rr.below(40) == 0) e.U8(9 + rr.below(247));          // unknown type: skipped (and counted)
                else e.Param(1 + (int)rr.below(8));
            }
            e.U8(0);
            Scr(0)->m_IP = Buf();
        });
        CC(S->ReadParametersForNewlyStartedScript(Scr(1)); return P32(S->m_IP);, ExeT<void>(0x464500, S, Scr(1)); return P32(S->m_IP););
    } ENDROW;
    ROW("CRunningScript::GetIndexOfGlobalVariable", 0x464700, 1500) {
        Tw tw([](Rng& rr) {
            FillVars(rr);
            const bool m = rr.below(3) == 0; Scr(0)->m_ThisMustBeTheOnlyMissionRunning = m;
            Em e{ rr, Buf(), m };
            if (rr.below(2)) { e.U8(2); e.U16(rr.below(65536)); } else { e.U8(7); e.Arr(false); }
            Scr(0)->m_IP = Buf();
        });
        CC(const auto v = S->GetIndexOfGlobalVariable(); return (uint64_t)v | ((uint64_t)P32(S->m_IP) << 16);,
           const auto v = ExeT<uint32_t>(0x464700, S); return (uint64_t)(v & 0xFFFF) | ((uint64_t)P32(S->m_IP) << 16););
    } ENDROW;
    ROW("CRunningScript::Init", 0x4648E0, 200) {
        CC(S->Init(); return 0;, ExeT<void>(0x4648E0, S); return 0;);
    } ENDROW;
    // the list rows: a random mix of the four scratch scripts forms the list, the head cell is in the scratch block
    static const auto MakeList = [](Rng& rr, bool selfIn) {
        CRunningScript* n[4] = { Scr(0), Scr(1), Scr(2), Scr(3) };
        for (int i = 3; i > 0; --i) std::swap(n[i], n[rr.below(i + 1)]);
        int cnt = rr.below(5);
        auto** head = reinterpret_cast<CRunningScript**>(g_sc + oHead);
        if (!selfIn) { int j = 0; for (int i = 0; i < 4; ++i) if (n[i] != Scr(0)) n[j++] = n[i]; cnt = std::min(cnt, 3); }
        else { bool in = false; for (int i = 0; i < cnt; ++i) in |= n[i] == Scr(0); if (!in) { cnt = std::max(cnt, 1); n[0] = Scr(0); } }
        for (int i = 0; i < 4; ++i) { n[i]->m_pNext = nullptr; n[i]->m_pPrev = nullptr; }
        for (int i = 0; i < cnt; ++i) { n[i]->m_pPrev = i ? n[i - 1] : nullptr; n[i]->m_pNext = i + 1 < cnt ? n[i + 1] : nullptr; }
        *head = cnt ? n[0] : nullptr;
        if (rr.below(5) == 0) {   // inconsistent but dereferenceable links / head
            for (int i = 0; i < 4; ++i) { n[i]->m_pNext = rr.below(2) ? nullptr : Scr(rr.below(4)); n[i]->m_pPrev = rr.below(2) ? nullptr : Scr(rr.below(4)); }
            *head = rr.below(3) ? Scr(rr.below(4)) : nullptr;
        }
    };
    ROW("CRunningScript::AddScriptToList", 0x464C00, 400) {
        Tw tw([](Rng& rr) { MakeList(rr, false); });
        auto** head = reinterpret_cast<CRunningScript**>(g_sc + oHead);
        CC(S->AddScriptToList(head); return P32(*head);, ExeT<void>(0x464C00, S, head); return P32(*head););
    } ENDROW;
    ROW("CRunningScript::RemoveScriptFromList", 0x464BD0, 400) {
        Tw tw([](Rng& rr) { MakeList(rr, true); });
        auto** head = reinterpret_cast<CRunningScript**>(g_sc + oHead);
        CC(S->RemoveScriptFromList(head); return P32(*head);, ExeT<void>(0x464BD0, S, head); return P32(*head););
    } ENDROW;
    ROW("CRunningScript::UpdateCompareFlag", 0x4859D0, 800) {
        Tw tw([](Rng& rr) {
            Scr(0)->m_AndOrState = (uint16)(rr.below(2) ? rr.below(32) : rr.u32());
            if (rr.below(2)) Scr(0)->m_AndOrState = (uint16)(rr.below(2) ? 1 + rr.below(8) : 21 + rr.below(8));
        });
        const int st = r.below(2);
        CC(S->UpdateCompareFlag(st != 0); return 0;, ExeT<void>(0x4859D0, S, st); return 0;);
    } ENDROW;
    ROW("CRunningScript::UpdatePC", 0x464DA0, 600) {
        Tw tw([](Rng& rr) { Scr(0)->m_BaseIP = reinterpret_cast<uint8_t*>(rr.u32()); Scr(0)->m_IP = reinterpret_cast<uint8_t*>(rr.u32()); });
        int32_t v;
        switch (r.below(6)) {
        case 0: v = (int32_t)r.below(CTheScripts::SCRIPT_SPACE_SIZE); break;   // ScriptSpace[newIP] is a hardened std::array in the port: in-bounds offsets only
        case 1: v = -(int32_t)r.below(300000); break;
        case 2: v = 0; break;
        case 3: v = -(int32_t)(r.u32() >> 1); break;
        case 4: v = (int32_t)r.below(CTheScripts::SCRIPT_SPACE_SIZE); break;
        default: v = (int32_t)r.below(5000); break;
        }
        CC(S->UpdatePC(v); return P32(S->m_IP);, ExeT<void>(0x464DA0, S, v); return P32(S->m_IP););
    } ENDROW;
    ROW("CRunningScript::GetPadState", 0x485B10, 800) {
        Tw tw([](Rng& rr) { FillGarbage(rr, (uintptr_t)&CPad::Pads[0], sizeof(CPad) * 2); });
        const int idx = r.below(2);
        const int btn = r.below(5) ? (int)r.below(20) : (int)r.below(40);
        CC(return (uint16_t)S->GetPadState((uint16)idx, (eButtonId)btn);, return (uint16_t)ExeT<uint32_t>(0x485B10, S, idx, btn););
    } ENDROW;
    ROW("CRunningScript::GetCorrectPedModelIndexForEmergencyServiceType", 0x464F50, 1200) {
        const uint32_t model = r.below(4) ? 275 + r.below(20) : r.below(2) ? r.below(400) : r.u32();
        const uint32_t pt = r.below(2) ? 6 : r.below(12);
        Tw tw([model](Rng&) { *reinterpret_cast<uint32_t*>(Out()) = model; });
        CC(S->GetCorrectPedModelIndexForEmergencyServiceType((ePedType)pt, reinterpret_cast<uint32*>(Out())); return 0;,
           ExeT<void>(0x464F50, S, pt, Out()); return 0;);
    } ENDROW;
}
