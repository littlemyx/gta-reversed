// S6-E: differential test of SCRIPT COMMAND HANDLERS (group g18, ids 1800..1899) against the original machine code (exe oracle, see game_oracle.h and
// script_oracle_test.cpp, whose scaffolding this file follows).
// The handler TU source/game_sa/Scripts/Commands/Ported/Group18.cpp is #included here (its handlers live in an anonymous namespace), the test maps
// gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original addresses and, for every random case, runs
//   (1) the exe's group processor ProcessCommands1600To1699 (0x493FE0, thiscall(this = fake CRunningScript, cmd)) on a synthetic opcode buffer, and
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then compares the script variables (ScriptSpace), the IP, the compare flag, the watched memory and the log of the callee functions the handler invoked
// (the exe callees are patched with host recorders, the C++ side defines the same recorders for the game functions).
// usage: script_oracle_g18_test.exe [-v] [-n cases] [name-substring ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group18.cpp"

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
// ---------------------------------------------------------------------------------------------------------------------------------
// call log: the callees of the handlers are replaced by recorders on both sides
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static void Log(uint32_t tag) { g_log.push_back(tag); }
static void LogF(uint32_t tag, std::initializer_list<float> v) { g_log.push_back(tag); for (float f : v) g_log.push_back(FB(f)); }
static void LogU(uint32_t tag, std::initializer_list<uint32_t> v) { g_log.push_back(tag); for (auto u : v) g_log.push_back(u); }

static constexpr int kNumPool = 4;
static CVehicle* g_veh;     static int g_vehRef;
static CObject*  g_obj;     static int g_objRef;
static CPed*     g_ped;     static int g_pedRef;
static CObject*  g_obj2;                                // result of FindNearestObjectOfType
static CPed*     g_peds[kNumPool];     static int g_pedRefs[kNumPool];
static CVehicle* g_vehs[kNumPool];     static int g_vehRefs[kNumPool];
static CMatrix*  g_mat;
static void*     g_fakeVtbl[256];
static void*     g_fakeVtblTask[64];
static uint8_t   g_fakeIntel[0x294];
static uint8_t   g_fakeTask[0x40];
static CEntity*  g_findResult;
static void*     g_holdTask;       // result of GetTaskHold
static void*     g_pedGroup;       // result of GetPedsGroup
static uint32_t  g_seed;           // makes the "random" predicate recorders vary between cases
static uint32_t Tag(const void* p) { // identity of a fake entity, so that logs do not contain host addresses
    if (!p) return 0;
    if (p == g_veh) return 1;
    if (p == g_obj) return 2;
    if (p == g_ped) return 3;
    if (p == g_obj2) return 4;
    if (p == g_fakeTask) return 5;
    if (p == g_fakeIntel) return 6;
    for (int i = 0; i < kNumPool; ++i) { if (p == g_peds[i]) return 0x10 + i; if (p == g_vehs[i]) return 0x20 + i; }
    return 9;
}
static uint32_t SlotOf(const void* slot, const void* base) { return (uint32_t)((const uint8_t*)slot - (const uint8_t*)base); }

static float FakeGroundZ(float x, float y) { return x * 0.25f + y * 0.125f - 7.0f; }
static bool  FakeIsDead(const CPed* p) { return ((Tag(p) * 5 + g_seed) % 4) == 0; }
static bool  FakeValidRandom(int type, bool a, bool b, bool c) { return ((type * 7 + (int)a + 2 * (int)b + 4 * (int)c + (int)g_seed) % 5) != 0; }
static int   FakeAppearance(const void* v) { return ((const uint8_t*)v)[0x600] % 4; }
static bool  FakeCanBeDeleted(const void* v) { return (((const uint8_t*)v)[0x601] & 3) != 0; }

// -- host recorders for the exe (cdecl / thiscall: __fastcall(this, edx, args...) pops the stack args like a thiscall callee)
static float __cdecl H_GroundZ(float x, float y) { return FakeGroundZ(x, y); }
static CEntity* __cdecl H_FindObj(int model, const CVector* p, float radius, int b2D, int bld, int veh, int ped, int obj, int dum) {
    LogU(41, { (uint32_t)model }); LogF(41, { p->x, p->y, p->z, radius }); LogU(41, { (uint8_t)b2D, (uint8_t)bld, (uint8_t)veh, (uint8_t)ped, (uint8_t)obj, (uint8_t)dum });
    return g_findResult;
}
static void* __fastcall H_GetTaskHold(void* self, int, int flag) { LogU(42, { Tag(self), (uint8_t)flag }); return g_holdTask; }
static void  __fastcall H_DropEntity(void* self, int, void* ped, int flag) { LogU(43, { Tag(self), Tag(ped), (uint8_t)flag }); }
static bool  __fastcall H_SetPedPosition(void* self, int, void* ped) { LogU(44, { Tag(self), Tag(ped) }); return ((Tag(ped) + g_seed) & 1) != 0; }
static bool  __fastcall H_ApplyDamage(void* self, int, void* veh, int comp, float a, float b) { LogF(45, { (float)SlotOf(self, veh), (float)comp, a, b }); return false; }
static void  __fastcall H_TellHeli(void* self, int, float a, float b, float c, float d, float e) { LogF(46, { (float)Tag(self), a, b, c, d, e }); }
static void  __fastcall H_CleanUpRef(void* self, int, void** slot) { LogU(47, { Tag(self), SlotOf(slot, g_veh) }); }
static void  __fastcall H_RegisterRef(void* self, int, void** slot) { LogU(48, { Tag(self), SlotOf(slot, g_veh) }); }
static void  __fastcall H_SetCharCreatedBy(void* self, int, int v) { LogU(49, { Tag(self), (uint8_t)v }); }
static void  __fastcall H_SetVehCreatedBy(void* self, int, int v) { LogU(50, { Tag(self), (uint8_t)v }); }
static void  __fastcall H_AddEntityToList(void* self, int, int handle, int type) { LogU(51, { (uint32_t)handle, (uint32_t)type }); }
static void* __cdecl    H_GetPedsGroup(void* ped) { LogU(52, { Tag(ped) }); return Tag(ped) == 0x11 ? g_pedGroup : nullptr; }
static bool  __fastcall H_IsPedDead(void* self, int, void* ped) { LogU(53, { Tag(ped) }); return FakeIsDead((CPed*)ped); }
static bool  __fastcall H_ValidRandomPed(void* self, int, int type, int a, int b, int c) { LogU(54, { (uint32_t)type, a != 0, b != 0, c != 0 }); return FakeValidRandom(type, a != 0, b != 0, c != 0); }
static int   __fastcall H_Appearance(void* self, int) { LogU(55, { Tag(self) }); return FakeAppearance(self); }
static bool  __fastcall H_CanBeDeleted(void* self, int) { LogU(56, { Tag(self) }); return FakeCanBeDeleted(self); }
static void __fastcall H_RotY(CMatrix* m, int, float a) { m->RotateY(a); }
static void  __fastcall H_BlowUpCar(void* self, int, void* damager, int hide) { LogU(57, { Tag(self), Tag(damager), (uint8_t)hide }); }

