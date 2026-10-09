// S6-D: differential test of SCRIPT COMMAND HANDLERS (group g16, ids 1600..1699) against the original machine code (exe oracle, see game_oracle.h and
// script_oracle_test.cpp, whose scaffolding this file follows).
// The handler TU source/game_sa/Scripts/Commands/Ported/Group16_17a.cpp is #included here (its handlers live in an anonymous namespace), the test maps
// gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original addresses and, for every random case, runs
//   (1) the exe's group processor ProcessCommands1600To1699 (0x493FE0, thiscall(this = fake CRunningScript, cmd)) on a synthetic opcode buffer, and
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then compares the script variables (ScriptSpace), the IP, the compare flag, the watched memory and the log of the callee functions the handler invoked
// (the exe callees are patched with host recorders, the C++ side defines the same recorders for the game functions).
// usage: script_oracle_g16_test.exe [-v] [-n cases] [name-substring ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group16_17a.cpp"

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

static CVehicle* g_veh;     static int g_vehRef;
static CObject*  g_obj;     static int g_objRef;
static CPed*     g_ped;     static int g_pedRef;
static CMatrix*  g_mat;
static void*     g_fakeVtbl[256];
static uint8_t   g_fakeIntel[0x294];
static uint8_t   g_fakeFx[16][8];
static uint32_t Tag(const void* p) { // identity of a fake entity, so that logs do not contain host addresses
    if (!p) return 0;
    if (p == g_veh) return 1;
    if (p == g_obj) return 2;
    if (p == g_ped) return 3;
    for (int i = 0; i < 16; ++i) if (p == g_fakeFx[i]) return 0x100 + i;
    return 9;
}

static float FakeGroundZ(float x, float y) { return x * 0.25f + y * 0.125f - 7.0f; }
static uint32_t g_statusRet; // return value of the faked GetStatus

// -- host recorders for the exe (cdecl / thiscall: __fastcall(this, edx, args...) pops the stack args like a thiscall callee)
static float __cdecl H_GroundZ(float x, float y) { return FakeGroundZ(x, y); }
static void  __fastcall H_Attach(void* self, int, void* target, float a, float b, float c, float d, float e, float f) { LogF(21, { (float)Tag(self), (float)Tag(target), a, b, c, d, e, f }); }
static void  __fastcall H_AttachQuat(void* self, int, void* target, void* off, void* quat) { LogF(22, { (float)Tag(self), (float)Tag(target), (float)Tag(off), (float)Tag(quat) }); }
static void  __fastcall H_Detach(void* self, int, float x, float y, float z, int flag) { LogF(23, { (float)Tag(self), x, y, z, (float)(uint8_t)flag }); }
static void  __fastcall H_PopDoor(void* self, int, int node, int door, int flag) { LogF(24, { (float)Tag(self), (float)node, (float)door, (float)(uint8_t)flag }); }
static void  __fastcall H_FixDoor(void* self, int, int node, int door) { LogF(25, { (float)Tag(self), (float)node, (float)door }); }
static void  __fastcall H_PopPanel(void* self, int, int node, int panel, int flag) { LogF(26, { (float)Tag(self), (float)node, (float)panel, (float)(uint8_t)flag }); }
static void  __fastcall H_FixPanel(void* self, int, int node, int panel) { LogF(27, { (float)Tag(self), (float)node, (float)panel }); }
static void  __fastcall H_FixTyre(void* self, int, int wheel) { LogF(28, { (float)Tag(self), (float)wheel }); }
static void  __fastcall H_FxPlay(void* self, int) { Log(29); g_log.push_back(Tag(self)); }
static void  __fastcall H_FxStop(void* self, int) { Log(30); g_log.push_back(Tag(self)); }
static void  __fastcall H_FxPlayAndKill(void* self, int) { Log(31); g_log.push_back(Tag(self)); }
static void  __fastcall H_FxKill(void* self, int) { Log(32); g_log.push_back(Tag(self)); }
static void  __cdecl H_RemoveScriptFx(int id) { LogU(33, { (uint32_t)id }); }
static void  __fastcall H_CleanupRemove(void* self, int, int handle, int type) { LogU(34, { (uint32_t)handle, (uint32_t)type }); }
static void  __fastcall H_OpenDoor(void* self, int, void* ped, int node, int door, float ratio, int sound) { LogF(35, { (float)Tag(self), (float)Tag(ped), (float)node, (float)door, ratio, (float)(uint8_t)sound }); }
static bool  __fastcall H_IsDoorMissing(void* self, int, int door) { LogF(36, { (float)door }); return (door * 7 + (int)(uintptr_t)self) % 3 == 0; }
static void  __fastcall H_UnloadDM(int) { }

