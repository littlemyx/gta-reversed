// S6-B: differential test of SCRIPT COMMAND HANDLERS against the original machine code (exe oracle, see game_oracle.h).
// The handler TUs of the S6-B batch (source/game_sa/Scripts/Commands/Ported/Group09..12.cpp) are #included here (their handlers live in anonymous
// namespaces), the test maps gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original addresses and, for every case, runs
//   (1) the exe's group processor (ProcessCommands900To999 .. 1200To1299, thiscall(this = fake CRunningScript, cmd)) on a synthetic opcode buffer, and
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then compares the script variables (ScriptSpace), the IP, the compare flag, the watched memory (whole fake vehicle / object, globals) and the log of the
// callee functions the handler invoked (the exe callees are patched with host recorders, the C++ side defines the same recorders for the game functions).
// usage: script_oracle_test.exe [-v] [-n cases] [name-substring ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group09.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group10.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group11.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group12.cpp"

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
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
template<class... F> static void Log(uint32_t tag, F... f) { g_log.push_back(tag); (g_log.push_back(FB((float)f)), ...); }

static float FakeGroundZ(float x, float y) { return x * 0.25f + y * 0.125f - 7.0f; }

// -- host recorders for the exe (cdecl / thiscall: __fastcall(this, edx, args...) pops the stack args like a thiscall callee)
static void  __cdecl H_ClearCars(float a, float b, float c, float d, float e, float f) { Log(1, a, b, c, d, e, f); }
static void  __cdecl H_ClearPeds(float a, float b, float c, float d, float e, float f) { Log(2, a, b, c, d, e, f); }
static float __cdecl H_GroundZ(float x, float y) { return FakeGroundZ(x, y); }
static void  __cdecl H_ClearExciting(const CVector* p, float r, uint8_t flag) { Log(3, p->x, p->y, p->z, r, (float)flag, 0.f); }
static void  __fastcall H_SetHeading(void* self, int, float h) { Log(4, h); }
static void  __fastcall H_SetHeliOrientation(void* self, int, float a) { Log(5, a); }
static void  __fastcall H_Extinguish(void* self, int) { Log(6); }
static void  __cdecl H_SwitchReal(void* veh) { Log(7); }
static void  __fastcall H_BurstTyre(void* self, int, int tyre, int phys) { Log(8, (float)(uint8_t)tyre, (float)(uint8_t)phys, 0, 0, 0, 0); }
static void  __fastcall H_PopBoot(void* self, int) { Log(9); }
static void  __fastcall H_CloseDoors(void* self, int) { Log(10); }
static void  __fastcall H_HeliGoto(void* self, int, float a, float b, float c, float d, float e) { Log(11, a, b, c, d, e, 0.f); }
static void  __fastcall H_PlaneGoto(void* self, int, float a, float b, float c, float d, float e) { Log(12, a, b, c, d, e, 0.f); }

// -- the same recorders as the game functions the handlers call (C++ side)
void  CWorld::ClearCarsFromArea(float a, float b, float c, float d, float e, float f) { Log(1, a, b, c, d, e, f); }
void  CWorld::ClearPedsFromArea(float a, float b, float c, float d, float e, float f) { Log(2, a, b, c, d, e, f); }
float CWorld::FindGroundZForCoord(float x, float y) { return FakeGroundZ(x, y); }
void  CWorld::ClearExcitingStuffFromArea(const CVector& p, float r, uint8 flag) { Log(3, p.x, p.y, p.z, r, (float)flag, 0.f); }
void  CPlaceable::SetHeading(float h) { Log(4, h); }
void  CAutomobile::SetHeliOrientation(float a) { Log(5, a); }
void  CVehicle::ExtinguishCarFire() { Log(6); }
void  CCarCtrl::SwitchVehicleToRealPhysics(CVehicle*) { Log(7); }
void  CAutomobile::PopBoot() { Log(9); }
void  CAutomobile::CloseAllDoors() { Log(10); }
void  CAutomobile::TellHeliToGoToCoors(float a, float b, float c, float d, float e) { Log(11, a, b, c, d, e, 0.f); }
void  CAutomobile::TellPlaneToGoToCoors(float a, float b, float c, float d, float e) { Log(12, a, b, c, d, e, 0.f); }

// -- batch 2 recorders
static uint32_t g_hostRet;   // value the recorders of the audio getters return (random per case)
static uint32_t __cdecl H_Marker(uint32_t id, int type, const CVector* p, float size, int r, int g, int b, int a, int period, float frac, int rot) {
    Log(20, p->x, p->y, p->z, size, (float)(uint8_t)r, (float)(uint8_t)a); Log(21, (float)type, (float)(uint16_t)period, frac, (float)(int16_t)rot, 0, 0); g_log.push_back(id); return 0; }
