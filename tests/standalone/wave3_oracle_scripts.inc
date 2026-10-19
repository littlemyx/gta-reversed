// ORACLE-W3-SCRIPTS: exe-oracle rows for the uncovered class A/B functions of game_sa/Scripts (CRunningScript / CTheScripts / CScriptsForBrains / CScripted2dEffects helpers, not the opcode handlers)
// on a real script fixture. Included by wave2_oracle_all.inc when W3_SCRIPTS_ONLY is defined (executable wave3_scripts_oracle_test = the W2 harness: deterministic arena, the whole exe data image +
// the scratch block compared after every case, entity reference world of logic_oracle_refs.inc compared through the Ext hooks).
//
// Fixture: CTheScripts::ScriptSpace (0xA49960), LocalVariablesForCurrentMission (0xA48960) and the other CTheScripts statics live at their exe addresses on both sides (NOTSA_ORACLE_ADDRESS_GLOBALS).
// A scratch block g_sc holds four CRunningScripts (0xE0 bytes each), a script buffer (encoded parameter streams: all eight parameter types, ArrayAccess records), a CScriptsForBrains copy,
// a fake ped / vehicle / objects / entities and out parameters. Variable spaces are filled with a mix of small values (valid array indices) and full-range garbage.
#include "Scripts/TheScripts.h"
#include "Scripts/RunningScript.h"
#include "Scripts/Scripted2dEffects.h"
#include "ScriptsForBrains.h"
#include "Scripts/SCMChunks.hpp"
#include "Pad.h"
#include "Timer.h"
#include "References.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Object/Object.h"
#include "Pools/Pools.h"
#include "PedGroups.h"
#include "Entity/Building.h"
#include "Core/Matrix.h"

namespace w3s {
using namespace w2;
constexpr size_t kScSize = 0x6000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t oS = 0x0000 /*4 x 0x100*/, oBuf = 0x1000 /*0x2000*/, oBrn = 0x3000 /*0x700*/, oOut = 0x3800 /*0x100*/, oHead = 0x3900 /*0x40*/, oPed = 0x3A00 /*0x800*/, oVeh = 0x4200 /*0xA80*/,
                 oObj = 0x4D00 /*0x200*/, oEnt = 0x4F00 /*4 x 0x100*/;
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static CRunningScript* Scr(int k) { return reinterpret_cast<CRunningScript*>(g_sc + oS + (size_t)k * 0x100); }
static uint8_t* Buf() { return g_sc + oBuf; }
static CPed* Ped() { return reinterpret_cast<CPed*>(g_sc + oPed); }
static CVehicle* Veh() { return reinterpret_cast<CVehicle*>(g_sc + oVeh); }
static CObject* Obj() { return reinterpret_cast<CObject*>(g_sc + oObj); }
static CEntity* Ent(int k) { return reinterpret_cast<CEntity*>(g_sc + oEnt + (size_t)k * 0x100); }
static uint8_t* Out() { return g_sc + oOut; }
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }

static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static std::vector<uint8_t> g_scMask;         // bytes of the scratch block that are not compared (padding the port zero-fills and the exe leaves alone)

static uint32_t Small(Rng& r) { return r.below(3000); }
static uint32_t SmallOrAny(Rng& r) { return r.below(2) ? Small(r) : r.u32(); }
// every variable the scripts can read: ScriptSpace, the mission locals, the four scripts' own locals: half small values (usable as array indices), half full-range garbage
static void FillVars(Rng& r) {
    auto* ss = reinterpret_cast<uint32_t*>(&CTheScripts::ScriptSpace[0]);
    for (size_t i = 0; i < CTheScripts::SCRIPT_SPACE_SIZE / 4; ++i) ss[i] = SmallOrAny(r);
    for (auto& v : CTheScripts::LocalVariablesForCurrentMission) v.uParam = SmallOrAny(r);
    for (int k = 0; k < 4; ++k) for (auto& v : Scr(k)->m_LocalVars) v.uParam = SmallOrAny(r);
}
static void NormScript(CRunningScript* s) {
    auto* b = reinterpret_cast<uint8_t*>(s);
    for (size_t off : { 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xD2, 0xD3, 0xD4, 0xDC }) b[off] &= 1;   // bool members: 0 / 1 as every exe store writes them
    s->m_pNext = s->m_pPrev = nullptr;
}
static void BuildS(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    for (int k = 0; k < 4; ++k) {
        CRunningScript* s = Scr(k);
        NormScript(s);
        s->m_BaseIP = Buf(); s->m_IP = Buf();
        s->m_StackDepth = (uint16)r.below(9);
        for (auto& ip : s->m_IPStack) ip = Buf() + r.below(0x1000);
    }
    CTimer::ms_fTimeStep = r.below(8) ? 0.2f + r.f01() * 3.f : GenF(r, 5.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    CTimer::ms_fOldTimeStep = r.below(8) ? 0.2f + r.f01() * 3.f : GenF(r, 5.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    CTimer::m_snTimeInMilliseconds = r.u32();
    if (g_tweak) g_tweak(r);
}

template<class PF, class EF>
static bool SCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = Prep(r, 0);
    BuildS(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    if (!g_scMask.empty()) sc.mask = g_scMask;
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef), kRf);
    Done();
    return ok;
}

