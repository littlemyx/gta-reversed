// ORACLE-W3-ENTITY: exe-oracle rows for the uncovered class A/B functions of game_sa/Entity (CEntity / CPhysical / CPlaceable / CBuilding / CAnimatedBuilding) on real entity fixtures.
// Included by wave2_oracle_all.inc when W3_ENTITY_ONLY is defined (executable wave3_entity_oracle_test = the W2 harness: deterministic arena, whole exe data image + the fixture compared after every case).
//
// Fixture g_fx (one block, the same addresses on the port and exe side): two physical entities A / B and a third C (garbage of the full vehicle size with plausible fields: type, model index, matrix or
// placement, collision counters ...), the model infos + collision models behind the exe's CModelInfo::ms_modelInfoPtrs (0xA9B0C8) for their model indices, the matrix nodes behind the exe's
// gMatrixList (0xB74288, three lists: used / static / free), placeables owning the other nodes, a collision point, output vectors. All calls are qualified (no vptr is read).
#include "Models/ModelInfo.h"
#include "Models/BaseModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "Collision/ColModel.h"
#include "Collision/ColPoint.h"
#include "Entity/Physical.h"
#include "Entity/Building.h"
#include "Entity/AnimatedBuilding.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Core/MatrixLinkList.h"
#include "References.h"
#include "Glass.h"
#include "Weather.h"
#include "Timer.h"
#include "TimeCycle.h"