static uint32_t __cdecl H_AddSphere(uint32_t id, float x, float y, float z, float r) { g_log.push_back(id); Log(22, x, y, z, r, 0, 0); return 0x1234; }
static void     __cdecl H_RemoveSphere(int h) { Log(23); g_log.push_back((uint32_t)h); }
static void     __cdecl H_WorldRemove(void* e) { Log(24); }
static void     __cdecl H_WorldAdd(void* e) { Log(25); }
static void     __fastcall H_SetOrientation(void* self, int, float x, float y, float z) { Log(26, x, y, z, 0, 0, 0); }
static void     __fastcall H_UpdateRwMatrix(void* self, int) { Log(27); }
static void     __fastcall H_UpdateRwFrame(void* self, int) { Log(28); }
static void     __fastcall H_SetIsStatic(void* self, int, int flag) { Log(29, (float)(flag & 0xFF)); auto* b = (uint8_t*)self + 0x1C; *b = (uint8_t)((*b & ~4) | ((flag & 1) << 2)); }
static void     __fastcall H_AddMoving(void* self, int) { Log(30); }
static void     __fastcall H_RemoveMoving(void* self, int) { Log(31); }
static void     __cdecl H_RemoveBuildings(int area) { Log(32, (float)area); }
static void     __cdecl H_StartExtra(int c, int f) { Log(33, (float)c, (float)(f & 0xFF)); }
static void     __cdecl H_StopExtra(int f) { Log(34, (float)(f & 0xFF)); }
static void     __cdecl H_ReqCol(const CVector* p, int area) { Log(35, p->x, p->y, p->z, (float)area, 0, 0); }
static void     __cdecl H_SetStat(int stat, float v) { Log(36, (float)stat, v); }
static void     __cdecl H_FastestTime(int stat, int t) { Log(37, (float)stat, (float)t); }
static void     __fastcall H_ClearMissionAudio(void* self, int, int slot) { Log(38, (float)(uint8_t)slot); }
static int      __fastcall H_AudioStatus(void* self, int, int slot) { Log(39, (float)(uint8_t)slot); return (int8_t)g_hostRet; }
static int      __fastcall H_AudioFinished(void* self, int, int slot) { Log(40, (float)(uint8_t)slot); return g_hostRet & 1; }
static void     __fastcall H_AudioPos(void* self, int, int slot, const CVector* p) { Log(41, (float)(uint8_t)slot, p->x, p->y, p->z, 0, 0); }
static void     __fastcall H_NearClip(void* self, int, float v) { Log(42, v); }
static void     __fastcall H_Interp(void* self, int, float a, float b, uint32_t t) { Log(43, a, b, (float)t, 0, 0, 0); }
static void     __fastcall H_Retune(void* self, int, int id) { Log(44, (float)(id & 0xFF)); }
static void     __fastcall H_Preload(void* self, int, int t) { Log(45, (float)(int16_t)t); }
static void     __fastcall H_PlayPre(void* self, int, int f) { Log(46, (float)(f & 0xFF)); }
static int      __cdecl H_GarageByName(const char* n) { char t[9] = {}; std::memcpy(t, n, 8); Log(47, (float)(uint8_t)t[0], (float)(uint8_t)t[1], (float)(uint8_t)t[7], 0, 0, 0); return (int16_t)(g_hostRet >> 8); }
static int      __cdecl H_GarageOpen(int g) { Log(48, (float)(int16_t)g); return g_hostRet & 1; }
static int      __cdecl H_GarageClosed(int g) { Log(49, (float)(int16_t)g); return (g_hostRet >> 1) & 1; }

// C++ side of the same recorders
void     C3dMarkers::PlaceMarkerSet(uint32 id, e3dMarkerType type, CVector& p, float size, uint8 r, uint8 g, uint8 b, uint8 a, uint16 period, float frac, int16 rot) {
    Log(20, p.x, p.y, p.z, size, (float)r, (float)a); Log(21, (float)type, (float)period, frac, (float)rot, 0, 0); g_log.push_back(id); }
