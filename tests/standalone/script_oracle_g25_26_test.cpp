// S6-J: differential test of SCRIPT COMMAND HANDLERS (groups g25 + g26, ids 2500..2638) against the original machine code (exe oracle, see game_oracle.h).
// Same machinery as script_oracle_g22_test.cpp: the handler TUs (source/game_sa/Scripts/Commands/Ported/Group25_26a.cpp / Group25_26b.cpp / Group25_26c.cpp, whose handlers live in
// anonymous namespaces) are #included, the test maps gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original addresses and, for every random case, runs
//   (1) the exe's group processor (ProcessCommands2500To2599 @0x47A760 / ProcessCommands2600To2699 @0x479DA0, thiscall(this = fake CRunningScript, cmd)) on a synthetic opcode buffer, and
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then compares the script variables (ScriptSpace + the first local variables), the script params, the IP, the compare flag, the watched memory and the log of the callee functions
// the handler invoked (the exe callees are patched with host recorders, the C++ side defines the same recorders for the game functions).
// Not covered here (task objects need the task pools): TASK_HAND_GESTURE (2589), TASK_FOLLOW_PATH_NODES_TO_COORD_WITH_RADIUS (2606), and the task creating path of PLAYER_TAKE_OFF_GOGGLES (2539).
// usage: script_oracle_g25_26_test.exe [-v] [-n cases] [name-substring ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group25_26a.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group25_26b.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group25_26c.cpp"

#include <algorithm>
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

// the test doesn't link RunningScript.cpp: forward the by-reference operand decoder / the text label reader to the exe's own
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType type) { return oracle::Fn<tScriptParam* __fastcall(CRunningScript*, int, int)>(0x464790)(this, 0, (int)type); }
void CRunningScript::CollectParameters(int16 n) { oracle::Fn<void __fastcall(CRunningScript*, int, int)>(0x464080)(this, 0, n); }
void CRunningScript::StoreParameters(int16 n) { oracle::Fn<void __fastcall(CRunningScript*, int, int)>(0x464370)(this, 0, n); }
void CRunningScript::ReadTextLabelFromScript(char* buffer, uint8 len) { oracle::Fn<void __fastcall(CRunningScript*, int, char*, int)>(0x463D50)(this, 0, buffer, len); }

// pools: the exe and the port both read the pool pointers at the original addresses
CPedPool*     GetPedPool()     { return *reinterpret_cast<CPedPool**>(0xB74490); }
CVehiclePool* GetVehiclePool() { return *reinterpret_cast<CVehiclePool**>(0xB74494); }
CObjectPool*  GetObjectPool()  { return *reinterpret_cast<CObjectPool**>(0xB7449C); }

// ---------------------------------------------------------------------------------------------------------------------------------
// call log: the callees of the handlers are replaced by recorders on both sides
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static void Log(uint32_t tag) { g_log.push_back(tag); }
static void LogF(uint32_t tag, std::initializer_list<float> v) { g_log.push_back(tag); for (float f : v) g_log.push_back(FB(f)); }
static void LogU(uint32_t tag, std::initializer_list<uint32_t> v) { g_log.push_back(tag); for (auto u : v) g_log.push_back(u); }
static void LogS(const char* s) { g_log.push_back(0x5A5A); uint32_t h = 2166136261u; for (; *s; ++s) h = (h ^ (uint8_t)*s) * 16777619u; g_log.push_back(h); }

static uint32_t g_hostRet;
static CVehicle* g_veh;     static int g_vehRef;
static CObject*  g_obj;     static int g_objRef;
static CPed*     g_peds[4]; static int g_pedRefs[4];
static CPed*&    g_ped = g_peds[0];
static int&      g_pedRef = g_pedRefs[0];
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

struct Watch { void* p; size_t n; };

struct Ctx {
    Rng*                 rng{};
    std::vector<Watch>   watch;          // memory regions compared after the command (and restored between the two runs)
    bool                 notFlag{};
    bool                 cleanup{};      // CRunningScript::m_UsesMissionCleanup
    int8_t               externalType{ -1 };
    uint32_t             lv[4]{};
    std::vector<uint8_t> opcode;
};

static int  g_cases = 3000;
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

struct Outcome { std::vector<uint8_t> mem; std::vector<uint32_t> log; ptrdiff_t ip; bool cond; uint16_t andor; bool ret; uint32_t lv[4]; uint32_t sp[8]; };

template<eScriptCommands Cmd, auto* Fn>
static bool RunCase(Ctx& c, std::string& desc) {
    constexpr int cmdId = (int)Cmd;
    constexpr unsigned groupFn = cmdId < 2600 ? 0x47A760 : 0x479DA0; // ProcessCommands2500To2599 / ProcessCommands2600To2699
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
        S.m_UsesMissionCleanup = c.cleanup;
        S.m_ExternalType = c.externalType;
        for (int i = 0; i < 4; ++i) std::memcpy(&S.m_LocalVars[i], &c.lv[i], 4);
        ScriptParams.fill({});
    };
    auto Collect = [&](bool ret) {
        Outcome o;
        Save(o.mem, c);
        o.log = g_log;
        o.ip = S.m_IP - op.data();
        o.cond = S.m_CondResult;
        o.andor = S.m_AndOrState;
        o.ret = ret;
        for (int i = 0; i < 4; ++i) std::memcpy(&o.lv[i], &S.m_LocalVars[i], 4);
        for (int i = 0; i < 8; ++i) o.sp[i] = ScriptParams[i].uParam;
        return o;
    };

    const uint32_t hostRet = g_hostRet;
    Reset(); g_log.clear(); g_hostRet = hostRet;
    const bool exeRet = oracle::Fn<unsigned char __fastcall(CRunningScript*, int, int)>(groupFn)(&S, 0, cmdId) != 0;
    const Outcome a = Collect(exeRet);

    Restore(pre, c);
    Reset(); g_log.clear(); g_hostRet = hostRet;
    const bool cppRet = notsa::script::detail::CommandParser<Cmd, Fn>(&S) != OR_CONTINUE;
    const Outcome b = Collect(cppRet);

    const bool same = a.mem == b.mem && a.log == b.log && a.ip == b.ip && a.cond == b.cond && a.andor == b.andor && a.ret == b.ret && !std::memcmp(a.lv, b.lv, sizeof(a.lv));
    if (!same) {
        char t[512];
        std::snprintf(t, sizeof(t), "mem %s log %s (%zu vs %zu) ip %td/%td cond %d/%d ret %d/%d lv %s", a.mem == b.mem ? "ok" : "DIFF", a.log == b.log ? "ok" : "DIFF", a.log.size(), b.log.size(),
                      a.ip, b.ip, a.cond, b.cond, a.ret, b.ret, std::memcmp(a.lv, b.lv, sizeof(a.lv)) ? "DIFF" : "ok");
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
        Ctx c; c.rng = &rng; c.notFlag = rng.below(4) == 0; c.cleanup = rng.below(2) == 0;
        g_hostRet = rng.u32();
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
static CVehicle* g_fv[3];   static int g_fvRef[3];  static CMatrix* g_fvMat[3];
static CObject*  g_fo[3];   static int g_foRef[3];
static CMatrix*  g_mat;
static void*     g_fakeVtbl[256];
static void*     g_fakeVtblPed[256];
static void*     g_fakeObjVtbl[256];
static uint8_t   g_fakeWanted[16];
static uint8_t   g_fakePD[0x100];       // fake CPlayerPedData
static uint8_t   g_fakeIntel[0x300];    // fake CPedIntelligence
static uint8_t   g_fakeGroup[8];
static uint8_t   g_fakeModelInfo[8][0x300];
static uint32_t Tag(const void* p) { // identity of a fake entity, so that logs do not contain host addresses
    if (!p) return 0;
    for (int i = 0; i < 3; ++i) if (p == g_fv[i]) return 1 + i;
    for (int i = 0; i < 3; ++i) if (p == g_fo[i]) return 10 + i;
    for (int i = 0; i < 4; ++i) if (p == g_peds[i]) return 20 + i;
    if (p == g_fakeWanted) return 30;
    if (p == g_fakeIntel) return 31;
    if (p == g_fakeGroup) return 32;
    if (p == g_S) return 33;
    return 99;
}
static uint32_t Hash32(uint32_t a, uint32_t b = 0) { uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u); h ^= h >> 15; h *= 0x85EBCA6Bu; h ^= h >> 13; return h; }
static uint32_t HashS(const char* s) { uint32_t h = 2166136261u; for (; *s; ++s) h = (h ^ (uint8_t)*s) * 16777619u; return h; }

