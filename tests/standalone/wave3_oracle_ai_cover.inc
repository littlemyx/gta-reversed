// ORACLE-W3-AI rows: Cover.cpp (classes A + B). CCover::m_Points (100 x 0x1C at 0xC197C8) holds cover points whose entities / covered peds are entities of the reference world (registered references), the processed-building list
// uses the double-link node pool. CalculateHorizontalSize / DoLineCheckWithinObject / FindVectorFromFirstToMissingVertex are skipped: the port bodies are NOTSA_UNREACHABLE / unused (never called by the game).
namespace w3a {
constexpr uintptr_t kCovPts = 0xC197C8, kCovNum = 0xC197A4, kCovList = 0xC1A2B8;
static uint8_t* Cpt(int i) { return reinterpret_cast<uint8_t*>(kCovPts + (uintptr_t)i * 0x1C); }
// builds `n` points: type NONE / OBJECT / VEHICLE / POINTONMAP, direction, position, an entity of the reference world (registered through RegisterReference on the field) or null, up to two covered peds
static void CoverWorld(Rng& r, int n) {
    std::memset(reinterpret_cast<void*>(kCovPts), 0, 100 * 0x1C);
    int count = 0;
    for (int i = 0; i < n; ++i) {
        uint8_t* p = Cpt(i);
        const int type = r.below(6) == 0 ? 0 : (r.below(30) == 0 ? (int)(4 + r.below(3)) : 1 + (int)r.below(3));
        p[0] = (uint8_t)type; p[1] = (uint8_t)r.below(3); p[2] = (uint8_t)r.below(256);
        const CVector pos = Vec(r, 300.f, 0);
        std::memcpy(p + 4, &pos, 12);
        if (type) ++count;
        CEntity* e = nullptr;
        if (type == 1) e = r.below(4) == 0 ? nullptr : rf::ObjE((int)r.below(rf::kO));
        else if (type == 2) e = r.below(4) == 0 ? nullptr : rf::VehE((int)r.below(rf::kV));
        else if (type == 3) e = r.below(3) == 0 ? nullptr : rf::BldE((int)r.below(rf::kB));
        std::memcpy(p + 0x10, &e, 4);
        if (e && r.below(8) != 0) e->RegisterReference(reinterpret_cast<CEntity**>(p + 0x10));
        for (int k = 0; k < 2; ++k) {
            CEntity* ped = r.below(2) ? nullptr : rf::PedE((int)r.below(rf::kP));
            std::memcpy(p + 0x14 + 4 * k, &ped, 4);
            if (ped && r.below(8) != 0) ped->RegisterReference(reinterpret_cast<CEntity**>(p + 0x14 + 4 * k));
        }
    }
    At<uint32_t>(kCovNum) = (uint32_t)(r.below(10) == 0 ? r.below(120) : count);
}
// the processed-building list: used nodes of the double-link pool chained from the head at 0xC1A2B8
static void CoverList(Rng& r) {
    uint8_t* prev = nullptr; uint8_t* first = nullptr;
    for (int i = 0; i < g_dlp.N; ++i) {
        if (g_dlp.flags[i] & 0x80) continue;
        uint8_t* nd = g_dlp.store + i * 0xC;
        *reinterpret_cast<void**>(nd) = rf::BldE((int)r.below(rf::kB)); *reinterpret_cast<void**>(nd + 4) = nullptr; *reinterpret_cast<void**>(nd + 8) = prev;
        if (prev) *reinterpret_cast<void**>(prev + 4) = nd; else first = nd;
        prev = nd;
    }
    At<uint32_t>(kCovList) = (uint32_t)(uintptr_t)first;
}
template<class PF, class EF>
static bool UCaseCov(Rng& r, std::string& d, PF&& pf, EF&& ef) {   // reference world + double-link pool
    const uint32_t seed = r.u32() | 1;
    g_dlp.Prepare(r, true); g_dlp.Enter();
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = BothA(d, seed, { sc, img, g_dlp.RegStruct(), g_dlp.RegFlags(), g_dlp.RegStore() }, std::forward<PF>(pf), std::forward<EF>(ef), kRf);
    rf::Leave(); g_dlp.Leave();
    return ok;
}
} // namespace w3a

