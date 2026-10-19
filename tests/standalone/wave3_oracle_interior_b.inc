// ORACLE-W3-INTERIOR rows B: InteriorGroup_c and InteriorManager_c (see wave3_oracle_interior.inc). ROW / IC / Tw are defined there.
static CVector g_pt;            // a world point chosen after the fixture is built
static float g_rad, g_bx, g_by, g_bz;
static int GrpInt(Rng& rr, int g) {   // index of a random non-null interior of group g (or -1)
    using namespace w3i;
    int c[8], n = 0;
    for (int k = 0; k < 8; ++k) { const uint32_t v = At<uint32_t>((uintptr_t)Grp(g) + 0x10 + 4 * (size_t)k); if (v) c[n++] = (int)((v - IMA) / kIntSz); }
    return n ? c[rr.below(n)] : -1;
}
static void SetPedPos(int k, const CVector& pos) {
    using namespace w3i;
    uint8_t* p = S(oPed + (size_t)k * 0x40);
    std::memcpy(p + 4, &pos, 12);
    const uint32_t m = At<uint32_t>((uintptr_t)p + offsetof(CPlaceable, m_matrix));
    if (m) std::memcpy(reinterpret_cast<void*>(m + 0x30), &pos, 12);
}
static Interior_c* g_retInt;    // an interior that is not in the interior pool
static int g_nInfos;
static int g_soIdx;             // a stealable-object table entry (or -1)
static void PickPoint(Rng& rr, int interiorHint) {   // mostly inside an interior (of the hinted group / any), sometimes anywhere
    const int ii = interiorHint >= 0 ? interiorHint : (int)rr.below(8);
    g_pt = rr.below(10) < 7 ? w3i::InteriorLocal(rr, ii, rr.below(4) == 0 ? 6.f : 1.f) : CVector(w3i::Fl(rr, 3000.f, 30), w3i::Fl(rr, 3000.f, 30), w3i::Fl(rr, 100.f, 30));
}
static void ZeroPool(Rng& rr) {   // an interior that is in no list
    using namespace w3i;
    std::vector<uint8_t*> pool;
    for (uint32_t p = At<uint32_t>(IMA + 0x3CA0); p; p = At<uint32_t>(p + 4)) pool.push_back(reinterpret_cast<uint8_t*>(p));
    std::vector<Interior_c*> cand;
    for (int i = 0; i < 8; ++i) { bool in = false; for (auto* q : pool) in |= q == reinterpret_cast<uint8_t*>(Int(i)); if (!in) cand.push_back(Int(i)); }
    if (cand.empty()) { PutList(IMp(0x3CA0), {}); cand.push_back(Int(0)); }
    g_retInt = cand[(size_t)rr.below((int)cand.size())];
}