// ---------------------------------------------------------------------------------------------------------------------------------
// host recorders for the exe (cdecl / thiscall: __fastcall(this, edx, args...) pops the stack args like a thiscall callee) and the same-named game functions for the port
static bool  __fastcall H_HasCollided(void* self, int, void* e) { LogU(1, { Tag(self), Tag(e) }); return (g_hostRet >> 3) & 1; }
bool CPhysical::GetHasCollidedWith(CEntity* e) { return H_HasCollided(this, 0, e); }
static int   __cdecl H_GetActualPickupIndex(int h) { LogU(2, { (uint32_t)h }); return (int)(Hash32(h, g_hostRet) % 5) - 2; }
int32 CPickups::GetActualPickupIndex(tPickupReference h) { return H_GetActualPickupIndex(h.num); }
static void* __cdecl H_FindPlayerWanted(int id) { LogU(3, { (uint32_t)id }); return g_fakeWanted; }
CWanted* FindPlayerWanted(int32 id) { return (CWanted*)H_FindPlayerWanted(id); }
static void  __fastcall H_ClearWanted(void* self, int) { LogU(4, { Tag(self) }); }
void CWanted::ClearWantedLevelAndGoOnParole() { H_ClearWanted(this, 0); }
static void* __cdecl H_FindPlayerPed(int id) { LogU(5, { (uint32_t)id }); return g_peds[(unsigned)(id + 1) % 4]; }
CPlayerPed* FindPlayerPed(int32 id) { return (CPlayerPed*)H_FindPlayerPed(id); }
static void  __cdecl H_UseDetonator(void* ped) { LogU(6, { Tag(ped) }); }
void CWorld::UseDetonator(CPed* ped) { H_UseDetonator(ped); }
static void  __cdecl H_SetColumnWidth(int menu, int col, int width) { LogU(7, { (uint32_t)(uint8_t)menu, (uint32_t)(uint8_t)col, (uint32_t)(uint16_t)width }); }
void CMenuSystem::SetColumnWidth(MenuId id, uint8 col, uint16 width) { H_SetColumnWidth(id, col, width); }
static int   __fastcall H_CountMembers(void* self, int) { LogU(8, { (uint32_t)((uint8_t*)self - (uint8_t*)0xC09928) }); return (int)(Hash32((uint32_t)(uintptr_t)self, g_hostRet) % 9) - 3; }
int32 CPedGroupMembership::CountMembersExcludingLeader() { return H_CountMembers(this, 0); }
static void  __fastcall H_RemoveNFollowers(void* self, int, int n) { LogU(9, { (uint32_t)((uint8_t*)self - (uint8_t*)0xC09928), (uint32_t)n }); }
void CPedGroupMembership::RemoveNFollowers(size_t n) { H_RemoveNFollowers(this, 0, (int)n); }
static void  __cdecl H_SetUpSkip(float x, float y, float z, float a, int p5, int p6, int p7) { LogF(10, { x, y, z, a, (float)(p5 & 0xFF), (float)Tag((void*)p6), (float)(p7 & 0xFF) }); }
void CGameLogic::SetUpSkip(CVector c, float a, bool b, CEntity* e, bool f) { H_SetUpSkip(c.x, c.y, c.z, a, b, (int)(uintptr_t)e, f); }
static int   __cdecl H_CreateCarGenerator(float x, float y, float z, float a, int model, int c1, int c2, int force, int alarm, int lock, int minD, int maxD, int ipl, int ign) {
    LogF(11, { x, y, z, a }); LogU(11, { (uint32_t)model, (uint32_t)(uint16_t)c1, (uint32_t)(uint16_t)c2, (uint32_t)(uint8_t)force, (uint32_t)(uint8_t)alarm, (uint32_t)(uint8_t)lock, (uint32_t)(uint16_t)minD, (uint32_t)(uint16_t)maxD, (uint32_t)(uint8_t)ipl, (uint32_t)(uint8_t)ign });
    return (int)(g_hostRet & 0xFFFF) - 5; }
int32 CTheCarGenerators::CreateCarGenerator(CVector p, float a, int32 model, int16 c1, int16 c2, uint8 force, uint8 alarm, uint8 lock, uint16 minD, uint16 maxD, uint8 ipl, bool ign) {
    return H_CreateCarGenerator(p.x, p.y, p.z, a, model, c1, c2, force, alarm, lock, minD, maxD, ipl, ign); }
static void  __fastcall H_PlateAdd(void* self, int, int id, const char* s) { LogU(12, { (uint32_t)((uint8_t*)self - (uint8_t*)0xC279D8), (uint32_t)id }); LogS(s); }
void CSpecialPlateHandler::Add(int32 id, const char* s) { H_PlateAdd(this, 0, id, s); }
static void  __cdecl H_AddLight(int type, float px, float py, float pz, float dx, float dy, float dz, float radius, float r, float g, float b, int fog, int extra, void* ent) {
    LogF(13, { (float)(type & 0xFF), px, py, pz, dx, dy, dz, radius, r, g, b, (float)(fog & 0xFF), (float)(extra & 0xFF), (float)Tag(ent) }); }
void CPointLights::AddLight(uint8 type, CVector p, CVector d, float radius, float r, float g, float b, uint8 fog, bool extra, CEntity* ent) {
    H_AddLight(type, p.x, p.y, p.z, d.x, d.y, d.z, radius, r, g, b, fog, extra, ent); }
