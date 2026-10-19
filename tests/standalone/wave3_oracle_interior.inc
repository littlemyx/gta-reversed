// ORACLE-W3-INTERIOR: exe-oracle rows for the uncovered class A/B functions of game_sa/Interior (Interior_c, InteriorGroup_c, InteriorManager_c, FurnitureGroup_c / SubGroup_c / Manager_c) on real interior fixtures.
// Included by wave2_oracle_all.inc when W3_INTERIOR_ONLY is defined (executable wave3_interior_oracle_test = the W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case).
//
// Fixture: the interior manager itself at its exe address (g_interiorMan 0xBAF670: eight Interior_c with tile grids / goto points / interior infos, eight InteriorGroup_c, the group list / pool, the interior pool, the
// stealable-object table, the interior-id table), the furniture manager (g_furnitureMan 0xBAB380 with its nine groups, the sub-group store 0xBAD3F8 and the furniture store 0xBAE1F8 as real linked lists), and a scratch block g_sc
// (2d-effect boxes for the interiors, peds with placements / matrices, entities with matrices, model infos + collision models behind the exe's ms_modelInfoPtrs 0xA9B0C8, furniture entities, entry/exits, interior-effect infos, out
// parameters). Both sides see the same addresses (NOTSA_ORACLE_ADDRESS_GLOBALS); the whole exe data image is compared after each case.
#include "InteriorManager_c.h"
#include "InteriorGroup_c.h"
#include "Interior_c.h"
#include "FurnitureManager_c.h"
#include "FurnitureGroup_c.h"
#include "FurnitureSubGroup_c.h"
#include "Furniture_c.h"
#include "EntryExit.h"
#include "Models/ModelInfo.h"
#include "Models/BaseModelInfo.h"
#include "Collision/ColModel.h"
#include "Entity/Entity.h"
#include "Entity/Ped/Ped.h"
#include "Camera.h"
#include "World.h"
#include "PlayerInfo.h"

namespace w3i {
using namespace w2;
constexpr uintptr_t IMA = 0xBAF670, FMA = 0xBAB380, SUBST = 0xBAD3F8, FURST = 0xBAE1F8, CURSUB = 0xBAB37C, CURFURN = 0xBAB378;
constexpr size_t kIntSz = 0x794, kGrpSz = 0xBC, kGrpBase = 0x3CAC, kImSize = 0x4758;
constexpr size_t kScSize = 0x4000;
alignas(16) static uint8_t g_sc[kScSize];
// scratch layout
constexpr size_t oBox = 0x000 /*16 * 0x40: C2dEffectInterior*/, oPed = 0x400 /*4 * 0x40*/, oEnt = 0x500 /*8 * 0x40*/, oMat = 0x700 /*8 * 0x50*/, oMi = 0x980 /*4 * 0x40*/, oCol = 0xA80 /*4 * 0x40*/,
                 oFe = 0xB80 /*16 * 0x10*/, oEnex = 0xC80 /*4 * 0x40*/, oFx = 0xD80 /*16 * 0x50*/, oStr = 0x1280 /*0x100*/, oOut = 0x1380 /*0x100*/, oVis = 0x1500 /*900 * 4*/;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint8_t* IMp(size_t off) { return reinterpret_cast<uint8_t*>(IMA + off); }
static Interior_c* Int(int i) { return reinterpret_cast<Interior_c*>(IMA + (size_t)i * kIntSz); }
static InteriorGroup_c* Grp(int g) { return reinterpret_cast<InteriorGroup_c*>(IMA + kGrpBase + (size_t)g * kGrpSz); }
static InteriorManager_c* Man() { return reinterpret_cast<InteriorManager_c*>(IMA); }
static FurnitureGroup_c* FG(int g) { return reinterpret_cast<FurnitureGroup_c*>(FMA + (size_t)g * 0xC); }
static FurnitureManager_c* FM() { return reinterpret_cast<FurnitureManager_c*>(FMA); }
static CEntity* Ent(int k) { return reinterpret_cast<CEntity*>(S(oEnt + (size_t)k * 0x40)); }
static CPed* Ped(int k) { return reinterpret_cast<CPed*>(S(oPed + (size_t)k * 0x40)); }
static C2dEffectInterior* Box(int k) { return reinterpret_cast<C2dEffectInterior*>(S(oBox + (size_t)k * 0x40)); }
static CEntryExit* Enex(int k) { return reinterpret_cast<CEntryExit*>(S(oEnex + (size_t)k * 0x40)); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }

static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static int g_wired[4];                        // model indices wired behind ms_modelInfoPtrs

template<class T> static void Shuffle(Rng& r, std::vector<T>& v) { for (size_t i = v.size(); i > 1; --i) std::swap(v[i - 1], v[(size_t)r.below((int)i)]); }
// intrusive lists: items have prev at +0 and next at +4; the list header is {head, tail, count}
static void PutList(uint8_t* list, const std::vector<uint8_t*>& items) {
    const uint32_t n = (uint32_t)items.size();
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t prev = i ? P32(items[i - 1]) : 0, next = i + 1 < n ? P32(items[i + 1]) : 0;
        std::memcpy(items[i], &prev, 4); std::memcpy(items[i] + 4, &next, 4);
    }
    const uint32_t head = n ? P32(items.front()) : 0, tail = n ? P32(items.back()) : 0;
    std::memcpy(list, &head, 4); std::memcpy(list + 4, &tail, 4); std::memcpy(list + 8, &n, 4);
}
static float Fl(Rng& r, float scale, int special = 14) {   // a game-range float with a few special values (no huge ones: sums of squares overflow float but not the exe's extended registers)
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    return GF(r, scale);
}
static float FlP(Rng& r, float scale) { return GF(r, scale); }   // finite

