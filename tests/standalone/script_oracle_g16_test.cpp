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
        uint32_t h = 2166136261u; for (size_t k = 4; k < g_allocSz[i]; ++k) h = (h ^ ((const uint8_t*)p)[k]) * 16777619u; return h;   // from +4: the vptr differs (C++ vs exe vtable)
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
static void* __fastcall H_cDive(void* self, int, void* veh, int time, const CVector* dir, int flag) { LogF(54, { (float)TaskOrd(self), (float)Tag(veh), (float)time, (float)(uint8_t)flag }); LogV3(dir); return self; }
CTaskComplexEvasiveDiveAndGetUp::CTaskComplexEvasiveDiveAndGetUp(CVehicle* veh, int32 time, const CVector& dir, bool flag) {
    LogF(54, { (float)TaskOrd(this), (float)Tag(veh), (float)time, (float)(uint8_t)flag }); LogV3(&dir); }
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

    // =========================================================================================================================
    // S6-D review: handlers whose callees are recorded on both sides (task constructors, IK chain manager, FX manager, resources)
    auto PedSetup = [&](Ctx& c) {
        auto& r = *c.rng;
        RandomFill((uint8_t*)g_ped + 0x5A0, 0x720 - 0x5A0, r);
        *reinterpret_cast<int8_t*>((uint8_t*)g_ped + 0x718) = (int8_t)r.below(13);   // valid weapon slots only (m_aWeapons is a checked std::array)
        RandomFill(g_wi, sizeof(g_wi), r);
        c.watch = { { g_ped, sizeof(CPed) } };
    };
    auto Hnd = [&](Rng& r, int ref) { return r.below(5) == 0 ? -1 : r.below(12) == 0 ? (int)r.u32() : ref; };   // pool handle, sometimes -1 / garbage (garbage only where the handler never dereferences it)
    auto Time = [&](Rng& r) { return r.below(4) == 0 ? -1 : r.below(5) == 0 ? -2 : r.below(2) ? (int)r.below(100000) : (int)r.u32(); };
    auto Zf = [&](Rng& r) { return r.below(3) == 0 ? -100.f - r.f01() * 5 : r.below(10) == 0 ? -100.f : r.F(50); };

    Test<COMMAND_GET_SEQUENCE_PROGRESS, &GetSequenceProgress>("1606 GET_SEQUENCE_PROGRESS", [&](Ctx& c) {
        auto& r = *c.rng; static uint8_t task[0x40];
        RandomFill(g_fakeIntel, sizeof(g_fakeIntel), r); RandomFill(task, sizeof(task), r);
        *reinterpret_cast<void**>(g_fakeIntel + 0x10) = task;
        g_statusRet = r.below(3) == 0 ? 0u : (uint32_t)(int)(r.below(4) - 1);
        c.watch = { { g_fakeIntel, sizeof(g_fakeIntel) } };
        c.opcode = Op(1606).I(g_pedRef).GV(0).b; });
    Test<COMMAND_CLEAR_LOOK_AT, &ClearLookAt>("1607 CLEAR_LOOK_AT", [&](Ctx& c) {
        auto& r = *c.rng; PedSetup(c); g_looking = r.below(2); *(int32_t*)0x8D2E98 = r.below(64);
        c.opcode = Op(1607).I(r.below(3) == 0 ? -1 : g_pedRef).b; });
    Test<COMMAND_TASK_LOOK_AT_OBJECT, &TaskLookAtObject>("1621 TASK_LOOK_AT_OBJECT", [&](Ctx& c) {
        auto& r = *c.rng; PedSetup(c); *(int32_t*)0x8D2E98 = r.below(64);
        c.opcode = Op(1621).I(r.below(3) == 0 ? -1 : g_pedRef).I(r.below(4) == 0 ? -1 : g_objRef).I(Time(r)).b; });
    Test<COMMAND_REMOVE_DECISION_MAKER, &RemoveDecisionMaker>("1628 REMOVE_DECISION_MAKER", [&](Ctx& c) {
        auto& r = *c.rng;
        auto* act = reinterpret_cast<uint8_t*>(0xC0B01C); auto* ref = reinterpret_cast<uint16_t*>(0xC0AFF4);
        for (int i = 0; i < 24; ++i) { act[i] = (uint8_t)r.below(2); } for (int i = 0; i < 24; ++i) { ref[i] = (uint16_t)r.below(4); }
        c.watch = { { act, 24 }, { ref, 48 } };
        g_rfrRet = r.below(2);
        const int idx = r.below(22); const int id = r.below(8) == 0 ? -1 : (int)((uint32_t)idx | ((uint32_t)r.below(4) << 16));
        c.opcode = Op(1628).I(id).b; });
    Test<COMMAND_BREAKPOINT, &BreakpointNop>("1633 BREAKPOINT (nop)", [&](Ctx& c) { c.opcode = Op(1633).Str8("abc").b; });
    Test<COMMAND_TASK_AIM_GUN_AT_COORD, &TaskAimGunAtCoord>("1639 TASK_AIM_GUN_AT_COORD", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1639).I(Hnd(r, g_pedRef)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).I(Time(r)).b; });
    Test<COMMAND_TASK_SHOOT_AT_COORD, &TaskShootAtCoord>("1640 TASK_SHOOT_AT_COORD", [&](Ctx& c) {
        auto& r = *c.rng; PedSetup(c);
        for (auto& w : g_wi) *reinterpret_cast<uint32_t*>(w + 0x18) = r.below(2) ? (1u << 8) | r.u32() : r.u32() & ~(1u << 8);
        c.opcode = Op(1640).I(r.below(5) == 0 ? -1 : g_pedRef).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).I(Time(r)).b; });

    g_looking = false;
    auto FxRand = [&](Ctx& c) {
        auto& r = *c.rng;
        g_fxResult = r.below(4) ? reinterpret_cast<FxSystem_c*>(g_fakeFx[r.below(16)]) : nullptr; g_haveMat = r.below(4) != 0;
        *reinterpret_cast<int32_t*>(&ScriptSpaceRef()[0]) = r.below(3) == 0 ? -1 : (int32_t)((uint32_t)r.below(8) | ((uint32_t)r.below(4) << 16));   // value the dead GetActualScriptThingIndex peek reads (the result variable)
        const char* names[] = { "", "a", "fx_one", "BLOOD", "smoke_1" };
        return std::string(names[r.below(5)]);
    };
    auto FxEntityRw = [&](Rng& r, void* e) { *reinterpret_cast<void**>((uint8_t*)e + 0x18) = r.below(2) ? nullptr : (void*)0x1234; };
    Test<COMMAND_CREATE_FX_SYSTEM, &CreateFxSystem>("1611 CREATE_FX_SYSTEM", [&](Ctx& c) {
        auto& r = *c.rng; auto nm = FxRand(c);
        c.opcode = Op(1611).Str8(nm.c_str()).Fl(r.F(50)).Fl(r.F(50)).Fl(Zf(r)).I(r.below(3)).GV(0).b; });
    auto FxOn = [&](Ctx& c, int cmd, int ref, bool dir, void* ent) {
        auto& r = *c.rng; auto nm = FxRand(c); *reinterpret_cast<void**>(ent) = g_fakeVtbl; FxEntityRw(r, ent);
        c.watch = { { ent, cmd == 1645 || cmd == 1646 ? sizeof(CObject) : cmd == 1643 || cmd == 1644 ? sizeof(CAutomobile) : sizeof(CPed) } };
        Op o(cmd); o.Str8(nm.c_str()).I(ref).Fl(r.F(50)).Fl(r.F(50)).Fl(Zf(r));
        if (dir) o.Fl(r.F(5)).Fl(r.F(5)).Fl(r.F(5));
        c.opcode = o.I(r.below(3)).GV(0).b;
    };
    Test<COMMAND_CREATE_FX_SYSTEM_ON_CHAR, &CreateFxSystemOnChar>("1641 CREATE_FX_SYSTEM_ON_CHAR", [&](Ctx& c) { FxOn(c, 1641, g_pedRef, false, g_ped); });
    Test<COMMAND_CREATE_FX_SYSTEM_ON_CHAR_WITH_DIRECTION, &CreateFxSystemOnCharWithDirection>("1642 CREATE_FX_SYSTEM_ON_CHAR_WITH_DIRECTION", [&](Ctx& c) { FxOn(c, 1642, g_pedRef, true, g_ped); });
    Test<COMMAND_CREATE_FX_SYSTEM_ON_CAR, &CreateFxSystemOnCar>("1643 CREATE_FX_SYSTEM_ON_CAR", [&](Ctx& c) { RandomVehicle(*c.rng); FxOn(c, 1643, g_vehRef, false, g_veh); });
    Test<COMMAND_CREATE_FX_SYSTEM_ON_CAR_WITH_DIRECTION, &CreateFxSystemOnCarWithDirection>("1644 CREATE_FX_SYSTEM_ON_CAR_WITH_DIRECTION", [&](Ctx& c) { RandomVehicle(*c.rng); FxOn(c, 1644, g_vehRef, true, g_veh); });
    Test<COMMAND_CREATE_FX_SYSTEM_ON_OBJECT, &CreateFxSystemOnObject>("1645 CREATE_FX_SYSTEM_ON_OBJECT", [&](Ctx& c) { RandomObject(*c.rng); FxOn(c, 1645, g_objRef, false, g_obj); });
    Test<COMMAND_CREATE_FX_SYSTEM_ON_OBJECT_WITH_DIRECTION, &CreateFxSystemOnObjectWithDirection>("1646 CREATE_FX_SYSTEM_ON_OBJECT_WITH_DIRECTION", [&](Ctx& c) { RandomObject(*c.rng); FxOn(c, 1646, g_objRef, true, g_obj); });

    Test<COMMAND_TASK_DESTROY_CAR, &TaskDestroyCar>("1650 TASK_DESTROY_CAR", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1650).I(Hnd(r, g_pedRef)).I(r.below(4) == 0 ? -1 : g_vehRef).b; });
    Test<COMMAND_TASK_DIVE_AND_GET_UP, &TaskDiveAndGetUp>("1651 TASK_DIVE_AND_GET_UP", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1651).I(Hnd(r, g_pedRef)).Fl(r.F(10)).Fl(r.F(10)).I(Time(r)).b; });
    Test<COMMAND_CUSTOM_PLATE_FOR_NEXT_CAR, &CustomPlateForNextCar>("1652 CUSTOM_PLATE_FOR_NEXT_CAR", [&](Ctx& c) {
        auto& r = *c.rng;
        g_miVtbl[4] = (void*)&H_MiType;
        for (int i = 0; i < 8; ++i) {
            RandomFill(g_fakeMi[i], 0x40, r); *reinterpret_cast<void**>(g_fakeMi[i]) = g_miVtbl;
            if (r.below(3) == 0) *reinterpret_cast<void**>(g_fakeMi[i] + 0x24) = nullptr;
            g_miType[i] = r.below(2) ? 6 : (int)r.below(10);
            *reinterpret_cast<void**>(0xA9B0C8 + 4 * i) = r.below(5) == 0 ? nullptr : g_fakeMi[i];
        }
        c.watch = { { (void*)0xA9B0C8, 32 }, { g_fakeMi, sizeof(g_fakeMi) } };
        char plate[9] = {}; const char alphabet[] = "ABZ09_ "; const int len = r.below(9);
        for (int i = 0; i < len; ++i) plate[i] = alphabet[r.below(7)];
        c.opcode = Op(1652).I(r.below(8)).Str8(plate).b; });
    Test<COMMAND_TASK_SHUFFLE_TO_NEXT_CAR_SEAT, &TaskShuffleToNextCarSeat>("1654 TASK_SHUFFLE_TO_NEXT_CAR_SEAT", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1654).I(Hnd(r, g_pedRef)).I(r.below(4) == 0 ? -1 : g_vehRef).b; });
    Test<COMMAND_TASK_CHAT_WITH_CHAR, &TaskChatWithChar>("1655 TASK_CHAT_WITH_CHAR", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1655).I(Hnd(r, g_pedRef)).I(r.below(4) == 0 ? -1 : g_pedRef).I(r.below(3) - 1).I(r.below(3) - 1).b; });
    Test<COMMAND_TASK_TOGGLE_PED_THREAT_SCANNER, &TaskTogglePedThreatScanner>("1672 TASK_TOGGLE_PED_THREAT_SCANNER", [&](Ctx& c) {
        auto& r = *c.rng; PedSetup(c); *(int32_t*)0x8D2E98 = r.below(64);
        c.opcode = Op(1672).I(r.below(3) == 0 ? -1 : g_pedRef).I(r.below(3) - 1).I(r.below(3) - 1).I(r.below(3) - 1).b; });
    Test<COMMAND_TASK_EVERYONE_LEAVE_CAR, &TaskEveryoneLeaveCar>("1675 TASK_EVERYONE_LEAVE_CAR", [&](Ctx& c) {
        auto& r = *c.rng; RandomVehicle(r);
        auto* v = static_cast<CVehicle*>(g_veh);
        v->m_pDriver = r.below(2) ? g_ped : nullptr;
        for (auto& p : v->m_apPassengers) p = r.below(2) ? g_ped : nullptr;
        v->m_nMaxPassengers = (uint8)r.below(9);
        VehWatch(c); c.opcode = Op(1675).I(g_vehRef).b; });
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