static void  __cdecl H_EnableBurglary(int en) { LogU(14, { (uint32_t)(uint8_t)en }); }
void CEntryExitManager::EnableBurglaryHouses(bool en) { H_EnableBurglary(en); }
static int   __cdecl H_GetActualScriptThingIndex(int ref, int type) { LogU(15, { (uint32_t)ref, (uint32_t)type }); return (int)(Hash32(ref, type + g_hostRet) % (type == 2 ? 11 : 26)) - 3; /* (searchlights: the array has 8 entries, keep the stray writes close) */ }
int32 CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { return H_GetActualScriptThingIndex(ref, (int)type); }
static void* __cdecl H_DMGetInstance() { Log(16); return g_fakeGroup; }
CDecisionMakerTypes* CDecisionMakerTypes::GetInstance() { return (CDecisionMakerTypes*)H_DMGetInstance(); }
static void  __fastcall H_Nitrous(void* self, int, int n) { LogU(17, { Tag(self), (uint32_t)(uint8_t)n }); }
void CAutomobile::NitrousControl(int8 n) { H_Nitrous(this, 0, n); }
static bool  __cdecl H_IsNextStationAllowed(void* t) { LogU(18, { Tag(t) }); return (g_hostRet >> 5) & 1; }
bool CTrain::IsNextStationAllowed(CTrain* t) { return H_IsNextStationAllowed(t); }
static bool  __fastcall H_LanguageChanged(void* self, int) { Log(19); return (g_hostRet >> 6) & 1; }
bool CMenuManager::HasLanguageChanged() { return H_LanguageChanged(this, 0); }
static bool  __cdecl H_GangWar() { Log(20); return (g_hostRet >> 7) & 1; }
bool CGangWars::GangWarFightingGoingOn() { return H_GangWar(); }
static void  __cdecl H_ManagePop() { Log(21); }
void CPopulation::ManageAllPopulation() { H_ManagePop(); }
static void  __fastcall H_PlayAnim(void* self, int, int cmd) { LogU(22, { Tag(self), (uint32_t)cmd }); }
void CRunningScript::PlayAnimScriptCommand(int32 cmd) { H_PlayAnim(this, 0, cmd); }
static void* __fastcall H_JetPack(void* self, int) { LogU(23, { Tag(self) }); return ((uint8_t*)self)[0x2F0] & 1 ? g_fakeIntel + 8 : nullptr; }
CTaskSimpleJetPack* CPedIntelligence::GetTaskJetPack() { return (CTaskSimpleJetPack*)H_JetPack(this, 0); }
static void  __cdecl H_Highlight(int menu, int item, int bought) { LogU(24, { (uint32_t)(uint8_t)menu, (uint32_t)(uint8_t)item, (uint32_t)((uint8_t)bought != 0) }); /* (the exe stores the raw byte of `bought`, the port a bool) */ }
void CMenuSystem::HighlightOneItem(MenuId id, uint8 item, bool bought) { H_Highlight(id, item, bought); }
static int   __cdecl H_CarColourFromGrid(int menu, int idx) { LogU(25, { (uint32_t)(uint8_t)menu, (uint32_t)(uint8_t)idx }); return (int)(Hash32(menu & 0xFF, idx & 0xFF) & 0xFF); }
uint8 CMenuSystem::GetCarColourFromGrid(MenuId id, uint8 idx) { return (uint8)H_CarColourFromGrid(id, idx); }
static uint8_t g_txt[4][16] = { "TXT_A", "TXT_BB", "TXT_CCC", "DUMMY_T" };
static const uint8_t* __fastcall H_TextGet(void* self, int, const char* key) { LogU(26, { (uint32_t)((uint8_t*)self - (uint8_t*)0xC1B340) }); LogS(key); return g_txt[Hash32(HashS(key)) & 3]; }
const GxtChar* CText::Get(const char* key) { return H_TextGet(this, 0, key); }
static float __cdecl H_StringWidth(const uint8_t* s, int full, int script) { LogU(27, { HashS((const char*)s), (uint32_t)(uint8_t)full, (uint32_t)(uint8_t)script });
    const uint32_t h = Hash32(HashS((const char*)s), g_hostRet); switch (h & 7) { case 0: return BF(0x7FC00000u); case 1: return 3.0e9f; case 2: return -3.0e9f; case 3: return -(float)(h & 0xFFF) - 0.5f; default: return (float)(h & 0xFFFF) * 0.37f; } }
float CFont::GetStringWidth(const GxtChar* s, bool full, bool script) { return H_StringWidth(s, full, script); }
static void  __cdecl H_InsertNumber(const uint8_t* src, int n1, int n2, int n3, int n4, int n5, int n6, uint8_t* dst) {
    LogU(28, { HashS((const char*)src), (uint32_t)n1, (uint32_t)n2, (uint32_t)n3, (uint32_t)n4, (uint32_t)n5, (uint32_t)n6 }); std::snprintf((char*)dst, 32, "N%d_%d", n1, (int)(HashS((const char*)src) & 0xFF)); }
