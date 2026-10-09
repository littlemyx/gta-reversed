// S6-I: differential test of SCRIPT COMMAND HANDLERS g24 (ids 2400..2499) against the original machine code (exe oracle, see game_oracle.h).
// Same machinery as script_oracle_g20_21_test.cpp: the handler TUs (source/game_sa/Scripts/Commands/Ported/Group24a.cpp / Group24b.cpp) are #included, the exe's group
// processor (ProcessCommands2400To2499 @0x478000) and the CommandParser instantiation of the C++ handler run on the same synthetic opcode buffer; ScriptSpace, IP,
// compare flag, watched memory and the log of the callee functions are compared (the exe callees are patched with host recorders, the C++ side defines the same ones).
// usage: script_oracle_g24_test.exe [-v] [-n cases] [name-substring ...]   (env RW_EXE_ORACLE=<path of gta_sa_compact.exe>)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group24a.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group24b.cpp"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------------------------------------------------------------
// stubs of the symbols the compiled game TUs reference but the test never calls
namespace notsa::standalone::detail { bool g_DataImageLoaded = true; }
namespace notsa::standalone::Fixups {
void RegisterFunction(uint32_t, void*, const char*) {}
void RegisterUnverified(uint32_t, const char*, int, bool, uint32_t) {}
}
void ReversibleHooks::RHManager::AddHookToCategory(std::string_view, HookInstallOptions, std::shared_ptr<ReversibleHook::TwoWayHook>) {}
void* CMemoryMgr::Malloc(uint32 size, uint32) { return std::malloc(size); }
void* CMemoryMgr::Malloc(uint32 size) { return std::malloc(size); }
void  CMemoryMgr::Free(void* p) { std::free(p); }

// CRunningScript members used by the parser / handlers (RunningScript.cpp is not part of this test)
uint8 CRunningScript::ScriptArgCharNextFreeBuffer = 0;
void CRunningScript::UpdateCompareFlag(bool state) { // verbatim from RunningScript.cpp
    if (m_NotFlag) state = !state;
    if (m_AndOrState == ANDOR_NONE) { m_CondResult = state; return; }
    if (m_AndOrState >= ANDS_1 && m_AndOrState <= ANDS_8) { m_CondResult &= state; m_AndOrState = (m_AndOrState == ANDS_1) ? ANDOR_NONE : m_AndOrState - 1; return; }
    if (m_AndOrState >= ORS_1 && m_AndOrState <= ORS_8) { m_CondResult |= state; m_AndOrState = (m_AndOrState == ORS_1) ? ANDOR_NONE : m_AndOrState - 1; return; }
}

std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers = {};
auto& TheCamera = StaticRef<CCamera>(0xB6F028);   // (Camera.cpp is not part of this test)

// pools: the exe and the port both read the pool pointers at the original addresses
CPedPool*     GetPedPool()     { return *reinterpret_cast<CPedPool**>(0xB74490); }
CVehiclePool* GetVehiclePool() { return *reinterpret_cast<CVehiclePool**>(0xB74494); }
CObjectPool*  GetObjectPool()  { return *reinterpret_cast<CObjectPool**>(0xB7449C); }

// ---------------------------------------------------------------------------------------------------------------------------------
// call log: the callees of the handlers are replaced by recorders on both sides
// The exe's handlers keep uninitialised stack buffers (e.g. GET_HASH_KEY's 16-char label; the port zero-initialises). Zero the stack below the oracle call so the exe side is
// deterministic and does not depend on the host compiler's frame layout (/Od vs /O2 left different junk there).
__declspec(noinline) static void ScrubStack() { volatile char junk[8192]; for (auto& b : junk) b = 0; }

static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
template<class... F> static void Log(uint32_t tag, F... f) { g_log.push_back(tag); (g_log.push_back(FB((float)f)), ...); }


static float FakeGroundZ(float x, float y) { return x * 0.25f + y * 0.125f - 7.0f; }
static void     __fastcall H_SetIsStatic(void* self, int, int flag) { Log(29, (float)(flag & 0xFF)); auto* b = (uint8_t*)self + 0x1C; *b = (uint8_t)((*b & ~4) | ((flag & 1) << 2)); }
static void     __fastcall H_AddMoving(void* self, int) { Log(30); }
void     CObject::SetIsStatic(bool f) { H_SetIsStatic(this, 0, f); }
void     CPhysical::AddToMovingList() { Log(30); }
static void* g_fakeVtblPed[256];
// ---------------------------------------------------------------------------------------------------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) { for (int i = 0; i < 4; ++i) u32(); }
    uint32_t u32() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    float    f01() { return (u32() & 0xFFFFFF) * (1.0f / 16777216.0f); }
    int      below(int n) { return (int)(u32() % (uint32_t)n); }
    // uniform in [-scale, scale]; sometimes a special value (NaN, +-0, denormal, tiny)
    float F(float scale, bool special = true) {
        if (special && below(100) < 6) {
            switch (below(5)) {
            case 0: return BF(0x7FC00000u);
            case 1: return 0.f;
            case 2: return -0.f;
            case 3: return BF(1 + (u32() & 0x7FFFFF));
            default: return 1e-20f;
            }
        }
        return (f01() * 2.0f - 1.0f) * scale;
    }
};

// opcode buffer builder
struct Op {
    std::vector<uint8_t> b;
    explicit Op(int cmd) { b.push_back(cmd & 0xFF); b.push_back((cmd >> 8) & 0x7F); }
    Op& I(int32_t v)    { b.push_back(1); for (int i = 0; i < 4; ++i) b.push_back((uint8_t)(v >> (8 * i))); return *this; }
    Op& Fl(float f)     { b.push_back(6); uint32_t u = FB(f); for (int i = 0; i < 4; ++i) b.push_back((uint8_t)(u >> (8 * i))); return *this; }
    Op& GV(uint16_t o)  { b.push_back(2); b.push_back((uint8_t)o); b.push_back((uint8_t)(o >> 8)); return *this; }   // global variable (byte offset in ScriptSpace)
    Op& LV(uint16_t i)  { b.push_back(3); b.push_back((uint8_t)i); b.push_back((uint8_t)(i >> 8)); return *this; }   // local variable (index)
    Op& Str8(const char* s) { b.push_back(9); char t[8] = {}; std::strncpy(t, s, 8); for (char c : t) b.push_back((uint8_t)c); return *this; }
};

struct Snap { std::vector<uint8_t> mem; };
struct Watch { void* p; size_t n; };

struct Ctx {
    Rng*                 rng{};
    std::vector<Watch>   watch;          // memory regions compared after the command (and restored between the two runs)
    bool                 notFlag{};
    uint32_t             lv[4]{};
    std::vector<uint8_t> opcode;
};

static int  g_cases = 3000;
static bool g_usesCleanup = false;
static bool g_verbose = false;
static std::vector<std::string> g_filters;
struct Row { std::string name; int cases = 0, bad = 0; };
static std::vector<Row> g_rows;
static CRunningScript* g_S = nullptr;

static bool Wanted(const std::string& name) {
    if (g_filters.empty()) return true;
    for (auto& f : g_filters) if (name.find(f) != std::string::npos) return true;
    return false;
}

static constexpr size_t kSpaceWatch = 256;
static auto& ScriptSpaceRef() { return *reinterpret_cast<uint8_t(*)[kSpaceWatch]>(&CTheScripts::ScriptSpace[0]); }

static void Save(std::vector<uint8_t>& out, const Ctx& c) {
    out.clear();
    auto add = [&](const void* p, size_t n) { const auto* b = (const uint8_t*)p; out.insert(out.end(), b, b + n); };
    add(&ScriptSpaceRef()[0], kSpaceWatch);
    for (auto& w : c.watch) add(w.p, w.n);
}
static void Restore(const std::vector<uint8_t>& in, const Ctx& c) {
    size_t o = 0;
    std::memcpy(&ScriptSpaceRef()[0], in.data() + o, kSpaceWatch); o += kSpaceWatch;
    for (auto& w : c.watch) { std::memcpy(w.p, in.data() + o, w.n); o += w.n; }
}

