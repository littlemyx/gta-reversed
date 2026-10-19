// ORACLE-W3-SCRIPTS rows C: CScriptsForBrains (ScriptsForBrains.cpp) and CScripted2dEffects / tUserList (Scripted2dEffects.cpp). ROW / CC / Tw are defined in wave3_oracle_scripts.inc.
static void TestW3ScriptsC() {
    using namespace w2; using namespace w3s;
    // `this` of the brain rows: a copy of the 1400 byte CScriptsForBrains at oBrn + 0x40 (slack in front for the index -1 read of IsObjectWithinBrainActivationRange, and behind for strcpy overflows)
    CScriptsForBrains* const SB = reinterpret_cast<CScriptsForBrains*>(g_sc + oBrn + 0x40);
    // padding bytes of tScriptForBrains (+5..+7 of every 20): the port copies a value-initialised temporary (zeroes), the exe leaves them
    const auto PadMask = []() { std::vector<uint8_t> m(kScSize, 0); for (int i = 0; i < 70; ++i) for (int k = 5; k < 8; ++k) m[oBrn + 0x40 + i * 20 + k] = 1; return m; };
    ROW("CScriptsForBrains::Init", 0x46A8C0, 100) {
        MaskScope ms(PadMask());
        CC(SB->Init(); return 0;, ExeT<void>(0x46A8C0, SB); return 0;);
    } ENDROW;
    ROW("CScriptsForBrains::AddNewStreamedScriptBrainForCodeUse", 0x46A9C0, 2000) {
        const int mode = r.below(4);
        const int16_t idx = (int16_t)(r.below(70) + (r.below(4) == 0 ? 1000 : 0));
        const int8_t type = (int8_t)(r.below(4) ? r.below(6) : (int)(int8_t)r.u32());
        const size_t len = r.below(8) ? r.below(8) : 8 + r.below(5);
        Tw tw([=](Rng& rr) {
            auto* b = reinterpret_cast<tScriptForBrains*>(reinterpret_cast<uint8_t*>(SB));
            for (int i = 0; i < 70; ++i) {
                int16_t v = (int16_t)rr.u32(); if (v == -1) v = 0;
                if (rr.below(3) == 0) v = -1;
                if (mode == 3) v = (int16_t)(rr.u32() % 1000);          // table full
                std::memcpy(reinterpret_cast<uint8_t*>(&b[i]), &v, 2);
            }
            char* nm = reinterpret_cast<char*>(Out());
            for (size_t i = 0; i < len; ++i) nm[i] = (char)('a' + rr.below(26));
            nm[len] = 0;
        });
        char* nm = reinterpret_cast<char*>(Out());
        CC(SB->AddNewStreamedScriptBrainForCodeUse(idx, nm, type); return 0;, ExeT<void>(0x46A9C0, SB, (int)idx, nm, (int)type); return 0;);
    } ENDROW;
    ROW("CScriptsForBrains::IsObjectWithinBrainActivationRange", 0x46B3D0, 3000) {
        const int bi = (int)r.below(70);   // (-1 reads brain[-1]: a hardened std::array assert in the port)
        const bool placement = r.below(3) == 0;
        Tw tw([=](Rng& rr) {
            auto* b = reinterpret_cast<uint8_t*>(SB);
            for (int i = 0; i < 70; ++i) {
                b[i * 20 + 2] = (uint8_t)(rr.below(3) ? 1 : rr.u32());                       // m_TypeOfBrain
                const float rad = rr.below(5) ? 1.f + rr.f01() * 80.f : GenF(rr, 100.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
                std::memcpy(b + i * 20 + 8, &rad, 4);
            }
            CObject* o = Obj();
            o->m_nStreamedScriptBrainToLoad = (int16_t)bi;
            auto* m = reinterpret_cast<CMatrix*>(Ent(3));
            std::memset(m, 0, sizeof(CMatrix));
            const auto P = [&]() { return rr.below(6) ? GF(rr, 200.f) : GenF(rr, 200.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF); };
            const CVector pos(P(), P(), P());
            if (placement) { o->m_matrix = nullptr; o->m_placement.m_vPosn = pos; } else { o->m_matrix = reinterpret_cast<CMatrixLink*>(m); m->GetPosition() = pos; }
            *reinterpret_cast<CVector*>(Out() + 0x20) = rr.below(3) ? CVector(pos.x + GF(rr, 90.f), pos.y + GF(rr, 90.f), pos.z + GF(rr, 90.f)) : CVector(P(), P(), P());
        });
        const CVector* pt = reinterpret_cast<const CVector*>(Out() + 0x20);
        CC(return B8(SB->IsObjectWithinBrainActivationRange(Obj(), *pt));, return B8(ExeT<bool>(0x46B3D0, SB, Obj(), pt)););
    } ENDROW;

    ROW("tUserList::IsPedTypeInList", 0x5FE960, 1500) {
        const int32_t pt = r.below(3) ? (int32_t)r.below(30) : (int32_t)r.u32();
        tUserList* const L = reinterpret_cast<tUserList*>(g_sc + oBrn + 0x40);
        Tw tw([](Rng& rr) {
            auto* l = reinterpret_cast<tUserList*>(g_sc + oBrn + 0x40);
            for (int i = 0; i < 4; ++i) { l->m_UserTypes[i] = rr.below(2) ? -2 : (rr.below(2) ? -1 : (int32)rr.below(300)); l->m_UserTypesByPedType[i] = rr.below(3) ? (int32)rr.below(30) : (int32)rr.u32(); }
            l->m_bUseList = rr.below(3) != 0;
        });
        CC(return B8(L->IsPedTypeInList(pt));, return B8(ExeT<bool>(0x5FE960, L, pt)););
    } ENDROW;
    ROW("CScripted2dEffects::Init", 0x6FA6F0, 100) {
        Tw tw([](Rng& rr) { FillGarbage(rr, (uintptr_t)&CScripted2dEffects::ms_radii[0], 0xC3E000 - 0xC39EE0); });
        CC(CScripted2dEffects::Init(); return 0;, ExeC<void>(0x6FA6F0); return 0;);
    } ENDROW;
    ROW("CScripted2dEffects::GetIndex", 0x6F9F60, 600) {
        const int k = r.below(72);
        const uint32_t off = r.below(5) == 0 ? (uint32_t)r.below(0x40) : 0;   // not exactly the start of an effect
        const C2dEffectPedAttractor* e = k >= 64 ? reinterpret_cast<const C2dEffectPedAttractor*>(Out()) : reinterpret_cast<const C2dEffectPedAttractor*>(0xC3AB00 + (uintptr_t)k * 0x40 + off);
        CC(return (uint32_t)CScripted2dEffects::GetIndex(e);, return ExeC<uint32_t>(0x6F9F60, e););
    } ENDROW;
    ROW("CScripted2dEffects::GetEffectPairs", 0x6FA840, 600) {
        const int k = r.below(72);
        const uint32_t off = r.below(5) == 0 ? (uint32_t)r.below(0x40) : 0;
        const C2dEffectPedAttractor* e = k >= 64 ? reinterpret_cast<const C2dEffectPedAttractor*>(Out()) : reinterpret_cast<const C2dEffectPedAttractor*>(0xC3AB00 + (uintptr_t)k * 0x40 + off);
        CC(return P32(CScripted2dEffects::GetEffectPairs(e));, return P32(ExeC<void*>(0x6FA840, e)););
    } ENDROW;
}
