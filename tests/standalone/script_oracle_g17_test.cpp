// S6-D: differential test of SCRIPT COMMAND HANDLERS (group g17b, ids 1700..1764) against the original machine code (exe oracle, see game_oracle.h and
// script_oracle_test.cpp, whose scaffolding this file follows).
// The handler TU source/game_sa/Scripts/Commands/Ported/Group16_17b.cpp is #included here (its handlers live in an anonymous namespace), the test maps
// gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original addresses and, for every random case, runs
//   (1) the exe's group processor ProcessCommands1700To1799 (0x496E00, thiscall(this = fake CRunningScript, cmd)) on a synthetic opcode buffer, and
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then compares the script variables (ScriptSpace), the IP, the compare flag, the watched memory and the log of the callee functions the handler invoked
// (the exe callees are patched with host recorders, the C++ side defines the same recorders for the game functions).
// usage: script_oracle_g17_test.exe [-v] [-n cases] [name-substring ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group16_17b.cpp"
#include "World.h"
#include "Pickups.h"
#include "ScriptResourceManager.h"
#include "MissionCleanup.h"
#include "DamageManager.h"
#include "Automobile.h"
#include "Fx/Fx.h"
#include "Fx/FxManager.h"
#include "Fx/FxSystem.h"
#include "Ragdoll/IKChainManager.h"
#include "DecisionMakers/DecisionMakerTypesFileLoader.h"
#include "Models/VehicleModelInfo.h"
#include "Tasks/PedScriptedTaskRecord.h"
#include "Tasks/TaskSequences.h"
#include "Tasks/TaskComplexUseSequence.h"
#include "TaskTypes/TaskSimpleClearLookAt.h"
#include "TaskTypes/TaskSimpleTriggerLookAt.h"
#include "TaskTypes/TaskSimpleGunControl.h"
#include "TaskTypes/TaskSimpleThrowControl.h"
#include "TaskTypes/TaskComplexDestroyCar.h"
#include "TaskTypes/TaskComplexEvasiveDiveAndGetUp.h"
#include "TaskTypes/TaskComplexPartnerChat.h"
#include "TaskTypes/TaskComplexShuffleSeats.h"
#include "TaskTypes/TaskComplexLeaveAnyCar.h"
#include "TaskTypes/TaskSimpleTogglePedThreatScanner.h"

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
static bool g_trace = false;
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static void Log(uint32_t tag) { if (g_trace) std::printf("    log %u\n", tag); g_log.push_back(tag); }
static void LogF(uint32_t tag, std::initializer_list<float> v) { if (g_trace) std::printf("    logF %u\n", tag); g_log.push_back(tag); for (float f : v) g_log.push_back(FB(f)); }
static void LogU(uint32_t tag, std::initializer_list<uint32_t> v) { if (g_trace) std::printf("    logU %u\n", tag); g_log.push_back(tag); for (auto u : v) g_log.push_back(u); }

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
static void  __fastcall H_PopDoor(void* self, int, int node, uint8_t door, int flag) { LogF(24, { (float)Tag(self), (float)node, (float)door, (float)(uint8_t)flag }); }
static void  __fastcall H_FixDoor(void* self, int, int node, uint8_t door) { LogF(25, { (float)Tag(self), (float)node, (float)door }); }
static void  __fastcall H_PopPanel(void* self, int, int node, int panel, int flag) { LogF(26, { (float)Tag(self), (float)node, (float)panel, (float)(uint8_t)flag }); }
static void  __fastcall H_FixPanel(void* self, int, int node, int panel) { LogF(27, { (float)Tag(self), (float)node, (float)panel }); }
static void  __fastcall H_FixTyre(void* self, int, int wheel) { LogF(28, { (float)Tag(self), (float)wheel }); }
static void  __fastcall H_FxPlay(void* self, int) { Log(29); g_log.push_back(Tag(self)); }
static void  __fastcall H_FxStop(void* self, int) { Log(30); g_log.push_back(Tag(self)); }
static void  __fastcall H_FxPlayAndKill(void* self, int) { Log(31); g_log.push_back(Tag(self)); }
static void  __fastcall H_FxKill(void* self, int) { Log(32); g_log.push_back(Tag(self)); }
static void  __cdecl H_RemoveScriptFx(int id) { LogU(33, { (uint32_t)id }); }
static void  __fastcall H_CleanupRemove(void* self, int, int handle, int type) { LogU(34, { (uint32_t)handle, (uint32_t)type }); }
static void  __fastcall H_OpenDoor(void* self, int, void* ped, int node, uint8_t door, float ratio, int sound) { LogF(35, { (float)Tag(self), (float)Tag(ped), (float)node, (float)door, ratio, (float)(uint8_t)sound }); }
static bool  __fastcall H_IsDoorMissing(void* self, int, uint8_t door) { LogF(36, { (float)door }); return (door * 7 + (int)(uintptr_t)self) % 3 == 0; }
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
    Op& Str8(const char* s) { b.push_back(9); for (int i = 0; i < 8; ++i) b.push_back(i < (int)std::strlen(s) ? (uint8_t)s[i] : 0); return *this; }   // static 8-byte text label
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