struct Outcome { std::vector<uint8_t> mem; std::vector<uint32_t> log; ptrdiff_t ip; bool cond; uint16_t andor; bool ret; };

template<eScriptCommands Cmd, auto* Fn>
static bool RunCase(Ctx& c, std::string& desc) {
    constexpr unsigned groupFn[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x472310, 0x470A90, 0, 0, 0x478000 };
    constexpr int cmdId = (int)Cmd;
    auto& S = *g_S;
    std::vector<uint8_t> op = c.opcode;
    std::vector<uint8_t> pre;
    Save(pre, c);

    auto Reset = [&] {
        std::memset(&S, 0, sizeof(S));
        S.m_BaseIP = op.data();
        S.m_IP = op.data() + 2;                // the opcode itself has been consumed by ProcessOneCommand
        S.m_NotFlag = c.notFlag;
        S.m_CondResult = false;
        S.m_AndOrState = 0;
        S.m_UsesMissionCleanup = g_usesCleanup;
        for (int i = 0; i < 4; ++i) std::memcpy(&S.m_LocalVars[i], &c.lv[i], 4);
    };
    auto Collect = [&](bool ret) {
        Outcome o;
        Save(o.mem, c);
        o.log = g_log;
        o.ip = S.m_IP - op.data();
        o.cond = S.m_CondResult;
        o.andor = S.m_AndOrState;
        o.ret = ret;
        return o;
    };

    ScrubStack();
    Reset(); g_log.clear();
    const bool exeRet = oracle::Fn<unsigned char __fastcall(CRunningScript*, int, int)>(groupFn[cmdId / 100])(&S, 0, cmdId) != 0;
    const Outcome a = Collect(exeRet);

    Restore(pre, c);
    Reset(); g_log.clear();
    const bool cppRet = notsa::script::detail::CommandParser<Cmd, Fn>(&S) != OR_CONTINUE;
    const Outcome b = Collect(cppRet);

    const bool same = a.mem == b.mem && a.log == b.log && a.ip == b.ip && a.cond == b.cond && a.andor == b.andor && a.ret == b.ret;
    if (!same) {
        char t[512];
        std::snprintf(t, sizeof(t), "mem %s log %s (%zu vs %zu) ip %td/%td cond %d/%d ret %d/%d", a.mem == b.mem ? "ok" : "DIFF", a.log == b.log ? "ok" : "DIFF", a.log.size(), b.log.size(),
                      a.ip, b.ip, a.cond, b.cond, a.ret, b.ret);
        desc = t;
        if (a.mem != b.mem) {
            for (size_t i = 0; i < a.mem.size() && i < b.mem.size(); ++i) {
                if (a.mem[i] != b.mem[i]) { std::snprintf(t, sizeof(t), " first mem diff at +0x%zx: exe %02x port %02x", i, a.mem[i], b.mem[i]); desc += t; break; }
            }
        }
        if (a.log != b.log) {
            for (size_t i = 0; i < a.log.size() || i < b.log.size(); ++i) {
                const uint32_t x = i < a.log.size() ? a.log[i] : 0xDEAD, y = i < b.log.size() ? b.log[i] : 0xDEAD;
                if (x != y) { std::snprintf(t, sizeof(t), " first log diff [%zu]: exe %08x port %08x", i, x, y); desc += t; break; }
            }
        }
    }
    return same;
}

// run N random cases of one command; `gen(ctx)` builds ctx.opcode / ctx.watch / fake entities for one case
template<eScriptCommands Cmd, auto* Fn>
static void Test(const char* name, std::function<void(Ctx&)> gen) {
    if (!Wanted(name)) return;
    Row row{ name };
    Rng rng(0xC0FFEE ^ (uint64_t)Cmd * 7919);
    for (int i = 0; i < g_cases; ++i) {
        Ctx c; c.rng = &rng; c.notFlag = rng.below(4) == 0;
        gen(c);
        std::string desc;
        row.cases++;
        if (!RunCase<Cmd, Fn>(c, desc)) {
            row.bad++;
            if (g_verbose || row.bad <= 3) {
                std::printf("  MISMATCH %-40s case %d (not=%d): %s\n", name, i, c.notFlag, desc.c_str());
                std::printf("    opcode:"); for (auto b : c.opcode) std::printf(" %02x", b); std::printf("\n");
            }
        }
    }
    g_rows.push_back(row);
    std::printf("  %-44s %6d cases  %s\n", name, row.cases, row.bad ? "MISMATCH" : "ok");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// fake entities in the real pools
static CVehicle* g_veh;     static int g_vehRef;
static CObject*  g_obj;     static int g_objRef;
static CMatrix*  g_mat;
static void* g_fakeVtbl[256];
static void* g_fakeObjVtbl[256];
static CPed*  g_ped; static int g_pedRef;
static tHandlingData* g_handling;

static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }

// fully random vehicle memory, but with the pointers the commands dereference kept valid
static void RandomVehicle(Rng& r, int type = -2) {
    RandomFill(g_veh, sizeof(CAutomobile), r);
    RandomFill(g_mat, sizeof(CMatrix), r);
    RandomFill(g_handling, sizeof(tHandlingData), r);
    *reinterpret_cast<void**>(g_veh) = g_fakeVtbl;                         // vptr
    g_veh->m_matrix = reinterpret_cast<decltype(g_veh->m_matrix)>(g_mat);   // +0x14
    g_veh->m_pHandlingData = g_handling;                                    // +0x384
    g_veh->m_pFire = reinterpret_cast<CFire*>(r.below(3) ? (uintptr_t)r.u32() : 0);
    g_veh->m_pDriver = r.below(2) ? g_ped : nullptr;
    for (auto& p : g_veh->m_apPassengers) p = r.below(2) ? nullptr : reinterpret_cast<CPed*>((uintptr_t)0x1000 + r.below(100));
    static const int types[] = { 0, 9, 0, 3, 4, 5, 1, 10, 11, 2 };
    g_veh->m_nVehicleType = (eVehicleType)(type != -2 ? type : types[r.below(10)]);
    g_veh->m_nMaxPassengers = (uint8)r.below(10);
    // sane floats (not NaN) for the ones used in arithmetic
    g_veh->m_fHealth = r.below(8) == 0 ? r.F(2000.f) : r.f01() * 1500.f;
    g_veh->m_vecMoveSpeed = CVector{ r.F(1.f, false), r.F(1.f, false), r.F(1.f, false) };
    g_veh->m_matrix->GetPosition() = CVector{ r.F(3000.f, false), r.F(3000.f, false), r.F(100.f, false) };
    g_veh->m_placement.m_vPosn = CVector{ r.F(3000.f, false), r.F(3000.f, false), r.F(100.f, false) };
}
static void RandomObject(Rng& r) {
    RandomFill(g_obj, sizeof(CObject), r);
    *reinterpret_cast<void**>(g_obj) = g_fakeObjVtbl;
    g_obj->m_vecMoveSpeed = CVector{ r.F(1.f, false), r.F(1.f, false), r.F(1.f, false) };
}

static void SetupFakeWorld() {
    // 1 vehicle + 1 object in the exe's pools (constructed with the game's own CPool code)
    auto* vp = new CVehiclePool(4, "oracle_veh");
    auto* op = new CObjectPool(4, "oracle_obj");
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp;
    *reinterpret_cast<CObjectPool**>(0xB7449C) = op;
    g_veh = vp->New();  g_vehRef = vp->GetRef(g_veh);
    g_obj = op->New();  g_objRef = op->GetRef(g_obj);
    g_mat = new CMatrix();
    g_handling = new tHandlingData();
    auto* pp = new CPedPool(4, "oracle_ped");
    *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    g_ped = pp->New(); g_pedRef = pp->GetRef(g_ped);
    g_fakeObjVtbl[0x10 / 4] = (void*)&H_SetIsStatic;     // CEntity::SetIsStatic
}


// the test doesn't link RunningScript.cpp: forward the by-reference operand decoder to the exe's own (0x464790)
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType type) { return oracle::Fn<tScriptParam* __fastcall(CRunningScript*, int, int)>(0x464790)(this, 0, (int)type); }

