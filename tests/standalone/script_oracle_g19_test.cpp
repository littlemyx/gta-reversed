// S6-E: differential test of SCRIPT COMMAND HANDLERS (group g19, ids 1900..1999) against the original machine code (exe oracle).
// Follows tests/standalone/script_oracle_g16_test.cpp: the handler TU Group19.cpp is #included here, the exe is mapped at its original addresses
// (env RW_EXE_ORACLE=<path>) and for every random case
//   (1) the exe's group processor ProcessCommands1900To1999 (0x46B460, thiscall(this = fake CRunningScript, cmd)) is run on a synthetic opcode buffer,
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then the script variables (ScriptSpace), the IP, the compare flag, the watched memory and the log of the callees are compared.
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group19.cpp"

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
namespace notsa::standalone::detail { bool g_DataImageLoaded = true; }
namespace notsa::standalone::Fixups {
void RegisterFunction(uint32_t, void*, const char*) {}
void RegisterUnverified(uint32_t, const char*, int, bool, uint32_t) {}
}
void ReversibleHooks::RHManager::AddHookToCategory(std::string_view, HookInstallOptions, std::shared_ptr<ReversibleHook::TwoWayHook>) {}
void* CMemoryMgr::Malloc(uint32 size, uint32) { return std::malloc(size); }
void* CMemoryMgr::Malloc(uint32 size) { return std::malloc(size); }
void  CMemoryMgr::Free(void* p) { std::free(p); }

uint8 CRunningScript::ScriptArgCharNextFreeBuffer = 0;
void CRunningScript::UpdateCompareFlag(bool state) { // verbatim from RunningScript.cpp
    if (m_NotFlag) state = !state;
    if (m_AndOrState == ANDOR_NONE) { m_CondResult = state; return; }
    if (m_AndOrState >= ANDS_1 && m_AndOrState <= ANDS_8) { m_CondResult &= state; m_AndOrState = (m_AndOrState == ANDS_1) ? ANDOR_NONE : m_AndOrState - 1; return; }
    if (m_AndOrState >= ORS_1 && m_AndOrState <= ORS_8) { m_CondResult |= state; m_AndOrState = (m_AndOrState == ORS_1) ? ANDOR_NONE : m_AndOrState - 1; return; }
}
// GetPointerToScriptVariable for global variables only (the test only uses `GV`)
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType) {
    const uint8_t type = *m_IP++;
    const uint16_t idx = *reinterpret_cast<const uint16_t*>(m_IP); m_IP += 2;
    (void)type;
    return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[idx]);
}

CPedPool*     GetPedPool()     { return *reinterpret_cast<CPedPool**>(0xB74490); }
CVehiclePool* GetVehiclePool() { return *reinterpret_cast<CVehiclePool**>(0xB74494); }
CObjectPool*  GetObjectPool()  { return *reinterpret_cast<CObjectPool**>(0xB7449C); }

// ---------------------------------------------------------------------------------------------------------------------------------
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static void Log(uint32_t tag) { g_log.push_back(tag); }
static void LogF(uint32_t tag, std::initializer_list<float> v) { g_log.push_back(tag); for (float f : v) g_log.push_back(FB(f)); }
static void LogU(uint32_t tag, std::initializer_list<uint32_t> v) { g_log.push_back(tag); for (auto u : v) g_log.push_back(u); }

static CVehicle* g_veh;     static int g_vehRef;
static CVehicle* g_veh2;    static int g_veh2Ref;
static CObject*  g_obj;     static int g_objRef;
static CPed*     g_ped;     static int g_pedRef;
static CMatrix*  g_mat;
static void*     g_fakeVtbl[256];
static uint8_t   g_fakeIntel[0x294];
static uint8_t   g_fakeEnt[0x100];     // an entity-like buffer that is NOT in a pool
static uint8_t   g_fakeTask[2][0x40];  // [0] primary task, [1] its sub task
static void*     g_taskVtbl[16];
static uint8_t   g_fakeMI[4][0x40];    // fake model infos
static void*     g_miVtbl[16];
static uint32_t Tag(const void* p) {
    if (!p) return 0;
    if (p == g_veh) return 1;
    if (p == g_obj) return 2;
    if (p == g_ped) return 3;
    if (p == g_veh2) return 4;
    if (p == g_fakeEnt) return 5;
    if (p == g_fakeTask[0]) return 6;
    if (p == g_fakeTask[1]) return 7;
    return 9;
}

// scenario state used by the recorders (set by the generators)
static uint32_t g_statusRet;
static void*    g_winchEnt;        // returned by QueryPickedUpEntityWithWinch / GetObjectCarriedWithRope
static void*    g_carriage;        // returned by FindCarriage
static void*    g_tag;             // returned by GetNearestTag
static float    g_ropeHeight;
static uint32_t g_taskTypeSub;
static void*    g_subTaskPtr;

// -- host recorders for the exe (cdecl / thiscall: __fastcall(this, edx, args...))
static int   __fastcall H_Fires(void* self, int, float a, float b, float c, float d, float e, float f) { LogF(40, { a, b, c, d, e, f }); return (int)(FB(a) % 5) + 1; }
static void  __cdecl    H_BlipZoom(int blip, int flag) { LogU(41, { (uint32_t)blip, (uint32_t)(flag & 1) }); }
static unsigned char __cdecl H_HasRec(int n) { LogU(42, { (uint32_t)n }); return (unsigned char)((n * 7 + 1) % 3 == 0); }
static void  __cdecl    H_ReqRec(int n) { LogU(43, { (uint32_t)n }); }
static void  __cdecl    H_RemTrain(void* t) { LogU(44, { Tag(t) }); }
static void  __cdecl    H_RelTrain(void* t) { LogU(45, { Tag(t) }); }
static void  __cdecl    H_SetTrainPos(void* t, float x, float y, float z) { LogF(46, { (float)Tag(t), x, y, z }); }
static void* __cdecl    H_FindCarriage(void* t, unsigned char i) { LogU(47, { Tag(t), i }); return i == 0 ? t : g_carriage; }
static void  __fastcall H_InitWinch(void* self, int, int a) { LogU(48, { Tag(self), (uint32_t)a }); }
static void  __fastcall H_RelWinch(void* self, int) { LogU(49, { Tag(self) }); }
static void* __fastcall H_QueryWinch(void* self, int) { LogU(50, { Tag(self) }); return g_winchEnt; }
static float __fastcall H_GetRope(void* self, int) { LogU(51, { Tag(self) }); return g_ropeHeight; }
static void  __fastcall H_SetRope(void* self, int, float h) { LogF(52, { (float)Tag(self), h }); }
static void* __fastcall H_GetCarried(void* self, int) { LogU(53, { Tag(self) }); return g_winchEnt; }
static void  __fastcall H_RelRope(void* self, int) { LogU(54, { Tag(self) }); }
static void  __fastcall H_SetAcq(void* self, int, int id, unsigned flag) { LogU(55, { (uint32_t)((uint8_t*)self - (uint8_t*)g_ped), (uint32_t)id, flag }); }
static void  __fastcall H_ClrAcq(void* self, int, int id, unsigned flag) { LogU(56, { (uint32_t)((uint8_t*)self - (uint8_t*)g_ped), (uint32_t)id, flag }); }
static void  __fastcall H_ClrAllAcq(void* self, int, int id) { LogU(57, { (uint32_t)((uint8_t*)self - (uint8_t*)g_ped), (uint32_t)id }); }
static void  __fastcall H_SetAlloc(void* self, int, int t) { LogU(58, { (uint32_t)(uintptr_t)self, (uint32_t)t }); }
static void  __fastcall H_SetDM(void* self, int, int t) { LogU(59, { (uint32_t)((uint8_t*)self - g_fakeIntel), (uint32_t)t }); }
static void  __fastcall H_CleanUpRef(void* self, int, void* ref) { LogU(60, { Tag(self), (uint32_t)((uint8_t*)ref - (uint8_t*)g_veh) }); }
static uint32_t RefTag(const void* ref);
static void  __fastcall H_RegRef(void* self, int, void* ref) { LogU(61, { Tag(self), RefTag(ref) }); }
static unsigned __cdecl H_GetExtra(unsigned k, int i) { LogU(62, { k, (uint32_t)i }); return k * 31 + (unsigned)i; }
static const char* __cdecl H_GetNameTag(unsigned k) { LogU(63, { k }); static const char* n[] = { "SHOPONE", "abcdefghij", "", "x" }; return n[k & 3]; }
static void  __cdecl    H_Buy(unsigned k, int e) { LogU(64, { k, (uint32_t)e }); }
static void  __cdecl    H_StoreClothes() { Log(65); }
static void  __cdecl    H_RestoreClothes() { Log(66); }
static void* __cdecl    H_NearestTag(const CVector* p) { LogF(67, { p->x, p->y, p->z }); return g_tag; }
static unsigned __cdecl H_GetStatus(void* ped, int op) { LogU(68, { Tag(ped), (uint32_t)op }); return g_statusRet; }
static void  __fastcall H_BreakTow(void* self, int) { LogU(69, { Tag(self) }); }
static void* __fastcall H_GetSubTask(void* self, int) { return g_subTaskPtr; }
static int   __fastcall H_GetTaskType(void* self, int) { return self == g_fakeTask[1] ? (int)g_taskTypeSub : 0x113; }
static int   __fastcall H_GetModelType(void* self, int) { return ((uint8_t*)self)[0x3F]; }
static uint32_t __fastcall H_GetRwModelType(void* self, int) { return 2; }   // rpCLUMP
static uint8_t g_fakeRwObject[16] = { 2 };

