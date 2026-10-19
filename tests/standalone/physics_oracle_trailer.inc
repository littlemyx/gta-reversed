// (f) CTrailer::ProcessEntityCollision 0x6CFFD0 on a fake trailer + a fake second entity (building / vehicle / ped / object), see physics_oracle_test.cpp
// PT2-TRAILER: playtest-2 crashed here (access violation reading ocd->m_nNumTriangles of a building whose col model has NO col data): the exe touches the other
// entity's col data only when that entity is the towing vehicle / the towed vehicle. Both sides run on a private copy of one static blob (trailer, entity, matrices, fake model
// infos, col models, col data, line / box / sphere arrays, the output col points); the whole blob and the return value are compared afterwards.
#include "Entity/Vehicle/Trailer.h"
#include "Collision/ColModel.h"
#include "Collision/ColPoint.h"
#include "Models/BaseModelInfo.h"
#include "Models/ModelInfo.h"
#include "MatrixLink.h"

// CEntity.cpp is not linked into this test: the model-info lookup of the other entity / the trailer is the exe's own (not under test)
CColModel* CEntity::GetColModel() const { return oracle::Fn<CColModel* __fastcall(const CEntity*, int)>(0x535300)(this, 0); }
void CEntity::RegisterReference(CEntity** ref) { oracle::Fn<void __fastcall(CEntity*, int, CEntity**)>(0x571B70)(this, 0, ref); }   // the exe's reference registry (CReferences::Init is run per case)