static void TestW3InteriorB() {
    using namespace w2; using namespace w3i;
    // ---- InteriorGroup_c
    ROW("InteriorGroup_c::Init", 0x5947E0, 300) {
        const int g = r.below(8), k = r.below(9), id = r.below(5) == 0 ? (int)r.below(100000) : r.below(300);
        IC(Grp(g)->Init(k == 8 ? nullptr : Ent(k), id); return 0;, ExeT<void>(0x5947E0, Grp(g), (void*)(k == 8 ? nullptr : Ent(k)), id); return 0;);
    } ENDROW;
    ROW("InteriorGroup_c::AddInterior", 0x594840, 400) {
        const int g = r.below(8), i = r.below(8);
        IC(Grp(g)->AddInterior(Int(i)); return 0;, ExeT<void>(0x594840, Grp(g), (void*)Int(i)); return 0;);
    } ENDROW;
    ROW("InteriorGroup_c::GetNumInteriorInfos", 0x594920, 500) {
        const int g = r.below(8), ty = r.below(8) == 0 ? (int)r.below(300) - 20 : r.below(11);
        IC(return (uint64_t)(uint32_t)Grp(g)->GetNumInteriorInfos(ty);, return (uint64_t)ExeT<uint32_t>(0x594920, Grp(g), ty););
    } ENDROW;
    ROW("InteriorGroup_c::FindClosestInteriorInfo", 0x594A50, 1200) {
        const int g = r.below(8), ty = r.below(4) == 0 ? -1 : r.below(8) == 0 ? (int)r.below(300) : 1 + r.below(10);
        Tw tw([g](Rng& rr) {
            PickPoint(rr, GrpInt(rr, g));
            g_rad = rr.below(12) == 0 ? Fl(rr, 20.f, 1) : 0.5f + rr.f01() * 12.f;
        });
        IC(return B8(Grp(g)->FindClosestInteriorInfo(ty, g_pt, g_rad, reinterpret_cast<InteriorInfo_t**>(S(oOut)), reinterpret_cast<Interior_c**>(S(oOut + 4)), reinterpret_cast<float*>(S(oOut + 8))));,
           return (uint64_t)ExeT<uint8_t>(0x594A50, Grp(g), ty, g_pt.x, g_pt.y, g_pt.z, g_rad, (void*)S(oOut), (void*)S(oOut + 4), (void*)S(oOut + 8)););
    } ENDROW;
    ROW("InteriorGroup_c::GetPed", 0x594B90, 200) {
        const int g = r.below(8), i = r.below(16);
        IC(return U(Grp(g)->GetPed(i));, return (uint64_t)ExeT<uint32_t>(0x594B90, Grp(g), i););
    } ENDROW;
    ROW("InteriorGroup_c::GetEntity", 0x594BD0, 100) {
        const int g = r.below(8);
        IC(return U(Grp(g)->GetEntity());, return (uint64_t)ExeT<uint32_t>(0x594BD0, Grp(g)););
    } ENDROW;
    ROW("InteriorGroup_c::ContainsInteriorType", 0x595250, 500) {
        const int g = r.below(8), ty = r.below(10) == 0 ? (int)r.below(600) - 30 : r.below(5);
        IC(return B8(Grp(g)->ContainsInteriorType(ty));, return (uint64_t)ExeT<uint8_t>(0x595250, Grp(g), ty););
    } ENDROW;
    ROW("InteriorGroup_c::CalcIsVisible", 0x595200, 800) {
        const int g = r.below(8);
        Tw tw([g](Rng& rr) {   // the first m_numInteriors slots are interiors (the game's groups are filled from the front)
            uint8_t* p = reinterpret_cast<uint8_t*>(Grp(g));
            for (int k = 0; k < p[0xF] && k < 8; ++k) if (!At<uint32_t>((uintptr_t)p + 0x10 + 4 * (size_t)k)) At<uint32_t>((uintptr_t)p + 0x10 + 4 * (size_t)k) = P32(Int(rr.below(8)));
            if (rr.below(3)) { const int ii = GrpInt(rr, g); if (ii >= 0) TheCamera.m_placement.m_vPosn = InteriorLocal(rr, ii, 7.f); }
        });
        IC(Grp(g)->CalcIsVisible(); return 0;, ExeT<void>(0x595200, Grp(g)); return 0;);
    } ENDROW;

    // ---- InteriorManager_c
    ROW("InteriorManager_c::ActivatePeds", 0x598080, 40) { const int b = r.below(2); IC(Man()->ActivatePeds(b != 0); return 0;, ExeT<void>(0x598080, Man(), b); return 0;); } ENDROW;
    ROW("InteriorManager_c::SetEntryExitPtr", 0x598180, 800) {
        const int k = r.below(4);
        Tw tw([](Rng& rr) {   // area: the linked entry's area decides; the rectangle equals the manager's one in a third of the cases
            if (rr.below(3) == 0) { const int e = (int)rr.below(4); std::memcpy(IMp(0x4740), S(oEnex + (size_t)e * 0x40) + offsetof(CEntryExit, m_recEntrance), 16); }
            if (rr.below(10) == 0) { float nan = std::numeric_limits<float>::quiet_NaN(); std::memcpy(IMp(0x4740) + 4 * rr.below(4), &nan, 4); std::memcpy(S(oEnex) + offsetof(CEntryExit, m_recEntrance) + 4 * rr.below(4), &nan, 4); }
        });
        IC(Man()->SetEntryExitPtr(Enex(k)); return 0;, ExeT<void>(0x598180, Man(), (void*)Enex(k)); return 0;);
    } ENDROW;
    ROW("InteriorManager_c::GetPedsInteriorGroup", 0x598240, 500) {
        const int k = r.below(5);
        IC(return U(Man()->GetPedsInteriorGroup(k == 4 ? reinterpret_cast<CPed*>(S(oOut + 0x40)) : Ped(k)));, return (uint64_t)ExeT<uint32_t>(0x598240, Man(), (void*)(k == 4 ? S(oOut + 0x40) : (uint8_t*)Ped(k))););
    } ENDROW;
    ROW("InteriorManager_c::IsGroupActive", 0x598280, 400) {
        const int ty = r.below(8) == 0 ? (int)r.below(700) - 100 : r.below(5);
        IC(return (uint64_t)(uint8_t)Man()->IsGroupActive(ty);, return (uint64_t)ExeT<uint8_t>(0x598280, Man(), ty););
    } ENDROW;
    ROW("InteriorManager_c::HasInteriorHadStealDataSetup", 0x5982B0, 500) {
        const int i = r.below(8);
        IC(return B8(Man()->HasInteriorHadStealDataSetup(Int(i)));, return (uint64_t)ExeT<uint8_t>(0x5982B0, Man(), (void*)Int(i)););
    } ENDROW;
    ROW("InteriorManager_c::FindStealableObjectId(int, int, CVector)", 0x5982F0, 800) {
        Tw tw([](Rng& rr) {   // arguments: an entry of the table (hit) or a near miss
            g_soIdx = (int)rr.below(32);
            uint8_t* o = IMp(0x43AC + (size_t)g_soIdx * 0x1C);
            std::memcpy(&g_bx, o + 0xC, 4); std::memcpy(&g_by, o + 0x10, 4); std::memcpy(&g_bz, o + 0x14, 4);
            if (rr.below(3) == 0) { static const float c[] = { 0.f, -0.f, 1.5f, -2.5f, 100.25f, 3000.f, std::numeric_limits<float>::quiet_NaN() }; g_by = c[rr.below(7)]; }
        });
        const int dm = r.below(4), di = r.below(4);
        IC(const auto* o = IMp(0x43AC + (size_t)g_soIdx * 0x1C); return (uint64_t)(uint32_t)Man()->FindStealableObjectId(At<int32_t>((uintptr_t)o + 8) + (di == 0), At<int32_t>((uintptr_t)o + 4) + (dm == 0), CVector(g_bx, g_by, g_bz));,
           const auto* o = IMp(0x43AC + (size_t)g_soIdx * 0x1C); return (uint64_t)ExeT<uint32_t>(0x5982F0, Man(), At<int32_t>((uintptr_t)o + 8) + (di == 0), At<int32_t>((uintptr_t)o + 4) + (dm == 0), g_bx, g_by, g_bz););
    } ENDROW;
    ROW("InteriorManager_c::FindStealableObjectId(CEntity*)", 0x598360, 500) {
        const int k = r.below(9);
        IC(return (uint64_t)(uint32_t)Man()->FindStealableObjectId(k == 8 ? nullptr : Ent(k));, return (uint64_t)ExeT<uint32_t>(0x598360, Man(), (void*)(k == 8 ? nullptr : Ent(k))););
    } ENDROW;
    ROW("InteriorManager_c::SetStealableObjectStolen", 0x598390, 500) {
        const int k = r.below(9), st = r.below(2);
        IC(Man()->SetStealableObjectStolen(k == 8 ? nullptr : Ent(k), (uint8)st); return 0;, ExeT<void>(0x598390, Man(), (void*)(k == 8 ? nullptr : Ent(k)), st); return 0;);
    } ENDROW;
    ROW("InteriorManager_c::GetVectorsInterior", 0x5983D0, 1000) {
        Tw tw([](Rng& rr) { PickPoint(rr, -1); std::memcpy(S(oOut + 0x20), &g_pt, 12); });
        IC(return U(Man()->GetVectorsInterior(*reinterpret_cast<CVector*>(S(oOut + 0x20))));, return (uint64_t)ExeT<uint32_t>(0x5983D0, Man(), (void*)S(oOut + 0x20)););
    } ENDROW;
    ROW("InteriorManager_c::GetInteriorFromPool", 0x5984A0, 200) { IC(return U(Man()->GetInteriorFromPool());, return (uint64_t)ExeT<uint32_t>(0x5984A0, Man());); } ENDROW;
    ROW("InteriorManager_c::ReturnInteriorToPool", 0x5984B0, 200) {
        Tw tw([](Rng& rr) { ZeroPool(rr); });
        IC(Man()->ReturnInteriorToPool(g_retInt); return 0;, ExeT<void>(0x5984B0, Man(), (void*)g_retInt); return 0;);
    } ENDROW;
    ROW("InteriorManager_c::GetPedsInterior", 0x598620, 800) {
        const int k = r.below(4);
        IC(return U(Man()->GetPedsInterior(Ped(k)));, return (uint64_t)ExeT<uint32_t>(0x598620, Man(), (void*)Ped(k)););
    } ENDROW;
    ROW("InteriorManager_c::IsInteriorEffectVisible", 0x598690, 1500) {
        const int e = r.below(8), b = r.below(16);
        Tw tw([e](Rng& rr) {   // camera / players inside or near the entity's bounding box (in the entity's frame) in most cases
            const auto* mi = reinterpret_cast<const CBaseModelInfo*>(*(reinterpret_cast<void**>(0xA9B0C8) + At<uint16_t>((uintptr_t)Ent(e) + offsetof(CEntity, m_nModelIndex))));
            const auto& bb = mi->m_pColModel->m_boundBox;
            auto* M = reinterpret_cast<CMatrix*>(S(oMat + (size_t)e * 0x50));
            auto Near = [&]() {
                const float u = -0.2f + rr.f01() * 1.4f, v = -0.2f + rr.f01() * 1.4f, w = -0.2f + rr.f01() * 1.4f;
                return M->TransformPoint(CVector(bb.m_vecMin.x + (bb.m_vecMax.x - bb.m_vecMin.x) * u, bb.m_vecMin.y + (bb.m_vecMax.y - bb.m_vecMin.y) * v, bb.m_vecMin.z + (bb.m_vecMax.z - bb.m_vecMin.z) * w));
            };
            if (rr.below(3)) TheCamera.m_placement.m_vPosn = Near();
            if (rr.below(3) == 0) SetPedPos(0, Near());
            if (rr.below(3) == 0) SetPedPos(1, Near());
            if (rr.below(25) == 0) { float v = std::numeric_limits<float>::quiet_NaN(); std::memcpy(&TheCamera.m_placement.m_vPosn.x + rr.below(3), &v, 4); }
        });
        IC(return B8(Man()->IsInteriorEffectVisible(Box(b), Ent(e)));, return (uint64_t)ExeT<uint8_t>(0x598690, Man(), (void*)Box(b), (void*)Ent(e)););
    } ENDROW;
    ROW("InteriorManager_c::PruneVisibleEffects", 0x598A60, 1200) {
        const int reqd = r.below(12) == 0 ? (int)r.below(40) : r.below(14), cap = r.below(14);
        Tw tw([cap](Rng& rr) {
            g_nInfos = cap; g_rad = rr.below(12) == 0 ? Fl(rr, 40.f, 1) : (rr.below(8) == 0 ? 0.f : 3.f + rr.f01() * 80.f);
            for (int k = 0; k < 16; ++k) {
                uint8_t* x = S(oFx + (size_t)k * 0x50);
                const uint32_t en = P32(Ent(rr.below(8))); std::memcpy(x, &en, 4);
                const uint32_t nf = rr.below(8) == 0 ? 0 : 1 + rr.below(rr.below(4) ? 3 : 8); std::memcpy(x + 4, &nf, 4);
                for (int j = 0; j < 8; ++j) { const uint32_t fx = P32(Box(rr.below(16))); std::memcpy(x + 8 + 4 * j, &fx, 4); }
                x[0x4C] = (uint8_t)rr.below(2);
            }
            if (rr.below(2)) TheCamera.m_placement.m_vPosn = CVector(FlP(rr, 600.f), FlP(rr, 600.f), FlP(rr, 50.f));
        });
        IC(Man()->PruneVisibleEffects(reinterpret_cast<InteriorEffectInfo_t*>(S(oFx)), (size_t)g_nInfos, (size_t)reqd, g_rad); return 0;,
           ExeT<void>(0x598A60, Man(), (void*)S(oFx), g_nInfos, reqd, g_rad); return 0;);
    } ENDROW;
}
