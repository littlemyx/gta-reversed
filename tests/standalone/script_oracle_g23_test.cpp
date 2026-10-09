// S6-H: differential test of SCRIPT COMMAND HANDLERS g23 against the original machine code (exe oracle, see game_oracle.h).
// Same machinery as script_oracle_g20_21_test.cpp: the handler TU (source/game_sa/Scripts/Commands/Ported/Group23.cpp) is #included, the exe's group
// processor (ProcessCommands2300To2399 @0x4762D0) and the CommandParser instantiation of the C++ handler run on the same synthetic opcode buffer;
// ScriptSpace, IP, compare flag, watched memory and the callee log are compared. Every callee of the handlers is replaced by a recorder on both sides
// (exe address patched with the host function / C++ symbol defined by the test), the ones that only read/write memory are forwarded to the exe's own code.
// usage: script_oracle_g23_test.exe [-v] [-n cases] [name-substring ...]   (env RW_EXE_ORACLE=<path of gta_sa_compact.exe>)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group23.cpp"

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
// the by-reference operand decoder / parameter plumbing of the script: the exe's own code
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType type) { return oracle::Fn<tScriptParam* __fastcall(CRunningScript*, int, int)>(0x464790)(this, 0, (int)type); }
void CRunningScript::CollectParameters(int16 n) { oracle::Fn<void __fastcall(CRunningScript*, int, int)>(0x464080)(this, 0, (int)n); }
void CRunningScript::StoreParameters(int16 n) { oracle::Fn<void __fastcall(CRunningScript*, int, int)>(0x464370)(this, 0, (int)n); }
void CRunningScript::ReadTextLabelFromScript(char* buf, uint8 n) { oracle::Fn<void __fastcall(CRunningScript*, int, char*, int)>(0x463D50)(this, 0, buf, (int)n); }

std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers = {};

// pools: the exe and the port both read the pool pointers at the original addresses
CPedPool*     GetPedPool()     { return *reinterpret_cast<CPedPool**>(0xB74490); }
CVehiclePool* GetVehiclePool() { return *reinterpret_cast<CVehiclePool**>(0xB74494); }
CObjectPool*  GetObjectPool()  { return *reinterpret_cast<CObjectPool**>(0xB7449C); }

// globals that live in TUs which are not part of this test
CHudColours&  HudColour   = StaticRef<CHudColours>(0xBAB22C);
CAudioEngine& AudioEngine = StaticRef<CAudioEngine>(0xB6BC90);
Fx_c&         g_fx        = StaticRef<Fx_c>(0xA9AE00);
namespace ModelIndices {
auto& MI_PICKUP_CAMERA = StaticRef<ModelIndex>(0x8CD5D8);
auto& MI_OYSTER        = StaticRef<ModelIndex>(0x8CD750);
auto& MI_HORSESHOE     = StaticRef<ModelIndex>(0x8CD754);
}

// functions that only read memory: the exe's own code
int16 CStreamedScripts::GetProperIndexFromIndexUsedByScript(int16 i) { return (int16)oracle::Fn<int __fastcall(void*, int, int)>(0x470810)(this, 0, (int)i); }
int32 CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { return oracle::Fn<int __cdecl(int, int)>(0x4839A0)(ref, (int)type); }
CRGBA CHudColours::GetRGB(eHudColours i) const { return m_aColours[i]; }
CPed* CPedGroupMembership::GetMember(int32 id) { return oracle::Fn<CPed* __fastcall(void*, int, int)>(0x5F69B0)(this, 0, (int)id); }

// ---------------------------------------------------------------------------------------------------------------------------------
// call log: the callees of the handlers are replaced by recorders on both sides
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static uint32_t U(float f) { return FB(f); }
static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint32_t U(unsigned v) { return v; }
static uint32_t U(int v) { return (uint32_t)v; }
template<class... F> static void Log(uint32_t tag, F... f) { g_log.push_back(tag); (g_log.push_back(U(f)), ...); }

static uint32_t g_ret;      // what the recorders return (random per case)
static float    g_retF;
static void*    g_base;     // base of the fake entity the `this` pointers are logged relative to
static uint32_t Off(const void* self) { return (uint32_t)((const uint8_t*)self - (const uint8_t*)g_base); }