static void WireModels(Rng& r) {
    auto* tab = reinterpret_cast<void**>(0xA9B0C8);
    for (int k = 0; k < 4; ++k) {
        int idx;
        for (;;) { idx = (int)r.below(20000); bool dup = false; for (int j = 0; j < k; ++j) dup |= g_wired[j] == idx; if (!dup) break; }
        g_wired[k] = idx;
        uint8_t* mi = S(oMi + (size_t)k * 0x40); uint8_t* col = S(oCol + (size_t)k * 0x40);
        FillGarbage(r, mi, 0x40); FillGarbage(r, col, 0x40);
        const uint32_t cp = P32(col); std::memcpy(mi + offsetof(CBaseModelInfo, m_pColModel), &cp, 4);
        auto* cm = reinterpret_cast<CColModel*>(col);
        const float ex = 0.2f + r.f01() * (r.below(8) ? 3.f : 20.f);
        cm->m_boundBox.m_vecMin = CVector(-ex * (0.1f + r.f01()), -ex * (0.1f + r.f01()), -ex * (0.1f + r.f01()));
        cm->m_boundBox.m_vecMax = CVector(ex * (0.1f + r.f01()), ex * (0.1f + r.f01()), ex * (0.1f + r.f01()));
        if (r.below(5) == 0) {   // furniture bounding boxes near the rounding threshold (fractions around 0.02) and whole numbers
            const float base = (float)r.below(6);
            cm->m_boundBox.m_vecMax.x = base + (r.below(2) ? 0.02f : 0.0199f + r.f01() * 0.0003f) - 0.5f * (float)r.below(2);
            cm->m_boundBox.m_vecMin.x = -(float)r.below(3) - (r.below(2) ? 0.f : 0.5f);
            cm->m_boundBox.m_vecMax.y = (float)r.below(5) + (r.below(2) ? 0.5f : 0.02f) - 0.5f;
            cm->m_boundBox.m_vecMin.y = -(float)r.below(4);
        }
        if (r.below(14) == 0) cm->m_boundBox.m_vecMax.x = Fl(r, 10.f, 1);
        if (r.below(14) == 0) cm->m_boundBox.m_vecMin.y = Fl(r, 10.f, 1);
        tab[idx] = mi;
    }
}

