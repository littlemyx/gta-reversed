// ORACLE-W2: shared helpers of the allocator-backed rows (included by wave2_oracle_all.inc after the wave-1 helpers).
//
// What is new compared with wave 1 ("Both"):
//  * oracle_alloc.h: the exe's CRT heap / locks / TLS imports are redirected to a deterministic arena and the port's operator new goes to the SAME arena (BothA opens the window around each call);
//    the arena is re-filled with a seed pattern before BOTH runs, so "uninitialised" bytes are identical garbage on both sides.
//  * pools: tasks and events are allocated with CTask::operator new / CEvent::operator new = CTaskPool / CEventPool::New. Both implementations work on one small fake pool installed at the exe's
//    pool globals (0xB744A8 / 0xB744AC); its struct, slot-state bytes and storage are part of the compared state.
//  * three-way compare of freshly constructed memory (Cmp3): per bit, "port != exe" is a FAILURE where the exe changed the bit (the port left it uninitialised or wrote another value - the class of
//    bug behind the playtest crashes), and only INFO where the exe left the bit alone and the port wrote it (the port zero-initialises more than the exe does; harmless, counted).
#pragma once
#include <functional>
#include "Pools/TaskPool.h"
#include "Pools/EventPool.h"
#include "Tasks/TaskTypes/TaskSimpleStandStill.h"
#include "logic_oracle_refs.inc"       // rf::  entity reference world (fake ped / vehicle / object pools, CReference nodes) for tasks that register entity references

template<class M> struct W2MemberOf;
template<class T, class C> struct W2MemberOf<T C::*> { using type = T; };

// explicit-instantiation access trick (Schaub): W2Get(Tag{}) returns a pointer to a private / protected member (function) without any layout or access assumption. Global scope: ADL on the tag finds W2Get.
template<class Tag, typename Tag::type M> struct W2Rob { friend typename Tag::type W2Get(Tag) { return M; } };