// ---- recorders (cdecl)
static void  H_HeatHaze(int a)                 { Log(1, a & 0xFF); }
static void  H_Darkness(int a, int alpha)      { Log(2, a & 0xFF, alpha); }
static void  H_AmbPlanes(int a)                { Log(3, a & 0xFF); }
static void  H_Weather()                       { Log(4); }
static bool  H_Help()                          { Log(5); return g_ret & 1; }
static bool  H_CheckDmg(int a, int b)          { Log(6, a, b); return g_ret & 1; }
static void  H_MenuItem(int id, int item)      { Log(7, id & 0xFF, item & 0xFF); }
static void  H_MissionDoesntRequire(int m)     { Log(8, m); }
static void  H_RemoveModel(int m)              { Log(9, m); }
static void  H_AudioZone(const char* s, int f) { Log(10, *(const uint32_t*)s, *(const uint32_t*)(s + 4), f & 0xFF); }
static void  H_Explosion(void* v, void* c, int type, float x, float y, float z, unsigned life, int snd, float shake, int vis) { Log(11, v, c, type, x, y, z, life, snd & 0xFF, shake, vis & 0xFF); }
static void  H_UpdateMoney(int ref, int money) { Log(12, ref, money & 0xFFFF); }
static void  H_SetUpSkip(float x, float y, float z, float angle, int after, void* veh, int fin) { Log(13, x, y, z, angle, after & 0xFF, veh, fin & 0xFF); }
static void  H_ClearSkip(int a)                { Log(14, a & 0xFF); }
static int   H_MaxGroup()                      { Log(15); return (int)g_ret; }
static void  H_IncStat(int stat, float v)      { Log(16, stat & 0xFFFF, v); }
static float H_GroundZ(float x, float y)       { Log(17, x, y); return g_retF; }
static int   H_GenNewOne(float x, float y, float z, unsigned model, unsigned type, unsigned ammo, unsigned mpd, int isEmpty, char* msg) { Log(18, x, y, z, model, type & 0xFF, ammo, mpd, isEmpty & 0xFF, msg); return (int)g_ret; }
static bool  H_Water(float x, float y, float z, float* out, int touching, void* normals) { Log(19, x, y, z, touching & 0xFF, normals); *out = g_retF; return g_ret & 1; }
static bool  H_WaterNoWaves(float x, float y, float z, float* out, float* a, float* b) { Log(20, x, y, z, a, b); *out = g_retF; return g_ret & 1; }
static short H_GarageByName(const char* s)     { Log(21, *(const uint32_t*)s, *(const uint32_t*)(s + 4)); return (short)g_ret; }
// ---- recorders (thiscall as __fastcall(self, edx, ...))
static void  __fastcall H_AddBrain(void*, int, int img, int model, int prio, int type, int grp, float radius) { Log(22, img & 0xFFFF, model & 0xFFFF, prio & 0xFFFF, type & 0xFF, grp & 0xFF, radius); }
static void  __fastcall H_Attach(void* self, int, void* ent, float a, float b, float c, float d, float e, float f) { Log(23, Off(self), ent, a, b, c, d, e, f); }
static short __fastcall H_Say(void* self, int, int ctx, unsigned delay, float prob, int a, int b, int c) { Log(24, Off(self), ctx & 0xFFFF, delay, prob, a & 0xFF, b & 0xFF, c & 0xFF); return (short)g_ret; }
static void* __fastcall H_AttachAudio(void*, int, int slot, void* ped) { Log(25, slot & 0xFF, ped); return nullptr; }
static bool  __fastcall H_Talking(void* self, int) { Log(26, Off(self)); return g_ret & 1; }
static void  __fastcall H_Preload(void*, int, int track) { Log(27, track & 0xFFFF); }
static char  __fastcall H_BeatStatus(void*, int) { Log(28); return (char)g_ret; }
static void  __fastcall H_PlayBeat(void*, int, int a) { Log(29, a & 0xFF); }
static void  __fastcall H_StopBeat(void*, int) { Log(30); }
static void  __fastcall H_DoorOpen(void* self, int, float ratio) { Log(31, Off(self), ratio); }
static void  __fastcall H_SetDoorStatus(void* self, int, int door, int status) { Log(32, Off(self), door & 0xFF, status & 0xFF); }
static void  __fastcall H_SetDoorDamage(void* self, int, int door, int a) { Log(33, Off(self), door & 0xFF, a & 0xFF); }
static void  __fastcall H_LockDoor(void* self, int) { Log(34, Off(self)); }
static void  __fastcall H_FxCtor(void*, int, float a, float b, float c, float d, float e, float f, float g) { Log(35, a, b, c, d, e, f, g); }
static void  __fastcall H_AddParticle(void*, int, const CVector* p, const CVector* v, float t, const void*, float rot, float l1, float l2, int local) { Log(36, p->x, p->y, p->z, v->x, v->y, v->z, t, rot, l1, l2, local & 0xFF); }
static void  __fastcall H_SetIsStatic(void* self, int, int flag) { Log(100, flag & 0xFF); auto* b = (uint8_t*)self + 0x1C; *b = (uint8_t)((*b & ~4) | ((flag & 1) << 2)); }