static CPed* g_pedP() { return g_ped; }
static void RandomPed(Rng& r) { RandomFill(g_ped, sizeof(CPed), r); *reinterpret_cast<void**>(g_ped) = g_fakeVtblPed; }


// ---------------------------------------------------------------------------------------------------------------------------------
// S6-I additions: more fake entities, the callee recorders (host function for the exe + a same-named game function for the port)
static const int kNumVeh = 3;
static CVehicle* g_fv[kNumVeh]; static int g_fvRef[kNumVeh]; static CMatrix* g_fvMat[kNumVeh];
static CObject*  g_fo[3];       static int g_foRef[3];        static CMatrix* g_foMat[3];
static uint8_t   g_fakeModelInfo[8][0x300];
static uint8_t   g_fakeSwim[0x20];
static uint32_t  Tag(const void* p) { // identity of a fake entity, so that logs do not contain host addresses
    if (!p) return 0;
    for (int i = 0; i < kNumVeh; ++i) if (p == g_fv[i]) return 1 + i;
    for (int i = 0; i < 3; ++i) if (p == g_fo[i]) return 10 + i;
    if (p == g_ped) return 20;
    if (p == g_veh) return 21;
    if (p == g_obj) return 22;
    if (p == g_S) return 23;
    return 99;
}
static uint32_t Hash32(uint32_t a, uint32_t b = 0) { uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u); h ^= h >> 15; h *= 0x85EBCA6Bu; h ^= h >> 13; return h; }

// -- CVehicle / misc game functions that the port calls and the test must define (same behaviour as the host recorder for the exe)
static int __fastcall H_GetUpgrade(void* self, int, int slot) { Log(100, (float)slot); return (int)(Hash32(slot, ((uint8_t*)self)[0x500 + (slot & 31)]) % 7) - 2; }
static int __fastcall H_GetReplacementUpgrade(void* self, int, int slot) { Log(101, (float)slot); return (int)(Hash32(slot + 77, ((uint8_t*)self)[0x500 + (slot & 31)]) % 7) - 2; }
int32 CVehicle::GetUpgrade(int32 slot) { return H_GetUpgrade(this, 0, slot); }
int32 CVehicle::GetReplacementUpgrade(int32 slot) { return H_GetReplacementUpgrade(this, 0, slot); }

static void __cdecl H_SwitchPoliceHelis(int en) { Log(102, (float)(en & 0xFF)); }
void CHeli::SwitchPoliceHelis(bool en) { H_SwitchPoliceHelis(en); }
static void __cdecl H_StoreMods() { Log(103); }
static void __cdecl H_RestoreMods() { Log(104); }
void CShopping::StoreVehicleMods() { H_StoreMods(); }
void CShopping::RestoreVehicleMods() { H_RestoreMods(); }
static void __cdecl H_ForceDeathRestart() { Log(105); }
static void __cdecl H_ResetStuffUponResurrection() { Log(106); }
void CGameLogic::ForceDeathRestart() { H_ForceDeathRestart(); }
void CGameLogic::ResetStuffUponResurrection() { H_ResetStuffUponResurrection(); }
static void __cdecl H_FailKillFrenzy() { Log(107); }
void CDarkel::FailKillFrenzy() { H_FailKillFrenzy(); }
static void __fastcall H_UnmarkAll(void*, int) { Log(108); }
static void __fastcall H_MarkNode(void*, int, float x, float y, float z) { Log(109, x, y, z); }
void CPathFind::UnMarkAllRoadNodesAsDontWander() { H_UnmarkAll(this, 0); }
void CPathFind::MarkRoadNodeAsDontWander(float x, float y, float z) { H_MarkNode(this, 0, x, y, z); }
static void __fastcall H_ClearFireFlags(void*, int) { Log(110); }
static void __fastcall H_Extinguish(void*, int, float x, float y, float z, float r) { Log(111, x, y, z, r); }
void CFireManager::ClearAllScriptFireFlags() { H_ClearFireFlags(this, 0); }
void CFireManager::ExtinguishPoint(CVector p, float r) { H_Extinguish(this, 0, p.x, p.y, p.z, r); }
static void __cdecl H_SetStat(int stat, float v) { Log(112, (float)stat, v); }
static void __cdecl H_IncStat(int stat, float v) { Log(113, (float)stat, v); }
void CStats::SetStatValue(eStats stat, float v) { H_SetStat((int)stat, v); }
void CStats::IncrementStat(eStats stat, float v) { H_IncStat((int)stat, v); }
static void __cdecl H_PlaybackAI(void* veh) { Log(114, (float)Tag(veh)); }
void CVehicleRecording::ChangeCarPlaybackToUseAI(CVehicle* v) { H_PlaybackAI(v); }
static void __fastcall H_AudioPos(void*, int, int ev, CVector* p) { Log(115, (float)(ev & 0xFFFF), p->x, p->y, p->z); }
static void __fastcall H_AudioObj(void*, int, int ev, void* o) { Log(116, (float)(ev & 0xFFFF), (float)Tag(o)); }
static void* __fastcall H_AudioAttach(void*, int, int slot, void* o) { Log(117, (float)(slot & 0xFF), (float)Tag(o)); return nullptr; }
static void __fastcall H_BeatTrack(void*, int, int p) { Log(118, (float)(p & 0xFF)); }
void CAudioEngine::ReportMissionAudioEvent(uint16 ev, CVector& p) { H_AudioPos(this, 0, ev, &p); }
void CAudioEngine::ReportMissionAudioEvent(uint16 ev, CObject* o) { H_AudioObj(this, 0, ev, o); }
CVector* CAudioEngine::AttachMissionAudioToObject(uint8 slot, CObject* o) { return (CVector*)H_AudioAttach(this, 0, slot, o); }
void CAudioEngine::PauseBeatTrack(bool p) { H_BeatTrack(this, 0, p); }
static void __cdecl H_SetUpSkip(float x, float y, float z, float a, int p5, int p6, int p7) { Log(119, x, y, z, a, (float)(p5 & 0xFF), (float)Tag((void*)p6), (float)(p7 & 0xFF)); }
void CGameLogic::SetUpSkip(CVector c, float a, bool b, CEntity* e, bool f) { H_SetUpSkip(c.x, c.y, c.z, a, b, (int)(uintptr_t)e, f); }
static void __fastcall H_SetCharCoords(void* S, int, void* ped, float x, float y, float z, int a, int b) { Log(120, (float)Tag(S), (float)Tag(ped), x, y, z, (float)(a & 0xFF), (float)(b & 0xFF)); }
void CRunningScript::SetCharCoordinates(CPed& ped, CVector p, bool a, bool b) { H_SetCharCoords(this, 0, &ped, p.x, p.y, p.z, a, b); }
static int __cdecl H_FindEnEx(const CVector2D* p, float r, int ign) { Log(121, p->x, p->y, r, (float)ign); return (int)(Hash32(FB(p->x), FB(r)) % 9) - 2; }
static void __cdecl H_SetEnExFlag(int idx, unsigned flag, int en) { Log(122, (float)idx, (float)flag, (float)(en & 0xFF)); }
int32 CEntryExitManager::FindNearestEntryExit(const CVector2D& p, float r, int32 ign) { return H_FindEnEx(&p, r, ign); }
void CEntryExitManager::SetEntryExitFlagWithIndex(int32 idx, uint32 flag, bool en) { H_SetEnExFlag(idx, flag, en); }
static void __fastcall H_AddBlood(void*, int, const CVector* p, const CVector* d, int n, float l) { Log(123, p->x, p->y, p->z, d->x, d->y, d->z, (float)n, l); }
void Fx_c::AddBlood(const CVector& p, const CVector& d, int32 n, float l) { H_AddBlood(this, 0, &p, &d, n, l); }
static int __fastcall H_DoorStatus(void* self, int, int door) { Log(124, (float)Tag((uint8_t*)self - 0x5A0), (float)door); return (int)(Hash32(door, ((uint8_t*)self)[0]) % 3); }
eDoorStatus CDamageManager::GetDoorStatus(eDoors door) const { return (eDoorStatus)H_DoorStatus((void*)this, 0, (int)door); }
static int __fastcall H_Remap(void* self, int) { Log(125, (float)Tag(self)); return (int)(Hash32(((uint8_t*)self)[0x500]) % 7) - 1; }
int32 CVehicle::GetRemapIndex() { return H_Remap(this, 0); }
static float __fastcall H_MovingOffset(void* self, int) { Log(126, (float)Tag(self)); return *(float*)((uint8_t*)self + 0x500); }
float CAutomobile::GetMovingCollisionOffset() { return H_MovingOffset(this, 0); }
static int __fastcall H_Appearance(void* self, int) { return (int)(((uint8_t*)self)[0x501] % 6); }
static int __fastcall H_CanBeDeleted(void* self, int) { return ((uint8_t*)self)[0x502] & 1; }
eVehicleAppearance CVehicle::GetVehicleAppearance() const { return (eVehicleAppearance)H_Appearance((void*)this, 0); }
bool CVehicle::CanBeDeleted() { return H_CanBeDeleted(this, 0) != 0; }
bool CVehicle::IsLawEnforcementVehicle() const { return oracle::Fn<unsigned char __fastcall(const void*, int)>(0x6D2370)(this, 0) != 0; }
static void* __fastcall H_Passenger(void* self, int) { Log(127, (float)Tag(self)); return ((uint8_t*)self)[0x503] & 1 ? (void*)g_ped : nullptr; }
CPed* CVehicle::PickRandomPassenger() { return (CPed*)H_Passenger(this, 0); }
static int __fastcall H_Say(void* self, int, int ctx, int delay, float prob, int a, int b, int c) { Log(128, (float)Tag(self), (float)(ctx & 0xFFFF), (float)delay, prob, (float)(a & 0xFF), (float)(b & 0xFF), (float)(c & 0xFF)); return 0; }
int16 CPed::Say(eGlobalSpeechContext ctx, uint32 delay, float prob, bool a, bool b, bool c) { return (int16)H_Say(this, 0, (int)ctx, delay, prob, a, b, c); }
static void* __fastcall H_GetTaskSwim(void* self, int) { return ((uint8_t*)self)[0] & 1 ? (void*)g_fakeSwim : nullptr; }
CTaskSimpleSwim* CPedIntelligence::GetTaskSwim() { return (CTaskSimpleSwim*)H_GetTaskSwim(this, 0); }
int32 CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { return oracle::Fn<int __cdecl(int, int)>(0x4839A0)(ref, (int)type); }
void CWaterLevel::SyncWater() { m_nWaterTimeOffset = CTimer::GetTimeInMS(); } // verbatim from WaterLevel.cpp (WaterLevel.cpp is not part of this test)
static float __cdecl H_GroundZ(float x, float y) { return FakeGroundZ(x, y); }
float CWorld::FindGroundZForCoord(float x, float y) { return FakeGroundZ(x, y); }
static void __cdecl H_FindObjs(unsigned model, const CVector* pt, float radius, int b2D, short* outCount, short maxCount, CEntity** out, int b, int v, int p, int o, int d) {
    Log(129, (float)model, pt->x, pt->y, pt->z, radius, (float)(b2D & 0xFF), (float)maxCount, (float)(b & 0xFF), (float)(v & 0xFF), (float)(p & 0xFF), (float)(o & 0xFF), (float)(d & 0xFF));
    const int k = (int)(Hash32(model, FB(radius)) % 4);
    for (int i = 0; i < k; ++i) out[i] = g_fo[i];
    *outCount = (short)k;
}
void CWorld::FindObjectsOfTypeInRange(uint32 model, const CVector& pt, float radius, bool b2D, int16* outCount, int16 maxCount, CEntity** out, bool b, bool v, bool p, bool o, bool d) {
    H_FindObjs(model, &pt, radius, b2D, outCount, maxCount, out, b, v, p, o, d);
}
static int __fastcall H_PickRandomCar(void*, int, int a, int b) { Log(130, (float)(a & 0xFF), (float)(b & 0xFF)); return (int)(Hash32(a & 0xFF, b & 0xFF) % 9) - 1; } // (the exe passes a dirty dword as the 2nd bool: only the low byte is meaningful)
eModelID CLoadedCarGroup::PickRandomCar(bool a, bool b) { return (eModelID)H_PickRandomCar(this, 0, a, b); }

