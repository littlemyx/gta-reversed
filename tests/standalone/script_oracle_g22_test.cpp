// S6-G: differential test of SCRIPT COMMAND HANDLERS (group g22, ids 2200..2299) against the original machine code (exe oracle, see game_oracle.h).
// Same machinery as script_oracle_test.cpp / script_oracle_g16_test.cpp: the handler TUs (source/game_sa/Scripts/Commands/Ported/Group22a.cpp, Group22b.cpp, whose
// handlers live in anonymous namespaces) are #included, the test maps gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original addresses and, for every random case, runs
//   (1) the exe's group processor ProcessCommands2200To2299 (0x474900, thiscall(this = fake CRunningScript, cmd)) on a synthetic opcode buffer, and
//   (2) the CommandParser instantiation of the C++ handler on the same buffer and the same start state,
// then compares the script variables (ScriptSpace + the first local variables), the IP, the compare flag, the watched memory and the log of the callee functions the handler
// invoked (the exe callees are patched with host recorders, the C++ side defines the same recorders for the game functions).
// Extra: TASK_USE_CLOSEST_MAP_ATTRACTOR's search helper (the port of the exe's unnamed 0x6002F0) is compared to the exe function directly (fake entities / model infos / 2d effects).
// usage: script_oracle_g22_test.exe [-v] [-n cases] [name-substring ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group22a.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group22b.cpp"

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

auto& g_fx = StaticRef<Fx_c>(0xA9AE00); // (Fx.cpp is not part of this test)

// ---------------------------------------------------------------------------------------------------------------------------------
// call log: the callees of the handlers are replaced by recorders on both sides
static std::vector<uint32_t> g_log;
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float    BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static void Log(uint32_t tag) { g_log.push_back(tag); }
static void LogF(uint32_t tag, std::initializer_list<float> v) { g_log.push_back(tag); for (float f : v) g_log.push_back(FB(f)); }
static void LogU(uint32_t tag, std::initializer_list<uint32_t> v) { g_log.push_back(tag); for (auto u : v) g_log.push_back(u); }
static void LogS(const char* s) { g_log.push_back(0x5A5A); uint32_t h = 2166136261u; for (; *s; ++s) h = (h ^ (uint8_t)*s) * 16777619u; g_log.push_back(h); }

static CVehicle* g_veh;     static int g_vehRef;
static CObject*  g_obj;     static int g_objRef;
static CPed*     g_peds[4]; static int g_pedRefs[4];
static CPed*&    g_ped = g_peds[0];
static int&      g_pedRef = g_pedRefs[0];
static CMatrix*  g_mat;
static void*     g_fakeVtbl[256];
static void*     g_fakeVtblPed[256];
static void*     g_fakeObjVtbl[256];
static uint8_t   g_fakeEE[3][0x40];     // fake CEntryExit structures
static uint8_t   g_fakeZone[0x20];
static uint8_t   g_fakeGroup[8];
static uint32_t  g_hostRet;
static uint32_t Tag(const void* p) { // identity of a fake entity, so that logs do not contain host addresses
    if (!p) return 0;
    if (p == g_veh) return 1;
    if (p == g_obj) return 2;
    for (int i = 0; i < 4; ++i) if (p == g_peds[i]) return 3 + i;
    for (int i = 0; i < 3; ++i) if (p == g_fakeEE[i]) return 0x10 + i;
    if (p == g_fakeZone) return 0x20;
    if (p == g_fakeGroup) return 0x21;
    return 9;
}

// -- host recorders for the exe (cdecl / thiscall: __fastcall(this, edx, args...) pops the stack args like a thiscall callee)
static void  __fastcall H_UpdateMovingCollision(void* self, int, float a) { LogF(40, { (float)Tag(self), a }); }
static bool  __fastcall H_IsDoorMissing(void* self, int, uint8_t door) { LogF(36, { (float)door }); return (door * 7 + (int)(uintptr_t)self) % 3 == 0; }
static bool  __fastcall H_IsDoorFullyOpenU32(void* self, int, uint32_t node) { LogF(41, { (float)node }); return (node * 5 + (int)(uintptr_t)self) % 3 == 0; }
static void  __fastcall H_OpenDoor(void* self, int, void* ped, int node, uint8_t door, float ratio, int sound) { LogF(35, { (float)Tag(self), (float)Tag(ped), (float)node, (float)door, ratio, (float)(uint8_t)sound }); }
static void  __fastcall H_BlowUp(void* self, int, int a, int b, int c, int d) { LogF(42, { (float)Tag(self), (float)(uint8_t)a, (float)(uint8_t)b, (float)(uint8_t)c, (float)(uint8_t)d }); }
static void  __cdecl H_ForceRender(int v) { LogU(43, { (uint32_t)(uint8_t)v }); }
static void  __cdecl H_RequestModel(int model, int flags) { LogU(44, { (uint32_t)model, (uint32_t)flags }); }
static int   __cdecl H_FindNearestEntryExit(const CVector2D* p, float r, int ignore) { LogF(45, { p->x, p->y, r, (float)ignore }); return (int)(g_hostRet % 4); }
static void* __cdecl H_GetInSlot(int idx) { LogU(46, { (uint32_t)idx }); return g_fakeEE[(unsigned)idx % 3]; }
static bool  __fastcall H_IsPlayer(void* self, int) { Log(47); return (g_hostRet >> 8) & 1; }
static void  __cdecl H_AddEntryExitToStack(void* ee) { LogU(48, { Tag(ee) }); }
static void  __cdecl H_StartExtraColour(int c, int f) { LogU(49, { (uint32_t)c, (uint32_t)(uint8_t)f }); }
static void  __cdecl H_StopExtraColour(int f) { LogU(50, { (uint32_t)(uint8_t)f }); }
static void  __cdecl H_NightVision(int v) { LogU(51, { (uint32_t)(uint8_t)v }); }
static void  __cdecl H_InfraredVision(int v) { LogU(52, { (uint32_t)(uint8_t)v }); }
static void  __fastcall H_SetCharCoordinates(void* self, int, void* ped, float x, float y, float z, int warp, int off) { LogF(53, { (float)Tag(ped), x, y, z, (float)(uint8_t)warp, (float)(uint8_t)off }); }
static bool  __cdecl H_SkipButton() { Log(54); return g_hostRet & 1; }
static int   __cdecl H_CreateNewMenu(int type, const char* title, float x, float y, float w, int columns, int inter, int bg, int align) {
    LogF(55, { (float)type, x, y, w, (float)(uint8_t)columns, (float)((uint8_t)inter != 0), (float)((uint8_t)bg != 0), (float)(uint8_t)align }); LogS(title); return (uint8_t)g_hostRet; }
static void  __cdecl H_SetColumnOrientation(int a, int b, int c) { LogU(56, { (uint32_t)(uint8_t)a, (uint32_t)(uint8_t)b, (uint32_t)(uint8_t)c }); }
static int   __cdecl H_CheckForSelected(int id) { LogU(57, { (uint32_t)(uint8_t)id }); return (int8_t)g_hostRet; }
static int   __cdecl H_CheckForAccept(int id) { LogU(58, { (uint32_t)(uint8_t)id }); return (int8_t)g_hostRet; }
static void  __cdecl H_ActivateOneItem(int a, int b, int c) { LogU(59, { (uint32_t)(uint8_t)a, (uint32_t)(uint8_t)b, (uint32_t)((uint8_t)c != 0) }); }
static void  __cdecl H_SwitchOffMenu(int a) { LogU(60, { (uint32_t)(uint8_t)a }); }
static void  __cdecl H_InsertMenu(int id, int col, const char* h, const char* r0, const char* r1, const char* r2, const char* r3, const char* r4, const char* r5, const char* r6, const char* r7,
                                  const char* r8, const char* r9, const char* r10, const char* r11) {
    LogU(61, { (uint32_t)(uint8_t)id, (uint32_t)(uint8_t)col });
    for (const char* s : { h, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11 }) LogS(s);
}
static void  __cdecl H_InsertOneMenuItemWithNumber(int id, int col, int row, const char* text, int n1, int n2) {
    LogU(62, { (uint32_t)(uint8_t)id, (uint32_t)(uint8_t)col, (uint32_t)(uint8_t)row, (uint32_t)n1, (uint32_t)n2 }); LogS(text); }