// ---------------------------------------------------------------------------------------------------------------------------------
// S6-D review: task / FX / misc recorders. Every task constructor the handlers call is replaced on BOTH sides by a recorder that logs the
// arguments it receives (this catches wrong argument order / wrong constants / wrong ctor); `operator new` is replaced by a logging calloc
// (this checks that the exe's allocation size equals sizeof the C++ class); GivePedScriptedTask / AddTask log the task's ordinal and a hash of
// its memory (fields the handler writes after the constructor, e.g. CTaskComplexUseSequence+0x10).
static std::vector<void*>  g_allocs;
static std::vector<size_t> g_allocSz;
static uint32_t TaskOrd(const void* p) {
    if (!p) return 0;
    for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == p) return (uint32_t)i + 1;
    return 0xF000; // stack object
}
static uint32_t TaskHash(const void* p) {
    for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == p) {
        uint32_t h = 2166136261u;
        for (size_t k = 4; k < g_allocSz[i]; ++k) {
            uint32_t w = 0; if (k % 4 == 0 && k + 4 <= g_allocSz[i]) std::memcpy(&w, (const uint8_t*)p + k, 4);
            if (w >= 0x400000u && w < 0x1100000u) { k += 3; h = (h ^ 0xAAu) * 16777619u; continue; }   // an embedded vtable pointer (exe image vs this test's image): not compared
            h = (h ^ ((const uint8_t*)p)[k]) * 16777619u;
        }
        return h;   // from +4: the vptr differs (C++ vs exe vtable)
    }
    return 0;
}
static void LogSt(const char* s) { g_log.push_back(0x57000000u); if (s) for (; *s; ++s) g_log.push_back((uint8_t)*s); g_log.push_back(0); }
static void LogV3(const CVector* v) { if (v) LogF(0, { v->x, v->y, v->z }); else LogF(0, { 0.f, 0.f, 0.f }); }

static void* __cdecl H_TaskNew(size_t n) { void* p = std::calloc(1, n); g_allocs.push_back(p); g_allocSz.push_back(n); LogU(40, { (uint32_t)n }); return p; }
void* CTask::operator new(size_t n) { return H_TaskNew(n); }
void  CTask::operator delete(void*) {}

static void __fastcall H_Gpst(CRunningScript*, int, int h, void* task, int cmd) { LogU(41, { (uint32_t)h, TaskOrd(task), TaskHash(task), (uint32_t)cmd }); }
void CRunningScript::GivePedScriptedTask(int32 h, CTask* t, int32 cmd) { H_Gpst(this, 0, h, t, cmd); }
static void __fastcall H_AddTask(void* self, int, void* task) { LogU(42, { (uint32_t)((uintptr_t)self - 0xC178F0u), TaskOrd(task), TaskHash(task) }); }
void CTaskComplexSequence::AddTask(CTask* t) { H_AddTask(this, 0, t); }

// -- constructors (exe: thiscall = __fastcall(this, edx, stack args...); the stack args are popped by the callee like the originals)
static void* __fastcall H_cTrigLook(void* self, int, void* e, int time, int bone, float vx, float vy, float vz, int torso, float spd, int blend, int prio) {
    LogF(50, { (float)TaskOrd(self), (float)Tag(e), (float)time, (float)bone, vx, vy, vz, (float)(uint8_t)torso, spd, (float)blend, (float)prio }); return self; }
CTaskSimpleTriggerLookAt::CTaskSimpleTriggerLookAt(CEntity* e, int32 time, int32 bone, RwV3d v, bool torso, float spd, int32 blend, int32 prio) {
    LogF(50, { (float)TaskOrd(this), (float)Tag(e), (float)time, (float)bone, v.x, v.y, v.z, (float)(uint8_t)torso, spd, (float)blend, (float)prio }); }
static void* __fastcall H_cGun(void* self, int, void* e, const CVector* tp, const CVector* mt, int cmd, int burst, int dur) {
    LogF(51, { (float)TaskOrd(self), (float)Tag(e), (float)(int8_t)cmd, (float)(int16_t)burst, (float)dur }); LogV3(tp); LogV3(mt); return self; }
CTaskSimpleGunControl::CTaskSimpleGunControl(CEntity* e, CVector const& tp, CVector const& mt, eGunCommand cmd, int16 burst, int32 dur) {
    LogF(51, { (float)TaskOrd(this), (float)Tag(e), (float)(int8_t)cmd, (float)(int16_t)burst, (float)dur }); LogV3(&tp); LogV3(&mt);
    std::memset(reinterpret_cast<uint8_t*>(this) + 4, 0, sizeof(*this) - 4); }   // the recorder replaces the whole ctor: undo the member initialisers so the memory equals the exe's (zeroed) task
static void* __fastcall H_cThrow(void* self, int, void* e, const CVector* pos) { LogF(52, { (float)TaskOrd(self), (float)Tag(e) }); LogV3(pos); return self; }
CTaskSimpleThrowControl::CTaskSimpleThrowControl(CEntity* e, CVector const* pos) { LogF(52, { (float)TaskOrd(this), (float)Tag(e) }); LogV3(pos); }
static void* __fastcall H_cDestroyCar(void* self, int, void* veh, uint32_t a, uint32_t b, uint32_t c) { LogU(53, { TaskOrd(self), Tag(veh), a, b, c }); return self; }
CTaskComplexDestroyCar::CTaskComplexDestroyCar(CVehicle* veh, uint32 a, uint32 b, uint32 c) { LogU(53, { TaskOrd(this), Tag(veh), a, b, c }); }
static bool g_suppress;   // set while a derived recorder constructs its (recorded elsewhere / replaced as a whole) base
static CVehicle* Sup() { g_suppress = true; return nullptr; }
static void* __fastcall H_cDive(void* self, int, void* veh, int time, const CVector* dir, int flag) { LogF(54, { (float)TaskOrd(self), (float)Tag(veh), (float)time, (float)(uint8_t)flag }); LogV3(dir); return self; }
CTaskComplexEvasiveDiveAndGetUp::CTaskComplexEvasiveDiveAndGetUp(CVehicle* veh, int32 time, const CVector& dir, bool flag) {
    if (!g_suppress) { LogF(54, { (float)TaskOrd(this), (float)Tag(veh), (float)time, (float)(uint8_t)flag }); LogV3(&dir); } }