static void BuildFurn(Rng& r) {
    FillGarbage(r, FMA, 0x2078);
    FillGarbage(r, SUBST, 0x1C * 128 + 0x14 * 256);
    std::vector<int> sgs(128), fs(256);
    for (int i = 0; i < 128; ++i) sgs[i] = i;
    for (int i = 0; i < 256; ++i) fs[i] = i;
    // the stores are filled from the front: the "next free" counters are the number of allocated entries (like Init/LoadFurniture); near the end / past the end sometimes
    const int maxSg = r.below(4) == 0 ? 126 + r.below(3) : 90, maxF = r.below(4) == 0 ? 254 + r.below(3) : 200;
    int sgi = 0, fi = 0;
    for (int g = 0; g < 9; ++g) {
        std::vector<uint8_t*> subs;
        const int ns = r.below(5);
        for (int k = 0; k < ns && sgi < maxSg; ++k) {
            uint8_t* sg = reinterpret_cast<uint8_t*>(SUBST + (size_t)sgi++ * 0x1C);
            uint32_t sid = r.below(4) == 0 ? (uint32_t)r.below(3) : (uint32_t)r.below(15); std::memcpy(sg + 8, &sid, 4);
            sg[0x18] = (uint8_t)r.below(2); sg[0x19] = (uint8_t)r.below(2); sg[0x1A] = (uint8_t)r.below(2);
            std::vector<uint8_t*> items;
            const int nf = r.below(8);
            for (int j = 0; j < nf && fi < maxF; ++j) {
                uint8_t* f = reinterpret_cast<uint8_t*>(FURST + (size_t)fi++ * 0x14);
                const int16_t mid = (int16_t)g_wired[r.below(4)]; std::memcpy(f + 8, &mid, 2);
                const int16_t id = (int16_t)(r.below(6) == 0 ? -1 : r.below(40)); std::memcpy(f + 0xA, &id, 2);
                const int wmin = r.below(6) == 0 ? (int)r.below(256) : (int)r.below(70), wmax = r.below(6) == 0 ? (int)r.below(256) : wmin + (int)r.below(50);
                f[0xE] = (uint8_t)wmin; f[0xF] = (uint8_t)wmax;
                f[0x10] = (uint8_t)r.below(2); f[0x11] = (uint8_t)r.below(2); f[0x12] = (uint8_t)r.below(2);
                items.push_back(f);
            }
            PutList(sg + 0xC, items);
            subs.push_back(sg);
        }
        PutList(reinterpret_cast<uint8_t*>(FMA + (size_t)g * 0xC), subs);
    }
    At<uint16_t>(CURSUB) = (uint16_t)(r.below(10) == 0 ? 128 + r.below(60) : sgi);
    At<uint16_t>(CURFURN) = (uint16_t)(r.below(10) == 0 ? 256 + r.below(60) : fi);
}

