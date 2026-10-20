// ORACLE-W3-EFFECTS rows G: CProjectileInfo::IsProjectileInRange (no destroy) and RemoveIfThisIsAProjectile (object that is not an active projectile) on the 32-slot projectile tables at their exe addresses.
// The destroying paths (CRadar::ClearBlipForEntity, CWorld::Remove, delete of the object) need a world / object pool fixture and are not covered.
namespace w3fg {
using namespace w3f; using namespace w2;
constexpr size_t oPrj = 0x12000;   // 32 fake projectile objects x 0x200
static void PutProjectiles(Rng& r) {
    FillGarbage(r, &gaProjectileInfo, sizeof(gaProjectileInfo));
    for (uint32 i = 0; i < MAX_PROJECTILES; ++i) {
        auto& info = gaProjectileInfo[i];
        info.m_bActive = r.below(3) != 0;
        info.m_nWeaponType = r.below(8) ? (r.below(2) ? (uint32)(18 + r.below(4)) : r.below(60)) : r.u32();   // grenade / molotov / rocket / heat seeking / satchel range mostly
        auto* pj = reinterpret_cast<CProjectile*>(S(oPrj + 0x200 * i));
        pj->m_matrix = r.below(8) == 0 ? reinterpret_cast<CMatrixLink*>(S(oPrj + 0x200 * i + 0x100)) : nullptr;   // a matrix: the position comes from the matrix, else from the placement
        pj->m_placement.m_vPosn = CVector(GN3(r, 100.f), GN3(r, 100.f), GN3(r, 30.f));
        if (pj->m_matrix) { pj->m_matrix->GetPosition() = CVector(GN3(r, 100.f), GN3(r, 100.f), GN3(r, 30.f)); }
        CProjectileInfo::ms_apProjectile[i] = (!info.m_bActive && r.below(4) == 0) ? nullptr : pj;   // (a null object of an active projectile faults in the exe too)
    }
}
} // namespace w3fg
static void TestW3FxG() {
    using namespace w2; using namespace w3f; using namespace w3fg;
    ROW("CProjectileInfo::IsProjectileInRange", 0x739860, 3000) {
        const float a = GN3(r, 120.f), b = GN3(r, 120.f), c = GN3(r, 120.f), e = GN3(r, 120.f), f = GN3(r, 40.f), g = GN3(r, 40.f);
        const bool big = r.below(3) == 0;
        const float x1 = big ? -200.f : std::min(a, b), x2 = big ? 200.f : std::max(a, b), y1 = big ? -200.f : std::min(c, e), y2 = big ? 200.f : std::max(c, e), z1 = big ? -60.f : std::min(f, g), z2 = big ? 60.f : std::max(f, g);
        const bool swapped = r.below(8) == 0;   // min / max exchanged: IsPointInside is evaluated as written
        Tu tu([](Rng& rr) { PutProjectiles(rr); });
        const float X1 = swapped ? x2 : x1, X2 = swapped ? x1 : x2;
        return UCase(r, d, [&]() -> uint64_t { return B8(CProjectileInfo::IsProjectileInRange(X1, X2, y1, y2, z1, z2, false)); },
            [&]() -> uint64_t { return oracle::Fn<bool __cdecl(float, float, float, float, float, float, uint32_t)>(0x739860)(X1, X2, y1, y2, z1, z2, 0) ? 1 : 0; });
    } ENDROW;
    ROW("CProjectileInfo::RemoveIfThisIsAProjectile", 0x739A40, 1500) {
        const unsigned k = r.below(34); const bool nullObj = r.below(10) == 0;
        Tu tu([](Rng& rr) { PutProjectiles(rr); for (uint32 i = 0; i < MAX_PROJECTILES; ++i) gaProjectileInfo[i].m_bActive = false; if (rr.below(2)) { for (uint32 i = 0; i < MAX_PROJECTILES; ++i) CProjectileInfo::ms_apProjectile[i] = reinterpret_cast<CProjectile*>(S(oPrj + 0x200 * rr.below(32))); } });   // no active projectile: the not-found path only
        auto obj = [&]() -> CObject* { return nullObj ? nullptr : reinterpret_cast<CObject*>(S(oPrj + 0x200 * (k % 34))); };
        return UCase(r, d, [&]() -> uint64_t { return B8(CProjectileInfo::RemoveIfThisIsAProjectile(obj())); },
            [&]() -> uint64_t { return oracle::Fn<bool __cdecl(CObject*)>(0x739A40)(obj()) ? 1 : 0; });
    } ENDROW;
}