// ---- encoded parameter streams (SCM): type byte + payload; the array record is ArrayAccess (base u16, index variable u16, size u8, flags u8: bit 7 = index variable is global)
struct Em {
    Rng& r; uint8_t* w; bool mission;
    void U8(uint32_t v) { *w++ = (uint8_t)v; }
    void U16(uint32_t v) { *reinterpret_cast<uint16_t*>(w) = (uint16_t)v; w += 2; }
    void U32(uint32_t v) { std::memcpy(w, &v, 4); w += 4; }
    uint32_t LocLoc() { return mission ? r.below(1000) : r.below(34); }
    void SetLocal(uint32_t loc, uint32_t v) { if (mission) CTheScripts::LocalVariablesForCurrentMission[loc].uParam = v; else Scr(0)->m_LocalVars[loc].uParam = v; }
    void Arr(bool localArray) {
        U16(localArray ? (mission ? r.below(980) : r.below(30)) : r.below(60000));
        const bool g = r.below(2);
        const uint32_t loc = g ? r.below(60000) : LocLoc();
        const uint32_t idx = r.below(mission ? 18 : 3);   // one small range for every array: index variables can be shared between the records of one stream
        U16(loc); U8(r.below(255));
        U8((r.below(4)) | (g ? 0x80 : 0));
        if (g) *reinterpret_cast<uint32_t*>(&CTheScripts::ScriptSpace[loc]) = idx; else SetLocal(loc, idx);
    }
    void Param(int kind) {   // 1..8
        U8(kind);
        switch (kind) {
        case 1: U32(r.u32()); break;
        case 2: U16(r.below(65536)); break;
        case 3: U16(LocLoc()); break;
        case 4: U8(r.u32()); break;
        case 5: U16(r.u32()); break;
        case 6: U32(r.below(3) ? r.u32() : std::bit_cast<uint32_t>(GF(r, 1000.f))); break;
        case 7: Arr(false); break;
        case 8: Arr(true); break;
        }
    }
};
} // namespace w3s

struct MaskScope { explicit MaskScope(std::vector<uint8_t> m) { w3s::g_scMask = std::move(m); } ~MaskScope() { w3s::g_scMask.clear(); } };
struct Tw { explicit Tw(std::function<void(Rng&)> f) { w3s::g_tweak = std::move(f); } ~Tw() { w3s::g_tweak = nullptr; } };
template<class Fn> static void RunScaledS(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3S_SCALE") ? std::max(1, std::atoi(std::getenv("W3S_SCALE"))) : 1;
    Run(name, std::forward<Fn>(fn), cases * scale);
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledS(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#define CC(PORT, EXE) return SCase(r, d, [&]() -> uint64_t { PORT }, [&]() -> uint64_t { EXE })
#include "wave3_oracle_scripts_a.inc"      // CRunningScript
#include "wave3_oracle_scripts_b.inc"      // CTheScripts
#include "wave3_oracle_scripts_c.inc"      // CScriptsForBrains / CScripted2dEffects

static void TestW3ScriptsAll() {
    using namespace w2;
    rf::Init(); g_tp.Init(); g_ep.Init();
    (void)spdlog::default_logger_raw();   // the registry / default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    TestW3ScriptsA();
    TestW3ScriptsB();
    TestW3ScriptsB2();
    TestW3ScriptsC();
}
