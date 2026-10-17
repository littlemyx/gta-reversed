// (f) the entity reference lists (BUG-REFS): CReferences::Init / ListSize / PruneAllReferencesInWorld and CEntity::RegisterReference / CleanUpOldReference /
// ResolveReferences / PruneReferences (0x5719B0, 0x5719E0, 0x571CC0, 0x571B70, 0x571A00, 0x571A40, 0x571A90) against the exe's machine code.
// Fixture: three fake pools (peds / vehicles / objects: hand-made slot states, the exe's element strides 0x7C4 / 0xA18 / 0x19C), a few buildings and loose entities that are in no pool,
// and an array of "holder" pointers (the CEntity* members of other objects). Port and exe work on the SAME statics (the 3000-node CReference pool, pEmptyList, the pool globals):
// snapshot -> port -> snapshot -> restore -> exe -> snapshot -> compare (every node, the free list, all entity storage, all holders), one random operation after the other.
// Some cases start with an (almost) exhausted node pool so that RegisterReference's prune-all-pools path (reverse slot order: peds, vehicles, objects) and the silent drop run.
#include "References.h"
#include "Reference.h"
#include "Pools/ObjectPool.h"

namespace rf {
constexpr int kP = 10, kV = 8, kO = 8, kB = 6, kX = 3;   // ped / vehicle / object slots, buildings, loose entities (in no pool)
constexpr int kOpHold = 48, kBurnHold = 3000, kHold = kOpHold + kBurnHold;
constexpr size_t kBStride = 0x100;
static_assert(sizeof(CCopPed) == 0x7C4 && sizeof(CHeli) == 0xA18 && sizeof(CCutsceneObject) == 0x19C, "the exe's pool strides (0x571BD2 / 0x571C1D / 0x571C60)");
static bool inited = false;
static uint8 *pedStore, *vehStore, *objStore, *pedSt, *vehSt, *objSt, *bStore, *xStore;
static CPedPool* pp; static CVehiclePool* vp; static CObjectPool* op;
static CPedPool* savedPp; static CVehiclePool* savedVp; static CObjectPool* savedOp;
static CEntity** H;
static std::vector<CEntity*> All;      // every entity the operations may target
static std::vector<Snap> snaps;

static void* Zc(size_t n) { return std::calloc(1, (n + 63) & ~size_t(63)); }
static CEntity* PedE(int i) { return reinterpret_cast<CEntity*>(pedStore + (size_t)i * sizeof(CCopPed)); }
static CEntity* VehE(int i) { return reinterpret_cast<CEntity*>(vehStore + (size_t)i * sizeof(CHeli)); }
static CEntity* ObjE(int i) { return reinterpret_cast<CEntity*>(objStore + (size_t)i * sizeof(CCutsceneObject)); }
static CEntity* BldE(int i) { return reinterpret_cast<CEntity*>(bStore + (size_t)i * kBStride); }
static CEntity* LooseE(int i) { return reinterpret_cast<CEntity*>(xStore + (size_t)i * kBStride); }

static void Init() {
    if (inited) return;
    inited = true;
    pedStore = (uint8*)Zc(kP * sizeof(CCopPed)); vehStore = (uint8*)Zc(kV * sizeof(CHeli)); objStore = (uint8*)Zc(kO * sizeof(CCutsceneObject));
    pedSt = (uint8*)Zc(kP); vehSt = (uint8*)Zc(kV); objSt = (uint8*)Zc(kO);
    bStore = (uint8*)Zc(kB * kBStride); xStore = (uint8*)Zc(kX * kBStride);
    H = (CEntity**)Zc(kHold * sizeof(CEntity*));
    pp = new (Zc(sizeof(CPedPool))) CPedPool(kP, pedStore, pedSt);
    vp = new (Zc(sizeof(CVehiclePool))) CVehiclePool(kV, vehStore, vehSt);
    op = new (Zc(sizeof(CObjectPool))) CObjectPool(kO, objStore, objSt);
    for (int i = 0; i < kP; ++i) All.push_back(PedE(i));
    for (int i = 0; i < kV; ++i) All.push_back(VehE(i));
    for (int i = 0; i < kO; ++i) All.push_back(ObjE(i));
    for (int i = 0; i < kB; ++i) All.push_back(BldE(i));
    for (int i = 0; i < kX; ++i) All.push_back(LooseE(i));
    snaps.emplace_back((uintptr_t)&CReferences::aRefs, sizeof(CReferences::aRefs));
    snaps.emplace_back((uintptr_t)&CReferences::pEmptyList, 4);
    snaps.emplace_back((uintptr_t)H, kHold * sizeof(CEntity*));
    snaps.emplace_back((uintptr_t)pedStore, kP * sizeof(CCopPed)); snaps.emplace_back((uintptr_t)vehStore, kV * sizeof(CHeli)); snaps.emplace_back((uintptr_t)objStore, kO * sizeof(CCutsceneObject));
    snaps.emplace_back((uintptr_t)bStore, kB * kBStride); snaps.emplace_back((uintptr_t)xStore, kX * kBStride);
}
// the pool globals belong to other test groups (lw::Init): swap ours in for the duration of one case
static void Enter() {
    savedPp = StaticRef<CPedPool*>(0xB74490); savedVp = StaticRef<CVehiclePool*>(0xB74494); savedOp = StaticRef<CObjectPool*>(0xB7449C);
    StaticRef<CPedPool*>(0xB74490) = pp; StaticRef<CVehiclePool*>(0xB74494) = vp; StaticRef<CObjectPool*>(0xB7449C) = op;
}
static void Leave() { StaticRef<CPedPool*>(0xB74490) = savedPp; StaticRef<CVehiclePool*>(0xB74494) = savedVp; StaticRef<CObjectPool*>(0xB7449C) = savedOp; }

using Blobs = std::vector<std::vector<uint8>>;
static void SaveAll(Blobs& o) { o.resize(snaps.size()); for (size_t i = 0; i < snaps.size(); ++i) { snaps[i].Save(); o[i] = snaps[i].b; } }
static void RestoreAll(const Blobs& o) { for (size_t i = 0; i < snaps.size(); ++i) { snaps[i].b = o[i]; snaps[i].Restore(); } }
static bool SameAll(const Blobs& a, const Blobs& b, std::string& d) {
    static const char* nm[] = { "aRefs", "pEmptyList", "holders", "pedStore", "vehStore", "objStore", "buildings", "loose" };
    bool ok = true;
    for (size_t i = 0; i < a.size(); ++i) if (!SameBlob(a[i].data(), b[i].data(), a[i].size())) { ok = false; d += std::string(" ") + nm[i] + DiffDesc(a[i].data(), b[i].data(), a[i].size(), "m"); }
    return ok;
}

// a fresh world: clean entity storage, random slot occupancy, building flags that decide whether RegisterReference registers, empty holders, a fresh (maybe nearly used up) node pool
static void Reset(Rng& r, int burnMode) {
    std::memset(pedStore, 0, kP * sizeof(CCopPed)); std::memset(vehStore, 0, kV * sizeof(CHeli)); std::memset(objStore, 0, kO * sizeof(CCutsceneObject));
    std::memset(bStore, 0, kB * kBStride); std::memset(xStore, 0, kX * kBStride); std::memset(H, 0, kHold * sizeof(CEntity*));
    for (int i = 0; i < kP; ++i) { pedSt[i] = r.below(4) ? (uint8)r.below(0x7F) : 0x80; PedE(i)->SetTypePed(); }
    for (int i = 0; i < kV; ++i) { vehSt[i] = r.below(4) ? (uint8)r.below(0x7F) : 0x80; VehE(i)->SetTypeVehicle(); }
    for (int i = 0; i < kO; ++i) { objSt[i] = r.below(4) ? (uint8)r.below(0x7F) : 0x80; ObjE(i)->SetTypeObject(); }
    for (int i = 0; i < kB; ++i) {
        auto* e = BldE(i); e->SetTypeBuilding();
        e->SetIplIndex(r.below(3) ? 0 : (uint8)(1 + r.below(200)));
        e->m_bIsTempBuilding = r.below(5) == 0; e->m_bIsProcObject = r.below(6) == 0;
    }
    for (int i = 0; i < kX; ++i) { auto* e = LooseE(i); if (i == 0) e->SetTypeDummy(); else if (i == 1) e->SetTypeObject(); else e->SetTypePed(); }
    CReferences::Init();
    if (burnMode == 0) { return; }
    // use the node pool up: nodes are chained straight onto random entities (not via RegisterReference), each with its own holder that mostly no longer points back (prunable) or still does
    const int leave = burnMode == 1 ? (int)r.below(4) : (int)r.below(41);
    const bool allLive = r.below(4) == 0;
    const bool onlyPooled = r.below(3) == 0;                              // all stale/live refs sit on prunable (pooled) entities
    int h = kOpHold, nFree = MAX_NUM_REFERENCES;
    while (nFree > leave && h < kHold) {
        --nFree;
        CReference* n = CReferences::pEmptyList; CReferences::pEmptyList = n->m_pNext;
        CEntity* e = All[onlyPooled ? r.below(kP + kV + kO) : r.below((uint32)All.size())];
        n->m_pNext = e->m_pReferences; e->m_pReferences = n; n->m_ppEntity = &H[h];
        H[h] = (allLive || r.below(3) == 0) ? e : (r.below(2) ? nullptr : All[r.below((uint32)All.size())]);
        ++h;
    }
}

enum Kind { K_REG, K_REGDUP, K_CLEAN, K_RESOLVE, K_PRUNE, K_PRUNEALL, K_OVERWRITE };
struct Op { Kind k; CEntity* e; int h; };
}   // namespace rf