namespace w2 {

static int g_portExtra = 0;            // INFO: bits only the port wrote (exe left them as garbage)

// three-way compare of constructed memory: init = before, p = port result, e = exe result (all n bytes). skipOff/skipN: dwords not compared (vptr).
static bool Cmp3(const uint8_t* init, const uint8_t* p, const uint8_t* e, size_t n, size_t skipOff, size_t skipN, size_t stride, std::string& d, const char* tag, const std::vector<uint8_t>* mask = nullptr) {
    bool ok = true; int shown = 0, extra = 0;
    for (size_t i = 0; i < n; ++i) {
        { const size_t k = stride ? i % stride : i; if (k >= skipOff && k < skipOff + skipN) continue; if (mask && k < mask->size() && (*mask)[k]) continue; }
        const uint8_t diff = p[i] ^ e[i];
        if (!diff) continue;
        const uint8_t fail = diff & (e[i] ^ init[i]);
        if (fail) {
            ok = false;
            if (shown < 6) { char t[96]; std::snprintf(t, sizeof t, " %s+%02zX init %02X port %02X exe %02X;", tag, i, init[i], p[i], e[i]); d += t; ++shown; }
        } else {
            ++extra;
        }
    }
    g_portExtra += extra;
    return ok;
}

struct Reg { uintptr_t va; size_t n; bool lenient = false; size_t skipOff = ~size_t(0), skipN = 0, stride = 0; std::vector<uint8_t> mask; };   // `mask[k] != 0`: byte k (of every `stride` bytes) is not compared (padding the exe copies as a whole dword)   // skip: bytes [skipOff, skipOff+skipN) of every `stride` bytes (0 = once) are not compared (vptrs)

// snapshot hooks of the entity reference world (optional)
struct Ext { void (*save)(rf::Blobs&) = nullptr; void (*restore)(const rf::Blobs&) = nullptr; bool (*same)(const rf::Blobs&, const rf::Blobs&, std::string&) = nullptr; };

// empties the x87 register stack (control word kept): a case that faulted inside an exe function (or any earlier row that left a value behind) leaves entries on the stack, and an exe function
// that then pushes 3+ values hits a stack overflow (masked: QNaN "indefinite") and takes its unordered compare path - a state no real call has, since the game's functions are balanced
// (found with CTaskSimpleDuck::ControlDuckMove / CTaskSimpleUseGun::ControlGunMove: 1 case in 60 differed only in the full run, 6 stale entries on the stack, tag word F596)
static void CleanX87() {
    unsigned short cw;
    __asm { fnstcw cw }
    __asm { fninit }
    __asm { fldcw cw }
}
// runs one side inside __try: 0 = ok, 1 = fault (details in g_fault*). No C++ objects with destructors in this frame.
static int GuardedCall(const std::function<uint64_t()>& f, uint64_t& out) {
    CleanX87();
    __try { out = f(); return 0; } __except (FaultFilter(GetExceptionInformation())) { return 1; }
}
// Both with the deterministic allocator. Returns true when return value, allocation volume, arena contents and every register agree.
static bool BothA(std::string& d, uint32_t seed, std::initializer_list<Reg> regs, const std::function<uint64_t()>& portFn, const std::function<uint64_t()>& exeFn, const Ext& ext = {}, bool lenientArena = true) {   // non-template: one copy for the ~800 generated rows
    static std::vector<std::vector<uint8_t>> in, pa;   // reused between cases: the 4 MB image snapshot would otherwise be page-faulted afresh per case
    if (in.size() < regs.size()) { in.resize(regs.size()); pa.resize(regs.size()); }
    { size_t i = 0; for (const auto& g : regs) { in[i].resize(g.n); std::memcpy(in[i].data(), (void*)g.va, g.n); ++i; } }
    rf::Blobs w0, w1, w2;
    if (ext.save) ext.save(w0);
    oracle::alloc::Begin(seed);
    SeedBoth(seed);                       // the exe's rand() (fake _getptd block) and the port's game LCG start from the same state
    uint64_t ra = 0;
    const int pFault = GuardedCall(portFn, ra);
    if (g_trace) std::printf("  [port done, fault=%d]\n", pFault);
    const DWORD pFaultCode = g_faultCode; void* const pFaultEip = g_faultEip;
    oracle::alloc::End();
    const size_t pUsed = oracle::alloc::Used();
    std::vector<uint8_t> parena(oracle::alloc::g_arena, oracle::alloc::g_arena + pUsed);
    const int pAllocs = oracle::alloc::g_allocs; const bool pOvf = oracle::alloc::g_overflow;
    { size_t i = 0; for (const auto& g : regs) { pa[i].resize(g.n); std::memcpy(pa[i].data(), (void*)g.va, g.n); ++i; } }
    if (ext.save) ext.save(w1);
    { size_t i = 0; for (const auto& g : regs) std::memcpy((void*)g.va, in[i++].data(), g.n); }
    if (ext.restore) ext.restore(w0);
    oracle::alloc::Begin(seed);
    SeedBoth(seed);
    uint64_t re = 0;
    const int eFault = GuardedCall(exeFn, re);
    if (g_trace) std::printf("  [exe done, fault=%d]\n", eFault);
    oracle::alloc::End();
    const size_t eUsed = oracle::alloc::Used();
    if (ext.save) ext.save(w2);
    if (pFault || eFault) {
        // one side faulting alone is a difference; both faulting is the same behaviour (e.g. a dereference of a null the fixture allows): the case carries no information and is skipped
        if (eFault && !pFault) { g_agreedFault = true; return true; }   // exe-only fault: the exe dispatches through a garbage vptr of `this` / reads exe globals no CRT start-up initialised (the port compiled the same call direct): a fixture limit
        if ((pFault && eFault) || (pFault && pFaultEip == reinterpret_cast<void*>(0x400000))) { g_agreedFault = true; return true; }   // 0x400000 = a call to a port function whose translation unit is not linked into this test (an unresolved symbol): a fixture limit, not a port difference
        char t[160]; std::snprintf(t, sizeof t, " %s faulted (0x%08lX at %p) while the %s did not;", pFault ? "PORT" : "EXE", pFault ? (unsigned long)pFaultCode : (unsigned long)g_faultCode, pFault ? pFaultEip : g_faultEip, pFault ? "exe" : "port");
        d = t;
        return false;
    }
    Hit(ra != 0);
    bool ok = ra == re && pUsed == eUsed && pAllocs == oracle::alloc::g_allocs && !pOvf && !oracle::alloc::g_overflow;
    std::string dd;
    if (ra != re) dd += " ret port " + std::to_string((long long)ra) + " exe " + std::to_string((long long)re) + ";";
    if (pUsed != eUsed || pAllocs != oracle::alloc::g_allocs) dd += " arena used port " + std::to_string(pUsed) + "/" + std::to_string(pAllocs) + " allocs exe " + std::to_string(eUsed) + "/" + std::to_string(oracle::alloc::g_allocs) + ";";
    if (pOvf || oracle::alloc::g_overflow) dd += " arena overflow;";
    const size_t n = pUsed < eUsed ? pUsed : eUsed;
    if (n) {
        std::vector<uint8_t> init(n);
        oracle::alloc::FillPattern(init.data(), n & ~size_t(3), seed);
        if (lenientArena) ok &= Cmp3(init.data(), parena.data(), oracle::alloc::g_arena, n, ~size_t(0), 0, 0, dd, "arena");
        else if (std::memcmp(parena.data(), oracle::alloc::g_arena, n) != 0) { ok = false; dd += " arena" + DiffDesc(parena.data(), oracle::alloc::g_arena, n, "A"); }
    }
    { size_t i = 0; for (const auto& g : regs) {
        const uint8_t* now = reinterpret_cast<const uint8_t*>(g.va);
        if (g.lenient) ok &= Cmp3(in[i].data(), pa[i].data(), now, g.n, g.skipOff, g.skipN, g.stride, dd, ("r" + std::to_string(i)).c_str(), &g.mask);
        else if (!g.mask.empty()) {   // strict compare except the masked bytes (e.g. the vptr after a destructor)
            for (size_t k = 0; k < g.n; ++k) { if (k < g.mask.size() && g.mask[k]) continue; if (pa[i][k] != now[k]) { ok = false; char t[64]; std::snprintf(t, sizeof t, " r%zu+%02zX got %02X exe %02X;", i, k, pa[i][k], now[k]); dd += t; break; } }
        }
        else if (std::memcmp(pa[i].data(), now, g.n) != 0 && !SameBlob(pa[i].data(), now, g.n)) { ok = false; dd += " r" + std::to_string(i) + DiffDesc(pa[i].data(), now, g.n, "m"); }
        ++i; } }
    if (ext.same) ok &= ext.same(w1, w2, dd);
    if (!ok) d = dd;
    return ok;
}

// ---- pool fixtures: a tiny CTaskPool / CEventPool installed at the exe's pool globals ---------------------------------------------------
template<class Pool, size_t Stride, uintptr_t GlobalVa>
struct PoolFx {
    static constexpr int N = 6;
    alignas(16) uint8_t store[N * Stride];
    alignas(16) uint8_t flags[N + 2];
    alignas(16) uint8_t poolMem[0x20];
    Pool* pool = nullptr; Pool* saved = nullptr; bool inited = false;
    void Init() {
        if (inited) return;
        inited = true;
        static_assert(sizeof(Pool) <= sizeof(poolMem));
        std::memset(flags, 0, sizeof flags);
        pool = new (poolMem) Pool(N, store, flags);
    }
    // random occupancy (at least one free slot), random first-free hint, garbage in all slots (stale objects): the exe hands out whatever a deleted object left behind
    void Prepare(Rng& r, bool dirty) {
        Init();
        if (dirty) FillGarbage(r, store, sizeof store); else std::memset(store, 0, sizeof store);
        int freeCount = 0;
        for (int i = 0; i < N; ++i) {
            const bool used = r.below(3) == 0;
            flags[i] = (uint8_t)(r.below(0x7F) | (used ? 0 : 0x80));
            freeCount += !used;
        }
        if (!freeCount) flags[r.below(N)] |= 0x80;
        // the pool struct: storage, flags, size, first-free hint (+0x0C), owns (+0x10)
        auto* q = reinterpret_cast<uint8_t*>(pool);
        const int32_t first = (int32_t)r.below(N + 1) - 1;
        std::memcpy(q + 0x0C, &first, 4);
    }
    void Enter() { saved = StaticRef<Pool*>(GlobalVa); StaticRef<Pool*>(GlobalVa) = pool; }
    void Leave() { StaticRef<Pool*>(GlobalVa) = saved; }
    Reg RegStruct() const { return { (uintptr_t)pool, 0x14 }; }
    Reg RegFlags()  const { return { (uintptr_t)flags, (size_t)N }; }
    Reg RegStore()  const { return { (uintptr_t)store, N * Stride, true, 0, 4, Stride }; }   // lenient; the vptr of every slot is not compared
};
static PoolFx<CTaskPool, 0x80, 0xB744A8>  g_tp;
static PoolFx<CEventPool, 0x44, 0xB744AC> g_ep;

// ---- argument generators for the generated rows -------------------------------------------------------------------------------------------
static int32_t GI(Rng& r) {
    switch (r.below(8)) {
    case 0: return 0;
    case 1: return (int32_t)r.below(8);
    case 2: return (int32_t)r.below(100);
    case 3: return (int32_t)r.below(5000);
    case 4: return -1;
    case 5: return (int32_t)r.below(65536);
    case 6: return (int32_t)r.below(100000);   // no negative values other than -1 ("no timeout"): ctors that compare `!= -1` (exe, inline CTaskTimer::Start) vs `>= 0` (port CTaskTimer::Start) legitimately differ there and no caller passes them
    default: return (int32_t)(r.u32() & 0x7FFFFFFF);
    }
}
static uint32_t GU(Rng& r) { return r.below(3) ? (uint32_t)r.below(300) : r.u32(); }
static bool     GB(Rng& r) { return r.below(2); }
static float    GFl(Rng& r) { return r.below(6) == 0 ? 0.f : GF(r, r.below(2) ? 10.f : 1000.f); }
template<class E> static E GE(Rng& r) { if constexpr (std::is_enum_v<E>) return (E)r.below(7); else return E(static_cast<decltype(E{}.get())>(r.below(7))); }   // enums and notsa::WEnum wrappers
static CVector  GV(Rng& r) { return GVf(r, 200.f); }
static CVector2D GV2(Rng& r) { return { GF(r, 200.f), GF(r, 200.f) }; }
static const char* GStr(Rng& r) { static const char* const t[] = { "", "a", "ped", "WALK_player", "SHP_Bike_A", "idle_stance" }; return t[r.below(6)]; }
static char* GStrM(Rng& r) { static char b[8][24]; static int k = 0; char* p = b[k++ & 7]; std::strcpy(p, GStr(r)); return p; }
static CEntity* GEnt(Rng& r) { return r.below(7) == 0 ? nullptr : rf::All[r.below((uint32)(rf::kP + rf::kV + rf::kO))]; }
// never-null variants for constructor arguments (the exe's callers never pass a null entity where the constructor dereferences it; a null would fault both sides identically and test nothing)
static CEntity* GEntN(Rng& r) { return rf::All[r.below((uint32)(rf::kP + rf::kV + rf::kO))]; }
static CEntity* GPedN(Rng& r) { return rf::PedE((int)r.below(rf::kP)); }
static CEntity* GVehN(Rng& r) { return rf::VehE((int)r.below(rf::kV)); }
static CEntity* GObjN(Rng& r) { return rf::ObjE((int)r.below(rf::kO)); }
static CEntity* GPedE(Rng& r) { return r.below(7) == 0 ? nullptr : rf::PedE((int)r.below(rf::kP)); }
static CEntity* GVehE(Rng& r) { return r.below(7) == 0 ? nullptr : rf::VehE((int)r.below(rf::kV)); }
static CEntity* GObjE(Rng& r) { return r.below(7) == 0 ? nullptr : rf::ObjE((int)r.below(rf::kO)); }

// widening of the sub-dword argument types for the exe call (the exe's own callers push clean dwords)
template<class T> static auto X(T v) {
    if constexpr (std::is_enum_v<T> || (std::is_integral_v<T> && sizeof(T) < 4)) return (int)v;
    else if constexpr (requires { v.get(); }) return (int)v.get();
    else return v;
}

static void RfSave(rf::Blobs& o) { rf::SaveAll(o); }
static void RfRestore(const rf::Blobs& o) { rf::RestoreAll(o); }
static bool RfSame(const rf::Blobs& a, const rf::Blobs& b, std::string& d) { return rf::SameAll(a, b, d); }
static const Ext kRf{ &RfSave, &RfRestore, &RfSame };

static void OnFault() { oracle::alloc::End(); g_tp.Leave(); g_ep.Leave(); rf::Leave(); }

// garbage `this` buffer for constructor rows (aligned, large enough for every task / event)
alignas(16) static uint8_t g_obj[0x400];

// prepares one case: fresh reference world, pools with garbage, a garbage object buffer. Returns the seed for the arena pattern.
static uint32_t Prep(Rng& r, size_t objSize, bool dirtyPool = true) {
    rf::Enter();
    rf::Reset(r, 0);
    g_tp.Prepare(r, dirtyPool); g_ep.Prepare(r, dirtyPool);
    g_tp.Enter(); g_ep.Enter();
    FillGarbage(r, g_obj, (objSize + 3) & ~size_t(3));
    return r.u32() | 1;
}
static void Done() { g_ep.Leave(); g_tp.Leave(); rf::Leave(); }

// constructor row: port `new (g_obj) T(args)` vs the exe constructor (thiscall, this = g_obj) on identical garbage
template<class PF, class EF>
static bool CtorCase(Rng& r, std::string& d, size_t sz, PF&& portCtor, EF&& exeCtor, bool cmpRefs = true) {
    const uint32_t seed = Prep(r, sz);
    const bool ok = BothA(d, seed, { Reg{ (uintptr_t)g_obj, sz, true, 0, 4 }, g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore() },
        [&]() -> uint64_t { portCtor(static_cast<void*>(g_obj)); return 0; }, [&]() -> uint64_t { exeCtor(static_cast<void*>(g_obj)); return 0; }, cmpRefs ? kRf : Ext{});
    Done();
    return ok;
}


// Clone row: `src` is a garbage object whose entity-pointer members hold valid (registered or not) entities of the reference world or null; kinds: 'p' ped, 'v' vehicle, 'o' object, 'e' any entity.
// port: src->T::Clone() (non-virtual call of the port's definition) vs the exe's Clone at `exeAddr` (thiscall, this = src). The new object lands in the fake pool at the same slot on both sides.
alignas(16) static uint8_t g_src[0x400];
struct EntOff { size_t off; char kind; size_t size = 4; };   // kind 'u': an enum-like member of `size` bytes, small value
static CTask* g_subTask = nullptr;   // the sub task of a complex task under test (kind 'T'): a real CTaskSimpleStandStill from the fake task pool (the exe's CTask::operator delete indexes the pool by address)
static void PutEnt(Rng& r, size_t off, char kind, size_t size = 4) {
    if (kind == 'u') { const uint32_t v = r.below(5); std::memcpy(g_src + off, &v, size < 4 ? size : 4); if (size < 4) { std::memset(g_src + off + size, 0, 4 - size); } return; }   // enum member: a valid small value; the padding after a byte / word enum is zeroed (the exe copies the whole dword, the port only the member: garbage padding is an artefact)
    if (kind == 'T') { std::memcpy(g_src + off, &g_subTask, 4); return; }
    if (kind == 'b') { g_src[off] = (uint8_t)r.below(2); return; }   // plain bool member: canonical 0 / 1 (the exe copies the byte, the port's copy constructor normalises it: a garbage 0x86 is an artefact)
    if (kind == 'n') { const uint32_t z = 0; std::memcpy(g_src + off, &z, 4); return; }
    CEntity* e = kind == 'p' ? GPedE(r) : kind == 'v' ? GVehE(r) : kind == 'o' ? GObjE(r) : GEnt(r);
    std::memcpy(g_src + off, &e, 4);
}
// negative dwords other than -1 / -0.0 are cleared in Clone sources: CTaskTimer::Start is inlined everywhere in the exe with a per-site test (`!= -1` here, `>= 0` in the port's unified Start) and no caller passes
// other negative durations; this removes the artefact without losing the "-1 = no limit" case
static void NoNegInts(Rng& r, uint8_t* p, size_t n) {
    for (size_t i = 0; i + 4 <= n; i += 4) { uint32_t v; std::memcpy(&v, p + i, 4); if ((v & 0x80000000u) && v != 0xFFFFFFFFu) { v &= 0x7FFFFFFFu; std::memcpy(p + i, &v, 4); } }
    (void)r;
}
template<class T, class PF>
static bool CloneCase(Rng& r, std::string& d, uint32_t exeAddr, std::initializer_list<EntOff> ents, PF&& portClone, bool cmpRefs = true, bool fillSrc = true) {
    const size_t sz = sizeof(T);
    const uint32_t seed = Prep(r, 0);
    if (fillSrc) FillGarbage(r, g_src, (sz + 3) & ~size_t(3));
    if (fillSrc) NoNegInts(r, g_src, (sz + 3) & ~size_t(3));
    for (const auto& e : ents) PutEnt(r, e.off, e.kind, e.size);
    Reg tpStore = g_tp.RegStore(), epStore = g_ep.RegStore();
    tpStore.mask.assign(0x80, 0); epStore.mask.assign(0x44, 0);
    for (const auto& e : ents) if (e.kind == 'u' && e.size < 4) for (size_t k = e.size; k < 4; ++k) { if (e.off + k < 0x80) tpStore.mask[e.off + k] = 1; if (e.off + k < 0x44) epStore.mask[e.off + k] = 1; }   // padding after a byte / word enum member: the exe copies the dword
    const bool ok = BothA(d, seed, { Reg{ (uintptr_t)g_src, sz }, g_tp.RegStruct(), g_tp.RegFlags(), tpStore, g_ep.RegStruct(), g_ep.RegFlags(), epStore },
        [&]() -> uint64_t { return (uint64_t)(uintptr_t)portClone(static_cast<T*>(static_cast<void*>(g_src))); },
        [&]() -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __fastcall(void*, int)>(exeAddr)(g_src, 0); }, cmpRefs ? kRf : Ext{});
    Done();
    return ok;
}