static void GenBox(Rng& r, tEffectInterior& b, bool tidy) {
    b.m_type = (uint8_t)(r.below(10) == 0 ? r.below(256) : r.below(5));
    b.m_groupId = (int8_t)(r.below(10) == 0 ? (int)r.below(256) : (int)r.below(8));
    auto dim = [&]() -> uint8_t { const int k = r.below(24); return (uint8_t)(k == 0 ? 0 : k == 1 ? 31 + r.below(10) : 2 + r.below(tidy ? 18 : 29)); };
    b.m_width = dim(); b.m_depth = dim();
    b.m_height = (uint8_t)(r.below(10) == 0 ? r.below(256) : 2 + r.below(4));
    static const int8_t doors[] = { -1, 0, 1, 2, 3, 4, 127, -128, -2 };
    b.m_door = r.below(4) == 0 ? doors[r.below(9)] : (int8_t)(r.below(3) - 1 + (int)r.below(2));
    auto range = [&](int8_t& s, int8_t& e, int maxv) {
        if (r.below(3) == 0) { s = -1; e = (int8_t)(r.below(2) ? -1 : r.below(maxv + 2)); return; }
        const int st = r.below(8) == 0 ? (int)r.below(256) - 128 : (int)r.below(maxv + 2);
        s = (int8_t)st; e = (int8_t)(r.below(8) == 0 ? (int)r.below(256) - 128 : st + (int)r.below(6) - (r.below(6) == 0 ? 3 : 0));
    };
    range(b.m_lDoorStart, b.m_lDoorEnd, b.m_depth); range(b.m_rDoorStart, b.m_rDoorEnd, b.m_depth); range(b.m_tDoorStart, b.m_tDoorEnd, b.m_width);
    range(b.m_lWindowStart, b.m_lWindowEnd, b.m_depth); range(b.m_rWindowStart, b.m_rWindowEnd, b.m_depth); range(b.m_tWindowStart, b.m_tWindowEnd, b.m_width);
    for (int i = 0; i < 3; ++i) {
        if (r.below(3) == 0) { b.m_noGoLeft[i] = -1; b.m_noGoBottom[i] = -1; }
        else if (r.below(10) == 0) { b.m_noGoLeft[i] = (int8_t)(r.below(256) - 128); b.m_noGoBottom[i] = (int8_t)(r.below(256) - 128); }
        else { b.m_noGoLeft[i] = (int8_t)r.below(b.m_width + 2); b.m_noGoBottom[i] = (int8_t)r.below(b.m_depth + 2); }
        b.m_noGoWidth[i] = (int8_t)(r.below(10) == 0 ? (int)r.below(256) - 128 : (int)r.below(8)); b.m_noGoDepth[i] = (int8_t)(r.below(10) == 0 ? (int)r.below(256) - 128 : (int)r.below(8));
    }
    b.m_seed = (uint8_t)r.below(256); b.m_status = (uint8_t)r.below(3);
    b.m_rot = r.below(3) ? (float)r.below(4) * 1.5707964f : r.f01() * 6.2831853f;
}
static void GenTiles(Rng& r, uint8_t* t) {
    std::memset(t, 0, 900);
    const int nr = r.below(7);
    for (int k = 0; k < nr; ++k) {
        const int x = r.below(30), y = r.below(30), w = 1 + r.below(7), d = 1 + r.below(7);
        const uint8_t v = r.below(3) == 0 ? 5 : (uint8_t)(r.below(12));
        for (int i = 0; i < w && x + i < 30; ++i) for (int j = 0; j < d && y + j < 30; ++j) t[(x + i) * 30 + y + j] = v;
    }
    const int nsp = r.below(5);
    for (int k = 0; k < nsp; ++k) t[r.below(900)] = (uint8_t)(r.below(4) ? r.below(12) : r.below(256));
    if (r.below(16) == 0) for (int k = 0; k < 900; ++k) t[k] = (uint8_t)r.below(12);
}
static void SetMatrixOf(Rng& r, uint8_t* m, size_t rightOff, size_t upOff, size_t atOff, size_t posOff) {   // a rotation about Z (+ rarely pitch / scale / specials) and a position
    float h = r.below(3) ? (float)r.below(4) * 1.5707964f : r.f01() * 6.2831853f, p = r.below(8) == 0 ? (r.f01() - 0.5f) * 1.5f : 0.f;
    const float cr = std::cos(h), sr = std::sin(h), cp = std::cos(p), sp = std::sin(p);
    float R[3] = { cr, sr, 0.f }, F[3] = { -sr * cp, cr * cp, sp }, Uv[3] = { sr * sp, -cr * sp, cp };
    if (r.below(10) == 0) { const float s = 0.3f + r.f01() * 2.f; for (auto& v : R) v *= s; for (auto& v : F) v *= s; }
    if (r.below(30) == 0) R[r.below(3)] = Fl(r, 2.f, 1);
    std::memcpy(m + rightOff, R, 12); std::memcpy(m + upOff, F, 12); std::memcpy(m + atOff, Uv, 12);
    const float pos[3] = { FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f) };
    std::memcpy(m + posOff, pos, 12);
    if (r.below(30) == 0) { const float v = Fl(r, 3000.f, 1); std::memcpy(m + posOff + 4 * r.below(3), &v, 4); }
}
static CVector InteriorLocal(Rng& r, int ii, float ex = 2.f) {   // a world point at a random place in (or near) interior ii, in its own frame
    uint8_t* p = reinterpret_cast<uint8_t*>(Int(ii));
    const auto* b = At<tEffectInterior*>((uintptr_t)p + 0x14);
    const float u = (r.f01() - 0.5f) * ((float)b->m_width + 2.f * ex), v = (r.f01() - 0.5f) * ((float)b->m_depth + 2.f * ex), w = (r.f01() - 0.5f) * ((float)b->m_height + 2.f * ex);
    float R[3], Uv[3], A[3], Pos[3];
    std::memcpy(R, p + 0x18, 12); std::memcpy(Uv, p + 0x28, 12); std::memcpy(A, p + 0x38, 12); std::memcpy(Pos, p + 0x48, 12);
    return CVector(Pos[0] + R[0] * u + Uv[0] * v + A[0] * w, Pos[1] + R[1] * u + Uv[1] * v + A[1] * w, Pos[2] + R[2] * u + Uv[2] * v + A[2] * w);
}