namespace w3e {
using namespace w2;
constexpr size_t kFxSize = 0x6000;
alignas(16) static uint8_t g_fx[kFxSize];
constexpr size_t kEntSize = 0xA80;
constexpr size_t kA = 0x0000, kB = 0x0A80, kC = 0x1500, kMiA = 0x1F80 /*0x310*/, kMiB = 0x2290 /*0x310*/, kColA = 0x25A0 /*0x40*/, kColB = 0x25E0 /*0x40*/, kNodes = 0x2640 /*12 * 0x60*/,
                 kPlc = 0x2AC0 /*12 * 0x20*/, kOut = 0x2C40 /*0x100*/, kCp = 0x2D40 /*0x60*/, kMat = 0x2DC0 /*0x80*/, kScratch = 0x2E40 /*0x100*/, kIn = 0x2F40 /*0x100*/;
constexpr int kNumNodes = 12;
static uint8_t* P(size_t off) { return g_fx + off; }
template<class T> static T& At(size_t off) { return *reinterpret_cast<T*>(g_fx + off); }
static inline uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
// clamps finite floats of a fixture range to +-lim and flushes denormal-range ones to 0 (squares of 1e20+ overflow float, not the exe's extended registers; such values are not game states). NaN / inf are kept.
static void ClampHuge(uint8_t* p, size_t n, float lim = 1e15f, float tiny = 1e-30f) {
    for (size_t i = 0; i + 4 <= n; i += 4) { float f; std::memcpy(&f, p + i, 4); if (std::isfinite(f) && std::fabs(f) > lim) { f = f < 0 ? -lim : lim; std::memcpy(p + i, &f, 4); } else if (f != 0.f && std::fabs(f) < tiny) { f = 0.f; std::memcpy(p + i, &f, 4); } }   // denormal-range floats keep more bits in the exe's extended registers
}
static CEntity* EntA() { return reinterpret_cast<CEntity*>(P(kA)); }
static CEntity* EntB() { return reinterpret_cast<CEntity*>(P(kB)); }
static CEntity* EntC() { return reinterpret_cast<CEntity*>(P(kC)); }
static CPhysical* PhyA() { return reinterpret_cast<CPhysical*>(P(kA)); }
static CPhysical* PhyB() { return reinterpret_cast<CPhysical*>(P(kB)); }
static CPlaceable* PlcA() { return reinterpret_cast<CPlaceable*>(P(kA)); }
static CMatrixLink* Node(int i) { return reinterpret_cast<CMatrixLink*>(P(kNodes + (size_t)i * 0x60)); }
static CPlaceable* Plc(int i) { return reinterpret_cast<CPlaceable*>(P(kPlc + (size_t)i * 0x20)); }
static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static int g_idxA = 0, g_idxB = 0;
static unsigned g_flMask = S_ALL;   // special values Fl() may produce (rows whose x87 chain overflows float but not the exe's extended registers drop S_HUGE)

// a finite game-range float with a few special values mixed in
static float Fl(Rng& r, float scale, int special = 12) {
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, g_flMask);       // NaN / inf / 3e38 / denormals
    return GF(r, scale);
}
static void WireMatrixBody(Rng& r, uint8_t* m) {   // the 4x4 part of a CMatrixLink (first 0x40 bytes) + the RW-matrix pointer / owns flag
    FillGarbage(r, m, 0x40);
    auto& M = *reinterpret_cast<CMatrix*>(m);
    if (r.below(4)) {   // mostly a proper rotation, like the game's entities
        const float h = r.f01() * 6.2831853f, p = (r.f01() - 0.5f) * (r.below(3) ? 1.2f : 3.1f);
        M.GetRight() = CVector(std::cos(h), std::sin(h), 0.f);
        M.GetForward() = CVector(-std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p));
        M.GetUp() = CVector(std::sin(h) * std::sin(p), -std::cos(h) * std::sin(p), std::cos(p));
        if (r.below(6) == 0) { const float s = 0.2f + r.f01() * 3.f; M.GetRight() *= s; M.GetForward() *= s; M.GetUp() *= (r.below(2) ? s : 1.f); }   // scaled matrices
    }
    M.GetPosition() = CVector(Fl(r, 3000.f, 20), Fl(r, 3000.f, 20), Fl(r, 100.f, 20));
    std::memset(m + 0x40, 0, 8);
}
static void WireModel(Rng& r, size_t miOff, size_t colOff, int idx) {
    FillGarbage(r, P(miOff), 0x310);
    FillGarbage(r, P(colOff), 0x40);
    At<void*>(miOff + offsetof(CBaseModelInfo, m_pColModel)) = P(colOff);
    auto* cm = reinterpret_cast<CColModel*>(P(colOff));
    const float ex = 0.1f + r.f01() * (r.below(8) ? 12.f : 80.f);
    cm->m_boundBox.m_vecMin = CVector(-ex * (0.2f + r.f01()), -ex * (0.2f + r.f01()), -ex * (0.2f + r.f01()));
    cm->m_boundBox.m_vecMax = CVector(ex * (0.2f + r.f01()), ex * (0.2f + r.f01()), ex * (0.2f + r.f01()));
    if (r.below(6) == 0) cm->m_boundBox.m_vecMin.x = Fl(r, 40.f, 3), cm->m_boundBox.m_vecMax.y = Fl(r, 40.f, 3);
    if (r.below(10) == 0) std::swap(cm->m_boundBox.m_vecMin.z, cm->m_boundBox.m_vecMax.z);
    cm->m_boundSphere.m_vecCenter = CVector(Fl(r, 4.f, 14), Fl(r, 4.f, 14), Fl(r, 3.f, 14));
    cm->m_boundSphere.m_fRadius = r.below(12) == 0 ? Fl(r, 60.f, 2) : r.f01() * 40.f;
    auto* tab = reinterpret_cast<void**>(0xA9B0C8);
    tab[idx] = P(miOff);
}
static void WireEntity(Rng& r, size_t off, size_t miOff, int idx, int nodeIdx, bool allowNullMatrix) {
    uint8_t* e = P(off);
    FillGarbage(r, e, kEntSize);
    // type (3 bits of +0x36) and status (5 bits): mostly game types
    const uint32_t ty = r.below(12) == 0 ? r.below(8) : 1 + r.below(5);
    e[0x36] = (uint8_t)((e[0x36] & 0xF8) | ty);
    At<uint16_t>(off + offsetof(CEntity, m_nModelIndex)) = (uint16_t)idx;
    if (allowNullMatrix && r.below(4) == 0) {
        At<void*>(off + offsetof(CPlaceable, m_matrix)) = nullptr;
        At<float>(off + 4) = Fl(r, 3000.f, 20); At<float>(off + 8) = Fl(r, 3000.f, 20); At<float>(off + 12) = Fl(r, 100.f, 20); At<float>(off + 16) = r.below(8) ? (r.f01() - 0.5f) * 7.f : Fl(r, 6.f, 4);
    } else {
        uint8_t* n = reinterpret_cast<uint8_t*>(Node(nodeIdx));
        WireMatrixBody(r, n);
        reinterpret_cast<CMatrixLink*>(n)->m_pOwner = reinterpret_cast<CPlaceable*>(e);
        At<void*>(off + offsetof(CPlaceable, m_matrix)) = n;
    }
    // game ranges of the counters / selectors the entity code indexes with
    At<uint8_t>(off + offsetof(CPhysical, m_nNumEntitiesCollided)) = (uint8_t)(r.below(8) == 0 ? r.below(256) : r.below(7));
    for (int i = 0; i < 6; ++i) {
        CEntity* c = r.below(3) == 0 ? nullptr : (r.below(2) ? EntB() : EntC());
        At<void*>(off + offsetof(CPhysical, m_apCollidedEntities) + 4 * (size_t)i) = c;
    }
    At<int8_t>(off + offsetof(CVehicle, m_vehicleSpecialColIndex)) = (int8_t)(r.below(3) ? -1 : (int)r.below(4));
    At<void*>(off + offsetof(CEntity, m_pReferences)) = nullptr;
    reinterpret_cast<CEntity*>(e)->SetLod(nullptr);
    At<void*>(off + 0x18) = nullptr;   // m_pRwObject: the RW-calling functions are compared on the matrix part only (RW entry points are exe code vs fakerw)
    At<void*>(off + offsetof(CPhysical, m_pDamageEntity)) = nullptr;
    At<void*>(off + offsetof(CPhysical, m_pAttachedTo)) = nullptr;
    At<void*>(off + offsetof(CPhysical, m_pMovingList)) = nullptr;
    (void)miOff;
}
static void BuildMatrixList(Rng& r) {
    auto& L = gMatrixList;
    // the nodes: 3 intrusive lists (used / static / free), distributed at random; used nodes are owned by a placeable that points back
    L.m_head.m_pNext = &L.m_tail; L.m_tail.m_pPrev = &L.m_head;
    L.m_allocatedListHead.m_pNext = &L.m_allocatedListTail; L.m_allocatedListTail.m_pPrev = &L.m_allocatedListHead;
    L.m_freeListHead.m_pNext = &L.m_freeListTail; L.m_freeListTail.m_pPrev = &L.m_freeListHead;
    L.m_head.m_pPrev = nullptr; L.m_tail.m_pNext = nullptr; L.m_allocatedListHead.m_pPrev = nullptr; L.m_allocatedListTail.m_pNext = nullptr; L.m_freeListHead.m_pPrev = nullptr; L.m_freeListTail.m_pNext = nullptr;
    L.m_pObjects = Node(0);
    const int mode = r.below(6);   // 0: everything used (free list empty -> the oldest link is evicted), 1: few free, else mixed
    for (int i = 0; i < kNumNodes; ++i) {
        CMatrixLink* n = Node(i);
        if (i < 3) continue;       // nodes 0..2 belong to entity A / B / C (WireEntity), linked below if they own a matrix
        const int k = mode == 0 ? (r.below(2) ? 0 : 1) : (mode == 1 ? (i < 10 ? r.below(2) : 2) : r.below(3));
        CMatrixLink* head = k == 0 ? &L.m_head : k == 1 ? &L.m_allocatedListHead : &L.m_freeListHead;
        n->m_pNext = head->m_pNext; head->m_pNext->m_pPrev = n; n->m_pPrev = head; head->m_pNext = n;
        if (k != 2) {
            Plc(i)->m_matrix = n; n->m_pOwner = Plc(i);
            FillGarbage(r, P(kPlc + (size_t)i * 0x20 + 4), 16);
            WireMatrixBody(r, reinterpret_cast<uint8_t*>(n));
        } else {
            n->m_pOwner = nullptr;
        }
    }
    // entity A / B / C matrices (when they have one) sit in the used / static lists
    int idx = 0;
    for (CEntity* e : { EntA(), EntB(), EntC() }) {
        CMatrixLink* n = Node(idx++);
        n->m_pPrev = n->m_pNext = nullptr;
        if (e->m_matrix == n) {
            CMatrixLink* head = r.below(2) ? &L.m_head : &L.m_allocatedListHead;
            n->m_pNext = head->m_pNext; head->m_pNext->m_pPrev = n; n->m_pPrev = head; head->m_pNext = n;
        } else {
            n->m_pOwner = nullptr;
        }
    }
}