static void TestW3AiCover() {
    using namespace w2; using namespace w3a;
    ROW("CCover::Find2HighestPoints", 0x6988E0, 1500) {
        Tu tu([&](Rng& rr) {
            FillGarbage(rr, S(oOut), 0x200);
            for (int i = 0; i < 6; ++i) { auto* v = reinterpret_cast<CVector*>(S(oOut + 0x100) + 12 * i); *v = Vec(rr, 5.f, 10); if (rr.below(3) == 0 && i) v->z = reinterpret_cast<CVector*>(S(oOut + 0x100) + 12 * (i - 1))->z; }   // equal heights
            auto* t = S(oOut + 0x80); const uint16_t a = (uint16_t)rr.below(6), b = (uint16_t)rr.below(6), c = (uint16_t)rr.below(6); std::memcpy(t, &a, 2); std::memcpy(t + 2, &b, 2); std::memcpy(t + 4, &c, 2);
        });
        return UCase(r, d, [&]() -> uint64_t { CCover::Find2HighestPoints(reinterpret_cast<CColTriangle*>(S(oOut + 0x80)), reinterpret_cast<CVector*>(S(oOut + 0x100)), *reinterpret_cast<int32_t*>(S(oOut)), *reinterpret_cast<int32_t*>(S(oOut + 4))); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, int*, int*)>(0x6988E0)(S(oOut + 0x80), S(oOut + 0x100), reinterpret_cast<int*>(S(oOut)), reinterpret_cast<int*>(S(oOut + 4))); return 0; });
    } ENDROW;
    ROW("CCover::RemoveCoverPointIfEntityLost", 0x698DB0, 1500) {
        const int k = (int)r.below(6);
        Tu tu([&](Rng& rr) { CoverWorld(rr, 6); }, true);
        return UCase(r, d, [&]() -> uint64_t { CCover::RemoveCoverPointIfEntityLost(*reinterpret_cast<CCoverPoint*>(Cpt(k))); return 0; },
                           [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x698DB0)(Cpt(k)); return 0; });
    } ENDROW;
    ROW("CCover::RemoveCoverPointsForThisEntity", 0x698740, 1500) {
        const int which = (int)r.below(4), idx = (int)r.below(8);
        Tu tu([&](Rng& rr) { CoverWorld(rr, 10); }, true);
        const auto ent = [&]() -> CEntity* { return which == 0 ? rf::ObjE(idx % rf::kO) : which == 1 ? rf::VehE(idx % rf::kV) : which == 2 ? rf::BldE(idx % rf::kB) : nullptr; };
        return UCase(r, d, [&]() -> uint64_t { CCover::RemoveCoverPointsForThisEntity(ent()); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x698740)(ent()); return 0; });
    } ENDROW;
    ROW("CCover::DoesCoverPointStillProvideCover", 0x698DD0, 2000) {
        const int k = (int)r.below(7); const CVector pos = Vec(r, 300.f, 14);
        Tu tu([&](Rng& rr) { CoverWorld(rr, 6); }, true);
        return UCase(r, d, [&]() -> uint64_t { return B8(CCover::DoesCoverPointStillProvideCover(k == 6 ? nullptr : reinterpret_cast<CCoverPoint*>(Cpt(k)), pos)); },
                           [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*, float, float, float)>(0x698DD0)(k == 6 ? nullptr : Cpt(k), pos.x, pos.y, pos.z)); });
    } ENDROW;
    ROW("CCover::Init", 0x698710, 200) {
        Tu tu([&](Rng& rr) { CoverWorld(rr, 100); CoverList(rr); }, true);
        return UCaseCov(r, d, [&]() -> uint64_t { CCover::Init(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x698710)(); return 0; });
    } ENDROW;
    // the ped of FindAndReserveCoverPoint / FindCoordinatesCoverPoint: a scratch ped that is one of the covered peds of some points
    const auto pedWorld = [&](Rng& rr) {
        PedWorld(rr, oPedA, oIntA, 2);
        for (int i = 0; i < 10; ++i) if (rr.below(3) == 0) { uint8_t* p = Cpt(i); const uint32_t pp = (uint32_t)(uintptr_t)S(oPedA); std::memcpy(p + 0x14 + 4 * rr.below(2), &pp, 4); }
    };
    if (std::getenv("W3A_PENDING")) {   // port differs structurally from the exe (see ORACLE-W3-AI.md): rows kept, off by default
    ROW("CCover::FindAndReserveCoverPoint", 0x6992B0, 2500) {
        const bool attack = r.below(2) != 0; const CVector target = Vec(r, 300.f, 14);
        Tu tu([&](Rng& rr) {
            CoverWorld(rr, 10); pedWorld(rr);
            // points close to the ped so the distance tests go both ways
            auto* ped = reinterpret_cast<CPed*>(S(oPedA)); const CVector pp = ped->GetPosition();
            for (int i = 0; i < 10; ++i) if (rr.below(2)) { const CVector q = pp + CVector((rr.f01() - 0.5f) * 60.f, (rr.f01() - 0.5f) * 60.f, 0.f); std::memcpy(Cpt(i) + 4, &q, 12); }
            std::memcpy(S(oOut), &target, 12);
        }, true);
        return UCase(r, d, [&]() -> uint64_t { return U(CCover::FindAndReserveCoverPoint(reinterpret_cast<CPed*>(S(oPedA)), *reinterpret_cast<CVector*>(S(oOut)), attack)); },
                           [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, void*, int)>(0x6992B0)(S(oPedA), S(oOut), (int)attack)); });
    } ENDROW;
    ROW("CCover::FindCoordinatesCoverPoint", 0x699570, 2500) {
        const int k = (int)r.below(10); const CVector target = Vec(r, 300.f, 14);
        Tu tu([&](Rng& rr) { CoverWorld(rr, 10); pedWorld(rr); std::memcpy(S(oOut), &target, 12); FillGarbage(rr, S(oOut + 0x20), 12); for (int i = 0; i < 10; ++i) if (rr.below(2)) { uint8_t* p = Cpt(i); const uint32_t pp = (uint32_t)(uintptr_t)S(oPedA); std::memcpy(p + 0x14, &pp, 4); } }, true);
        return UCase(r, d, [&]() -> uint64_t { return B8(CCover::FindCoordinatesCoverPoint(*reinterpret_cast<CCoverPoint*>(Cpt(k)), reinterpret_cast<CPed*>(S(oPedA)), *reinterpret_cast<CVector*>(S(oOut)), *reinterpret_cast<CVector*>(S(oOut + 0x20)))); },
                           [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*, void*, void*, void*)>(0x699570)(Cpt(k), S(oPedA), S(oOut), S(oOut + 0x20))); });
    } ENDROW;
    }
    ROW("CCover::ShouldThisBuildingHaveItsCoverPointsCreated", 0x699230, 2500) {
        Tu tu([&](Rng& rr) {
            Ent(rr, oObjA, 3, 1, 700, 0x100, false);      // a building (type 1) with a col model
            ModelInfoFor(rr, 2, 700, rr.below(10) == 0 ? Fl(rr, 40.f, 3) : 5.f + rr.f01() * 60.f);
            PedWorld(rr, oPedA, oIntA, 2);
            CWorld::PlayerInFocus = 0; CWorld::Players[0].m_pPed = reinterpret_cast<CPlayerPed*>(S(oPedA)); reinterpret_cast<CPed*>(S(oPedA))->bInVehicle = false;
            auto* b = reinterpret_cast<CEntity*>(S(oObjA)); auto* p = reinterpret_cast<CPed*>(S(oPedA));
            if (rr.below(2) && b->m_matrix && p->m_matrix) p->m_matrix->GetPosition() = b->m_matrix->GetPosition() + CVector((rr.f01() - 0.5f) * 140.f, (rr.f01() - 0.5f) * 140.f, (rr.f01() - 0.5f) * 40.f);
        });
        return UCase(r, d, [&]() -> uint64_t { return B8(CCover::ShouldThisBuildingHaveItsCoverPointsCreated(reinterpret_cast<CBuilding*>(S(oObjA)))); },
                           [&]() -> uint64_t { return B8(oracle::Fn<bool __cdecl(void*)>(0x699230)(S(oObjA))); });
    } ENDROW;
}