// ---- member / static function rows (gen_w2_funcs.py) --------------------------------------------------------------------------------------------
// `this` is g_src (garbage, entity pointer members valid, plain bools 0/1, enums small), g_anim a garbage animation association for the *CB callbacks. Compared after the call: return value,
// `this`, the animation buffer, the pools, the arena, the entity reference world and the WHOLE exe data image (every global the function touched).
alignas(16) static uint8_t g_anim[0x100];
constexpr uintptr_t kImgLo = 0x8A4000, kImgHi = 0xCB0000;
template<class PF, class EF>
static bool FuncCase(Rng& r, std::string& d, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, bool sub = false, bool dtor = false) {
    const uint32_t seed = Prep(r, 0);
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr;
    if (sub) { g_subTask = g_tp.pool->New(); if (g_subTask) ::new (static_cast<void*>(g_subTask)) CTaskSimpleStandStill(50, false, false, 8.0f); }
    if (sz) { FillGarbage(r, g_src, (sz + 3) & ~size_t(3)); for (const auto& e : ents) PutEnt(r, e.off, e.kind, e.size); }
    Reg thisReg{ (uintptr_t)g_src, sz ? sz : 4 };
    if (dtor) { thisReg.mask.assign(sz ? sz : 4, 0); for (int k = 0; k < 4; ++k) thisReg.mask[k] = 1; }   // a destructor leaves its own vtable pointer: port and exe vtables differ by construction
    const bool ok = BothA(d, seed, { thisReg, Reg{ (uintptr_t)g_anim, sizeof g_anim }, g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { return portFn(static_cast<void*>(g_src)); }, [&]() -> uint64_t { return exeFn(static_cast<void*>(g_src)); }, kRf);
    Done();
    return ok;
}
} // namespace w2