// -- the same recorders as the game functions the handlers call (C++ side)
float CWorld::FindGroundZForCoord(float x, float y) { return FakeGroundZ(x, y); }
void  CPhysical::AttachEntityToEntity(CPhysical* target, CVector o, CVector r) { LogF(21, { (float)Tag(this), (float)Tag(target), o.x, o.y, o.z, r.x, r.y, r.z }); }
void  CPhysical::AttachEntityToEntity(CPhysical* target, CVector* o, CQuaternion* q) { LogF(22, { (float)Tag(this), (float)Tag(target), (float)Tag(o), (float)Tag(q) }); }
void  CPhysical::DettachEntityFromEntity(float x, float y, float z, bool flag) { LogF(23, { (float)Tag(this), x, y, z, (float)flag }); }
void  CAutomobile::PopDoor(eCarNodes node, eDoors door, bool flag) { LogF(24, { (float)Tag(this), (float)node, (float)door, (float)flag }); }
void  CAutomobile::FixDoor(int32 node, eDoors door) { LogF(25, { (float)Tag(this), (float)node, (float)door }); }
void  CAutomobile::PopPanel(eCarNodes node, ePanels panel, bool flag) { LogF(26, { (float)Tag(this), (float)node, (float)panel, (float)flag }); }
void  CAutomobile::FixPanel(eCarNodes node, ePanels panel) { LogF(27, { (float)Tag(this), (float)node, (float)panel }); }
void  CAutomobile::FixTyre(eWheels wheel) { LogF(28, { (float)Tag(this), (float)wheel }); }
void  FxSystem_c::Play() { Log(29); g_log.push_back(Tag(this)); }
void  FxSystem_c::Stop() { Log(30); g_log.push_back(Tag(this)); }
void  FxSystem_c::PlayAndKill() { Log(31); g_log.push_back(Tag(this)); }
void  FxSystem_c::Kill() { Log(32); g_log.push_back(Tag(this)); }
void  CTheScripts::RemoveScriptEffectSystem(int32 id) { LogU(33, { (uint32_t)id }); }
void  CMissionCleanup::RemoveEntityFromList(int32 handle, MissionCleanUpEntityType type) { LogU(34, { (uint32_t)handle, (uint32_t)type }); }
// the exe's own pure functions, used by the port as callees
int32 CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { return oracle::Fn<int __cdecl(int, int)>(0x4839A0)(ref, (int)type); }
int32 CPickups::GetActualPickupIndex(tPickupReference r) { return oracle::Fn<int __cdecl(int)>(0x4552A0)(r.num); }
eCarNodes CDamageManager::GetCarNodeIndexFromDoor(eDoors door) { return (eCarNodes)oracle::Fn<int __cdecl(int)>(0x6C26F0)((int)door); }
eCarNodes CDamageManager::GetCarNodeIndexFromPanel(ePanels panel) { return (eCarNodes)oracle::Fn<int __cdecl(int)>(0x6C26A0)((int)panel); }

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
};

struct Watch { void* p; size_t n; };