// -- the same recorders as the game functions the handlers call (C++ side)
uint32 CFireManager::GetNumFiresInArea(float a, float b, float c, float d, float e, float f) { LogF(40, { a, b, c, d, e, f }); return (int)(FB(a) % 5) + 1; }
void   CRadar::SetBlipAlwaysDisplayInZoom(tBlipHandle blip, bool flag) { LogU(41, { (uint32_t)blip, (uint32_t)flag }); }
bool   CVehicleRecording::HasRecordingFileBeenLoaded(int32 n) { LogU(42, { (uint32_t)n }); return (n * 7 + 1) % 3 == 0; }
void   CVehicleRecording::RequestRecordingFile(int32 n) { LogU(43, { (uint32_t)n }); }
void   CTrain::RemoveOneMissionTrain(CTrain* t) { LogU(44, { Tag(t) }); }
void   CTrain::ReleaseOneMissionTrain(CTrain* t) { LogU(45, { Tag(t) }); }
void   CTrain::SetNewTrainPosition(CTrain* t, CVector p) { LogF(46, { (float)Tag(t), p.x, p.y, p.z }); }
CTrain* CTrain::FindCarriage(CTrain* t, uint8 i) { LogU(47, { Tag(t), i }); return i == 0 ? t : (CTrain*)g_carriage; }
void     CVehicle::InitWinch(int32 a) { LogU(48, { Tag(this), (uint32_t)a }); }
void     CVehicle::ReleasePickedUpEntityWithWinch() const { LogU(49, { Tag(this) }); }
CEntity* CVehicle::QueryPickedUpEntityWithWinch() const { LogU(50, { Tag(this) }); return (CEntity*)g_winchEnt; }
float    CObject::GetRopeHeight() { LogU(51, { Tag(this) }); return g_ropeHeight; }
void     CObject::SetRopeHeight(float h) { LogF(52, { (float)Tag(this), h }); }
CEntity* CObject::GetObjectCarriedWithRope() { LogU(53, { Tag(this) }); return (CEntity*)g_winchEnt; }
void     CObject::ReleaseObjectCarriedWithRope() { LogU(54, { Tag(this) }); }
void CAcquaintance::SetAsAcquaintance(AcquaintanceId id, uint32 flag) { LogU(55, { (uint32_t)((uint8_t*)this - (uint8_t*)g_ped), (uint32_t)id, flag }); }
void CAcquaintance::ClearAsAcquaintance(AcquaintanceId id, uint32 flag) { LogU(56, { (uint32_t)((uint8_t*)this - (uint8_t*)g_ped), (uint32_t)id, flag }); }
void CAcquaintance::ClearAcquaintances(AcquaintanceId id) { LogU(57, { (uint32_t)((uint8_t*)this - (uint8_t*)g_ped), (uint32_t)id }); }
void CPedGroupIntelligence::SetDefaultTaskAllocatorType(ePedGroupDefaultTaskAllocatorType t) { LogU(58, { (uint32_t)(uintptr_t)this, (uint32_t)t }); }
void CPedIntelligence::SetPedDecisionMakerType(int32 t) { LogU(59, { (uint32_t)((uint8_t*)this - g_fakeIntel), (uint32_t)t }); }
void CEntity::CleanUpOldReference(CEntity** ref) { LogU(60, { Tag(this), (uint32_t)((uint8_t*)ref - (uint8_t*)g_veh) }); }
void CEntity::RegisterReference(CEntity** ref) { LogU(61, { Tag(this), RefTag(ref) }); }
int32 CShopping::GetExtraInfo(uint32 k, int32 i) { LogU(62, { k, (uint32_t)i }); return (int32)(k * 31 + (unsigned)i); }
const char* CShopping::GetNameTag(uint32 k) { LogU(63, { k }); static const char* n[] = { "SHOPONE", "abcdefghij", "", "x" }; return n[k & 3]; }
void CShopping::Buy(uint32 k, int32 e) { LogU(64, { k, (uint32_t)e }); }
void CShopping::StoreClothesState() { Log(65); }
void CShopping::RestoreClothesState() { Log(66); }
CEntity* CTagManager::GetNearestTag(const CVector& p) { LogF(67, { p.x, p.y, p.z }); return (CEntity*)g_tag; }
eScriptedTaskStatus CPedScriptedTaskRecord::GetStatus(CPed* ped, int32 op) { LogU(68, { Tag(ped), (uint32_t)op }); return (eScriptedTaskStatus)g_statusRet; }

// the exe's own pure functions, used by the port as callees
int32 CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { return oracle::Fn<int __cdecl(int, int)>(0x4839A0)(ref, (int)type); }
float CAutomobile::GetCarPitch() { return oracle::Fn<float __fastcall(void*, int)>(0x6A6050)(this, 0); }
CWeaponInfo* CWeaponInfo::GetWeaponInfo(eWeaponType t, eWeaponSkill s) { return oracle::Fn<CWeaponInfo* __cdecl(int, int)>(0x743C60)((int)t, (int)s); }
uint32 CPedType::GetPedFlag(ePedType t) { return oracle::Fn<unsigned __cdecl(int)>(0x608830)((int)t); }
char* MakeUpperCase(char* s) { return oracle::Fn<char* __cdecl(char*)>(0x718710)(s); }
bool CSearchLight::IsPointInsideLitEllipse(const CVector& p, int32 i) { return oracle::Fn<unsigned char __cdecl(const CVector*, int)>(0x493280)(&p, i) != 0; }
static_assert(offsetof(CAudioEngine, m_BeatInfo) == 8);
tBeatInfo* CAudioEngine::GetBeatInfo() { return &m_BeatInfo; }   // the exe's version also asks AEAudioHardware (not initialised here); patched to the same
static void* __fastcall H_GetBeatInfo(void* self, int) { return (uint8_t*)self + 8; }
CAudioEngine& AudioEngine = *reinterpret_cast<CAudioEngine*>(0xB6BC90);

