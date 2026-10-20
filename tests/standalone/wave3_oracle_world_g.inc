// ORACLE-W3-PATHS rows G: CWorld sector-list templates (CPtrListDoubleLink<CPhysical*>) on a fixture of fake entities with positions, model indices, scan codes and col-model bounding radii (see wave3_oracle_world.inc).
// Fixture: 24 entity slots of 0x800 bytes at g_sc+0x8000 (garbage, then position / matrix / model index / scan code / creator fields set), 8 fake model infos + col models at g_sc+0x1FA00.., a double-link list of 0..16 nodes
// (the same entity may occur twice: the scan code dedupes it) at g_sc+0x15000. The exe is called cdecl with the list object (a pointer to its first node) as its first reference argument.
// Included from wave3_oracle_world_d.inc.
namespace w3g {
using namespace w3w; using namespace w2;
constexpr size_t oEnt = 0x8000, oNode = 0x15000, oList = 0x16000, oCol = 0x1FA00, oMi = 0x1FC00, oRes = 0x16100;
constexpr int kEnt = 24;
static CEntity* EntP(int k) { return reinterpret_cast<CEntity*>(S(oEnt + (size_t)k * 0x800)); }
using List = CPtrListDoubleLink<CObject*>;   // (World.cpp instantiates the templates for the typed sector lists, not for CPhysical*)
static List& ListR() { return *reinterpret_cast<List*>(S(oList)); }
struct PedAcc : CPed { static size_t Off() { return offsetof(PedAcc, m_nCreatedBy); } };
static size_t PedCreatedByOff() { return PedAcc::Off(); }
// entities around `centre` within `spread`
static void BuildSectors(Rng& r, const CVector& centre, float spread) {
    for (int m = 0; m < 8; ++m) {
        uint8_t* cm = S(oCol + (size_t)m * 0x40);
        reinterpret_cast<CColModel*>(cm)->m_boundSphere.m_fRadius = r.below(8) == 0 ? Fl(r, 20.f, 1) : r.f01() * 25.f;
        uint8_t* mi = S(oMi + (size_t)m * 0x40);
        const uint32_t cmp = P32(cm); std::memcpy(mi + 20, &cmp, 4);          // CBaseModelInfo::m_pColModel
        CModelInfo::ms_modelInfoPtrs[m] = reinterpret_cast<CBaseModelInfo*>(mi);
    }
    for (int k = 0; k < kEnt; ++k) {
        CEntity* e = EntP(k);
        e->m_nModelIndex = (uint16)r.below(8);
        e->SetTypeObject();
        e->SetScanCode(r.below(4) == 0 ? CWorld::ms_nCurrentScanCode : (uint16)r.u32());
        const CVector pos(centre.x + (r.f01() * 2 - 1) * spread, centre.y + (r.f01() * 2 - 1) * spread, centre.z + (r.f01() * 2 - 1) * spread);
        if (r.below(2) || k >= 14) { e->m_matrix = nullptr; e->m_placement.m_vPosn = pos; }
        else { uint8_t* m = MatP(k); std::memcpy(m + 0x30, &pos, 12); e->m_matrix = reinterpret_cast<CMatrixLink*>(m); }
        static_cast<CVehicle*>(e)->m_nCreatedBy = (eVehicleCreatedBy)(r.below(5));
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(e) + PedCreatedByOff()) = (uint8_t)(r.below(3));
        static_cast<CObject*>(e)->m_nObjectType = (uint8)r.below(8);
    }
    const int n = r.below(8) == 0 ? 0 : 1 + (int)r.below(16);
    uint32_t* nodes = reinterpret_cast<uint32_t*>(S(oNode));
    for (int i = 0; i < n; ++i) {
        nodes[i * 3 + 0] = P32(EntP(r.below(kEnt)));
        nodes[i * 3 + 1] = i + 1 < n ? P32(&nodes[(i + 1) * 3]) : 0;
        nodes[i * 3 + 2] = i ? P32(&nodes[(i - 1) * 3]) : 0;
    }
    const uint32_t head = n ? P32(&nodes[0]) : 0; std::memcpy(S(oList), &head, 4);
}
// entity scatter around the query point: tiny (denormal range) distances differ between the x87 (extended exponent range) and the float code, and are no game values
static float Spread(float rad) { return (rad > 1e-3f && rad < 1e6f) ? rad * 1.2f : 50.f; }
static CVector QPos(Rng& r) { return CVector(GF(r, 2500.f), GF(r, 2500.f), GF(r, 80.f)); }
} // namespace w3g