struct Ctx {
    Rng*                 rng{};
    std::vector<Watch>   watch;          // memory regions compared after the command (and restored between the two runs)
    bool                 notFlag{};
    bool                 cleanup{};      // CRunningScript::m_UsesMissionCleanup
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
    constexpr unsigned groupFn = 0x493FE0; // ProcessCommands1600To1699
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
        S.m_UsesMissionCleanup = c.cleanup;
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
    const bool exeRet = oracle::Fn<unsigned char __fastcall(CRunningScript*, int, int)>(groupFn)(&S, 0, cmdId) != 0;
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
        Ctx c; c.rng = &rng; c.notFlag = rng.below(4) == 0; c.cleanup = rng.below(2) == 0;
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
static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }

// fully random vehicle memory, but with the pointers the commands dereference kept valid
static void RandomVehicle(Rng& r) {
    RandomFill(g_veh, sizeof(CAutomobile), r);
    RandomFill(g_mat, sizeof(CMatrix), r);
    *reinterpret_cast<void**>(g_veh) = g_fakeVtbl;                         // vptr
    g_veh->m_matrix = reinterpret_cast<decltype(g_veh->m_matrix)>(g_mat);   // +0x14
    g_veh->m_vecMoveSpeed = CVector{ r.F(1.f, false), r.F(1.f, false), r.F(1.f, false) };
    g_veh->m_pAttachedTo = r.below(2) ? nullptr : reinterpret_cast<CPhysical*>((uintptr_t)0x1000 + r.below(100));
    auto* a = static_cast<CAutomobile*>(g_veh);
    for (auto& n : a->m_aCarNodes) n = r.below(3) ? nullptr : reinterpret_cast<RwFrame*>((uintptr_t)0x2000);
    g_veh->m_fMass = r.below(8) == 0 ? r.F(1e6f) : r.f01() * 5000.f;
}
static void RandomObject(Rng& r) {
    RandomFill(g_obj, sizeof(CObject), r);
    g_obj->m_pAttachedTo = r.below(2) ? nullptr : reinterpret_cast<CPhysical*>((uintptr_t)0x1000 + r.below(100));
}

static void SetupFakeWorld() {
    auto* pp = new CPedPool(4, "oracle_ped");
    auto* vp = new CVehiclePool(4, "oracle_veh");
    auto* op = new CObjectPool(4, "oracle_obj");
    *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp;
    *reinterpret_cast<CObjectPool**>(0xB7449C) = op;
    g_ped = pp->New();  g_pedRef = pp->GetRef(g_ped);
    g_veh = vp->New();  g_vehRef = vp->GetRef(g_veh);
    g_obj = op->New();  g_objRef = op->GetRef(g_obj);
    g_mat = new CMatrix();
    std::memset(g_ped, 0, sizeof(CPed));
    reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C)[0] = g_fakeIntel;   // CPed::m_pIntelligence
    g_fakeVtbl[0x98 / 4] = (void*)&H_IsDoorMissing;      // CVehicle::IsDoorMissing(eDoors) -- slot 38
    g_fakeVtbl[0x6C / 4] = (void*)&H_OpenDoor;           // CVehicle::OpenDoor(ped, node, door, ratio, sound) -- slot 27
}

// ---------------------------------------------------------------------------------------------------------------------------------
static float RandAngle(Rng& r) { return r.below(10) == 0 ? r.F(50000.f) : r.below(4) == 0 ? r.F(400.f, false) : r.F(2000.f, true); }