void CRunningScript::ReadTextLabelFromScript(char* buf, uint8 len) { oracle::Fn<void __fastcall(CRunningScript*, int, char*, int)>(0x463D50)(this, 0, buf, len); } // the exe's own (RunningScript.cpp is not part of this test)
static void __cdecl H_SetHeading(unsigned id, float h) { Log(131, (float)id, h); }
void CCheckpoints::SetHeading(uint32 id, float h) { H_SetHeading(id, h); }
static void __fastcall H_DestroyFx(void*, int, void* fx) { Log(132, (float)(uintptr_t)fx); }
void FxManager_c::DestroyFxSystem(FxSystem_c* fx) { H_DestroyFx(this, 0, fx); }
static void __cdecl H_RemoveScriptFx(int id) { Log(133, (float)id); }
void CTheScripts::RemoveScriptEffectSystem(int32 id) { H_RemoveScriptFx(id); }
static void __fastcall H_CleanupRemove(void*, int, int handle, int type) { Log(134, (float)handle, (float)(type & 0xFF)); }
void CMissionCleanup::RemoveEntityFromList(int32 handle, MissionCleanUpEntityType type) { H_CleanupRemove(this, 0, handle, type); }
static uint8_t g_fakeCheckpoint[64];
static int __cdecl H_toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; } // the exe's CRT toupper (0x823A0E) needs the locale state

static void SetupFakeWorld24() {
    auto* vp = new CVehiclePool(8, "oracle_veh24");
    auto* op = new CObjectPool(8, "oracle_obj24");
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp;
    *reinterpret_cast<CObjectPool**>(0xB7449C) = op;
    for (int i = 0; i < kNumVeh; ++i) { g_fv[i] = vp->New(); g_fvRef[i] = vp->GetRef(g_fv[i]); g_fvMat[i] = new CMatrix(); }
    for (int i = 0; i < 3; ++i) { g_fo[i] = op->New(); g_foRef[i] = op->GetRef(g_fo[i]); g_foMat[i] = new CMatrix(); }
    g_veh = g_fv[0]; g_vehRef = g_fvRef[0]; g_mat = g_fvMat[0];
    g_obj = g_fo[0]; g_objRef = g_foRef[0];
    g_handling = new tHandlingData();
    auto* pp = new CPedPool(4, "oracle_ped24");
    *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    g_ped = pp->New(); g_pedRef = pp->GetRef(g_ped);
    auto** mi = reinterpret_cast<uint8_t**>(0xA9B0C8);   // CModelInfo::ms_modelInfoPtrs
    for (int i = 0; i < 8; ++i) mi[i] = g_fakeModelInfo[i];
}

static void RandomVeh24(CVehicle* v, CMatrix* m, Rng& r, int type = -2, int model = -1) {
    RandomFill(v, sizeof(CAutomobile), r);
    RandomFill(m, sizeof(CMatrix), r);
    *reinterpret_cast<void**>(v) = g_fakeVtbl;
    v->m_matrix = reinterpret_cast<decltype(v->m_matrix)>(m);
    v->m_pHandlingData = g_handling;
    v->m_pFire = nullptr;
    v->m_pDriver = nullptr;
    for (auto& p : v->m_apPassengers) p = nullptr;
    static const int types[] = { 0, 9, 0, 3, 4, 5, 1, 10, 11, 2 };
    v->m_nVehicleType = (eVehicleType)(type != -2 ? type : types[r.below(10)]);
    v->m_nModelIndex = (int16)(model >= 0 ? model : r.below(8));
    v->m_matrix->GetPosition() = CVector{ r.F(100.f, false), r.F(100.f, false), r.F(20.f, false) };
    v->m_placement.m_vPosn = CVector{ r.F(100.f, false), r.F(100.f, false), r.F(20.f, false) };
    v->m_autoPilot.m_ObstructingEntity = nullptr;
    *reinterpret_cast<uint8_t*>(reinterpret_cast<uint8_t*>(v) + 0x36) = 0x02; // entity type vehicle (+ random status bits cleared)
}
static void RandomModelInfos(Rng& r) {
    for (auto& m : g_fakeModelInfo) RandomFill(m, sizeof(m), r);
    RandomFill(reinterpret_cast<void*>(0xC2B9DC), 8 * 0xE0 + 0x20, r); // the first handling entries (m_aVehicleHandling @0xC2B9DC, stride 0xE0)
    for (int i = 0; i < 8; ++i) g_fakeModelInfo[i][0x4A] = (uint8_t)r.below(8);   // handling index (byte)
}