static void BuildIM(Rng& r) {
    FillGarbage(r, IMA, kImSize);
    for (int b = 0; b < 16; ++b) {
        uint8_t* e = S(oBox + (size_t)b * 0x40);
        FillGarbage(r, e, 0x40);
        const CVector pos(FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f)); std::memcpy(e, &pos, 12);
        const uint32_t ty = EFFECT_INTERIOR; std::memcpy(e + 0xC, &ty, 4);
        GenBox(r, *static_cast<tEffectInterior*>(reinterpret_cast<C2dEffectInterior*>(e)), r.below(2));
    }
    // entities / peds / matrices
    for (int k = 0; k < 8; ++k) {
        uint8_t* e = S(oEnt + (size_t)k * 0x40); uint8_t* m = S(oMat + (size_t)k * 0x50);
        FillGarbage(r, e, 0x40); FillGarbage(r, m, 0x50);
        SetMatrixOf(r, m, 0x0, 0x10, 0x20, 0x30);
        const uint32_t zero = 0; std::memcpy(m + 0x40, &zero, 4);
        e[0x36] = (uint8_t)((e[0x36] & 0xF8) | (r.below(2) ? 1 : r.below(2) ? 4 : 5));          // building / object / dummy (a vehicle would read its special collision index)
        At<uint16_t>((uintptr_t)e + offsetof(CEntity, m_nModelIndex)) = (uint16_t)g_wired[r.below(4)];
        const uint32_t mp = P32(m); std::memcpy(e + offsetof(CPlaceable, m_matrix), &mp, 4);
        const CVector pp = *reinterpret_cast<CVector*>(m + 0x30); std::memcpy(e + 4, &pp, 12);   // placement = matrix position
    }
    // entry / exits
    for (int k = 0; k < 4; ++k) {
        uint8_t* e = S(oEnex + (size_t)k * 0x40);
        FillGarbage(r, e, 0x40);
        const uint32_t link = r.below(2) ? 0 : P32(Enex(r.below(4)));
        std::memcpy(e + offsetof(CEntryExit, m_pLink), &link, 4);
        e[offsetof(CEntryExit, m_nArea)] = (uint8_t)(r.below(3) == 0 ? 0 : r.below(5) == 0 ? r.below(256) : 1 + r.below(18));
        const CRect rc(FlP(r, 100.f), FlP(r, 100.f), FlP(r, 100.f), FlP(r, 100.f)); std::memcpy(e + offsetof(CEntryExit, m_recEntrance), &rc, 16);
    }
    // furniture entities
    std::vector<int> fo(16); for (int i = 0; i < 16; ++i) fo[i] = i; Shuffle(r, fo);
    int fni = 0;
    // interiors
    std::vector<uint8_t*> poolInts;
    for (int i = 0; i < 8; ++i) {
        uint8_t* p = reinterpret_cast<uint8_t*>(Int(i));
        const uint32_t box = P32(static_cast<tEffectInterior*>(Box(r.below(16)))); std::memcpy(p + 0x14, &box, 4);
        const uint32_t grp = r.below(10) == 0 ? 0 : P32(Grp(r.below(8))); std::memcpy(p + 0xC, &grp, 4);
        const int32_t area = (int32_t)r.below(10); std::memcpy(p + 0x10, &area, 4);
        SetMatrixOf(r, p, 0x18, 0x28, 0x38, 0x48);
        { const CVector pos = *reinterpret_cast<CVector*>(p + 0x48); std::memcpy(p + 0x400, &pos, 12); }   // m_position
        std::vector<uint8_t*> fes;
        const int nfe = r.below(4);
        for (int k = 0; k < nfe && fni < 16; ++k) {
            uint8_t* fe = S(oFe + (size_t)fo[fni++] * 0x10);
            const uint32_t en = P32(Ent(r.below(8))); std::memcpy(fe + 8, &en, 4);
            fes.push_back(fe);
        }
        PutList(p + 0x5C, fes);
        GenTiles(r, p + 0x68);
        p[0x40C] = (uint8_t)(r.below(10) == 0 ? r.below(20) : r.below(9)); p[0x40D] = (uint8_t)(r.below(12) == 0 ? r.below(20) : r.below(17));
        for (int k = 0; k < 16; ++k) {
            uint8_t* inf = p + 0x590 + (size_t)k * 0x20;
            inf[0] = (uint8_t)(r.below(12) == 0 ? r.below(256) : 1 + r.below(10)); inf[1] = (uint8_t)r.below(2);
            const CVector ip = r.below(3) ? InteriorLocal(r, i, 0.5f) : CVector(Fl(r, 3000.f, 20), Fl(r, 3000.f, 20), Fl(r, 100.f, 20)); std::memcpy(inf + 4, &ip, 12);
            const uint32_t en = r.below(3) ? 0 : P32(Ent(r.below(8))); std::memcpy(inf + 0x1C, &en, 4);
        }
        if (r.below(3) == 0) poolInts.push_back(p);
    }
    PutList(IMp(0x3CA0), poolInts);
    // peds (in or near the interiors)
    for (int k = 0; k < 4; ++k) {
        uint8_t* p = S(oPed + (size_t)k * 0x40);
        FillGarbage(r, p, 0x40);
        const uint32_t zero = 0; std::memcpy(p + offsetof(CPlaceable, m_matrix), &zero, 4);
        const CVector pp = r.below(3) ? InteriorLocal(r, r.below(8)) : CVector(FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f));
        std::memcpy(p + 4, &pp, 12);
        if (r.below(4) == 0) { uint8_t* m = S(oMat + (size_t)k * 0x50); std::memcpy(m + 0x30, &pp, 12); const uint32_t mp = P32(m); std::memcpy(p + offsetof(CPlaceable, m_matrix), &mp, 4); }
    }
    // groups
    std::vector<uint8_t*> listG, poolG;
    for (int g = 0; g < 8; ++g) {
        uint8_t* p = reinterpret_cast<uint8_t*>(Grp(g));
        const uint32_t ent = r.below(8) == 0 ? 0 : P32(Ent(r.below(8))); std::memcpy(p + 8, &ent, 4);
        p[0xC] = (uint8_t)g; p[0xD] = (uint8_t)(r.below(10) == 0 ? r.below(256) : r.below(8)); p[0xE] = (uint8_t)(r.below(10) == 0 ? r.below(256) : r.below(4));
        int n = r.below(9);
        p[0xF] = (uint8_t)n;
        const bool dense = r.below(10) < 7;
        for (int k = 0; k < 8; ++k) {
            uint32_t v = 0;
            if (dense ? k < n : r.below(2)) v = P32(Int(r.below(8)));
            std::memcpy(p + 0x10 + 4 * k, &v, 4);
        }
        const uint32_t en = r.below(3) ? 0 : P32(Enex(r.below(4))); std::memcpy(p + 0x30, &en, 4);
        p[0x34] = (uint8_t)r.below(2); p[0x35] = (uint8_t)r.below(2);
        const int np = r.below(10) == 0 ? r.below(17) : r.below(6);
        p[0x36] = (uint8_t)np;
        for (int k = 0; k < 16; ++k) {
            uint32_t v = (k < np || r.below(4) == 0) && r.below(6) ? P32(Ped(r.below(4))) : 0;
            std::memcpy(p + 0x38 + 4 * k, &v, 4);
        }
        p[0xB8] = (uint8_t)r.below(2); p[0xB9] = (uint8_t)r.below(2); p[0xBA] = (uint8_t)r.below(2);
        (r.below(2) ? listG : poolG).push_back(p);
    }
    Shuffle(r, listG); Shuffle(r, poolG);
    PutList(IMp(0x428C), listG); PutList(IMp(0x4298), poolG);
    // stealable objects / interior ids
    std::vector<int32_t> iids;
    for (int i = 0; i < 8; ++i) iids.push_back(At<int32_t>((uintptr_t)Int(i) + 8));
    static const int32_t kModels[] = { 1200, 1201, 2000, 2001, 14000, 0, -1 };
    static const float kCoords[] = { 0.f, -0.f, 1.5f, -2.5f, 100.25f, 3000.f };
    At<uint32_t>(IMA + 0x42A4) = (uint32_t)(r.below(12) == 0 ? 64 : r.below(14));
    for (int k = 0; k < 64; ++k) At<int32_t>(IMA + 0x42A8 + 4 * (size_t)k) = r.below(3) ? iids[r.below(8)] : (int32_t)r.below(5);
    At<int32_t>(IMA + 0x43A8) = (int32_t)(r.below(10) == 0 ? 32 : r.below(12));
    for (int k = 0; k < 32; ++k) {
        uint8_t* o = IMp(0x43AC + (size_t)k * 0x1C);
        const uint32_t en = r.below(5) == 0 ? 0 : P32(Ent(r.below(8))); std::memcpy(o, &en, 4);
        const int32_t mid = kModels[r.below(7)]; std::memcpy(o + 4, &mid, 4);
        const int32_t iid = r.below(2) ? iids[r.below(8)] : (int32_t)r.below(4); std::memcpy(o + 8, &iid, 4);
        const float pos[3] = { kCoords[r.below(6)], kCoords[r.below(6)], kCoords[r.below(6)] }; std::memcpy(o + 0xC, pos, 12);
        o[0x18] = (uint8_t)r.below(2);
    }
    for (int k = 0; k < 16; ++k) IMp(0x472C)[k] = (uint8_t)r.below(2);
    { const uint32_t en = r.below(3) ? P32(Enex(r.below(4))) : 0; std::memcpy(IMp(0x473C), &en, 4); }
    { CRect rc(FlP(r, 100.f), FlP(r, 100.f), FlP(r, 100.f), FlP(r, 100.f)); if (r.below(3) == 0) std::memcpy(&rc, S(oEnex + (size_t)r.below(4) * 0x40) + offsetof(CEntryExit, m_recEntrance), 16); std::memcpy(IMp(0x4740), &rc, 16); }
    IMp(0x4750)[0] = (uint8_t)r.below(2); IMp(0x4751)[0] = (uint8_t)r.below(2); IMp(0x4752)[0] = (uint8_t)r.below(2);
}

