// S6-F: differential test of SCRIPT COMMAND HANDLERS g20/g21 against the original machine code (exe oracle, see game_oracle.h).
// Same machinery as script_oracle_test.cpp: the handler TUs (source/game_sa/Scripts/Commands/Ported/Group20a/20b/21a/21b.cpp) are #included, the exe's group
// processors (ProcessCommands2000To2099 @0x472310, 2100To2199 @0x470A90) and the CommandParser instantiation of the C++ handler run on the same synthetic
// opcode buffer; ScriptSpace, IP, compare flag, watched memory and the callee log are compared.
// usage: script_oracle_g20_21_test.exe [-v] [-n cases] [name-substring ...]   (env RW_EXE_ORACLE=<path of gta_sa_compact.exe>)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include "../../source/game_sa/Scripts/Commands/Ported/Group20a.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group20b.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group21a.cpp"
#include "../../source/game_sa/Scripts/Commands/Ported/Group21b.cpp"

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
    constexpr unsigned groupFn[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x472310, 0x470A90 };
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

static void TestAll() {
    auto VehWatch = [](Ctx& c) { c.watch = { { g_veh, sizeof(CAutomobile) } }; };
    auto PedWatch = [](Ctx& c) { c.watch = { { g_ped, sizeof(CPed) } }; };
    auto ObjWatch = [](Ctx& c) { c.watch = { { g_obj, sizeof(CObject) } }; };
    auto Veh = [&](Ctx& c, int cmd) { RandomVehicle(*c.rng); VehWatch(c); return Op(cmd).I(g_vehRef); };
    auto Ped = [&](Ctx& c, int cmd) { RandomPed(*c.rng); PedWatch(c); return Op(cmd).I(g_pedRef); };
    auto Obj = [&](Ctx& c, int cmd) { RandomObject(*c.rng); ObjWatch(c); return Op(cmd).I(g_objRef); };
    auto SmallOrAny = [](Ctx& c) { return c.rng->below(3) ? (int32_t)c.rng->below(6) : (int32_t)c.rng->u32(); };

    // ---- globals / script variables
    Test<COMMAND_GET_CURRENT_DAY_OF_WEEK, &GetCurrentDayOfWeek>("2000 GET_CURRENT_DAY_OF_WEEK", [](Ctx& c) {
        c.watch = { { (void*)0xB7014E, 1 } }; *reinterpret_cast<uint8_t*>(0xB7014E) = (uint8_t)c.rng->u32(); c.opcode = Op(2000).GV(4).b; });
    Test<COMMAND_GET_CURRENT_DATE, &GetCurrentDate>("2101 GET_CURRENT_DATE", [](Ctx& c) {
        c.watch = { { (void*)0xB70154, 2 } }; *reinterpret_cast<uint8_t*>(0xB70154) = (uint8_t)c.rng->u32(); *reinterpret_cast<uint8_t*>(0xB70155) = (uint8_t)c.rng->u32();
        c.opcode = Op(2101).GV(4).GV(8).b; });
    Test<COMMAND_IS_INT_LVAR_EQUAL_TO_INT_VAR, &IsIntLvarEqualToIntVar>("2006 IS_INT_LVAR_EQUAL_TO_INT_VAR", [](Ctx& c) {
        const uint32_t v = c.rng->below(3) ? c.rng->below(4) : c.rng->u32();
        c.lv[1] = c.rng->below(2) ? v : c.rng->below(4);
        *reinterpret_cast<uint32_t*>(&ScriptSpaceRef()[24]) = v;
        c.opcode = Op(2006).LV(1).GV(24).b; });
    Test<COMMAND_IS_FLOAT_LVAR_EQUAL_TO_FLOAT_VAR, &IsFloatLvarEqualToFloatVar>("2007 IS_FLOAT_LVAR_EQUAL_TO_FLOAT_VAR", [](Ctx& c) {
        const float v = c.rng->F(5.f);
        c.lv[2] = FB(c.rng->below(2) ? v : c.rng->F(5.f));
        *reinterpret_cast<float*>(&ScriptSpaceRef()[24]) = v;
        c.opcode = Op(2007).LV(2).GV(24).b; });
    Test<COMMAND_DISPLAY_HUD, &DisplayHud>("2086 DISPLAY_HUD", [](Ctx& c) { c.watch = { { (void*)0xA444A0, 1 } }; c.opcode = Op(2086).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_MAX_FIRE_GENERATIONS, &SetMaxFireGenerations>("2088 SET_MAX_FIRE_GENERATIONS", [](Ctx& c) { c.watch = { { (void*)0xB728E0, 4 } }; c.opcode = Op(2088).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_POOL_TABLE_COORDS, &SetPoolTableCoords>("2096 SET_POOL_TABLE_COORDS", [](Ctx& c) {
        c.watch = { { (void*)0x8CDEF4, 0x18 } };
        c.opcode = Op(2096).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).Fl(c.rng->F(500)).Fl(c.rng->F(500)).Fl(c.rng->F(50)).b; });
    Test<COMMAND_GET_CITY_PLAYER_IS_IN, &GetCityPlayerIsIn>("2114 GET_CITY_PLAYER_IS_IN", [](Ctx& c) {
        c.watch = { { (void*)0xBA6718, 4 } }; *reinterpret_cast<uint32_t*>(0xBA6718) = c.rng->below(5); c.opcode = Op(2114).I(c.rng->below(2)).GV(4).b; });
    Test<COMMAND_SET_HEADING_FOR_ATTACHED_PLAYER, &SetHeadingForAttachedPlayer>("2136 SET_HEADING_FOR_ATTACHED_PLAYER", [](Ctx& c) {
        c.watch = { { (void*)0xA44498, 8 } }; c.opcode = Op(2136).I(0).Fl(c.rng->F(400)).Fl(c.rng->F(30)).b; });
    Test<COMMAND_IS_ATTACHED_PLAYER_HEADING_ACHIEVED, &IsAttachedPlayerHeadingAchieved>("2145 IS_ATTACHED_PLAYER_HEADING_ACHIEVED", [](Ctx& c) {
        c.watch = { { (void*)0xA44498, 4 } }; *reinterpret_cast<float*>(0xA44498) = c.rng->F(1.f); c.opcode = Op(2145).I(0).b; });

    // ---- ped commands (whole fake ped compared)
    Test<COMMAND_SET_CHAR_SHOOT_RATE, &SetCharShootRate>("2013 SET_CHAR_SHOOT_RATE", [&](Ctx& c) { c.opcode = Ped(c, 2013).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_GIVE_MELEE_ATTACK_TO_CHAR, &GiveMeleeAttackToChar>("2046 GIVE_MELEE_ATTACK_TO_CHAR", [&](Ctx& c) {
        auto op = Ped(c, 2046); if (c.rng->below(2)) reinterpret_cast<uint8_t*>(g_ped)[0x72D] = (uint8_t)c.rng->below(8);
        c.opcode = op.I(c.rng->below(3) ? (int32_t)c.rng->below(8) : (int32_t)c.rng->u32()).I(c.rng->below(3) ? (int32_t)c.rng->below(8) : (int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_CHAR_KINDA_STAY_IN_SAME_PLACE, &SetCharKindaStayInSamePlace>("2070 SET_CHAR_KINDA_STAY_IN_SAME_PLACE", [&](Ctx& c) { c.opcode = Ped(c, 2070).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_CHAR_WEAPON_SKILL, &SetCharWeaponSkill>("2074 SET_CHAR_WEAPON_SKILL", [&](Ctx& c) { c.opcode = Ped(c, 2074).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_HEADING_LIMIT_FOR_ATTACHED_CHAR, &SetHeadingLimitForAttachedChar>("2183 SET_HEADING_LIMIT_FOR_ATTACHED_CHAR", [&](Ctx& c) {
        c.opcode = Ped(c, 2183).I((int32_t)c.rng->u32()).Fl(c.rng->F(400)).b; });
    Test<COMMAND_GET_CHAR_HIGHEST_PRIORITY_EVENT, &GetCharHighestPriorityEvent>("2062 GET_CHAR_HIGHEST_PRIORITY_EVENT", [&](Ctx& c) {
        auto op = Ped(c, 2062); static uint8_t intel[0x100]; RandomFill(intel, 0x100, *c.rng); *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(g_ped) + 0x47C) = intel;
        c.watch.push_back(Watch{ (void*)intel, 0x100 }); c.opcode = op.GV(4).b; });

    // ---- vehicle commands (whole fake vehicle compared)
    Test<COMMAND_SET_VEHICLE_AREA_VISIBLE, &SetVehicleAreaVisible>("2112 SET_VEHICLE_AREA_VISIBLE", [&](Ctx& c) { c.opcode = Veh(c, 2112).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SELECT_WEAPONS_FOR_VEHICLE, &SelectWeaponsForVehicle>("2113 SELECT_WEAPONS_FOR_VEHICLE", [&](Ctx& c) { c.opcode = Veh(c, 2113).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_VEHICLE_CAN_BE_TARGETTED, &SetVehicleCanBeTargetted>("2126 SET_VEHICLE_CAN_BE_TARGETTED", [&](Ctx& c) { c.opcode = Veh(c, 2126).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_CAR_CAN_BE_VISIBLY_DAMAGED, &SetCarCanBeVisiblyDamaged>("2130 SET_CAR_CAN_BE_VISIBLY_DAMAGED", [&](Ctx& c) { c.opcode = Veh(c, 2130).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_HELI_REACHED_TARGET_DISTANCE, &SetHeliReachedTargetDistance>("2131 SET_HELI_REACHED_TARGET_DISTANCE", [&](Ctx& c) { c.opcode = Veh(c, 2131).I((int32_t)c.rng->u32()).b; });
    Test<COMMAND_SET_VEHICLE_DIRT_LEVEL, &SetVehicleDirtLevel>("2168 SET_VEHICLE_DIRT_LEVEL", [&](Ctx& c) { c.opcode = Veh(c, 2168).Fl(c.rng->F(20)).b; });
    Test<COMMAND_SET_CAR_ENGINE_BROKEN, &SetCarEngineBroken>("2077 SET_CAR_ENGINE_BROKEN", [&](Ctx& c) { c.opcode = Veh(c, 2077).I(c.rng->below(3)).b; });
    Test<COMMAND_DOES_CAR_HAVE_HYDRAULICS, &DoesCarHaveHydraulics>("2051 DOES_CAR_HAVE_HYDRAULICS", [&](Ctx& c) {
        auto op = Veh(c, 2051); reinterpret_cast<uint8_t*>(g_veh)[0x594] = (uint8_t)c.rng->below(3); c.opcode = op.b; });
    Test<COMMAND_SET_HELI_BLADES_FULL_SPEED, &SetHeliBladesFullSpeed>("2085 SET_HELI_BLADES_FULL_SPEED", [&](Ctx& c) {
        auto op = Veh(c, 2085); reinterpret_cast<uint8_t*>(g_veh)[0x594] = (uint8_t)c.rng->below(6); c.opcode = op.b; });
    Test<COMMAND_GET_CAR_UPRIGHT_VALUE, &GetCarUprightValue>("2111 GET_CAR_UPRIGHT_VALUE", [&](Ctx& c) { c.opcode = Veh(c, 2111).GV(4).b; });
    Test<COMMAND_SET_VEHICLE_AIR_RESISTANCE_MULTIPLIER, &SetVehicleAirResistanceMultiplier>("2187 SET_VEHICLE_AIR_RESISTANCE_MULTIPLIER", [&](Ctx& c) {
        auto op = Veh(c, 2187); if (c.rng->below(3) == 0) g_handling->m_fDragMult = c.rng->F(0.02f); if (c.rng->below(8) == 0) g_veh->m_pHandlingData = nullptr;
        c.opcode = op.Fl(c.rng->F(5)).b; });
    Test<COMMAND_ADD_TO_CAR_ROTATION_VELOCITY, &AddToCarRotationVelocity>("2010 ADD_TO_CAR_ROTATION_VELOCITY", [&](Ctx& c) {
        c.opcode = Veh(c, 2010).Fl(c.rng->F(10)).Fl(c.rng->F(10)).Fl(c.rng->F(10)).b; });
    Test<COMMAND_SET_CAR_ROTATION_VELOCITY, &SetCarRotationVelocity>("2011 SET_CAR_ROTATION_VELOCITY", [&](Ctx& c) {
        c.watch.clear(); c.opcode = Veh(c, 2011).Fl(c.rng->F(10)).Fl(c.rng->F(10)).Fl(c.rng->F(10)).b; c.watch.push_back({ (void*)0xB7CB5C, 4 });
        *reinterpret_cast<float*>(0xB7CB5C) = c.rng->f01() * 3.f; });

    // ---- object commands
    Test<COMMAND_SET_OBJECT_COLLISION_DAMAGE_EFFECT, &SetObjectCollisionDamageEffect>("2039 SET_OBJECT_COLLISION_DAMAGE_EFFECT", [&](Ctx& c) { c.opcode = Obj(c, 2039).I(c.rng->below(3)).b; });
    Test<COMMAND_SET_OBJECT_ONLY_DAMAGED_BY_PLAYER, &SetObjectOnlyDamagedByPlayer>("2165 SET_OBJECT_ONLY_DAMAGED_BY_PLAYER", [&](Ctx& c) { c.opcode = Obj(c, 2165).I(c.rng->below(3)).b; });
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g20_21_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available
    { unsigned cw; _controlfp_s(&cw, _PC_24, _MCW_PC); }   // the game runs at PC=24 after D3D CreateDevice
    oracle::Patch(0x542800, (void*)&H_AddMoving);
    static CRunningScript script;
    g_S = &script;
    SetupFakeWorld();
    g_fakeVtbl[0x10 / 4] = (void*)&H_SetIsStatic;
    std::printf("script_oracle_g20_21_test: %d random cases per command (PC24)\n", g_cases);
    TestAll();
    int bad = 0, n = 0;
    for (auto& r : g_rows) { bad += r.bad; n++; }
    std::printf("\n%d commands, %d with mismatches\n", n, (int)std::count_if(g_rows.begin(), g_rows.end(), [](const Row& r) { return r.bad > 0; }));
    return bad ? 1 : 0;
}