uint32   CTheScripts::AddScriptSphere(uint32 id, CVector p, float radius) { g_log.push_back(id); Log(22, p.x, p.y, p.z, radius, 0, 0); return 0x1234; }
void     CTheScripts::RemoveScriptSphere(int32 h) { Log(23); g_log.push_back((uint32_t)h); }
void     CWorld::Remove(CEntity*) { Log(24); }
void     CWorld::Add(CEntity*) { Log(25); }
void     CPlaceable::SetOrientation(float x, float y, float z) { Log(26, x, y, z, 0, 0, 0); }
void     CEntity::UpdateRwMatrix() { Log(27); }
void     CEntity::UpdateRwFrame() { Log(28); }
void     CObject::SetIsStatic(bool f) { H_SetIsStatic(this, 0, f); }   // (never used: the fake object's vtable points at the host recorder)
void     CPhysical::AddToMovingList() { Log(30); }
void     CPhysical::RemoveFromMovingList() { Log(31); }
void     CStreaming::RemoveBuildingsNotInArea(eAreaCodes a) { Log(32, (float)a); }
void     CTimeCycle::StartExtraColour(int32 c, bool f) { Log(33, (float)c, (float)f); }
void     CTimeCycle::StopExtraColour(bool f) { Log(34, (float)f); }
void     CColStore::RequestCollision(const CVector& p, eAreaCodes a) { Log(35, p.x, p.y, p.z, (float)a, 0, 0); }
void     CStats::SetStatValue(eStats s, float v) { Log(36, (float)s, v); }
void     CStats::RegisterFastestTime(eStats s, int32 t) { Log(37, (float)s, (float)t); }
void     CAudioEngine::ClearMissionAudio(uint8 s) { Log(38, (float)s); }
int8     CAudioEngine::GetMissionAudioLoadingStatus(uint8 s) { Log(39, (float)s); return (int8)g_hostRet; }
bool     CAudioEngine::IsMissionAudioSampleFinished(uint8 s) { Log(40, (float)s); return g_hostRet & 1; }
void     CAudioEngine::SetMissionAudioPosition(uint8 s, CVector& p) { Log(41, (float)s, p.x, p.y, p.z, 0, 0); }
void     CCamera::SetNearClipScript(float v) { Log(42, v); }
void     CCamera::SetParametersForScriptInterpolation(float a, float b, uint32 t) { Log(43, a, b, (float)t, 0, 0, 0); }
void     CAudioEngine::RetuneRadio(eRadioID id) { Log(44, (float)((int)id & 0xFF)); }
void     CAudioEngine::PreloadBeatTrack(int16 t) { Log(45, (float)t); }
void     CAudioEngine::PlayPreloadedBeatTrack(bool f) { Log(46, (float)f); }
int16    CGarages::GetGarageNumberByName(const char* n) { char t[9] = {}; std::memcpy(t, n, 8); Log(47, (float)(uint8_t)t[0], (float)(uint8_t)t[1], (float)(uint8_t)t[7], 0, 0, 0); return (int16_t)(g_hostRet >> 8); }
bool     CGarages::IsGarageOpen(int16 g) { Log(48, (float)g); return g_hostRet & 1; }
bool     CGarages::IsGarageClosed(int16 g) { Log(49, (float)g); return (g_hostRet >> 1) & 1; }

// CDamageManager accessors: the port's handler is tested, not the accessor -> forward to the exe's own
uint32 CDamageManager::GetEngineStatus() { return oracle::Fn<uint32_t __fastcall(CDamageManager*, int)>(0x6C22C0)(this, 0); }
eCarWheelStatus CDamageManager::GetWheelStatus(eCarWheel wheel) const { return (eCarWheelStatus)oracle::Fn<uint32_t __fastcall(const CDamageManager*, int, int)>(0x6C21B0)(this, 0, (int)wheel); }
// (CVehicle::BurstTyre is virtual: both sides go through the fake vtable of the fake vehicle below)

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
    Op& Str8(const char* s) { b.push_back(9); char t[8] = {}; std::strncpy(t, s, 8); for (char c : t) b.push_back((uint8_t)c); return *this; }
};

struct Snap { std::vector<uint8_t> mem; };
struct Watch { void* p; size_t n; };

struct Ctx {
    Rng*                 rng{};
    std::vector<Watch>   watch;          // memory regions compared after the command (and restored between the two runs)
    bool                 notFlag{};
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
    constexpr unsigned groupFn[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x483BD0, 0x489500, 0x48A320, 0x48B590 };
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
    g_fakeVtbl[0xB0 / 4] = (void*)&H_BurstTyre;          // CVehicle::BurstTyre(uint8, bool) -- vtable slot 44, the same on both sides
}

// ---------------------------------------------------------------------------------------------------------------------------------
static float RandAngle(Rng& r) { return r.below(10) == 0 ? r.F(50000.f) : r.F(2000.f, false); }