static FurnitureSubGroup_c* g_sgPtr = nullptr;   // a sub-group that is part of a group's list (chosen/created by EnsureSubGroup after BuildFurn)
static void EnsureSubGroup(Rng& r) {
    std::vector<uint8_t*> all;
    for (int g = 0; g < 9; ++g) for (uint32_t p = At<uint32_t>(FMA + (size_t)g * 0xC); p; p = At<uint32_t>(p + 4)) all.push_back(reinterpret_cast<uint8_t*>(p));
    if (all.empty()) {   // no sub-group built: the first store entry becomes the only member of group 0
        uint8_t* sg = reinterpret_cast<uint8_t*>(SUBST);
        std::vector<uint8_t*> v{ sg }; PutList(reinterpret_cast<uint8_t*>(FMA), v); PutList(sg + 0xC, {});
        sg[0x18] = (uint8_t)r.below(2); sg[0x19] = (uint8_t)r.below(2); sg[0x1A] = (uint8_t)r.below(2);
        all.push_back(sg);
    }
    g_sgPtr = reinterpret_cast<FurnitureSubGroup_c*>(all[(size_t)r.below((int)all.size())]);
}
static void BuildWorld(Rng& r) {
    // camera (placement only) near an interior, players' peds
    TheCamera.m_matrix = nullptr;
    const CVector cp = r.below(3) ? InteriorLocal(r, r.below(8), 8.f) : CVector(FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f));
    TheCamera.m_placement.m_vPosn = cp;
    CWorld::PlayerInFocus = 0;
    CWorld::Players[0].m_pPed = reinterpret_cast<CPlayerPed*>(Ped(0));
    CWorld::Players[1].m_pPed = r.below(3) == 0 ? nullptr : reinterpret_cast<CPlayerPed*>(Ped(1));
}