namespace trl {
constexpr int TMI = 602, EMI = 603;   // model indices of the fake model infos
static_assert(sizeof(CTrailer) <= 0x1000);
struct TrFx {
    alignas(16) uint8 t[0x1000];
    alignas(16) uint8 tmat[sizeof(CMatrixLink) + 32];
    alignas(16) uint8 e[0x1000];
    alignas(16) uint8 emat[sizeof(CMatrixLink) + 32];
    alignas(16) uint8 tmi[0x100];
    alignas(16) uint8 emi[0x100];
    alignas(16) uint8 tcm[sizeof(CColModel)];
    alignas(16) uint8 ecm[sizeof(CColModel)];
    alignas(16) uint8 tcd[sizeof(CCollisionData)];
    alignas(16) uint8 ecd[sizeof(CCollisionData)];
    alignas(16) uint8 tsp[4 * sizeof(CColSphere)];
    alignas(16) uint8 tbx[4 * sizeof(CColBox)];
    alignas(16) uint8 tln[8 * sizeof(CColLine)];
    alignas(16) uint8 esp[4 * sizeof(CColSphere)];
    alignas(16) uint8 ebx[4 * sizeof(CColBox)];
    alignas(16) uint8 out[34 * sizeof(CColPoint)];
    alignas(16) uint8 guard[16];
};
static TrFx g_trfx;
static CVector Cv(Rng& r, float s) { return { (r.f01() * 2.f - 1.f) * s, (r.f01() * 2.f - 1.f) * s, (r.f01() * 2.f - 1.f) * s }; }
static void SetMat(uint8* mat, float yaw, const CVector& pos) {
    auto& m = *reinterpret_cast<CMatrixLink*>(mat);
    m.GetRight() = CVector{ std::cos(yaw), std::sin(yaw), 0.f };
    m.GetForward() = CVector{ -std::sin(yaw), std::cos(yaw), 0.f };
    m.GetUp() = CVector{ 0.f, 0.f, 1.f };
    m.GetPosition() = pos;
}
template<class T> static T* Put(uint8* base, int idx, const T& v) { auto* p = reinterpret_cast<T*>(base) + idx; std::memcpy((void*)p, &v, sizeof v); return p; }

// kind: 0 building (type 1), 1 vehicle (2), 2 ped (3), 3 object (4)
static int Build(Rng& r, TrFx& f) {
    std::memset(&f, 0, sizeof f);
    const int kind = r.below(10) < 4 ? 0 : (int)r.below(4);
    const bool towRel = r.below(5) == 0;   // the entity is the towing vehicle / the towed vehicle: the triangles are hidden
    const bool nullEcd = !towRel && r.below(3) == 0;   // the playtest-2 condition: a col model without col data
    auto* t = reinterpret_cast<CTrailer*>(f.t);
    auto* e = reinterpret_cast<CEntity*>(f.e);

    // trailer
    f.t[0x36] = (uint8)((r.below(8) == 0 ? r.below(10) : 3) << 3 | 2);      // vehicle; status: mostly PHYSICS, sometimes SIMPLE / others
    *reinterpret_cast<uint16*>(f.t + 0x22) = TMI;
    reinterpret_cast<CVehicle*>(f.t)->m_vehicleSpecialColIndex = -1;   // else CEntity::GetColModel returns CVehicle::m_aSpecialColModel[0]
    *reinterpret_cast<uint32*>(f.t + 0x14) = (uint32)(uintptr_t)f.tmat;
    SetMat(f.tmat, r.f01() * 6.28f, Cv(r, 0.3f));
    auto* p = reinterpret_cast<CPhysical*>(f.t);
    *reinterpret_cast<uint32*>(&p->m_nPhysicalFlags) = (r.below(4) == 0 ? 0x1000u : 0u) | (r.below(6) == 0 ? 0x8000u : 0u) | (r.below(4) == 0 ? 0x4u : 0u);
    t->m_fTrailerTowedRatio = r.below(3) == 0 ? 0.f : r.f01();
    for (int i = 0; i < 4; ++i) t->m_fWheelsSuspensionCompression[i] = r.below(3) ? 1.f : r.f01();
    for (int i = 0; i < 2; ++i) t->m_supportRatios[i] = r.below(3) ? 1.f : r.f01();
    for (int i = 0; i < 4; ++i) { t->m_apWheelCollisionEntity[i] = nullptr; }
    // COLLPOINT-ASSIGN: the wheel / support col points are written with the exe's CALLED CColPoint::operator= (0x40FC80, sites 0x6D01D7 / 0x6D0312), which leaves field_C, field_1C and the 2 bytes at 0x26
    // alone: seed those with junk so that a full 0x2C-byte copy (the port before) shows up as a mismatch
    for (int i = 0; i < 6; ++i) {
        uint8* cp = i < 4 ? f.t + 0x724 + 0x2C * i : f.t + 0x988 + 0x2C * (i - 4);
        const uint32 j0 = r.u32(), j1 = r.u32(), j2 = r.u32();
        std::memcpy(cp + 0x0C, &j0, 4); std::memcpy(cp + 0x1C, &j1, 4); std::memcpy(cp + 0x26, &j2, 2);
    }
    if (towRel) { if (r.below(2)) t->m_pTowingVehicle = reinterpret_cast<CVehicle*>(f.e); else t->m_pVehicleBeingTowed = reinterpret_cast<CVehicle*>(f.e); }
    else if (r.below(4) == 0) { t->m_pTowingVehicle = reinterpret_cast<CVehicle*>(f.t + 0x800); t->m_pVehicleBeingTowed = reinterpret_cast<CVehicle*>(f.t + 0x840); }   // other vehicles, not the entity

    // the other entity
    static const uint8 types[] = { 1, 2, 3, 4 };
    f.e[0x36] = (uint8)(types[kind] | (r.below(10) << 3));
    *reinterpret_cast<uint16*>(f.e + 0x22) = EMI;
    if (kind == 1) reinterpret_cast<CVehicle*>(f.e)->m_vehicleSpecialColIndex = -1;
    *reinterpret_cast<uint32*>(f.e + 0x14) = (uint32)(uintptr_t)f.emat;
    *reinterpret_cast<uint32*>(f.e + 0x1C) = r.u32();                       // entity flags: the tunnel bits are copied for a building
    if (kind != 0) *reinterpret_cast<uint32*>(&reinterpret_cast<CPhysical*>(f.e)->m_nPhysicalFlags) = r.below(2) ? 0x4u : 0u;   // bDisableCollisionForce
    const bool flat = true;   // every type gets geometry: a vehicle / object under a wheel line goes through RegisterReference (the exe's own, below; the reference pool is re-initialised per case)
    const float eh = (r.f01() - 0.7f) * 1.2f;   // top of the ground box relative to the trailer origin
    SetMat(f.emat, 0.f, CVector{ Cv(r, 0.4f).x, Cv(r, 0.4f).y, 0.f });

    // trailer col model: bounding box / sphere, 0..2 spheres, 0..2 boxes, 6 or 0 lines (4 wheel lines + 2 support lines)
    auto& tcm = *reinterpret_cast<CColModel*>(f.tcm);
    tcm.m_boundBox.m_vecMin = CVector{ -1.3f, -4.2f, -1.f }; tcm.m_boundBox.m_vecMax = CVector{ 1.3f, 4.2f, 2.2f };
    tcm.m_boundSphere.m_vecCenter = CVector{ 0.f, 0.f, 0.4f }; tcm.m_boundSphere.m_fRadius = 4.9f;
    tcm.m_pColData = reinterpret_cast<CCollisionData*>(f.tcd);
    auto& tcd = *reinterpret_cast<CCollisionData*>(f.tcd);
    tcd.m_nNumSpheres = (uint16)r.below(3); tcd.m_nNumBoxes = (uint16)r.below(3);
    tcd.m_nNumLines = r.below(6) == 0 ? 0 : 6;
    tcd.m_nNumTriangles = towRel ? (uint16)(1 + r.below(5)) : 0;
    tcd.m_pSpheres = reinterpret_cast<CColSphere*>(f.tsp); tcd.m_pBoxes = reinterpret_cast<CColBox*>(f.tbx); tcd.m_pLines = reinterpret_cast<CColLine*>(f.tln);
    for (int i = 0; i < tcd.m_nNumSpheres; ++i) { CColSphere s; s.m_vecCenter = Cv(r, 1.5f); s.m_fRadius = 0.2f + r.f01() * 0.8f; Put(f.tsp, i, s); }
    for (int i = 0; i < tcd.m_nNumBoxes; ++i) { CColBox b{}; const CVector c = Cv(r, 1.f); b.m_vecMin = c - CVector{ 0.4f, 0.8f, 0.3f }; b.m_vecMax = c + CVector{ 0.4f, 0.8f, 0.3f }; Put(f.tbx, i, b); }
    const float lx[6] = { -1.1f, 1.1f, -1.1f, 1.1f, -0.5f, 0.5f }, ly[6] = { 3.2f, 3.2f, -3.2f, -3.2f, 2.0f, 2.0f };
    for (int i = 0; i < 6; ++i) { CColLine l; l.m_vecStart = CVector{ lx[i], ly[i], 0.6f }; l.m_vecEnd = CVector{ lx[i], ly[i], -0.7f - (i >= 4 ? 0.3f : 0.f) }; Put(f.tln, i, l); }

    // other col model: a big flat box (the ground) + extras; no col data in the playtest-2 case
    auto& ecm = *reinterpret_cast<CColModel*>(f.ecm);
    ecm.m_boundBox.m_vecMin = CVector{ -9.f, -9.f, -3.f }; ecm.m_boundBox.m_vecMax = CVector{ 9.f, 9.f, eh + 3.f };
    ecm.m_boundSphere.m_vecCenter = CVector{ 0.f, 0.f, 0.f }; ecm.m_boundSphere.m_fRadius = 14.f;
    ecm.m_pColData = nullEcd ? nullptr : reinterpret_cast<CCollisionData*>(f.ecd);
    auto& ecd = *reinterpret_cast<CCollisionData*>(f.ecd);
    if (!nullEcd) {
        ecd.m_nNumSpheres = flat ? (uint16)r.below(3) : 0;
        ecd.m_nNumBoxes = flat ? (uint16)(1 + r.below(3)) : 0;
        ecd.m_nNumTriangles = towRel ? (uint16)(1 + r.below(5)) : 0;
        ecd.m_pSpheres = reinterpret_cast<CColSphere*>(f.esp); ecd.m_pBoxes = reinterpret_cast<CColBox*>(f.ebx);
        for (int i = 0; i < ecd.m_nNumSpheres; ++i) { CColSphere s; s.m_vecCenter = Cv(r, 3.f); s.m_fRadius = 0.4f + r.f01(); Put(f.esp, i, s); }
        for (int i = 0; i < ecd.m_nNumBoxes; ++i) {
            CColBox b{};
            if (i == 0) { b.m_vecMin = CVector{ -8.f, -8.f, eh - 2.f }; b.m_vecMax = CVector{ 8.f, 8.f, eh }; } else { const CVector c = Cv(r, 3.f); b.m_vecMin = c - CVector{ 0.5f, 0.5f, 0.5f }; b.m_vecMax = c + CVector{ 0.5f, 0.5f, 0.5f }; }
            b.m_Surface.m_nMaterial = (eSurfaceType)r.below(12);
            Put(f.ebx, i, b);
        }
    }

    // fake model infos (the exe's CEntity::GetColModel -> CModelInfo::ms_modelInfoPtrs[idx]->m_pColModel)
    reinterpret_cast<CBaseModelInfo*>(f.tmi)->m_pColModel = &tcm;
    reinterpret_cast<CBaseModelInfo*>(f.emi)->m_pColModel = &ecm;
    CModelInfo::ms_modelInfoPtrs[TMI] = reinterpret_cast<CBaseModelInfo*>(f.tmi);
    CModelInfo::ms_modelInfoPtrs[EMI] = reinterpret_cast<CBaseModelInfo*>(f.emi);
    CTimer::m_snTimeInMilliseconds = r.u32() % 100000;
    (void)e;
    return kind;
}
// Comparison of two result blobs. Two kinds of bytes are NOT behaviour and are not compared:
//  - CColPoint bytes 0x20..0x2B (surface / piece / lighting A + the pad + m_fDepth) of the trailer's wheel / support col points: ProcessColModels writes them for sphere hits only, a LINE hit leaves them
//    as they were in the caller's uninitialised stack array (the exe: stack garbage, the port: zero-initialised std::array) - the copy into the trailer just carries that garbage along.
//  - the float fields of the col points that ProcessColModels wrote into `out` may differ by a few ulp (the port's ProcessColModels is not bit-exact against the exe: separate finding, counted in `ulpOnly`).
static int g_ulpOnly = 0;
static bool SameFx(const uint8* a, const uint8* b, std::string& why) {
    unsigned cw = 0; _controlfp_s(&cw, 0, 0);
    const bool pc53 = (cw & _MCW_PC) == _PC_53;   // only the informational 53-bit mode is lenient; the game mode (PC24) compares every bit of every field that is behaviour
    bool ok = true; int shown = 0; bool ulp = false;
    for (size_t o = 0; o + 4 <= sizeof(TrFx); o += 4) {
        uint32_t x, y; std::memcpy(&x, a + o, 4); std::memcpy(&y, b + o, 4);
        if (x == y) continue;
        if (o >= offsetof(TrFx, t) && o < offsetof(TrFx, t) + 0x1000) {
            const size_t rel = o - offsetof(TrFx, t);
            const auto cpRel = [&](size_t base, int n) -> int { return (rel >= base && rel < base + 0x2C * n) ? (int)((rel - base) % 0x2C) : -1; };
            const int w = cpRel(0x724, 4), sp = cpRel(0x988, 2);
            if ((w >= 0x20 && w <= 0x2B) || (sp >= 0x20 && sp <= 0x2B)) continue;
        }
        if (o >= offsetof(TrFx, out) && o < offsetof(TrFx, out) + sizeof(TrFx::out)) {
            const size_t rel = (o - offsetof(TrFx, out)) % 0x2C;
            const bool isF = rel < 0x20 || rel == 0x28;
            const int64_t dx = (int64_t)(int32_t)x - (int32_t)y;
            if (pc53 && isF && ((x ^ y) & 0x80000000u) == 0 && std::llabs(dx) <= 64) { ulp = true; continue; }
        }
        ok = false;
        if (shown++ < 8) { char t[96]; std::snprintf(t, sizeof t, " blob+%04zX got %08X exe %08X", o, x, y); why += t; }
    }
    if (ok && ulp) ++g_ulpOnly;
    return ok;
}
}   // namespace trl