void CMessages::InsertNumberInString(const GxtChar* src, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6, GxtChar* dst) { H_InsertNumber(src, n1, n2, n3, n4, n5, n6, dst); }
static bool  __cdecl H_HelpDisplayed() { Log(29); return (g_hostRet >> 9) & 1; }
bool CHud::HelpMessageDisplayed() { return H_HelpDisplayed(); }
static void  __cdecl H_StringCopy(uint8_t* d, const uint8_t* s, int len) { LogU(30, { HashS((const char*)s), (uint32_t)(uint16_t)len }); std::snprintf((char*)d, 64, "%s", (const char*)s); }
void CMessages::StringCopy(GxtChar* d, const GxtChar* s, uint16 len) { H_StringCopy(d, s, len); }
static void  __cdecl H_InsertKeys(uint8_t* s) { LogU(31, { HashS((const char*)s) }); }
void CMessages::InsertPlayerControlKeysInString(GxtChar* s) { H_InsertKeys(s); }
static unsigned __cdecl H_StringLength(const uint8_t* s) { LogU(32, { HashS((const char*)s) }); return (unsigned)std::strlen((const char*)s); }
uint32 CMessages::GetStringLength(const GxtChar* s) { return H_StringLength(s); }
static bool  __cdecl H_StringCompare(const uint8_t* a, const uint8_t* b, int len) { LogU(33, { HashS((const char*)a), HashS((const char*)b), (uint32_t)(uint16_t)len }); return (g_hostRet >> 11) & 1; }
bool CMessages::StringCompare(const GxtChar* a, const GxtChar* b, uint16 len) { return H_StringCompare(a, b, len); }
static void  __fastcall H_NeverFollow(void* self, int, int en) { LogU(34, { Tag(self), (uint32_t)(uint8_t)en }); }
void CPlayerPed::ForceGroupToNeverFollow(bool en) { H_NeverFollow(this, 0, en); }
static bool  __cdecl H_SkipWaiting() { Log(35); return (g_hostRet >> 13) & 1; }
bool CGameLogic::IsSkipWaitingForScriptToFadeIn() { return H_SkipWaiting(); }
static int   __cdecl H_UserMarkerSet(float x, float y, float z, int col) { LogF(36, { x, y, z, (float)(uint8_t)col }); /* (the callee only uses the low byte, `movzx eax, byte [esp + 8]`) */ return (int)(g_hostRet & 0xFF) - 2; }
int32 C3dMarkers::User3dMarkerSet(float x, float y, float z, eHudColours col) { return H_UserMarkerSet(x, y, z, (int)col); }
static void  __cdecl H_UserMarkerDelete(int slot) { LogU(37, { (uint32_t)slot }); }
void C3dMarkers::User3dMarkerDelete(int32 slot) { H_UserMarkerDelete(slot); }
static void  __fastcall H_SwitchBrains(void* self, int, int id, int st) { LogU(38, { (uint32_t)((uint8_t*)self - (uint8_t*)0xA90CF0), (uint32_t)(uint8_t)id, (uint32_t)(uint8_t)st }); }
void CScriptsForBrains::SwitchAllObjectBrainsWithThisID(int8 id, bool st) { H_SwitchBrains(this, 0, id, st); }
static bool  __fastcall H_IsPedDead(void* self, int, void* ped) { LogU(39, { Tag(ped) }); return (g_hostRet >> (Tag(ped) & 7)) & 1; }
bool CRunningScript::IsPedDead(CPed* ped) const { return H_IsPedDead((void*)this, 0, ped); }
static void* __cdecl H_GetPedsGroup(void* ped) { LogU(40, { Tag(ped) }); return ((g_hostRet >> 12) >> (Tag(ped) & 7)) & 1 ? g_fakeGroup : nullptr; }
CPedGroup* CPedGroups::GetPedsGroup(const CPed* ped) { return (CPedGroup*)H_GetPedsGroup((void*)ped); }
static bool  __cdecl H_IsCarModel(int model) { LogU(41, { (uint32_t)model }); return (Hash32(model, g_hostRet) & 1) != 0; }
bool CModelInfo::IsCarModel(int32 model) { return H_IsCarModel(model); }
// virtual methods called through the fake vtables (the same host function serves the exe and the port)
static void  __fastcall H_Fix(void* self, int) { LogU(42, { Tag(self) }); }
static bool  __fastcall H_TestCollision(void* self, int, int apply) { LogU(43, { Tag(self), (uint32_t)(uint8_t)apply }); return (g_hostRet >> 15) & 1; }
static void  __fastcall H_DeleteRw(void* self, int) { LogU(44, { Tag(self) }); }
static void  __fastcall H_SetModelIndex(void* self, int, int model) { LogU(45, { Tag(self), (uint32_t)model }); *reinterpret_cast<uint32_t*>((uint8_t*)self + 0x4D4) = 0xDEAD0000u | (uint32_t)(model & 0xFFFF); }
static void  __fastcall H_TakeOffGoggles(void* self, int) { LogU(46, { Tag(self) }); }
void CPed::TakeOffGoggles() { H_TakeOffGoggles(this, 0); }
// the exe's own helpers
auto& g_LoadMonitor = StaticRef<CLoadMonitor>(0xB72978);
auto& FrontEndMenuManager = StaticRef<CMenuManager>(0xBA6748);

// ---------------------------------------------------------------------------------------------------------------------------------
static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }

static void RandomVeh(CVehicle* v, CMatrix* m, Rng& r, int type = -2, int model = -1) {
    RandomFill(v, sizeof(CAutomobile), r);
    RandomFill(m, sizeof(CMatrix), r);
    *reinterpret_cast<void**>(v) = g_fakeVtbl;
    v->m_matrix = reinterpret_cast<decltype(v->m_matrix)>(m);
    v->m_pFire = nullptr;
    v->m_pDriver = nullptr;
    for (auto& p : v->m_apPassengers) p = nullptr;
    static const int types[] = { 0, 9, 0, 3, 4, 5, 1, 10, 11, 2 };
    v->m_nVehicleType = (eVehicleType)(type != -2 ? type : types[r.below(10)]);
    v->m_nModelIndex = (int16)(model >= 0 ? model : r.below(8));
}
static void RandomPed(CPed* p, Rng& r) {
    RandomFill(p, sizeof(CPed), r);
    *reinterpret_cast<void**>(p) = g_fakeVtblPed;
    p->m_matrix = nullptr;
    p->m_placement.m_vPosn = CVector{ r.F(50.f, false), r.F(50.f, false), r.F(50.f, false) };
}
static void RandomObject(CObject* o, Rng& r) {
    RandomFill(o, sizeof(CObject), r);
    *reinterpret_cast<void**>(o) = g_fakeObjVtbl;
}
static void RandomModelInfos(Rng& r) {
    for (auto& m : g_fakeModelInfo) RandomFill(m, sizeof(m), r);
    RandomFill(reinterpret_cast<void*>(0xC2B9DC), 8 * 0xE0 + 0x20, r); // the first handling entries (m_aVehicleHandling @0xC2B9DC, stride 0xE0)
    for (int i = 0; i < 8; ++i) g_fakeModelInfo[i][0x4A] = (uint8_t)r.below(8);   // handling index (byte)
}

static void SetupFakeWorld() {
    auto* pp = new CPedPool(8, "oracle_ped");
    auto* vp = new CVehiclePool(8, "oracle_veh");
    auto* op = new CObjectPool(8, "oracle_obj");
    *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp;
    *reinterpret_cast<CObjectPool**>(0xB7449C) = op;
    for (int i = 0; i < 4; ++i) { g_peds[i] = pp->New(); g_pedRefs[i] = pp->GetRef(g_peds[i]); std::memset(g_peds[i], 0, sizeof(CPed)); }
    for (int i = 0; i < 3; ++i) { g_fv[i] = vp->New(); g_fvRef[i] = vp->GetRef(g_fv[i]); g_fvMat[i] = new CMatrix(); }
    for (int i = 0; i < 3; ++i) { g_fo[i] = op->New(); g_foRef[i] = op->GetRef(g_fo[i]); }
    g_mat = new CMatrix();
    for (int i = 0; i < 8; ++i) CModelInfo::ms_modelInfoPtrs[i] = reinterpret_cast<CBaseModelInfo*>(g_fakeModelInfo[i]);
    g_fakeVtbl[0xC8 / 4] = (void*)&H_Fix;                    // CVehicle::Fix -- slot 50
    g_fakeObjVtbl[0x34 / 4] = (void*)&H_TestCollision;       // CEntity::TestCollision -- slot 13
    g_fakeVtblPed[0x20 / 4] = (void*)&H_DeleteRw;            // CEntity::DeleteRwObject -- slot 8
    g_fakeVtblPed[0x14 / 4] = (void*)&H_SetModelIndex;       // CEntity::SetModelIndex -- slot 5
}

static std::string Lbl(Rng& r) { // a text label of 1..8 chars (zero padded)
    static const char* pool[] = { "DUMMY", "ABC", "MENU_1", "X", "GANG_A", "HELP_01", "a", "TEST1234", "NOPE", "_", "AB_CD_EF" };
    return pool[r.below(11)];
}

static std::string Lbl7(Rng& r) { // text label of 1..7 chars (the exe's 8 char labels are not terminated: NOTSA zero terminates them)
    static const char* pool[] = { "DUMMY", "ABC", "MENU_1", "X", "GANG_A", "HELP_01", "a", "NOPE", "_", "AB_CD" };
    return pool[r.below(10)];
}