// ---- the same callees as C++ symbols (what the port calls)
void  CPostEffects::ScriptHeatHazeFXSwitch(bool b) { H_HeatHaze(b); }
void  CPostEffects::ScriptDarknessFilterSwitch(bool b, int32 a) { H_Darkness(b, a); }
void  CPlane::SwitchAmbientPlanes(bool b) { H_AmbPlanes(b); }
void  CWeather::SetWeatherToAppropriateTypeNow() { H_Weather(); }
bool  CHud::HelpMessageDisplayed() { return H_Help(); }
bool  CDarkel::CheckDamagedWeaponType(eWeaponType a, eWeaponType b) { return H_CheckDmg(a, b); }
void  CMenuSystem::SetActiveMenuItem(MenuId id, int8 item) { H_MenuItem(id, item); }
void  CStreaming::SetMissionDoesntRequireModel(int32 m) { H_MissionDoesntRequire(m); }
void  CStreaming::RemoveModel(int32 m) { H_RemoveModel(m); }
void  CAudioZones::SwitchAudioZone(const char* s, bool f) { H_AudioZone(s, f); }
void  CExplosion::AddExplosion(CEntity* v, CEntity* c, eExplosionType t, CVector p, uint32 life, uint8 snd, float shake, uint8 vis) { H_Explosion(v, c, t, p.x, p.y, p.z, life, snd, shake, vis); }
void  CPickups::UpdateMoneyPerDay(tPickupReference r, uint16 m) { H_UpdateMoney(r.num, m); }
void  CGameLogic::SetUpSkip(CVector p, float angle, bool after, CEntity* veh, bool fin) { H_SetUpSkip(p.x, p.y, p.z, angle, after, veh, fin); }
void  CGameLogic::ClearSkip(bool a) { H_ClearSkip(a); }
int32 CStats::FindMaxNumberOfGroupMembers() { return H_MaxGroup(); }
void  CStats::IncrementStat(eStats s, float v) { H_IncStat(s, v); }
float CWorld::FindGroundZForCoord(float x, float y) { return H_GroundZ(x, y); }
tPickupReference CPickups::GenerateNewOne(CVector p, uint32 model, ePickupType type, uint32 ammo, uint32 mpd, bool isEmpty, char* msg) { return tPickupReference{ H_GenNewOne(p.x, p.y, p.z, model, type, ammo, mpd, isEmpty, msg) }; }
bool  CWaterLevel::GetWaterLevel(float x, float y, float z, float& out, uint8 touching, CVector* normals) { return H_Water(x, y, z, &out, touching, normals); }
bool  CWaterLevel::GetWaterLevelNoWaves(CVector p, float* out, float* a, float* b) { return H_WaterNoWaves(p.x, p.y, p.z, out, a, b); }
int16 CGarages::GetGarageNumberByName(const char* s) { return H_GarageByName(s); }
void  CScriptsForBrains::AddNewScriptBrain(int16 img, int16 model, uint16 prio, int8 type, int8 grp, float radius) { H_AddBrain(this, 0, img, model, prio, type, grp, radius); }
void  CPhysical::AttachEntityToEntity(CPhysical* e, CVector o, CVector r) { H_Attach(this, 0, e, o.x, o.y, o.z, r.x, r.y, r.z); }
int16 CPed::Say(eGlobalSpeechContext c, uint32 d, float p, bool a, bool b, bool cc) { return H_Say(this, 0, c, d, p, a, b, cc); }
CVector* CAudioEngine::AttachMissionAudioToPed(uint8 slot, CPed* ped) { H_AttachAudio(this, 0, slot, ped); return nullptr; }
bool  CPed::GetPedTalking() { return H_Talking(this, 0); }
void  CAudioEngine::PreloadBeatTrack(int16 t) { H_Preload(this, 0, t); }
int8  CAudioEngine::GetBeatTrackStatus() { return H_BeatStatus(this, 0); }
void  CAudioEngine::PlayPreloadedBeatTrack(bool a) { H_PlayBeat(this, 0, a); }
void  CAudioEngine::StopBeatTrack() { H_StopBeat(this, 0); }
void  CDoor::Open(float ratio) { H_DoorOpen(this, 0, ratio); }
void  CDamageManager::SetDoorStatus(eDoors door, eDoorStatus status) { H_SetDoorStatus(this, 0, (int)door, (int)status); }
void  CAutomobile::SetDoorDamage(eDoors door, bool a) { H_SetDoorDamage(this, 0, (int)door, a); }
void  CObject::LockDoor() { H_LockDoor(this, 0); }
void  CObject::SetIsStatic(bool f) { H_SetIsStatic(this, 0, f); }
FxPrtMult_c::FxPrtMult_c(float a, float b, float c, float d, float e, float f, float g) { H_FxCtor(this, 0, a, b, c, d, e, f, g); }
void  FxSystem_c::AddParticle(const CVector& p, const CVector& v, float t, const FxPrtMult_c& m, float rot, float l1, float l2, bool local) { H_AddParticle(this, 0, &p, &v, t, &m, rot, l1, l2, local); }

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

struct Outcome { std::vector<uint8_t> mem; std::vector<uint32_t> log; ptrdiff_t ip; bool cond; uint16_t andor; bool ret; };

template<eScriptCommands Cmd, auto* Fn>
static bool RunCase(Ctx& c, std::string& desc) {
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
        S.m_UsesMissionCleanup = false;
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

    Reset(); g_log.clear();
    const bool exeRet = oracle::Fn<unsigned char __fastcall(CRunningScript*, int, int)>(0x4762D0)(&S, 0, cmdId) != 0;
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
        g_ret = rng.u32(); g_retF = BF(rng.u32() & 0x7FFFFFFFu ? rng.u32() : 0u); // random bit patterns (NaNs included)
        if (rng.below(3)) g_retF = rng.F(100.f, false);
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
    std::printf("  %-52s %6d cases  %s\n", name, row.cases, row.bad ? "MISMATCH" : "ok");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// fake entities in the real pools
static CVehicle* g_veh;     static int g_vehRef;
static CObject*  g_obj;     static int g_objRef;
static CMatrix*  g_mat;
static void* g_fakeVtbl[256];
static void* g_fakeObjVtbl[256];
static void* g_fakeVtblPed[256];
static CPed*  g_ped; static int g_pedRef;
static tHandlingData* g_handling;
static uint8_t g_fakeFxSystem[64];
static uint8_t g_enex[0x80];

static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }

static void RandomVehicle(Rng& r) {
    RandomFill(g_veh, sizeof(CAutomobile), r);
    RandomFill(g_mat, sizeof(CMatrix), r);
    *reinterpret_cast<void**>(g_veh) = g_fakeVtbl;                         // vptr
    g_veh->m_matrix = reinterpret_cast<decltype(g_veh->m_matrix)>(g_mat);   // +0x14
    g_veh->m_pHandlingData = g_handling;                                    // +0x384
    g_base = g_veh;
}
static void RandomObject(Rng& r) {
    RandomFill(g_obj, sizeof(CObject), r);
    *reinterpret_cast<void**>(g_obj) = g_fakeObjVtbl;
    g_base = g_obj;
}
static void RandomPed(Rng& r) { RandomFill(g_ped, sizeof(CPed), r); *reinterpret_cast<void**>(g_ped) = g_fakeVtblPed; g_base = g_ped; }