static void* __fastcall H_cShuffle(void* self, int, void* veh) { LogU(55, { TaskOrd(self), Tag(veh) }); return self; }
CTaskComplexShuffleSeats::CTaskComplexShuffleSeats(CVehicle* veh) { LogU(55, { TaskOrd(this), Tag(veh) }); }
static void* __fastcall H_cChat(void* self, int, const char* name, void* partner, int lead, float dist, int cnt, int a7, int a8, float px, float py, float pz) {
    LogF(56, { (float)TaskOrd(self), (float)Tag(partner), (float)(uint8_t)lead, dist, (float)cnt, (float)(uint8_t)a7, (float)(uint8_t)a8, px, py, pz }); LogSt(name); return self; }
static void BreakpointNop(const char*) {}
static void* __cdecl H_DmInstance() { static uint8_t b[64]; return b; }   // CDecisionMakerTypes::GetInstance (0x4684F0) would allocate through the exe CRT
CTaskComplexPartner::CTaskComplexPartner(const char*, CPed*, bool, float, bool, int8, CVector) {}   // the exe's CTaskComplexPartnerChat ctor is replaced as a whole (H_cChat), so the base ctor is not recorded
CTaskComplexPartnerChat::CTaskComplexPartnerChat(const char* name, CPed* partner, bool lead, float dist, int8 cnt, bool a7, bool a8, CVector p) : CTaskComplexPartner(name, partner, lead, dist, false, cnt, p) {
    LogF(56, { (float)TaskOrd(this), (float)Tag(partner), (float)(uint8_t)lead, dist, (float)(int)cnt, (float)(uint8_t)a7, (float)(uint8_t)a8, p.x, p.y, p.z }); LogSt(name); }
static void* __fastcall H_cLeave(void* self, int, int delay, int a, int b) { LogU(57, { TaskOrd(self), (uint32_t)delay, (uint8_t)a, (uint8_t)b }); return self; }
CTaskComplexLeaveAnyCar::CTaskComplexLeaveAnyCar(int32 delay, bool a, bool b) { LogU(57, { TaskOrd(this), (uint32_t)delay, (uint8_t)a, (uint8_t)b }); }
static void* __fastcall H_cThreat(void* self, int, int a, int b, int c) { LogU(58, { TaskOrd(self), (uint8_t)a, (uint8_t)b, (uint8_t)c }); return self; }
CTaskSimpleTogglePedThreatScanner::CTaskSimpleTogglePedThreatScanner(bool a, bool b, bool c) { LogU(58, { TaskOrd(this), (uint8_t)a, (uint8_t)b, (uint8_t)c }); }
static int __fastcall H_threatProc(void* self, int, void* ped) { LogU(59, { TaskOrd(self), Tag(ped) }); return 1; }
bool CTaskSimpleTogglePedThreatScanner::ProcessPed(CPed* ped) { LogU(59, { TaskOrd(this), Tag(ped) }); return true; }

// -- IK chain manager
static bool g_looking;
static void __fastcall H_IkLookAt(void* self, int, const char* purpose, void* ped, void* ent, int time, int bone, const CVector* off, int torso, float spd, int blend, int prio, int force) {
    LogSt(purpose); LogF(60, { (float)(uintptr_t)self, (float)Tag(ped), (float)Tag(ent), (float)time, (float)bone, (float)Tag(off), (float)(uint8_t)torso, spd, (float)blend, (float)prio, (float)(uint8_t)force }); }
void IKChainManager_c::LookAt(const char* purpose, CPed* ped, CEntity* e, int32 time, eBoneTag32 bone, CVector* off, bool torso, float spd, int32 blend, int32 prio, bool force) {
    LogSt(purpose); LogF(60, { (float)(uintptr_t)this, (float)Tag(ped), (float)Tag(e), (float)time, (float)bone, (float)Tag(off), (float)(uint8_t)torso, spd, (float)blend, (float)prio, (float)(uint8_t)force }); }
static int __fastcall H_IkIsLooking(void* self, int, void* ped) { LogU(61, { (uint32_t)(uintptr_t)self, Tag(ped) }); return g_looking; }
bool IKChainManager_c::IsLooking(CPed* ped) const { LogU(61, { (uint32_t)(uintptr_t)this, Tag(ped) }); return g_looking; }
static void __fastcall H_IkAbort(void* self, int, void* ped, uint32_t blend) { LogU(62, { (uint32_t)(uintptr_t)self, Tag(ped), blend }); }
void IKChainManager_c::AbortLookAt(CPed* ped, uint32 blend) { LogU(62, { (uint32_t)(uintptr_t)this, Tag(ped), blend }); }
IKChainManager_c& g_ikChainMan = *reinterpret_cast<IKChainManager_c*>(0xC15448);

// -- weapon info for 1640
static uint8_t g_wi[4][0x40];
static void* __cdecl H_GetWeaponInfo(int type, int skill) { LogU(63, { (uint32_t)type, (uint32_t)skill }); return g_wi[(type * 5 + skill) & 3]; }
CWeaponInfo* CWeaponInfo::GetWeaponInfo(eWeaponType type, eWeaponSkill skill) { LogU(63, { (uint32_t)type, (uint32_t)skill }); return reinterpret_cast<CWeaponInfo*>(g_wi[((int)type * 5 + (int)skill) & 3]); }