static void  __cdecl H_SetBlipEntryExit(uint32_t blip, void* ee) { LogU(63, { blip, Tag(ee) }); }
static void  __cdecl H_SetCoordBlipAppearance(uint32_t blip, int app) { LogU(64, { blip, (uint32_t)(uint8_t)app }); }
static void  __fastcall H_ObjectInAngledArea(void* self, int, int cmd) { LogU(65, { (uint32_t)cmd }); }
static void  __fastcall H_SetGearUp(void* self, int) { LogU(66, { Tag(self) }); }
static void  __fastcall H_SetGearDown(void* self, int) { LogU(67, { Tag(self) }); }
static bool  __cdecl H_ScriptAttachAnimGroup(int model, const char* name) { LogU(68, { (uint32_t)model }); LogS(name); return true; }
static void  __cdecl H_AddToListOfSpecialAnimGroups(int model, const char* name) { LogU(69, { (uint32_t)model }); LogS(name); }
static void  __cdecl H_AppendToNextCutscene(const char* a, const char* b) { Log(70); LogS(a); LogS(b); }
static void  __fastcall H_AddSparks(void* self, int, const CVector* o, const CVector* d, float force, int n, float ax, float ay, float az, int type, float spread, float life) {
    LogF(71, { o->x, o->y, o->z, d->x, d->y, d->z, force, (float)n, ax, ay, az, (float)(uint8_t)type, spread, life }); }
static void* __cdecl H_FindSmallestZone(const CVector* p, int b) { LogF(72, { p->x, p->y, p->z, (float)(uint8_t)b }); return g_fakeZone; }
static bool  __fastcall H_IsPedDead(void* self, int, void* ped) { LogU(73, { Tag(ped) }); return (g_hostRet >> (Tag(ped) & 7)) & 1; }
static void* __cdecl H_GetPedsGroup(void* ped) { LogU(74, { Tag(ped) }); return ((g_hostRet >> 12) >> (Tag(ped) & 7)) & 1 ? g_fakeGroup : nullptr; }
static void  __fastcall H_SetCharCreatedBy(void* self, int, int v) { LogU(75, { Tag(self), (uint32_t)(uint8_t)v }); }
static void  __fastcall H_AddEntityToList(void* self, int, int handle, int type) { LogU(76, { (uint32_t)handle, (uint32_t)(uint8_t)type }); }
static int   __cdecl H_CRT_stricmp(const char* a, const char* b) { return _stricmp(a, b); }

// -- the same recorders as the game functions the handlers call (C++ side)
bool  CAutomobile::UpdateMovingCollision(float a) { LogF(40, { (float)Tag(this), a }); return false; }
void  C3dMarkers::ForceRender(bool v) { LogU(43, { (uint32_t)v }); }
void  CStreaming::RequestModel(int32 model, int32 flags) { LogU(44, { (uint32_t)model, (uint32_t)flags }); }
int32 CEntryExitManager::FindNearestEntryExit(const CVector2D& p, float r, int32 ignore) { LogF(45, { p.x, p.y, r, (float)ignore }); return (int)(g_hostRet % 4); }
CEntryExit* CEntryExitManager::GetInSlot(int32 idx) { LogU(46, { (uint32_t)idx }); return reinterpret_cast<CEntryExit*>(g_fakeEE[(unsigned)idx % 3]); }
bool  CPed::IsPlayer() const { Log(47); return (g_hostRet >> 8) & 1; }
void  CEntryExitManager::AddEntryExitToStack(CEntryExit* ee) { LogU(48, { Tag(ee) }); }
void  CTimeCycle::StartExtraColour(int32 c, bool f) { LogU(49, { (uint32_t)c, (uint32_t)f }); }
void  CTimeCycle::StopExtraColour(bool f) { LogU(50, { (uint32_t)f }); }
void  CPostEffects::ScriptNightVisionSwitch(bool v) { LogU(51, { (uint32_t)v }); }
void  CPostEffects::ScriptInfraredVisionSwitch(bool v) { LogU(52, { (uint32_t)v }); }
void  CRunningScript::SetCharCoordinates(CPed& ped, CVector p, bool warp, bool off) { LogF(53, { (float)Tag(&ped), p.x, p.y, p.z, (float)warp, (float)off }); }
bool  CCutsceneMgr::IsCutsceneSkipButtonBeingPressed() { Log(54); return g_hostRet & 1; }
MenuId CMenuSystem::CreateNewMenu(eMenuType type, const char* title, float x, float y, float w, uint8 columns, bool inter, bool bg, eFontAlignment align) {
    LogF(55, { (float)type, x, y, w, (float)columns, (float)inter, (float)bg, (float)(uint8_t)align }); LogS(title); return (uint8)g_hostRet; }
void  CMenuSystem::SetColumnOrientation(MenuId a, uint8 b, uint8 c) { LogU(56, { (uint32_t)a, (uint32_t)b, (uint32_t)c }); }
int8  CMenuSystem::CheckForSelected(MenuId id) { LogU(57, { (uint32_t)id }); return (int8)g_hostRet; }
int8  CMenuSystem::CheckForAccept(MenuId id) { LogU(58, { (uint32_t)id }); return (int8)g_hostRet; }
void  CMenuSystem::ActivateOneItem(MenuId a, uint8 b, bool c) { LogU(59, { (uint32_t)a, (uint32_t)b, (uint32_t)c }); }
void  CMenuSystem::SwitchOffMenu(MenuId a) { LogU(60, { (uint32_t)a }); }
void  CMenuSystem::InsertMenu(MenuId id, uint8 col, const char* h, const char* r0, const char* r1, const char* r2, const char* r3, const char* r4, const char* r5, const char* r6, const char* r7,
                              const char* r8, const char* r9, const char* r10, const char* r11) {
    LogU(61, { (uint32_t)id, (uint32_t)col });
    for (const char* s : { h, r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11 }) LogS(s);
}
void  CMenuSystem::InsertOneMenuItemWithNumber(MenuId id, uint8 col, uint8 row, const char* text, int32 n1, int32 n2) {
    LogU(62, { (uint32_t)id, (uint32_t)col, (uint32_t)row, (uint32_t)n1, (uint32_t)n2 }); LogS(text); }
void  CRadar::SetBlipEntryExit(tBlipHandle blip, CEntryExit* ee) { LogU(63, { blip, Tag(ee) }); }
void  CRadar::SetCoordBlipAppearance(tBlipHandle blip, eBlipAppearance app) { LogU(64, { blip, (uint32_t)app }); }
void  CRunningScript::ObjectInAngledAreaCheckCommand(int32 cmd) { LogU(65, { (uint32_t)cmd }); }
void  CPlane::SetGearUp() { LogU(66, { Tag(this) }); }
void  CPlane::SetGearDown() { LogU(67, { Tag(this) }); }
bool  CTheScripts::ScriptAttachAnimGroupToCharModel(int32 model, const char* name) { LogU(68, { (uint32_t)model }); LogS(name); return true; }
void  CTheScripts::AddToListOfSpecialAnimGroupsAttachedToCharModels(int32 model, const char* name) { LogU(69, { (uint32_t)model }); LogS(name); }
void  CCutsceneMgr::AppendToNextCutscene(const char* a, const char* b) { Log(70); LogS(a); LogS(b); }
void  Fx_c::AddSparks(const CVector& o, const CVector& d, float force, int32 n, CVector across, eSparkType type, float spread, float life) {
    LogF(71, { o.x, o.y, o.z, d.x, d.y, d.z, force, (float)n, across.x, across.y, across.z, (float)(uint8_t)type, spread, life }); }