static void SetupFakeWorld() {
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
    g_fakeVtbl[0x10 / 4]    = (void*)&H_SetIsStatic;
}

static int32_t SmallOrAny(Ctx& c, int n = 6) { return c.rng->below(3) ? (int32_t)c.rng->below(n) : (int32_t)c.rng->u32(); }
static int32_t Flag(Ctx& c) { return c.rng->below(3) ? (int32_t)c.rng->below(2) : (int32_t)c.rng->u32(); }

static void TestAll() {
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CAutomobile) } }; };
    auto PedWatch = [](Ctx& c) { c.watch = { { g_ped, sizeof(CPed) } }; };
    auto ObjWatch = [](Ctx& c) { c.watch = { { g_obj, sizeof(CObject) } }; };
    auto Veh = [&](Ctx& c, int cmd) { RandomVehicle(*c.rng); VehWatch(c); return Op(cmd).I(g_vehRef); };
    auto Ped = [&](Ctx& c, int cmd) { RandomPed(*c.rng); PedWatch(c); return Op(cmd).I(g_pedRef); };
    auto Obj = [&](Ctx& c, int cmd) { RandomObject(*c.rng); ObjWatch(c); return Op(cmd).I(g_objRef); };
    const Watch params{ (void*)0xA43C78, 0x20 }; // ScriptParams[0..7]: compared by the handlers that call CollectParameters / StoreParameters themselves
    const Watch streamed{ (void*)0xA47B60, 0xA48 };
    auto FillStreamed = [&](Ctx& c) {
        RandomFill((void*)0xA47B60, 0xA48, *c.rng);
        *reinterpret_cast<int16_t*>(0xA47B60 + 0xA44) = (int16_t)(c.rng->below(2) ? 82 : c.rng->below(8)); // m_nCountOfScripts
        for (int i = 0; i < 82; ++i) *reinterpret_cast<int16_t*>(0xA47B60 + i * 32 + 6) = (int16_t)c.rng->below(10);
        c.watch.push_back(streamed);
    };
    auto ScriptIdx = [](Ctx& c) { return c.rng->below(4) ? (int32_t)c.rng->below(10) : (int32_t)c.rng->u32(); };

    // ---- no callee / flags
    Test<COMMAND_SET_HEATHAZE_EFFECT, &SetHeathazeEffect>("2301 SET_HEATHAZE_EFFECT", [&](Ctx& c) { c.opcode = Op(2301).I(Flag(c)).b; });
    Test<COMMAND_IS_HELP_MESSAGE_BEING_DISPLAYED, &IsHelpMessageBeingDisplayed>("2302 IS_HELP_MESSAGE_BEING_DISPLAYED", [&](Ctx& c) { c.opcode = Op(2302).b; });
    Test<COMMAND_HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON, &HasObjectBeenDamagedByWeapon>("2303 HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON", [&](Ctx& c) {
        auto op = Obj(c, 2303); const int8_t last = (int8_t)reinterpret_cast<uint8_t*>(g_obj)[0x148];
        int32_t w;
        switch (c.rng->below(5)) { case 0: w = 0x38; break; case 1: w = 0x39; break; case 2: w = (int32_t)last; break; case 3: w = (int32_t)c.rng->u32(); break; default: w = (int32_t)c.rng->below(60); }
        c.opcode = op.I(w).b; });
    Test<COMMAND_HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON, &HasObjectBeenDamagedByWeapon>("2303 HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON (no object)", [&](Ctx& c) {
        c.opcode = Op(2303).I(g_objRef + 1 + c.rng->below(3)).I(c.rng->below(0x40)).b; });
    Test<COMMAND_CLEAR_OBJECT_LAST_WEAPON_DAMAGE, &ClearObjectLastWeaponDamage>("2304 CLEAR_OBJECT_LAST_WEAPON_DAMAGE", [&](Ctx& c) { c.opcode = Obj(c, 2304).b; });
    Test<COMMAND_CLEAR_OBJECT_LAST_WEAPON_DAMAGE, &ClearObjectLastWeaponDamage>("2304 CLEAR_OBJECT_LAST_WEAPON_DAMAGE (no object)", [&](Ctx& c) {
        c.opcode = Op(2304).I(g_objRef + 1 + c.rng->below(3)).b; ObjWatch(c); });
    Test<COMMAND_GET_HUD_COLOUR, &GetHudColour>("2308 GET_HUD_COLOUR", [&](Ctx& c) {
        c.watch = { { (void*)0xBAB22C, 0x3C } }; RandomFill((void*)0xBAB22C, 0x3C, *c.rng);
        const int32_t idx = (int32_t)c.rng->below(15) | (c.rng->below(2) ? (int32_t)(c.rng->u32() & ~0xFFu) : 0);
        c.opcode = Op(2308).I(idx).GV(8).GV(12).GV(16).GV(20).b; });
    Test<COMMAND_LOCK_DOOR, &LockDoor>("2309 LOCK_DOOR", [&](Ctx& c) { c.opcode = Obj(c, 2309).I(Flag(c)).b; });
    Test<COMMAND_SET_OBJECT_MASS, &SetObjectMass>("2310 SET_OBJECT_MASS", [&](Ctx& c) { c.opcode = Obj(c, 2310).Fl(c.rng->F(1000.f)).b; });
    Test<COMMAND_GET_OBJECT_MASS, &GetObjectMass>("2311 GET_OBJECT_MASS", [&](Ctx& c) { c.opcode = Obj(c, 2311).GV(8).b; });
    Test<COMMAND_SET_OBJECT_TURN_MASS, &SetObjectTurnMass>("2312 SET_OBJECT_TURN_MASS", [&](Ctx& c) { c.opcode = Obj(c, 2312).Fl(c.rng->F(1000.f)).b; });
    Test<COMMAND_GET_OBJECT_TURN_MASS, &GetObjectTurnMass>("2313 GET_OBJECT_TURN_MASS", [&](Ctx& c) { c.opcode = Obj(c, 2313).GV(8).b; });
    Test<COMMAND_SET_ACTIVE_MENU_ITEM, &SetActiveMenuItem>("2318 SET_ACTIVE_MENU_ITEM", [&](Ctx& c) { c.opcode = Op(2318).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_MARK_STREAMED_SCRIPT_AS_NO_LONGER_NEEDED, &MarkStreamedScriptAsNoLongerNeeded>("2319 MARK_STREAMED_SCRIPT_AS_NO_LONGER_NEEDED", [&](Ctx& c) {
        FillStreamed(c); c.opcode = Op(2319).I(ScriptIdx(c)).b; });
    Test<COMMAND_REMOVE_STREAMED_SCRIPT, &RemoveStreamedScript>("2320 REMOVE_STREAMED_SCRIPT", [&](Ctx& c) {
        FillStreamed(c); c.opcode = Op(2320).I(ScriptIdx(c)).b; });
    Test<COMMAND_SET_MESSAGE_FORMATTING, &SetMessageFormatting>("2322 SET_MESSAGE_FORMATTING", [&](Ctx& c) {
        c.watch = { { (void*)0xA44B60, 8 } }; RandomFill((void*)0xA44B60, 8, *c.rng);
        c.opcode = Op(2322).I(Flag(c)).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_WEATHER_TO_APPROPRIATE_TYPE_NOW, &SetWeatherToAppropriateTypeNow>("2325 SET_WEATHER_TO_APPROPRIATE_TYPE_NOW", [&](Ctx& c) { c.opcode = Op(2325).b; });
    Test<COMMAND_WINCH_CAN_PICK_OBJECT_UP, &WinchCanPickObjectUp>("2326 WINCH_CAN_PICK_OBJECT_UP", [&](Ctx& c) { c.opcode = Obj(c, 2326).I(Flag(c)).b; });
    Test<COMMAND_SWITCH_AUDIO_ZONE, &SwitchAudioZone>("2327 SWITCH_AUDIO_ZONE", [&](Ctx& c) {
        static const char* names[] = { "ZONE1", "AB", "12345678", "", "x" };
        c.watch = { params }; c.opcode = Op(2327).Str8(names[c.rng->below(5)]).I(Flag(c)).b; });
    Test<COMMAND_SET_CAR_ENGINE_ON, &SetCarEngineOn>("2328 SET_CAR_ENGINE_ON", [&](Ctx& c) { c.opcode = Veh(c, 2328).I(Flag(c)).b; });
    Test<COMMAND_SET_CAR_LIGHTS_ON, &SetCarLightsOn>("2329 SET_CAR_LIGHTS_ON", [&](Ctx& c) { c.opcode = Veh(c, 2329).I(Flag(c)).b; });
    Test<COMMAND_GET_PLANE_UNDERCARRIAGE_POSITION, &GetPlaneUndercarriagePosition>("2335 GET_PLANE_UNDERCARRIAGE_POSITION", [&](Ctx& c) { c.opcode = Veh(c, 2335).GV(8).b; });
    Test<COMMAND_SWITCH_AMBIENT_PLANES, &SwitchAmbientPlanes>("2339 SWITCH_AMBIENT_PLANES", [&](Ctx& c) { c.opcode = Op(2339).I(Flag(c)).b; });
    Test<COMMAND_SET_DARKNESS_EFFECT, &SetDarknessEffect>("2340 SET_DARKNESS_EFFECT", [&](Ctx& c) { c.opcode = Op(2340).I(Flag(c)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT, &GetNumberOfInstancesOfStreamedScript>("2342 GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT", [&](Ctx& c) {
        FillStreamed(c); c.opcode = Op(2342).I(ScriptIdx(c)).GV(8).b; });
    Test<COMMAND_ALLOCATE_STREAMED_SCRIPT_TO_RANDOM_PED, &AllocateStreamedScriptToRandomPed>("2344 ALLOCATE_STREAMED_SCRIPT_TO_RANDOM_PED", [&](Ctx& c) {
        FillStreamed(c); c.opcode = Op(2344).I(ScriptIdx(c)).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_ALLOCATE_STREAMED_SCRIPT_TO_OBJECT, &AllocateStreamedScriptToObject>("2345 ALLOCATE_STREAMED_SCRIPT_TO_OBJECT", [&](Ctx& c) {
        FillStreamed(c); RandomFill((void*)0xA44B70, 8 * 0x1C, *c.rng);
        const int32_t model = c.rng->below(2) ? (int32_t)c.rng->below(20000) : -(int32_t)c.rng->below(8);
        c.opcode = Op(2345).I(ScriptIdx(c)).I(model).I((int32_t)c.rng->u32()).Fl(c.rng->F(50.f)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_GROUP_MEMBER, &GetGroupMember>("2347 GET_GROUP_MEMBER", [&](Ctx& c) {
        Ped(c, 2347);
        c.watch.clear();
        const int idx = c.rng->below(8); const uint16_t id = (uint16_t)c.rng->below(1000);
        for (int i = 0; i < 8; ++i) { reinterpret_cast<char*>(0xC098E0)[i] = (char)c.rng->below(2); reinterpret_cast<uint16_t*>(0xC098D0)[i] = (uint16_t)c.rng->below(1000); }
        reinterpret_cast<char*>(0xC098E0)[idx] = 1; reinterpret_cast<uint16_t*>(0xC098D0)[idx] = id;
        auto* members = reinterpret_cast<CPed**>(0xC09920 + idx * 0x2D4 + 8 + 4);
        for (int i = 0; i < 8; ++i) members[i] = c.rng->below(2) ? g_ped : nullptr;
        c.opcode = Op(2347).I((int32_t)(((uint32_t)id << 16) | (uint32_t)idx)).I((int32_t)c.rng->below(8)).GV(8).b; });
    Test<COMMAND_GET_WATER_HEIGHT_AT_COORDS, &GetWaterHeightAtCoords>("2350 GET_WATER_HEIGHT_AT_COORDS", [&](Ctx& c) {
        c.opcode = Op(2350).Fl(c.rng->F(3000.f)).Fl(c.rng->F(3000.f)).I(Flag(c)).GV(8).b; });
    Test<COMMAND_ATTACH_CAR_TO_OBJECT, &AttachCarToObject>("2361 ATTACH_CAR_TO_OBJECT", [&](Ctx& c) {
        auto op = Veh(c, 2361); RandomObject(*c.rng); ObjWatch(c); c.watch.push_back({ g_veh, sizeof(CAutomobile) }); g_base = g_veh;
        c.opcode = op.I(g_objRef).Fl(c.rng->F(5.f)).Fl(c.rng->F(5.f)).Fl(c.rng->F(5.f)).Fl(c.rng->F(400.f)).Fl(c.rng->F(400.f)).Fl(c.rng->F(400.f)).b; });
    Test<COMMAND_SET_GARAGE_RESPRAY_FREE, &SetGarageRespray>("2362 SET_GARAGE_RESPRAY_FREE", [&](Ctx& c) {
        c.watch = { params, { (void*)0x96C048, 4 * 0xD8 } }; RandomFill((void*)0x96C048, 4 * 0xD8, *c.rng);
        g_ret = (uint32_t)(int16_t)((int)c.rng->below(7) - 2);
        c.opcode = Op(2362).Str8(c.rng->below(2) ? "GARAGE1" : "ABCDEFGH").I(Flag(c)).b; });
    Test<COMMAND_SET_CHAR_BULLETPROOF_VEST, &SetCharBulletproofVest>("2363 SET_CHAR_BULLETPROOF_VEST", [&](Ctx& c) { c.opcode = Ped(c, 2363).I(Flag(c)).b; });
    Test<COMMAND_SET_GROUP_FOLLOW_STATUS, &SetGroupFollowStatus>("2368 SET_GROUP_FOLLOW_STATUS", [&](Ctx& c) {
        c.watch = { { (void*)0xC09920, 8 * 0x2D4 } }; RandomFill((void*)0xC09920, 8 * 0x2D4, *c.rng);
        const int idx = c.rng->below(10); const uint16_t id = (uint16_t)c.rng->below(1000);
        for (int i = 0; i < 8; ++i) { reinterpret_cast<char*>(0xC098E0)[i] = (char)c.rng->below(2); reinterpret_cast<uint16_t*>(0xC098D0)[i] = (uint16_t)c.rng->below(1000); }
        if (idx < 8 && c.rng->below(4)) { reinterpret_cast<char*>(0xC098E0)[idx] = 1; reinterpret_cast<uint16_t*>(0xC098D0)[idx] = id; }
        const int32_t h = c.rng->below(10) == 0 ? -1 : (int32_t)(((uint32_t)id << 16) | (uint32_t)idx);
        c.opcode = Op(2368).I(h).I(Flag(c)).b; });
    Test<COMMAND_SET_SEARCHLIGHT_CLIP_IF_COLLIDING, &SetSearchlightClipIfColliding>("2369 SET_SEARCHLIGHT_CLIP_IF_COLLIDING", [&](Ctx& c) {
        c.watch = { { (void*)0xA94D68, 8 * 0x7C } }; RandomFill((void*)0xA94D68, 8 * 0x7C, *c.rng);
        const int idx = c.rng->below(8); const int16_t id = (int16_t)c.rng->below(1000);
        auto* sl = reinterpret_cast<uint8_t*>(0xA94D68) + idx * 0x7C;
        if (c.rng->below(4)) { sl[0] = 1; *reinterpret_cast<int16_t*>(sl + 4) = id; }
        const int32_t h = c.rng->below(10) == 0 ? -1 : (int32_t)(((uint32_t)(uint16_t)id << 16) | (uint32_t)idx);
        c.opcode = Op(2369).I(h).I(Flag(c)).b; });
    Test<COMMAND_SET_CHAR_USES_UPPERBODY_DAMAGE_ANIMS_ONLY, &SetCharUsesUpperbodyDamageAnimsOnly>("2374 SET_CHAR_USES_UPPERBODY_DAMAGE_ANIMS_ONLY", [&](Ctx& c) { c.opcode = Ped(c, 2374).I(Flag(c)).b; });
    Test<COMMAND_SET_CHAR_SAY_CONTEXT, &SetCharSayContext>("2375 SET_CHAR_SAY_CONTEXT", [&](Ctx& c) { c.opcode = Ped(c, 2375).I((int32_t)c.rng->u32()).GV(8).b; });
    Test<COMMAND_ADD_EXPLOSION_VARIABLE_SHAKE, &AddExplosionVariableShake>("2376 ADD_EXPLOSION_VARIABLE_SHAKE", [&](Ctx& c) {
        c.opcode = Op(2376).Fl(c.rng->F(3000.f)).Fl(c.rng->F(3000.f)).Fl(c.rng->F(100.f)).I(SmallOrAny(c, 20)).Fl(c.rng->F(5.f)).b; });
    Test<COMMAND_ATTACH_MISSION_AUDIO_TO_CHAR, &AttachMissionAudioToChar>("2377 ATTACH_MISSION_AUDIO_TO_CHAR", [&](Ctx& c) {
        c.opcode = Op(2377).I(SmallOrAny(c, 5)).I(c.rng->below(4) ? g_pedRef : g_pedRef + 1 + c.rng->below(3)).b; PedWatch(c); });
    Test<COMMAND_UPDATE_PICKUP_MONEY_PER_DAY, &UpdatePickupMoneyPerDay>("2378 UPDATE_PICKUP_MONEY_PER_DAY", [&](Ctx& c) { c.opcode = Op(2378).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_NAME_OF_ENTRY_EXIT_CHAR_USED, &GetNameOfEntryExitCharUsed>("2379 GET_NAME_OF_ENTRY_EXIT_CHAR_USED", [&](Ctx& c) {
        auto op = Ped(c, 2379); RandomFill(g_enex, sizeof(g_enex), *c.rng);
        g_enex[c.rng->below(0x30)] = 0; for (int i = 0; i < 0x30; ++i) if (g_enex[i] == 0 && c.rng->below(2)) g_enex[i] = 'A' + (char)c.rng->below(26);
        g_enex[0x2F] = 0;
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x78C) = c.rng->below(5) ? (void*)g_enex : nullptr;
        for (int i = 0; i < 0x30; ++i) ScriptSpaceRef()[64 + i] = (uint8_t)c.rng->u32();
        c.opcode = op.GV(64).b; });
    Test<COMMAND_GET_POSITION_OF_ENTRY_EXIT_CHAR_USED, &GetPositionOfEntryExitCharUsed>("2380 GET_POSITION_OF_ENTRY_EXIT_CHAR_USED", [&](Ctx& c) {
        auto op = Ped(c, 2380); RandomFill(g_enex, sizeof(g_enex), *c.rng);
        for (int o = 8; o <= 0x2C; o += 4) *reinterpret_cast<float*>(g_enex + o) = c.rng->F(3000.f);
        *reinterpret_cast<void**>(g_enex + 0x38) = c.rng->below(2) ? (void*)g_enex : nullptr;
        *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x78C) = c.rng->below(6) ? (void*)g_enex : nullptr;
        c.watch.push_back(params); for (int i = 0; i < 8; ++i) reinterpret_cast<uint32_t*>(0xA43C78)[i] = c.rng->u32();
        c.opcode = op.GV(8).GV(12).GV(16).GV(20).b; });
    Test<COMMAND_IS_CHAR_TALKING, &IsCharTalking>("2381 IS_CHAR_TALKING", [&](Ctx& c) { c.opcode = Ped(c, 2381).b; });
    Test<COMMAND_SET_UP_SKIP, &SetUpSkip>("2384 SET_UP_SKIP", [&](Ctx& c) { c.opcode = Op(2384).Fl(c.rng->F(3000.f)).Fl(c.rng->F(3000.f)).Fl(c.rng->F(100.f)).Fl(c.rng->F(400.f)).b; });
    Test<COMMAND_CLEAR_SKIP, &ClearSkip>("2385 CLEAR_SKIP", [&](Ctx& c) { c.opcode = Op(2385).b; });
    Test<COMMAND_PRELOAD_BEAT_TRACK, &PreloadBeatTrack>("2386 PRELOAD_BEAT_TRACK", [&](Ctx& c) { c.opcode = Op(2386).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_BEAT_TRACK_STATUS, &GetBeatTrackStatus>("2387 GET_BEAT_TRACK_STATUS", [&](Ctx& c) { c.opcode = Op(2387).GV(8).b; });
    Test<COMMAND_PLAY_BEAT_TRACK, &PlayBeatTrack>("2388 PLAY_BEAT_TRACK", [&](Ctx& c) { c.opcode = Op(2388).b; });
    Test<COMMAND_STOP_BEAT_TRACK, &StopBeatTrack>("2389 STOP_BEAT_TRACK", [&](Ctx& c) { c.opcode = Op(2389).b; });
    Test<COMMAND_FIND_MAX_NUMBER_OF_GROUP_MEMBERS, &FindMaxNumberOfGroupMembers>("2390 FIND_MAX_NUMBER_OF_GROUP_MEMBERS", [&](Ctx& c) { c.opcode = Op(2390).GV(8).b; });
    Test<COMMAND_VEHICLE_DOES_PROVIDE_COVER, &VehicleDoesProvideCover>("2391 VEHICLE_DOES_PROVIDE_COVER", [&](Ctx& c) { c.opcode = Veh(c, 2391).I(Flag(c)).b; });
    auto Pickup = [&](Ctx& c, int cmd) {
        RandomFill((void*)0x8CD5D8, 2, *c.rng); RandomFill((void*)0x8CD750, 2, *c.rng); RandomFill((void*)0x8CD754, 2, *c.rng);
        const float z = c.rng->below(2) ? c.rng->F(100.f) : -100.f - std::fabs(c.rng->F(100.f, false));
        return Op(cmd).Fl(c.rng->F(3000.f)).Fl(c.rng->F(3000.f)).Fl(c.rng->below(8) == 0 ? -100.f : z).GV(8).b; };
    Test<COMMAND_CREATE_SNAPSHOT_PICKUP, &CreateSnapshotPickup>("2392 CREATE_SNAPSHOT_PICKUP", [&](Ctx& c) { c.opcode = Pickup(c, 2392); });
    Test<COMMAND_CREATE_HORSESHOE_PICKUP, &CreateHorseshoePickup>("2393 CREATE_HORSESHOE_PICKUP", [&](Ctx& c) { c.opcode = Pickup(c, 2393); });
    Test<COMMAND_CREATE_OYSTER_PICKUP, &CreateOysterPickup>("2394 CREATE_OYSTER_PICKUP", [&](Ctx& c) { c.opcode = Pickup(c, 2394); });
    Test<COMMAND_HAS_OBJECT_BEEN_UPROOTED, &HasObjectBeenUprooted>("2395 HAS_OBJECT_BEEN_UPROOTED", [&](Ctx& c) { c.opcode = Obj(c, 2395).b; });
    Test<COMMAND_ADD_SMOKE_PARTICLE, &AddSmokeParticle>("2396 ADD_SMOKE_PARTICLE", [&](Ctx& c) {
        *reinterpret_cast<void**>(0xA9AE20) = g_fakeFxSystem;
        auto op = Op(2396); for (int i = 0; i < 12; ++i) op.Fl(c.rng->F(10.f));
        c.opcode = op.b; });
    Test<COMMAND_IS_CHAR_STUCK_UNDER_CAR, &IsCharStuckUnderCar>("2397 IS_CHAR_STUCK_UNDER_CAR", [&](Ctx& c) { c.opcode = Ped(c, 2397).b; });
    Test<COMMAND_CONTROL_CAR_DOOR, &ControlCarDoor>("2398 CONTROL_CAR_DOOR", [&](Ctx& c) {
        auto op = Veh(c, 2398); c.watch.push_back(params);
        c.opcode = op.I((int32_t)c.rng->below(14) - 3).I(SmallOrAny(c, 5)).I(c.rng->below(2) ? (int32_t)c.rng->below(12) - 3 : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_DOOR_ANGLE_RATIO, &GetDoorAngleRatio>("2399 GET_DOOR_ANGLE_RATIO", [&](Ctx& c) {
        auto op = Veh(c, 2399); const int door = (int)c.rng->below(14) - 3;
        if (c.rng->below(6) == 0) { // signalling NaN in the angle of the door
            const uint32_t sn = 0x7F800001u | (c.rng->u32() & 0x3FFFFF);
            if (door >= 0 && door < 6) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(g_veh) + 0x5C4 + door * 0x18) = sn;
            else if (door >= 6)        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(g_veh) + 0x718) = sn;
        }
        c.opcode = op.I(door).GV(8).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g23_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice
    // the exe's callees -> the recorders
    oracle::Patch(0x701160, (void*)&H_HeatHaze);
    oracle::Patch(0x701170, (void*)&H_Darkness);
    oracle::Patch(0x6CCC50, (void*)&H_AmbPlanes);
    oracle::Patch(0x72A790, (void*)&H_Weather);
    oracle::Patch(0x588B50, (void*)&H_Help);
    oracle::Patch(0x43D9E0, (void*)&H_CheckDmg);
    oracle::Patch(0x5820C0, (void*)&H_MenuItem);
    oracle::Patch(0x409C90, (void*)&H_MissionDoesntRequire);
    oracle::Patch(0x4089A0, (void*)&H_RemoveModel);
    oracle::Patch(0x508320, (void*)&H_AudioZone);
    oracle::Patch(0x736A50, (void*)&H_Explosion);
    oracle::Patch(0x455680, (void*)&H_UpdateMoney);
    oracle::Patch(0x4423C0, (void*)&H_SetUpSkip);
    oracle::Patch(0x441560, (void*)&H_ClearSkip);
    oracle::Patch(0x559A50, (void*)&H_MaxGroup);
    oracle::Patch(0x55C180, (void*)&H_IncStat);
    oracle::Patch(0x569660, (void*)&H_GroundZ);
    oracle::Patch(0x456F20, (void*)&H_GenNewOne);
    oracle::Patch(0x6EB690, (void*)&H_Water);
    oracle::Patch(0x6E8580, (void*)&H_WaterNoWaves);
    oracle::Patch(0x447680, (void*)&H_GarageByName);
    oracle::Patch(0x46A930, (void*)&H_AddBrain);
    oracle::Patch(0x54D570, (void*)&H_Attach);
    oracle::Patch(0x5EFFE0, (void*)&H_Say);
    oracle::Patch(0x507310, (void*)&H_AttachAudio);
    oracle::Patch(0x5EFF50, (void*)&H_Talking);
    oracle::Patch(0x507F40, (void*)&H_Preload);
    oracle::Patch(0x507170, (void*)&H_BeatStatus);
    oracle::Patch(0x507180, (void*)&H_PlayBeat);
    oracle::Patch(0x5071A0, (void*)&H_StopBeat);
    oracle::Patch(0x6F4790, (void*)&H_DoorOpen);
    oracle::Patch(0x6C21C0, (void*)&H_SetDoorStatus);
    oracle::Patch(0x6B1600, (void*)&H_SetDoorDamage);
    oracle::Patch(0x59F5C0, (void*)&H_LockDoor);
    oracle::Patch(0x4AB290, (void*)&H_FxCtor);
    oracle::Patch(0x4AA440, (void*)&H_AddParticle);
    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g23_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