// -- scripted task record / script resources / decision makers
static int __cdecl H_GetStatus(void* ped, int opcode) { LogU(64, { Tag(ped), (uint32_t)opcode }); return (int)g_statusRet; }
eScriptedTaskStatus CPedScriptedTaskRecord::GetStatus(CPed* ped, int32 opcode) { LogU(64, { Tag(ped), (uint32_t)opcode }); return (eScriptedTaskStatus)g_statusRet; }
static bool g_rfrRet;
static int __fastcall H_RemoveFromResMgr(void*, int, int id, int type, void* script) { LogU(65, { (uint32_t)id, (uint32_t)type, script == g_S ? 7u : 9u }); return g_rfrRet; }
bool CScriptResourceManager::RemoveFromResourceManager(int32 id, eScriptResourceType type, CRunningScript* script) { LogU(65, { (uint32_t)id, (uint32_t)type, script == g_S ? 7u : 9u }); return g_rfrRet; }
static void __cdecl H_UnloadDm2(int idx) { LogU(66, { (uint32_t)idx }); }
void CDecisionMakerTypesFileLoader::UnloadDecisionMaker(eDecisionTypes idx) { LogU(66, { (uint32_t)idx }); }

// -- FX creation
static FxSystem_c* g_fxResult;
static bool g_haveMat;
static alignas(16) uint8_t g_rwMatBuf[0x40];
static void FakeMatFromVec(RwMatrix* out, const CVector* origin, const CVector* dir) { // deterministic stand-in for Fx_c::CreateMatFromVec (0x49E950)
    LogF(70, { origin->x, origin->y, origin->z, dir->x, dir->y, dir->z });
    float* f = reinterpret_cast<float*>(out);
    for (int i = 0; i < 12; ++i) f[i] = (i < 3 ? origin->x : i < 6 ? origin->y : i < 9 ? dir->x : dir->y) + (float)i;
}
static void __fastcall H_MatFromVec(void*, int, RwMatrix* out, const CVector* o, const CVector* d) { FakeMatFromVec(out, o, d); }
void Fx_c::CreateMatFromVec(RwMatrix* out, const CVector* o, const CVector* d) { FakeMatFromVec(out, o, d); }
static void* __fastcall H_CreateFxPos(void*, int, const char* name, const CVector* pos, void* mat, int flag) {
    LogSt(name); LogF(71, { pos->x, pos->y, pos->z, (float)(mat ? 1 : 0), (float)(uint8_t)flag }); return g_fxResult; }
FxSystem_c* FxManager_c::CreateFxSystem(const char* name, const CVector& pos, RwMatrix* mat, bool flag) {
    LogSt(name); LogF(71, { pos.x, pos.y, pos.z, (float)(mat ? 1 : 0), (float)(uint8_t)flag }); return g_fxResult; }
static void* __fastcall H_CreateFxMat(void*, int, const char* name, const RwMatrix* tr, void* mat, int flag) {
    LogSt(name); const float* f = reinterpret_cast<const float*>(tr); LogF(72, { f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], (float)(mat ? 1 : 0), (float)(uint8_t)flag }); return g_fxResult; }
FxSystem_c* FxManager_c::CreateFxSystem(const char* name, const RwMatrix& tr, RwMatrix* mat, bool flag) {
    LogSt(name); const float* f = reinterpret_cast<const float*>(&tr); LogF(72, { f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], (float)(mat ? 1 : 0), (float)(uint8_t)flag }); return g_fxResult; }
static uint32_t __cdecl H_AddScriptFx(void* fx) { LogU(73, { Tag(fx) }); return 0x30000u + Tag(fx); }
uint32 CTheScripts::AddScriptEffectSystem(FxSystem_c* fx) { LogU(73, { Tag(fx) }); return 0x30000u + Tag(fx); }
static void __fastcall H_CleanupAdd(void*, int, int handle, int type) { LogU(74, { (uint32_t)handle, (uint32_t)type }); }
void CMissionCleanup::AddEntityToList(int32 handle, MissionCleanUpEntityType type) { LogU(74, { (uint32_t)handle, (uint32_t)type }); }
static RwMatrix* __fastcall H_GetModelMat(void* self, int) { LogU(75, { Tag(self) }); return g_haveMat ? reinterpret_cast<RwMatrix*>(g_rwMatBuf) : nullptr; }
RwMatrix* CEntity::GetModellingMatrix() { LogU(75, { Tag(this) }); return g_haveMat ? reinterpret_cast<RwMatrix*>(g_rwMatBuf) : nullptr; }
static void __fastcall H_CreateRw(void* self, int) { LogU(76, { Tag(self) }); *reinterpret_cast<void**>((uint8_t*)self + 0x18) = g_rwMatBuf; }
FxManager_c& g_fxMan = *reinterpret_cast<FxManager_c*>(0xA9AE80);
Fx_c&        g_fx    = *reinterpret_cast<Fx_c*>(0xA9AE00);

// -- model infos (1652)
static uint8_t g_fakeMi[8][0x40];
static int g_miType[8];
static void* g_miVtbl[16];
static int __fastcall H_MiType(void* self, int) { return g_miType[((uint8_t*)self - &g_fakeMi[0][0]) / 0x40]; }
static void __fastcall H_SetPlate(void* self, int, char* text) { LogU(77, { (uint32_t)(((uint8_t*)self - &g_fakeMi[0][0]) / 0x40) }); char b[9]; std::memcpy(b, text, 8); b[8] = 0; LogSt(b); }
void CVehicleModelInfo::SetCustomCarPlateText(char* text) { H_SetPlate(this, 0, text); }