static void TestAll() {
    // ---- pure value / flag commands
    Test<COMMAND_CONVERT_METRES_TO_FEET, &ConvertMetresToFeet>("1061 CONVERT_METRES_TO_FEET", [](Ctx& c) { c.opcode = Op(1061).Fl(c.rng->F(1e4f)).GV(8).b; });
    Test<COMMAND_CONVERT_METRES_TO_FEET_INT, &ConvertMetresToFeetInt>("1069 CONVERT_METRES_TO_FEET_INT", [](Ctx& c) {
        c.opcode = Op(1069).I(c.rng->below(4) ? (int32_t)c.rng->u32() % 100000 : (int32_t)c.rng->u32()).GV(8).b; });
    Test<COMMAND_IS_INT_VAR_EQUAL_TO_CONSTANT, &IsIntVarEqualToConstant>("1187 IS_INT_VAR_EQUAL_TO_CONSTANT", [](Ctx& c) {
        const int32_t v = c.rng->below(3) ? (int32_t)c.rng->below(10) : (int32_t)c.rng->u32();
        *reinterpret_cast<int32_t*>(&ScriptSpaceRef()[16]) = v;
        c.opcode = Op(1187).GV(16).I(c.rng->below(2) ? v : (int32_t)c.rng->below(10)).b; });
    Test<COMMAND_IS_PC_VERSION, &IsPCVersion>("1157 IS_PC_VERSION", [](Ctx& c) { c.opcode = Op(1157).b; });
    Test<COMMAND_GET_MAX_WANTED_LEVEL, &GetMaxWantedLevel>("1295 GET_MAX_WANTED_LEVEL", [](Ctx& c) {
        c.watch = { { (void*)0x8CDEE4, 4 } }; *reinterpret_cast<int32_t*>(0x8CDEE4) = (int32_t)c.rng->u32(); c.opcode = Op(1295).GV(4).b; });
    Test<COMMAND_ARE_CREDITS_FINISHED, &AreCreditsFinished>("1078 ARE_CREDITS_FINISHED", [](Ctx& c) {
        c.watch = { { (void*)0xC6E97C, 1 } }; *reinterpret_cast<uint8_t*>(0xC6E97C) = (uint8_t)c.rng->below(3); c.opcode = Op(1078).b; });
    Test<COMMAND_FREEZE_ONSCREEN_TIMER, &FreezeOnscreenTimer>("918 FREEZE_ONSCREEN_TIMER", [](Ctx& c) {
        c.watch = { { (void*)0xBA18D9, 1 } }; c.opcode = Op(918).I(c.rng->below(3)).b; });
    Test<COMMAND_SWITCH_STREAMING, &SwitchStreaming>("943 SWITCH_STREAMING", [](Ctx& c) {
        c.watch = { { (void*)0x9654B0, 1 } }; c.opcode = Op(943).I(c.rng->below(3)).b; });
    Test<COMMAND_SWITCH_WORLD_PROCESSING, &SwitchWorldProcessing>("951 SWITCH_WORLD_PROCESSING", [](Ctx& c) {
        c.watch = { { (void*)0xB7CD6D, 1 } }; c.opcode = Op(951).I(c.rng->below(3)).b; });

    // ---- area commands with recorded callees
    Test<COMMAND_CLEAR_AREA_OF_CARS, &ClearAreaOfCars>("954 CLEAR_AREA_OF_CARS", [](Ctx& c) {
        c.opcode = Op(954).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).b; });
    Test<COMMAND_CLEAR_AREA_OF_CHARS, &ClearAreaOfChars>("1067 CLEAR_AREA_OF_CHARS", [](Ctx& c) {
        c.opcode = Op(1067).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).b; });
    Test<COMMAND_CLEAR_AREA, &ClearArea>("917 CLEAR_AREA", [](Ctx& c) {
        c.opcode = Op(917).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->below(2) ? c.rng->F(300) : -100.f).Fl(c.rng->F(50)).I(c.rng->below(2) ? (int32_t)c.rng->u32() : 1).b; });

    // ---- vehicle commands: whole fake vehicle compared
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CAutomobile) } }; };
    auto VehOp = [](Ctx& c, int cmd) { return Op(cmd).I(g_vehRef); };
    auto VehFlag = [&](int cmd) { return [=](Ctx& c) { RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, cmd).I(c.rng->below(3)).b; }; };
    Test<COMMAND_SWITCH_CAR_SIREN, &SwitchCarSiren>("919 SWITCH_CAR_SIREN", VehFlag(919));
    Test<COMMAND_SET_CAR_WATERTIGHT, &SetCarWatertight>("924 SET_CAR_WATERTIGHT", VehFlag(924));
    Test<COMMAND_SET_CAR_STRONG, &SetCarStrong>("939 SET_CAR_STRONG", VehFlag(939));
    Test<COMMAND_SET_UPSIDEDOWN_CAR_NOT_DAMAGED, &SetUpsidedownCarNotDamaged>("1005 SET_UPSIDEDOWN_CAR_NOT_DAMAGED", VehFlag(1005));
    Test<COMMAND_SET_CAR_CAN_BE_DAMAGED, &SetCarCanBeDamaged>("1013 SET_CAR_CAN_BE_DAMAGED", VehFlag(1013));
    Test<COMMAND_SET_CAR_AVOID_LEVEL_TRANSITIONS, &SetCarAvoidLevelTransitions>("1064 SET_CAR_AVOID_LEVEL_TRANSITIONS", VehFlag(1064));
    Test<COMMAND_SET_CAR_STAY_IN_FAST_LANE, &SetCarStayInFastLane>("1126 SET_CAR_STAY_IN_FAST_LANE", VehFlag(1126));
    Test<COMMAND_MARK_CAR_AS_CONVOY_CAR, &MarkCarAsConvoyCar>("1213 MARK_CAR_AS_CONVOY_CAR", VehFlag(1213));
    Test<COMMAND_SET_HELI_STABILISER, &SetHeliStabiliser>("1247 SET_HELI_STABILISER", VehFlag(1247));
    Test<COMMAND_SET_CAR_STRAIGHT_LINE_DISTANCE, &SetCarStraightLineDistance>("1248 SET_CAR_STRAIGHT_LINE_DISTANCE", VehFlag(1248));
    Test<COMMAND_SET_CAR_RANDOM_ROUTE_SEED, &SetCarRandomRouteSeed>("1163 SET_CAR_RANDOM_ROUTE_SEED", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1163).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_CAR_STATUS, &SetCarStatus>("930 SET_CAR_STATUS", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 930).I(c.rng->below(3) ? (int32_t)c.rng->below(12) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_CAR_TRACTION, &SetCarTraction>("1059 SET_CAR_TRACTION", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1059).Fl(c.rng->F(5)).b; });
    Test<COMMAND_SET_CAR_TEMP_ACTION, &SetCarTempAction>("1143 SET_CAR_TEMP_ACTION", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.watch.push_back({ (void*)0xB7CB84, 4 }); *reinterpret_cast<uint32_t*>(0xB7CB84) = c.rng->u32();
        c.opcode = VehOp(c, 1143).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_CAR_COLOURS, &GetCarColours>("1011 GET_CAR_COLOURS", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1011).GV(0).GV(4).b; });
    Test<COMMAND_GET_CAR_MODEL, &GetCarModel>("1089 GET_CAR_MODEL", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1089).GV(0).b; });
    Test<COMMAND_IS_CAR_VISIBLY_DAMAGED, &IsCarVisiblyDamaged>("969 IS_CAR_VISIBLY_DAMAGED", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 969).b; });
    Test<COMMAND_IS_CAR_PASSENGER_SEAT_FREE, &IsCarPassengerSeatFree>("1073 IS_CAR_PASSENGER_SEAT_FREE", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1073).I(c.rng->below(12) - 1).b; });
    Test<COMMAND_IS_CAR_ON_FIRE, &IsCarOnFire>("1173 IS_CAR_ON_FIRE", [&](Ctx& c) {
        RandomVehicle(*c.rng); if (c.rng->below(2)) static_cast<CAutomobile*>(g_veh)->m_damageManager.m_nEngineStatus = (uint8)(200 + c.rng->below(56));
        VehWatch(c); c.opcode = VehOp(c, 1173).b; });
    Test<COMMAND_IS_CAR_TYRE_BURST, &IsCarTyreBurst>("1174 IS_CAR_TYRE_BURST", [&](Ctx& c) {
        RandomVehicle(*c.rng); auto* a = static_cast<CAutomobile*>(g_veh);
        for (auto& w : a->m_damageManager.m_anWheelsStatus) w = (eCarWheelStatus)c.rng->below(3);
        reinterpret_cast<uint8_t*>(g_veh)[0x65C] = (uint8_t)c.rng->below(3); reinterpret_cast<uint8_t*>(g_veh)[0x65D] = (uint8_t)c.rng->below(3);
        VehWatch(c); c.opcode = VehOp(c, 1174).I((int32_t)c.rng->below(6)).b; });
    Test<COMMAND_BURST_CAR_TYRE, &BurstCarTyre>("1278 BURST_CAR_TYRE", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1278).I(c.rng->below(2) ? (int32_t)c.rng->below(6) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_CAR_FORWARD_SPEED, &SetCarForwardSpeed>("1210 SET_CAR_FORWARD_SPEED", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1210).Fl(c.rng->F(300)).b; });
    Test<COMMAND_TURN_CAR_TO_FACE_COORD, &TurnCarToFaceCoord>("927 TURN_CAR_TO_FACE_COORD", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c);
        if (c.rng->below(2)) g_veh->m_matrix = nullptr;      // placement position instead of the matrix one
        c.opcode = VehOp(c, 927).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).b; });
    Test<COMMAND_SET_HELI_ORIENTATION, &SetHeliOrientation>("1232 SET_HELI_ORIENTATION", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1232).Fl(RandAngle(*c.rng)).b; });
    Test<COMMAND_HELI_GOTO_COORDS, &HeliGotoCoords>("1186 HELI_GOTO_COORDS", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1186).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(300)).Fl(c.rng->F(300)).Fl(c.rng->F(300)).b; });
    Test<COMMAND_PLANE_GOTO_COORDS, &PlaneGotoCoords>("1234 PLANE_GOTO_COORDS", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1234).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(300)).Fl(c.rng->F(300)).Fl(c.rng->F(300)).b; });
    Test<COMMAND_POP_CAR_BOOT, &PopCarBoot>("1249 POP_CAR_BOOT", [&](Ctx& c) { RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1249).b; });
    Test<COMMAND_CLOSE_ALL_CAR_DOORS, &CloseAllCarDoors>("1288 CLOSE_ALL_CAR_DOORS", [&](Ctx& c) { RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1288).b; });
    Test<COMMAND_IS_CAR_WAITING_FOR_WORLD_COLLISION, &IsCarWaitingForWorldCollision>("1265 IS_CAR_WAITING_FOR_WORLD_COLLISION", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = VehOp(c, 1265).b; });

    // ---- object commands: whole fake object compared
    auto ObjWatch = [](Ctx& c) { c.watch = { { g_obj, sizeof(CObject) } }; };
    Test<COMMAND_ADD_TO_OBJECT_VELOCITY, &AddToObjectVelocity>("908 ADD_TO_OBJECT_VELOCITY", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(908).I(g_objRef).Fl(c.rng->F(100)).Fl(c.rng->F(100)).Fl(c.rng->F(100)).b; });
    Test<COMMAND_SET_OBJECT_DRAW_LAST, &SetObjectDrawLast>("1048 SET_OBJECT_DRAW_LAST", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1048).I(g_objRef).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_OBJECT_RECORDS_COLLISIONS, &SetObjectRecordsCollisions>("1241 SET_OBJECT_RECORDS_COLLISIONS", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1241).I(g_objRef).I(c.rng->below(3)).b; });
    Test<COMMAND_HAS_OBJECT_COLLIDED_WITH_ANYTHING, &HasObjectCollidedWithAnything>("1242 HAS_OBJECT_COLLIDED_WITH_ANYTHING", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1242).I(g_objRef).b; });
    Test<COMMAND_IS_OBJECT_IN_WATER, &IsObjectInWater>("1255 IS_OBJECT_IN_WATER", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1255).I(c.rng->below(4) ? g_objRef : -1).b; });
    Test<COMMAND_SORT_OUT_OBJECT_COLLISION_WITH_CAR, &SortOutObjectCollisionWithCar>("1294 SORT_OUT_OBJECT_COLLISION_WITH_CAR", [&](Ctx& c) {
        RandomObject(*c.rng); RandomVehicle(*c.rng); ObjWatch(c); c.opcode = Op(1294).I(g_objRef).I(c.rng->below(3) ? g_vehRef : -1).b; });

    // ---- batch 2: recorders / globals / pools
    auto SetRet = [](Ctx& c) { g_hostRet = c.rng->u32(); };
    Test<COMMAND_DRAW_SPHERE, &DrawSphere>("929 DRAW_SPHERE", [](Ctx& c) {
        c.opcode = Op(929).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->below(2) ? c.rng->F(300) : -100.f).Fl(c.rng->F(50)).b; });
    Test<COMMAND_ADD_SPHERE, &AddSphere>("956 ADD_SPHERE", [](Ctx& c) {
        c.opcode = Op(956).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->below(2) ? c.rng->F(300) : -100.f).Fl(c.rng->F(50)).GV(12).b; });
    Test<COMMAND_REMOVE_SPHERE, &RemoveSphere>("957 REMOVE_SPHERE", [](Ctx& c) { c.opcode = Op(957).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_OBJECT_ROTATION, &SetObjectRotation>("1107 SET_OBJECT_ROTATION", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1107).I(g_objRef).Fl(c.rng->F(720)).Fl(c.rng->F(720)).Fl(c.rng->F(720)).b; });
    Test<COMMAND_SET_OBJECT_DYNAMIC, &SetObjectDynamic>("914 SET_OBJECT_DYNAMIC", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(914).I(g_objRef).I(c.rng->below(3)).b; });
    Test<COMMAND_CLEAR_CAR_LAST_WEAPON_DAMAGE, &ClearCarLastWeaponDamage>("1128 CLEAR_CAR_LAST_WEAPON_DAMAGE", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1128).I(c.rng->below(3) ? g_vehRef : -1).b; });
    Test<COMMAND_GET_DRIVER_OF_CAR, &GetDriverOfCar>("1132 GET_DRIVER_OF_CAR", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1132).I(g_vehRef).GV(0).b; });
    Test<COMMAND_SET_ENABLE_RC_DETONATE, &SetEnableRCDetonate>("1162 SET_ENABLE_RC_DETONATE", [](Ctx& c) {
        c.watch = { { (void*)0xC1CC00, 2 } }; c.opcode = Op(1162).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_ENABLE_RC_DETONATE_ON_CONTACT, &SetEnableRCDetonateOnContact>("1238 SET_ENABLE_RC_DETONATE_ON_CONTACT", [](Ctx& c) {
        c.watch = { { (void*)0xC1CC00, 2 } }; c.opcode = Op(1238).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_AREA_VISIBLE, &SetAreaVisible>("1211 SET_AREA_VISIBLE", [](Ctx& c) {
        c.watch = { { (void*)0xB72914, 4 } }; c.opcode = Op(1211).I(c.rng->below(2) ? (int32_t)c.rng->below(19) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_EXTRA_COLOURS, &SetExtraColours>("1273 SET_EXTRA_COLOURS", [](Ctx& c) { c.opcode = Op(1273).I((int32_t)c.rng->below(30)).I(c.rng->below(3)).b; });
    Test<COMMAND_CLEAR_EXTRA_COLOURS, &ClearExtraColours>("1274 CLEAR_EXTRA_COLOURS", [](Ctx& c) { c.opcode = Op(1274).I(c.rng->below(3)).b; });
    Test<COMMAND_REQUEST_COLLISION, &RequestCollision>("1252 REQUEST_COLLISION", [](Ctx& c) {
        c.watch = { { (void*)0xB72914, 4 } }; *reinterpret_cast<int32_t*>(0xB72914) = (int32_t)c.rng->below(19); c.opcode = Op(1252).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).b; });
    Test<COMMAND_SET_CAR_MODEL_COMPONENTS, &SetCarModelComponents>("1286 SET_CAR_MODEL_COMPONENTS", [](Ctx& c) {
        c.watch = { { (void*)0x8A6458, 2 } }; c.opcode = Op(1286).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_FREE_HEALTH_CARE, &SetFreeHealthCare>("1044 SET_FREE_HEALTH_CARE", [](Ctx& c) {
        c.watch = { { (void*)0xB7CD98, 0x190 * 2 } }; RandomFill((void*)0xB7CD98, 0x190 * 2, *c.rng); c.opcode = Op(1044).I(c.rng->below(2)).I(c.rng->below(3)).b; });
    Test<COMMAND_GET_WHEELIE_STATS, &GetWheelieStats>("1276 GET_WHEELIE_STATS", [](Ctx& c) {
        c.watch = { { (void*)0xB7CD98, 0x190 * 2 } }; RandomFill((void*)0xB7CD98, 0x190 * 2, *c.rng); c.opcode = Op(1276).I(c.rng->below(2)).GV(0).GV(4).GV(8).GV(12).GV(16).GV(20).b; });
    Test<COMMAND_GET_REMOTE_CONTROLLED_CAR, &GetRemoteControlledCar>("1156 GET_REMOTE_CONTROLLED_CAR", [&](Ctx& c) {
        c.watch = { { (void*)0xB7CD98, 0x190 * 2 } }; RandomFill((void*)0xB7CD98, 0x190 * 2, *c.rng);
        const int p = c.rng->below(2); CWorld::Players[p].m_pRemoteVehicle = c.rng->below(3) ? g_veh : nullptr;
        c.opcode = Op(1156).I(p).GV(0).b; });
    Test<COMMAND_SET_TOTAL_NUMBER_OF_MISSIONS, &SetTotalNumberOfMissions>("1068 SET_TOTAL_NUMBER_OF_MISSIONS", [](Ctx& c) { c.opcode = Op(1068).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_REGISTER_FASTEST_TIME, &RegisterFastestTime>("1070 REGISTER_FASTEST_TIME", [](Ctx& c) { c.opcode = Op(1070).I((int32_t)c.rng->below(300)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_CLEAR_MISSION_AUDIO, &ClearMissionAudio>("1037 CLEAR_MISSION_AUDIO", [](Ctx& c) { c.opcode = Op(1037).I((int32_t)c.rng->below(6)).b; });
    Test<COMMAND_HAS_MISSION_AUDIO_LOADED, &HasMissionAudioLoaded>("976 HAS_MISSION_AUDIO_LOADED", [&](Ctx& c) { SetRet(c); c.opcode = Op(976).I((int32_t)c.rng->below(6)).b; });
    Test<COMMAND_HAS_MISSION_AUDIO_FINISHED, &HasMissionAudioFinished>("978 HAS_MISSION_AUDIO_FINISHED", [&](Ctx& c) { SetRet(c); c.opcode = Op(978).I((int32_t)c.rng->below(6)).b; });
    Test<COMMAND_SET_MISSION_AUDIO_POSITION, &SetMissionAudioPosition>("983 SET_MISSION_AUDIO_POSITION", [](Ctx& c) {
        c.opcode = Op(983).I((int32_t)c.rng->below(6)).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).b; });
    Test<COMMAND_SET_NEAR_CLIP, &SetNearClip>("1053 SET_NEAR_CLIP", [](Ctx& c) { c.opcode = Op(1053).Fl(c.rng->F(5)).b; });
    Test<COMMAND_SET_INTERPOLATION_PARAMETERS, &SetInterpolationParameters>("1120 SET_INTERPOLATION_PARAMETERS", [](Ctx& c) { c.opcode = Op(1120).Fl(c.rng->F(100)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_RADIO_CHANNEL, &SetRadioChannel>("1054 SET_RADIO_CHANNEL", [](Ctx& c) {
        c.opcode = Op(1054).I(c.rng->below(3) == 0 ? (int32_t)c.rng->u32() : (int32_t)c.rng->below(14) - 1).b; });
    Test<COMMAND_PLAY_MISSION_PASSED_TUNE, &PlayMissionPassedTune>("916 PLAY_MISSION_PASSED_TUNE", [](Ctx& c) { c.opcode = Op(916).I((int32_t)c.rng->below(5) - 1).b; });
    Test<COMMAND_SET_MUSIC_DOES_FADE, &SetMusicDoesFade>("1084 SET_MUSIC_DOES_FADE", [](Ctx& c) { c.watch = { { (void*)0xB6F04D, 1 } }; c.opcode = Op(1084).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_DRUNK_INPUT_DELAY, &SetDrunkInputDelay>("1021 SET_DRUNK_INPUT_DELAY", [](Ctx& c) {
        c.watch = { { (void*)&CPad::Pads[0], sizeof(CPad) * 2 } }; c.opcode = Op(1021).I(c.rng->below(2)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GET_POSITION_OF_ANALOGUE_STICKS, &GetPositionOfAnalogueSticks>("1172 GET_POSITION_OF_ANALOGUE_STICKS", [](Ctx& c) {
        c.watch = { { (void*)&CPad::Pads[0], sizeof(CPad) * 2 } }; RandomFill(&CPad::Pads[0], sizeof(CPad) * 2, *c.rng); c.opcode = Op(1172).I(c.rng->below(2)).GV(0).GV(4).GV(8).GV(12).b; });
    Test<COMMAND_IS_ANY_PICKUP_AT_COORDS, &IsAnyPickupAtCoords>("1164 IS_ANY_PICKUP_AT_COORDS", [](Ctx& c) {
        auto& r = *c.rng; const float tx = r.F(10, false), ty = r.F(10, false), tz = r.F(10, false);
        c.watch = { { (void*)&CPickups::aPickUps[0], sizeof(CPickup) * MAX_NUM_PICKUPS } };
        std::memset(&CPickups::aPickUps[0], 0, sizeof(CPickup) * MAX_NUM_PICKUPS);
        for (int i = 0; i < 20; ++i) {
            auto& p = CPickups::aPickUps[r.below(MAX_NUM_PICKUPS)];
            p.m_nPickupType = (ePickupType)(1 + r.below(8));
            p.m_vecPos = CVector{ tx + r.F(1.0f, false), ty + r.F(1.0f, false), tz + r.F(1.0f, false) };
        }
        c.opcode = Op(1164).Fl(tx).Fl(ty).Fl(tz).b; });
    auto GarageOp = [&](int cmd) { return [=](Ctx& c) { g_hostRet = c.rng->u32(); char n[9] = {}; for (int i = 0; i < 8; ++i) n[i] = (char)('A' + c.rng->below(26)); c.opcode = Op(cmd).Str8(n).b; }; };
    Test<COMMAND_IS_GARAGE_OPEN, &IsGarageOpen>("944 IS_GARAGE_OPEN", GarageOp(944));
    Test<COMMAND_IS_GARAGE_CLOSED, &IsGarageClosed>("945 IS_GARAGE_CLOSED", GarageOp(945));
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice

    // exe callees of the handlers -> host recorders
    oracle::Patch(0x566610, (void*)&H_ClearCars);
    oracle::Patch(0x5667F0, (void*)&H_ClearPeds);
    oracle::Patch(0x569660, (void*)&H_GroundZ);
    oracle::Patch(0x56A0D0, (void*)&H_ClearExciting);
    oracle::Patch(0x43E0C0, (void*)&H_SetHeading);
    oracle::Patch(0x6A2450, (void*)&H_SetHeliOrientation);
    oracle::Patch(0x6D2460, (void*)&H_Extinguish);
    oracle::Patch(0x423FC0, (void*)&H_SwitchReal);
    oracle::Patch(0x6AF910, (void*)&H_PopBoot);
    oracle::Patch(0x6A4520, (void*)&H_CloseDoors);
    oracle::Patch(0x6A2390, (void*)&H_HeliGoto);
    oracle::Patch(0x6A2470, (void*)&H_PlaneGoto);
    oracle::Patch(0x725BA0, (void*)&H_Marker);
    oracle::Patch(0x483B30, (void*)&H_AddSphere);
    oracle::Patch(0x483BA0, (void*)&H_RemoveSphere);
    oracle::Patch(0x563280, (void*)&H_WorldRemove);
    oracle::Patch(0x563220, (void*)&H_WorldAdd);
    oracle::Patch(0x439A80, (void*)&H_SetOrientation);
    oracle::Patch(0x446F90, (void*)&H_UpdateRwMatrix);
    oracle::Patch(0x532B00, (void*)&H_UpdateRwFrame);
    oracle::Patch(0x542800, (void*)&H_AddMoving);
    oracle::Patch(0x542860, (void*)&H_RemoveMoving);
    oracle::Patch(0x4094B0, (void*)&H_RemoveBuildings);
    oracle::Patch(0x55FEC0, (void*)&H_StartExtra);
    oracle::Patch(0x55FF20, (void*)&H_StopExtra);
    oracle::Patch(0x410C00, (void*)&H_ReqCol);
    oracle::Patch(0x55A070, (void*)&H_SetStat);
    oracle::Patch(0x55A0B0, (void*)&H_FastestTime);
    oracle::Patch(0x5072F0, (void*)&H_ClearMissionAudio);
    oracle::Patch(0x5072A0, (void*)&H_AudioStatus);
    oracle::Patch(0x5072C0, (void*)&H_AudioFinished);
    oracle::Patch(0x507300, (void*)&H_AudioPos);
    oracle::Patch(0x50BF90, (void*)&H_NearClip);
    oracle::Patch(0x50C030, (void*)&H_Interp);
    oracle::Patch(0x507E10, (void*)&H_Retune);
    oracle::Patch(0x507F40, (void*)&H_Preload);
    oracle::Patch(0x507180, (void*)&H_PlayPre);
    oracle::Patch(0x447680, (void*)&H_GarageByName);
    oracle::Patch(0x447D00, (void*)&H_GarageOpen);
    oracle::Patch(0x447D30, (void*)&H_GarageClosed);

    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