static void TestTrailer() {
    // the exe's CCollision::ProcessColModels has function-local statics (guard word 0x9689D0, bits 0..4); their one-time initialisation registers atexit destructors through the exe's
    // uninitialised CRT (HeapAlloc through an unpatched import): mark them initialised, the arrays are zero-initialised plain data
    *reinterpret_cast<uint32*>(0x9689D0) = 0x1F;
    Run("CTrailer::ProcessEntityCollision 0x6CFFD0", [](Rng& r, std::string& d) {
        using namespace trl;
        auto& f = g_trfx;
        oracle::Fn<void __cdecl()>(0x5719B0)();   // CReferences::Init: all reference nodes free again, so that the port run and the exe run get the same nodes
        const int kind = Build(r, f);
        auto* t = reinterpret_cast<CTrailer*>(f.t);
        auto* ent = reinterpret_cast<CEntity*>(f.e);
        const bool nullEcd = reinterpret_cast<CColModel*>(f.ecm)->m_pColData == nullptr;
        const std::vector<uint8> snap(reinterpret_cast<uint8*>(&f), reinterpret_cast<uint8*>(&f) + sizeof f);
        const int32 ra = t->CTrailer::ProcessEntityCollision(ent, reinterpret_cast<CColPoint*>(f.out));
        const std::vector<uint8> a(reinterpret_cast<uint8*>(&f), reinterpret_cast<uint8*>(&f) + sizeof f);
        std::memcpy(&f, snap.data(), sizeof f);
        oracle::Fn<void __cdecl()>(0x5719B0)();
        CModelInfo::ms_modelInfoPtrs[TMI] = reinterpret_cast<CBaseModelInfo*>(f.tmi);
        CModelInfo::ms_modelInfoPtrs[EMI] = reinterpret_cast<CBaseModelInfo*>(f.emi);
        const int32 re = oracle::Fn<int __fastcall(void*, int, void*, void*)>(0x6CFFD0)(t, 0, ent, f.out);
        const std::vector<uint8> b(reinterpret_cast<uint8*>(&f), reinterpret_cast<uint8*>(&f) + sizeof f);
        Hit(ra > 0 || a != snap);
        std::string why;
        if (ra == re && SameFx(a.data(), b.data(), why)) return true;
        d = "kind " + std::to_string(kind) + (nullEcd ? " nullEcd" : "") + " ret got " + std::to_string(ra) + " exe " + std::to_string(re) + why;
        return false;
    });
    std::printf("    (CTrailer::ProcessEntityCollision: %d case runs (PC53 only) had output col points differing by a few ulp only - CCollision::ProcessColModels at the 53-bit mode)\n", trl::g_ulpOnly);
}