// ---------------------------------------------------------------------------------------------------------------------------------
// S6-D review (g17 part): further recorders. Tasks with inline (header) constructors run their real code on the port side and are compared by
// memory hash against the exe's real constructors; all other task constructors are replaced by recorders on both sides.
static uint32_t RefTag(const void* ref) {   // a reference slot inside a recorded task: (task ordinal << 16) | offset; otherwise the offset from the fake vehicle
    for (size_t i = 0; i < g_allocs.size(); ++i) if ((const uint8_t*)ref >= (const uint8_t*)g_allocs[i] && (const uint8_t*)ref < (const uint8_t*)g_allocs[i] + g_allocSz[i]) return (uint32_t)((i + 1) << 16) | (uint32_t)((const uint8_t*)ref - (const uint8_t*)g_allocs[i]);
    return (uint32_t)((const uint8_t*)ref - (const uint8_t*)g_veh);
}
static void __fastcall H_RegRef(void* self, int, void* ref) { LogU(80, { Tag(self), RefTag(ref) }); }
void CEntity::RegisterReference(CEntity** ref) { LogU(80, { Tag(this), RefTag(ref) }); }
static void __fastcall H_CleanRef(void* self, int, void* ref) { LogU(81, { Tag(self), RefTag(ref) }); }
void CEntity::CleanUpOldReference(CEntity** ref) { LogU(81, { Tag(this), RefTag(ref) }); }