// ---------------------------------------------------------------------------------------------------------------------------------

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
    Op& S8(const char* s) { b.push_back(9); for (int i = 0; i < 8; ++i) b.push_back(i < (int)std::strlen(s) ? (uint8_t)s[i] : 0); return *this; }   // static 8-byte text label
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
// S6-D review additions: task constructor recorders (see script_oracle_g18_test.cpp for the idea). Tasks whose constructors are inline in the
// headers (CTaskComplexSeekEntityStandard, CTaskComplexSequence) run their real code on the port side and are compared by memory hash against
// the exe's real constructors.
static std::vector<void*>  g_allocs;
static std::vector<size_t> g_allocSz;
static uint32_t TaskOrd(const void* p) {
    if (!p) return 0;
    for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == p) return (uint32_t)i + 1;
    return 0xF000;
}
static uint32_t TaskHash(const void* p) {
    for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == p) {
        uint32_t h = 2166136261u;
        for (size_t k = 4; k < g_allocSz[i]; ++k) {
            uint32_t w = 0; if (k % 4 == 0 && k + 4 <= g_allocSz[i]) std::memcpy(&w, (const uint8_t*)p + k, 4);
            if (w >= 0x400000u && w < 0x1100000u) { k += 3; h = (h ^ 0xAAu) * 16777619u; continue; }   // an embedded vtable pointer (exe image vs this test's image): not compared
            h = (h ^ ((const uint8_t*)p)[k]) * 16777619u;
        }
        return h;
    }
    return 0;
}
static uint32_t RefTag(const void* ref) {   // a reference slot inside a recorded task: (task ordinal << 16) | offset; otherwise the offset from the fake vehicle
    for (size_t i = 0; i < g_allocs.size(); ++i) if ((const uint8_t*)ref >= (const uint8_t*)g_allocs[i] && (const uint8_t*)ref < (const uint8_t*)g_allocs[i] + g_allocSz[i]) return (uint32_t)((i + 1) << 16) | (uint32_t)((const uint8_t*)ref - (const uint8_t*)g_allocs[i]);
    return (uint32_t)((const uint8_t*)ref - (const uint8_t*)g_veh);
}
static void LogV3(const CVector* v) { if (v) LogF(0, { v->x, v->y, v->z }); else LogF(0, { 0.f, 0.f, 0.f }); }
static void Zero4(void* p, size_t n) { std::memset(reinterpret_cast<uint8_t*>(p) + 4, 0, n - 4); }
static void* __cdecl H_TaskNew(size_t n) { void* p = std::calloc(1, n); g_allocs.push_back(p); g_allocSz.push_back(n); LogU(140, { (uint32_t)n }); return p; }
void* CTask::operator new(size_t n) { return H_TaskNew(n); }
void  CTask::operator delete(void*) {}
static void __fastcall H_Gpst(CRunningScript*, int, int h, void* task, int cmd) {
    if (std::getenv("DUMPT")) { std::printf("   task %u:", TaskOrd(task)); for (size_t i = 0; i < g_allocs.size(); ++i) if (g_allocs[i] == task) for (size_t k = 0; k < g_allocSz[i]; ++k) std::printf("%s%02x", k % 4 ? "" : " ", ((uint8_t*)task)[k]); std::printf("\n"); } LogU(141, { (uint32_t)h, TaskOrd(task), TaskHash(task), (uint32_t)cmd }); }
void CRunningScript::GivePedScriptedTask(int32 h, CTask* t, int32 cmd) { H_Gpst(this, 0, h, t, cmd); }
static void __fastcall H_AddTask(void* self, int, void* task) { LogU(142, { TaskOrd(self), TaskOrd(task), TaskHash(task) }); }
void CTaskComplexSequence::AddTask(CTask* t) { H_AddTask(this, 0, t); }