CZone* CTheZones::FindSmallestZoneForPosition(const CVector& p, bool b) { LogF(72, { p.x, p.y, p.z, (float)b }); return reinterpret_cast<CZone*>(g_fakeZone); }
bool  CRunningScript::IsPedDead(CPed* ped) const { LogU(73, { Tag(ped) }); return (g_hostRet >> (Tag(ped) & 7)) & 1; }
CPedGroup* CPedGroups::GetPedsGroup(const CPed* ped) { LogU(74, { Tag(ped) }); return ((g_hostRet >> 12) >> (Tag(ped) & 7)) & 1 ? reinterpret_cast<CPedGroup*>(g_fakeGroup) : nullptr; }
void  CPed::SetCharCreatedBy(ePedCreatedBy v) { LogU(75, { Tag(this), (uint32_t)v }); }
void  CMissionCleanup::AddEntityToList(int32 handle, MissionCleanUpEntityType type) { LogU(76, { (uint32_t)handle, (uint32_t)type }); }
// the exe's own pure functions, used by the port as callees
eCarNodes CDamageManager::GetCarNodeIndexFromDoor(eDoors door) { return (eCarNodes)oracle::Fn<int __cdecl(int)>(0x6C26F0)((int)door); }
eClothesModelPart CClothes::GetTextureDependency(eClothesTexturePart p) { return (eClothesModelPart)oracle::Fn<int __cdecl(int)>(0x5A7EA0)((int)p); }
int16 CStreamedScripts::GetProperIndexFromIndexUsedByScript(int16 id) { return oracle::Fn<int16 __fastcall(CStreamedScripts*, int, int)>(0x470810)(this, 0, id); }

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
    constexpr unsigned groupFn = 0x474900; // ProcessCommands2200To2299
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

// ---------------------------------------------------------------------------------------------------------------------------------
static void RandomFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }

// fully random vehicle memory, but with the pointers the commands dereference kept valid
static void RandomVehicle(Rng& r) {
    RandomFill(g_veh, sizeof(CAutomobile), r);
    RandomFill(g_mat, sizeof(CMatrix), r);
    *reinterpret_cast<void**>(g_veh) = g_fakeVtbl;                         // vptr
    g_veh->m_matrix = reinterpret_cast<decltype(g_veh->m_matrix)>(g_mat);   // +0x14
    auto* a = static_cast<CAutomobile*>(g_veh);
    for (auto& n : a->m_aCarNodes) n = r.below(3) ? nullptr : reinterpret_cast<RwFrame*>((uintptr_t)0x2000);
    g_veh->m_nModelIndex = (uint16)(r.below(4) ? r.below(4) : 0xFFFF); // (the vehicle model info is looked up with the int16 index; the table below has 4 fakes at 0..3)
    if (g_veh->m_nModelIndex == 0xFFFF) g_veh->m_nModelIndex = (uint16)r.below(4);
}
static void RandomPed(CPed* p, Rng& r) {
    RandomFill(p, sizeof(CPed), r);
    *reinterpret_cast<void**>(p) = g_fakeVtblPed;
    p->m_matrix = nullptr;
    p->m_placement.m_vPosn = CVector{ r.F(50.f, false), r.F(50.f, false), r.F(50.f, false) };
}
static void RandomObject(Rng& r) {
    RandomFill(g_obj, sizeof(CObject), r);
    *reinterpret_cast<void**>(g_obj) = g_fakeObjVtbl;
}