// -- the same recorders as the game functions the handlers call (C++ side)
float CWorld::FindGroundZForCoord(float x, float y) { return FakeGroundZ(x, y); }
CEntity* CWorld::FindNearestObjectOfType(int32 m, const CVector& p, float radius, bool b2D, bool bld, bool veh, bool ped, bool obj, bool dum) { return H_FindObj(m, &p, radius, b2D, bld, veh, ped, obj, dum); }
CTaskSimpleHoldEntity* CPedIntelligence::GetTaskHold(bool flag) { return (CTaskSimpleHoldEntity*)H_GetTaskHold(this, 0, flag); }
void CTaskSimpleHoldEntity::DropEntity(CPed* ped, bool flag) { H_DropEntity(this, 0, ped, flag); }
bool CDamageManager::ApplyDamage(CAutomobile* veh, tComponent comp, float a, float b) { return H_ApplyDamage(this, 0, veh, comp, a, b); }
void CAutomobile::TellHeliToGoToCoors(float a, float b, float c, float d, float e) { H_TellHeli(this, 0, a, b, c, d, e); }
void CEntity::CleanUpOldReference(CEntity** s) { H_CleanUpRef(this, 0, (void**)s); }
void CEntity::RegisterReference(CEntity** s) { H_RegisterRef(this, 0, (void**)s); }
void CPed::SetCharCreatedBy(ePedCreatedBy v) { H_SetCharCreatedBy(this, 0, v); }
void CVehicle::SetVehicleCreatedBy(eVehicleCreatedBy v) { H_SetVehCreatedBy(this, 0, v); }
void CMissionCleanup::AddEntityToList(int32 handle, MissionCleanUpEntityType type) { H_AddEntityToList(this, 0, handle, type); }
CPedGroup* CPedGroups::GetPedsGroup(const CPed* ped) { return (CPedGroup*)H_GetPedsGroup((void*)ped); }
bool CRunningScript::IsPedDead(CPed* ped) const { return H_IsPedDead((void*)this, 0, ped); }
bool CRunningScript::ThisIsAValidRandomPed(ePedType type, bool a, bool b, bool c) { return H_ValidRandomPed(this, 0, type, a, b, c); }
eVehicleAppearance CVehicle::GetVehicleAppearance() const { return (eVehicleAppearance)H_Appearance((void*)this, 0); }
bool CVehicle::CanBeDeleted() { return H_CanBeDeleted(this, 0); }
void CRunningScript::ReadTextLabelFromScript(char* b, uint8 n) { oracle::Fn<void __fastcall(void*, int, char*, int)>(0x463D50)(this, 0, b, n); }
// the exe's own pure functions, used by the port as callees
int32 CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { return oracle::Fn<int __cdecl(int, int)>(0x4839A0)(ref, (int)type); }
float CPlaceable::GetHeading() const { return oracle::Fn<float __fastcall(const void*, int)>(0x441DB0)(this, 0); }
void  CPlane::IsAlreadyFlying() { oracle::Fn<void __fastcall(void*, int)>(0x6CAB90)(this, 0); }
int32 CRadar::GetActualBlipArrayIndex(tBlipHandle h) { return oracle::Fn<int __cdecl(int)>(0x582870)((int)h); }
int32 CShopping::GetPrice(uint32 id) { return oracle::Fn<int __cdecl(unsigned)>(0x49AD50)(id); }
void  CStreaming::DisableCopBikes(bool v) { oracle::Fn<void __cdecl(int)>(0x407D10)(v); }
uint32 CPedType::GetPedFlag(ePedType t) { return oracle::Fn<int __cdecl(int)>(0x608830)((int)t); }
void  CPedType::SetPedTypeAsAcquaintance(AcquaintanceId id, ePedType t, int32 f) { oracle::Fn<void __cdecl(int, int, int)>(0x608E20)((int)id, (int)t, f); }
void  CPedType::ClearPedTypeAsAcquaintance(AcquaintanceId id, ePedType t, int32 f) { oracle::Fn<void __cdecl(int, int, int)>(0x6089F0)((int)id, (int)t, f); }

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
    Op& S8(const char* s) { b.push_back(9); for (int i = 0; i < 8; ++i) b.push_back(i < (int)std::strlen(s) ? (uint8_t)s[i] : 0); return *this; }
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
// S6-D review additions: task / decision-maker recorders. Every task constructor the handlers call is replaced on BOTH sides by a recorder that
// logs the arguments it receives (wrong argument order / constants / ctor show up as a log diff); `operator new` is a logging calloc (the exe's
// allocation size must equal sizeof the C++ class); GivePedScriptedTask logs the task ordinal and a hash of its memory from +4 (fields the handler
// writes after the constructor, e.g. +0xE of the gang drive-by task, +0x18 / +0x1C of the car-set-ped-in tasks).
static std::vector<void*>  g_allocs;
static std::vector<size_t> g_allocSz;
static uint32_t TaskOrd(const void* p) {
    if (!p) return 0;
    for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == p) return (uint32_t)i + 1;
    return 0xF000;
}
static uint32_t TaskHash(const void* p) {
    for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == p) {
        uint32_t h = 2166136261u; for (size_t k = 4; k < g_allocSz[i]; ++k) h = (h ^ ((const uint8_t*)p)[k]) * 16777619u; return h;
    }
    return 0;
}
static void LogV3(const CVector* v) { if (v) LogF(0, { v->x, v->y, v->z }); else LogF(0, { 0.f, 0.f, 0.f }); }
static void Zero4(void* p, size_t n) { std::memset(reinterpret_cast<uint8_t*>(p) + 4, 0, n - 4); }   // a recorder replaces the whole ctor: undo member initialisers so the memory equals the exe's (calloc'd) task

static void* __cdecl H_TaskNew(size_t n) { void* p = std::calloc(1, n); g_allocs.push_back(p); g_allocSz.push_back(n); LogU(140, { (uint32_t)n }); return p; }
void* CTask::operator new(size_t n) { return H_TaskNew(n); }
void  CTask::operator delete(void*) {}
static void __fastcall H_Gpst(CRunningScript*, int, int h, void* task, int cmd) { LogU(141, { (uint32_t)h, TaskOrd(task), TaskHash(task), (uint32_t)cmd }); }
void CRunningScript::GivePedScriptedTask(int32 h, CTask* t, int32 cmd) { H_Gpst(this, 0, h, t, cmd); }

static void* __fastcall H_cGang(void* self, int, void* target, const CVector* pos, float abort, int freq, int style, int seatRHS) {
    LogF(150, { (float)TaskOrd(self), (float)Tag(target), abort, (float)(int8_t)freq, (float)(int8_t)style, (float)(uint8_t)seatRHS }); LogV3(pos); return self; }