static void TestAll() {
    // ---- pure float function (x87 loops, NaN branches)
    Test<COMMAND_LIMIT_ANGLE, &LimitAngle>("1622 LIMIT_ANGLE", [](Ctx& c) { c.opcode = Op(1622).Fl(RandAngle(*c.rng)).GV(8).b; });

    // ---- vehicle / object getters and flags (whole fake entity compared)
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CAutomobile) } }; };
    auto ObjWatch = [](Ctx& c) { c.watch = { { g_obj, sizeof(CObject) } }; };
    Test<COMMAND_GET_CAR_SPEED_VECTOR, &GetCarSpeedVector>("1698 GET_CAR_SPEED_VECTOR", [&](Ctx& c) {
        RandomVehicle(*c.rng); if (c.rng->below(5) == 0) g_veh->m_vecMoveSpeed = CVector{ c.rng->F(1e30f), c.rng->F(1e-3f), c.rng->F(5.f) };
        VehWatch(c); c.opcode = Op(1698).I(g_vehRef).GV(0).GV(4).GV(8).b; });
    Test<COMMAND_GET_CAR_MASS, &GetCarMass>("1699 GET_CAR_MASS", [&](Ctx& c) { RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1699).I(g_vehRef).GV(0).b; });
    Test<COMMAND_IS_VEHICLE_ATTACHED, &IsVehicleAttached>("1670 IS_VEHICLE_ATTACHED", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1670).I(c.rng->below(4) ? g_vehRef : -1).b; });
    Test<COMMAND_IS_OBJECT_ATTACHED, &IsObjectAttached>("1669 IS_OBJECT_ATTACHED", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1669).I(c.rng->below(4) ? g_objRef : -1).b; });
    Test<COMMAND_FORCE_CAR_LIGHTS, &ForceCarLights>("1663 FORCE_CAR_LIGHTS", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1663).I(g_vehRef).I(c.rng->below(2) ? (int32_t)c.rng->below(4) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_OBJECT_RENDER_SCORCHED, &SetObjectRenderScorched>("1620 SET_OBJECT_RENDER_SCORCHED", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1620).I(g_objRef).I(c.rng->below(3)).b; });

    // ---- ped intelligence
    Test<COMMAND_SET_FOLLOW_NODE_THRESHOLD_DISTANCE, &SetFollowNodeThresholdDistance>("1608 SET_FOLLOW_NODE_THRESHOLD_DISTANCE", [&](Ctx& c) {
        RandomFill(g_fakeIntel, sizeof(g_fakeIntel), *c.rng); c.watch = { { g_fakeIntel, sizeof(g_fakeIntel) } };
        c.opcode = Op(1608).I(g_pedRef).Fl(c.rng->F(100.f)).b; });

    // ---- pickups: aPickUps (0x9788C0, 0x20 stride) random for the first entries
    Test<COMMAND_GET_PICKUP_COORDINATES, &GetPickupCoordinates>("1627 GET_PICKUP_COORDINATES", [&](Ctx& c) {
        RandomFill((void*)0x9788C0, 4 * 0x20, *c.rng); c.watch = { { (void*)0x9788C0, 4 * 0x20 } };
        const int idx = c.rng->below(4);
        const int16_t ref = *reinterpret_cast<int16_t*>(0x9788C0 + idx * 0x20 + offsetof(CPickup, m_nReferenceIndex));
        const int32_t h = c.rng->below(6) == 0 ? -1 : c.rng->below(5) == 0 ? (int32_t)((uint32_t)c.rng->below(620) | ((uint32_t)c.rng->below(4) << 16)) : (int32_t)((uint32_t)idx | ((uint32_t)(uint16_t)ref << 16));
        c.opcode = Op(1627).I(h).GV(0).GV(4).GV(8).b; });

    // ---- 2d effect user lists (ADD_PEDTYPE_AS_ATTRACTOR_USER): ms_userLists (0xC3A200, 0x24 stride), activation flags and reference indices
    Test<COMMAND_ADD_PEDTYPE_AS_ATTRACTOR_USER, &AddPedTypeAsAttractorUser>("1664 ADD_PEDTYPE_AS_ATTRACTOR_USER", [&](Ctx& c) {
        auto& r = *c.rng;
        const int n = 8;
        RandomFill((void*)0xC3A200, n * 0x24, r);
        for (int i = 0; i < n; ++i) { auto* l = (int32_t*)(0xC3A200 + i * 0x24); for (int k = 0; k < 4; ++k) if (r.below(2)) l[k] = -1; ((uint8_t*)l)[0x20] = (uint8_t)r.below(2); }
        for (int i = 0; i < n; ++i) { *(uint8_t*)(0xC3A1A0 + i) = (uint8_t)r.below(4) != 0; *(uint16_t*)(0xC3A120 + i * 2) = (uint16_t)r.below(5); }
        c.watch = { { (void*)0xC3A200, n * 0x24 } };
        const int idx = r.below(n + 2);
        c.opcode = Op(1664).I((int32_t)((uint32_t)idx | ((uint32_t)r.below(5) << 16))).I(r.below(30) - 3).b; });

    // ---- FX systems: ScriptEffectSystemArray (0xA44110, 8-byte entries {bool used; int16 id; FxSystem_c*})
    auto FxSetup = [&](Ctx& c) {
        auto& r = *c.rng;
        for (int i = 0; i < 8; ++i) {
            auto* e = (uint8_t*)(0xA44110 + i * 8);
            e[0] = (uint8_t)r.below(4) != 0; *(int16_t*)(e + 2) = (int16_t)r.below(4); *(void**)(e + 4) = r.below(4) ? (void*)g_fakeFx[r.below(16)] : nullptr;
        }
        c.watch = { { (void*)0xA44110, 64 } };
        const int idx = r.below(10);
        const int32_t h = r.below(8) == 0 ? -1 : (int32_t)((uint32_t)idx | ((uint32_t)r.below(5) << 16));
        return h;
    };
    Test<COMMAND_PLAY_FX_SYSTEM, &PlayFxSystem>("1612 PLAY_FX_SYSTEM", [&](Ctx& c) { c.opcode = Op(1612).I(FxSetup(c)).b; });
    Test<COMMAND_STOP_FX_SYSTEM, &StopFxSystem>("1614 STOP_FX_SYSTEM", [&](Ctx& c) { c.opcode = Op(1614).I(FxSetup(c)).b; });
    Test<COMMAND_PLAY_AND_KILL_FX_SYSTEM, &PlayAndKillFxSystem>("1615 PLAY_AND_KILL_FX_SYSTEM", [&](Ctx& c) { c.opcode = Op(1615).I(FxSetup(c)).b; });
    Test<COMMAND_KILL_FX_SYSTEM, &KillFxSystem>("1616 KILL_FX_SYSTEM", [&](Ctx& c) { c.opcode = Op(1616).I(FxSetup(c)).b; });

    // ---- attach / detach (callees recorded: both overloads of AttachEntityToEntity, DettachEntityFromEntity)
    auto Deg = [](Rng& r) { return r.below(8) == 0 ? r.F(1e6f) : r.F(720.f); };
    Test<COMMAND_ATTACH_OBJECT_TO_CAR, &AttachObjectToCar>("1665 ATTACH_OBJECT_TO_CAR", [&](Ctx& c) {
        auto& r = *c.rng; RandomObject(r); RandomVehicle(r); ObjWatch(c);
        c.opcode = Op(1665).I(g_objRef).I(g_vehRef).Fl(r.F(10)).Fl(r.F(10)).Fl(r.F(10)).Fl(Deg(r)).Fl(Deg(r)).Fl(Deg(r)).b; });
    Test<COMMAND_ATTACH_OBJECT_TO_OBJECT, &AttachObjectToObject>("1690 ATTACH_OBJECT_TO_OBJECT", [&](Ctx& c) {
        auto& r = *c.rng; RandomObject(r); ObjWatch(c);
        c.opcode = Op(1690).I(g_objRef).I(g_objRef).Fl(r.F(10)).Fl(r.F(10)).Fl(r.F(10)).Fl(Deg(r)).Fl(Deg(r)).Fl(Deg(r)).b; });
    Test<COMMAND_ATTACH_OBJECT_TO_CHAR, &AttachObjectToChar>("1691 ATTACH_OBJECT_TO_CHAR", [&](Ctx& c) {
        auto& r = *c.rng; RandomObject(r); ObjWatch(c);
        c.opcode = Op(1691).I(g_objRef).I(g_pedRef).Fl(r.F(10)).Fl(r.F(10)).Fl(r.F(10)).Fl(Deg(r)).Fl(Deg(r)).Fl(Deg(r)).b; });
    Test<COMMAND_ATTACH_CAR_TO_CAR, &AttachCarToCar>("1667 ATTACH_CAR_TO_CAR", [&](Ctx& c) {
        auto& r = *c.rng; RandomVehicle(r); VehWatch(c);
        const float ox = r.below(3) == 0 ? -999.9f + (r.below(3) - 1) * 0.001f * (float)r.below(2) : r.below(3) == 0 ? -1000.f - r.f01() : r.F(10);
        c.opcode = Op(1667).I(g_vehRef).I(g_vehRef).Fl(ox).Fl(r.F(10)).Fl(r.F(10)).Fl(Deg(r)).Fl(Deg(r)).Fl(Deg(r)).b; });
    Test<COMMAND_DETACH_OBJECT, &DetachObject>("1666 DETACH_OBJECT", [&](Ctx& c) {
        auto& r = *c.rng; RandomObject(r); ObjWatch(c);
        c.opcode = Op(1666).I(r.below(5) ? g_objRef : -1).Fl(Deg(r)).Fl(Deg(r)).Fl(r.F(10)).I(r.below(3)).b; });
    Test<COMMAND_DETACH_CAR, &DetachCar>("1668 DETACH_CAR", [&](Ctx& c) {
        auto& r = *c.rng; RandomVehicle(r); VehWatch(c);
        c.opcode = Op(1668).I(r.below(5) ? g_vehRef : -1).Fl(Deg(r)).Fl(Deg(r)).Fl(r.F(10)).I(r.below(3)).b; });

    // ---- car damage commands (callees recorded; GetCarNodeIndexFrom* forwarded to the exe)
    Test<COMMAND_POP_CAR_DOOR, &PopCarDoor>("1673 POP_CAR_DOOR", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1673).I(g_vehRef).I(c.rng->below(6)).I(c.rng->below(3)).b; });
    Test<COMMAND_FIX_CAR_DOOR, &FixCarDoor>("1674 FIX_CAR_DOOR", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1674).I(g_vehRef).I(c.rng->below(6)).b; });
    Test<COMMAND_POP_CAR_PANEL, &PopCarPanel>("1687 POP_CAR_PANEL", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1687).I(g_vehRef).I(c.rng->below(7)).I(c.rng->below(3)).b; });
    Test<COMMAND_FIX_CAR_PANEL, &FixCarPanel>("1688 FIX_CAR_PANEL", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1688).I(g_vehRef).I(c.rng->below(7)).b; });
    Test<COMMAND_FIX_CAR_TYRE, &FixCarTyre>("1689 FIX_CAR_TYRE", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1689).I(g_vehRef).I(c.rng->below(4)).b; });
    Test<COMMAND_OPEN_CAR_DOOR, &OpenCarDoor>("1623 OPEN_CAR_DOOR", [&](Ctx& c) {
        RandomVehicle(*c.rng); VehWatch(c); c.opcode = Op(1623).I(g_vehRef).I(c.rng->below(6)).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g16_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice

    // exe callees of the handlers -> host recorders
    oracle::Patch(0x569660, (void*)&H_GroundZ);
    oracle::Patch(0x54D570, (void*)&H_Attach);
    oracle::Patch(0x54D690, (void*)&H_AttachQuat);
    oracle::Patch(0x5442F0, (void*)&H_Detach);
    oracle::Patch(0x6ADEF0, (void*)&H_PopDoor);
    oracle::Patch(0x6A35A0, (void*)&H_FixDoor);
    oracle::Patch(0x6ADF80, (void*)&H_PopPanel);
    oracle::Patch(0x6A3670, (void*)&H_FixPanel);
    oracle::Patch(0x6A3580, (void*)&H_FixTyre);
    oracle::Patch(0x4AA2F0, (void*)&H_FxPlay);
    oracle::Patch(0x4AA390, (void*)&H_FxStop);
    oracle::Patch(0x4AA3D0, (void*)&H_FxPlayAndKill);
    oracle::Patch(0x4AA3F0, (void*)&H_FxKill);
    oracle::Patch(0x492FD0, (void*)&H_RemoveScriptFx);
    oracle::Patch(0x4654B0, (void*)&H_CleanupRemove);

    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g16_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