static uint8_t g_fakeMI[4][0x100];
static void SetupFakeWorld() {
    auto* pp = new CPedPool(8, "oracle_ped");
    auto* vp = new CVehiclePool(4, "oracle_veh");
    auto* op = new CObjectPool(4, "oracle_obj");
    *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp;
    *reinterpret_cast<CObjectPool**>(0xB7449C) = op;
    for (int i = 0; i < 4; ++i) { g_peds[i] = pp->New(); g_pedRefs[i] = pp->GetRef(g_peds[i]); std::memset(g_peds[i], 0, sizeof(CPed)); }
    g_veh = vp->New();  g_vehRef = vp->GetRef(g_veh);
    g_obj = op->New();  g_objRef = op->GetRef(g_obj);
    g_mat = new CMatrix();
    for (int i = 0; i < 4; ++i) CModelInfo::ms_modelInfoPtrs[i] = reinterpret_cast<CBaseModelInfo*>(g_fakeMI[i]);
    g_fakeVtbl[0x6C / 4] = (void*)&H_OpenDoor;                // CVehicle::OpenDoor(ped, node, door, ratio, sound) -- slot 27
    g_fakeVtbl[0x84 / 4] = (void*)&H_IsDoorFullyOpenU32;      // CVehicle::IsDoorFullyOpenU32 -- slot 33
    g_fakeVtbl[0x98 / 4] = (void*)&H_IsDoorMissing;           // CVehicle::IsDoorMissing(eDoors) -- slot 38
    g_fakeVtbl[0xA8 / 4] = (void*)&H_BlowUp;                  // CVehicle::BlowUpCarCutSceneNoExtras -- slot 42
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void RandomEE(Rng& r) {
    for (auto& e : g_fakeEE) {
        RandomFill(e, sizeof(e), r);
        // m_pLink (+0x38): null, itself, or another fake entry
        const int k = r.below(3);
        *reinterpret_cast<void**>(e + 0x38) = k == 0 ? nullptr : g_fakeEE[r.below(3)];
        if (r.below(3) == 0) e[0x32] = 0;     // area
        if (r.below(3) == 0) e[0x33] = 0;     // sky colour
    }
}

static std::string Lbl(Rng& r) { // a text label of 1..7 chars (the exe's stack buffers are not terminated for 8) or "DUMMY"
    static const char* pool[] = { "DUMMY", "dummy", "ABC", "MENU_1", "X", "GANG_A", "Dummy", "SPLASH", "a", "TEST123" };
    return pool[r.below(10)];
}

// the 6 variants of the bit test commands use a different operand kind for the value / the bit
static Op BitOperand(Op op, Ctx& c, int kind, uint16_t gvOff, int lvIdx, uint32_t bitValue) {
    if (kind == 0) return op.I((int32_t)bitValue);
    if (kind == 1) { *reinterpret_cast<uint32_t*>(&ScriptSpaceRef()[gvOff]) = bitValue; return op.GV(gvOff); }
    c.lv[lvIdx] = bitValue; return op.LV((uint16_t)lvIdx);
}

static void TestAll() {
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CAutomobile) } }; };
    auto PedWatch = [](Ctx& c) { c.watch = { { g_ped, sizeof(CPed) } }; };
    auto ObjWatch = [](Ctx& c) { c.watch = { { g_obj, sizeof(CObject) } }; };
    auto Veh = [&](Ctx& c, int cmd) { RandomVehicle(*c.rng); VehWatch(c); return Op(cmd).I(g_vehRef); };
    auto Ped = [&](Ctx& c, int cmd) { RandomPed(g_ped, *c.rng); PedWatch(c); return Op(cmd).I(g_pedRef); };
    auto Small = [](Ctx& c, int n) { return c.rng->below(3) ? (int32_t)c.rng->below(n) : (int32_t)c.rng->u32(); };
    auto RandBool = [](Ctx& c) { return c.rng->below(3) ? (int32_t)c.rng->below(2) : (int32_t)c.rng->u32(); };

    // ---- pure global state
    Test<COMMAND_ENABLE_CRANE_CONTROLS, &EnableCraneControls>("2200 ENABLE_CRANE_CONTROLS", [&](Ctx& c) {
        c.watch = { { (void*)0xA44494, 3 } }; c.opcode = Op(2200).I(RandBool(c)).I(RandBool(c)).I(RandBool(c)).b; });
    Test<COMMAND_CAN_TRIGGER_GANG_WAR_WHEN_ON_A_MISSION, &CanTriggerGangWarWhenOnAMission>("2211 CAN_TRIGGER_GANG_WAR_WHEN_ON_A_MISSION", [&](Ctx& c) {
        c.watch = { { (void*)0x96AB93, 1 } }; c.opcode = Op(2211).I(RandBool(c)).b; });
    Test<COMMAND_SET_GANG_WARS_TRAINING_MISSION, &SetGangWarsTrainingMission>("2220 SET_GANG_WARS_TRAINING_MISSION", [&](Ctx& c) {
        c.watch = { { (void*)0x96AB91, 1 } }; c.opcode = Op(2220).I(RandBool(c)).b; });
    Test<COMMAND_SWITCH_DEATH_PENALTIES, &SwitchDeathPenalties>("2269 SWITCH_DEATH_PENALTIES", [&](Ctx& c) {
        c.watch = { { (void*)0x8A5E48, 1 } }; c.opcode = Op(2269).I(RandBool(c)).b; });
    Test<COMMAND_SWITCH_ARREST_PENALTIES, &SwitchArrestPenalties>("2270 SWITCH_ARREST_PENALTIES", [&](Ctx& c) {
        c.watch = { { (void*)0x8A5E49, 1 } }; c.opcode = Op(2270).I(RandBool(c)).b; });
    Test<COMMAND_SET_EXTRA_HOSPITAL_RESTART_POINT, &SetExtraHospitalRestartPoint>("2271 SET_EXTRA_HOSPITAL_RESTART_POINT", [&](Ctx& c) {
        c.watch = { { (void*)0xA43414, 12 }, { (void*)0xA43254, 8 } };
        c.opcode = Op(2271).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(50)).Fl(c.rng->F(360)).b; });
    Test<COMMAND_SET_EXTRA_POLICE_STATION_RESTART_POINT, &SetExtraPoliceStationRestartPoint>("2272 SET_EXTRA_POLICE_STATION_RESTART_POINT", [&](Ctx& c) {
        c.watch = { { (void*)0xA43420, 12 }, { (void*)0xA4324C, 8 } };
        c.opcode = Op(2272).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).Fl(c.rng->F(50)).Fl(c.rng->F(360)).b; });
    Test<COMMAND_FIND_NUMBER_TAGS_TAGGED, &FindNumberTagsTagged>("2273 FIND_NUMBER_TAGS_TAGGED", [&](Ctx& c) {
        *reinterpret_cast<int32_t*>(0xA9AD74) = (int32_t)c.rng->u32(); c.watch = { { (void*)0xA9AD74, 4 } }; c.opcode = Op(2273).GV(4).b; });
    Test<COMMAND_GET_TERRITORY_UNDER_CONTROL_PERCENTAGE, &GetTerritoryUnderControlPercentage>("2274 GET_TERRITORY_UNDER_CONTROL_PERCENTAGE", [&](Ctx& c) {
        const float v = c.rng->below(10) == 0 ? BF(c.rng->u32()) : c.rng->below(2) ? c.rng->f01() : c.rng->F(1e7f);
        *reinterpret_cast<float*>(0x96AB9C) = v; c.watch = { { (void*)0x96AB9C, 4 } }; c.opcode = Op(2274).GV(4).b; });
    Test<COMMAND_DISABLE_ALL_ENTRY_EXITS, &DisableAllEntryExits>("2279 DISABLE_ALL_ENTRY_EXITS", [&](Ctx& c) {
        c.watch = { { (void*)0x96A7C8, 1 } }; c.opcode = Op(2279).I(RandBool(c)).b; });
    Test<COMMAND_SET_CREATE_RANDOM_GANG_MEMBERS, &SetCreateRandomGangMembers>("2282 SET_CREATE_RANDOM_GANG_MEMBERS", [&](Ctx& c) {
        c.watch = { { (void*)0xC0FCB2, 1 } }; c.opcode = Op(2282).I(RandBool(c)).b; });
    Test<COMMAND_SHOW_UPDATE_STATS, &ShowUpdateStats>("2296 SHOW_UPDATE_STATS", [&](Ctx& c) {
        c.watch = { { (void*)0x8CDE56, 1 } }; c.opcode = Op(2296).I(RandBool(c)).b; });
    Test<COMMAND_GET_CUTSCENE_OFFSET, &GetCutsceneOffset>("2257 GET_CUTSCENE_OFFSET", [&](Ctx& c) {
        RandomFill((void*)0xBC4034, 12, *c.rng); c.watch = { { (void*)0xBC4034, 12 } }; c.opcode = Op(2257).GV(0).GV(4).GV(8).b; });

    // ---- bit commands on script variables (every operand kind)
    auto IsBit = [&](int cmd, bool local) {
        return [=, &Small](Ctx& c) {
            auto& r = *c.rng;
            const int kind = (cmd - 2228) % 3; // CONST / VAR / LVAR for the bit operand
            const uint32_t value = r.u32(), bit = r.below(3) ? r.below(32) : r.u32();
            Op op(cmd);
            if (local) { c.lv[0] = value; op.LV(0); } else { *reinterpret_cast<uint32_t*>(&ScriptSpaceRef()[16]) = value; op.GV(16); }
            c.opcode = BitOperand(op, c, kind, 20, 1, bit).b;
        };
    };
    Test<COMMAND_IS_GLOBAL_VAR_BIT_SET_CONST, &IsVarBitSet>("2228 IS_GLOBAL_VAR_BIT_SET_CONST", IsBit(2228, false));
    Test<COMMAND_IS_GLOBAL_VAR_BIT_SET_VAR, &IsVarBitSet>("2229 IS_GLOBAL_VAR_BIT_SET_VAR", IsBit(2229, false));
    Test<COMMAND_IS_GLOBAL_VAR_BIT_SET_LVAR, &IsVarBitSet>("2230 IS_GLOBAL_VAR_BIT_SET_LVAR", IsBit(2230, false));
    Test<COMMAND_IS_LOCAL_VAR_BIT_SET_CONST, &IsVarBitSet>("2231 IS_LOCAL_VAR_BIT_SET_CONST", IsBit(2231, true));
    Test<COMMAND_IS_LOCAL_VAR_BIT_SET_VAR, &IsVarBitSet>("2232 IS_LOCAL_VAR_BIT_SET_VAR", IsBit(2232, true));
    Test<COMMAND_IS_LOCAL_VAR_BIT_SET_LVAR, &IsVarBitSet>("2233 IS_LOCAL_VAR_BIT_SET_LVAR", IsBit(2233, true));
    auto ChangeBit = [&](int cmd, int base, bool local) {
        return [=](Ctx& c) {
            auto& r = *c.rng;
            const int kind = (cmd - base) % 3;
            const uint32_t value = r.u32(), bit = r.below(3) ? r.below(32) : r.u32();
            Op op(cmd);
            if (local) { c.lv[0] = value; op.LV(0); } else { *reinterpret_cast<uint32_t*>(&ScriptSpaceRef()[16]) = value; op.GV(16); }
            c.opcode = BitOperand(op, c, kind, 20, 1, bit).b;
        };
    };
    Test<COMMAND_SET_GLOBAL_VAR_BIT_CONST, &SetVarBit<VAR_GLOBAL>>("2234 SET_GLOBAL_VAR_BIT_CONST", ChangeBit(2234, 2234, false));
    Test<COMMAND_SET_GLOBAL_VAR_BIT_VAR, &SetVarBit<VAR_GLOBAL>>("2235 SET_GLOBAL_VAR_BIT_VAR", ChangeBit(2235, 2234, false));
    Test<COMMAND_SET_GLOBAL_VAR_BIT_LVAR, &SetVarBit<VAR_GLOBAL>>("2236 SET_GLOBAL_VAR_BIT_LVAR", ChangeBit(2236, 2234, false));
    Test<COMMAND_SET_LOCAL_VAR_BIT_CONST, &SetVarBit<VAR_LOCAL>>("2237 SET_LOCAL_VAR_BIT_CONST", ChangeBit(2237, 2237, true));
    Test<COMMAND_SET_LOCAL_VAR_BIT_VAR, &SetVarBit<VAR_LOCAL>>("2238 SET_LOCAL_VAR_BIT_VAR", ChangeBit(2238, 2237, true));
    Test<COMMAND_SET_LOCAL_VAR_BIT_LVAR, &SetVarBit<VAR_LOCAL>>("2239 SET_LOCAL_VAR_BIT_LVAR", ChangeBit(2239, 2237, true));
    Test<COMMAND_CLEAR_GLOBAL_VAR_BIT_CONST, &ClearVarBit<VAR_GLOBAL>>("2240 CLEAR_GLOBAL_VAR_BIT_CONST", ChangeBit(2240, 2240, false));
    Test<COMMAND_CLEAR_GLOBAL_VAR_BIT_VAR, &ClearVarBit<VAR_GLOBAL>>("2241 CLEAR_GLOBAL_VAR_BIT_VAR", ChangeBit(2241, 2240, false));
    Test<COMMAND_CLEAR_GLOBAL_VAR_BIT_LVAR, &ClearVarBit<VAR_GLOBAL>>("2242 CLEAR_GLOBAL_VAR_BIT_LVAR", ChangeBit(2242, 2240, false));
    Test<COMMAND_CLEAR_LOCAL_VAR_BIT_CONST, &ClearVarBit<VAR_LOCAL>>("2243 CLEAR_LOCAL_VAR_BIT_CONST", ChangeBit(2243, 2243, true));
    Test<COMMAND_CLEAR_LOCAL_VAR_BIT_VAR, &ClearVarBit<VAR_LOCAL>>("2244 CLEAR_LOCAL_VAR_BIT_VAR", ChangeBit(2244, 2243, true));
    Test<COMMAND_CLEAR_LOCAL_VAR_BIT_LVAR, &ClearVarBit<VAR_LOCAL>>("2245 CLEAR_LOCAL_VAR_BIT_LVAR", ChangeBit(2245, 2243, true));

    // ---- peds / vehicles / objects (the whole fake entity is compared)
    Test<COMMAND_GET_PED_TYPE, &GetPedType>("2207 GET_PED_TYPE", [&](Ctx& c) { c.opcode = Ped(c, 2207).GV(4).b; });
    Test<COMMAND_SET_CHAR_MAX_HEALTH, &SetCharMaxHealth>("2223 SET_CHAR_MAX_HEALTH", [&](Ctx& c) {
        c.opcode = Ped(c, 2223).I(c.rng->below(3) ? (int32_t)c.rng->below(2000) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE, &SetCharCanBeKnockedOffBike>("2246 SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE", [&](Ctx& c) { c.opcode = Ped(c, 2246).I(Small(c, 5)).b; });
    Test<COMMAND_WINCH_CAN_PICK_VEHICLE_UP, &WinchCanPickVehicleUp>("2213 WINCH_CAN_PICK_VEHICLE_UP", [&](Ctx& c) { c.opcode = Veh(c, 2213).I(RandBool(c)).b; });
    Test<COMMAND_VEHICLE_CAN_BE_TARGETTED_BY_HS_MISSILE, &VehicleCanBeTargettedByHSMissile>("2290 VEHICLE_CAN_BE_TARGETTED_BY_HS_MISSILE", [&](Ctx& c) { c.opcode = Veh(c, 2290).I(RandBool(c)).b; });
    Test<COMMAND_SET_FREEBIES_IN_VEHICLE, &SetFreebiesInVehicle>("2291 SET_FREEBIES_IN_VEHICLE", [&](Ctx& c) { c.opcode = Veh(c, 2291).I(RandBool(c)).b; });
    Test<COMMAND_SET_OBJECT_AS_STEALABLE, &SetObjectAsStealable>("2281 SET_OBJECT_AS_STEALABLE", [&](Ctx& c) {
        RandomObject(*c.rng); ObjWatch(c); c.opcode = Op(2281).I(g_objRef).I(RandBool(c)).b; });
    Test<COMMAND_GET_VEHICLE_CLASS, &GetVehicleClass>("2284 GET_VEHICLE_CLASS", [&](Ctx& c) {
        for (auto& m : g_fakeMI) RandomFill(m, sizeof(m), *c.rng);
        c.opcode = Veh(c, 2284).GV(4).b; });

    // ---- vehicle virtuals / recorded callees
    Test<COMMAND_CONTROL_MOVABLE_VEHICLE_PART, &ControlMovableVehiclePart>("2212 CONTROL_MOVABLE_VEHICLE_PART", [&](Ctx& c) {
        c.opcode = Veh(c, 2212).Fl(c.rng->F(400.f)).b; });
    Test<COMMAND_OPEN_CAR_DOOR_A_BIT, &OpenCarDoorABit>("2214 OPEN_CAR_DOOR_A_BIT", [&](Ctx& c) { c.opcode = Veh(c, 2214).I(c.rng->below(6)).Fl(c.rng->F(2.f)).b; });
    Test<COMMAND_IS_CAR_DOOR_FULLY_OPEN, &IsCarDoorFullyOpen>("2215 IS_CAR_DOOR_FULLY_OPEN", [&](Ctx& c) { c.opcode = Veh(c, 2215).I(c.rng->below(6)).b; });
    Test<COMMAND_EXPLODE_CAR_IN_CUTSCENE_SHAKE_AND_BITS, &ExplodeCarInCutsceneShakeAndBits>("2251 EXPLODE_CAR_IN_CUTSCENE_SHAKE_AND_BITS", [&](Ctx& c) {
        c.opcode = Veh(c, 2251).I(Small(c, 3)).I(Small(c, 3)).I(Small(c, 3)).b; });
    Test<COMMAND_SET_PLANE_UNDERCARRIAGE_UP, &SetPlaneUndercarriageUp>("2278 SET_PLANE_UNDERCARRIAGE_UP", [&](Ctx& c) { c.opcode = Veh(c, 2278).I(RandBool(c)).b; });
    Test<COMMAND_SET_ALWAYS_DRAW_3D_MARKERS, &SetAlwaysDraw3dMarkers>("2216 SET_ALWAYS_DRAW_3D_MARKERS", [&](Ctx& c) { c.opcode = Op(2216).I(RandBool(c)).b; });
    Test<COMMAND_SET_NIGHT_VISION, &SetNightVision>("2225 SET_NIGHT_VISION", [&](Ctx& c) { c.opcode = Op(2225).I(RandBool(c)).b; });
    Test<COMMAND_SET_INFRARED_VISION, &SetInfraredVision>("2226 SET_INFRARED_VISION", [&](Ctx& c) { c.opcode = Op(2226).I(RandBool(c)).b; });
    Test<COMMAND_IS_SKIP_CUTSCENE_BUTTON_PRESSED, &IsSkipCutsceneButtonPressed>("2256 IS_SKIP_CUTSCENE_BUTTON_PRESSED", [&](Ctx& c) { c.opcode = Op(2256).b; });
    Test<COMMAND_SET_CHAR_COORDINATES_DONT_WARP_GANG, &SetCharCoordinatesDontWarpGang>("2247 SET_CHAR_COORDINATES_DONT_WARP_GANG", [&](Ctx& c) {
        c.opcode = Op(2247).I(g_pedRef).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).b; });
    Test<COMMAND_IS_OBJECT_IN_ANGLED_AREA_2D, &IsObjectInAngledArea>("2275 IS_OBJECT_IN_ANGLED_AREA_2D", [&](Ctx& c) { c.opcode = Op(2275).b; });
    Test<COMMAND_IS_OBJECT_IN_ANGLED_AREA_3D, &IsObjectInAngledArea>("2276 IS_OBJECT_IN_ANGLED_AREA_3D", [&](Ctx& c) { c.opcode = Op(2276).b; });

    // ---- streamed scripts: StreamedScripts (0xA47B60, 82 entries of 0x20) random; the load state of the streaming infos
    auto StreamSetup = [&](Ctx& c) {
        auto& r = *c.rng;
        auto* ss = reinterpret_cast<uint8_t*>(0xA47B60);
        for (int i = 0; i < 82; ++i) *reinterpret_cast<int16_t*>(ss + i * 0x20 + 6) = (int16_t)(r.below(4) ? r.below(100) : r.u32());
        for (int i = -1; i < 82; ++i) reinterpret_cast<uint8_t*>(0x8E4CC0 + (26230 + i) * 20)[0x10] = (uint8_t)r.below(4);
        c.watch = { { (void*)(0x8E4CC0 + (26230 - 1) * 20), 83 * 20 } };
        const int idx = r.below(90);
        return (int32_t)(r.below(6) == 0 ? (int32_t)r.u32() : (int32_t)(*reinterpret_cast<int16_t*>(ss + (idx % 82) * 0x20 + 6)));
    };
    Test<COMMAND_STREAM_SCRIPT, &StreamScript>("2217 STREAM_SCRIPT", [&](Ctx& c) { c.opcode = Op(2217).I(StreamSetup(c)).b; });
    Test<COMMAND_HAS_STREAMED_SCRIPT_LOADED, &HasStreamedScriptLoaded>("2219 HAS_STREAMED_SCRIPT_LOADED", [&](Ctx& c) { c.opcode = Op(2219).I(StreamSetup(c)).b; });

    // ---- shopping: the ported CShopping::AddPriceModifier / RemovePriceModifier (Shopping.cpp is linked) against the exe's
    auto ShopSetup = [&](Ctx& c) {
        auto& r = *c.rng;
        auto* mods = reinterpret_cast<uint32_t*>(0xA98650);
        const int n = r.below(6);
        *reinterpret_cast<int32_t*>(0xA9A7D0) = n;
        for (int i = 0; i < 8; ++i) { mods[i * 2] = r.below(5); mods[i * 2 + 1] = r.u32(); }
        const int np = r.below(5);
        *reinterpret_cast<int32_t*>(0xA9A7CC) = np;
        for (int i = 0; i < 6; ++i) RandomFill(reinterpret_cast<uint8_t*>(0xA986F0) + i * 0x18, 0x18, r), *reinterpret_cast<uint32_t*>(0xA986F0 + i * 0x18) = r.below(5);
        c.watch = { { (void*)0xA98650, 8 * 8 }, { (void*)0xA9A7D0, 4 }, { (void*)0xA9A7CC, 4 }, { (void*)0xA986F0, 6 * 0x18 } };
        return (int32_t)r.below(6);
    };
    Test<COMMAND_ADD_PRICE_MODIFIER, &AddPriceModifier>("2248 ADD_PRICE_MODIFIER", [&](Ctx& c) { const auto k = ShopSetup(c); c.opcode = Op(2248).I(k).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_REMOVE_PRICE_MODIFIER, &RemovePriceModifier>("2249 REMOVE_PRICE_MODIFIER", [&](Ctx& c) { const auto k = ShopSetup(c); c.opcode = Op(2249).I(k).b; });

    // ---- entry exits
    Test<COMMAND_SET_CHAR_HAS_USED_ENTRY_EXIT, &SetCharHasUsedEntryExit>("2221 SET_CHAR_HAS_USED_ENTRY_EXIT", [&](Ctx& c) {
        RandomEE(*c.rng); c.watch = { { g_fakeEE, sizeof(g_fakeEE) }, { (void*)0x96A7C4, 4 } };
        auto op = Ped(c, 2221); c.opcode = op.Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).b; });
    Test<COMMAND_SET_BLIP_ENTRY_EXIT, &SetBlipEntryExit>("2268 SET_BLIP_ENTRY_EXIT", [&](Ctx& c) {
        c.opcode = Op(2268).I((int32_t)c.rng->u32()).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).b; });
    Test<COMMAND_SET_COORD_BLIP_APPEARANCE, &SetCoordBlipAppearance>("2299 SET_COORD_BLIP_APPEARANCE", [&](Ctx& c) {
        c.opcode = Op(2299).I((int32_t)c.rng->u32()).I(Small(c, 6)).b; });

    // ---- menus
    Test<COMMAND_CREATE_MENU, &ScriptCreateMenu>("2260 CREATE_MENU", [&](Ctx& c) {
        auto& r = *c.rng;
        *reinterpret_cast<int32_t*>(0xC17044) = r.below(3) ? 640 + r.below(2000) : (int32_t)r.u32(); *reinterpret_cast<int32_t*>(0xC17048) = r.below(3) ? 448 + r.below(1500) : (int32_t)r.u32();
        c.watch = { { (void*)0xC17044, 8 } };
        c.opcode = Op(2260).Str8(Lbl(r).c_str()).Fl(r.F(700)).Fl(r.F(500)).Fl(r.F(700)).I(Small(c, 6)).I(RandBool(c)).I(RandBool(c)).I(Small(c, 4)).b; });
    Test<COMMAND_SET_MENU_COLUMN_ORIENTATION, &SetMenuColumnOrientation>("2262 SET_MENU_COLUMN_ORIENTATION", [&](Ctx& c) {
        c.opcode = Op(2262).I(Small(c, 9)).I(Small(c, 9)).I(Small(c, 4)).b; });
    Test<COMMAND_GET_MENU_ITEM_SELECTED, &GetMenuItemSelected>("2263 GET_MENU_ITEM_SELECTED", [&](Ctx& c) { c.opcode = Op(2263).I(Small(c, 9)).GV(4).b; });
    Test<COMMAND_GET_MENU_ITEM_ACCEPTED, &GetMenuItemAccepted>("2264 GET_MENU_ITEM_ACCEPTED", [&](Ctx& c) { c.opcode = Op(2264).I(Small(c, 9)).GV(4).b; });
    Test<COMMAND_ACTIVATE_MENU_ITEM, &ActivateMenuItem>("2265 ACTIVATE_MENU_ITEM", [&](Ctx& c) { c.opcode = Op(2265).I(Small(c, 9)).I(Small(c, 12)).I(c.rng->below(3) ? (int32_t)c.rng->below(2) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_DELETE_MENU, &ScriptDeleteMenu>("2266 DELETE_MENU", [&](Ctx& c) { c.opcode = Op(2266).I(Small(c, 9)).b; });
    Test<COMMAND_SET_MENU_COLUMN, &SetMenuColumn>("2267 SET_MENU_COLUMN", [&](Ctx& c) {
        Op op(2267); op.I(Small(c, 9)).I(Small(c, 9)); for (int i = 0; i < 13; ++i) op.Str8(Lbl(*c.rng).c_str()); c.opcode = op.b; });
    Test<COMMAND_SET_MENU_ITEM_WITH_NUMBER, &SetMenuItemWithNumber>("2286 SET_MENU_ITEM_WITH_NUMBER", [&](Ctx& c) {
        c.opcode = Op(2286).I(Small(c, 9)).I(Small(c, 9)).I(Small(c, 12)).Str8(Lbl(*c.rng).c_str()).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_MENU_ITEM_WITH_2_NUMBERS, &SetMenuItemWithNumber>("2287 SET_MENU_ITEM_WITH_2_NUMBERS", [&](Ctx& c) {
        c.opcode = Op(2287).I(Small(c, 9)).I(Small(c, 9)).I(Small(c, 12)).Str8(Lbl(*c.rng).c_str()).I((int32_t)c.rng->u32()).I((int32_t)c.rng->u32()).b; });

    // ---- text labels / zones / fx / clothes
    Test<COMMAND_ATTACH_ANIMS_TO_MODEL, &AttachAnimsToModel>("2280 ATTACH_ANIMS_TO_MODEL", [&](Ctx& c) {
        auto& r = *c.rng;
        for (int i = 0; i < 6; ++i) *reinterpret_cast<int32_t*>(0xA44B70 + i * 0x1C + 0x18) = (int32_t)r.u32();
        c.opcode = Op(2280).I(r.below(3) ? -(int32_t)r.below(6) : (int32_t)(r.u32() & 0x7FFFFFFF)).Str8(Lbl(r).c_str()).b; });
    Test<COMMAND_APPEND_TO_NEXT_CUTSCENE, &AppendToNextCutsceneCmd>("2288 APPEND_TO_NEXT_CUTSCENE", [&](Ctx& c) {
        c.opcode = Op(2288).Str8(Lbl(*c.rng).c_str()).Str8(Lbl(*c.rng).c_str()).b; });
    Test<COMMAND_GET_NAME_OF_INFO_ZONE, &GetNameOfInfoZone>("2289 GET_NAME_OF_INFO_ZONE", [&](Ctx& c) {
        RandomFill(g_fakeZone, sizeof(g_fakeZone), *c.rng); if (c.rng->below(2)) std::memset(g_fakeZone + c.rng->below(9), 0, 8);
        c.watch = { { g_fakeZone, sizeof(g_fakeZone) } };
        c.opcode = Op(2289).Fl(c.rng->F(3000)).Fl(c.rng->F(3000)).Fl(c.rng->F(100)).GV(8).b; });
    Test<COMMAND_ADD_SPARKS, &AddSparks>("2283 ADD_SPARKS", [&](Ctx& c) {
        auto& r = *c.rng;
        auto dir = [&] { return r.below(8) == 0 ? 0.f : r.below(20) == 0 ? 1e-30f : r.below(20) == 0 ? 1e20f : r.F(20.f); };
        c.opcode = Op(2283).Fl(r.F(3000)).Fl(r.F(3000)).Fl(r.F(100)).Fl(dir()).Fl(dir()).Fl(dir()).I(Small(c, 40)).b; });
    Test<COMMAND_GET_CLOTHES_ITEM, &GetClothesItem>("2295 GET_CLOTHES_ITEM", [&](Ctx& c) {
        static uint8_t desc[0x100];
        RandomFill(desc, sizeof(desc), *c.rng);
        const int pl = c.rng->below(2);
        *reinterpret_cast<void**>(0xB7CD98 + pl * 0x190 + 8) = desc;
        c.opcode = Op(2295).I(pl).I(c.rng->below(14)).GV(4).GV(8).b; });

    // ---- ped searches: 4 fake peds in the pool, random placement / flags (callee recorders IsPedDead / GetPedsGroup / SetCharCreatedBy / AddEntityToList)
    auto PedSearchSetup = [&](Ctx& c) {
        auto& r = *c.rng;
        for (int i = 0; i < 4; ++i) {
            auto* p = g_peds[i];
            RandomPed(p, r);
            reinterpret_cast<uint8_t*>(p)[0x484] = r.below(5) ? 1 : (uint8_t)r.below(4);                    // m_nCreatedBy
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x1C) &= ~0x800u;                   // !bRemoveFromWorld mostly
            if (r.below(8) == 0) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x1C) |= 0x800u;
            if (r.below(2)) reinterpret_cast<uint8_t*>(p)[0x470] &= ~8; else if (r.below(6) == 0) reinterpret_cast<uint8_t*>(p)[0x470] |= 8;
            if (r.below(4)) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(p) + 0x474) &= ~0x800000u;
            p->m_nModelIndex = (uint16)r.below(4);
            p->m_placement.m_vPosn = CVector{ r.F(12.f, false), r.F(12.f, false), r.F(12.f, false) };
            if (r.below(4) == 0) { p->m_matrix = reinterpret_cast<decltype(p->m_matrix)>(g_mat); g_mat->GetPosition() = CVector{ r.F(12.f, false), r.F(12.f, false), r.F(12.f, false) }; }
        }
        for (auto& m : g_fakeMI) { RandomFill(m, sizeof(m), r); if (r.below(3)) *reinterpret_cast<uint16_t*>(m + 0x32) |= 1; }
        c.watch = { { (void*)0xC0EC24, 4 } };
        for (int i = 0; i < 4; ++i) c.watch.push_back({ g_peds[i], sizeof(CPed) });
        *reinterpret_cast<uint32_t*>(0xC0EC24) = r.below(100);
        const float rad = r.below(8) == 0 ? r.F(50.f) : r.f01() * 25.f;
        return rad;
    };
    Test<COMMAND_GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS, &GetRandomCharInSphereOnlyDrugsBuyers>("2206 GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS", [&](Ctx& c) {
        const float rad = PedSearchSetup(c);
        c.opcode = Op(2206).Fl(c.rng->F(10.f)).Fl(c.rng->F(10.f)).Fl(c.rng->F(10.f)).Fl(rad).GV(4).b; });
    Test<COMMAND_GET_RANDOM_CHAR_IN_SPHERE_NO_BRAIN, &GetRandomCharInSphereNoBrain>("2277 GET_RANDOM_CHAR_IN_SPHERE_NO_BRAIN", [&](Ctx& c) {
        const float rad = PedSearchSetup(c);
        c.opcode = Op(2277).Fl(c.rng->F(10.f)).Fl(c.rng->F(10.f)).Fl(c.rng->F(10.f)).Fl(rad).GV(4).b; });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// TASK_USE_CLOSEST_MAP_ATTRACTOR's search (the exe's unnamed 0x6002F0) against the port, on fake entities / model infos / 2d effects
static uint8_t  g_fx2d[24][0x48];          // fake C2dEffect structures (0x48 >= the attractor layout)
static uint8_t  g_fakeEntity[6][0x200];
static int      g_nEntities;
static void __cdecl H_FindObjectsInRange(const CVector* pt, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** out, bool b, bool v, bool p, bool o, bool d) {
    LogF(80, { pt->x, pt->y, pt->z, radius, (float)b2D, (float)maxCount, (float)b, (float)v, (float)p, (float)o, (float)d });
    for (int i = 0; i < g_nEntities; ++i) out[i] = reinterpret_cast<CEntity*>(g_fakeEntity[i]);
    *outCount = (int16_t)g_nEntities;
}
void CWorld::FindObjectsInRange(const CVector& pt, float radius, bool b2D, int16* outCount, int16 maxCount, CEntity** out, bool b, bool v, bool p, bool o, bool d) {
    H_FindObjectsInRange(&pt, radius, b2D, outCount, maxCount, out, b, v, p, o, d); }
static void* __fastcall H_Get2dEffect(void* mi, int, int idx) { LogU(81, { (uint32_t)(((uint8_t*)mi - g_fakeMI[0]) / 0x100), (uint32_t)idx }); return g_fx2d[(((uint8_t*)mi - g_fakeMI[0]) / 0x100 * 5 + idx) % 24]; }
C2dEffect* CBaseModelInfo::Get2dEffect(int32 idx) const { return reinterpret_cast<C2dEffect*>(H_Get2dEffect((void*)this, 0, idx)); }
static void* g_fakeMgr = (void*)0x1234;
static void* __cdecl H_GetPedAttractorManager() { return g_fakeMgr; }
static bool  __fastcall H_HasEmptySlot(void* self, int, void* fx, void* entity) { LogU(82, { (uint32_t)(((uint8_t*)fx - g_fx2d[0]) / 0x48), (uint32_t)(((uint8_t*)entity - g_fakeEntity[0]) / 0x200) }); return (g_hostRet >> (((uint8_t*)fx - g_fx2d[0]) / 0x48 & 15)) & 1; }
CPedAttractorManager* GetPedAttractorManager() { return reinterpret_cast<CPedAttractorManager*>(g_fakeMgr); }
bool CPedAttractorManager::HasEmptySlot(const C2dEffectPedAttractor* fx, const CEntity* e) { return H_HasEmptySlot(this, 0, (void*)fx, (void*)e); }
// CEntity::TransformFromObjectSpace(const CVector&) const: the exe's thiscall(sret, offset) -- a deterministic stand-in on both sides
static CVector* __fastcall H_TransformFromObjectSpace(void* self, int, CVector* out, const CVector* off) {
    const auto* b = (const uint8_t*)self; const float k = (float)((b - g_fakeEntity[0]) / 0x200) * 0.75f;
    *out = CVector{ off->x * 1.25f + k, off->y * 0.5f - k, off->z + 2.0f * k }; LogF(83, { off->x, off->y, off->z }); return out;
}
CVector CEntity::TransformFromObjectSpace(const CVector& off) const { CVector o; H_TransformFromObjectSpace((void*)this, 0, &o, &off); return o; }

static_assert(offsetof(CBaseModelInfo, m_n2dfxCount) == 0xD);
static void TestAttractorSearch() {
    if (!Wanted("0x6002F0 FindClosestScriptedAttractor")) return;
    Row row{ "0x6002F0 FindClosestScriptedAttractor" };
    Rng rng(0xA77AC7);
    for (int i = 0; i < g_cases; ++i) {
        auto& r = rng;
        g_hostRet = r.u32();
        // model infos: fixed 2dfx counts
        for (auto& m : g_fakeMI) { RandomFill(m, sizeof(m), r); m[0xD] = (uint8_t)r.below(5); }
        for (auto& e : g_fx2d) {
            RandomFill(e, sizeof(e), r);
            *reinterpret_cast<CVector*>(e) = CVector{ r.F(30.f, false), r.F(30.f, false), r.F(30.f, false) };
            e[0xC] = r.below(5) ? 3 : (uint8_t)r.below(5);                    // type
            e[0x34] = r.below(3) ? 5 : (uint8_t)r.below(10);                  // attractor type
            static const char* names[] = { "", "AAA", "aaa", "BBB", "ABCDEFG" };
            std::memset(e + 0x38, 0, 8); std::strcpy((char*)e + 0x38, names[r.below(5)]);
        }
        g_nEntities = r.below(7);
        for (int k = 0; k < g_nEntities; ++k) {
            RandomFill(g_fakeEntity[k], 0x40, r);
            *reinterpret_cast<uint16_t*>(g_fakeEntity[k] + 0x22) = (uint16)r.below(4);
        }
        const CVector pos{ r.F(40.f, false), r.F(40.f, false), r.F(40.f, false) };
        const float   radius = r.below(8) == 0 ? r.F(10.f) : r.f01() * 60.f;
        const int     model  = r.below(3) == 0 ? -1 : (int)r.below(4);
        const char*   names[] = { nullptr, "AAA", "aaa", "BBB", "" };
        const char*   name   = names[r.below(5)];
        const bool    checkFree = r.below(2);
        C2dEffect*    exclude = r.below(3) == 0 ? reinterpret_cast<C2dEffect*>(g_fx2d[r.below(24)]) : nullptr;
        const int     attrType = r.below(4) ? 5 : (int)r.below(10);

        void* exeFx = reinterpret_cast<void*>(0x1); void* exeEnt = reinterpret_cast<void*>(0x1);
        g_log.clear();
        const bool exeRet = oracle::Fn<unsigned char __cdecl(const CVector*, float, int, int, const char*, int, void*, void**, void**)>(0x6002F0)(&pos, radius, model, attrType, name, checkFree, exclude, &exeFx, &exeEnt) != 0;
        const auto exeLog = g_log;
        C2dEffect* cppFx = reinterpret_cast<C2dEffect*>(0x1); CEntity* cppEnt = reinterpret_cast<CEntity*>(0x1);
        g_log.clear();
        const bool cppRet = FindClosestScriptedAttractor(pos, radius, model, attrType, name, checkFree, exclude, cppFx, cppEnt);
        const auto cppLog = g_log;
        row.cases++;
        if (exeRet != cppRet || exeFx != (void*)cppFx || exeEnt != (void*)cppEnt || exeLog != cppLog) {
            row.bad++;
            if (row.bad <= 3) std::printf("  MISMATCH 0x6002F0 case %d: ret %d/%d fx %p/%p ent %p/%p log %zu/%zu\n", i, exeRet, cppRet, exeFx, (void*)cppFx, exeEnt, (void*)cppEnt, exeLog.size(), cppLog.size());
        }
    }
    g_rows.push_back(row);
    std::printf("  %-44s %6d cases  %s\n", row.name.c_str(), row.cases, row.bad ? "MISMATCH" : "ok");
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g22_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice

    // exe callees of the handlers -> host recorders
    oracle::Patch(0x6A1460, (void*)&H_UpdateMovingCollision);
    oracle::Patch(0x722870, (void*)&H_ForceRender);
    oracle::Patch(0x4087E0, (void*)&H_RequestModel);
    oracle::Patch(0x43F4B0, (void*)&H_FindNearestEntryExit);
    oracle::Patch(0x43EF00, (void*)&H_GetInSlot);
    oracle::Patch(0x5DF8F0, (void*)&H_IsPlayer);
    oracle::Patch(0x43E410, (void*)&H_AddEntryExitToStack);
    oracle::Patch(0x55FEC0, (void*)&H_StartExtraColour);
    oracle::Patch(0x55FF20, (void*)&H_StopExtraColour);
    oracle::Patch(0x701120, (void*)&H_NightVision);
    oracle::Patch(0x701140, (void*)&H_InfraredVision);
    oracle::Patch(0x464DC0, (void*)&H_SetCharCoordinates);
    oracle::Patch(0x4D5D10, (void*)&H_SkipButton);
    oracle::Patch(0x582300, (void*)&H_CreateNewMenu);
    oracle::Patch(0x582080, (void*)&H_SetColumnOrientation);
    oracle::Patch(0x5807E0, (void*)&H_CheckForSelected);
    oracle::Patch(0x5807C0, (void*)&H_CheckForAccept);
    oracle::Patch(0x581B30, (void*)&H_ActivateOneItem);
    oracle::Patch(0x580750, (void*)&H_SwitchOffMenu);
    oracle::Patch(0x581E00, (void*)&H_InsertMenu);
    oracle::Patch(0x581D70, (void*)&H_InsertOneMenuItemWithNumber);
    oracle::Patch(0x583F00, (void*)&H_SetBlipEntryExit);
    oracle::Patch(0x583E50, (void*)&H_SetCoordBlipAppearance);
    oracle::Patch(0x4883F0, (void*)&H_ObjectInAngledArea);
    oracle::Patch(0x6CAC20, (void*)&H_SetGearUp);
    oracle::Patch(0x6CAC70, (void*)&H_SetGearDown);
    oracle::Patch(0x474800, (void*)&H_ScriptAttachAnimGroup);
    oracle::Patch(0x474750, (void*)&H_AddToListOfSpecialAnimGroups);
    oracle::Patch(0x4D5DB0, (void*)&H_AppendToNextCutscene);
    oracle::Patch(0x49F040, (void*)&H_AddSparks);
    oracle::Patch(0x572360, (void*)&H_FindSmallestZone);
    oracle::Patch(0x464D70, (void*)&H_IsPedDead);
    oracle::Patch(0x5F7E80, (void*)&H_GetPedsGroup);
    oracle::Patch(0x5E47E0, (void*)&H_SetCharCreatedBy);
    oracle::Patch(0x4637E0, (void*)&H_AddEntityToList);
    oracle::Patch(0x8229B6, (void*)&H_CRT_stricmp);
    // the attractor search callees
    oracle::Patch(0x564A20, (void*)&H_FindObjectsInRange);
    oracle::Patch(0x4C4C70, (void*)&H_Get2dEffect);
    oracle::Patch(0x5EE190, (void*)&H_GetPedAttractorManager);
    oracle::Patch(0x5EBB00, (void*)&H_HasEmptySlot);
    oracle::Patch(0x5334F0, (void*)&H_TransformFromObjectSpace);

    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    std::printf("script_oracle_g22_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    TestAttractorSearch();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