static void TestW3WorldG() {
    using namespace w2; using namespace w3w; using namespace w3g;
    constexpr size_t oCnt = w3g::oRes, oOutE = w3g::oRes + 0x10;
    // common random query: point P, radius, b2D, max count, output buffer (null 1/6)
    ROW("CWorld::FindObjectsInRangeSectorList", 0x563500, 1500) {
        const CVector P = QPos(r); const float rad = r.below(10) == 0 ? Fl(r, 100.f, 1) : 5.f + r.f01() * 120.f; const bool b2D = r.below(2);
        const int16_t maxC = (int16_t)(r.below(8) == 0 ? (int)r.below(3) : 1 + (int)r.below(20)); const bool noOut = r.below(6) == 0; const uint32_t sc = r.u32();
        Tw tw([&](Rng& rr) { CWorld::ms_nCurrentScanCode = (uint16)sc; BuildSectors(rr, P, Spread(rad)); });
        const auto reset = [&] { std::memset(S(oCnt), 0xCD, 0x100); SAt<int16_t>(oCnt) = 0; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset(); CWorld::FindObjectsInRangeSectorList(ListR(), P, rad, b2D, &SAt<int16_t>(oCnt), maxC, noOut ? nullptr : reinterpret_cast<CEntity**>(S(oOutE))); return (uint16_t)SAt<int16_t>(oCnt);
        }, [&]() -> uint64_t {
            reset(); oracle::Fn<void __cdecl(void*, const CVector*, float, uint32_t, int16_t*, uint32_t, void*)>(0x563500)(S(oList), &P, rad, (uint32_t)b2D, &SAt<int16_t>(oCnt), (uint32_t)(int32_t)maxC, noOut ? nullptr : S(oOutE)); return (uint16_t)SAt<int16_t>(oCnt);
        });
    } ENDROW;
    ROW("CWorld::FindObjectsOfTypeInRangeSectorList", 0x5635C0, 1500) {
        const CVector P = QPos(r); const float rad = r.below(10) == 0 ? Fl(r, 100.f, 1) : 5.f + r.f01() * 120.f; const bool b2D = r.below(2); const uint32_t model = r.below(10) == 0 ? r.u32() : r.below(8);
        const int16_t maxC = (int16_t)(r.below(8) == 0 ? (int)r.below(3) : 1 + (int)r.below(20)); const bool noOut = r.below(6) == 0; const uint32_t sc = r.u32();
        Tw tw([&](Rng& rr) { CWorld::ms_nCurrentScanCode = (uint16)sc; BuildSectors(rr, P, Spread(rad)); });
        const auto reset = [&] { std::memset(S(oCnt), 0xCD, 0x100); SAt<int16_t>(oCnt) = 0; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset(); CWorld::FindObjectsOfTypeInRangeSectorList(model, ListR(), P, rad, b2D, &SAt<int16_t>(oCnt), maxC, noOut ? nullptr : reinterpret_cast<CEntity**>(S(oOutE))); return (uint16_t)SAt<int16_t>(oCnt);
        }, [&]() -> uint64_t {
            reset(); oracle::Fn<void __cdecl(uint32_t, void*, const CVector*, float, uint32_t, int16_t*, uint32_t, void*)>(0x5635C0)(model, S(oList), &P, rad, (uint32_t)b2D, &SAt<int16_t>(oCnt), (uint32_t)(int32_t)maxC, noOut ? nullptr : S(oOutE)); return (uint16_t)SAt<int16_t>(oCnt);
        });
    } ENDROW;
    ROW("CWorld::FindNearestObjectOfTypeSectorList", 0x565450, 1500) {
        const CVector P = QPos(r); const float rad = r.below(10) == 0 ? Fl(r, 100.f, 1) : 5.f + r.f01() * 120.f; const bool b2D = r.below(2); const int32_t model = r.below(5) == 0 ? -1 - (int32_t)r.below(3) : (int32_t)r.below(8); const uint32_t sc = r.u32();
        Tw tw([&](Rng& rr) { CWorld::ms_nCurrentScanCode = (uint16)sc; BuildSectors(rr, P, Spread(rad)); });
        const auto reset = [&] { std::memset(S(oCnt), 0xCD, 0x100); SAt<uint32_t>(oCnt) = 0; SAt<float>(oCnt + 4) = 1234.5f; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset(); CWorld::FindNearestObjectOfTypeSectorList(model, ListR(), P, rad, b2D, SAt<CEntity*>(oCnt), SAt<float>(oCnt + 4)); return FB(SAt<float>(oCnt + 4)) ^ (uint64_t)SAt<uint32_t>(oCnt) << 32;
        }, [&]() -> uint64_t {
            reset(); oracle::Fn<void __cdecl(int32_t, void*, const CVector*, float, uint32_t, void*, void*)>(0x565450)(model, S(oList), &P, rad, (uint32_t)b2D, S(oCnt), S(oCnt + 4)); return FB(SAt<float>(oCnt + 4)) ^ (uint64_t)SAt<uint32_t>(oCnt) << 32;
        });
    } ENDROW;
    ROW("CWorld::FindObjectsIntersectingCubeSectorList", 0x5650E0, 1500) {
        const CVector A = QPos(r); const CVector B(A.x + r.f01() * 150.f, A.y + r.f01() * 150.f, A.z + r.f01() * 60.f); const CVector mn = r.below(8) ? A : CVector(Fl(r, 100.f, 1), A.y, A.z);
        const int16_t maxC = (int16_t)(r.below(8) == 0 ? (int)r.below(3) : 1 + (int)r.below(20)); const bool noOut = r.below(6) == 0; const uint32_t sc = r.u32();
        Tw tw([&](Rng& rr) { CWorld::ms_nCurrentScanCode = (uint16)sc; BuildSectors(rr, CVector((A.x + B.x) / 2, (A.y + B.y) / 2, (A.z + B.z) / 2), 120.f); });
        const auto reset = [&] { std::memset(S(oCnt), 0xCD, 0x100); SAt<int16_t>(oCnt) = 0; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset(); CWorld::FindObjectsIntersectingCubeSectorList(ListR(), mn, B, &SAt<int16_t>(oCnt), maxC, noOut ? nullptr : reinterpret_cast<CEntity**>(S(oOutE))); return (uint16_t)SAt<int16_t>(oCnt);
        }, [&]() -> uint64_t {
            reset(); oracle::Fn<void __cdecl(void*, const CVector*, const CVector*, int16_t*, uint32_t, void*)>(0x5650E0)(S(oList), &mn, &B, &SAt<int16_t>(oCnt), (uint32_t)(int32_t)maxC, noOut ? nullptr : S(oOutE)); return (uint16_t)SAt<int16_t>(oCnt);
        });
    } ENDROW;
    ROW("CWorld::FindMissionEntitiesIntersectingCubeSectorList", 0x565300, 1500) {
        const CVector A = QPos(r); const CVector B(A.x + r.f01() * 150.f, A.y + r.f01() * 150.f, A.z + r.f01() * 60.f);
        const int kind = (int)r.below(3);   // 0 vehicles list, 1 peds list, 2 objects list
        const int16_t maxC = (int16_t)(r.below(8) == 0 ? (int)r.below(3) : 1 + (int)r.below(20)); const bool noOut = r.below(6) == 0; const uint32_t sc = r.u32();
        Tw tw([&](Rng& rr) { CWorld::ms_nCurrentScanCode = (uint16)sc; BuildSectors(rr, CVector((A.x + B.x) / 2, (A.y + B.y) / 2, (A.z + B.z) / 2), 120.f); });
        const auto reset = [&] { std::memset(S(oCnt), 0xCD, 0x100); SAt<int16_t>(oCnt) = 0; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset(); CWorld::FindMissionEntitiesIntersectingCubeSectorList(ListR(), A, B, &SAt<int16_t>(oCnt), maxC, noOut ? nullptr : reinterpret_cast<CEntity**>(S(oOutE)), kind == 0, kind == 1, kind == 2); return (uint16_t)SAt<int16_t>(oCnt);
        }, [&]() -> uint64_t {
            reset(); oracle::Fn<void __cdecl(void*, const CVector*, const CVector*, int16_t*, uint32_t, void*, uint32_t, uint32_t, uint32_t)>(0x565300)(S(oList), &A, &B, &SAt<int16_t>(oCnt), (uint32_t)(int32_t)maxC, noOut ? nullptr : S(oOutE), kind == 0, kind == 1, kind == 2); return (uint16_t)SAt<int16_t>(oCnt);
        });
    } ENDROW;
    ROW("CWorld::FindObjectsIntersectingAngledCollisionBoxSectorList", 0x565200, 1500) {
        const CVector C = QPos(r); const CVector bmin(-r.f01() * 40.f, -r.f01() * 40.f, -r.f01() * 20.f), bmax(r.f01() * 40.f, r.f01() * 40.f, r.f01() * 20.f);
        const int16_t maxC = (int16_t)(r.below(8) == 0 ? (int)r.below(3) : 1 + (int)r.below(20)); const bool noOut = r.below(6) == 0; const uint32_t sc = r.u32();
        Tw tw([&](Rng& rr) {
            CWorld::ms_nCurrentScanCode = (uint16)sc; BuildSectors(rr, C, 60.f);
            float* m = reinterpret_cast<float*>(MatP(15));   // rotation about z by a random angle (rows 0..2: right, forward, up), position 0
            const float a = rr.f01() * 6.28f, c = std::cos(a), s = std::sin(a);
            const float rot[12] = { c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0 }; std::memcpy(m, rot, sizeof rot);
        });
        CBox box; box.m_vecMin = bmin; box.m_vecMax = bmax;
        const auto reset = [&] { std::memset(S(oCnt), 0xCD, 0x100); SAt<int16_t>(oCnt) = 0; };
        return w3w::WCase(r, d, [&]() -> uint64_t {
            reset(); CWorld::FindObjectsIntersectingAngledCollisionBoxSectorList(ListR(), box, *reinterpret_cast<CMatrix*>(MatP(15)), C, &SAt<int16_t>(oCnt), maxC, noOut ? nullptr : reinterpret_cast<CEntity**>(S(oOutE))); return (uint16_t)SAt<int16_t>(oCnt);
        }, [&]() -> uint64_t {
            reset(); oracle::Fn<void __cdecl(void*, const CBox*, const void*, const CVector*, int16_t*, uint32_t, void*)>(0x565200)(S(oList), &box, MatP(15), &C, &SAt<int16_t>(oCnt), (uint32_t)(int32_t)maxC, noOut ? nullptr : S(oOutE)); return (uint16_t)SAt<int16_t>(oCnt);
        });
    } ENDROW;
}