CTaskSimpleGangDriveBy::CTaskSimpleGangDriveBy(CEntity* target, const CVector* pos, float abort, int8 freq, eDrivebyStyle style, bool seatRHS) {
    LogF(150, { (float)TaskOrd(this), (float)Tag(target), abort, (float)freq, (float)(int8_t)style, (float)(uint8_t)seatRHS }); LogV3(pos); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cGun(void* self, int, void* e, const CVector* tp, const CVector* mt, int cmd, int burst, int dur) {
    LogF(157, { (float)TaskOrd(self), (float)Tag(e), (float)(int8_t)cmd, (float)(int16_t)burst, (float)dur }); LogV3(tp); LogV3(mt); return self; }
CTaskSimpleGunControl::CTaskSimpleGunControl(CEntity* e, CVector const& tp, CVector const& mt, eGunCommand cmd, int16 burst, int32 dur) {
    LogF(157, { (float)TaskOrd(this), (float)Tag(e), (float)(int8_t)cmd, (float)(int16_t)burst, (float)dur }); LogV3(&tp); LogV3(&mt); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cPhone(void* self, int, int duration) { LogU(151, { TaskOrd(self), (uint32_t)duration }); return self; }
CTaskComplexUseMobilePhone::CTaskComplexUseMobilePhone(int32 duration) { LogU(151, { TaskOrd(this), (uint32_t)duration }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cAsDriver(void* self, int, void* veh, void* utility) { LogU(152, { TaskOrd(self), Tag(veh), (uint32_t)(uintptr_t)utility }); return self; }
CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(CVehicle* veh, CTaskUtilityLineUpPedWithCar* utility) { LogU(152, { TaskOrd(this), Tag(veh), (uint32_t)(uintptr_t)utility }); Zero4(this, sizeof(*this)); }
CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(CVehicle* veh, bool warp, CTaskUtilityLineUpPedWithCar* utility) : CTaskSimpleCarSetPedInAsDriver{ veh, utility } { m_bWarpingInToCar = warp; }   // NOTSA overload, as in the repo
static void* __fastcall H_cAsPass(void* self, int, void* veh, int door, void* utility) { LogU(153, { TaskOrd(self), Tag(veh), (uint32_t)door, (uint32_t)(uintptr_t)utility }); return self; }
CTaskSimpleCarSetPedInAsPassenger::CTaskSimpleCarSetPedInAsPassenger(CVehicle* veh, eTargetDoor door, bool warp, CTaskUtilityLineUpPedWithCar* utility) {
    LogU(153, { TaskOrd(this), Tag(veh), (uint32_t)door, (uint32_t)(uintptr_t)utility }); Zero4(this, sizeof(*this)); m_bWarpingInToCar = warp; }
static void* __fastcall H_cEffect(void* self, int, void* fx, void* e) { LogU(154, { TaskOrd(self), (uint32_t)(uintptr_t)fx, Tag(e) }); return self; }
CTaskComplexUseEffect::CTaskComplexUseEffect(C2dEffectPedAttractor* fx, CEntity* e) { LogU(154, { TaskOrd(this), (uint32_t)(uintptr_t)fx, Tag(e) }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cFlee(void* self, int, void* e, int attack, float safe, int fleeT, int shootT, int recT, float steal, int period, float tol) {
    LogF(155, { (float)TaskOrd(self), (float)Tag(e), (float)(uint8_t)attack, safe, (float)fleeT, (float)shootT, (float)recT, steal, (float)period, tol }); return self; }
CTaskComplexSmartFleeEntity::CTaskComplexSmartFleeEntity(CEntity*, bool, float, int32, int32, float, eMoveState) {}   // base ctor of the recorded ctor below (the exe's whole ctor is replaced)
CTaskComplexFleeAnyMeans::CTaskComplexFleeAnyMeans(CEntity* e, bool attack, float safe, int32 fleeT, int32 shootT, int32 recT, float steal, int32 period, float tol) : CTaskComplexSmartFleeEntity(e, attack, safe, fleeT, period, tol) {
    LogF(155, { (float)TaskOrd(this), (float)Tag(e), (float)(uint8_t)attack, safe, (float)fleeT, (float)shootT, (float)recT, steal, (float)period, tol }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cDie(void* self, int, int weapon, int group, int anim, float blend, float speed, int a, int b, int dir, int c) {
    LogF(156, { (float)TaskOrd(self), (float)weapon, (float)group, (float)anim, blend, speed, (float)(uint8_t)a, (float)(uint8_t)b, (float)dir, (float)(uint8_t)c }); return self; }
CTaskComplexDie::CTaskComplexDie(eWeaponType weapon, AssocGroupId group, AnimationId anim, float blend, float speed, bool a, bool b, eDirection dir, bool c) {
    LogF(156, { (float)TaskOrd(this), (float)weapon, (float)group, (float)anim, blend, speed, (float)(uint8_t)a, (float)(uint8_t)b, (float)dir, (float)(uint8_t)c }); Zero4(this, sizeof(*this)); }

// -- scripted task lookup (1833 quit path): the fake primary task has the fake vtable (slot 4 = GetTaskType, slot 6 = MakeAbortable)
static bool g_haveTask; static int g_taskType;
static void* __fastcall H_FindTask(void* self, int, int idx, int type) { LogU(160, { SlotOf(self, g_fakeIntel), (uint32_t)idx, (uint32_t)type }); return g_haveTask ? g_fakeTask : nullptr; }
CTask* CTaskManager::FindTaskByType(ePrimaryTasks idx, eTaskType type) const { return (CTask*)H_FindTask((void*)this, 0, idx, type); }
static int  __fastcall H_TaskType(void*, int) { return g_taskType; }
static int  __fastcall H_MakeAbortable(void* self, int, void* ped, int prio, void* ev) { LogU(161, { Tag(self), Tag(ped), (uint32_t)prio, (uint32_t)(uintptr_t)ev }); return 1; }

static int __cdecl H_ComputeDoor(const void* veh, int seat) { LogU(162, { Tag(veh), (uint32_t)seat }); return (int)((Tag(veh) * 3 + (uint32_t)seat) & 7); }
int32 CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(const CVehicle* v, int32 s) { return H_ComputeDoor(v, s); }

// -- decision makers
static void* __cdecl H_DmInstance() { static uint8_t b[64]; return b; }
CDecisionMakerTypes* CDecisionMakerTypes::GetInstance() { return (CDecisionMakerTypes*)H_DmInstance(); }
static void __fastcall H_DmFlush(void*, int, int idx, int ev) { LogU(170, { (uint32_t)idx, (uint32_t)ev }); }
void CDecisionMakerTypes::FlushDecisionMakerEventResponse(int32 idx, eEventType ev) { H_DmFlush(this, 0, idx, ev); }
static void __fastcall H_DmAdd(void*, int, int idx, int ev, int task, float* ch, int* fl) { LogU(171, { (uint32_t)idx, (uint32_t)ev, (uint32_t)task, (uint32_t)fl[0], (uint32_t)fl[1] }); LogF(0, { ch[0], ch[1], ch[2], ch[3] }); }
void CDecisionMakerTypes::AddEventResponse(int32 idx, eEventType ev, eTaskType task, float* ch, int32* fl) { H_DmAdd(this, 0, idx, ev, task, ch, fl); }

// -- RNG-free preamble per run
static uint32_t g_caseSeed;
static void PreRun() { g_allocs.clear(); g_allocSz.clear(); }

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
    constexpr unsigned groupFn = 0x46D050; // ProcessCommands1800To1899
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

// ---------------------------------------------------------------------------------------------------------------------------------
static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }
static int32_t Flag(Rng& r) { return r.below(3) == 0 ? 0 : r.below(2) ? 1 : (int32_t)r.u32(); }

// fully random vehicle memory (the pool slot is CHeli sized), but with the pointers the commands dereference kept valid
static void RandomVehicleAt(CVehicle* v, Rng& r) {
    RandomFill(v, sizeof(CHeli), r);
    *reinterpret_cast<void**>(v) = g_fakeVtbl;                                // vptr
    v->m_matrix = reinterpret_cast<decltype(v->m_matrix)>(g_mat);             // +0x14
    if (r.below(4) == 0) v->m_autoPilot.m_nCarMission = (eCarMission)(57 + r.below(2));
    v->m_autoPilot.m_TargetEntity = r.below(2) ? nullptr : reinterpret_cast<CVehicle*>(g_obj);
    if (r.below(4) == 0) v->m_nModelIndex = 0x208;
    ((uint8_t*)v)[0x600] = (uint8_t)r.u32(); ((uint8_t*)v)[0x601] = (uint8_t)r.u32();
}
static void RandomVehicle(Rng& r) {
    RandomFill(g_mat, sizeof(CMatrix), r);
    RandomVehicleAt(g_veh, r);
}
static void RandomObject(Rng& r) {
    RandomFill(g_obj, sizeof(CObject), r);
    if (r.below(3) == 0) g_obj->m_fHealth = r.below(2) ? r.F(1e12f) : r.F(100.f);
}
static void RandomPedAt(CPed* p, Rng& r) {
    RandomFill(p, sizeof(CPed), r);
    p->m_matrix = nullptr;                                                    // +0x14: position from the placement
    *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(p) + 0x47C) = g_fakeIntel;   // CPed::m_pIntelligence
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x484) = r.below(4) ? 1 : r.below(4);          // m_nCreatedBy
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x598) = r.below(30);   // m_nPedType
    if (r.below(4)) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x1C) &= ~0x800u;
    if (r.below(4)) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x470) &= ~8u;
}

static void SetupFakeWorld() {
    auto* pp = new CPedPool(kNumPool, "oracle_ped");
    auto* vp = new CVehiclePool(kNumPool, "oracle_veh");
    auto* op = new CObjectPool(4, "oracle_obj");
    *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp;
    *reinterpret_cast<CObjectPool**>(0xB7449C) = op;
    for (int i = 0; i < kNumPool; ++i) {
        g_peds[i] = pp->New(); g_pedRefs[i] = pp->GetRef(g_peds[i]); std::memset(g_peds[i], 0, sizeof(CPed));
        g_vehs[i] = vp->New(); g_vehRefs[i] = vp->GetRef(g_vehs[i]);
    }
    g_ped = g_peds[0]; g_pedRef = g_pedRefs[0];
    g_veh = g_vehs[0]; g_vehRef = g_vehRefs[0];
    g_obj = op->New();  g_objRef = op->GetRef(g_obj);
    g_obj2 = op->New();
    g_mat = new CMatrix();
    reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C)[0] = g_fakeIntel;   // CPed::m_pIntelligence
    g_fakeVtbl[0xA4 / 4] = (void*)&H_BlowUpCar;          // CVehicle::BlowUpCar(damager, hide) -- slot 41
    g_fakeVtblTask[0x20 / 4] = (void*)&H_SetPedPosition; // CTaskSimpleHoldEntity::SetPedPosition(ped) -- slot 8
    *reinterpret_cast<void**>(g_fakeTask) = g_fakeVtblTask;
    static uint8_t acq[32 * 20];                         // CPedType::ms_apPedTypes (32 types x CAcquaintance {5 x uint32})
    *reinterpret_cast<void**>(0xC0BBE8) = acq;
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestAll() {
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CHeli) } }; };
    auto ObjWatch = [](Ctx& c) { c.watch = { { g_obj, sizeof(CObject) } }; };
    auto PedWatch = [](Ctx& c) { c.watch = { { g_ped, sizeof(CPed) } }; };
    auto VehSetup = [&](Ctx& c) { g_seed = c.rng->u32(); RandomVehicle(*c.rng); VehWatch(c); };
    // handle of a vehicle / ped: valid, -1, or garbage
    auto VH = [](Rng& r) { return r.below(5) == 0 ? -1 : r.below(8) == 0 ? (g_vehRef ^ 0x55) : g_vehRef; };
    auto PH = [](Rng& r) { return r.below(5) == 0 ? -1 : r.below(8) == 0 ? (g_pedRef ^ 0x55) : g_pedRef; };

    // ---- flags / fields of vehicles
    Test<COMMAND_SET_CAR_STAY_IN_SLOW_LANE, &SetCarStayInSlowLane>("1812 SET_CAR_STAY_IN_SLOW_LANE", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1812).I(g_vehRef).I(Flag(*c.rng)).b; });
    Test<COMMAND_SET_CAR_CAN_GO_AGAINST_TRAFFIC, &SetCarCanGoAgainstTraffic>("1851 SET_CAR_CAN_GO_AGAINST_TRAFFIC", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1851).I(g_vehRef).I(Flag(*c.rng)).b; });
    Test<COMMAND_PLANE_ATTACK_PLAYER, &PlaneAttackPlayer>("1806 PLANE_ATTACK_PLAYER", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1806).I(g_vehRef).I((int32_t)c.rng->u32()).Fl(c.rng->F(1000.f)).b; });
    Test<COMMAND_PLANE_FLY_IN_DIRECTION, &PlaneFlyInDirection>("1807 PLANE_FLY_IN_DIRECTION", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1807).I(g_vehRef).Fl(c.rng->F(720.f)).Fl(c.rng->F(1000.f)).Fl(c.rng->F(1000.f)).b; });
    Test<COMMAND_PLANE_FOLLOW_ENTITY, &PlaneFollowEntity>("1808 PLANE_FOLLOW_ENTITY", [&](Ctx& c) { VehSetup(c); auto& r = *c.rng; c.opcode = Op(1808).I(g_vehRef).I(PH(r)).I(VH(r)).Fl(r.F(1000.f)).b; });
    Test<COMMAND_HELI_ATTACK_PLAYER, &HeliAttackPlayer>("1828 HELI_ATTACK_PLAYER", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1828).I(g_vehRef).I((int32_t)c.rng->u32()).Fl(c.rng->F(1000.f)).b; });
    Test<COMMAND_HELI_FOLLOW_ENTITY, &HeliFollowEntity<MISSION_HELI_FOLLOW_ENTITY>>("1830 HELI_FOLLOW_ENTITY", [&](Ctx& c) { VehSetup(c); auto& r = *c.rng; c.opcode = Op(1830).I(g_vehRef).I(PH(r)).I(VH(r)).Fl(r.F(1000.f)).b; });
    Test<COMMAND_POLICE_HELI_CHASE_ENTITY, &HeliFollowEntity<MISSION_HELI_POLICE_BEHAVIOUR>>("1831 POLICE_HELI_CHASE_ENTITY", [&](Ctx& c) { VehSetup(c); auto& r = *c.rng; c.opcode = Op(1831).I(g_vehRef).I(PH(r)).I(VH(r)).Fl(r.F(1000.f)).b; });
    Test<COMMAND_SET_PLANE_THROTTLE, &SetPlaneThrottle>("1858 SET_PLANE_THROTTLE", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1858).I(g_vehRef).Fl(c.rng->F(10.f)).b; });
    Test<COMMAND_HELI_LAND_AT_COORDS, &HeliLandAtCoords>("1859 HELI_LAND_AT_COORDS", [&](Ctx& c) { VehSetup(c); auto& r = *c.rng; c.opcode = Op(1859).I(g_vehRef).Fl(r.F(3000.f)).Fl(r.F(3000.f)).Fl(r.F(300.f)).Fl(r.F(300.f)).Fl(r.F(300.f)).b; });
    Test<COMMAND_PLANE_STARTS_IN_AIR, &PlaneStartsInAir>("1861 PLANE_STARTS_IN_AIR", [&](Ctx& c) { VehSetup(c); CTimer::m_snTimeInMilliseconds = c.rng->u32(); c.opcode = Op(1861).I(g_vehRef).b; });
    Test<COMMAND_EXPLODE_CAR_IN_CUTSCENE, &ExplodeCarInCutscene>("1804 EXPLODE_CAR_IN_CUTSCENE", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1804).I(g_vehRef).b; });
    Test<COMMAND_DAMAGE_CAR_PANEL, &DamageCarComponent<0xB>>("1840 DAMAGE_CAR_PANEL", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1840).I(g_vehRef).I(c.rng->below(20)).b; });
    Test<COMMAND_DAMAGE_CAR_DOOR, &DamageCarComponent<5>>("1852 DAMAGE_CAR_DOOR", [&](Ctx& c) { VehSetup(c); c.opcode = Op(1852).I(g_vehRef).I(c.rng->below(20)).b; });
    Test<COMMAND_SET_CAR_ROLL, &SetCarRoll>("1841 SET_CAR_ROLL", [&](Ctx& c) {
        VehSetup(c); c.watch.push_back({ g_mat, sizeof(CMatrix) });
        auto& r = *c.rng; for (int i = 0; i < 12; ++i) ((float*)g_mat)[i] = r.F(2.f, false);
        if (r.below(4) == 0) ((float*)g_mat)[0] = ((float*)g_mat)[1] = 0.f;     // degenerate heading
        c.opcode = Op(1841).I(g_vehRef).Fl(r.F(720.f)).b; });
    Test<COMMAND_SET_CAR_AS_MISSION_CAR, &SetCarAsMissionCar>("1891 SET_CAR_AS_MISSION_CAR", [&](Ctx& c) { VehSetup(c); *(uint8_t*)((uint8_t*)g_veh + 0x4A4) = (uint8_t)c.rng->below(5); c.opcode = Op(1891).I(g_vehRef).b; });

    // ---- objects
    Test<COMMAND_GET_OBJECT_HEALTH, &GetObjectHealth>("1822 GET_OBJECT_HEALTH", [&](Ctx& c) {
        RandomObject(*c.rng); if (c.rng->below(4) == 0) g_obj->m_fHealth = BF(c.rng->u32()); ObjWatch(c); c.opcode = Op(1822).I(g_objRef).GV(0).b; });
    Test<COMMAND_SET_OBJECT_HEALTH, &SetObjectHealth>("1823 SET_OBJECT_HEALTH", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1823).I(g_objRef).I(c.rng->below(2) ? (int32_t)c.rng->u32() : c.rng->below(2000) - 500).b; });
    Test<COMMAND_SET_OBJECT_VISIBLE, &SetObjectVisible>("1872 SET_OBJECT_VISIBLE", [&](Ctx& c) { RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(1872).I(g_objRef).I(Flag(*c.rng)).b; });
    Test<COMMAND_IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED, &IsClosestObjectOfTypeSmashedOrDamaged>("1814 IS_CLOSEST_OBJECT_SMASHED_OR_DAMAGED", [&](Ctx& c) {
        auto& r = *c.rng; RandomFill(g_obj2, sizeof(CObject), r);
        if (r.below(4)) *(uint8_t*)((uint8_t*)g_obj2 + 0x36) = (*(uint8_t*)((uint8_t*)g_obj2 + 0x36) & ~7) | 4;   // type OBJECT
        g_findResult = r.below(6) == 0 ? nullptr : g_obj2;
        c.watch = { { g_obj2, 0x150 } };
        const float z = r.below(3) == 0 ? -100.f - r.f01() * 50.f : r.below(8) == 0 ? -100.f : r.F(500.f);
        c.opcode = Op(1814).Fl(r.F(3000.f)).Fl(r.F(3000.f)).Fl(z).Fl(r.F(50.f)).I(r.below(4) == 0 ? -(int)(1 + r.below(5)) : (int)r.below(20000)).I(Flag(r)).I(Flag(r)).b; });

    // ---- peds
    Test<COMMAND_HAS_CHAR_BEEN_ARRESTED, &HasCharBeenArrested>("1857 HAS_CHAR_BEEN_ARRESTED", [&](Ctx& c) {
        RandomPedAt(g_ped, *c.rng); if (c.rng->below(2)) *(uint32_t*)((uint8_t*)g_ped + 0x530) = 0x3F; PedWatch(c); c.opcode = Op(1857).I(g_pedRef).b; });
    Test<COMMAND_SET_INFORM_RESPECTED_FRIENDS, &SetInformRespectedFriends>("1870 SET_INFORM_RESPECTED_FRIENDS", [&](Ctx& c) {
        RandomFill(g_fakeIntel, sizeof(g_fakeIntel), *c.rng); c.watch = { { g_fakeIntel, sizeof(g_fakeIntel) } };
        c.opcode = Op(1870).I(g_pedRef).Fl(c.rng->F(100.f)).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_DROP_OBJECT, &DropObject>("1803 DROP_OBJECT", [&](Ctx& c) {
        auto& r = *c.rng; g_seed = r.u32(); RandomPedAt(g_ped, r); g_holdTask = r.below(3) ? g_fakeTask : nullptr; PedWatch(c);
        c.opcode = Op(1803).I(g_pedRef).I(Flag(r)).b; });
    Test<COMMAND_IS_CHAR_HOLDING_OBJECT, &IsCharHoldingObject>("1847 IS_CHAR_HOLDING_OBJECT", [&](Ctx& c) {
        auto& r = *c.rng; g_seed = r.u32(); for (auto* p : g_peds) RandomPedAt(p, r);
        g_holdTask = r.below(4) ? g_fakeTask : nullptr; RandomFill(g_fakeTask + 8, 8, r);
        *(void**)(g_fakeTask + 8) = r.below(3) ? (void*)g_obj : r.below(2) ? (void*)g_obj2 : nullptr;       // m_pEntityToHold
        c.watch = { { g_fakeTask, sizeof(g_fakeTask) } };
        c.opcode = Op(1847).I(r.below(3) == 0 ? -1 : r.below(8) == 0 ? (g_pedRef ^ 0x55) : g_pedRef).I(r.below(3) == 0 ? -1 : g_objRef).b; });
    Test<COMMAND_GET_RANDOM_CAR_IN_SPHERE_NO_SAVE, &GetRandomCarInSphereNoSave>("1854 GET_RANDOM_CAR_IN_SPHERE_NO_SAVE", [&](Ctx& c) {
        auto& r = *c.rng; g_seed = r.u32();
        const float x = r.F(100.f, false), y = r.F(100.f, false), z = r.F(10.f, false);
        for (auto* v : g_vehs) {
            RandomVehicleAt(v, r); v->m_matrix = nullptr;
            *(CVector*)((uint8_t*)v + 4) = CVector{ x + r.F(30.f, false), y + r.F(30.f, false), z + r.F(10.f, false) };
            v->m_nModelIndex = (int16_t)(r.below(3) ? 400 + r.below(3) : r.below(600));
            if (r.below(3)) *(uint32_t*)((uint8_t*)v + 0x428) &= ~1u;
            v->vehicleFlags.bIsLawEnforcer = r.below(6) == 0;
        }
        c.watch = {};
        c.opcode = Op(1854).Fl(x).Fl(y).Fl(z).Fl(r.below(5) == 0 ? r.F(20.f) : 10.f + r.f01() * 30.f).I(r.below(3) == 0 ? -1 : 400 + r.below(3)).GV(0).b; });
    Test<COMMAND_GET_RANDOM_CHAR_IN_SPHERE, &GetRandomCharInSphere>("1855 GET_RANDOM_CHAR_IN_SPHERE", [&](Ctx& c) {
        auto& r = *c.rng; g_seed = r.u32(); g_pedGroup = (void*)0x1234;
        const float x = r.F(100.f, false), y = r.F(100.f, false), z = r.F(10.f, false);
        for (auto* p : g_peds) {
            RandomPedAt(p, r);
            *(CVector*)((uint8_t*)p + 4) = CVector{ x + r.F(30.f, false), y + r.F(30.f, false), z + r.F(10.f, false) };
        }
        *(uint32_t*)0xC0EC24 = r.u32();
        c.watch = { { (void*)0xC0EC24, 4 } };
        c.opcode = Op(1855).Fl(x).Fl(y).Fl(z).Fl(r.below(5) == 0 ? r.F(20.f) : 10.f + r.f01() * 30.f).I(Flag(r)).I(Flag(r)).I(Flag(r)).GV(0).b; });

    // ---- globals
    Test<COMMAND_SET_RADAR_ZOOM, &SetRadarZoom>("1883 SET_RADAR_ZOOM", [&](Ctx& c) { c.watch = { { (void*)0xA444A0, 8 } }; RandomFill((void*)0xA444A0, 8, *c.rng); c.opcode = Op(1883).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SWITCH_COPS_ON_BIKES, &SwitchCopsOnBikes>("1836 SWITCH_COPS_ON_BIKES", [&](Ctx& c) { c.watch = { { (void*)0x9654BC, 8 } }; RandomFill((void*)0x9654BC, 8, *c.rng); c.opcode = Op(1836).I(Flag(*c.rng)).b; });
    auto SuppSetup = [&](Ctx& c) {
        auto& r = *c.rng; auto* l = (int32_t*)0xA44940;
        for (int i = 0; i < 41; ++i) l[i] = r.below(3) == 0 ? -1 : (int32_t)r.below(6);
        if (r.below(3) == 0) for (int i = 0; i < 40; ++i) l[i] = (int32_t)r.below(5);      // full list (no free slot)
        c.watch = { { l, 41 * 4 } };
    };
    Test<COMMAND_SUPPRESS_CAR_MODEL, &SuppressCarModel>("1842 SUPPRESS_CAR_MODEL", [&](Ctx& c) { SuppSetup(c); c.opcode = Op(1842).I(c.rng->below(2) ? (int32_t)c.rng->below(8) : -1 + (int32_t)c.rng->below(3) * 7).b; });
    Test<COMMAND_DONT_SUPPRESS_CAR_MODEL, &DontSuppressCarModel>("1843 DONT_SUPPRESS_CAR_MODEL", [&](Ctx& c) { SuppSetup(c); c.opcode = Op(1843).I((int32_t)c.rng->below(8)).b; });
    Test<COMMAND_DONT_SUPPRESS_ANY_CAR_MODELS, &DontSuppressAnyCarModels>("1844 DONT_SUPPRESS_ANY_CAR_MODELS", [&](Ctx& c) { SuppSetup(c); c.opcode = Op(1844).b; });
    auto PatrolSetup = [&](Ctx& c) {
        auto& r = *c.rng; RandomFill((void*)0xC18DB8, 0x1A4, r); *(int32_t*)0xC18DB8 = r.below(10) - 1;
        c.watch = { { (void*)0xC18DB8, 0x1A4 } };
    };
    Test<COMMAND_FLUSH_PATROL_ROUTE, &FlushPatrolRoute>("1876 FLUSH_PATROL_ROUTE", [&](Ctx& c) { PatrolSetup(c); c.opcode = Op(1876).b; });
    Test<COMMAND_EXTEND_PATROL_ROUTE, &ExtendPatrolRoute>("1877 EXTEND_PATROL_ROUTE", [&](Ctx& c) {
        PatrolSetup(c); auto& r = *c.rng;
        static const char* names[] = { "NONE", "NONE", "None", "NONEX", "anim_a", "KICK", "", "abcdefg" };
        static const char* groups[] = { "PED", "BLOCK", "", "x", "gangs" };
        c.opcode = Op(1877).Fl(r.F(300.f)).Fl(r.F(300.f)).Fl(r.F(30.f)).S8(names[r.below(8)]).S8(groups[r.below(5)]).b; });

    // ---- exe tables reached through exe callees (forwarded)
    Test<COMMAND_SET_RELATIONSHIP, &SetRelationship>("1862 SET_RELATIONSHIP", [&](Ctx& c) {
        auto& r = *c.rng; RandomFill(*(void**)0xC0BBE8, 32 * 20, r); c.watch = { { *(void**)0xC0BBE8, 32 * 20 } };
        c.opcode = Op(1862).I(r.below(5)).I(r.below(30)).I(r.below(34)).b; });
    Test<COMMAND_CLEAR_RELATIONSHIP, &ClearRelationship>("1863 CLEAR_RELATIONSHIP", [&](Ctx& c) {
        auto& r = *c.rng; RandomFill(*(void**)0xC0BBE8, 32 * 20, r); c.watch = { { *(void**)0xC0BBE8, 32 * 20 } };
        c.opcode = Op(1863).I(r.below(5)).I(r.below(30)).I(r.below(34)).b; });
    Test<COMMAND_DOES_BLIP_EXIST, &DoesBlipExist>("1884 DOES_BLIP_EXIST", [&](Ctx& c) {
        auto& r = *c.rng; for (int i = 0; i < 32; ++i) { auto* b = (uint8_t*)(0xBA86F0 + i * 0x28); (void)b; }
        c.opcode = Op(1884).I(r.below(3) == 0 ? -1 : (int32_t)r.u32()).b; });
    Test<COMMAND_GET_NUMBER_OF_ITEMS_IN_SHOP, &GetNumberOfItemsInShop>("1887 GET_NUMBER_OF_ITEMS_IN_SHOP", [&](Ctx& c) { *(int32_t*)0xA9A7F0 = (int32_t)c.rng->u32(); c.opcode = Op(1887).GV(0).b; });
    Test<COMMAND_GET_ITEM_IN_SHOP, &GetItemInShop>("1888 GET_ITEM_IN_SHOP", [&](Ctx& c) { RandomFill((void*)0xA9A318, 300 * 4, *c.rng); c.opcode = Op(1888).I(c.rng->below(300)).GV(0).b; });
    Test<COMMAND_GET_PRICE_OF_ITEM, &GetPriceOfItem>("1889 GET_PRICE_OF_ITEM", [&](Ctx& c) {
        auto& r = *c.rng; *(uint32_t*)0xA9A310 = 300; RandomFill((void*)0xA986F0, 300 * 8, r); *(int32_t*)0xA9A7CC = r.below(300);
        c.opcode = Op(1889).I((int32_t)r.u32()).GV(0).b; });

    // =========================================================================================================================
    // S6-D review: decision makers (recorded), task commands (constructor recorders), quit path of the mobile phone
    {
        auto DmSetup = [&](Ctx& c) {
            auto& r = *c.rng;
            auto* act = reinterpret_cast<uint8_t*>(0xC0B01C); auto* ref = reinterpret_cast<uint16_t*>(0xC0AFF4);
            for (int i = 0; i < 24; ++i) { act[i] = (uint8_t)r.below(2); ref[i] = (uint16_t)r.below(4); }
            c.watch = { { act, 24 }, { ref, 48 } };
            const int idx = r.below(22);
            return r.below(8) == 0 ? -1 : (int)((uint32_t)idx | ((uint32_t)r.below(4) << 16));
        };
        Test<COMMAND_CLEAR_CHAR_DECISION_MAKER_EVENT_RESPONSE, &ClearDecisionMakerEventResponse>("1800 CLEAR_CHAR_DECISION_MAKER_EVENT_RESPONSE", [&](Ctx& c) { const int dm = DmSetup(c); c.opcode = Op(1800).I(dm).I((int32_t)c.rng->u32() % 300).b; });
        Test<COMMAND_CLEAR_GROUP_DECISION_MAKER_EVENT_RESPONSE, &ClearDecisionMakerEventResponse>("1865 CLEAR_GROUP_DECISION_MAKER_EVENT_RESPONSE", [&](Ctx& c) { const int dm = DmSetup(c); c.opcode = Op(1865).I(dm).I((int32_t)c.rng->u32() % 300).b; });
        auto Add = [&](Ctx& c, int cmd) { auto& r = *c.rng; const int dm = DmSetup(c);
            c.opcode = Op(cmd).I(dm).I(r.below(300)).I(r.below(1500)).Fl(r.F(100)).Fl(r.F(100)).Fl(r.F(100)).Fl(r.F(100)).I((int32_t)r.u32()).I((int32_t)r.u32()).b; };
        Test<COMMAND_ADD_CHAR_DECISION_MAKER_EVENT_RESPONSE, &AddDecisionMakerEventResponse>("1801 ADD_CHAR_DECISION_MAKER_EVENT_RESPONSE", [&](Ctx& c) { Add(c, 1801); });
        Test<COMMAND_ADD_GROUP_DECISION_MAKER_EVENT_RESPONSE, &AddDecisionMakerEventResponse>("1866 ADD_GROUP_DECISION_MAKER_EVENT_RESPONSE", [&](Ctx& c) { Add(c, 1866); });
    }
    g_fakeVtblTask[0x10 / 4] = (void*)&H_TaskType;
    g_fakeVtblTask[0x18 / 4] = (void*)&H_MakeAbortable;
    auto PHnd = [&](Rng& r) { return r.below(5) == 0 ? -1 : g_pedRef; };
    auto TgtP = [&](Rng& r) { return r.below(4) == 0 ? -1 : g_pedRefs[r.below(kNumPool)]; };
    auto TgtV = [&](Rng& r) { return r.below(3) == 0 ? -1 : g_vehRefs[r.below(kNumPool)]; };
    Test<COMMAND_TASK_DRIVE_BY, &TaskDriveBy>("1811 TASK_DRIVE_BY", [&](Ctx& c) {
        auto& r = *c.rng; PedWatch(c);
        RandomFill(g_fakeTask, sizeof(g_fakeTask), r); *reinterpret_cast<void**>(g_fakeTask) = g_fakeVtblTask;
        *reinterpret_cast<void**>(g_fakeTask + 0x34) = r.below(3) == 0 ? nullptr : (void*)g_peds[r.below(kNumPool)];
        *reinterpret_cast<void**>(g_fakeIntel + 0x10) = r.below(4) == 0 ? nullptr : g_fakeTask; g_taskType = r.below(2) ? 0x3FE : (int)r.below(2000);
        c.watch.push_back({ g_fakeIntel, sizeof(g_fakeIntel) });
        c.opcode = Op(1811).I(PHnd(r)).I(TgtP(r)).I(TgtV(r)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(100)).I(r.below(2) ? (int)r.below(5) : (int)r.u32()).I(Flag(r)).I(r.below(2) ? (int)r.below(100) : (int)r.u32()).b; });
    Test<COMMAND_TASK_USE_MOBILE_PHONE, &TaskUseMobilePhone>("1833 TASK_USE_MOBILE_PHONE", [&](Ctx& c) {
        auto& r = *c.rng; PedWatch(c);
        RandomFill(g_fakeTask, sizeof(g_fakeTask), r); *reinterpret_cast<void**>(g_fakeTask) = g_fakeVtblTask;
        g_haveTask = r.below(4) != 0; g_taskType = r.below(3) ? 0x640 : (int)r.below(2000);
        c.watch.push_back({ g_fakeTask, sizeof(g_fakeTask) });
        c.opcode = Op(1833).I(g_pedRef).I(r.below(3) == 0 ? 1 + (int)r.below(5) : r.below(3) == 0 ? -(int)r.below(5) : (int)r.u32()).b; });
    Test<COMMAND_TASK_WARP_CHAR_INTO_CAR_AS_DRIVER, &TaskWarpCharIntoCarAsDriver>("1834 TASK_WARP_CHAR_INTO_CAR_AS_DRIVER", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1834).I(PHnd(r)).I(TgtV(r)).b; });
    Test<COMMAND_TASK_WARP_CHAR_INTO_CAR_AS_PASSENGER, &TaskWarpCharIntoCarAsPassenger>("1835 TASK_WARP_CHAR_INTO_CAR_AS_PASSENGER", [&](Ctx& c) {
        auto& r = *c.rng; for (int i = 0; i < kNumPool; ++i) RandomVehicleAt(g_vehs[i], r);
        for (int i = 0; i < kNumPool; ++i) c.watch.push_back({ g_vehs[i], sizeof(CHeli) });
        c.opcode = Op(1835).I(PHnd(r)).I(g_vehRefs[r.below(kNumPool)]).I(r.below(2) ? (int)r.below(8) - 2 : (int)r.u32() % 20).b; });
    Test<COMMAND_TASK_USE_ATTRACTOR, &TaskUseAttractor>("1868 TASK_USE_ATTRACTOR", [&](Ctx& c) {
        auto& r = *c.rng; auto* act = reinterpret_cast<uint8_t*>(0xC3A1A0); auto* ref = reinterpret_cast<uint16_t*>(0xC3A120);
        for (int i = 0; i < 70; ++i) { act[i] = (uint8_t)r.below(2); ref[i] = (uint16_t)r.below(3); }
        c.watch = { { act, 70 }, { ref, 140 } };
        const int idx = r.below(70);
        c.opcode = Op(1868).I(PHnd(r)).I(r.below(8) == 0 ? -1 : (int)((uint32_t)idx | ((uint32_t)r.below(3) << 16))).b; });
    Test<COMMAND_TASK_SHOOT_AT_CHAR, &TaskShootAtChar>("1869 TASK_SHOOT_AT_CHAR", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1869).I(PHnd(r)).I(TgtP(r)).I(r.below(2) ? (int)r.below(10000) : (int)r.u32()).b; });
    Test<COMMAND_TASK_FLEE_CHAR_ANY_MEANS, &TaskFleeCharAnyMeans>("1873 TASK_FLEE_CHAR_ANY_MEANS", [&](Ctx& c) {
        auto& r = *c.rng; *(float*)0xC18CF0 = r.F(10); c.watch = { { (void*)0xC18CF0, 4 } };
        c.opcode = Op(1873).I(PHnd(r)).I(TgtP(r)).Fl(r.F(100)).I((int)r.u32() % 100000).I(Flag(r)).I((int)r.u32() % 100000).I((int)r.u32() % 100000).Fl(r.F(100)).b; });
    Test<COMMAND_TASK_DEAD, &TaskDead>("1890 TASK_DEAD", [&](Ctx& c) { auto& r = *c.rng; c.opcode = Op(1890).I(PHnd(r)).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g18_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice

    // exe callees of the handlers -> host recorders
    oracle::Patch(0x569660, (void*)&H_GroundZ);
    if (!std::getenv("MATCHK")) oracle::Patch(0x59B2C0, (void*)&H_RotY);   // CMatrix::RotateY of the port differs from the exe (see MATCHK): compare the handler logic only
    oracle::Patch(0x5693F0, (void*)&H_FindObj);
    oracle::Patch(0x600FF0, (void*)&H_GetTaskHold);
    oracle::Patch(0x6930F0, (void*)&H_DropEntity);
    oracle::Patch(0x6C24B0, (void*)&H_ApplyDamage);
    oracle::Patch(0x6A2390, (void*)&H_TellHeli);
    oracle::Patch(0x571A00, (void*)&H_CleanUpRef);
    oracle::Patch(0x571B70, (void*)&H_RegisterRef);
    oracle::Patch(0x5E47E0, (void*)&H_SetCharCreatedBy);
    oracle::Patch(0x6D5D70, (void*)&H_SetVehCreatedBy);
    oracle::Patch(0x4637E0, (void*)&H_AddEntityToList);
    oracle::Patch(0x5F7E80, (void*)&H_GetPedsGroup);
    oracle::Patch(0x464D70, (void*)&H_IsPedDead);
    oracle::Patch(0x489490, (void*)&H_ValidRandomPed);
    oracle::Patch(0x6D1080, (void*)&H_Appearance);
    oracle::Patch(0x6D1180, (void*)&H_CanBeDeleted);

    if (std::getenv("MATCHK")) {
        Rng r(5); int bad = 0;
        for (int i = 0; i < 2000; ++i) {
            alignas(16) CMatrix a, b; for (int k = 0; k < 12; ++k) ((float*)&a)[k] = r.F(2.f, false); std::memcpy(&b, &a, sizeof(CMatrix));
            const float ang = r.F(12.f, false);
            a.RotateY(ang);
            oracle::Fn<void __fastcall(void*, int, float)>(0x59B2C0)(&b, 0, ang);
            if (std::memcmp(&a, &b, 0x30)) bad++;
        }
        std::printf("RotateY mismatches: %d/2000\n", bad);
        return 0;
    }
    oracle::Patch(0x61A5A0, (void*)&H_TaskNew);
    oracle::Patch(0x465C20, (void*)&H_Gpst);
    oracle::Patch(0x6217D0, (void*)&H_cGang);
    oracle::Patch(0x6348A0, (void*)&H_cPhone);
    oracle::Patch(0x6470E0, (void*)&H_cAsDriver);
    oracle::Patch(0x646FE0, (void*)&H_cAsPass);
    oracle::Patch(0x6321F0, (void*)&H_cEffect);
    oracle::Patch(0x61F3F0, (void*)&H_cGun);
    oracle::Patch(0x65CC60, (void*)&H_cFlee);
    oracle::Patch(0x630040, (void*)&H_cDie);
    oracle::Patch(0x6817D0, (void*)&H_FindTask);
    oracle::Patch(0x64F190, (void*)&H_ComputeDoor);
    oracle::Patch(0x4684F0, (void*)&H_DmInstance);
    oracle::Patch(0x604490, (void*)&H_DmFlush);
    oracle::Patch(0x6044C0, (void*)&H_DmAdd);

    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g18_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