// gives an entity that has only a placement a matrix node (linked into the used list), like the game's physical entities always have one
static void EnsureMatrix(Rng& r, CEntity* e, int nodeIdx) {
    if (e->m_matrix) return;
    CMatrixLink* n = Node(nodeIdx);
    WireMatrixBody(r, reinterpret_cast<uint8_t*>(n));
    e->m_matrix = n; n->m_pOwner = reinterpret_cast<CPlaceable*>(e);
    auto& head = gMatrixList.m_head;
    n->m_pPrev = &head; n->m_pNext = head.m_pNext; head.m_pNext->m_pPrev = n; head.m_pNext = n;
}

// the standard fixture: fresh garbage everywhere, then the plausible fields every row can rely on
static void BuildW(Rng& r) {
    FillGarbage(r, g_fx, kFxSize);
    g_idxA = (int)r.below(20000); do { g_idxB = (int)r.below(20000); } while (g_idxB == g_idxA);
    WireModel(r, kMiA, kColA, g_idxA); WireModel(r, kMiB, kColB, g_idxB);
    WireEntity(r, kA, kMiA, g_idxA, 0, true);
    WireEntity(r, kB, kMiB, g_idxB, 1, true);
    WireEntity(r, kC, kMiB, g_idxB, 2, true);
    BuildMatrixList(r);
    // game state the entity code reads
    CTimer::ms_fTimeStep = r.below(8) ? 0.2f + r.f01() * 3.f : Fl(r, 5.f, 2);
    CTimer::ms_fTimeStepNonClipped = CTimer::ms_fTimeStep;
    CTimer::m_snTimeInMilliseconds = r.u32();
    CTimer::m_UserPause = r.below(5) == 0; CTimer::m_CodePause = r.below(10) == 0;
    CWeather::WindDir = CVector(Fl(r, 1.f, 10), Fl(r, 1.f, 10), 0.f);
    CWeather::Wind = r.below(5) ? r.f01() * 1.2f : Fl(r, 2.f, 3);
    CReferences::Init();
    if (g_tweak) g_tweak(r);
}