static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    WireModels(r);
    BuildFurn(r);
    BuildIM(r);
    BuildWorld(r);
    if (g_tweak) g_tweak(r);
}

template<class PF, class EF>
static bool ICase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    auto* tab = reinterpret_cast<void**>(0xA9B0C8);
    std::vector<uint8_t> tabSave(sizeof(void*) * 20000); std::memcpy(tabSave.data(), tab, tabSave.size());
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
    std::memcpy(tab, tabSave.data(), tabSave.size());
    return ok;
}
} // namespace w3i

struct Tw { explicit Tw(std::function<void(Rng&)> f) { w3i::g_tweak = std::move(f); } ~Tw() { w3i::g_tweak = nullptr; } };
template<class Fn> static void RunScaledI(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3I_SCALE") ? std::max(1, std::atoi(std::getenv("W3I_SCALE"))) : 1;
    Run(name, std::forward<Fn>(fn), cases * scale);
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledI(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#define IC(PORT, EXE) return ICase(r, d, [&]() -> uint64_t { PORT }, [&]() -> uint64_t { EXE })
#include "wave3_oracle_interior_a.inc"      // furniture groups / sub-groups / manager
#include "wave3_oracle_interior_b.inc"      // interior groups / interior manager
#include "wave3_oracle_interior_c.inc"      // Interior_c

static void TestW3InteriorAll() {
    TestW3InteriorA();
    TestW3InteriorB();
    TestW3InteriorC();
}