static void* __fastcall H_cDiveAtt(void* self, int, int t) { LogU(210, { TaskOrd(self), (uint32_t)t }); return self; }
CTaskComplexDiveFromAttachedEntityAndGetUp::CTaskComplexDiveFromAttachedEntityAndGetUp(int32 t) : CTaskComplexEvasiveDiveAndGetUp(Sup(), 0, CVector{}, false) { g_suppress = false; LogU(210, { TaskOrd(this), (uint32_t)t }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cSit(void* self, int, int d, int a, int b) { LogU(211, { TaskOrd(self), (uint32_t)d, (uint8_t)a, (uint8_t)b }); return self; }
CTaskComplexSitDownThenIdleThenStandUp::CTaskComplexSitDownThenIdleThenStandUp(int32 d, bool a, bool b) { LogU(211, { TaskOrd(this), (uint32_t)d, (uint8_t)a, (uint8_t)b }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cTurn(void* self, int, const CVector* c, float a, float b) { LogF(212, { (float)TaskOrd(self), a, b }); LogV3(c); return self; }
CTaskComplexTurnToFaceEntityOrCoord::CTaskComplexTurnToFaceEntityOrCoord(const CVector& c, float a, float b) { LogF(212, { (float)TaskOrd(this), a, b }); LogV3(&c); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cDrivePR(void* self, int, void* veh, const void* route, float spd, uint32_t mode, int model, float rad, int style) { LogF(213, { (float)TaskOrd(self), (float)Tag(veh), (float)(uintptr_t)route, spd, (float)mode, (float)model, rad, (float)style }); return self; }
CTaskComplexDrivePointRoute::CTaskComplexDrivePointRoute(CVehicle* veh, CPointRoute const& route, float spd, uint32 mode, eModelID model, float rad, eCarDrivingStyle style) {
    LogF(213, { (float)TaskOrd(this), (float)Tag(veh), (float)(uintptr_t)&route, spd, (float)mode, (float)(int)model, rad, (float)(int)style }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cGoAim(void* self, int, int ms, const CVector* mp, void* aimE, float ax, float ay, float az, float r, float s) { LogF(214, { (float)TaskOrd(self), (float)ms, (float)Tag(aimE), ax, ay, az, r, s }); LogV3(mp); return self; }
CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(eMoveState ms, const CVector& mp, CEntity* aimE, CVector aim, const float r, const float s) { LogF(214, { (float)TaskOrd(this), (float)(int)ms, (float)Tag(aimE), aim.x, aim.y, aim.z, r, s }); LogV3(&mp); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cTempAct(void* self, int, void* veh, int act, uint32_t t) { LogU(215, { TaskOrd(self), Tag(veh), (uint32_t)act, t }); return self; }
CTaskSimpleCarDrive::CTaskSimpleCarDrive(CVehicle*, CTaskUtilityLineUpPedWithCar*, bool) {}   // base ctor of the recorded ctor below
CTaskSimpleCarSetTempAction::CTaskSimpleCarSetTempAction(CVehicle* veh, eAutoPilotTempAction act, uint32 t) : CTaskSimpleCarDrive(veh, nullptr, false) { LogU(215, { TaskOrd(this), Tag(veh), (uint32_t)act, t }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cCarMission(void* self, int, void* veh, void* tgt, int mission, int style, float spd) { LogF(216, { (float)TaskOrd(self), (float)Tag(veh), (float)Tag(tgt), (float)mission, (float)style, spd }); return self; }
CTaskComplexCarDrive::CTaskComplexCarDrive(CVehicle*, bool) {}   // base ctor of the recorded ctor below
CTaskComplexCarDriveMission::CTaskComplexCarDriveMission(CVehicle* veh, CEntity* tgt, eCarMission mission, eCarDrivingStyle style, float spd) : CTaskComplexCarDrive(veh, true) { LogF(216, { (float)TaskOrd(this), (float)Tag(veh), (float)Tag(tgt), (float)(int)mission, (float)(int)style, spd }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cRunAnim(void* self, int, int grp, int anim, float blend, int hold) { LogF(217, { (float)TaskOrd(self), (float)grp, (float)anim, blend, (float)(uint8_t)hold }); return self; }
CTaskSimpleAnim::CTaskSimpleAnim(bool) {}
CTaskSimpleRunAnim::CTaskSimpleRunAnim(AssocGroupId grp, AnimationId anim, float blend, bool hold) : CTaskSimpleAnim(hold) { LogF(217, { (float)TaskOrd(this), (float)(int)grp, (float)(int)anim, blend, (float)(uint8_t)hold }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }
static void* __fastcall H_cArrest(void* self, int, void* tgt) { LogU(218, { TaskOrd(self), Tag(tgt) }); return self; }
CTaskComplexArrestPed::CTaskComplexArrestPed(CPed* tgt) { LogU(218, { TaskOrd(this), Tag(tgt) }); std::memset((uint8_t*)this + 4, 0, sizeof(*this) - 4); }

// -- create commands
static uint32_t __cdecl H_AddSL(float sx, float sy, float sz, void* ent, float tx, float ty, float tz, float r1, float r2) { LogF(220, { sx, sy, sz, (float)Tag(ent), tx, ty, tz, r1, r2 }); return 0x20000u + (FB(sx) & 0xFF); }
uint32 CTheScripts::AddScriptSearchLight(CVector s, CEntity* e, CVector t, float r1, float r2) { return H_AddSL(s.x, s.y, s.z, e, t.x, t.y, t.z, r1, r2); }
static uint32_t __cdecl H_AddCp(float ax, float ay, float az, float bx, float by, float bz, float r, int type) { LogF(221, { ax, ay, az, bx, by, bz, r, (float)type }); return 0x30000u + (FB(ax) & 0xFF); }
uint32 CTheScripts::AddScriptCheckpoint(CVector a, CVector b, float r, eCheckpointType type) { return H_AddCp(a.x, a.y, a.z, b.x, b.y, b.z, r, (int)type); }
static int __cdecl H_SetBlip(int type, int handle, int color, int display, const char*) { LogU(222, { (uint32_t)type, (uint32_t)handle, (uint32_t)color, (uint32_t)display }); return 0x40000 + handle; }
tBlipHandle CRadar::SetEntityBlip(eBlipType type, int32 handle, uint32 color, eBlipDisplay display) { return std::bit_cast<tBlipHandle>(H_SetBlip((int)type, handle, (int)color, (int)display, nullptr)); }
static void __cdecl H_BlipScale(int blip, int size) { LogU(223, { (uint32_t)blip, (uint32_t)size }); }
void CRadar::ChangeBlipScale(tBlipHandle blip, int32 size) { H_BlipScale(std::bit_cast<int>(blip), size); }
static void __cdecl H_GetGrpDM(int idx, char* out) { LogU(224, { (uint32_t)idx }); std::snprintf(out, 16, "dm%d", idx); }
void CDecisionMakerTypesFileLoader::GetGrpDMName(int32 idx, char* out) { H_GetGrpDM(idx, out); }
static int __cdecl H_LoadDM(const char* name, int type, int mission) { LogU(225, { (uint32_t)(uint8_t)name[2], (uint32_t)type, (uint32_t)(uint8_t)mission }); return (int)name[2] & 15; }
int32 CDecisionMakerTypesFileLoader::LoadDecisionMaker(const char* name, eDecisionTypes type, bool mission) { return H_LoadDM(name, (int)type, mission); }
static int __cdecl H_NewUnique(int idx, int type) { LogU(226, { (uint32_t)idx, (uint32_t)type }); return idx | 0x50000; }
int32 CTheScripts::GetNewUniqueScriptThingIndex(int32 idx, eScriptThingType type) { return H_NewUnique(idx, (int)type); }

// -- script members used by the handlers (RunningScript.cpp is not part of this test): the exe's own versions operate on the same fake script
std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers = {};
int32 CRunningScript::CollectNextParameterWithoutIncreasingPC() { return oracle::Fn<int __fastcall(CRunningScript*, int)>(0x464250)(this, 0); }
void  CRunningScript::ReadTextLabelFromScript(char* buf, uint8 n) { oracle::Fn<void __fastcall(CRunningScript*, int, char*, int)>(0x463D50)(this, 0, buf, n); }

// -- RNG: the exe's rand (0x821B1E) runs on a fake _tiddata, the port on the game LCG
static uint32_t g_fakePtd[0x100];
static void* __cdecl HostGetPtd() { return g_fakePtd; }
static void SeedBoth(uint32_t seed) { notsa::GameSRand(seed); g_fakePtd[5] = seed; }
static uint32_t g_caseSeed;
static void PreRun() { g_allocs.clear(); g_allocSz.clear(); SeedBoth(g_caseSeed); }

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
    constexpr unsigned groupFn = 0x496E00; // ProcessCommands1700To1799
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
        PreRun();
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

    Reset(); g_log.clear(); if (g_trace) std::printf("   exe\n");
    const bool exeRet = oracle::Fn<unsigned char __fastcall(CRunningScript*, int, int)>(groupFn)(&S, 0, cmdId) != 0;
    const Outcome a = Collect(exeRet);

    Restore(pre, c);
    Reset(); g_log.clear(); if (g_trace) std::printf("   port\n");
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
        Ctx c; c.rng = &rng; c.notFlag = rng.below(4) == 0; c.cleanup = rng.below(2) == 0; g_caseSeed = rng.u32();
        gen(c);
        if (g_trace) { std::printf("  case %d:", i); for (auto b : c.opcode) std::printf(" %02x", b); std::printf("\n"); }
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
static int32_t Flag(Rng& r) { return r.below(3) == 0 ? 0 : r.below(2) ? 1 : (int32_t)r.u32(); }
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
    *reinterpret_cast<void**>(g_ped) = g_fakeVtbl;
    g_fakeVtbl[0x1C / 4] = (void*)&H_CreateRw;           // CEntity::CreateRwObject -- slot 7
    g_fakeVtbl[0x6C / 4] = (void*)&H_OpenDoor;           // CVehicle::OpenDoor(ped, node, door, ratio, sound) -- slot 27
}

// ---------------------------------------------------------------------------------------------------------------------------------
static float RandAngle(Rng& r) { return r.below(10) == 0 ? r.F(50000.f) : r.below(4) == 0 ? r.F(400.f, false) : r.F(2000.f, true); }

#include "../../source/game_sa/Tasks/TaskTypes/SeekEntity/PosCalculators/EntitySeekPosCalculatorRadiusAngleOffset.cpp"   // its real ctor is compared (inside the task memory) with the exe's

static void TestAll() {
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CAutomobile) } }; };
    auto PedSetup = [&](Ctx& c) { c.watch = { { g_ped, sizeof(CPed) } }; };
    auto Hnd = [&](Rng& r, int ref) { return r.below(5) == 0 ? -1 : ref; };
    auto Time = [&](Rng& r) { return r.below(4) == 0 ? -1 : r.below(5) == 0 ? -2 : r.below(2) ? (int)r.below(100000) : (int)r.u32(); };
    auto Q = [&](Rng& r, int ref) { return r.below(4) == 0 ? -1 : ref; };   // optional entity
    g_miVtbl[4] = (void*)&H_MiType;

    Test<COMMAND_TASK_DIVE_FROM_ATTACHMENT_AND_GET_UP, &TaskDiveFromAttachmentAndGetUp>("1701 TASK_DIVE_FROM_ATTACHMENT_AND_GET_UP", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1701).I(Hnd(r, g_pedRef)).I(Time(r)).b; });
    Test<COMMAND_TASK_GOTO_CHAR_OFFSET, &TaskGotoCharOffset>("1704 TASK_GOTO_CHAR_OFFSET", [&](Ctx& c) {
        auto& r = *c.rng; *(int32_t*)0x8D237C = r.below(8); c.watch = { { (void*)0x8D237C, 4 } };
        c.opcode = Op(1704).I(Hnd(r, g_pedRef)).I(Q(r, g_pedRef)).I(r.below(3) == 0 ? -(int)r.below(5) : (int)r.u32() % 100000).Fl(r.F(50)).Fl(r.F(720)).b; });
    Test<COMMAND_TASK_LOOK_AT_COORD, &TaskLookAtCoord>("1705 TASK_LOOK_AT_COORD", [&](Ctx& c) {
        auto& r = *c.rng; *(int32_t*)0x8D2E98 = r.below(64); PedSetup(c);
        c.opcode = Op(1705).I(r.below(3) == 0 ? -1 : g_pedRef).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).I(Time(r)).b; });
    Test<COMMAND_TASK_SIT_DOWN, &TaskSitDown>("1712 TASK_SIT_DOWN", [&](Ctx& c) { auto& r = *c.rng; c.opcode = Op(1712).I(Hnd(r, g_pedRef)).I(Time(r)).b; });
    Test<COMMAND_TASK_TURN_CHAR_TO_FACE_COORD, &TaskTurnCharToFaceCoord>("1722 TASK_TURN_CHAR_TO_FACE_COORD", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1722).I(Hnd(r, g_pedRef)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).b; });
    Test<COMMAND_TASK_DRIVE_POINT_ROUTE, &TaskDrivePointRoute>("1723 TASK_DRIVE_POINT_ROUTE", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1723).I(Hnd(r, g_pedRef)).I(Q(r, g_vehRef)).Fl(r.F(50)).b; });
    Test<COMMAND_TASK_GO_TO_COORD_WHILE_AIMING, &TaskGoToCoordWhileAiming>("1730 TASK_GO_TO_COORD_WHILE_AIMING", [&](Ctx& c) {
        auto& r = *c.rng;
        c.opcode = Op(1730).I(Hnd(r, g_pedRef)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).I((int)r.u32() % 10).Fl(r.F(10)).Fl(r.F(10)).I(Q(r, g_pedRef)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).b; });
    Test<COMMAND_TASK_CAR_TEMP_ACTION, &TaskCarTempAction>("1735 TASK_CAR_TEMP_ACTION", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1735).I(Hnd(r, g_pedRef)).I(Q(r, g_vehRef)).I((int)r.u32() % 20).I((int)r.u32() % 100000).b; });
    Test<COMMAND_TASK_CAR_MISSION, &TaskCarMission>("1761 TASK_CAR_MISSION", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1761).I(Hnd(r, g_pedRef)).I(r.below(3) == 0 ? -1 : g_vehRef).I(r.below(3) == 0 ? -1 : g_vehRef).I((int)r.u32() % 70).Fl(r.F(100)).I((int)r.u32() % 10).b; });
    Test<COMMAND_TASK_GO_TO_OBJECT, &TaskGoToObject>("1762 TASK_GO_TO_OBJECT", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1762).I(Hnd(r, g_pedRef)).I(Q(r, g_objRef)).I((int)r.u32() % 100000).Fl(r.F(50)).b; });
    Test<COMMAND_TASK_WEAPON_ROLL, &TaskWeaponRoll>("1763 TASK_WEAPON_ROLL", [&](Ctx& c) { auto& r = *c.rng; c.opcode = Op(1763).I(Hnd(r, g_pedRef)).I(Flag(r)).b; });
    Test<COMMAND_TASK_CHAR_ARREST_CHAR, &TaskCharArrestChar>("1764 TASK_CHAR_ARREST_CHAR", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1764).I(Hnd(r, g_pedRef)).I(Q(r, g_pedRef)).b; });

    // ---- create commands: the callees are recorded (argument order into AddScriptSearchLight / AddScriptCheckpoint / SetEntityBlip / LoadDecisionMaker)
    auto Dest = [&](Ctx& c) { *reinterpret_cast<int32_t*>(&ScriptSpaceRef()[0]) = c.rng->below(3) == 0 ? -1 : (int32_t)((uint32_t)c.rng->below(8) | ((uint32_t)c.rng->below(4) << 16)); };
    Test<COMMAND_CREATE_SEARCHLIGHT, &CreateSearchlight>("1713 CREATE_SEARCHLIGHT", [&](Ctx& c) {
        auto& r = *c.rng; Dest(c);
        c.opcode = Op(1713).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(10)).Fl(r.F(10)).GV(0).b; });
    Test<COMMAND_CREATE_SEARCHLIGHT_ON_VEHICLE, &CreateSearchlightOnVehicle>("1729 CREATE_SEARCHLIGHT_ON_VEHICLE", [&](Ctx& c) {
        auto& r = *c.rng; Dest(c); RandomVehicle(r); VehWatch(c);
        c.opcode = Op(1729).I(Q(r, g_vehRef)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(10)).Fl(r.F(10)).GV(0).b; });
    Test<COMMAND_CREATE_CHECKPOINT, &CreateCheckpoint>("1749 CREATE_CHECKPOINT", [&](Ctx& c) {
        auto& r = *c.rng; Dest(c);
        c.opcode = Op(1749).I((int)r.u32() % 12).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(10)).GV(0).b; });
    Test<COMMAND_ADD_BLIP_FOR_SEARCHLIGHT, &AddBlipForSearchlight>("1732 ADD_BLIP_FOR_SEARCHLIGHT", [&](Ctx& c) {
        auto& r = *c.rng; Dest(c); c.opcode = Op(1732).I(r.below(8) == 0 ? -1 : (int)((uint32_t)r.below(8) | ((uint32_t)r.below(4) << 16))).GV(0).b; });
    Test<COMMAND_LOAD_GROUP_DECISION_MAKER, &LoadGroupDecisionMaker>("1710 LOAD_GROUP_DECISION_MAKER", [&](Ctx& c) {
        auto& r = *c.rng; Dest(c); c.opcode = Op(1710).I(r.below(12)).GV(0).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-t")) g_trace = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g17_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
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

    oracle::Patch(0x61A5A0, (void*)&H_TaskNew);
    oracle::Patch(0x465C20, (void*)&H_Gpst);
    oracle::Patch(0x632D10, (void*)&H_AddTask);
    oracle::Patch(0x634440, (void*)&H_cTrigLook);
    oracle::Patch(0x61F3F0, (void*)&H_cGun);
    oracle::Patch(0x61F8B0, (void*)&H_cThrow);
    oracle::Patch(0x621C00, (void*)&H_cDestroyCar);
    oracle::Patch(0x6536B0, (void*)&H_cDive);
    oracle::Patch(0x63D130, (void*)&H_cShuffle);
    oracle::Patch(0x684290, (void*)&H_cChat);
    oracle::Patch(0x421150, (void*)&H_cLeave);
    oracle::Patch(0x492D10, (void*)&H_cThreat);
    oracle::Patch(0x6337A0, (void*)&H_threatProc);
    oracle::Patch(0x618970, (void*)&H_IkLookAt);
    oracle::Patch(0x6181A0, (void*)&H_IkIsLooking);
    oracle::Patch(0x618280, (void*)&H_IkAbort);
    oracle::Patch(0x743C60, (void*)&H_GetWeaponInfo);
    oracle::Patch(0x608710, (void*)&H_GetStatus);
    oracle::Patch(0x470510, (void*)&H_RemoveFromResMgr);
    oracle::Patch(0x607A70, (void*)&H_UnloadDm2);
    oracle::Patch(0x49E950, (void*)&H_MatFromVec);
    oracle::Patch(0x4A9BE0, (void*)&H_CreateFxPos);
    oracle::Patch(0x4A9BB0, (void*)&H_CreateFxMat);
    oracle::Patch(0x492F90, (void*)&H_AddScriptFx);
    oracle::Patch(0x4637E0, (void*)&H_CleanupAdd);
    oracle::Patch(0x46A2D0, (void*)&H_GetModelMat);
    oracle::Patch(0x4C8980, (void*)&H_SetPlate);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    oracle::Patch(0x4684F0, (void*)&H_DmInstance);

    oracle::Patch(0x571B70, (void*)&H_RegRef);
    oracle::Patch(0x571A00, (void*)&H_CleanRef);
    oracle::Patch(0x492E20, (void*)&H_cDiveAtt);
    oracle::Patch(0x631460, (void*)&H_cSit);
    oracle::Patch(0x66B910, (void*)&H_cTurn);
    oracle::Patch(0x6433E0, (void*)&H_cDrivePR);
    oracle::Patch(0x668790, (void*)&H_cGoAim);
    oracle::Patch(0x63D6F0, (void*)&H_cTempAct);
    oracle::Patch(0x63CC30, (void*)&H_cCarMission);
    oracle::Patch(0x61A8B0, (void*)&H_cRunAnim);
    oracle::Patch(0x68B990, (void*)&H_cArrest);
    oracle::Patch(0x493000, (void*)&H_AddSL);
    oracle::Patch(0x4935A0, (void*)&H_AddCp);
    oracle::Patch(0x5839A0, (void*)&H_SetBlip);
    oracle::Patch(0x583CC0, (void*)&H_BlipScale);
    oracle::Patch(0x600880, (void*)&H_GetGrpDM);
    oracle::Patch(0x607D30, (void*)&H_LoadDM);
    oracle::Patch(0x483720, (void*)&H_NewUnique);
    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g17_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