template<class PF, class EF>
static bool ECase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildW) {
    const uint32_t seed = r.u32() | 1;
    auto* tab = reinterpret_cast<void**>(0xA9B0C8);
    std::vector<uint8_t> tabSave(sizeof(void*) * 20000); std::memcpy(tabSave.data(), tab, tabSave.size());
    build(r);
    Reg fx{ (uintptr_t)g_fx, kFxSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = BothA(d, seed, { fx, img }, std::forward<PF>(pf), std::forward<EF>(ef));
    std::memcpy(tab, tabSave.data(), tabSave.size());
    return ok;
}
// exe thiscall / cdecl call helpers
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
} // namespace w3e

struct Tw { explicit Tw(std::function<void(Rng&)> f) { w3e::g_tweak = std::move(f); } ~Tw() { w3e::g_tweak = nullptr; } };
template<class Fn> static void RunScaledE(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3E_SCALE") ? std::max(1, std::atoi(std::getenv("W3E_SCALE"))) : 1;
    Run(name, std::forward<Fn>(fn), cases * scale);
}

#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledE(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#define EC(PORT, EXE) return ECase(r, d, [&]() -> uint64_t { PORT }, [&]() -> uint64_t { EXE })
#include "wave3_oracle_entity_a.inc"      // CEntity / CPhysical / CAnimatedBuilding rows on the entity fixture
#include "wave3_oracle_entity_b.inc"      // CPlaceable / matrix list / pool-pointer rows
#include "wave3_oracle_entity_c.inc"      // CPhysical state rows, pool-pointer validity
#include "wave3_oracle_entity_d.inc"      // occlusion, lighting early-outs

static void TestW3EntityAll() {
    TestW3EntityA();
    TestW3EntityB();
    TestW3EntityC();
    TestW3EntityD();
}