static void* __fastcall H_cClimb(void* self, int) { LogU(180, { TaskOrd(self) }); return self; }
CTaskComplexJump::CTaskComplexJump(eForceClimb) { LogU(180, { TaskOrd(this) }); }   // CTaskComplexClimb's only effect in the ctor is the base ctor with FORCE
static void* __fastcall H_cUseSeq(void* self, int, int idx) { LogU(181, { TaskOrd(self), (uint32_t)idx }); return self; }
CTaskComplexUseSequence::CTaskComplexUseSequence(int32 idx) { LogU(181, { TaskOrd(this), (uint32_t)idx }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cSeekAim(void* self, int, void* e, float a, float b) { LogF(182, { (float)TaskOrd(self), (float)Tag(e), a, b }); return self; }
CTaskComplexSeekEntityAiming::CTaskComplexSeekEntityAiming(CEntity* e, float a, float b) { LogF(182, { (float)TaskOrd(this), (float)Tag(e), a, b }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cKill(void* self, int, void* t, int time, int flags, int delay, int chance, int comp) { LogU(183, { TaskOrd(self), Tag(t), (uint32_t)time, (uint32_t)flags, (uint32_t)delay, (uint32_t)chance, (uint8_t)comp }); return self; }
CTaskComplexKillPedOnFoot::CTaskComplexKillPedOnFoot(CPed* t, int32 time, int32 flags, int32 delay, int32 chance, uint8 comp, bool, bool) { LogU(183, { TaskOrd(this), Tag(t), (uint32_t)time, (uint32_t)flags, (uint32_t)delay, (uint32_t)chance, (uint8_t)comp }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cJet(void* self, int, const CVector* pos, float h, int hover, void* e) { LogF(184, { (float)TaskOrd(self), h, (float)hover, (float)Tag(e) }); LogV3(pos); LogU(0, { pos ? 1u : 0u }); return self; }
CTaskSimpleJetPack::CTaskSimpleJetPack(const CVector* pos, float h, int32 hover, CEntity* e) { LogF(184, { (float)TaskOrd(this), h, (float)hover, (float)Tag(e) }); LogV3(pos); LogU(0, { pos ? 1u : 0u }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cPickUp(void* self, int, void* e, int grp) { LogU(185, { TaskOrd(self), Tag(e), (uint32_t)grp }); return self; }
CTaskComplexGoPickUpEntity::CTaskComplexGoPickUpEntity(CEntity* e, AssocGroupId grp) { LogU(185, { TaskOrd(this), Tag(e), (uint32_t)grp }); Zero4(this, sizeof(*this)); }
static void* __fastcall H_cGoStand(void* self, int, int ms, const CVector* pos, float r, float s, int a, int b) { LogF(186, { (float)TaskOrd(self), (float)ms, r, s, (float)(uint8_t)a, (float)(uint8_t)b }); LogV3(pos); return self; }
static void* __fastcall H_cSlide(void* self, int, const CVector* pos, float ang, float spd) { LogF(187, { (float)TaskOrd(self), ang, spd }); LogV3(pos); return self; }
static void* __fastcall H_cSetDm(void* self, int, int dm) { LogU(188, { TaskOrd(self), (uint32_t)dm }); return self; }
CTaskSimpleSetCharDecisionMaker::CTaskSimpleSetCharDecisionMaker(uint32 dm) { LogU(188, { TaskOrd(this), dm }); Zero4(this, sizeof(*this)); }

// -- IPL store (1910-1912)
static int  __cdecl H_FindIpl(const char* n) { LogU(190, { 0 }); for (int i = 0; i < 8 && n[i]; ++i) g_log.push_back((uint8_t)n[i]); g_log.push_back(0); return (int)(n[0] % 7) - 2; }
static void __cdecl H_ReqIpl(int s) { LogU(191, { (uint32_t)s }); }
static void __cdecl H_RemIpl(int s) { LogU(192, { (uint32_t)s }); }
static void __cdecl H_RemIplFar(int s) { LogU(193, { (uint32_t)s }); }
int32 CIplStore::FindIplSlot(const char* n) { return H_FindIpl(n); }
void  CIplStore::RequestIplAndIgnore(int32 s) { H_ReqIpl(s); }
void  CIplStore::RemoveIplAndIgnore(int32 s) { H_RemIpl(s); }
void  CIplStore::RemoveIplWhenFarAway(int32 s) { H_RemIplFar(s); }
void  CRunningScript::ReadTextLabelFromScript(char* b, uint8 n) { oracle::Fn<void __fastcall(void*, int, char*, int)>(0x463D50)(this, 0, b, n); }
static uint32_t g_caseSeed;
static void PreRun() { g_allocs.clear(); g_allocSz.clear(); }
CTaskComplexGoToPointAndStandStill::CTaskComplexGoToPointAndStandStill(eMoveState ms, const CVector& pos, float r, float s, bool a, bool b, bool) { H_cGoStand(this, 0, ms, &pos, r, s, a, b); Zero4(this, sizeof(*this)); }
CTaskSimpleAnim::CTaskSimpleAnim(bool) {}
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim() : CTaskSimpleAnim(false) {}   // base ctors of the recorded SlideToCoord ctor below
CTaskSimpleSlideToCoord::CTaskSimpleSlideToCoord(const CVector& pos, float ang, float spd) { H_cSlide(this, 0, &pos, ang, spd); Zero4(this, sizeof(*this)); }

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
    constexpr unsigned groupFn = 0x46B460; // ProcessCommands1900To1999
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
                if (a.mem[i] != b.mem[i]) { std::snprintf(t, sizeof(t), " first mem diff at +0x%zx: exe %02x port %02x [var0 exe %08x port %08x]", i, a.mem[i], b.mem[i], *(uint32_t*)&a.mem[0], *(uint32_t*)&b.mem[0]); desc += t; break; }
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
static int32_t Flag(Rng& r) { return r.below(3) == 0 ? 0 : r.below(2) ? 1 : (int32_t)r.u32(); }
static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }
static void SetType(void* e, int t) { auto* b = (uint8_t*)e + 0x36; *b = (uint8_t)((*b & ~7) | t); }

static void RandomVehicle(CVehicle* v, Rng& r, int type = 2) {
    RandomFill(v, sizeof(CHeli), r);
    RandomFill(g_mat, sizeof(CMatrix), r);
    *reinterpret_cast<void**>(v) = g_fakeVtbl;
    v->m_matrix = reinterpret_cast<decltype(v->m_matrix)>(g_mat);
    SetType(v, type);
}
static void RandomObject(Rng& r) {
    RandomFill(g_obj, sizeof(CObject), r);
    *reinterpret_cast<void**>(g_obj) = g_fakeVtbl;
    g_obj->m_matrix = r.below(2) ? nullptr : reinterpret_cast<decltype(g_obj->m_matrix)>(g_mat);
    SetType(g_obj, 4);
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
    g_veh2 = vp->New(); g_veh2Ref = vp->GetRef(g_veh2);
    g_obj = op->New();  g_objRef = op->GetRef(g_obj);
    g_mat = new CMatrix();
    std::memset(g_ped, 0, sizeof(CPed));
    reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C)[0] = g_fakeIntel;
    g_fakeVtbl[0xF8 / 4] = (void*)&H_BreakTow;           // CVehicle::BreakTowLink -- slot 62
    g_taskVtbl[2] = (void*)&H_GetSubTask;                // CTask::GetSubTask -- slot 2
    g_taskVtbl[4] = (void*)&H_GetTaskType;               // CTask::GetTaskType -- slot 4
    *reinterpret_cast<void**>(g_fakeTask[0]) = g_taskVtbl;
    *reinterpret_cast<void**>(g_fakeTask[1]) = g_taskVtbl;
    g_miVtbl[9] = (void*)&H_GetRwModelType;              // CBaseModelInfo::GetRwModelType -- slot 9 (debug assertion of GetRwObject)
    g_miVtbl[4] = (void*)&H_GetModelType;                // CBaseModelInfo::GetModelType -- slot 4
    for (auto& mi : g_fakeMI) *reinterpret_cast<void**>(mi) = g_miVtbl;
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestAll() {
    using Gen = std::function<void(Ctx&)>;
    auto VehWatch = [](Ctx& c, CVehicle* v = g_veh) { c.watch.push_back({ v, sizeof(CHeli) }); };
    auto Pick = [](Rng& r, std::initializer_list<int32_t> l) { return *(l.begin() + r.below((int)l.size())); };
    auto Stale = [](Rng& r) -> int32_t { return (int32_t)((r.below(4) << 8) | r.below(256)); };   // a stale handle (pool slot in range, wrong flags): the exe's GetAtRef has no bounds check
    auto VehH = [&](Rng& r) -> int32_t { switch (r.below(8)) { case 0: return -1; case 1: return g_veh2Ref; case 2: return Stale(r); default: return g_vehRef; } };
    auto PedH = [&](Rng& r) -> int32_t { switch (r.below(6)) { case 0: return -1; case 1: return Stale(r); default: return g_pedRef; } };

    // ---- plain data
    Test<COMMAND_IS_MESSAGE_BEING_DISPLAYED, &IsMessageBeingDisplayed>("1903 IS_MESSAGE_BEING_DISPLAYED", [&](Ctx& c) {
        RandomFill((void*)0xC1A7F0, 8, *c.rng); if (c.rng->below(2)) *(uint32_t*)0xC1A7F0 = 0; c.watch.push_back({ (void*)0xC1A7F0, 8 }); c.opcode = Op(1903).b; });
    Test<COMMAND_SET_CHAR_IS_TARGET_PRIORITY, &SetCharIsTargetPriority>("1904 SET_CHAR_IS_TARGET_PRIORITY", [&](Ctx& c) {
        RandomFill(g_ped, 0x500, *c.rng); reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C)[0] = g_fakeIntel; c.watch.push_back({ g_ped, 0x500 });
        c.opcode = Op(1904).I(g_pedRef).I(Pick(*c.rng, { 0, 1, 2, -1, 0x100, (int32_t)c.rng->u32() })).b; });
    Test<COMMAND_LISTEN_TO_PLAYER_GROUP_COMMANDS, &ListenToPlayerGroupCommands>("1995 LISTEN_TO_PLAYER_GROUP_COMMANDS", [&](Ctx& c) {
        RandomFill(g_ped, 0x500, *c.rng); reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C)[0] = g_fakeIntel; c.watch.push_back({ g_ped, 0x500 });
        c.opcode = Op(1995).I(g_pedRef).I(Pick(*c.rng, { 0, 1, 2, -1, 0x100, (int32_t)c.rng->u32() })).b; });
    Test<COMMAND_GET_AREA_VISIBLE, &GetAreaVisible>("1918 GET_AREA_VISIBLE", [&](Ctx& c) {
        *(uint32_t*)0xB72914 = c.rng->u32(); c.watch.push_back({ (void*)0xB72914, 4 }); c.opcode = Op(1918).GV(0).b; });
    Test<COMMAND_SET_NEXT_DESIRED_MOVE_STATE, &SetNextDesiredMoveState>("1953 SET_NEXT_DESIRED_MOVE_STATE", [&](Ctx& c) {
        *(uint32_t*)0x8D237C = c.rng->u32(); c.watch.push_back({ (void*)0x8D237C, 4 }); c.opcode = Op(1953).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_AREA51_SAM_SITE, &SetArea51SamSite>("1960 SET_AREA51_SAM_SITE", [&](Ctx& c) {
        *(uint8_t*)0xBB4A72 = (uint8_t)c.rng->u32(); c.watch.push_back({ (void*)0xBB4A72, 4 }); c.opcode = Op(1960).I(Pick(*c.rng, { 0, 1, 2, -1, (int32_t)c.rng->u32() })).b; });

    // ---- float / area
    Test<COMMAND_GET_NUMBER_OF_FIRES_IN_AREA, &GetNumberOfFiresInArea>("1926 GET_NUMBER_OF_FIRES_IN_AREA", [&](Ctx& c) {
        auto& r = *c.rng; const float k = r.below(3) ? 100.f : 1e6f;
        c.opcode = Op(1926).Fl(r.F(k)).Fl(r.F(k)).Fl(r.F(k)).Fl(r.F(k)).Fl(r.F(k)).Fl(r.F(k)).GV(0).b; });
    Test<COMMAND_GET_CAR_PITCH, &GetCarPitchScript>("1917 GET_CAR_PITCH", [&](Ctx& c) {
        RandomVehicle(g_veh, *c.rng); auto* m = (float*)g_mat; if (c.rng->below(3)) { for (int i = 0; i < 16; ++i) m[i] = c.rng->F(1.5f, false); } VehWatch(c);
        c.watch.push_back({ g_mat, sizeof(CMatrix) }); c.opcode = Op(1917).I(g_vehRef).GV(0).b; });
    Test<COMMAND_GET_WEAPONTYPE_MODEL, &GetWeapontypeModel>("1921 GET_WEAPONTYPE_MODEL", [&](Ctx& c) {
        c.opcode = Op(1921).I(c.rng->below(60) - 2).GV(0).b; });
    Test<COMMAND_GET_WEAPONTYPE_SLOT, &GetWeapontypeSlot>("1922 GET_WEAPONTYPE_SLOT", [&](Ctx& c) {
        c.opcode = Op(1922).I(c.rng->below(60) - 2).GV(0).b; });
    Test<COMMAND_SET_BLIP_ALWAYS_DISPLAY_ON_ZOOMED_RADAR, &SetBlipAlwaysDisplayOnZoomedRadar>("1983 SET_BLIP_ALWAYS_DISPLAY_ON_ZOOMED_RADAR", [&](Ctx& c) {
        c.opcode = Op(1983).I((int32_t)c.rng->u32()).I(Pick(*c.rng, { 0, 1, 2, 3, 0x100, 0x101, -1, (int32_t)c.rng->u32() })).b; });

    // ---- recordings / shopping
    Test<COMMAND_REQUEST_CAR_RECORDING, &RequestCarRecording>("1984 REQUEST_CAR_RECORDING", [&](Ctx& c) { c.opcode = Op(1984).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_HAS_CAR_RECORDING_BEEN_LOADED, &HasCarRecordingBeenLoaded>("1985 HAS_CAR_RECORDING_BEEN_LOADED", [&](Ctx& c) { c.opcode = Op(1985).I((int32_t)c.rng->below(40)).b; });
    Test<COMMAND_GET_SHOPPING_EXTRA_INFO, &GetShoppingExtraInfo>("1923 GET_SHOPPING_EXTRA_INFO", [&](Ctx& c) { c.opcode = Op(1923).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).GV(0).b; });
    Test<COMMAND_BUY_ITEM, &BuyItem>("1936 BUY_ITEM", [&](Ctx& c) { c.opcode = Op(1936).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_STORE_CLOTHES_STATE, &StoreClothesState>("1939 STORE_CLOTHES_STATE", [&](Ctx& c) { c.opcode = Op(1939).b; });
    Test<COMMAND_RESTORE_CLOTHES_STATE, &RestoreClothesState>("1940 RESTORE_CLOTHES_STATE", [&](Ctx& c) { c.opcode = Op(1940).b; });
    Test<COMMAND_GET_NAME_OF_ITEM, &GetNameOfItem>("1932 GET_NAME_OF_ITEM", [&](Ctx& c) {
        RandomFill(&ScriptSpaceRef()[0x40], 16, *c.rng); c.opcode = Op(1932).I((int32_t)c.rng->u32()).GV(0x40).b; });
    Test<COMMAND_GET_LOADED_SHOP, &GetLoadedShop>("1968 GET_LOADED_SHOP", [&](Ctx& c) {
        auto& r = *c.rng; char* s = (char*)0xA9A7D8; RandomFill(s, 24, r);
        const int n = r.below(8); for (int i = 0; i < n; ++i) s[i] = (char)(0x20 + r.below(0x5F)); s[n] = 0;   // a NUL inside the 8 copied chars (the exe's temp buffer has no terminator slot)
        RandomFill(&ScriptSpaceRef()[0x40], 16, r); c.watch.push_back({ s, 24 }); c.opcode = Op(1968).GV(0x40).b; });
    Test<COMMAND_GET_NEAREST_TAG_POSITION, &GetNearestTagPosition>("1958 GET_NEAREST_TAG_POSITION", [&](Ctx& c) {
        auto& r = *c.rng; RandomObject(r); c.watch.push_back({ g_obj, sizeof(CObject) }); c.watch.push_back({ g_mat, sizeof(CMatrix) });
        float* m = (float*)g_mat; for (int i = 0; i < 16; ++i) m[i] = r.F(500.f, false);
        g_tag = r.below(3) == 0 ? nullptr : (void*)g_obj;
        c.opcode = Op(1958).Fl(r.F(100.f)).Fl(r.F(100.f)).Fl(r.F(100.f)).GV(0).GV(4).GV(8).b; });

    // ---- trains / winch / rope
    Test<COMMAND_DELETE_MISSION_TRAIN, &DeleteMissionTrain>("1981 DELETE_MISSION_TRAIN", [&](Ctx& c) { c.opcode = Op(1981).I(VehH(*c.rng)).b; });
    Test<COMMAND_MARK_MISSION_TRAIN_AS_NO_LONGER_NEEDED, &MarkMissionTrainAsNoLongerNeeded>("1982 MARK_MISSION_TRAIN_AS_NO_LONGER_NEEDED", [&](Ctx& c) { c.opcode = Op(1982).I(VehH(*c.rng)).b; });
    Test<COMMAND_SET_MISSION_TRAIN_COORDINATES, &SetMissionTrainCoordinates>("1991 SET_MISSION_TRAIN_COORDINATES", [&](Ctx& c) {
        c.opcode = Op(1991).I(VehH(*c.rng)).Fl(c.rng->F(1000.f)).Fl(c.rng->F(1000.f)).Fl(c.rng->F(1000.f)).b; });
    Test<COMMAND_GET_TRAIN_CARRIAGE, &GetTrainCarriage>("1930 GET_TRAIN_CARRIAGE", [&](Ctx& c) {
        auto& r = *c.rng; g_carriage = Pick(r, { 0, 0, 1 }) ? (void*)g_veh2 : nullptr;
        c.opcode = Op(1930).I(VehH(r)).I(Pick(r, { 0, 1, 2, 5, 255, 256, 257, -1 })).GV(0).b; });
    auto Entities = [&](Rng& r) {   // what hangs on the winch / rope
        RandomVehicle(g_veh, r, 2); RandomVehicle(g_veh2, r, 2); RandomObject(r); RandomFill(g_fakeEnt, sizeof(g_fakeEnt), r);
        std::memset(g_ped, 0, 0x100); SetType(g_ped, 3);
        switch (r.below(6)) { case 0: g_winchEnt = nullptr; break; case 1: g_winchEnt = g_veh; break; case 2: g_winchEnt = g_ped; break; case 3: g_winchEnt = g_obj; break;
                              case 4: g_winchEnt = g_veh2; break; default: SetType(g_fakeEnt, Pick(r, { 0, 1, 5, 6, 7 })); g_winchEnt = g_fakeEnt; }
    };
    Test<COMMAND_GRAB_ENTITY_ON_WINCH, &GrabEntityOnWinch>("1931 GRAB_ENTITY_ON_WINCH", [&](Ctx& c) {
        Entities(*c.rng); c.opcode = Op(1931).I(g_vehRef).GV(0).GV(4).GV(8).b; });
    Test<COMMAND_GRAB_ENTITY_ON_ROPE_FOR_OBJECT, &GrabEntityOnRopeForObject>("1944 GRAB_ENTITY_ON_ROPE_FOR_OBJECT", [&](Ctx& c) {
        Entities(*c.rng); c.opcode = Op(1944).I(g_objRef).GV(0).GV(4).GV(8).b; });
    Test<COMMAND_ATTACH_WINCH_TO_HELI, &AttachWinchToHeli>("1928 ATTACH_WINCH_TO_HELI", [&](Ctx& c) { c.opcode = Op(1928).I(g_vehRef).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_RELEASE_ENTITY_FROM_WINCH, &ReleaseEntityFromWinch>("1929 RELEASE_ENTITY_FROM_WINCH", [&](Ctx& c) { c.opcode = Op(1929).I(g_vehRef).b; });
    Test<COMMAND_GET_ROPE_HEIGHT_FOR_OBJECT, &GetRopeHeightForObject>("1942 GET_ROPE_HEIGHT_FOR_OBJECT", [&](Ctx& c) { g_ropeHeight = c.rng->F(100.f); c.opcode = Op(1942).I(g_objRef).GV(0).b; });
    Test<COMMAND_SET_ROPE_HEIGHT_FOR_OBJECT, &SetRopeHeightForObject>("1943 SET_ROPE_HEIGHT_FOR_OBJECT", [&](Ctx& c) { c.opcode = Op(1943).I(g_objRef).Fl(c.rng->F(100.f)).b; });
    Test<COMMAND_RELEASE_ENTITY_FROM_ROPE_FOR_OBJECT, &ReleaseEntityFromRopeForObject>("1945 RELEASE_ENTITY_FROM_ROPE_FOR_OBJECT", [&](Ctx& c) { c.opcode = Op(1945).I(g_objRef).b; });
    Test<COMMAND_IS_TRAILER_ATTACHED_TO_CAB, &IsTrailerAttachedToCab>("1963 IS_TRAILER_ATTACHED_TO_CAB", [&](Ctx& c) {
        auto& r = *c.rng; RandomVehicle(g_veh, r); RandomVehicle(g_veh2, r); VehWatch(c); VehWatch(c, g_veh2);
        for (auto* v : { g_veh, g_veh2 }) { v->m_pTowingVehicle = Pick(r, { 0, 1, 2 }) == 0 ? nullptr : (r.below(2) ? g_veh : g_veh2); v->m_pVehicleBeingTowed = r.below(3) == 0 ? nullptr : (r.below(2) ? g_veh : g_veh2); }
        int32_t a = Pick(r, { g_vehRef, g_veh2Ref, -1 }), b = Pick(r, { g_vehRef, g_veh2Ref, -1 }); if (a == -1 && b == -1) b = g_veh2Ref;   // both null dereferences null in both
        c.opcode = Op(1963).I(a).I(b).b; });
    Test<COMMAND_DETACH_TRAILER_FROM_CAB, &DetachTrailerFromCab>("1964 DETACH_TRAILER_FROM_CAB", [&](Ctx& c) {
        auto& r = *c.rng; RandomVehicle(g_veh, r); RandomVehicle(g_veh2, r); VehWatch(c); VehWatch(c, g_veh2);
        for (auto* v : { g_veh, g_veh2 }) { v->m_pTowingVehicle = r.below(3) == 0 ? nullptr : (r.below(2) ? g_veh : g_veh2); v->m_pVehicleBeingTowed = r.below(3) == 0 ? nullptr : (r.below(2) ? g_veh : g_veh2); }
        int32_t a = Pick(r, { g_vehRef, g_veh2Ref, -1 }), b = Pick(r, { g_vehRef, g_veh2Ref, -1 }); if (a == -1 && b == -1) b = g_veh2Ref;
        c.opcode = Op(1964).I(a).I(b).b; });
    Test<COMMAND_ACTIVATE_HELI_SPEED_CHEAT, &ActivateHeliSpeedCheat>("1979 ACTIVATE_HELI_SPEED_CHEAT", [&](Ctx& c) {
        RandomVehicle(g_veh, *c.rng); VehWatch(c); c.opcode = Op(1979).I(g_vehRef).I(Pick(*c.rng, { 0, 1, 255, 256, -1, (int32_t)c.rng->u32() })).b; });
    Test<COMMAND_HELI_KEEP_ENTITY_IN_VIEW, &HeliKeepEntityInView>("1920 HELI_KEEP_ENTITY_IN_VIEW", [&](Ctx& c) {
        auto& r = *c.rng; RandomVehicle(g_veh, r); RandomVehicle(g_veh2, r); VehWatch(c);
        auto& ap = g_veh->m_autoPilot; ap.m_nCarMission = (eCarMission)Pick(r, { 0x39, 0x3A, 0x33, 7, (int32_t)r.below(256) });
        ap.m_TargetEntity = r.below(3) == 0 ? nullptr : (r.below(2) ? g_veh2 : (CVehicle*)g_ped);
        int32_t ph = Pick(r, { g_pedRef, g_pedRef, -1, Stale(r) }), vh = Pick(r, { g_veh2Ref, -1, -1, Stale(r), g_vehRef });
        if ((ph < 0 || ph != g_pedRef) && (vh < 0 || (vh != g_veh2Ref && vh != g_vehRef))) vh = g_veh2Ref;   // a null target crashes in both
        const float a = r.below(5) == 0 ? r.F(1e10f) : r.F(400.f);
        c.opcode = Op(1920).I(g_vehRef).I(ph).I(vh).Fl(a).Fl(r.F(1000.f)).b; });

    // ---- peds
    Test<COMMAND_SET_CHAR_RELATIONSHIP, &SetCharRelationship>("1914 SET_CHAR_RELATIONSHIP", [&](Ctx& c) {
        c.opcode = Op(1914).I(g_pedRef).I((int32_t)c.rng->u32()).I(Pick(*c.rng, { 0, 1, 5, 31, 32, -1, 12345 })).b; });
    Test<COMMAND_CLEAR_CHAR_RELATIONSHIP, &ClearCharRelationship>("1915 CLEAR_CHAR_RELATIONSHIP", [&](Ctx& c) {
        c.opcode = Op(1915).I(g_pedRef).I((int32_t)c.rng->u32()).I(Pick(*c.rng, { 0, 1, 5, 31, 32, -1, 12345 })).b; });
    Test<COMMAND_CLEAR_ALL_CHAR_RELATIONSHIPS, &ClearAllCharRelationships>("1916 CLEAR_ALL_CHAR_RELATIONSHIPS", [&](Ctx& c) {
        c.opcode = Op(1916).I(g_pedRef).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_TASK_SET_CHAR_DECISION_MAKER, &TaskSetCharDecisionMaker>("1980 TASK_SET_CHAR_DECISION_MAKER (char path)", [&](Ctx& c) {
        auto& r = *c.rng; RandomFill(g_fakeIntel, sizeof(g_fakeIntel), r); c.watch.push_back({ g_fakeIntel, sizeof(g_fakeIntel) });
        for (int i = 0; i < 20; ++i) { ((uint16_t*)0xC0AFF4)[i] = (uint16_t)r.below(5); ((uint8_t*)0xC0B01C)[i] = (uint8_t)r.below(2); }
        c.opcode = Op(1980).I(g_pedRef).I(Pick(r, { -1, 0, 1, 7, 19, 20, (int32_t)(r.below(25) | (r.below(5) << 16)), (int32_t)(r.below(25) | (r.below(5) << 16)) })).b; });
    Test<COMMAND_SET_GROUP_DEFAULT_TASK_ALLOCATOR, &SetGroupDefaultTaskAllocator>("1971 SET_GROUP_DEFAULT_TASK_ALLOCATOR", [&](Ctx& c) {
        auto& r = *c.rng; c.opcode = Op(1971).I(Pick(r, { 0, 1, 2, 7, 8, -1, (int32_t)(r.below(10) | (r.below(5) << 16)), (int32_t)(r.below(10) | (r.below(5) << 16)) })).I((int32_t)r.u32()).b; });
    Test<COMMAND_GET_SEQUENCE_PROGRESS_RECURSIVE, &GetSequenceProgressRecursive>("1956 GET_SEQUENCE_PROGRESS_RECURSIVE", [&](Ctx& c) {
        auto& r = *c.rng; RandomFill(g_fakeTask, sizeof(g_fakeTask), r);
        *reinterpret_cast<void**>(g_fakeTask[0]) = g_taskVtbl; *reinterpret_cast<void**>(g_fakeTask[1]) = g_taskVtbl;
        RandomFill(g_fakeIntel, sizeof(g_fakeIntel), r); *reinterpret_cast<void**>(g_fakeIntel + 0x10) = g_fakeTask[0];
        g_statusRet = Pick(r, { 0, 1, 2, 3, -1 }); g_subTaskPtr = r.below(3) ? (void*)g_fakeTask[1] : nullptr; g_taskTypeSub = r.below(2) ? 0x113 : r.below(500);
        c.watch.push_back({ g_fakeTask, sizeof(g_fakeTask) }); c.watch.push_back({ g_fakeIntel, sizeof(g_fakeIntel) });
        c.opcode = Op(1956).I(g_pedRef).GV(0).GV(4).b; });
    Test<COMMAND_IS_CHAR_IN_ANY_SEARCHLIGHT, &IsCharInAnySearchlight>("1961 IS_CHAR_IN_ANY_SEARCHLIGHT", [&](Ctx& c) {
        auto& r = *c.rng; RandomObject(r); (void)0;
        // 8 lights: used flag, id, the target spot and the two axis vectors; the ped sits near the spots
        auto* L = (uint8_t*)0xA94D68; for (int i = 0; i < 8; ++i) {
            uint8_t* l = L + i * 0x7C; RandomFill(l, 0x7C, r); l[0] = (uint8_t)(r.below(3) != 0);
            float* ts = (float*)(l + 0x58); float* a = (float*)(l + 0x64); float* b = (float*)(l + 0x70);
            ts[0] = r.F(50.f, false); ts[1] = r.F(50.f, false); ts[2] = r.F(5.f, false);
            for (int k = 0; k < 3; ++k) { a[k] = r.F(r.below(4) ? 20.f : 1e-3f, r.below(8) == 0); b[k] = r.F(20.f, r.below(8) == 0); }
        }
        c.watch.push_back({ L, 8 * 0x7C });
        RandomFill(g_ped, 0x500, r); reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C)[0] = g_fakeIntel; c.watch.push_back({ g_ped, 0x500 }); c.watch.push_back({ g_mat, sizeof(CMatrix) });
        float* m = (float*)g_mat; for (int i = 0; i < 16; ++i) m[i] = r.F(40.f, false);
        g_ped->m_matrix = r.below(2) ? nullptr : reinterpret_cast<decltype(g_ped->m_matrix)>(g_mat);
        *reinterpret_cast<CVector*>((uint8_t*)g_ped + 4) = CVector{ r.F(50.f, false), r.F(50.f, false), r.F(5.f, false) };   // placement.pos
        c.opcode = Op(1961).I(g_pedRef).GV(0).b; });
    Test<COMMAND_GET_BEAT_PROXIMITY, &GetBeatProximity>("1969 GET_BEAT_PROXIMITY", [&](Ctx& c) {
        auto& r = *c.rng; auto* bi = (int32_t*)0xB6BC90 + offsetof(CAudioEngine, m_BeatInfo) / 4;
        for (int i = 0; i < 0xAC / 4; ++i) bi[i] = r.below(6) == 0 ? 0 : r.below(10) == 0 ? INT32_MIN : (int32_t)(r.u32() >> r.below(32)) * (r.below(2) ? -1 : 1);
        bi[0xA0 / 4] = r.below(4) != 0; if (r.below(3) == 0) bi[0xA8 / 4] = 0;
        c.watch.push_back({ bi, 0xAC });
        c.opcode = Op(1969).I(Pick(r, { 0, 0, 0, 1, -1, 2, -2, 5, -5, 9, -9, 10, -10, 11, -11, 14, -14 })).GV(0).GV(4).GV(8).b; });
    Test<COMMAND_CUSTOM_PLATE_DESIGN_FOR_NEXT_CAR, &CustomPlateDesignForNextCar>("1905 CUSTOM_PLATE_DESIGN_FOR_NEXT_CAR", [&](Ctx& c) {
        auto& r = *c.rng; for (auto& mi : g_fakeMI) { RandomFill(mi, sizeof(mi), r); *reinterpret_cast<void**>(mi) = g_miVtbl; mi[0x3F] = (uint8_t)Pick(r, { 6, 6, 5, 0, 7 }); *reinterpret_cast<void**>(mi + 0x24) = r.below(3) ? (void*)g_fakeRwObject : nullptr; c.watch.push_back({ mi, sizeof(mi) }); }
        for (int i = 0; i < 6; ++i) CModelInfo::ms_modelInfoPtrs[i] = r.below(4) == 0 ? nullptr : (CBaseModelInfo*)g_fakeMI[r.below(4)];
        c.watch.push_back({ &CModelInfo::ms_modelInfoPtrs[0], 6 * sizeof(void*) });
        c.opcode = Op(1905).I(r.below(6)).I((int32_t)r.u32()).b; });
    // =========================================================================================================================
    // S6-D review: task commands (constructor recorders) and the IPL commands
    {
        auto PH = [&](Rng& r) { return r.below(5) == 0 ? -1 : g_pedRef; };
        Test<COMMAND_TASK_GOTO_CAR, &TaskGotoCar>("1906 TASK_GOTO_CAR", [&](Ctx& c) {
            auto& r = *c.rng; RandomVehicle(g_veh, r); VehWatch(c);
            c.opcode = Op(1906).I(PH(r)).I(r.below(4) == 0 ? -1 : g_vehRef).I(r.below(3) == 0 ? -(int)r.below(100) : (int)r.u32() % 100000).Fl(r.F(50)).b; });
        Test<COMMAND_TASK_CLIMB, &TaskClimb>("1935 TASK_CLIMB", [&](Ctx& c) { auto& r = *c.rng; c.opcode = Op(1935).I(PH(r)).I(Flag(r)).b; });
        Test<COMMAND_PERFORM_SEQUENCE_TASK_FROM_PROGRESS, &PerformSequenceTaskFromProgress>("1952 PERFORM_SEQUENCE_TASK_FROM_PROGRESS", [&](Ctx& c) {
            auto& r = *c.rng;
            // CTheScripts::ScriptSequenceTaskArray (type 4): random activity / ids
            auto* base = reinterpret_cast<uint8_t*>(&CTheScripts::ScriptSequenceTaskArray[0]);
            RandomFill(base, 8 * 8, r); c.watch.push_back({ base, 64 });
            const int idx = r.below(10);
            c.opcode = Op(1952).I(PH(r)).I(r.below(8) == 0 ? -1 : (int)((uint32_t)idx | ((uint32_t)r.below(4) << 16))).I((int)r.u32() % 50).I((int)r.u32() % 50).b; });
        Test<COMMAND_TASK_GOTO_CHAR_AIMING, &TaskGotoCharAiming>("1955 TASK_GOTO_CHAR_AIMING", [&](Ctx& c) {
            auto& r = *c.rng; c.opcode = Op(1955).I(PH(r)).I(r.below(4) == 0 ? -1 : g_pedRef).Fl(r.F(50)).Fl(r.F(50)).b; });
        Test<COMMAND_TASK_KILL_CHAR_ON_FOOT_TIMED, &TaskKillCharOnFootTimed>("1957 TASK_KILL_CHAR_ON_FOOT_TIMED", [&](Ctx& c) {
            auto& r = *c.rng; c.opcode = Op(1957).I(PH(r)).I(r.below(4) == 0 ? -1 : g_pedRef).I(r.below(3) == 0 ? -1 : (int)r.u32()).b; });
        Test<COMMAND_TASK_JETPACK, &TaskJetpack>("1959 TASK_JETPACK", [&](Ctx& c) { auto& r = *c.rng; c.opcode = Op(1959).I(PH(r)).b; });
        Test<COMMAND_TASK_COMPLEX_PICKUP_OBJECT, &TaskComplexPickupObject>("1993 TASK_COMPLEX_PICKUP_OBJECT", [&](Ctx& c) {
            auto& r = *c.rng; c.opcode = Op(1993).I(PH(r)).I(r.below(4) == 0 ? -1 : g_objRef).b; });
        Test<COMMAND_TASK_CHAR_SLIDE_TO_COORD, &TaskCharSlideToCoord>("1997 TASK_CHAR_SLIDE_TO_COORD", [&](Ctx& c) {
            auto& r = *c.rng; c.opcode = Op(1997).I(PH(r)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(50)).Fl(r.F(720)).Fl(r.F(5)).b; });
        Test<COMMAND_TASK_SET_CHAR_DECISION_MAKER, &TaskSetCharDecisionMaker>("1980 TASK_SET_CHAR_DECISION_MAKER (sequence path)", [&](Ctx& c) {
            auto& r = *c.rng;
            for (int i = 0; i < 20; ++i) { ((uint16_t*)0xC0AFF4)[i] = (uint16_t)r.below(5); ((uint8_t*)0xC0B01C)[i] = (uint8_t)r.below(2); }
            c.opcode = Op(1980).I(-1).I(Pick(r, { -1, 0, 1, 7, 19, 20, (int32_t)(r.below(25) | (r.below(5) << 16)), (int32_t)(r.below(25) | (r.below(5) << 16)) })).b; });
        auto Ipl = [&](Ctx& c, int cmd) { auto& r = *c.rng; static const char* names[] = { "", "a", "LAe_1", "airport", "xyz", "Z12", "ab_cd" }; c.opcode = Op(cmd).S8(names[r.below(7)]).b; };
        Test<COMMAND_REQUEST_IPL, &RequestIpl>("1910 REQUEST_IPL", [&](Ctx& c) { Ipl(c, 1910); });
        Test<COMMAND_REMOVE_IPL, &RemoveIpl>("1911 REMOVE_IPL", [&](Ctx& c) { Ipl(c, 1911); });
        Test<COMMAND_REMOVE_IPL_DISCREETLY, &RemoveIplDiscreetly>("1912 REMOVE_IPL_DISCREETLY", [&](Ctx& c) { Ipl(c, 1912); });
    }
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g19_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice

    oracle::Patch(0x61A5A0, (void*)&H_TaskNew);
    oracle::Patch(0x465C20, (void*)&H_Gpst);
    oracle::Patch(0x632D10, (void*)&H_AddTask);
    oracle::Patch(0x46A630, (void*)&H_cClimb);
    oracle::Patch(0x635450, (void*)&H_cUseSeq);
    oracle::Patch(0x694B90, (void*)&H_cSeekAim);
    oracle::Patch(0x620E30, (void*)&H_cKill);
    oracle::Patch(0x67B4E0, (void*)&H_cJet);
    oracle::Patch(0x6919C0, (void*)&H_cPickUp);
    oracle::Patch(0x668120, (void*)&H_cGoStand);
    oracle::Patch(0x66C3E0, (void*)&H_cSlide);
    oracle::Patch(0x46A470, (void*)&H_cSetDm);
    oracle::Patch(0x404AC0, (void*)&H_FindIpl);
    oracle::Patch(0x405850, (void*)&H_ReqIpl);
    oracle::Patch(0x405890, (void*)&H_RemIpl);
    oracle::Patch(0x4058D0, (void*)&H_RemIplFar);
    oracle::Patch(0x539860, (void*)&H_Fires);
    oracle::Patch(0x5071B0, (void*)&H_GetBeatInfo);
    oracle::Patch(0x583DB0, (void*)&H_BlipZoom);
    oracle::Patch(0x45A060, (void*)&H_HasRec);
    oracle::Patch(0x45A020, (void*)&H_ReqRec);
    oracle::Patch(0x6F5DC0, (void*)&H_RemTrain);
    oracle::Patch(0x6F5DF0, (void*)&H_RelTrain);
    oracle::Patch(0x6F7140, (void*)&H_SetTrainPos);
    oracle::Patch(0x6F5EB0, (void*)&H_FindCarriage);
    oracle::Patch(0x6D3B60, (void*)&H_InitWinch);
    oracle::Patch(0x6D3CB0, (void*)&H_RelWinch);
    oracle::Patch(0x6D3CF0, (void*)&H_QueryWinch);
    oracle::Patch(0x59F380, (void*)&H_GetRope);
    oracle::Patch(0x59F3A0, (void*)&H_SetRope);
    oracle::Patch(0x59F3C0, (void*)&H_GetCarried);
    oracle::Patch(0x59F3E0, (void*)&H_RelRope);
    oracle::Patch(0x608DA0, (void*)&H_SetAcq);
    oracle::Patch(0x608980, (void*)&H_ClrAcq);
    oracle::Patch(0x6089A0, (void*)&H_ClrAllAcq);
    oracle::Patch(0x5FBB70, (void*)&H_SetAlloc);
    oracle::Patch(0x600B50, (void*)&H_SetDM);
    oracle::Patch(0x571A00, (void*)&H_CleanUpRef);
    oracle::Patch(0x571B70, (void*)&H_RegRef);
    oracle::Patch(0x49ADE0, (void*)&H_GetExtra);
    oracle::Patch(0x49ADA0, (void*)&H_GetNameTag);
    oracle::Patch(0x49BF70, (void*)&H_Buy);
    oracle::Patch(0x49B200, (void*)&H_StoreClothes);
    oracle::Patch(0x49B240, (void*)&H_RestoreClothes);
    oracle::Patch(0x49D160, (void*)&H_NearestTag);
    oracle::Patch(0x608710, (void*)&H_GetStatus);

    *reinterpret_cast<void**>(0xC0B030) = std::calloc(1, 0xF1C0);   // CDecisionMakerTypes singleton (GetInstance 0x4684F0 would construct it with the game's allocator)
    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g19_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