static void TestRefs() {
    using namespace rf;
    Init();

    Run("CReferences::Init 0x5719B0", [&](Rng& r, std::string& d) {
        Enter();
        static Blobs w0, w1, w2;
        for (size_t i = 0; i < 2; ++i) snaps[i].FillRandom(r);   // garbage in the node pool and in pEmptyList: Init must overwrite all of it
        SaveAll(w0);
        CReferences::Init(); SaveAll(w1);
        RestoreAll(w0);
        oracle::Fn<void __cdecl()>(0x5719B0)(); SaveAll(w2);
        Leave();
        d.clear(); return SameAll(w1, w2, d);
    }, 40);

    Run("CReferences::ListSize 0x5719E0", [&](Rng& r, std::string& d) {
        Enter();
        CReferences::Init();
        CReference* head = nullptr;                              // a prefix of the free list, a chain ending in a null next, or null
        const int mode = r.below(3);
        if (mode == 1) { head = CReferences::pEmptyList; for (int i = (int)r.below(3000); i > 0 && head->m_pNext; --i) head = head->m_pNext; }
        else if (mode == 2) { head = &CReferences::aRefs[r.below(3000)]; }
        const uint32 a = CReferences::ListSize(head), e = oracle::Fn<uint32 __cdecl(CReference*)>(0x5719E0)(head);
        Leave();
        Hit(a > 0);
        if (a != e) { d = "port " + std::to_string(a) + " exe " + std::to_string(e); return false; }
        return true;
    }, 200);

    // one random operation after the other on the shared statics
    Run("CEntity reference list ops (Register/CleanUp/Resolve/Prune/PruneAll) 0x571A00..0x571CC0", [&](Rng& r, std::string& d) {
        Enter();
        const int burn = r.below(3);                              // 0: fresh node pool, 1: almost used up, 2: used up but for a few dozen
        Reset(r, burn);
        std::vector<std::pair<CEntity*, int>> regs;               // (entity, holder) pairs that were registered, for CleanUp / duplicate registration
        static Blobs w0, w1, w2;
        bool ok = true, prunePath = false;
        for (int step = 0; step < 90 && ok; ++step) {
            Op o{ K_REG, All[r.below((uint32)All.size())], (int)r.below(kOpHold) };
            const int k = r.below(100);
            if (k < 38) { o.k = K_REG; if (r.below(5)) H[o.h] = o.e; }                                          // `m_p = e; e->RegisterReference(&m_p)` (sometimes without the assignment)
            else if (k < 46 && !regs.empty()) { o.k = K_REGDUP; auto pr = regs[r.below((uint32)regs.size())]; o.e = pr.first; o.h = pr.second; if (r.below(4)) H[o.h] = o.e; }
            else if (k < 60) { o.k = K_CLEAN; if (!regs.empty() && r.below(4)) { auto pr = regs[r.below((uint32)regs.size())]; o.e = pr.first; o.h = pr.second; } if (r.below(2)) H[o.h] = nullptr; }
            else if (k < 68) { o.k = K_RESOLVE; }
            else if (k < 80) { o.k = K_PRUNE; }
            else if (k < 86) { o.k = K_PRUNEALL; }
            else { o.k = K_OVERWRITE; H[o.h] = r.below(3) ? All[r.below((uint32)All.size())] : nullptr; }
            if ((o.k == K_REG || o.k == K_REGDUP) && !CReferences::pEmptyList) { prunePath = true; }
            SaveAll(w0);
            switch (o.k) {
            case K_REG: case K_REGDUP: o.e->RegisterReference(&H[o.h]); break;
            case K_CLEAN:     o.e->CleanUpOldReference(&H[o.h]); break;
            case K_RESOLVE:   o.e->ResolveReferences(); break;
            case K_PRUNE:     o.e->PruneReferences(); break;
            case K_PRUNEALL:  CReferences::PruneAllReferencesInWorld(); break;
            case K_OVERWRITE: break;
            }
            SaveAll(w1);
            RestoreAll(w0);
            switch (o.k) {
            case K_REG: case K_REGDUP: oracle::Fn<void __fastcall(CEntity*, int, CEntity**)>(0x571B70)(o.e, 0, &H[o.h]); break;
            case K_CLEAN:     oracle::Fn<void __fastcall(CEntity*, int, CEntity**)>(0x571A00)(o.e, 0, &H[o.h]); break;
            case K_RESOLVE:   oracle::Fn<void __fastcall(CEntity*, int)>(0x571A40)(o.e, 0); break;
            case K_PRUNE:     oracle::Fn<void __fastcall(CEntity*, int)>(0x571A90)(o.e, 0); break;
            case K_PRUNEALL:  oracle::Fn<void __cdecl()>(0x571CC0)(); break;
            case K_OVERWRITE: break;
            }
            SaveAll(w2);
            if (o.k == K_REG) { regs.emplace_back(o.e, o.h); }
            d.clear();
            if (!SameAll(w1, w2, d)) {
                static const char* kn[] = { "Register", "RegisterAgain", "CleanUpOldReference", "ResolveReferences", "PruneReferences", "PruneAllReferencesInWorld", "(holder overwrite)" };
                char t[96]; std::snprintf(t, sizeof t, "burn %d step %d %s e=%p (index %d) h=%d:", burn, step, kn[o.k], (void*)o.e, (int)(std::find(All.begin(), All.end(), o.e) - All.begin()), o.h);
                d = t + d; ok = false;
            }
        }
        Leave();
        Hit(prunePath);                                          // coverage: RegisterReference ran with an empty free list (prune path / silent drop)
        return ok;
    }, 300);
}