static void UsedObjectArrayInit(Rng& r) { for (int k = 1; k <= 3; ++k) CTheScripts::UsedObjectArray[k].nModelIndex = (eModelID)r.below(1000); }

static void TestAll24() {
    auto VehW = [](Ctx& c) { for (int i = 0; i < kNumVeh; ++i) c.watch.push_back({ g_fv[i], sizeof(CAutomobile) }); };
    auto Veh = [&](Ctx& c, int cmd, int type = -2) {
        c.watch.clear(); RandomModelInfos(*c.rng);
        for (int i = 0; i < kNumVeh; ++i) RandomVeh24(g_fv[i], g_fvMat[i], *c.rng, i == 0 ? type : -2);
        VehW(c); return Op(cmd).I(g_fvRef[0]); };
    auto Ped = [&](Ctx& c, int cmd) { RandomPed(*c.rng); c.watch = { { g_ped, sizeof(CPed) } }; return Op(cmd).I(g_pedRef); };
    auto Obj = [&](Ctx& c, int cmd) { RandomObject(*c.rng); c.watch = { { g_fo[0], sizeof(CObject) } }; return Op(cmd).I(g_foRef[0]); };
    auto Small = [](Ctx& c) { return c.rng->below(3) ? (int32_t)c.rng->below(4) : (int32_t)c.rng->u32(); };
    auto Raw = [](Ctx& c, void* p, size_t n) { c.watch.push_back({ p, n }); };
    auto Rnd32 = [](Ctx& c, uint32_t addr) { *reinterpret_cast<uint32_t*>(addr) = c.rng->u32(); };
    auto Pad = [&](Ctx& c) { const int id = c.rng->below(2); Raw(c, reinterpret_cast<void*>(0xB73458 + id * 0x134), 0x134); for (int i = 0; i < 0x134; ++i) reinterpret_cast<uint8_t*>(0xB73458 + id * 0x134)[i] = (uint8_t)c.rng->u32(); return id; };

    // ---- plain state writes
    Test<COMMAND_SET_PLAYER_DISPLAY_VITAL_STATS_BUTTON, &SetPlayerDisplayVitalStatsButton>("2400 SET_PLAYER_DISPLAY_VITAL_STATS_BUTTON", [&](Ctx& c) { const int id = Pad(c); c.opcode = Op(2400).I(id).I(Small(c)).b; });
    Test<COMMAND_SET_PLAYER_CYCLE_WEAPON_BUTTON, &SetPlayerCycleWeaponButton>("2450 SET_PLAYER_CYCLE_WEAPON_BUTTON", [&](Ctx& c) { const int id = Pad(c); c.opcode = Op(2450).I(id).I(Small(c)).b; });
    Test<COMMAND_SET_CHAR_KEEP_TASK, &SetCharKeepTask>("2401 SET_CHAR_KEEP_TASK", [&](Ctx& c) { c.opcode = Ped(c, 2401).I(Small(c)).b; });
    Test<COMMAND_SET_CHAR_FORCE_DIE_IN_CAR, &SetCharForceDieInCar>("2434 SET_CHAR_FORCE_DIE_IN_CAR", [&](Ctx& c) { c.opcode = Ped(c, 2434).I(Small(c)).b; });
    Test<COMMAND_SET_CHAR_DRUGGED_UP, &SetCharDruggedUp>("2471 SET_CHAR_DRUGGED_UP", [&](Ctx& c) { c.opcode = Ped(c, 2471).I(Small(c)).b; });
    Test<COMMAND_TASK_SET_IGNORE_WEAPON_RANGE_FLAG, &TaskSetIgnoreWeaponRangeFlag>("2463 TASK_SET_IGNORE_WEAPON_RANGE_FLAG (ped)", [&](Ctx& c) { c.opcode = Ped(c, 2463).I(Small(c)).b; });
    Test<COMMAND_SET_ONLY_CREATE_GANG_MEMBERS, &SetOnlyCreateGangMembers>("2435 SET_ONLY_CREATE_GANG_MEMBERS", [&](Ctx& c) { Raw(c, (void*)0xC0FCB3, 1); c.opcode = Op(2435).I(Small(c)).b; });
    Test<COMMAND_SET_CREATE_RANDOM_COPS, &SetCreateRandomCops>("2462 SET_CREATE_RANDOM_COPS", [&](Ctx& c) { Raw(c, (void*)0xC0FCB4, 1); c.opcode = Op(2462).I(Small(c)).b; });
    Test<COMMAND_SET_HELP_MESSAGE_BOX_SIZE, &SetHelpMessageBoxSize>("2441 SET_HELP_MESSAGE_BOX_SIZE", [&](Ctx& c) { Raw(c, (void*)0x8D0934, 4); c.opcode = Op(2441).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_GUNSHOT_SENSE_RANGE_FOR_RIOT2, &SetGunshotSenseRangeForRiot2>("2442 SET_GUNSHOT_SENSE_RANGE_FOR_RIOT2", [&](Ctx& c) { Raw(c, (void*)0x8A625C, 4); c.opcode = Op(2442).Fl(c.rng->F(500.f)).b; });
    Test<COMMAND_DRAW_CROSSHAIR, &DrawCrosshair>("2467 DRAW_CROSSHAIR", [&](Ctx& c) { Raw(c, (void*)0xA44490, 4); c.opcode = Op(2467).I(Small(c)).b; });
    Test<COMMAND_SHOW_BLIPS_ON_ALL_LEVELS, &ShowBlipsOnAllLevels>("2470 SHOW_BLIPS_ON_ALL_LEVELS", [&](Ctx& c) { Raw(c, (void*)0xA444A2, 1); c.opcode = Op(2470).I(Small(c)).b; });
    Test<COMMAND_HIDE_ALL_FRONTEND_BLIPS, &HideAllFrontendBlips>("2476 HIDE_ALL_FRONTEND_BLIPS", [&](Ctx& c) { Raw(c, (void*)0xA444A1, 1); c.opcode = Op(2476).I(Small(c)).b; });
    Test<COMMAND_DISPLAY_CAR_NAMES, &DisplayCarNames>("2489 DISPLAY_CAR_NAMES", [&](Ctx& c) { Raw(c, (void*)0xBAA3F9, 1); c.opcode = Op(2489).I(Small(c)).b; });
    Test<COMMAND_SET_MINIGAME_IN_PROGRESS, &SetMinigameInProgress>("2493 SET_MINIGAME_IN_PROGRESS", [&](Ctx& c) { Raw(c, (void*)0xA444A7, 2); Raw(c, (uint8_t*)g_S + 0xC8, 1); c.opcode = Op(2493).I(Small(c)).b; });
    Test<COMMAND_IS_MINIGAME_IN_PROGRESS, &IsMinigameInProgress>("2494 IS_MINIGAME_IN_PROGRESS", [&](Ctx& c) { *reinterpret_cast<uint8_t*>(0xA444A8) = (uint8_t)c.rng->below(2); c.opcode = Op(2494).b; });
    Test<COMMAND_SET_FORCE_RANDOM_CAR_MODEL, &SetForceRandomCarModel>("2495 SET_FORCE_RANDOM_CAR_MODEL", [&](Ctx& c) { Raw(c, (void*)0xA4448C, 4); c.opcode = Op(2495).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_IS_NIGHT_VISION_ACTIVE, &IsNightVisionActive>("2461 IS_NIGHT_VISION_ACTIVE", [&](Ctx& c) { *reinterpret_cast<uint8_t*>(0xC402B8) = (uint8_t)c.rng->below(2); c.opcode = Op(2461).b; });
    Test<COMMAND_SYNC_WATER, &SyncWater>("2417 SYNC_WATER", [&](Ctx& c) { Raw(c, (void*)0xC228A4, 4); Rnd32(c, 0xB7CB84); c.opcode = Op(2417).b; });
    Test<COMMAND_DOES_SCRIPT_FIRE_EXIST, &DoesScriptFireExist>("2419 DOES_SCRIPT_FIRE_EXIST", [&](Ctx& c) { c.opcode = Op(2419).I(Small(c)).b; });

    // ---- vehicle / ped / object queries
    Test<COMMAND_IS_BIG_VEHICLE, &IsBigVehicle>("2409 IS_BIG_VEHICLE", [&](Ctx& c) { c.opcode = Veh(c, 2409).b; });
    Test<COMMAND_HAS_TRAIN_DERAILED, &HasTrainDerailed>("2433 HAS_TRAIN_DERAILED", [&](Ctx& c) { c.opcode = Veh(c, 2433).b; });
    Test<COMMAND_IS_CAR_LOW_RIDER, &IsCarLowRider>("2414 IS_CAR_LOW_RIDER", [&](Ctx& c) { c.opcode = Veh(c, 2414).b; });
    Test<COMMAND_IS_CAR_STREET_RACER, &IsCarStreetRacer>("2415 IS_CAR_STREET_RACER", [&](Ctx& c) { c.opcode = Veh(c, 2415).b; });
    Test<COMMAND_GET_NUM_CAR_COLOURS, &GetNumCarColours>("2429 GET_NUM_CAR_COLOURS", [&](Ctx& c) { c.opcode = Veh(c, 2429).GV(4).b; });
    Test<COMMAND_GET_CAR_DOOR_LOCK_STATUS, &GetCarDoorLockStatus>("2483 GET_CAR_DOOR_LOCK_STATUS", [&](Ctx& c) { c.opcode = Veh(c, 2483).GV(4).b; });
    Test<COMMAND_SET_VEHICLE_IS_CONSIDERED_BY_PLAYER, &SetVehicleIsConsideredByPlayer>("2480 SET_VEHICLE_IS_CONSIDERED_BY_PLAYER", [&](Ctx& c) { c.opcode = Veh(c, 2480).I(Small(c)).b; });
    Test<COMMAND_SET_CAR_COLLISION, &SetCarCollision>("2458 SET_CAR_COLLISION", [&](Ctx& c) { c.opcode = Veh(c, 2458).I(Small(c)).b; });
    Test<COMMAND_IS_EMERGENCY_SERVICES_VEHICLE, &IsEmergencyServicesVehicle>("2421 IS_EMERGENCY_SERVICES_VEHICLE", [&](Ctx& c) {
        static const int models[] = { 0x1A0, 0x197, 0x220, 0x1AE, 596, 597, 598, 599, 523, 427, 490, 528, 601, 400, 0x1A1 };
        auto op = Veh(c, 2421); g_fv[0]->m_nModelIndex = (int16)models[c.rng->below(15)]; c.opcode = op.b; });
    Test<COMMAND_GET_CAR_BLOCKING_CAR, &GetCarBlockingCar>("2439 GET_CAR_BLOCKING_CAR", [&](Ctx& c) {
        auto op = Veh(c, 2439); const int k = c.rng->below(4);
        g_fv[0]->m_autoPilot.m_ObstructingEntity = k == 0 ? nullptr : (CEntity*)(k == 3 ? (void*)g_fo[0] : (void*)g_fv[k - 1]);
        c.opcode = op.GV(4).b; });
    Test<COMMAND_GET_CURRENT_CAR_MOD, &GetCurrentCarMod>("2413 GET_CURRENT_CAR_MOD", [&](Ctx& c) { auto op = Veh(c, 2413); c.opcode = op.I(c.rng->below(4) ? (int32_t)c.rng->below(20) : (int32_t)c.rng->u32()).GV(4).b; });
    Test<COMMAND_GET_CURRENT_VEHICLE_PAINTJOB, &GetCurrentVehiclePaintjob>("2440 GET_CURRENT_VEHICLE_PAINTJOB", [&](Ctx& c) { c.opcode = Veh(c, 2440).GV(4).b; });
    Test<COMMAND_GET_CAR_MOVING_COMPONENT_OFFSET, &GetCarMovingComponentOffset>("2445 GET_CAR_MOVING_COMPONENT_OFFSET", [&](Ctx& c) { c.opcode = Veh(c, 2445).GV(4).b; });
    Test<COMMAND_IS_CAR_DOOR_DAMAGED, &IsCarDoorDamaged>("2491 IS_CAR_DOOR_DAMAGED", [&](Ctx& c) { c.opcode = Veh(c, 2491).I(c.rng->below(8)).b; /* door ids only: the port's eDoors is a byte enum */ });
    Test<COMMAND_RANDOM_PASSENGER_SAY, &RandomPassengerSay>("2475 RANDOM_PASSENGER_SAY", [&](Ctx& c) { c.opcode = Veh(c, 2475).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_CHANGE_PLAYBACK_TO_USE_AI, &ChangePlaybackToUseAI>("2459 CHANGE_PLAYBACK_TO_USE_AI", [&](Ctx& c) { c.opcode = Veh(c, 2459).b; });
    Test<COMMAND_IS_CHAR_IN_ANY_TRAIN, &IsCharInAnyTrain>("2478 IS_CHAR_IN_ANY_TRAIN", [&](Ctx& c) {
        auto op = Ped(c, 2478); auto* v = g_fv[0]; RandomVeh24(v, g_fvMat[0], *c.rng, c.rng->below(2) ? 6 : -2);
        *reinterpret_cast<CVehicle**>(reinterpret_cast<uint8_t*>(g_ped) + 0x58C) = v; c.opcode = op.b; });
    Test<COMMAND_IS_CHAR_HEAD_MISSING, &IsCharHeadMissing>("2472 IS_CHAR_HEAD_MISSING", [&](Ctx& c) {
        auto op = Ped(c, 2472); if (c.rng->below(2)) reinterpret_cast<uint8_t*>(g_ped)[0x754] = 2; c.opcode = c.rng->below(8) ? op.b : Op(2472).I(-1).b; });
    Test<COMMAND_GET_OBJECT_MODEL, &GetObjectModel>("2436 GET_OBJECT_MODEL", [&](Ctx& c) { c.opcode = Obj(c, 2436).GV(4).b; });

    // ---- strings
    auto StrCase = [&](Ctx& c, int cmd) {
        auto fill = [&](int off, int maxLen) { const int n = c.rng->below(maxLen + 1); for (int i = 0; i < 16; ++i) ScriptSpaceRef()[off + i] = i < n ? (uint8_t)('A' + c.rng->below(26)) : 0; };
        fill(8, 10); fill(24, 10);
        for (int i = 0; i < 16; ++i) ScriptSpaceRef()[40 + i] = (uint8_t)('a' + c.rng->below(26));
        c.opcode = Op(cmd).GV(8).GV(24).GV(40).b; };
    Test<COMMAND_STRING_CAT16, &StringCat>("2443 STRING_CAT16", [&](Ctx& c) { StrCase(c, 2443); });
    Test<COMMAND_STRING_CAT8, &StringCat>("2444 STRING_CAT8", [&](Ctx& c) { StrCase(c, 2444); });
    Test<COMMAND_GET_HASH_KEY, &GetHashKey>("2473 GET_HASH_KEY", [&](Ctx& c) {
        char t[9] = {}; const int n = 1 + c.rng->below(8); for (int i = 0; i < n; ++i) t[i] = (char)(c.rng->below(2) ? 'a' + c.rng->below(26) : 'A' + c.rng->below(26)); c.opcode = Op(2473).Str8(t).GV(4).b; });

    // ---- calls with logs only
    Test<COMMAND_SWITCH_POLICE_HELIS, &SwitchPoliceHelisCmd>("2410 SWITCH_POLICE_HELIS", [&](Ctx& c) { c.opcode = Op(2410).I(Small(c)).b; });
    Test<COMMAND_STORE_CAR_MOD_STATE, &StoreCarModState>("2411 STORE_CAR_MOD_STATE", [&](Ctx& c) { c.opcode = Op(2411).b; });
    Test<COMMAND_RESTORE_CAR_MOD_STATE, &RestoreCarModState>("2412 RESTORE_CAR_MOD_STATE", [&](Ctx& c) { c.opcode = Op(2412).b; });
    Test<COMMAND_FORCE_DEATH_RESTART, &ForceDeathRestart>("2416 FORCE_DEATH_RESTART", [&](Ctx& c) { c.opcode = Op(2416).b; });
    Test<COMMAND_RESET_STUFF_UPON_RESURRECTION, &ResetStuffUponResurrection>("2420 RESET_STUFF_UPON_RESURRECTION", [&](Ctx& c) { c.opcode = Op(2420).b; });
    Test<COMMAND_CLEAR_ALL_SCRIPT_FIRE_FLAGS, &ClearAllScriptFireFlags>("2438 CLEAR_ALL_SCRIPT_FIRE_FLAGS", [&](Ctx& c) { c.opcode = Op(2438).b; });
    Test<COMMAND_FAIL_KILL_FRENZY, &FailKillFrenzy>("2498 FAIL_KILL_FRENZY", [&](Ctx& c) { c.opcode = Op(2498).b; });
    Test<COMMAND_UNMARK_ALL_ROAD_NODES_AS_DONT_WANDER, &UnMarkAllRoadNodesAsDontWander>("2453 UNMARK_ALL_ROAD_NODES_AS_DONT_WANDER", [&](Ctx& c) { c.opcode = Op(2453).b; });
    Test<COMMAND_MARK_ROAD_NODE_AS_DONT_WANDER, &MarkRoadNodeAsDontWander>("2452 MARK_ROAD_NODE_AS_DONT_WANDER", [&](Ctx& c) { c.opcode = Op(2452).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).b; });
    Test<COMMAND_EXTINGUISH_FIRE_AT_POINT, &ExtinguishFireAtPoint>("2432 EXTINGUISH_FIRE_AT_POINT", [&](Ctx& c) { c.opcode = Op(2432).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(50)).b; });
    Test<COMMAND_SET_MISSION_RESPECT_TOTAL, &SetMissionRespectTotal>("2455 SET_MISSION_RESPECT_TOTAL", [&](Ctx& c) { c.opcode = Op(2455).I(Small(c)).b; });
    Test<COMMAND_AWARD_PLAYER_MISSION_RESPECT, &AwardPlayerMissionRespect>("2456 AWARD_PLAYER_MISSION_RESPECT", [&](Ctx& c) { c.opcode = Op(2456).I(Small(c)).b; });
    Test<COMMAND_PAUSE_CURRENT_BEAT_TRACK, &PauseCurrentBeatTrack>("2449 PAUSE_CURRENT_BEAT_TRACK", [&](Ctx& c) { c.opcode = Op(2449).I(c.rng->below(2)).b; /* 0/1: the exe passes the raw byte to the bool parameter */ });
    Test<COMMAND_REPORT_MISSION_AUDIO_EVENT_AT_POSITION, &ReportMissionAudioEventAtPosition>("2426 REPORT_MISSION_AUDIO_EVENT_AT_POSITION", [&](Ctx& c) {
        c.opcode = Op(2426).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_REPORT_MISSION_AUDIO_EVENT_AT_OBJECT, &ReportMissionAudioEventAtObject>("2427 REPORT_MISSION_AUDIO_EVENT_AT_OBJECT", [&](Ctx& c) {
        c.opcode = Op(2427).I(c.rng->below(4) ? g_foRef[c.rng->below(3)] : -1).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_ATTACH_MISSION_AUDIO_TO_OBJECT, &AttachMissionAudioToObject>("2428 ATTACH_MISSION_AUDIO_TO_OBJECT", [&](Ctx& c) {
        c.opcode = Op(2428).I(Small(c)).I(c.rng->below(4) ? g_foRef[c.rng->below(3)] : -1).b; });
    Test<COMMAND_SET_UP_SKIP_AFTER_MISSION, &SetUpSkipAfterMission>("2479 SET_UP_SKIP_AFTER_MISSION", [&](Ctx& c) {
        c.opcode = Op(2479).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(400)).b; });
    Test<COMMAND_SET_CLOSEST_ENTRY_EXIT_FLAG, &SetClosestEntryExitFlag>("2484 SET_CLOSEST_ENTRY_EXIT_FLAG", [&](Ctx& c) {
        c.opcode = Op(2484).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(50)).I((int32_t)c.rng->u32()).I(Small(c)).b; });
    Test<COMMAND_ADD_BLOOD, &AddBlood>("2488 ADD_BLOOD", [&](Ctx& c) {
        RandomPed(*c.rng); c.watch = { { g_ped, sizeof(CPed) } };
        c.opcode = Op(2488).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(1)).Fl(c.rng->F(1)).Fl(c.rng->F(1)).I(Small(c)).I(g_pedRef).b; });
    Test<COMMAND_SET_CHAR_COORDINATES_NO_OFFSET, &SetCharCoordinatesNoOffset>("2418 SET_CHAR_COORDINATES_NO_OFFSET", [&](Ctx& c) {
        c.opcode = Ped(c, 2418).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).b; });
    Test<COMMAND_GET_RANDOM_CAR_MODEL_IN_MEMORY, &GetRandomCarModelInMemory>("2482 GET_RANDOM_CAR_MODEL_IN_MEMORY", [&](Ctx& c) {
        RandomModelInfos(*c.rng); c.opcode = Op(2482).I(Small(c)).GV(4).GV(8).b; });
    Test<COMMAND_IS_CHAR_SWIMMING, &IsCharSwimming>("2405 IS_CHAR_SWIMMING", [&](Ctx& c) {
        auto op = Ped(c, 2405); static uint8_t intel[0x294]; RandomFill(intel, sizeof(intel), *c.rng); *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C) = intel;
        c.opcode = op.b; });

    Test<COMMAND_GET_CHAR_SWIM_STATE, &GetCharSwimState>("2406 GET_CHAR_SWIM_STATE", [&](Ctx& c) {
        auto op = Ped(c, 2406); static uint8_t intel[0x294]; RandomFill(intel, sizeof(intel), *c.rng); intel[0] |= 1; RandomFill(g_fakeSwim, sizeof(g_fakeSwim), *c.rng);
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C) = intel; c.opcode = op.GV(4).b; });
    Test<COMMAND_SET_CHECKPOINT_HEADING, &SetCheckpointHeading>("2454 SET_CHECKPOINT_HEADING", [&](Ctx& c) {
        RandomFill(g_fakeCheckpoint, sizeof(g_fakeCheckpoint), *c.rng);
        auto* arr = reinterpret_cast<uint8_t*>(0xA44070); const int idx = c.rng->below(3), id = 1 + c.rng->below(1000);
        for (int i = 0; i < 3; ++i) { arr[i * 8] = (uint8_t)c.rng->below(2); *reinterpret_cast<int16_t*>(arr + i * 8 + 2) = (int16_t)id; *reinterpret_cast<void**>(arr + i * 8 + 4) = c.rng->below(4) ? (void*)g_fakeCheckpoint : nullptr; }
        const int32_t handle = c.rng->below(6) == 0 ? -1 : (c.rng->below(5) == 0 ? (int32_t)c.rng->u32() & 0x00FF0003 : ((id << 16) | idx));
        c.opcode = Op(2454).I(handle).Fl(c.rng->F(400)).b; });
    Test<COMMAND_KILL_FX_SYSTEM_NOW, &KillFxSystemNow>("2422 KILL_FX_SYSTEM_NOW", [&](Ctx& c) {
        g_usesCleanup = c.rng->below(2) != 0;
        auto* arr = reinterpret_cast<uint8_t*>(0xA44110); const int idx = c.rng->below(3), id = 1 + c.rng->below(1000);
        for (int i = 0; i < 3; ++i) { arr[i * 8] = (uint8_t)c.rng->below(2); *reinterpret_cast<int16_t*>(arr + i * 8 + 2) = (int16_t)id; *reinterpret_cast<uintptr_t*>(arr + i * 8 + 4) = c.rng->below(4) ? 0x1000 + i : 0; }
        const int32_t handle = c.rng->below(6) == 0 ? -1 : ((id << 16) | idx);
        c.opcode = Op(2422).I(handle).b; });

    // ---- scanning / searching
    Test<COMMAND_IS_COP_VEHICLE_IN_AREA_3D_NO_SAVE, &IsCopVehicleInArea3DNoSave>("2499 IS_COP_VEHICLE_IN_AREA_3D_NO_SAVE", [&](Ctx& c) {
        static const int models[] = { 596, 597, 598, 599, 523, 427, 490, 528, 601, 0x1AE, 400, 411 };
        auto op = Veh(c, 2499); for (int i = 0; i < kNumVeh; ++i) g_fv[i]->m_nModelIndex = (int16)models[c.rng->below(12)];
        auto R = [&](float s) { return c.rng->below(4) ? c.rng->F(s) : c.rng->F(s) * 0.1f; };
        c.opcode = Op(2499).Fl(R(80)).Fl(R(80)).Fl(R(15)).Fl(R(80)).Fl(R(80)).Fl(R(15)).b; });
    Test<COMMAND_GET_RANDOM_CAR_OF_TYPE_IN_ANGLED_AREA_NO_SAVE, &GetRandomCarOfTypeInAngledAreaNoSave>("2496 GET_RANDOM_CAR_OF_TYPE_IN_ANGLED_AREA_NO_SAVE", [&](Ctx& c) {
        auto op = Veh(c, 2496);
        for (int i = 0; i < kNumVeh; ++i) { reinterpret_cast<uint8_t*>(g_fv[i])[0x501] = (uint8_t)(c.rng->below(6)); reinterpret_cast<uint8_t*>(g_fv[i])[0x502] = (uint8_t)c.rng->below(2); }
        c.opcode = Op(2496).Fl(c.rng->F(80)).Fl(c.rng->F(80)).Fl(c.rng->F(80)).Fl(c.rng->F(80)).Fl(c.rng->below(4) ? 10.f + c.rng->F(60, false) : c.rng->F(60)).I(c.rng->below(2) ? -1 : (int32_t)c.rng->below(8)).GV(4).b; });
    Test<COMMAND_SET_CHAR_USES_COLLISION_CLOSEST_OBJECT_OF_TYPE, &SetCharUsesCollisionClosestObjectOfType>("2437 SET_CHAR_USES_COLLISION_CLOSEST_OBJECT_OF_TYPE", [&](Ctx& c) {
        RandomPed(*c.rng); c.watch = { { g_ped, sizeof(CPed) } };
        for (int i = 0; i < 3; ++i) { RandomObject(*c.rng); }
        for (int i = 0; i < 3; ++i) { RandomFill(g_fo[i], sizeof(CObject), *c.rng); RandomFill(g_foMat[i], sizeof(CMatrix), *c.rng);
            *reinterpret_cast<void**>(g_fo[i]) = g_fakeObjVtbl; g_fo[i]->m_matrix = reinterpret_cast<decltype(g_fo[i]->m_matrix)>(g_foMat[i]);
            g_fo[i]->m_matrix->GetPosition() = CVector{ c.rng->F(60, false), c.rng->F(60, false), c.rng->F(30, false) }; g_fo[i]->m_placement.m_vPosn = CVector{ c.rng->F(60, false), c.rng->F(60, false), c.rng->F(30, false) };
            if (c.rng->below(3) == 0) g_fo[i]->m_matrix = nullptr; }
        UsedObjectArrayInit(*c.rng);
        c.opcode = Op(2437).Fl(c.rng->F(60)).Fl(c.rng->F(60)).Fl(c.rng->below(3) ? c.rng->F(40) : -100.f - c.rng->f01() * 50.f).Fl(c.rng->F(50, false) + 55.f).I(c.rng->below(4) ? (int32_t)c.rng->below(1000) : -(int32_t)(1 + c.rng->below(3))).I(Small(c)).I(g_pedRef).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g24_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice
    oracle::Patch(0x542800, (void*)&H_AddMoving);
    oracle::Patch(0x823A0E, (void*)&H_toupper);
    oracle::Patch(0x6D3650, (void*)&H_GetUpgrade);
    oracle::Patch(0x6D3A50, (void*)&H_GetReplacementUpgrade);
    oracle::Patch(0x6C4800, (void*)&H_SwitchPoliceHelis);
    oracle::Patch(0x49B280, (void*)&H_StoreMods);
    oracle::Patch(0x49B3C0, (void*)&H_RestoreMods);
    oracle::Patch(0x441240, (void*)&H_ForceDeathRestart);
    oracle::Patch(0x442980, (void*)&H_ResetStuffUponResurrection);
    oracle::Patch(0x43DC60, (void*)&H_FailKillFrenzy);
    oracle::Patch(0x44D400, (void*)&H_UnmarkAll);
    oracle::Patch(0x450560, (void*)&H_MarkNode);
    oracle::Patch(0x5397A0, (void*)&H_ClearFireFlags);
    oracle::Patch(0x539450, (void*)&H_Extinguish);
    oracle::Patch(0x55A070, (void*)&H_SetStat);
    oracle::Patch(0x55C180, (void*)&H_IncStat);
    oracle::Patch(0x45A360, (void*)&H_PlaybackAI);
    oracle::Patch(0x507340, (void*)&H_AudioPos);
    oracle::Patch(0x507350, (void*)&H_AudioObj);
    oracle::Patch(0x507320, (void*)&H_AudioAttach);
    oracle::Patch(0x507200, (void*)&H_BeatTrack);
    oracle::Patch(0x4423C0, (void*)&H_SetUpSkip);
    oracle::Patch(0x464DC0, (void*)&H_SetCharCoords);
    oracle::Patch(0x43F4B0, (void*)&H_FindEnEx);
    oracle::Patch(0x43EF90, (void*)&H_SetEnExFlag);
    oracle::Patch(0x49EB00, (void*)&H_AddBlood);
    oracle::Patch(0x6C2230, (void*)&H_DoorStatus);
    oracle::Patch(0x6D0B70, (void*)&H_Remap);
    oracle::Patch(0x6A2150, (void*)&H_MovingOffset);
    oracle::Patch(0x6D1080, (void*)&H_Appearance);
    oracle::Patch(0x6D1180, (void*)&H_CanBeDeleted);
    oracle::Patch(0x6D2A10, (void*)&H_Passenger);
    oracle::Patch(0x5EFFE0, (void*)&H_Say);
    oracle::Patch(0x601070, (void*)&H_GetTaskSwim);
    oracle::Patch(0x569660, (void*)&H_GroundZ);
    oracle::Patch(0x564C70, (void*)&H_FindObjs);
    oracle::Patch(0x611C50, (void*)&H_PickRandomCar);
    oracle::Patch(0x722970, (void*)&H_SetHeading);
    oracle::Patch(0x4A9810, (void*)&H_DestroyFx);
    oracle::Patch(0x492FD0, (void*)&H_RemoveScriptFx);
    oracle::Patch(0x4654B0, (void*)&H_CleanupRemove);
    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld24();
    g_fakeVtbl[0x10 / 4] = (void*)&H_SetIsStatic;
    g_fakeObjVtbl[0x10 / 4] = (void*)&H_SetIsStatic;
    std::printf("script_oracle_g24_test: %d random cases per command (PC24)\n", g_cases);
    TestAll24();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