static void TestAll() {
    auto Small = [](Ctx& c, int n = 4) { return c.rng->below(3) ? (int32_t)c.rng->below(n) : (int32_t)c.rng->u32(); };
    auto Flag = [](Ctx& c) { return c.rng->below(3) ? (int32_t)c.rng->below(2) : (int32_t)c.rng->u32(); };
    auto Raw = [](Ctx& c, uint32_t addr, size_t n) { c.watch.push_back({ (void*)addr, n }); };
    auto RandMem = [](Ctx& c, uint32_t addr, size_t n) { RandomFill((void*)addr, n, *c.rng); c.watch.push_back({ (void*)addr, n }); };
    auto RandBool = [](Ctx& c, uint32_t addr) { *reinterpret_cast<uint8_t*>(addr) = (uint8_t)c.rng->below(2); c.watch.push_back({ (void*)addr, 1 }); };
    auto Veh = [&](Ctx& c, int cmd, int type = -2) {
        c.watch.clear(); RandomModelInfos(*c.rng);
        for (int i = 0; i < 3; ++i) { RandomVeh(g_fv[i], g_fvMat[i], *c.rng, i == 0 ? type : -2); c.watch.push_back({ g_fv[i], sizeof(CAutomobile) }); }
        return Op(cmd).I(g_fvRef[0]); };
    auto Obj = [&](Ctx& c, int cmd) { c.watch.clear(); for (int i = 0; i < 3; ++i) { RandomObject(g_fo[i], *c.rng); c.watch.push_back({ g_fo[i], sizeof(CObject) }); } return Op(cmd).I(g_foRef[0]); };
    auto PlayerPed = [&](Ctx& c, int pid) { // Players[pid].m_pPed = fake ped 0 (random memory, fake vtable, fake intelligence / player data)
        RandomPed(g_ped, *c.rng); RandomFill(g_fakeIntel, sizeof(g_fakeIntel), *c.rng); RandomFill(g_fakePD, sizeof(g_fakePD), *c.rng);
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C) = g_fakeIntel;
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x480) = g_fakePD;
        *reinterpret_cast<void**>(0xB7CD98 + pid * 0x190) = g_ped;
        c.watch.push_back({ g_ped, sizeof(CPed) }); c.watch.push_back({ g_fakePD, sizeof(g_fakePD) }); };

    // ---- plain flag writes on vehicles / objects / trains
    Test<COMMAND_SET_PETROL_TANK_WEAKPOINT, &SetPetrolTankWeakpoint>("2500 SET_PETROL_TANK_WEAKPOINT", [&](Ctx& c) { c.opcode = Veh(c, 2500).I(Flag(c)).b; });
    Test<COMMAND_SET_OBJECT_PROOFS, &SetObjectProofs>("2506 SET_OBJECT_PROOFS", [&](Ctx& c) { c.opcode = Obj(c, 2506).I(Flag(c)).I(Flag(c)).I(Flag(c)).I(Flag(c)).I(Flag(c)).b; });
    Test<COMMAND_SET_TRAIN_FORCED_TO_SLOW_DOWN, &SetTrainForcedToSlowDown>("2511 SET_TRAIN_FORCED_TO_SLOW_DOWN", [&](Ctx& c) { c.opcode = Veh(c, 2511).I(Flag(c)).b; });
    Test<COMMAND_FIND_TRAIN_DIRECTION, &FindTrainDirection>("2531 FIND_TRAIN_DIRECTION", [&](Ctx& c) { c.opcode = Veh(c, 2531).b; });
    Test<COMMAND_IS_VEHICLE_ON_ALL_WHEELS, &IsVehicleOnAllWheels>("2512 IS_VEHICLE_ON_ALL_WHEELS", [&](Ctx& c) {
        auto op = Veh(c, 2512); auto* b = reinterpret_cast<uint8_t*>(g_fv[0]); b[0x804] = c.rng->below(2) ? 4 : (uint8_t)c.rng->below(6); b[0x960] = c.rng->below(2) ? 4 : (uint8_t)c.rng->below(6); c.opcode = op.b; });
    Test<COMMAND_RESET_VEHICLE_HYDRAULICS, &ResetVehicleHydraulics>("2558 RESET_VEHICLE_HYDRAULICS", [&](Ctx& c) { c.opcode = Veh(c, 2558, c.rng->below(2) ? 0 : -2).b; });
    Test<COMMAND_SET_EXTRA_CAR_COLOURS, &SetExtraCarColours>("2577 SET_EXTRA_CAR_COLOURS", [&](Ctx& c) { c.opcode = Veh(c, 2577).I(Small(c, 130)).I(Small(c, 130)).b; });
    Test<COMMAND_HAS_CAR_BEEN_RESPRAYED, &HasCarBeenResprayed>("2581 HAS_CAR_BEEN_RESPRAYED", [&](Ctx& c) { c.opcode = Veh(c, 2581).b; });
    Test<COMMAND_IMPROVE_CAR_BY_CHEATING, &ImproveCarByCheating>("2593 IMPROVE_CAR_BY_CHEATING", [&](Ctx& c) { c.opcode = Veh(c, 2593).I(Flag(c)).b; });
    Test<COMMAND_CHANGE_CAR_COLOUR_FROM_MENU, &ChangeCarColourFromMenu>("2594 CHANGE_CAR_COLOUR_FROM_MENU", [&](Ctx& c) {
        c.watch.clear(); for (int i = 0; i < 3; ++i) { RandomVeh(g_fv[i], g_fvMat[i], *c.rng); c.watch.push_back({ g_fv[i], sizeof(CAutomobile) }); }
        c.opcode = Op(2594).I(Small(c, 8)).I(g_fvRef[0]).I(c.rng->below(2) ? 1 : Small(c, 4)).I(Small(c, 64)).b; });
    Test<COMMAND_GIVE_NON_PLAYER_CAR_NITRO, &GiveNonPlayerCarNitro>("2537 GIVE_NON_PLAYER_CAR_NITRO", [&](Ctx& c) { c.opcode = Veh(c, 2537).b; });
    Test<COMMAND_IS_NEXT_STATION_ALLOWED, &IsNextStationAllowed>("2566 IS_NEXT_STATION_ALLOWED", [&](Ctx& c) { c.opcode = Veh(c, 2566).b; });
    Test<COMMAND_FIX_CAR, &FixCar>("2608 FIX_CAR", [&](Ctx& c) { c.opcode = Veh(c, 2608).b; });
    Test<COMMAND_IS_CAR_TOUCHING_CAR, &IsCarTouchingCar>("2507 IS_CAR_TOUCHING_CAR", [&](Ctx& c) { c.opcode = Veh(c, 2507).I(g_fvRef[1 + c.rng->below(2)]).b; });
    Test<COMMAND_IS_OBJECT_INTERSECTING_WORLD, &IsObjectIntersectingWorld>("2556 IS_OBJECT_INTERSECTING_WORLD", [&](Ctx& c) { c.opcode = Obj(c, 2556).b; });
    Test<COMMAND_GET_CAR_MODEL_VALUE, &GetCarModelValue>("2529 GET_CAR_MODEL_VALUE", [&](Ctx& c) { RandomModelInfos(*c.rng); c.opcode = Op(2529).I(c.rng->below(8)).GV(4).b; });
    Test<COMMAND_SET_UP_SKIP_FOR_SPECIFIC_VEHICLE, &SetUpSkipForSpecificVehicle>("2528 SET_UP_SKIP_FOR_SPECIFIC_VEHICLE", [&](Ctx& c) {
        c.opcode = Op(2528).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(400)).I(g_fvRef[c.rng->below(3)]).b; });
    Test<COMMAND_SET_UP_SKIP_FOR_VEHICLE_FINISHED_BY_SCRIPT, &SetUpSkipForVehicleFinishedByScript>("2613 SET_UP_SKIP_FOR_VEHICLE_FINISHED_BY_SCRIPT", [&](Ctx& c) {
        c.opcode = Op(2613).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(400)).I(g_fvRef[c.rng->below(3)]).b; });

    // ---- plain global state
    Test<COMMAND_ARE_SUBTITLES_SWITCHED_ON, &AreSubtitlesSwitchedOn>("2504 ARE_SUBTITLES_SWITCHED_ON", [&](Ctx& c) { RandBool(c, 0xBA678C); c.opcode = Op(2504).b; });
    Test<COMMAND_ENABLE_AMBIENT_CRIME, &EnableAmbientCrime>("2514 ENABLE_AMBIENT_CRIME", [&](Ctx& c) { Raw(c, 0xB72994, 1); c.opcode = Op(2514).I(Flag(c)).b; });
    Test<COMMAND_SET_AIRCRAFT_CARRIER_SAM_SITE, &SetAircraftCarrierSamSite>("2532 SET_AIRCRAFT_CARRIER_SAM_SITE", [&](Ctx& c) { Raw(c, 0x8D0A24, 1); c.opcode = Op(2532).I(Small(c, 2)).b; });
    Test<COMMAND_FORCE_BIG_MESSAGE_AND_COUNTER, &ForceBigMessageAndCounter>("2542 FORCE_BIG_MESSAGE_AND_COUNTER", [&](Ctx& c) { Raw(c, 0xBAA3FA, 1); c.opcode = Op(2542).I(Flag(c)).b; });
    Test<COMMAND_FORCE_ALL_VEHICLE_LIGHTS_OFF, &ForceAllVehicleLightsOff>("2615 FORCE_ALL_VEHICLE_LIGHTS_OFF", [&](Ctx& c) { Raw(c, 0xC1CC18, 1); c.opcode = Op(2615).I(Flag(c)).b; });
    Test<COMMAND_SET_SCRIPT_COOP_GAME, &SetScriptCoopGame>("2623 SET_SCRIPT_COOP_GAME", [&](Ctx& c) { Raw(c, 0x96A8A8, 1); c.opcode = Op(2623).I(Flag(c)).b; });
    Test<COMMAND_ALLOW_PAUSE_IN_WIDESCREEN, &AllowPauseInWidescreen>("2632 ALLOW_PAUSE_IN_WIDESCREEN", [&](Ctx& c) { Raw(c, 0xBA677C, 1); c.opcode = Op(2632).I(Flag(c)).b; });
    Test<COMMAND_IS_PLAYER_CONTROL_ON, &IsPlayerControlOn>("2535 IS_PLAYER_CONTROL_ON", [&](Ctx& c) {
        const int id = c.rng->below(2); RandMem(c, 0xB73458 + id * 0x134, 0x134); c.opcode = Op(2535).I(id).b; });
    Test<COMMAND_DOES_DECISION_MAKER_EXIST, &DoesDecisionMakerExist>("2546 DOES_DECISION_MAKER_EXIST", [&](Ctx& c) { for (int i = 0; i < 20; ++i) reinterpret_cast<uint8_t*>(0xC0B01C)[i] = (uint8_t)c.rng->below(2); Raw(c, 0xC0B01C, 20); c.opcode = Op(2546).I(Small(c, 30)).b; });
    Test<COMMAND_SWITCH_ON_GROUND_SEARCHLIGHT, &SwitchOnGroundSearchlight>("2562 SWITCH_ON_GROUND_SEARCHLIGHT", [&](Ctx& c) { RandomFill((void*)0xA94D68, 0x7C * 8, *c.rng); Raw(c, 0xA94D68, 0x7C * 8); c.opcode = Op(2562).I(Small(c, 30)).I(Flag(c)).b; });
    Test<COMMAND_DOES_PICKUP_EXIST, &DoesPickupExist>("2513 DOES_PICKUP_EXIST", [&](Ctx& c) { c.opcode = Op(2513).I(Small(c, 30)).b; });
    Test<COMMAND_IS_THIS_MODEL_A_CAR, &IsThisModelACar>("2561 IS_THIS_MODEL_A_CAR", [&](Ctx& c) { c.opcode = Op(2561).I(Small(c, 700)).b; });

    // ---- pure call forwarding
    Test<COMMAND_CLEAR_WANTED_LEVEL_IN_GARAGE, &ClearWantedLevelInGarage>("2516 CLEAR_WANTED_LEVEL_IN_GARAGE", [&](Ctx& c) { c.opcode = Op(2516).b; });
    Test<COMMAND_USE_DETONATOR, &UseDetonator>("2521 USE_DETONATOR", [&](Ctx& c) { c.opcode = Op(2521).b; });
    Test<COMMAND_IS_GANG_WAR_FIGHTING_GOING_ON, &IsGangWarFightingGoingOn>("2563 IS_GANG_WAR_FIGHTING_GOING_ON", [&](Ctx& c) { c.opcode = Op(2563).b; });
    Test<COMMAND_HAS_LANGUAGE_CHANGED, &HasLanguageChanged>("2575 HAS_LANGUAGE_CHANGED", [&](Ctx& c) { c.opcode = Op(2575).b; });
    Test<COMMAND_MANAGE_ALL_POPULATION, &ManageAllPopulation>("2579 MANAGE_ALL_POPULATION", [&](Ctx& c) { c.opcode = Op(2579).b; });
    Test<COMMAND_IS_SKIP_WAITING_FOR_SCRIPT_TO_FADE_IN, &IsSkipWaitingForScriptToFadeIn>("2614 IS_SKIP_WAITING_FOR_SCRIPT_TO_FADE_IN", [&](Ctx& c) { c.opcode = Op(2614).b; });
    Test<COMMAND_ENABLE_BURGLARY_HOUSES, &EnableBurglaryHouses>("2534 ENABLE_BURGLARY_HOUSES", [&](Ctx& c) { c.opcode = Op(2534).I(Flag(c)).b; });
    Test<COMMAND_TASK_PLAY_ANIM_SECONDARY, &TaskPlayAnimSecondary>("2586 TASK_PLAY_ANIM_SECONDARY", [&](Ctx& c) { c.opcode = Op(2586).b; });
    Test<COMMAND_HIGHLIGHT_MENU_ITEM, &HighlightMenuItem>("2595 HIGHLIGHT_MENU_ITEM", [&](Ctx& c) { c.opcode = Op(2595).I(Small(c, 8)).I(Small(c, 12)).I(c.rng->below(2) ? (int32_t)c.rng->below(2) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_MENU_COLUMN_WIDTH, &SetMenuColumnWidth>("2523 SET_MENU_COLUMN_WIDTH", [&](Ctx& c) {
        Raw(c, 0xC17044, 4); *reinterpret_cast<int32_t*>(0xC17044) = c.rng->below(8) == 0 ? (int32_t)c.rng->u32() : (int32_t)c.rng->below(4096);
        c.opcode = Op(2523).I(Small(c, 8)).I(Small(c, 4)).I(c.rng->below(3) ? (int32_t)c.rng->below(1000) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_CREATE_USER_3D_MARKER, &CreateUser3dMarker>("2624 CREATE_USER_3D_MARKER", [&](Ctx& c) {
        c.opcode = Op(2624).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).I(Small(c, 20)).GV(4).b; });
    Test<COMMAND_REMOVE_USER_3D_MARKER, &RemoveUser3dMarker>("2625 REMOVE_USER_3D_MARKER", [&](Ctx& c) { c.opcode = Op(2625).I(Small(c, 8)).b; });
    Test<COMMAND_SWITCH_OBJECT_BRAINS, &SwitchObjectBrains>("2630 SWITCH_OBJECT_BRAINS", [&](Ctx& c) { c.opcode = Op(2630).I(Small(c, 256)).I(Flag(c)).b; });
    Test<COMMAND_DRAW_LIGHT_WITH_RANGE, &DrawLightWithRange>("2533 DRAW_LIGHT_WITH_RANGE", [&](Ctx& c) {
        auto col = [&] { return c.rng->below(4) ? (int32_t)c.rng->below(300) - 20 : (int32_t)c.rng->u32(); };
        c.opcode = Op(2533).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).I(col()).I(col()).I(col()).Fl(c.rng->F(80)).b; });
    Test<COMMAND_SET_PLAYER_GROUP_TO_FOLLOW_NEVER, &SetPlayerGroupToFollowNever>("2609 SET_PLAYER_GROUP_TO_FOLLOW_NEVER", [&](Ctx& c) { c.opcode = Op(2609).I(c.rng->below(3) ? -1 : (int32_t)c.rng->below(3)).I(Flag(c)).b; });

    // ---- player ped commands (CWorld::Players[pid].m_pPed = fake ped)
    Test<COMMAND_SET_PLAYER_MODEL, &SetPlayerModel>("2503 SET_PLAYER_MODEL", [&](Ctx& c) { const int pid = c.rng->below(2); PlayerPed(c, pid); c.opcode = Op(2503).I(pid).I(Small(c, 300)).b; });
    Test<COMMAND_FORCE_INTERIOR_LIGHTING_FOR_PLAYER, &ForceInteriorLightingForPlayer>("2519 FORCE_INTERIOR_LIGHTING_FOR_PLAYER", [&](Ctx& c) {
        const int pid = c.rng->below(2); PlayerPed(c, pid); c.opcode = Op(2519).I(pid).I(Flag(c)).b; });
    Test<COMMAND_IS_PLAYER_USING_JETPACK, &IsPlayerUsingJetpack>("2572 IS_PLAYER_USING_JETPACK", [&](Ctx& c) {
        const int pid = c.rng->below(2); PlayerPed(c, pid); c.opcode = Op(2572).I(pid).b; });
    Test<COMMAND_MAKE_ROOM_IN_PLAYER_GANG_FOR_MISSION_PEDS, &MakeRoomInPlayerGangForMissionPeds>("2525 MAKE_ROOM_IN_PLAYER_GANG_FOR_MISSION_PEDS", [&](Ctx& c) {
        PlayerPed(c, 0); *reinterpret_cast<uint32_t*>(g_fakePD + 0x38) = c.rng->below(8); c.opcode = Op(2525).I(c.rng->below(3) ? (int32_t)c.rng->below(6) - 2 : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_PLAYER_TAKE_OFF_GOGGLES, &PlayerTakeOffGoggles>("2539 PLAYER_TAKE_OFF_GOGGLES (no task creating path)", [&](Ctx& c) {
        const int pid = c.rng->below(2); PlayerPed(c, pid);
        const int32_t instantly = c.rng->below(2) ? 0 : (int32_t)c.rng->u32() | 1;
        auto* ped = reinterpret_cast<uint8_t*>(g_ped);
        if (instantly != 0) { // never reach the task allocation: no goggles, or the primary task slot 3 (intelligence +0x10) is taken
            if (c.rng->below(2)) *reinterpret_cast<uint32_t*>(ped + 0x4FC) = 0; else { *reinterpret_cast<uint32_t*>(ped + 0x4FC) |= 1; *reinterpret_cast<uint32_t*>(g_fakeIntel + 0x10) |= 1; }
        }
        c.opcode = Op(2539).I(pid).I(instantly).b; });

    // ---- scanning
    Test<COMMAND_IS_MONEY_PICKUP_AT_COORDS, &IsMoneyPickupAtCoords>("2522 IS_MONEY_PICKUP_AT_COORDS", [&](Ctx& c) {
        auto& r = *c.rng; auto& pk = CPickups::aPickUps;
        RandomFill(pk.data(), sizeof(CPickup) * pk.size(), r);
        for (auto& p : pk) { reinterpret_cast<uint8_t*>(&p)[0x1C] = r.below(3) ? 8 : (uint8_t)r.below(12); p.SetPosn(CVector{ r.F(50.f, false), r.F(50.f, false), r.F(50.f, false) }); }
        const auto& q = pk[r.below((int)pk.size())]; CVector at = q.GetPosn();
        const float sc = r.below(3) ? 0.6f : 0.0f;
        c.opcode = Op(2522).Fl(at.x + r.F(sc, false)).Fl(at.y + r.F(sc, false)).Fl(at.z + r.F(sc, false)).b; });
    Test<COMMAND_CREATE_CAR_GENERATOR_WITH_PLATE, &CreateCarGeneratorWithPlate>("2530 CREATE_CAR_GENERATOR_WITH_PLATE", [&](Ctx& c) {
        auto& r = *c.rng;
        float z; switch (r.below(6)) { case 0: z = -100.f; break; case 1: z = -100.5f; break; case 2: z = -99.9f; break; case 3: z = BF(0x7FC00000u); break; default: z = r.F(100); }
        auto sm = [&] { return r.below(3) ? (int32_t)r.below(300) : (int32_t)r.u32(); };
        c.opcode = Op(2530).Fl(r.F(3000)).Fl(r.F(3000)).Fl(z).Fl(r.F(400)).I(sm()).I(sm()).I(sm()).I(sm()).I(sm()).I(sm()).I(sm()).I(sm()).Str8(Lbl(r).c_str()).GV(4).b; });
    Test<COMMAND_GET_STRING_WIDTH, &GetStringWidth>("2557 GET_STRING_WIDTH", [&](Ctx& c) { c.opcode = Op(2557).Str8(Lbl7(*c.rng).c_str()).GV(4).b; });
    Test<COMMAND_GET_STRING_WIDTH_WITH_NUMBER, &GetStringWidthWithNumber>("2568 GET_STRING_WIDTH_WITH_NUMBER", [&](Ctx& c) { c.opcode = Op(2568).Str8(Lbl7(*c.rng).c_str()).I(Small(c, 1000)).GV(4).b; });
    Test<COMMAND_IS_THIS_HELP_MESSAGE_BEING_DISPLAYED, &IsThisHelpMessageBeingDisplayed>("2602 IS_THIS_HELP_MESSAGE_BEING_DISPLAYED", [&](Ctx& c) {
        RandMem(c, 0xBAA480, 400); reinterpret_cast<uint8_t*>(0xBAA480)[c.rng->below(60)] = 0; c.opcode = Op(2602).Str8(Lbl7(*c.rng).c_str()).b; });
    Test<COMMAND_IS_LAST_BUILDING_MODEL_SHOT_BY_PLAYER, &IsLastBuildingModelShotByPlayer>("2618 IS_LAST_BUILDING_MODEL_SHOT_BY_PLAYER", [&](Ctx& c) {
        auto& r = *c.rng; const int pid = r.below(2);
        for (int i = 0; i < 8; ++i) *reinterpret_cast<int32_t*>(0xA44B70 + i * 0x1C + 0x18) = r.below(4) ? (int32_t)r.below(1000) : (int32_t)r.u32();
        const int32_t model = r.below(2) ? -(int32_t)r.below(7) : (int32_t)r.below(1000);
        int32_t resolved = model < 0 ? *reinterpret_cast<int32_t*>(0xA44B70 + (-model) * 0x1C + 0x18) : model;
        *reinterpret_cast<int32_t*>(0xB7CD98 + pid * 0x190 + 0xA0) = r.below(2) ? resolved : (int32_t)r.u32();
        c.opcode = Op(2618).I(pid).I(model).b; });
    Test<COMMAND_CLEAR_LAST_BUILDING_MODEL_SHOT_BY_PLAYER, &ClearLastBuildingModelShotByPlayer>("2619 CLEAR_LAST_BUILDING_MODEL_SHOT_BY_PLAYER", [&](Ctx& c) {
        const int pid = c.rng->below(2); Raw(c, 0xB7CD98 + pid * 0x190 + 0xA0, 4); c.opcode = Op(2619).I(pid).b; });
    Test<COMMAND_GET_RANDOM_CHAR_IN_AREA_OFFSET_NO_SAVE, &GetRandomCharInAreaOffsetNoSave>("2622 GET_RANDOM_CHAR_IN_AREA_OFFSET_NO_SAVE", [&](Ctx& c) {
        auto& r = *c.rng;
        c.watch.clear();
        for (int i = 0; i < 4; ++i) {
            auto* p = g_peds[i];
            RandomPed(p, r);
            reinterpret_cast<uint8_t*>(p)[0x484] = r.below(5) ? 1 : (uint8_t)r.below(4);                    // m_nCreatedBy
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x1C) &= ~0x800u;                   // !bRemoveFromWorld mostly
            if (r.below(8) == 0) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x1C) |= 0x800u;
            if (r.below(2)) reinterpret_cast<uint8_t*>(p)[0x470] &= ~8; else if (r.below(6) == 0) reinterpret_cast<uint8_t*>(p)[0x470] |= 8;
            if (r.below(4)) reinterpret_cast<uint8_t*>(p)[0x46D] &= ~1; else reinterpret_cast<uint8_t*>(p)[0x46D] |= 1;   // bInVehicle (+0x46C bit 8)
            p->m_placement.m_vPosn = CVector{ r.F(12.f), r.F(12.f), r.F(12.f) };
            if (r.below(4) == 0) { p->m_matrix = reinterpret_cast<decltype(p->m_matrix)>(g_mat); g_mat->GetPosition() = CVector{ r.F(12.f), r.F(12.f), r.F(12.f) }; }
            c.watch.push_back({ p, sizeof(CPed) });
        }
        auto off = [&] { return r.below(8) == 0 ? r.F(20.f) : r.f01() * 14.f; };
        c.opcode = Op(2622).Fl(r.F(12.f)).Fl(r.F(12.f)).Fl(r.F(12.f)).Fl(off()).Fl(off()).Fl(off()).GV(4).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g25_26_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice

    // exe callees of the handlers -> host recorders
    oracle::Patch(0x543540, (void*)&H_HasCollided);
    oracle::Patch(0x4552A0, (void*)&H_GetActualPickupIndex);
    oracle::Patch(0x56E230, (void*)&H_FindPlayerWanted);
    oracle::Patch(0x5625A0, (void*)&H_ClearWanted);
    oracle::Patch(0x56E210, (void*)&H_FindPlayerPed);
    oracle::Patch(0x5660B0, (void*)&H_UseDetonator);
    oracle::Patch(0x582050, (void*)&H_SetColumnWidth);
    oracle::Patch(0x5F6AA0, (void*)&H_CountMembers);
    oracle::Patch(0x5FB1D0, (void*)&H_RemoveNFollowers);
    oracle::Patch(0x4423C0, (void*)&H_SetUpSkip);
    oracle::Patch(0x6F31A0, (void*)&H_CreateCarGenerator);
    oracle::Patch(0x6F2D90, (void*)&H_PlateAdd);
    oracle::Patch(0x7000E0, (void*)&H_AddLight);
    oracle::Patch(0x43F180, (void*)&H_EnableBurglary);
    oracle::Patch(0x4839A0, (void*)&H_GetActualScriptThingIndex);
    oracle::Patch(0x4684F0, (void*)&H_DMGetInstance);
    oracle::Patch(0x6A3EA0, (void*)&H_Nitrous);
    oracle::Patch(0x6F7260, (void*)&H_IsNextStationAllowed);
    oracle::Patch(0x573CD0, (void*)&H_LanguageChanged);
    oracle::Patch(0x443AC0, (void*)&H_GangWar);
    oracle::Patch(0x6160A0, (void*)&H_ManagePop);
    oracle::Patch(0x470150, (void*)&H_PlayAnim);
    oracle::Patch(0x601110, (void*)&H_JetPack);
    oracle::Patch(0x581C10, (void*)&H_Highlight);
    oracle::Patch(0x5822B0, (void*)&H_CarColourFromGrid);
    oracle::Patch(0x6A0050, (void*)&H_TextGet);
    oracle::Patch(0x71A0E0, (void*)&H_StringWidth);
    oracle::Patch(0x69DE90, (void*)&H_InsertNumber);
    oracle::Patch(0x588B50, (void*)&H_HelpDisplayed);
    oracle::Patch(0x69DB70, (void*)&H_StringCopy);
    oracle::Patch(0x69E160, (void*)&H_InsertKeys);
    oracle::Patch(0x69DB50, (void*)&H_StringLength);
    oracle::Patch(0x69DBD0, (void*)&H_StringCompare);
    oracle::Patch(0x60C800, (void*)&H_NeverFollow);
    oracle::Patch(0x4416C0, (void*)&H_SkipWaiting);
    oracle::Patch(0x720FD0, (void*)&H_UserMarkerSet);
    oracle::Patch(0x721090, (void*)&H_UserMarkerDelete);
    oracle::Patch(0x46A900, (void*)&H_SwitchBrains);
    oracle::Patch(0x464D70, (void*)&H_IsPedDead);
    oracle::Patch(0x5F7E80, (void*)&H_GetPedsGroup);
    oracle::Patch(0x4C5AA0, (void*)&H_IsCarModel);
    oracle::Patch(0x5E6010, (void*)&H_TakeOffGoggles);

    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g25_26_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
