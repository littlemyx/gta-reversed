#ifdef NOTSA_MDRIVER
#include "StdInc.h"
#include "mdriver.h"
#include "Garages.h"
#include "loadlog.h"
#include "standalone/Fixups.h"
#include "Scripts/TheScripts.h"
#include "Scripts/RunningScript.h"
#include "CutsceneMgr.h"
#include "EntryExitManager.h"
#include "CarEnterExit.h"
#include "TagManager.h"
#include "CarCtrl.h"
#include "Streaming.h"
#include "Camera.h"
#include "Game.h"
#include "TimeCycle.h"
#include "Tasks/TaskTypes/TaskSimpleHoldEntity.h"
#include "scmtrace.h"
#include "PedType.h"
#include "PedGroups.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFoot.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFootMelee.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFootArmed.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFootStealth.h"
#include "Tasks/TaskTypes/TaskSimpleCarSetPedInAsDriver.h"
#include "Tasks/TaskTypes/TaskSimpleCarSetPedInAsPassenger.h"
#include <bit>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <memory>
#include <optional>
#include <array>

namespace notsa::mdriver {
namespace {
bool Generic();
struct Cond { std::string expr, op; double val; };
struct Step { std::string name; std::vector<Cond> conds; std::vector<std::string> actions; };
std::vector<Step>        g_steps;
std::string              g_script;           // thread name
int                      g_watchVeh = -1, g_buddyRef = -1, g_stall = 600, g_maxAnswers = 12;
bool                     g_loaded = false, g_started = false, g_done = false, g_failed = false;
std::FILE*               g_out = nullptr;
void                   (*g_push)(const char*, bool) = nullptr;
void                   (*g_run)(const std::string&) = nullptr;
CRunningScript*          g_thread = nullptr;
int                      g_sameCount = 0;
const uint8*             g_lastIp = nullptr;
std::map<int, int>       g_answers;
std::vector<std::string> g_queue;            // actions of the running step
size_t                   g_qpos = 0;
int                      g_wait = 0;         // frames left of a `wait`
std::string              g_until; int g_untilLeft = 0; // `waituntil` condition / frames left
int                      g_quitIn = -1;
uint64_t                 g_clock = 0;

uint32 Loc(int n) { return CTheScripts::LocalVariablesForCurrentMission[n].uParam; }
float  LocF(int n) { return CTheScripts::LocalVariablesForCurrentMission[n].fParam; }
CPed*  Cj() { return FindPlayerPed(); }

CEntryExit* FindEnex(const char* name, int area) {
    for (auto& e : CEntryExitManager::GetPool()->GetAllValid()) {
        if (!std::strncmp(e.m_szName, name, 8) && (int)e.m_nArea == area) return &e;
    }
    return nullptr;
}
// AD3: interior-aware warps (generic mode only): the player's area after a warp to (x,y,z). z below the interior band -> 0; in the band the interior side (m_nArea != 0) entry exit nearest in 3D (< 120 m), else unchanged
int AreaForPos(float x, float y, float z, int cur) {
    if (z < 800.f) return 0;
    CEntryExit* best = nullptr; float bd = 120.f;
    for (auto& e : CEntryExitManager::GetPool()->GetAllValid()) {
        if (e.m_nArea == 0) continue;
        const auto c = e.GetPosition(); const float d = std::sqrt((c.x - x) * (c.x - x) + (c.y - y) * (c.y - y) + (c.z - z) * (c.z - z));
        if (d < bd) { bd = d; best = &e; }
    }
    return best ? (int)best->m_nArea : cur;
}
// what CEntryExit::TransitionFinished does to the player once the area changes (area code, CGame::currArea, enex pointer + stack, visible objects, the held object)
void SetPlayerArea(CPed* ped, int area, float x, float y, float z) {
    if (!ped || (int)ped->GetAreaCode() == area) return;
    CEntryExit* in = nullptr;
    if (area != 0) { float bd = 1e9f; for (auto& e : CEntryExitManager::GetPool()->GetAllValid()) { if ((int)e.m_nArea != area) continue; const auto c = e.GetPosition(); const float d = (c.x - x) * (c.x - x) + (c.y - y) * (c.y - y) + (c.z - z) * (c.z - z); if (d < bd) { bd = d; in = &e; } } }
    CGame::currArea = (eAreaCodes)area;
    ped->SetAreaCode((eAreaCodes)area);
    if (ped->m_pVehicle && ped->bInVehicle) ped->m_pVehicle->SetAreaCode((eAreaCodes)area);
    if (auto* h = ped->GetEntityThatThisPedIsHolding()) h->SetAreaCode((eAreaCodes)area);
    if (area == 0) { ped->m_pEnex = nullptr; CEntryExitManager::ms_entryExitStackPosn = 0; CTimeCycle::StopExtraColour(0); }
    else if (in && in->m_pLink) { ped->m_pEnex = in->m_pLink; CEntryExitManager::AddEntryExitToStack(in->m_pLink); }
    CEntryExitManager::SetAreaCodeForVisibleObjects();
}
CObject* FirstCrate(int lo, int hi) {
    for (int n = lo; n <= hi; n++) {
        if (auto* o = CPools::GetObject(Loc(n)); o && o->objectFlags.bIsLiftable) return o;
    }
    return nullptr;
}
double Eval(const std::string& expr) {
    char name[32]{}; std::vector<double> a;
    { std::stringstream ss(expr); std::string t; std::getline(ss, t, ':'); std::snprintf(name, sizeof name, "%s", t.c_str()); while (std::getline(ss, t, ':')) a.push_back(std::atof(t.c_str())); }
    auto* ped = Cj();
    if (!std::strcmp(name, "stall"))     return g_sameCount;
    if (!std::strcmp(name, "frame"))     return CTimer::GetFrameCounter();
    if (!std::strcmp(name, "cutscene"))  return CCutsceneMgr::IsCutsceneProcessing();
    if (name[0] == 'L' && std::isdigit((unsigned char)name[1])) return Loc(std::atoi(name + 1));
    if (!std::strcmp(name, "area"))      return ped ? (int)ped->GetAreaCode() : -1;
    if (!std::strcmp(name, "invehicle")) return ped && ped->bInVehicle;
    if (!std::strcmp(name, "holding"))   return ped && ped->GetEntityThatThisPedIsHolding() != nullptr;
    if (!std::strcmp(name, "fading"))    return TheCamera.GetFading();
    if (!std::strcmp(name, "hasveh"))    return CPools::GetVehicle(Loc((int)a[0])) != nullptr;
    if (!std::strcmp(name, "peddist"))   return ped ? std::hypot(ped->GetPosition().x - a[0], ped->GetPosition().y - a[1]) : 1e9;
    if (!std::strcmp(name, "vehdist")) { auto* v = CPools::GetVehicle(Loc((int)a[0])); return v ? std::hypot(v->GetPosition().x - a[1], v->GetPosition().y - a[2]) : 1e9; }
    if (!std::strcmp(name, "crate"))     return FirstCrate((int)a[0], (int)a[1]) != nullptr;
    return 0;
}
bool Cmp(double v, const std::string& op, double r) {
    return op == "==" ? v == r : op == "!=" ? v != r : op == ">" ? v > r : op == "<" ? v < r : op == ">=" ? v >= r : v <= r;
}
void Emit(const std::string& cmd) { // run an action through the replay path and record it
    if (g_out) { std::fprintf(g_out, "at:%llu\ncmd:%s\n# rec=%d gf=%u\n", (unsigned long long)g_clock, cmd.c_str(), notsa::rr::LoadLogRecFrame(), (unsigned)notsa::rr::LoadLogFrameCounter()); std::fflush(g_out); } // `# rec=` (no colon: the injector skips it) = recorded frame / CTimer frame the action ran in
    char name[32]{}; std::sscanf(cmd.c_str(), "%31[^:]", name);
    if (!Cmd(name, cmd.size() > std::strlen(name) ? cmd.substr(std::strlen(name) + 1) : "") && g_run) g_run(cmd);
}
std::string F(double v) { char b[40]; std::snprintf(b, sizeof b, "%.9g", (double)(float)v); return b; }
// the ped put at (x,y,z) with heading h (the player walked there)
std::string AreaSuffix(float x, float y, float z, int forced = -1) { // generic mode: ":<area>" when the warp changes the player's area (recorded in the route line: replay needs no table)
    auto* c = Cj(); if (!Generic() || !c) return "";
    const int a = forced >= 0 ? forced : AreaForPos(x, y, z, (int)c->GetAreaCode());
    return a != (int)c->GetAreaCode() ? ":" + std::to_string(a) : "";
}
void PedTo(float x, float y, float z, float h, bool cam) { Emit("mdped:" + F(x) + ":" + F(y) + ":" + F(z) + ":" + F(h) + ":" + (cam ? "1" : "0") + AreaSuffix(x, y, z)); }
void StandBefore(float ex, float ey, float ez, float dist) { // ryder1_driver.ts standBefore: CJ `dist` m from the entity along the line from it to where he is now, facing it
    auto* c = Cj(); if (!c) return;
    float dx = c->GetPosition().x - ex, dy = c->GetPosition().y - ey;
    const float len = std::hypot(dx, dy) ? (float)std::hypot(dx, dy) : 1.f; dx /= len; dy /= len;
    PedTo(ex + dx * dist, ey + dy * dist, ez + 0.3f, std::atan2(dx, -dy), false);
}
void Dispatch(const std::string& line) { // one action (`wait` / `waituntil` only arm the counters)
    std::stringstream ss(line); std::string op; ss >> op;
    auto* ped = Cj();
    if (op == "wait") { ss >> g_wait; }
    else if (op == "waituntil") { std::string e, o; double v; int n; ss >> e >> o >> v >> n; g_until = e + " " + o + " " + std::to_string(v); g_untilLeft = n; }
    else if (op == "key") { std::string d, n; ss >> d >> n; Emit("mdkey:" + d + ":" + n); }
    else if (op == "pedwarp") { float x, y, z; ss >> x >> y >> z; PedTo(x, y, z, ped ? ped->GetHeading() : 0.f, false); }
    else if (op == "vehwarp") { float x, y, z; ss >> x >> y >> z; Emit("mdveh:" + F(x) + ":" + F(y) + ":" + F(z)); }
    else if (op == "standcrate") { int lo, hi; float d; ss >> lo >> hi >> d; if (auto* o = FirstCrate(lo, hi)) StandBefore(o->GetPosition().x, o->GetPosition().y, o->GetPosition().z, d); }
    else if (op == "standobj") { int h; float d; ss >> h >> d; if (auto* o = CPools::GetObject(h)) StandBefore(o->GetPosition().x, o->GetPosition().y, o->GetPosition().z, d); } // AD3: generic crate pickup
    else if (op == "standdrop") { int lx, ly; float d; ss >> lx >> ly >> d; if (ped) StandBefore(LocF(lx), LocF(ly), ped->GetPosition().z - 0.3f, d); }
    else if (op == "standenex") { // entryExitWalk.ts standBeforeEntryExit; `link` = the linked (other side) entry exit
        std::string name, link; int area; float d; ss >> name >> area >> link >> d;
        auto* e = FindEnex(name.c_str(), area); if (e && link == "link") e = e->m_pLink;
        if (!e) return;
        const auto c = e->GetPosition(); float dx = c.x - e->m_vecExitPos.x, dy = c.y - e->m_vecExitPos.y; const float len = (float)std::hypot(dx, dy);
        PedTo(c.x - dx / len * d, c.y - dy / len * d, c.z, std::atan2(-dx / len, dy / len), true);
    }
    else if (op == "entercar") { // enterCarByKey: `d` m left of the left door, facing the car
        int slot; float d; ss >> slot >> d;
        if (auto* v = CPools::GetVehicle(Loc(slot))) {
            const auto door = CCarEnterExit::GetPositionToOpenCarDoor(v, CAR_DOOR_LF); const CVector l = -v->GetRight();
            PedTo(door.x + l.x * d, door.y + l.y * d, v->GetPosition().z + 0.2f, std::atan2(l.x, -l.y), false);
        }
    }
}
void Load(const char* path) {
    std::ifstream f(path); std::string l; Step* cur = nullptr;
    while (std::getline(f, l)) {
        const auto h = l.find('#'); if (h != std::string::npos) l.resize(h);
        std::stringstream ss(l); std::string op; if (!(ss >> op)) continue;
        if (op == "script") ss >> g_script;
        else if (op == "stall") ss >> g_stall;
        else if (op == "answers") ss >> g_maxAnswers;
        else if (op == "step") { g_steps.emplace_back(); cur = &g_steps.back(); ss >> cur->name; }
        else if (op == "if" && cur) { Cond c; ss >> c.expr >> c.op >> c.val; cur->conds.push_back(c); }
        else if (op == "do" && cur) { std::string rest; std::getline(ss, rest); cur->actions.push_back(rest); }
    }
    g_loaded = true;
}
CRunningScript* FindThread() {
    for (auto* s = CTheScripts::pActiveScripts; s; s = s->m_pNext) {
        if (!_strnicmp((const char*)s->m_szName, g_script.c_str(), 8)) return s;
    }
    return nullptr;
}
// ---- AD1/AD4: generic driver (NOTSA_MDRIVER_TABLE = driver-classes.tsv, NOTSA_MDRIVER_RESULT = result line file). AD4: an exact mirror of the TS driver rules (web/packages/game/test/autodrive: drive.ts, driverTable.ts decide(), branch.ts, actions.ts).
// The C++-only inventions of AD3 (interior-aware warp, liftable pickup) are behind NOTSA_MDRIVER_AD3 (off by default); the rotation / LATEST experiments are removed.
struct Arg { uint32_t bits = 0; int kind = 0; };
struct Row { uint16_t op; int match; /*0 any 1 plain 2 not*/ std::string name, action; std::vector<std::string> args; int settle; };
std::vector<Row>         g_rows;
// a conditional command of the root thread as the TS StallSink records it (opcode, NOT form, state, decoded operands, the WAIT count, where the following GOTO_IF_FALSE led)
struct Rec { const uint8_t* p; uint16_t raw; int cond; int waitSeq; std::vector<Arg> args; bool hasBranch = false, jumped = false, waitAfter = false; const uint8_t *target = nullptr, *fall = nullptr; uint32_t vref[2] = { 0, 0 }; /* CR22: identity of operand 0 / 1 when it is a variable: (type << 16) | offset-or-index, else 0 */ };
using RecP = std::shared_ptr<Rec>;
std::map<const uint8_t*, RecP> g_deadConds; // CR21
std::map<const uint8_t*, RecP> g_flagConds; // CR22: every plain `var == number` condition seen (IS_INT_VAR/LVAR_EQUAL_TO_NUMBER): the flag tests of the fail-flag guard
std::set<uint32_t> g_areaVars; int g_enterArea = -1; // CR23: variables written by GET_AREA_VISIBLE (077E); the interior id a compare of one of them asks for
std::map<uint32_t, int> g_coordVeh; struct SphSearch { int veh; int model; float radius; }; std::map<uint32_t, SphSearch> g_sphSearch; SphSearch g_sphCur{}; // CR24: GET_CAR_COORDINATES x var -> vehicle handle; GET_RANDOM_CAR_IN_SPHERE (073E) result var -> {the vehicle whose coordinates are the centre, model, radius}
std::map<uint32_t, std::array<float, 4>> g_tagAreas; float g_tagRect[4]{}; // CR22: destination variable -> area of the last GET_PERCENTAGE_TAGGED_IN_AREA (0702) that wrote it; the area of the picked decision
std::deque<RecP>         g_recs;             // the last 64 conditional commands (KEEP)
std::vector<RecP>        g_group; bool g_fallPending = false; // CR19: the conditions of the last GOTO_IF_FALSE, and whether it fell through (the next command decides waitAfter)
std::vector<RecP>        g_pending;          // the conditions since the last GOTO_IF_FALSE (max 12)
int                      g_waitSeq = 0;
std::map<int, int>       g_arity;            // opcode -> operand count (tools/standalone/scmscan/argcounts.tsv, exe derived; -1 variadic)
uint32_t                 g_frame0 = 0;
std::string              g_name = "?", g_stopReason, g_failText, g_lastLabel;
int                      g_stopOp = -1; uint32_t g_stopIp = 0;
bool                     g_pass = false, g_fail = false, g_endedFail = false;
int                      g_passFrame = -1, g_failFrame = -1, g_endFrame = -1, g_killFrame = -1000000, g_killSettle = 0;
int                      g_maxFrames = 30000, g_fightClock = 0, g_fightKills = 0;
std::set<int>            g_enemyTypes;       // ped types shot for going after CJ himself (TS enemyTypes)
std::map<const uint8_t*, int> g_attemptsAt;  // actions per stall ip
std::map<std::string, int>    g_noProgress;  // (ip, row, args) -> times
bool Generic() { static const bool g = std::getenv("NOTSA_MDRIVER_TABLE") != nullptr; return g; }
bool Ad3() { static const bool g = std::getenv("NOTSA_MDRIVER_AD3") != nullptr; return g; }

int GFrame() { return (int)(CTimer::GetFrameCounter() - g_frame0); }
const uint8_t* Resolve(const CRunningScript* s, int32_t off) { return off < 0 ? s->m_BaseIP - off : CTheScripts::ScriptSpace.data() + off; }
tScriptParam* LocalAt(const CRunningScript* s, int i) { auto* m = const_cast<CRunningScript*>(s); return m->m_ThisMustBeTheOnlyMissionRunning ? &CTheScripts::LocalVariablesForCurrentMission[i] : &m->m_LocalVars[i]; }
// P4C1: the look-ahead decodes a command that has not run yet: an array index variable can hold anything then (missions 39 / 50 / 74 read far outside ScriptSpace / the locals) -> an out-of-range operand ends the decode (the caller sees fewer arguments)
bool LocalOk(const CRunningScript* s, int64_t i) { return i >= 0 && (size_t)i < (const_cast<CRunningScript*>(s)->m_ThisMustBeTheOnlyMissionRunning ? CTheScripts::LocalVariablesForCurrentMission.size() : s->m_LocalVars.size()); }
bool GlobalOk(int64_t off) { return off >= 0 && (size_t)off + 4 <= CTheScripts::ScriptSpace.size(); }
std::vector<Arg> DecodeArgs(const CRunningScript* s, const uint8_t* p, int n) { // the first n arguments of the command at p (SCM typed arguments)
    std::vector<Arg> r; p += 2;
    for (int k = 0; k < n; k++) {
        Arg a; const uint8_t t = *p++;
        switch (t) {
        case 1: a.bits = *(const uint32_t*)p; p += 4; break;
        case 2: a.bits = *(const uint32_t*)(CTheScripts::ScriptSpace.data() + *(const uint16_t*)p); a.kind = 2; p += 2; break;
        case 3: if (!LocalOk(s, *(const uint16_t*)p)) return r; a.bits = LocalAt(s, *(const uint16_t*)p)->uParam; a.kind = 2; p += 2; break;
        case 4: a.bits = (uint32_t)(int32_t)*(const int8_t*)p; p += 1; break;
        case 5: a.bits = (uint32_t)(int32_t)*(const int16_t*)p; p += 2; break;
        case 6: a.bits = *(const uint32_t*)p; a.kind = 1; p += 4; break;
        case 7: case 8: { // array: u16 base, u16 index var, u8 size, u8 flags (bit 7: index var is GLOBAL = scm::ArrayAccess::IdxVarIsGlobal; CR15: was inverted)
            const uint16_t base = *(const uint16_t*)p, iv = *(const uint16_t*)(p + 2); const uint8_t fl = p[5];
            if ((fl & 0x80) ? !GlobalOk(iv) : !LocalOk(s, iv)) return r;
            const int idx = (int)((fl & 0x80) ? *(const uint32_t*)(CTheScripts::ScriptSpace.data() + iv) : LocalAt(s, iv)->uParam);
            if (t == 7 ? !GlobalOk((int64_t)base + 4 * (int64_t)idx) : !LocalOk(s, (int64_t)base + idx)) return r;
            a.bits = t == 7 ? *(const uint32_t*)(CTheScripts::ScriptSpace.data() + base + 4 * idx) : LocalAt(s, base + idx)->uParam; a.kind = 2; p += 6; break; }
        case 9: p += 8; break;
        case 0xA: p += 16; break;
        case 0xE: p += 1 + *p; break;
        default: return r;
        }
        r.push_back(a);
    }
    return r;
}
float  AF(const Arg& a) { return a.kind == 0 ? (float)(int32_t)a.bits : std::bit_cast<float>(a.bits); }
std::optional<double> ArgFloat(const Rec& r, size_t i) { if (i >= r.args.size()) return std::nullopt; return (double)AF(r.args[i]); } // driverTable.argFloat
std::optional<double> ArgInt(const Rec& r, size_t i) { // driverTable.argInt: a float constant truncates, an integer / variable is itself
    if (i >= r.args.size()) return std::nullopt; const auto& a = r.args[i];
    return a.kind == 1 ? (double)std::trunc(std::bit_cast<float>(a.bits)) : (double)(int32_t)a.bits;
}
int Arity(int op) { auto it = g_arity.find(op); return it == g_arity.end() || it->second < 0 ? -1 : it->second; }
void LoadArity() {
    const char* p = std::getenv("NOTSA_MDRIVER_ARGCOUNTS"); std::ifstream f(p ? p : "tools/standalone/scmscan/argcounts.tsv"); std::string l;
    while (std::getline(f, l)) { unsigned op; int n; if (std::sscanf(l.c_str(), "0x%x\t%d", &op, &n) == 2) g_arity[(int)op] = n; }
    if (g_arity.empty()) std::fprintf(stderr, "[mdriver] WARNING: no argcounts (the guard walk reads operands up to the next non-type byte)\n");
}

// ---- branch.ts: where the jump after a condition leads ----
bool InSpace(const uint8_t* p) { const uint8_t* b = CTheScripts::ScriptSpace.data(); return p >= b && p + 24 <= b + CTheScripts::ScriptSpace.size(); }
const uint8_t* SkipOperand(const uint8_t* p, bool& mfail, bool& isInt, int32_t& iv) {
    isInt = false;
    switch (*p) {
    case 1: isInt = true; iv = *(const int32_t*)(p + 1); return p + 5;
    case 6: return p + 5;
    case 2: case 3: case 5: case 10: case 11: case 16: case 17: return p + 3;
    case 4: return p + 2;
    case 7: case 8: case 12: case 13: case 18: case 19: return p + 7;
    case 9: mfail = !std::memcmp(p + 1, "M_FAIL\0\0", 8); return p + 9;
    case 15: return p + 17;
    case 14: return p + 2 + p[1];
    default: return nullptr;
    }
}
bool WalkFail(const CRunningScript* s, const uint8_t* p, int hops, bool sub, std::set<const uint8_t*>& seen) {
    for (int n = 0; n < 200 && InSpace(p); n++) {
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (!seen.insert(p).second) return false;
        if (op == 0x0001) return false;
        if (op == 0x0051) return !sub;
        const uint8_t* q = p + 2; bool mf = false, isI = false, firstInt = false; int32_t iv = 0, first = 0; const int want = Arity(op); int count = 0;
        for (;;) {
            if (want >= 0 && count >= want) break;
            if (!InSpace(q)) return false;
            const uint8_t* r = SkipOperand(q, mf, isI, iv);
            if (!r) break;
            if (mf) return true;
            if (count == 0) { firstInt = isI; first = iv; }
            q = r; count++;
        }
        if (op == 0x0002 || op == 0x0050) {
            if (!firstInt || hops >= 12) return false;
            const uint8_t* nx = Resolve(s, first);
            if (op == 0x0002) return WalkFail(s, nx, hops + 1, sub, seen);
            if (WalkFail(s, nx, hops + 1, true, seen)) return true;
        }
        p = q;
    }
    return false;
}
// CR22: the fail-FLAG guard. 11 BIG SMOKE: IS_CHAR_DEAD @65 -> true branch: print a fail text (label INT2_F2, not M_FAIL), @45 = 1, goto on; the M_FAIL print is reached later by the flag test @45 == 1 (ip 2d41).
// WalkFail alone calls that branch GoesOn and the driver kills the friend. A true branch that sets a variable to a constant is a Guard when a flag test of that very variable/value (any `var == number` condition seen in this run) leads to the M_FAIL print.
const uint8_t* CmdOps(const uint8_t* p, const uint8_t* ops[3], int& cnt) { // end of the command at p (nullptr: undecodable); the first three operand pointers
    const int op = *(const uint16_t*)p & 0x7FFF; const int want = Arity(op); const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0; cnt = 0;
    for (;;) {
        if (want >= 0 && cnt >= want) break;
        if (!InSpace(q)) return nullptr;
        const uint8_t* r = SkipOperand(q, mf, isI, iv); if (!r) break;
        if (cnt < 3) ops[cnt] = q;
        q = r; cnt++;
    }
    return q;
}
bool OperandVar(const uint8_t* q, uint32_t& ref) { if (*q == 2 || *q == 3) { ref = ((uint32_t)*q << 16) | *(const uint16_t*)(q + 1); return true; } return false; }
bool OperandInt(const uint8_t* q, int32_t& v) { switch (*q) { case 1: v = *(const int32_t*)(q + 1); return true; case 4: v = *(const int8_t*)(q + 1); return true; case 5: v = *(const int16_t*)(q + 1); return true; default: return false; } }
void FlagSets(const CRunningScript* s, const uint8_t* p, int hops, std::vector<std::pair<uint32_t, int32_t>>& out, std::set<const uint8_t*>& seen) { // the `var = constant` commands of the straight-line path from p (gotos followed, a condition / wait / return ends it)
    for (int n = 0; n < 40 && InSpace(p); n++) {
        const int op = *(const uint16_t*)p & 0x7FFF; if (!seen.insert(p).second) return;
        if (op == 0x0001 || op == 0x0051 || op == 0x00D6 || op == 0x004D || op == 0x0050) return;
        const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return;
        if ((op == 0x0004 || op == 0x0006) && cnt >= 2) { uint32_t ref; int32_t v; if (OperandVar(ops[0], ref) && OperandInt(ops[1], v)) out.push_back({ ref, v }); }
        if (op == 0x0002) { int32_t off; if (cnt < 1 || !OperandInt(ops[0], off) || hops >= 8) return; FlagSets(s, Resolve(s, off), hops + 1, out, seen); return; }
        p = e;
    }
}
bool FlagGuard(const CRunningScript* s, const Rec& r) {
    if (!r.fall || g_flagConds.empty() || std::getenv("NOTSA_MDRIVER_NOFLAGGUARD")) return false;
    std::vector<std::pair<uint32_t, int32_t>> sets; std::set<const uint8_t*> seen; FlagSets(s, r.fall, 0, sets, seen);
    for (auto& [ref, val] : sets) for (auto& [pp, f] : g_flagConds) {
        if (!f->vref[0] || f->vref[0] != ref || !f->hasBranch) continue;
        const auto v = ArgInt(*f, 1); if (!v || (int32_t)*v != val) continue;
        std::set<const uint8_t*> sn; if (WalkFail(s, f->fall, 0, false, sn)) return true;
    }
    return false;
}
enum class Verdict { Wait, GoesOn, Guard, Unknown };
Verdict VerdictOf(const CRunningScript* s, const Rec& r) {
    if (!r.hasBranch) return Verdict::Unknown;
    if (r.jumped && r.target < r.p) return Verdict::Wait;
    std::set<const uint8_t*> seen; if (WalkFail(s, r.fall, 0, false, seen)) return Verdict::Guard;
    return FlagGuard(s, r) ? Verdict::Guard : Verdict::GoesOn; // CR22
}

// CR22: static scan of the mission's code at its start: every singleton-group plain IS_CHAR_DEAD site and `var == number` flag test (the dynamic window only knows the conditions already run: 11 BIG SMOKE shoots Ryder/Smoke at f6621, long before the
// IS_CHAR_DEAD(@65) guard at ip 4cc6 ran). Linear decode of the code from the base ip (stops at the first undecodable command); the handle operand is re-read from the variable when a site is used.
void StaticScan(const CRunningScript* s) {
    if (std::getenv("NOTSA_MDRIVER_NOSTATICSCAN")) return;
    const uint8_t* p = s->m_BaseIP; RecP pend; bool single = false; int n = 0, nd = 0, nf = 0;
    while (InSpace(p) && n++ < 60000) {
        const uint16_t w = *(const uint16_t*)p; const int op = w & 0x7FFF; if (op > 0x0AFF) break;
        const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) break;
        if (Arity(op) < 0 && InSpace(e) && *e == 0) e++; // variadic: the END operand
        if (op == 0x004D) { if (pend) { pend->hasBranch = true; pend->jumped = false; pend->fall = p + 7; pend->target = p + 7; if ((pend->raw & 0x7FFF) == 0x0118) { g_deadConds[pend->p] = pend; nd++; } else { g_flagConds[pend->p] = pend; nf++; } } pend.reset(); single = false; p = e; continue; }
        if (op == 0x00D6) { int32_t v; single = cnt >= 1 && OperandInt(ops[0], v) && v == 0; pend.reset(); p = e; continue; }
        if (single && !(w & 0x8000) && (op == 0x0118 || op == 0x0038 || op == 0x0039)) {
            auto r = std::make_shared<Rec>(); r->p = p; r->raw = w; r->cond = -1; r->waitSeq = 0; r->args = DecodeArgs(s, p, op == 0x0118 ? 1 : 2);
            for (int k = 0; k < 2 && k < cnt; k++) { uint32_t ref; if (OperandVar(ops[k], ref)) r->vref[k] = ref; }
            pend = r;
        } else pend.reset();
        single = false; p = e;
    }
    notsa::standalone::Fixups::Log("[mdriver] static scan: base %x %d commands, %d IS_CHAR_DEAD sites, %d flag tests", (unsigned)(s->m_BaseIP - CTheScripts::ScriptSpace.data()), n, nd, nf);
}
// ---- actions.ts: guards, buddies ----
bool Likes(ePedType a, ePedType b) { return ((CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_LIKE, a) | CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_RESPECT, a)) & (1u << b)) != 0; }
bool Hates(ePedType a, ePedType b) { return ((CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_DISLIKE, a) | CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_HATE, a)) & (1u << b)) != 0; }
bool LikesSide(int t) { return Likes((ePedType)t, PED_TYPE_PLAYER1) || Likes(PED_TYPE_PLAYER1, (ePedType)t); }
bool HatesSide(int t) { return Hates((ePedType)t, PED_TYPE_PLAYER1) || Hates(PED_TYPE_PLAYER1, (ePedType)t); }
bool ProtectedPed(CPed* p) { auto* c = FindPlayerPed(); return (int)p->m_nPedType <= 3 || LikesSide((int)p->m_nPedType) || (c && c->GetPlayerGroup().GetMembership().IsMember(p)); }
bool ProtectedHandle(int h) { // actions.ts protectedHandle
    if (auto* p = CPools::GetPed(h)) return p == Cj() || ProtectedPed(p);
    auto* v = CPools::GetVehicle(h); if (!v) return false;
    std::vector<CPed*> occ; if (v->m_pDriver) occ.push_back(v->m_pDriver); for (auto* pp : v->m_apPassengers) if (pp) occ.push_back(pp);
    for (auto* o : occ) { if (o == Cj() || ProtectedPed(o) || (o->m_fHealth > 0.f && !HatesSide((int)o->m_nPedType))) return true; }
    return false;
}
bool IsPlayerGuard(const Rec& r) {
    const int op = r.raw & 0x7FFF; if (op != 0x0118 && op != 0x0119) return false;
    auto* ped = Cj(); const auto h = ArgInt(r, 0); if (!ped || !h) return false;
    return op == 0x0118 ? CPools::GetPed((int)*h) == ped : ped->m_pVehicle && CPools::GetVehicle((int)*h) == ped->m_pVehicle;
}
// CR22: the handles the script fails the mission for when they die (a plain IS_CHAR_DEAD site, singleton group, whose true branch reaches the fail text directly or through a fail flag): never killed by the driver, whatever the site that asks for the kill
const std::set<int>& Guarded() {
    static std::set<int> out; static int lastFr = -1; static std::map<const uint8_t*, std::pair<size_t, bool>> cache;
    const int fr = (int)CTimer::GetFrameCounter(); if (fr == lastFr) return out; lastFr = fr; out.clear();
    if (std::getenv("NOTSA_MDRIVER_NOGUARDPROTECT") || !g_thread) return out;
    for (auto& [site, r] : g_deadConds) {
        if ((r->raw & 0x8000) || (r->raw & 0x7FFF) != 0x0118) continue;
        auto& ce = cache[site]; const size_t gen = g_flagConds.size() + g_deadConds.size();
        if (ce.first != gen + 1) { ce = { gen + 1, VerdictOf(g_thread, *r) == Verdict::Guard }; if (std::getenv("NOTSA_MDRIVER_PLOG") && ce.second) notsa::standalone::Fixups::Log("[mdplog] guard site ip=%x flagconds=%zu", (unsigned)(site - g_thread->m_BaseIP), g_flagConds.size()); }
        if (!ce.second) continue; const auto da = DecodeArgs(g_thread, site, 1); if (!da.empty()) out.insert((int)da[0].bits);
    }
    return out;
}
bool KillRefusal(const CRunningScript* s, const Rec& r) { // true = the driver must not kill for this condition
    const int op = r.raw & 0x7FFF; if ((op != 0x0118 && op != 0x0119) || (r.raw & 0x8000)) return false;
    const auto h = ArgInt(r, 0); if (!h) return false;
    if (ProtectedHandle((int)*h) || Guarded().count((int)*h)) return true;
    const auto v = VerdictOf(s, r); return !(v == Verdict::Wait || v == Verdict::GoesOn);
}
bool NotInCarGuard(const CRunningScript* s, const Rec& r) {
    const int op = r.raw & 0x7FFF; if (!(r.raw & 0x8000) || (op != 0x00DB && op != 0x00DF)) return false;
    if (r.waitAfter && !r.jumped && !std::getenv("NOTSA_MDRIVER_NOWAITNOTGUARD")) return false; // CR22: the true branch of the NOT-in-car test is the WAIT itself (`WHILE NOT IS_CHAR_IN_CAR`, 13): the script waits for the car, it is no guard
    const auto v = VerdictOf(s, r); return v == Verdict::Guard || v == Verdict::GoesOn;
}
bool AnyGuard(const Rec& r) { return IsPlayerGuard(r) || KillRefusal(g_thread, r) || NotInCarGuard(g_thread, r); }

std::string KeyName(const std::string& k) {
    static const std::pair<const char*, const char*> m[] = { { "Enter", "return" }, { "Space", "space" }, { "Escape", "escape" }, { "ShiftLeft", "lshift" }, { "ControlLeft", "lctrl" }, { "ArrowUp", "up" }, { "ArrowDown", "down" }, { "ArrowLeft", "left" }, { "ArrowRight", "right" }, { "Tab", "tab" } };
    for (auto& [a, b] : m) if (k == a) return b;
    if (k.size() == 4 && !k.compare(0, 3, "Key")) return std::string(1, (char)std::tolower((unsigned char)k[3]));
    if (!k.empty() && std::isdigit((unsigned char)k[0])) { // pad button id (script IS_BUTTON_PRESSED numbering) -> default PC keyboard binding
        switch (std::atoi(k.c_str())) { case 14: return "lshift"; case 15: return "return"; case 16: return "space"; case 17: return "lctrl"; case 8: return "up"; case 9: return "down"; case 10: return "left"; case 11: return "right"; default: return ""; }
    }
    return "";
}
void LoadTable(const char* path) {
    std::ifstream f(path); std::string l; bool header = true;
    while (std::getline(f, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.empty() || l[0] == '#') continue;
        if (header) { header = false; continue; }
        std::vector<std::string> c; { size_t b = 0; for (;;) { const auto t = l.find('\t', b); if (t == std::string::npos) { c.push_back(l.substr(b)); break; } c.push_back(l.substr(b, t - b)); b = t + 1; } }
        if (c.size() < 6) continue;
        Row r; r.op = (uint16_t)std::strtoul(c[0].c_str(), nullptr, 16); r.name = c[1]; r.match = c[2] == "any" ? 0 : c[2] == "plain" ? 1 : 2; r.action = c[3];
        { std::stringstream ss(c[4]); std::string t; while (std::getline(ss, t, ',')) { const auto b = t.find_first_not_of(' '), e = t.find_last_not_of(' '); r.args.push_back(b == std::string::npos ? "" : t.substr(b, e - b + 1)); } }
        r.settle = std::atoi(c[5].c_str()); g_rows.push_back(std::move(r));
    }
    g_loaded = true;
}
void WriteResult() {
    const char* rp = std::getenv("NOTSA_MDRIVER_RESULT"); if (!rp) return;
    std::FILE* f = std::fopen(rp, "w"); if (!f) return;
    const bool failed = !g_pass && (g_fail || g_endedFail) && g_stopReason.empty();
    const char* res = g_pass ? "PASSED" : failed ? "FAILED" : "STOP";
    const std::string ftxt = !g_failText.empty() ? g_failText : !g_lastLabel.empty() ? g_lastLabel : "M_FAIL";
    const std::string reason = g_pass ? "-" : failed ? ftxt : (g_stopReason.empty() ? "ended" : g_stopReason);
    char op[8] = "-"; if (g_stopOp >= 0) std::snprintf(op, sizeof op, "%04X", g_stopOp);
    std::fprintf(f, "%s\t%d\t%s\t%x\t%s\t%s\n", res, g_pass ? g_passFrame : g_fail ? g_failFrame : g_endFrame >= 0 ? g_endFrame : GFrame(), g_name.c_str(), g_stopIp, op, reason.c_str());
    std::fclose(f);
}
void Stop(const std::string& reason, int op, uint32_t ip) { if (g_stopReason.empty()) { g_stopReason = reason; g_stopOp = op; g_stopIp = ip; } g_quitIn = 0; std::fprintf(stderr, "[mdriver] stop: %s (op %04X ip %x frame %d)\n", reason.c_str(), op < 0 ? 0 : op, ip, GFrame()); }
void QueueActions(const std::vector<std::string>& acts, int settle) { g_queue = acts; if (settle > 0) g_queue.push_back("wait " + std::to_string(settle)); g_qpos = 0; }

// ---- driverTable.ts decide() ----
struct RArg { bool str = false; double n = 0; std::string s; };
std::string RKey(const std::optional<std::vector<RArg>>& a) { if (!a) return "null"; std::string k; for (auto& x : *a) { k += x.str ? "s:" + x.s : "n:" + std::to_string(x.n); k += ','; } return k; }
bool IsNum(const std::string& s) { size_t i = 0; if (i < s.size() && s[i] == '-') i++; const size_t d0 = i; while (i < s.size() && std::isdigit((unsigned char)s[i])) i++; if (i == d0) return false; if (i == s.size()) return true; if (s[i] != '.') return false; i++; const size_t d1 = i; while (i < s.size() && std::isdigit((unsigned char)s[i])) i++; return i > d1 && i == s.size(); }
std::optional<std::vector<RArg>> ResolveArgs(const Row& row, const Rec& rec) {
    const bool fl = row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint";
    std::vector<RArg> out;
    for (auto& a : row.args) {
        RArg r;
        if (a.size() > 1 && a[0] == '$' && std::all_of(a.begin() + 1, a.end(), [](char c) { return std::isdigit((unsigned char)c); })) {
            const auto v = fl ? ArgFloat(rec, (size_t)std::atoi(a.c_str() + 1)) : ArgInt(rec, (size_t)std::atoi(a.c_str() + 1));
            if (!v || !std::isfinite(*v)) return std::nullopt;
            r.n = *v;
        } else if (IsNum(a)) r.n = std::atof(a.c_str());
        else { r.str = true; r.s = a; }
        out.push_back(r);
    }
    return out;
}
bool Matches(const Row& w, const Rec& r) { return w.op == (r.raw & 0x7FFF) && (w.match == 0 || (w.match == 2) == ((r.raw & 0x8000) != 0)); }
struct Dec { bool act = false; const Row* row = nullptr; int rowIdx = -1; RecP cond; std::vector<RArg> args; int op = -1; std::string reason; };
CEntity* EntityOfHandle(int h, int op);
Dec Decide(const std::vector<RecP>& loop, int attempt, bool wide = false) {
    Dec d;
    // CR19: the wait group. The last condition group of the pass fell straight into a WAIT and is an OR (00D6 21..27: `WHILE OR(NOT a, NOT b, ...)`): the thread waits until EVERY member is false. The NOT-form members that are
    // false are already satisfied (a is true), so they are no candidates (NOT IS_CHAR_SITTING_IN_CAR false = CJ sits: the Enter key got him OUT of the car, 12 13 15 16 58); the NOT-form members that are TRUE
    // are the blockers (a is false): the plain row of their opcode (entercar / vehwarp / pedwarp) answers them, after the false conditions found no row.
    std::set<const uint8_t*> satisfied; std::vector<RecP> blockers;
    if (!loop.empty() && !std::getenv("NOTSA_MDRIVER_NOORGROUP")) {
        const auto& lr = loop.back();
        if (lr->hasBranch && !lr->jumped && lr->waitAfter) {
            std::vector<RecP> mem; for (auto& r : loop) if (r->fall == lr->fall && r->waitSeq == lr->waitSeq) mem.push_back(r);
            const uint8_t* f = mem.front()->p;
            if (f[-4] == 0xD6 && f[-3] == 0x00 && f[-2] == 0x04 && ((f[-1] >= 21 && f[-1] <= 27) || (f[-1] <= 7 && !std::getenv("NOTSA_MDRIVER_NOANDBLOCK")))) // CR22: AND groups (and singletons: `WHILE NOT IS_CHAR_IN_CAR`, 13) that fell into the WAIT too: their TRUE NOT-form members are the blockers
                for (auto& m : mem) if (m->raw & 0x8000) { if (m->cond == 0) satisfied.insert(m->p); else if (!AnyGuard(*m)) blockers.push_back(m); }
        }
    }
    if (!std::getenv("NOTSA_MDRIVER_NOESCORT") && blockers.size() > 1) { // CR24 (11 BIG SMOKE: `WHILE OR(NOT CJ at house, NOT CJ near Ryder)`): in the wait group the TRUE `NOT CJ near char X` blockers go before the `NOT CJ at place` ones (the table rows are in file order, the place row would always win): the place blockers are dropped for this round
        const auto isNearB = [](const RecP& c) { const int op = c->raw & 0x7FFF; if (!((op >= 0x00F2 && op <= 0x00F4) || (op >= 0x0104 && op <= 0x0106))) return false; const auto h = ArgInt(*c, 0); return h && EntityOfHandle((int)*h, op) == (CEntity*)Cj(); };
        const auto isPlaceB = [](const RecP& c) { const int op = c->raw & 0x7FFF; return (op >= 0x00EC && op <= 0x00F1) || (op >= 0x00FE && op <= 0x0103); };
        if (std::any_of(blockers.begin(), blockers.end(), isNearB) && std::any_of(blockers.begin(), blockers.end(), isPlaceB)) blockers.erase(std::remove_if(blockers.begin(), blockers.end(), isPlaceB), blockers.end());
    }
    if (!std::getenv("NOTSA_MDRIVER_NOORGROUP")) { // CR19: ANY OR group of the pass with a TRUE member is true: its false members (13 TAGGING UP TURF: OR(at A, at B) with CJ at B) are satisfied too, no candidates
        std::map<std::pair<const uint8_t*, int>, std::vector<RecP>> groups; for (auto& r : loop) if (r->hasBranch) groups[{ r->fall, r->waitSeq }].push_back(r);
        for (auto& [k, mem] : groups) {
            const uint8_t* f = mem.front()->p; if (!(f[-4] == 0xD6 && f[-3] == 0x00 && f[-2] == 0x04 && f[-1] >= 21 && f[-1] <= 27)) continue;
            if (std::any_of(mem.begin(), mem.end(), [](auto& m) { return m->cond == 1; })) for (auto& m : mem) if (m->cond == 0) satisfied.insert(m->p);
        }
    }
    std::vector<RecP> falses;
    for (size_t k = loop.size(); k-- > 0;) { const auto& r = loop[k]; if (r->cond == 0 && !AnyGuard(*r) && !satisfied.count(r->p)) falses.push_back(r); }
    if (falses.empty() && blockers.empty()) { d.reason = "no condition evaluated false in the loop (the guards left out)"; return d; }
    // CR22: built-in verbs for opcodes the TS table has no row for (proposed rows: see C:/gta-scratch/cr22/FINDINGS.txt FOR TS). They come before every table row (a wait row of another false condition of the pass must not shadow them).
    static const Row kDmg{ 0x0366, 1, "HAS_OBJECT_BEEN_DAMAGED", "damageobj", { "$0" }, 30 };
    static const Row kTag{ 0x0038, 0, "int compare of the GET_PERCENTAGE_TAGGED_IN_AREA result", "tagarea", {}, 60 };
    if (!std::getenv("NOTSA_MDRIVER_NOBUILTIN")) {
        const auto tagCond = [&](const RecP& c) { // an integer compare of the result variable of a GET_PERCENTAGE_TAGGED_IN_AREA: the area it measured (a false plain compare, or the TRUE NOT-form compare of the wait group: `WHILE NOT $27 == 100`)
            const int op = c->raw & 0x7FFF; const bool intCmp = (op >= 0x0018 && op <= 0x0041) || (op >= 0x04A3 && op <= 0x04B7) || (op >= 0x07D6 && op <= 0x07D9); if (!intCmp) return false;
            for (int k = 0; k < 2; k++) if (c->vref[k]) if (auto it = g_tagAreas.find(c->vref[k]); it != g_tagAreas.end()) { for (int j = 0; j < 4; j++) g_tagRect[j] = it->second[j]; return true; }
            return false;
        };
        for (auto& c : falses) {
            const int op = c->raw & 0x7FFF; if (c->raw & 0x8000) continue;
            if (op == 0x0366) { auto a2 = ResolveArgs(kDmg, *c); if (a2 && wide && (a2->empty() || !CPools::GetObject((int)(*a2)[0].n))) continue; /* CR23: a record of an older pass: the object may be gone */ if (a2) { d.act = true; d.row = &kDmg; d.rowIdx = -2; d.cond = c; d.args = *a2; d.op = op; return d; } }
            if (tagCond(c)) { d.act = true; d.row = &kTag; d.rowIdx = -3; d.cond = c; d.op = op; return d; }
        }
        static const Row kEnter{ 0x0039, 1, "int compare of the GET_AREA_VISIBLE result", "enterarea", {}, 200 };
        for (auto& c : falses) { // CR23 (16 NINES AND AK'S: `WHILE @86 != 15` after the Binco blip): a false plain `var == N` of a GET_AREA_VISIBLE result variable: walk in through the entrance of interior N (an entry exit whose interior side is in area N)
            const int op = c->raw & 0x7FFF; if ((c->raw & 0x8000) || (op != 0x0038 && op != 0x0039) || std::getenv("NOTSA_MDRIVER_NOENTERAREA")) continue;
            if (!(c->vref[0] && g_areaVars.count(c->vref[0]))) continue;
            const auto n = ArgInt(*c, 1); if (!n || *n < 0) continue;
            g_enterArea = (int)*n; d.act = true; d.row = &kEnter; d.rowIdx = -4; d.cond = c; d.op = op; return d;
        }
        static const Row kSph{ 0x0119, 2, "NOT IS_CAR_DEAD of a GET_RANDOM_CAR_IN_SPHERE result", "tosphere", {}, 90 };
        for (auto& c : falses) { // CR24 (10 BEEFY BARON: `GET_CAR_COORDINATES @34 -> GET_RANDOM_CAR_IN_SPHERE_NO_SAVE(coords, 30 m, 459) -> @36; WHILE NOT IS_CAR_DEAD(@36)`): the sphere is centred on a vehicle (the remote-controlled RC plane) that stands still the whole run: bring THAT vehicle next to the nearest car of the wanted model
            if (c->raw != 0x8119 || !c->vref[0] || std::getenv("NOTSA_MDRIVER_NOSPHERE")) continue;
            const auto it = g_sphSearch.find(c->vref[0]); if (it == g_sphSearch.end()) continue;
            g_sphCur = it->second; d.act = true; d.row = &kSph; d.rowIdx = -5; d.cond = c; d.op = 0x0119; return d;
        }
        for (auto& c : blockers) if (tagCond(c)) { d.act = true; d.row = &kTag; d.rowIdx = -3; d.cond = c; d.op = c->raw & 0x7FFF; return d; }
    }
    if (!std::getenv("NOTSA_MDRIVER_NOESCORT")) { // CR24 (11 BIG SMOKE): escort ordering. A false `CJ near char X` locate (00F2..00F4, 0104..0106 with CJ as the first operand) of the same pass as a false `CJ at place` locate is answered FIRST: the buddy of the escort stage is frozen by the script's own helper
        // (FREEZE_CHAR/CAR_POSITION when he is more than 130 m from CJ), so a lone CJ warp to the place leaves the escorted entity behind for ever. CJ joins the buddy (it unfreezes and rides on); the place locate is answered at the next stall.
        const auto isNear = [](const RecP& c) { const int op = c->raw & 0x7FFF; if ((c->raw & 0x8000) || c->cond != 0 || !((op >= 0x00F2 && op <= 0x00F4) || (op >= 0x0104 && op <= 0x0106))) return false; const auto h = ArgInt(*c, 0); return h && EntityOfHandle((int)*h, op) == (CEntity*)Cj(); };
        const auto isPlace = [](const RecP& c) { const int op = c->raw & 0x7FFF; return !(c->raw & 0x8000) && ((op >= 0x00EC && op <= 0x00F1) || (op >= 0x00FE && op <= 0x0103)); };
        if (std::any_of(falses.begin(), falses.end(), isPlace))
            for (auto& c : falses) {
                if (!isNear(c)) continue;
                for (size_t i = 0; i < g_rows.size(); i++) {
                    const auto& row = g_rows[i]; if (row.action == "wait" || !Matches(row, *c)) continue;
                    auto a = ResolveArgs(row, *c); if (!a) continue;
                    d.act = true; d.row = &row; d.rowIdx = (int)i; d.cond = c; d.args = *a; d.op = c->raw & 0x7FFF; return d;
                }
            }
    }
    for (int pass = 0; pass < 2; pass++) {
    if (pass == 1 && blockers.empty()) break;
    for (size_t i = 0; i < g_rows.size(); i++) {
        const auto& row = g_rows[i];
        if (pass == 0 && !blockers.empty() && row.action == "wait") continue; // CR19: a wait row does not shadow the blockers of the wait group
        for (auto& c : falses) {
            if (!Matches(row, *c)) continue;
            struct Opt { RecP c; std::optional<std::vector<RArg>> args; }; std::vector<Opt> opts; std::set<std::string> seen;
            for (auto& q : falses) { if (!Matches(row, *q)) continue; auto a = ResolveArgs(row, *q); if (seen.insert(RKey(a)).second) opts.push_back({ q, a }); }
            auto& pick = opts[(size_t)attempt % opts.size()];
            if (row.action == "key" && row.op == 0x00E1 && attempt < 3 && !std::getenv("NOTSA_MDRIVER_NOKEYDEFER")) { // CR22 (10 BEEFY BARON): a button poll whose failing jump leads on to ANOTHER false condition of the pass that only a wait row answers (a world event: here `NOT IS_CAR_DEAD(@36)`, the Topfun van the script searches for after the button test) is not what the script waits for: it waits for that event first. The button is pressed only after 3 stalls.
                size_t at = 0; while (at < loop.size() && loop[at] != pick.c) at++;
                bool eventAfter = false;
                for (size_t k = at + 1; k < loop.size() && !eventAfter; k++) { const auto& r = loop[k]; if (r->cond != 0 || AnyGuard(*r) || satisfied.count(r->p)) continue; for (auto& w : g_rows) if (w.action == "wait" && Matches(w, *r)) { eventAfter = true; break; } }
                if (eventAfter) break; // the next row (the wait rows) answers
            }
            if (!pick.args && !row.args.empty()) { d.op = pick.c->raw & 0x7FFF; d.reason = "unhandled " + row.name + ": argument of the row cannot be read"; return d; }
            d.act = true; d.row = &row; d.rowIdx = (int)i; d.cond = pick.c; d.args = pick.args.value_or(std::vector<RArg>{}); d.op = pick.c->raw & 0x7FFF; return d;
        }
    }
    if (pass == 0 && !blockers.empty()) for (size_t i = 0; i < g_rows.size(); i++) { // CR19: the true NOT-form members of the wait OR group (last member first)
        const auto& row = g_rows[i]; if (row.match == 2 || !(row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint" || row.action == "entercar" || (row.action == "warpto_handle" && !std::getenv("NOTSA_MDRIVER_NOESCORT")))) continue; // CR24: warpto_handle too (the TRUE `NOT CJ near char X` blocker of an escort wait loop)
        for (size_t k = blockers.size(); k-- > 0;) {
            const auto& c = blockers[k]; if (row.op != (c->raw & 0x7FFF)) continue;
            auto a = ResolveArgs(row, *c); if (!a) continue;
            d.act = true; d.row = &row; d.rowIdx = (int)i; d.cond = c; d.args = *a; d.op = c->raw & 0x7FFF; return d;
        }
    }
    } // pass: 0 = (wait rows skipped, blockers), 1 = the wait rows again
    // a TRUE NOT-form locate in a wait loop (92 CASIN10): the plain row of the same opcode when it is a warp
    for (size_t i = 0; i < g_rows.size(); i++) {
        const auto& row = g_rows[i]; if (row.match == 2 || !(row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint")) continue;
        for (size_t k = loop.size(); k-- > 0;) {
            const auto& c = loop[k]; if (!(c->raw & 0x8000) || c->cond != 1 || AnyGuard(*c) || row.op != (c->raw & 0x7FFF)) continue;
            auto a = ResolveArgs(row, *c); if (!a) continue;
            d.act = true; d.row = &row; d.rowIdx = (int)i; d.cond = c; d.args = *a; d.op = c->raw & 0x7FFF; return d;
        }
    }
    if (falses.empty()) { d.reason = "no condition evaluated false in the loop (the guards left out)"; return d; }
    const auto& last = *falses[0]; d.op = last.raw & 0x7FFF;
    char b[16]; std::snprintf(b, sizeof b, "%04X", d.op); std::string nm; for (auto& w : g_rows) if (w.op == d.op) { nm = w.name; break; }
    d.reason = std::string("unhandled ") + b + ((last.raw & 0x8000) ? " (NOT form)" : "") + (nm.empty() ? "" : " " + nm); return d;
}

// ---- actions.ts: entities of a handle, execute() ----
CEntity* EntityOfHandle(int h, int op) { // a locate says which pool its operand is from
    if ((op >= 0x01AD && op <= 0x01B0) || (op >= 0x0202 && op <= 0x0207)) return CPools::GetVehicle(h);
    if (op >= 0x0471 && op <= 0x0476) return CPools::GetObject(h);
    if ((op >= 0x00EC && op <= 0x00F4) || (op >= 0x00FE && op <= 0x0106)) return CPools::GetPed(h);
    if (auto* p = CPools::GetPed(h)) return p;
    if (auto* v = CPools::GetVehicle(h)) return v;
    return CPools::GetObject(h);
}
bool IsLocateOp(int op) { return (op >= 0x00EC && op <= 0x00F1) || (op >= 0x00FE && op <= 0x0103) || (op >= 0x01AD && op <= 0x01B0); }
std::string LocatedOther(const Rec& r) { // "ped:<handle>" / "veh:<handle>" of the entity a locate is about when it is not CJ (nor his car): that one is moved
    const int op = r.raw & 0x7FFF; if (!IsLocateOp(op)) return "";
    const auto h = ArgInt(r, 0); if (!h) return "";
    auto* e = EntityOfHandle((int)*h, op); auto* me = Cj();
    if (!e || e == me || (me && me->bInVehicle && e == (CEntity*)me->m_pVehicle)) return "";
    return (op >= 0x01AD ? "1:" : "0:") + std::to_string((int)*h);
}
// MS1: actions.ts pickDoorSpot / tryEnterCar. CJ is stood 2 m from a door of the car (left, right, behind: the first whose line to the door is free), 40 frames pass, the Enter key, up to 600 frames, then the script's own WARP_CHAR_INTO_CAR (mdwarpin)
CVehicle* NearestFreeCar(CPed* c, float best) {
    CVehicle* car = nullptr;
    for (auto& vv : GetVehiclePool()->GetAllValid()) {
        if (vv.m_pDriver || vv.m_nDoorLock == CARLOCK_LOCKED || vv.m_nDoorLock == CARLOCK_LOCKED_PLAYER_INSIDE || vv.m_nDoorLock == CARLOCK_LOCKOUT_PLAYER_ONLY) continue;
        const float dd = (float)(vv.GetPosition() - c->GetPosition()).Magnitude();
        if (dd < best) { best = dd; car = &vv; }
    }
    return car;
}
std::vector<std::string> EnterCarActions(CVehicle* car) {
    const auto door = CCarEnterExit::GetPositionToOpenCarDoor(car, CAR_DOOR_LF); const CVector right = car->GetRight(), front = car->GetForward();
    const float cx[3] = { door.x - right.x * 2.f, door.x + right.x * 2.f, door.x - front.x * 2.f }, cy[3] = { door.y - right.y * 2.f, door.y + right.y * 2.f, door.y - front.y * 2.f };
    const auto free = [&](float x, float y) {
        for (int k = 0; k <= 4; k++) { const float t = k / 4.f; if (CWorld::TestSphereAgainstWorld(CVector{ x + (door.x - x) * t, y + (door.y - y) * t, car->GetPosition().z + 0.2f }, 0.35f, nullptr, true, false, false, false, false, false)) return false; }
        return true;
    };
    int pick = 0; for (int k = 0; k < 3; k++) if (free(cx[k], cy[k])) { pick = k; break; }
    float dx = door.x - cx[pick], dy = door.y - cy[pick]; const float len = std::hypot(dx, dy) ? (float)std::hypot(dx, dy) : 1.f; dx /= len; dy /= len;
    return { "emit mdped:" + F(cx[pick]) + ":" + F(cy[pick]) + ":" + F(car->GetPosition().z + 0.2f) + ":" + F(std::atan2(-dx, dy)) + ":0", "wait 40", "key down return", "wait 1", "key up return", "waituntil invehicle == 1 600", "emit mdwarpin:" + std::to_string(GetVehiclePool()->GetRef(car)) };
}
void DispatchRow(const Dec& d) { // run one table action (Stop() on a failclass / bad verb)
    const Row& row = *d.row; const int cop = d.op;
    auto F_ = [&](size_t i) -> float { return i < d.args.size() ? (float)d.args[i].n : 0.f; };
    auto I_ = [&](size_t i) -> int { return i < d.args.size() ? (int)d.args[i].n : 0; };
    auto* c = Cj();
    const std::string& v = row.action;
    const auto warp = [&](float x, float y, float z, bool vehOnly) -> std::string { // actions.ts warp: CJ's vehicle when he drives one, else CJ (z + 1.1); z <= -100 = the ground
        const bool inCar = c && c->bInVehicle;
        if (vehOnly && !inCar) return "";
        return std::string(inCar ? "emit mdwveh:" : "emit mdwped:") + F(x) + ":" + F(y) + ":" + F(z) + (inCar || !Ad3() ? "" : AreaSuffix(x, y, z));
    };
    const auto one = [&](const std::string& a) { QueueActions(a.empty() ? std::vector<std::string>{} : std::vector<std::string>{ a }, row.settle); };
    if (v == "wait") { QueueActions({}, row.settle); return; }
    if (v == "pedwarp" || v == "vehwarp") {
        if (d.cond) { const auto o = LocatedOther(*d.cond); if (!o.empty()) { QueueActions({ "emit mdwother:" + o + ":" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) }, row.settle); return; } } // LOCATE_CAR_3D(car 2049, ...) is satisfied by moving THAT car
        if (v == "pedwarp" && c && c->bInVehicle) { // CR10: actions.ts pedwarp on an ON_FOOT locate while CJ is in a vehicle: get out first (Enter, up to 300 frames; getOnFoot), then warp him (mission 77: 8x mdwveh on LOCATE_CHAR_ON_FOOT_3D)
            static const std::set<int> onFoot = { 0xE4, 0xE7, 0xEA, 0xED, 0xF0, 0xF3, 0xF6, 0xF9, 0xFC, 0xFF, 0x102, 0x105, 0x197, 0x19A, 0x19C, 0x19F, 0x1A1, 0x1A4, 0x1A6, 0x1A9, 0x1FD, 0x200, 0x203, 0x206, 0x27C, 0x27F, 0x282, 0x285, 0x288 };
            if (onFoot.count(cop & 0x7FFF)) { QueueActions({ "key down return", "wait 1", "key up return", "waituntil invehicle == 0 300", "emit mdwveh:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) }, row.settle); return; } // CR11: after a 300-frame timeout CJ may still be in the vehicle: TS warp() then moves the CAR; mdwveh does that (ped warp when he is out)
        }
        one(warp(F_(0), F_(1), F_(2), v == "vehwarp")); return;
    }
    if (v == "midpoint") {
        float x, y, z = -100.f;
        if (d.args.size() >= 6) { x = (F_(0) + F_(3)) / 2; y = (F_(1) + F_(4)) / 2; z = (F_(2) + F_(5)) / 2; } else { x = (F_(0) + F_(2)) / 2; y = (F_(1) + F_(3)) / 2; }
        one(warp(x, y, z, false)); return;
    }
    if (v == "entercar" || v == "nearest_car") {
        CVehicle* car = nullptr;
        if (v == "entercar") { car = CPools::GetVehicle(I_(0)); if (!car) { Stop("driver action failed: entercar " + std::to_string(I_(0)) + ": no such car", cop, 0); return; } }
        else if (c) { car = NearestFreeCar(c, 50.f); if (!car) { Stop("by nature: no_car", cop, 0); return; } }
        if (!car || !c) { QueueActions({}, row.settle); return; }
        if (v == "entercar" && d.cond && (cop == 0x00DB || cop == 0x0448)) { // MS1 (actions.ts entercar): IS_CHAR_IN_CAR(buddy, car) wants THAT char in the car: set in as a passenger like WARP_CHAR_INTO_CAR_AS_PASSENGER
            const auto h = ArgInt(*d.cond, 0);
            if (h) if (auto* oth = CPools::GetPed((int)*h)) if (oth != c) {
                // CR19 (58 PHOTO): a member of CJ's group (TASK_COMPLEX_BE_IN_GROUP) follows its leader: set in as a passenger while CJ stands outside it climbs out again. CJ gets into the car first (Enter key / warp), then the buddy is set in
                const bool follower = oth->GetIntelligence()->m_TaskMgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY) && oth->GetIntelligence()->m_TaskMgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY)->GetTaskType() == TASK_COMPLEX_BE_IN_GROUP;
                // (a follower set in while CJ is outside is walked out again by its FollowLeader task 0x696820/0x665600, exe-checked; with CJ at the wheel the group enters him by itself: only a repeat sets him in)
                std::vector<std::string> acts; const bool leaderFirst = follower && !(c->bInVehicle && c->m_pVehicle == car) && !std::getenv("NOTSA_MDRIVER_NOLEADERFIRST");
                if (leaderFirst) acts = EnterCarActions(car); else acts.push_back("emit mdwarppass:" + std::to_string((int)*h) + ":" + std::to_string(I_(0)));
                QueueActions(acts, row.settle); return; }
        }
        auto acts = EnterCarActions(car);
        QueueActions(acts, row.settle);
        return;
    }
    if (v == "kill") {
        const int h = I_(0);
        if (ProtectedHandle(h)) { Stop("driver action failed: driver would kill a buddy (kill " + std::to_string(h) + ")", cop, 0); return; }
        std::string cmd;
        if (auto* p = CPools::GetPed(h)) { if (p != c) cmd = "emit mdkillped:" + std::to_string(h); } else if (CPools::GetVehicle(h)) cmd = "emit mdkillcar:" + std::to_string(h);
        if (cmd.empty()) { QueueActions({}, 0); return; } // kill: no such char or car (TS returns without settling)
        QueueActions({ cmd }, row.settle); g_killFrame = GFrame(); g_killSettle = row.settle; return;
    }
    if (v == "key") {
        std::string kk = d.args.empty() ? "" : d.args[0].str ? d.args[0].s : std::to_string((int)d.args[0].n); const int n = d.args.size() > 1 && !d.args[1].str ? (int)d.args[1].n : 8;
        const std::string nm = KeyName(kk); if (nm.empty()) { Stop("unmapped-key " + kk, cop, 0); return; }
        if (!kk.empty() && std::isdigit((unsigned char)kk[0])) { QueueActions({ "emit mdpad:" + kk + ":" + std::to_string(n), "key down " + nm, "wait " + std::to_string(n), "key up " + nm }, row.settle); return; } // CR4: numeric pad button: the pad state itself, as TS (PadInput.joy[field] = 255)
        QueueActions({ "key down " + nm, "wait " + std::to_string(n), "key up " + nm }, row.settle); return;
    }
    if (v == "skipcut") { QueueActions(CCutsceneMgr::IsCutsceneProcessing() ? std::vector<std::string>{ "emit mdskipcut:0" } : std::vector<std::string>{}, row.settle); return; }
    if (v == "warpto_handle") {
        const int h = I_(0); CEntity* e = EntityOfHandle(h, cop);
        if (!e) { QueueActions({}, 0); return; }
        const auto pos = e->GetPosition();
        if (auto* o = CPools::GetObject(h); o && o->objectFlags.bIsLiftable && c && !c->GetEntityThatThisPedIsHolding()) { // CR4: default on (was AD3-only): stand 1.5 m before a liftable object and press the pick-up key
            QueueActions({ "standobj " + std::to_string(h) + " 1.5", "wait 10", "key down return", "wait 1", "key up return", "wait 90" }, row.settle); return;
        }
        if (c && c->bInVehicle && c->m_pVehicle && e->GetIsTypeVehicle() && e != c->m_pVehicle && !std::getenv("NOTSA_MDRIVER_NOCLEARSPOT")) { // CR21: the target is a (possibly moving) car: put CJ's car in a clear spot beside / behind it, never onto it (89: the ambulances cruise; the +0.5 m warp overlapped one -> RAMMED_BY_CAR every frame -> CJ dead). General rule for the TS driver: candidates behind, left, right, ahead of the target at (half extent of both cars + 1.5 m), the first with no building/vehicle/object within the car's radius; none free -> the old +0.5 m
            auto* tcm = e->GetColModel(); auto* ocm = c->m_pVehicle->GetColModel();
            if (tcm && ocm) {
                const auto tb = tcm->GetBoundingBox(), ob = ocm->GetBoundingBox();
                const float tl = (tb.m_vecMax.y - tb.m_vecMin.y) * 0.5f, ol = (ob.m_vecMax.y - ob.m_vecMin.y) * 0.5f, tw = (tb.m_vecMax.x - tb.m_vecMin.x) * 0.5f, ow = (ob.m_vecMax.x - ob.m_vecMin.x) * 0.5f;
                const float dl = tl + ol + 1.5f, dw = tw + ow + 1.5f, rad = std::max(ol, ow) + 0.5f;
                const CVector fw = e->GetForward(), rt = e->GetRight();
                const CVector cand[4] = { pos - fw * dl, pos - rt * dw, pos + rt * dw, pos + fw * dl };
                for (const auto& q : cand) {
                    if (CWorld::TestSphereAgainstWorld(CVector{ q.x, q.y, q.z + 0.6f }, rad, c->m_pVehicle, false, true, false, true, false, false) || CWorld::TestSphereAgainstWorld(CVector{ q.x, q.y, q.z + 1.4f }, 1.0f, c->m_pVehicle, true, false, false, false, false, false)) continue; // cars/objects within the car radius; buildings (walls, poles) above the road surface
                    notsa::standalone::Fixups::Log("[mdriver] warpto_handle: clear spot beside car %d (%.2f,%.2f) -> (%.2f,%.2f)", h, pos.x, pos.y, q.x, q.y);
                    one(warp(q.x, q.y, q.z, false)); return;
                }
            }
        }
        notsa::standalone::Fixups::Log("[mdriver] warpto_handle: no clear spot, +0.5 m");
        one(warp(pos.x + 0.5f, pos.y, pos.z, false)); return;
    }
    if (v == "damageobj") { QueueActions({ "emit mddmgobj:" + std::to_string(I_(0)) }, row.settle); return; } // CR22: HAS_OBJECT_BEEN_DAMAGED(obj): CJ damages the object (16 NINES AND AK'S shooting range)
    if (v == "tosphere") { // CR24: the vehicle at the centre of a GET_RANDOM_CAR_IN_SPHERE search is moved next to the nearest car the game's own search would accept (appearance 1/2, not a law enforcer, wanted model, CanBeDeleted: the mission's decoy is refused), 8 m above it
        auto* cv = CPools::GetVehicle(g_sphCur.veh); CVehicle* best = nullptr; float bd = 1e9f;
        if (cv) for (auto& vv : GetVehiclePool()->GetAllValid()) {
            if (&vv == cv || (vv.GetVehicleAppearance() != 1 && vv.GetVehicleAppearance() != 2) || vv.vehicleFlags.bIsLawEnforcer || (g_sphCur.model >= 0 && (int)(int16_t)vv.m_nModelIndex != g_sphCur.model) || !vv.CanBeDeleted()) continue;
            const float dd = (vv.GetPosition() - cv->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; best = &vv; }
        }
        if (!best) { QueueActions({}, row.settle); return; }
        const auto bp = best->GetPosition();
        QueueActions({ "emit mdwother:1:" + std::to_string(g_sphCur.veh) + ":" + F(bp.x) + ":" + F(bp.y) + ":" + F(bp.z + 8.f) }, row.settle); return;
    }
    if (v == "tagarea") { QueueActions({ "emit mdtag:" + F(g_tagRect[0]) + ":" + F(g_tagRect[1]) + ":" + F(g_tagRect[2]) + ":" + F(g_tagRect[3]) }, row.settle); return; } // CR22: the script waits for the tagged percentage of an area: spray every tag of it (13 TAGGING UP TURF)
    if (v == "enterarea") { // CR23: stand on the entrance marker of the nearest exterior entry exit (area 0) whose linked interior side is in area g_enterArea (CEntryExitManager does the rest: fade, area switch)
        auto* me = Cj(); CEntryExit* best = nullptr; float bd = 1e30f;
        if (me) for (auto& ex : CEntryExitManager::GetPool()->GetAllValid()) { if (g_enterArea == 0 ? ((int)ex.m_nArea != (int)CGame::currArea || ex.m_nArea == 0 || !ex.m_pLink || (int)ex.m_pLink->m_nArea != 0) /* leave: the exit marker of the interior CJ is in */ : ((int)ex.m_nArea != 0 || !ex.m_pLink || (int)ex.m_pLink->m_nArea != g_enterArea)) continue; const auto c = ex.GetPosition(); const float dd = (c - me->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; best = &ex; } }
        if (!best) { QueueActions({}, 0); return; }
        const auto c = best->GetPosition(); float dx = c.x - best->m_vecExitPos.x, dy = c.y - best->m_vecExitPos.y; const float len = std::max(0.01f, (float)std::hypot(dx, dy));
        notsa::standalone::Fixups::Log("[mdriver] enterarea %d: entry exit %.8s at %.1f,%.1f,%.1f (%.0f m from CJ)", g_enterArea, best->m_szName, c.x, c.y, c.z, bd);
        QueueActions({ "pedwarp " + F(c.x) + " " + F(c.y) + " " + F(c.z) }, row.settle); return;
    }
    if (v == "failclass") { Stop("by nature: " + (d.args.empty() ? std::string("failclass") : d.args[0].s), cop, 0); return; }
    Stop("badverb " + v, cop, 0);
}
CPed* KillTarget(CPed* ped) { // actions.ts killTarget: a kill-ped-on-foot task anywhere in the task tree whose target is a ped (the on-foot / melee / armed classes)
    auto& tm = ped->GetIntelligence()->m_TaskMgr;
    std::vector<CTask*> roots(tm.m_aPrimaryTasks.begin(), tm.m_aPrimaryTasks.end()); roots.insert(roots.end(), tm.m_aSecondaryTasks.begin(), tm.m_aSecondaryTasks.end());
    for (auto* root : roots) {
        for (CTask* t = root; t; t = t->GetSubTask()) {
            CPed* tg = nullptr;
            switch (t->GetTaskType()) {
            case TASK_COMPLEX_KILL_PED_ON_FOOT: tg = static_cast<CTaskComplexKillPedOnFoot*>(t)->m_target; break;
            case TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE: tg = static_cast<CTaskComplexKillPedOnFootMelee*>(t)->m_target; break;
            case TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED: tg = static_cast<CTaskComplexKillPedOnFootArmed*>(t)->m_target; break;
            case TASK_COMPLEX_KILL_PED_ON_FOOT_STEALTH: tg = static_cast<CTaskComplexKillPedOnFootStealth*>(t)->m_target; break; // CR11: actions.ts KILL_TASKS also lists TIMED / STAND_STILL / WHILE_DUCKING / STEALTH
            case TASK_COMPLEX_KILL_PED_ON_FOOT_TIMED: case TASK_COMPLEX_KILL_PED_ON_FOOT_STAND_STILL: case TASK_KILL_PED_ON_FOOT_WHILE_DUCKING: tg = static_cast<CTaskComplexKillPedOnFoot*>(t)->m_target; break;
            default: break;
            }
            if (tg) return tg;
        }
    }
    return nullptr;
}
bool IsBuddyTarget(CPed* target, CPed* attacker, CPed* me) {
    if (target == me || ProtectedPed(target)) return true;
    if (target->m_fHealth <= 0.f || HatesSide((int)target->m_nPedType) || g_enemyTypes.count((int)target->m_nPedType)) return false;
    if ((int)target->m_nPedType < 23 || target->m_nPedType == attacker->m_nPedType) return false;
    if ((int)target->m_nPedType >= 24 && std::getenv("NOTSA_MDRIVER_MISSIONFOE")) return false; // CR21 (OPT-IN: regresses 77 where type-25 MIB agents attack a type-24 friend): a mission-type ped (MISSION1..8, script made) that is neither liked, in CJ's group nor a member of a protected type is a scripted enemy, not a buddy (11 BIG SMOKE: Ryder/Smoke (type 24) shooting a Ballas (type 25) were shot dead by the driver -> INT2_F2)
    auto* tg = KillTarget(target); return !tg || (tg != me && !ProtectedPed(tg));
}
void ShootDead() { // actions.ts shootAttackers: every ped (not a buddy type) whose kill task targets CJ / a buddy is shot dead (max 60)
    if (g_fightKills >= 60) return; auto* me = Cj(); if (!me) return;
    std::vector<int> victims;
    const std::set<int>& guarded = Guarded();
    for (auto& p : GetPedPool()->GetAllValid()) {
        if (&p == me || p.m_fHealth <= 0.f || ProtectedPed(&p) || guarded.count(CPools::GetPedRef(&p))) continue;
        auto* tg = KillTarget(&p); if (!tg || !IsBuddyTarget(tg, &p, me) || LikesSide((int)p.m_nPedType)) continue;
        if (std::getenv("NOTSA_MDRIVER_PLOG")) notsa::standalone::Fixups::Log("[mdplog] shootdead: attacker h=%d type=%d model=%d target h=%d type=%d model=%d hp=%.0f tgtkill=%p", CPools::GetPedRef(&p), (int)p.m_nPedType, (int)p.m_nModelIndex, CPools::GetPedRef(tg), (int)tg->m_nPedType, (int)tg->m_nModelIndex, tg->m_fHealth, (void*)KillTarget(tg));
        victims.push_back(CPools::GetPedRef(&p)); if (tg == me) g_enemyTypes.insert((int)p.m_nPedType); g_fightKills++;
    }
    for (int h : victims) Emit("mdkillped:" + std::to_string(h));
}
static float g_passStat0 = 0.f;
static std::map<int,int> g_padHold; // CR4: button -> frames left
static std::string ShooterDesc(CEntity* e) { // CR18: debug: model, vehicle (model, driver or passenger), distance to CJ, active weapon of the last damager of CJ's car
    if (!e || e->GetType() != ENTITY_TYPE_PED) return "-"; auto* p = static_cast<CPed*>(e); char b[160]; auto* me = Cj();
    std::snprintf(b, sizeof b, "ped%d/inveh=%d(%d,%s)/d=%.1f/wpn=%d", (int)p->m_nModelIndex, (int)p->bInVehicle, p->m_pVehicle ? (int)p->m_pVehicle->m_nModelIndex : -1, p->m_pVehicle ? (p->m_pVehicle->m_pDriver == p ? "drv" : "pax") : "-", me ? (p->GetPosition() - me->GetPosition()).Magnitude() : -1.f, (int)p->GetActiveWeapon().m_Type);
    return b;
}
void TickGeneric() {
    for (auto it = g_padHold.begin(); it != g_padHold.end();) { if (it->second > 0) --it->second; it = it->second > 0 ? std::next(it) : g_padHold.erase(it); }
    if (!g_loaded) { LoadTable(std::getenv("NOTSA_MDRIVER_TABLE")); LoadArity(); if (const char* o = std::getenv("NOTSA_MDRIVER_OUT")) g_out = std::fopen(o, "w"); if (const char* m = std::getenv("NOTSA_MDRIVER_MAXFRAMES")) g_maxFrames = std::atoi(m); }
    if (static const char* st = std::getenv("NOTSA_MDRIVER_STALL")) g_stall = std::atoi(st); // CR16: per-run override of the stall threshold (TS stallFrames = 600 frames of 33.33 ms = 20 s; 1000 frames of the 20 ms RR step are the same game time; allmissions.sh sets it for mission 46)
    if (g_quitIn >= 0) { if (g_quitIn-- == 0) { WriteResult(); Emit("quit"); if (g_out) { std::fclose(g_out); g_out = nullptr; } g_done = true; } return; }
    auto* root = notsa::scmtrace::Root();
    if (!g_started) { if (!root) return; g_started = true; StaticScan(root); if (!std::getenv("NOTSA_MDRIVER_NOCITY")) CStats::SetStatValue(STAT_CITY_UNLOCKED, 2.f); /* CR10: a mission started cold stands where the story would have opened the cities: SF/LV closed = 4 stars on arrival (SetPlayerWantedLevelForForbiddenTerritories), cops attack */ g_passStat0 = CStats::GetStatValue(STAT_MISSIONS_PASSED); g_thread = root; g_frame0 = CTimer::GetFrameCounter(); g_sameCount = 0; g_lastIp = nullptr; }
    const int fr = GFrame();
    if (root) { char n[9]{}; std::memcpy(n, root->m_szName, 8); g_name = n; }
    if (!root) { // the thread ended (TS: !alive(script)): PASSED / FAILED, or STOP when a kill of the driver was just before it
        g_endFrame = fr;
        if (!g_pass && CStats::GetStatValue(STAT_MISSIONS_PASSED) > g_passStat0) { g_pass = true; g_passFrame = fr; } // CR6: TS checks the stat at thread end too (87 78: cutscene-only, the stat rises in the thread's last command)
        if (!g_pass && fr - g_killFrame <= g_killSettle + 5) { g_stopReason = "driver stop: killed a guard"; g_stopOp = -1; }
        else if (!g_pass) g_endedFail = true;
        notsa::standalone::Fixups::Log("[mdriver] STAT_MISSIONS_PASSED %.1f at start %.1f", CStats::GetStatValue(STAT_MISSIONS_PASSED), g_passStat0); std::fprintf(stderr, "[mdriver] mission thread ended at frame %d (pass=%d fail=%d)\n", fr, (int)g_pass, (int)g_fail); g_quitIn = 50; return;
    }
    if (g_pass && fr - g_passFrame > 1500) { g_quitIn = 0; return; }
    if (g_fail && fr - g_failFrame > 600) { g_quitIn = 0; return; }
    if (fr >= g_maxFrames) { Stop("budget: " + std::to_string(g_maxFrames) + " frames", -1, (uint32_t)(root->m_IP - root->m_BaseIP)); return; }
    if (!g_pass && CStats::GetStatValue(STAT_MISSIONS_PASSED) > g_passStat0) { g_pass = true; g_passFrame = fr; } // CR4: TS drive.ts: PASSED = STAT_MISSIONS_PASSED rose (0x0318 SET_LATEST_MISSION_PASSED; missions 87 78 print no M_PASS)
    if (!std::getenv("NOTSA_MDRIVER_NOFIGHT") && ++g_fightClock % 3 == 0 && !CCutsceneMgr::IsCutsceneProcessing() && Cj()) ShootDead();
    if (static const bool plog = std::getenv("NOTSA_MDRIVER_PLOG") != nullptr; plog && Cj() && fr % 50 == 0) { // CR8: CJ state every 50 frames (debug)
        auto* me = Cj(); const auto pp = me->GetPosition(); auto* v = me->bInVehicle ? me->m_pVehicle : nullptr; auto* w = FindPlayerWanted();
        notsa::standalone::Fixups::Log("[mdplog] f%d cj=%.1f,%.1f,%.1f hp=%.0f st=%d inv=%d veh=%d(%.0f hp, st %d, drown %d) wanted=%d lastDmg=%d wpn=%d\n", fr, pp.x, pp.y, pp.z, me->m_fHealth, (int)me->m_nPedState, (int)me->bInVehicle,
            v ? (int)v->m_nModelIndex : -1, v ? v->m_fHealth : 0.f, v ? (int)v->GetStatus() : 0, v ? (int)v->vehicleFlags.bIsDrowning : 0, w ? (int)w->m_WantedLevel : -1, me->m_pLastEntityDamage ? (int)me->m_pLastEntityDamage->GetType() : -1, (int)me->m_nLastWeaponDamage);
        for (int gi = 0; gi < CGarages::NumGarages; gi++) { auto& g = CGarages::GetGarage(gi); if (g.m_nType != PAYNSPRAY || g.CalcDistToGarageRectangleSquared(pp.x, pp.y) > 900.f) continue; notsa::standalone::Fixups::Log("[mdplog] garage %d door=%d inside=%d outside=%d sprayable=%d NoResprays=%d free=%d money=%d spd=%.3f wantedBackOff=%d\n", gi, (int)g.m_nDoorState, (int)g.IsStaticPlayerCarEntirelyInside(), (int)g.IsPlayerOutsideGarage(0.0f), v ? (int)CGarages::IsCarSprayable(v) : -1, (int)CGarages::NoResprays, (int)CGarages::RespraysAreFree, (int)CWorld::Players[CWorld::PlayerInFocus].m_nMoney, v ? v->m_vecMoveSpeed.Magnitude() : -1.f, w ? (int)w->m_bPoliceBackOffGarage : -1); }
    }
    if (static const bool bl = std::getenv("NOTSA_MDRIVER_BIKELOG") != nullptr; bl && root && fr % 30 == 0) { // CR17: debug: the script cars of the locals 52..65 (mission 55 ambush bikes) and CJ's car
        std::string o; for (int li = 52; li < 66; li++) { auto* c = CPools::GetVehicle(LocalAt(root, li)->iParam); if (!c) continue; o += " L" + std::to_string(li) + "=" + std::to_string((int)c->GetPosition().x) + "," + std::to_string((int)c->GetPosition().y) + (c->m_pDriver ? "d" : "-") + (c->m_fHealth > 0 ? "" : "X"); }
        auto* me = Cj(); const auto pp = me ? me->GetPosition() : CVector{}; notsa::standalone::Fixups::Log("[bikelog] f%d cj=%d,%d%s", fr, (int)pp.x, (int)pp.y, o.c_str());
    }
    if (static const bool el = std::getenv("NOTSA_MDRIVER_ENTERLOG") != nullptr; el && Cj() && (fr % 10 == 0 || (std::getenv("NOTSA_MDRIVER_ENTERLOG")[0] == (char)50 && fr > 5880))) { // CR19: debug: CJ's task chain + cars within 12 m (door lock, status, driver) every 10 frames
        if (fr % 200 == 0) { std::string th; for (auto* t = CTheScripts::pActiveScripts; t; t = t->m_pNext) { char n[9]{}; std::memcpy(n, t->m_szName, 8); th += std::string(" ") + n; } int32_t v676 = 0, v409 = 0, v418 = 0, v34 = 0, v40 = 0, v713 = 0; std::memcpy(&v676, CTheScripts::ScriptSpace.data() + 2704, 4); std::memcpy(&v409, CTheScripts::ScriptSpace.data() + 409 * 4, 4); std::memcpy(&v418, CTheScripts::ScriptSpace.data() + 418 * 4, 4); std::memcpy(&v34, CTheScripts::ScriptSpace.data() + 34 * 4, 4); std::memcpy(&v40, CTheScripts::ScriptSpace.data() + 40 * 4, 4); std::memcpy(&v713, CTheScripts::ScriptSpace.data() + 713 * 4, 4); notsa::standalone::Fixups::Log("[enterlog] f%d g676=%d $409(onmission)=%d $418=%d $713=%d $34(hour)=%d $40(area)=%d currArea=%d threads:%s", fr, v676, v409, v418, v713, v34, v40, (int)CGame::currArea, th.c_str()); }
        auto* me = Cj(); const auto pp = me->GetPosition(); std::string tk; auto& tm = me->GetIntelligence()->m_TaskMgr;
        for (int ti = 0; ti < 5; ti++) { if (auto* t = tm.GetTaskPrimary(ti)) { tk += " P" + std::to_string(ti) + ":"; for (auto* tt = t; tt; tt = tt->GetSubTask()) tk += std::to_string((int)tt->GetTaskType()) + ">"; } }
        for (int ti = 0; ti < 6; ti++) { if (auto* t = tm.GetTaskSecondary((eSecondaryTask)ti)) tk += " S" + std::to_string(ti) + ":" + std::to_string((int)t->GetTaskType()); }
        notsa::standalone::Fixups::Log("[enterlog] f%d cj=%.2f,%.2f,%.2f h=%.2f inv=%d st=%d ctl=%s%s", fr, pp.x, pp.y, pp.z, me->GetHeading(), (int)me->bInVehicle, (int)me->m_nPedState, CPad::GetPad(0)->DisablePlayerControls ? "OFF" : "on", tk.c_str());
        if (auto* wv = g_watchVeh >= 0 ? CPools::GetVehicle(g_watchVeh) : nullptr) { const auto wp = wv->GetPosition(); notsa::standalone::Fixups::Log("[enterlog]   watch car %d model %d pos %.2f,%.2f,%.2f hp %.0f st %d spd %.3f drv %s wrecked %d upZ %.2f", g_watchVeh, (int)wv->m_nModelIndex, wp.x, wp.y, wp.z, wv->m_fHealth, (int)wv->GetStatus(), wv->m_vecMoveSpeed.Magnitude(), wv->m_pDriver ? "y" : "-", (int)(wv->GetStatus() == STATUS_WRECKED), wv->GetUp().z); }
        if (auto* bd = g_buddyRef >= 0 ? CPools::GetPed(g_buddyRef) : nullptr) { std::string bt; auto& btm = bd->GetIntelligence()->m_TaskMgr; for (int ti = 0; ti < 5; ti++) if (auto* t = btm.GetTaskPrimary(ti)) { bt += " P" + std::to_string(ti) + ":"; for (auto* tt = t; tt; tt = tt->GetSubTask()) bt += std::to_string((int)tt->GetTaskType()) + ">"; } const auto bp = bd->GetPosition(); notsa::standalone::Fixups::Log("[enterlog]   buddy %d model %d pos %.2f,%.2f,%.2f inv %d st %d hp %.0f veh %p%s", g_buddyRef, (int)bd->m_nModelIndex, bp.x, bp.y, bp.z, (int)bd->bInVehicle, (int)bd->m_nPedState, bd->m_fHealth, (void*)bd->m_pVehicle, bt.c_str()); }
        for (auto& vv : GetVehiclePool()->GetAllValid()) { const auto cp = vv.GetPosition(); if ((cp - pp).Magnitude() > 12.f) continue; std::string occ; for (auto* pa : vv.m_apPassengers) if (pa) occ += "p"; notsa::standalone::Fixups::Log("[enterlog]   car ref %d model %d pos %.2f,%.2f,%.2f fwd %.2f,%.2f lock %d status %d drv %s %s mission %d hp %.0f spd %.3f", GetVehiclePool()->GetRef(&vv), (int)vv.m_nModelIndex, cp.x, cp.y, cp.z, vv.GetForward().x, vv.GetForward().y, (int)vv.m_nDoorLock, (int)vv.GetStatus(), vv.m_pDriver ? (vv.m_pDriver == me ? "CJ" : "ped") : "-", occ.c_str(), (int)vv.m_nCreatedBy, vv.m_fHealth, vv.m_vecMoveSpeed.Magnitude()); }
    }
    if (static const bool hl = std::getenv("NOTSA_MDRIVER_HPLOG") != nullptr; hl && Cj()) { // CR18: debug: every health drop of the car CJ is in (position, speed, water, fire, last damager)
        static float s_hp = -1.f; auto* v = Cj()->bInVehicle ? Cj()->m_pVehicle : nullptr;
        if (v && v->m_fHealth != s_hp) { if (s_hp >= 0.f && v->m_fHealth < s_hp) notsa::standalone::Fixups::Log("[hplog] f%d hp %.0f->%.0f pos=%.1f,%.1f,%.1f spd=%.3f water=%d drown=%d fire=%d wpn=%d dmgEnt=%d inv=%d shooter=%s", fr, s_hp, v->m_fHealth, v->GetPosition().x, v->GetPosition().y, v->GetPosition().z, v->m_vecMoveSpeed.Magnitude(), (int)v->physicalFlags.bTouchingWater, (int)v->vehicleFlags.bIsDrowning, v->m_pFire ? 1 : 0, (int)v->m_nLastWeaponDamageType, v->m_pLastDamageEntity ? (int)v->m_pLastDamageEntity->GetType() : -1, (int)v->physicalFlags.bInvulnerable, ShooterDesc(v->m_pLastDamageEntity).c_str()); s_hp = v->m_fHealth; }
    }
    if (static const bool xl = std::getenv("NOTSA_MDRIVER_ENEXLOG") != nullptr; xl && Cj() && fr % 25 == 0 && fr > 16400 && fr < 17600) { // CR23: debug: why does standing on an entrance marker not start the transition
        auto* me = Cj(); const auto pp = me->GetPosition(); std::string tk; auto& tm = me->GetIntelligence()->m_TaskMgr; for (int ti = 0; ti < 5; ti++) if (auto* t = tm.GetTaskPrimary(ti)) { tk += " P" + std::to_string(ti) + ":"; for (auto* tt = t; tt; tt = tt->GetSubTask()) tk += std::to_string((int)tt->GetTaskType()) + ">"; }
        for (auto& ex : CEntryExitManager::GetPool()->GetAllValid()) { if (std::strncmp(ex.m_szName, "CSCHP", 5) || ex.m_nArea != 0) continue; const auto c = ex.GetPosition(); if ((c - pp).Magnitude() > 30.f) continue;
            notsa::standalone::Fixups::Log("[enexlog] f%d cj=%.2f,%.2f,%.2f inArea=%d access=%d vis=%d on=%d off=%d clock=%d:%d state=%d active=%p inCtl=%d pstate=%d ctlOff=%d gstate=%d disabled=%d cutscene=%d area=%d tasks=%s", fr, pp.x, pp.y, pp.z, (int)ex.IsInArea(pp), (int)ex.bEnableAccess, (int)ex.IsVisibleByTime(), (int)ex.m_nTimeOn, (int)ex.m_nTimeOff, (int)CClock::GetGameClockHours(), (int)CClock::GetGameClockMinutes(), (int)CEntryExitManager::ms_exitEnterState, (void*)CEntryExitManager::mp_Active, (int)me->IsPedInControl(), (int)me->m_nPedState, (int)CPad::GetPad(0)->DisablePlayerControls, (int)CGameLogic::GameState, (int)CEntryExitManager::ms_bDisabled, (int)CCutsceneMgr::ms_cutsceneProcessing, (int)CGame::currArea, tk.c_str()); }
    }
    if (static const char* ll = std::getenv("NOTSA_MDRIVER_LOGLOCALS"); ll && root && fr % 60 == 0 && Cj()) { // CR23: debug: "n,n,..": script locals that hold a ped or vehicle handle: model, position, distance to CJ; and (CARSLOG=1) the vehicles within 150 m of CJ by model
        std::string o; const auto pp = Cj()->GetPosition();
        for (const char* q = ll; *q;) { const int li = std::atoi(q); while (*q && *q != ',') q++; if (*q == ',') q++; const int h = LocalAt(root, li)->iParam; if (auto* pe = CPools::GetPed(h)) { const auto p2 = pe->GetPosition(); o += " L" + std::to_string(li) + "=ped" + std::to_string((int)pe->m_nModelIndex) + "@" + std::to_string((int)p2.x) + "," + std::to_string((int)p2.y) + " d" + std::to_string((int)(p2 - pp).Magnitude()) + (pe->m_fHealth > 0 ? "" : "X") + (pe->bInVehicle ? "v" : ""); } else if (auto* ve = CPools::GetVehicle(h)) { const auto p2 = ve->GetPosition(); o += " L" + std::to_string(li) + "=veh" + std::to_string((int)ve->m_nModelIndex) + "@" + std::to_string((int)p2.x) + "," + std::to_string((int)p2.y) + " d" + std::to_string((int)(p2 - pp).Magnitude()) + (ve->m_fHealth > 0 ? "" : "X") + " spd" + std::to_string((int)(ve->m_vecMoveSpeed.Magnitude() * 100)); } else o += " L" + std::to_string(li) + "=-"; }
        notsa::standalone::Fixups::Log("[loglocals] f%d cj=%d,%d%s", fr, (int)pp.x, (int)pp.y, o.c_str());
        if (std::getenv("NOTSA_MDRIVER_CARSLOG")) { std::map<int, int> cnt; float nearest459 = 1e9f; int total = 0, near150 = 0; for (auto& vv : GetVehiclePool()->GetAllValid()) { total++; const auto cw0 = FindPlayerCentreOfWorld(0); const float d = (vv.GetPosition() - cw0).Magnitude(); if (d < 150.f) { near150++; cnt[(int)vv.m_nModelIndex]++; } if (vv.m_nModelIndex == 459 && d < nearest459) nearest459 = d; } std::string m; for (auto& [k, v] : cnt) m += " " + std::to_string(k) + "x" + std::to_string(v); const auto cw = FindPlayerCentreOfWorld(0); notsa::standalone::Fixups::Log("[carslog] f%d vehicles total %d within150 %d nearest459 %.0f models:%s | currArea %d cj z %.1f centreOfWorld %.0f,%.0f,%.0f numRandomCars %d maxInUse %u densityMul %.2f countDownAtStart %d outside %d", fr, total, near150, nearest459, m.c_str(), (int)CGame::currArea, pp.z, cw.x, cw.y, cw.z, (int)CCarCtrl::NumRandomCars, (unsigned)CCarCtrl::MaxNumberOfCarsInUse, (float)CCarCtrl::CarDensityMultiplier, (int)CCarCtrl::CountDownToCarsAtStart, (int)CGame::CanSeeOutSideFromCurrArea()); }
    }
    if (static const char* cl = std::getenv("NOTSA_MDRIVER_CARLOG"); cl && root && fr % 10 == 0) { // CR16: debug: the script car (local N) and the ped in it every 10 frames
        const int li = std::atoi(cl); const int li2 = std::getenv("NOTSA_MDRIVER_CARLOG2") ? std::atoi(std::getenv("NOTSA_MDRIVER_CARLOG2")) : -1;
        auto* cr = CPools::GetVehicle(LocalAt(root, li)->iParam);
        if (cr) {
            auto* dr = cr->m_pDriver; std::string tk;
            if (dr) { auto& tm = dr->GetIntelligence()->m_TaskMgr; for (int k = 0; k < 5; k++) for (CTask* t = tm.m_aPrimaryTasks[k]; t; t = t->GetSubTask()) tk += std::to_string((int)t->GetTaskType()) + (t->GetSubTask() ? ">" : ""), tk += ","; }
            (void)li2;
            notsa::standalone::Fixups::Log("[carlog] f%d car=%.1f,%.1f,%.1f spd=%.3f st=%d drv=%d dmode=%d tasks=%s ip=%x", fr, cr->GetPosition().x, cr->GetPosition().y, cr->GetPosition().z, cr->m_vecMoveSpeed.Magnitude(), (int)cr->GetStatus(), dr ? (int)dr->m_nModelIndex : -1, (int)cr->m_autoPilot.m_nCarMission, tk.c_str(), (unsigned)(root->m_IP - root->m_BaseIP));
        }
    }
    if (!std::getenv("NOTSA_MDRIVER_NOGOD")) if (auto* me = Cj()) { // CR4: TS drive.ts driverFrame: CJ and the vehicle he drives are invulnerable every frame (AUTODRIVE_GOD=0 off)
        me->physicalFlags.bInvulnerable = true;
        if (me->bInVehicle && me->m_pVehicle) me->m_pVehicle->physicalFlags.bInvulnerable = true;
    }
    for (;;) {
        if (g_wait > 0) { g_wait--; return; }
        if (g_untilLeft > 0) { // MS1: `waituntil` (the tryEnterCar 600-frame wait)
            std::stringstream us(g_until); std::string e, o; double v; us >> e >> o >> v;
            if (Cmp(Eval(e), o, v)) g_untilLeft = 0; else { g_untilLeft--; return; }
        }
        if (g_qpos >= g_queue.size()) break;
        const std::string line = g_queue[g_qpos++];
        if (!line.compare(0, 5, "emit ")) Emit(line.substr(5)); else Dispatch(line);
    }
    g_queue.clear(); g_qpos = 0;
    if (CCutsceneMgr::IsCutsceneProcessing()) g_sameCount = 0;
    else if (root->m_IP == g_lastIp) g_sameCount++;
    else { g_sameCount = 0; g_lastIp = root->m_IP; }
    if (g_sameCount >= g_stall) for (auto& r : g_recs) if (r->waitSeq >= g_waitSeq - 3 && (r->raw & 0x7FFF) == 0x010F && r->cond == 0) { if (auto* w = FindPlayerWanted()) if (w->m_WantedLevel != eWantedLevel::WANTED_CLEAN) { w->SetWantedLevel(eWantedLevel::WANTED_CLEAN); g_sameCount = 0; return; } break; } // CR11: as drive.ts:309 (at the stall threshold, then no decide this round; level only): // CR9: the thread waits for the wanted level to drop (SWEET4 ip 0x45e7 after the second pay n spray stage; the exe decays above 1 star only in the countryside, a real player drives into the pay n spray = ClearWantedLevelAndGoOnParole 0x44AC16): clear it, as the TS port does implicitly
    if (g_sameCount < g_stall) return;
    const uint32_t at = (uint32_t)(root->m_IP - root->m_BaseIP);
    const uint8_t* ipPtr = root->m_IP;
    g_sameCount = 0;
    std::vector<RecP> loop; for (auto& r : g_recs) if (r->waitSeq >= g_waitSeq - 3) loop.push_back(r);
    if (std::getenv("NOTSA_MDRIVER_BIKELOG")) { std::string o; for (auto& r : loop) if (r->cond == 0) { o += " " + std::to_string((unsigned)(r->p - root->m_BaseIP)) + "/" + std::to_string(r->raw & 0x7FFF); } notsa::standalone::Fixups::Log("[decide] frame %d attempt %d loop %zu falses:%s", fr, g_attemptsAt[ipPtr], loop.size(), o.c_str()); }
    Dec d = Decide(loop, g_attemptsAt[ipPtr]);
    if ((!d.act || d.row->action == "wait") && !std::getenv("NOTSA_MDRIVER_NOWIDE") && g_recs.size() > loop.size()) { // CR23 (16 NINES AND AK'S): the loop window (the last 3 passes) is one frame of a script that tests ONE array element per pass (@42 = 0..4, 5 passes per revolution) and the stall frame is phase-locked to the slot of an object that no longer exists: the answer is the plain `wait` of the end-of-stage locate, the target that can be shot is only in older passes. Rule: when the narrow window gives only `wait` (or nothing), look at every retained pass of the stalled loop (the 64 records, all from the stall period) and take a non-wait answer from there.
        const std::vector<RecP> wide(g_recs.begin(), g_recs.end());
        Dec d2 = Decide(wide, g_attemptsAt[ipPtr], true);
        if (d2.act && d2.row->action != "wait") { std::fprintf(stderr, "[mdriver] frame %d widened window (%zu -> %zu records): %s\n", fr, loop.size(), wide.size(), d2.row->name.c_str()); d = d2; }
    }
    if (!d.act) { Stop(d.reason, d.op, at); return; }
    if (d.act && d.row->action == "wait" && g_attemptsAt[ipPtr] >= 1 && !std::getenv("NOTSA_MDRIVER_NOESCALATE")) { // CR21: the wait row answered this wake ip before and the thread still stands: a TRUE `NOT IS_CHAR_DEAD(x)` of the pass is a wait-until-dead (89: the paramedic passengers of the three ambulances stay alive once the cruising ambulances moved on; the plain 0118 rows only see the drivers): kill that live non-buddy char
        for (const int kop : { 0x0118, 0x0119 }) { // CR23 (16 NINES AND AK'S, last stage: `WHILE NOT IS_CAR_DEAD(@83)`): the car form too (the plain 0119 kill row wrecks the car; a car with a buddy / CJ in it is refused by ProtectedHandle)
        if (kop == 0x0119 && std::getenv("NOTSA_MDRIVER_NOCARESCALATE")) break;
        const Row* kr = nullptr; int kri = -1; for (size_t i = 0; i < g_rows.size(); i++) if (g_rows[i].op == kop && g_rows[i].match == 1 && g_rows[i].action == "kill") { kr = &g_rows[i]; kri = (int)i; break; }
        if (kr && d.row == kr) break;
        if (kr) for (size_t k = loop.size(); k-- > 0;) {
            const auto& r = loop[k]; if (r->raw != (0x8000 | kop) || r->cond != 1 || AnyGuard(*r)) continue;
            if (kop == 0x0119 && !(r->waitAfter && !r->jumped)) continue; // the car form only when the TRUE branch IS the WAIT of its group (`WHILE NOT IS_CAR_DEAD(x)`, 16): elsewhere `NOT IS_CAR_DEAD` is a plain if (10: the RC plane @34 whose death fails the mission)
            const auto a2 = ResolveArgs(*kr, *r); if (!a2 || a2->empty()) continue;
            const int h = (int)(*a2)[0].n;
            if (kop == 0x0118) { auto* pp = CPools::GetPed(h); if (!pp || pp == Cj() || pp->m_fHealth <= 0.f || ProtectedHandle(h) || Guarded().count(h)) continue; }
            else { auto* vv = CPools::GetVehicle(h); if (!vv || vv->m_fHealth <= 0.f || ProtectedHandle(h) || (Cj() && Cj()->m_pVehicle == vv)) continue; }
            d.row = kr; d.rowIdx = kri; d.cond = r; d.args = *a2; d.op = kop; break;
        }
        if (d.row == kr) break;
        }
    }
    g_attemptsAt[ipPtr]++;
    const std::string key = std::to_string(at) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args);
    const int n = ++g_noProgress[key];
    g_stopIp = at; g_stopOp = d.op; // the last answered condition (reported when the mission ends without a driver stop)
    if (d.row->action == "failclass") { DispatchRow(d); return; }
    if (n > 8) { char b[16]; std::snprintf(b, sizeof b, "%04X", d.op); Stop("the driver acted " + std::to_string(n - 1) + " times (" + d.row->action + ") and the condition stays false: " + b + " " + d.row->name, d.op, at); return; }
    std::fprintf(stderr, "[mdriver] frame %d ip %x row %d %04X %s -> %s\n", fr, at, d.rowIdx, d.op, d.row->name.c_str(), d.row->action.c_str());
    notsa::standalone::Fixups::Log("[mdriver] decide frame %d ip %x row %d %04X %s -> %s args %s cond-args %zu", fr, at, d.rowIdx, d.op, d.row->name.c_str(), d.row->action.c_str(), RKey(d.args).c_str(), d.cond ? d.cond->args.size() : (size_t)0); // CR16
    DispatchRow(d);
}
} // namespace

bool Enabled() { static const bool e = std::getenv("NOTSA_MDRIVER") != nullptr || Generic(); return e; }
void OnCommand(const CRunningScript* s, const uint8_t* ptr, uint16_t raw, int cond) {
    if (!Generic() || !g_loaded) return;
    if (s == notsa::scmtrace::Root()) { // the StallSink: conditions with their state + operands, WAIT count, where the GOTO_IF_FALSE after them led
        const int op = raw & 0x7FFF;
        if (g_fallPending) { if (op == 0x0001) for (auto& r : g_group) r->waitAfter = true; g_fallPending = false; g_group.clear(); } // CR19: a group that fell through straight into a WAIT
        if (op == 0x0001) g_waitSeq++;
        else if (op == 0x004D) {
            const bool jumped = s->m_IP != ptr + 7;
            for (auto& r : g_pending) { r->hasBranch = true; r->jumped = jumped; r->fall = ptr + 7; r->target = jumped ? s->m_IP : ptr + 7; }
            if (!jumped) { g_group = g_pending; g_fallPending = true; }
            g_pending.clear();
        } else if (op == 0x077E) { // CR23: GET_AREA_VISIBLE var: remember the destination variable (a later `var == N` wait = be in interior N)
            if (InSpace(ptr)) { uint32_t ref = 0; if (OperandVar(ptr + 2, ref)) g_areaVars.insert(ref); }
        } else if (op == 0x0702) { // CR22: GET_PERCENTAGE_TAGGED_IN_AREA x1 y1 x2 y2 -> var: remember the area and the destination variable (a later `var == 100` wait = spray these tags)
            if (InSpace(ptr)) {
                const uint8_t* ops5[5]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0;
                for (; k < 5 && q && InSpace(q); k++) { ops5[k] = q; q = SkipOperand(q, mf, isI, iv); }
                uint32_t ref = 0; const auto a = DecodeArgs(s, ptr, 4);
                if (k == 5 && a.size() == 4 && OperandVar(ops5[4], ref)) { g_tagAreas[ref] = { AF(a[0]), AF(a[1]), AF(a[2]), AF(a[3]) }; }
            }
        } else if (op == 0x00AA || op == 0x073E) { // CR24 (10 BEEFY BARON): GET_CAR_COORDINATES h x y z (remember which vehicle the coordinate variable was read from), then GET_RANDOM_CAR_IN_SPHERE_NO_SAVE x y z radius model -> var: when the centre is a vehicle's own coordinates, remember (result var -> that vehicle, model, radius)
            if (InSpace(ptr)) {
                const int nops = op == 0x00AA ? 2 : 6; const uint8_t* o[6]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0;
                for (; k < nops && q && InSpace(q); k++) { o[k] = q; q = SkipOperand(q, mf, isI, iv); }
                uint32_t ref = 0, ref2 = 0;
                if (k == nops && op == 0x00AA) { const auto a = DecodeArgs(s, ptr, 1); if (a.size() == 1 && OperandVar(o[1], ref)) g_coordVeh[ref] = (int)(int32_t)a[0].bits; }
                else if (k == nops && OperandVar(o[0], ref) && OperandVar(o[5], ref2)) { const auto a = DecodeArgs(s, ptr, 5); if (a.size() == 5) { if (auto it = g_coordVeh.find(ref); it != g_coordVeh.end()) g_sphSearch[ref2] = { it->second, (int)(int32_t)a[4].bits, AF(a[3]) }; else g_sphSearch.erase(ref2); } }
            }
        } else if (cond >= 0) {
            auto r = std::make_shared<Rec>(); r->p = ptr; r->raw = raw; r->cond = cond; r->waitSeq = g_waitSeq;
            const int n = Arity(op); r->args = DecodeArgs(s, ptr, n >= 0 ? n : 10);
            { const uint8_t* ops[3]{}; int cnt = 0; if (InSpace(ptr) && CmdOps(ptr, ops, cnt)) for (int k = 0; k < 2 && k < cnt; k++) { uint32_t ref; if (OperandVar(ops[k], ref)) r->vref[k] = ref; } } // CR22
            if (!(raw & 0x8000) && (op == 0x0038 || op == 0x0039)) g_flagConds[ptr] = r; // CR22
            g_recs.push_back(r); if (g_recs.size() > 64) g_recs.pop_front();
            if (!(raw & 0x8000) && op == 0x0118) g_deadConds[ptr] = r; // CR21: every plain IS_CHAR_DEAD site seen (the 64-condition window misses the rarely-run ones)
            g_pending.push_back(r); if (g_pending.size() > 12) g_pending.erase(g_pending.begin());
        }
    }
    if (ptr[2] != 9) return;
    const char* lab = (const char*)(ptr + 3); const int op = raw & 0x7FFF;
    if (!std::memcmp(lab, "M_PASS\0", 7)) { if (!g_pass) { g_pass = true; g_passFrame = GFrame(); } return; }
    if (!std::memcmp(lab, "M_FAIL\0", 7) && op >= 0x00BA && op <= 0x00BD) { // CR4: only the print opcodes (TS branch.ts); mission 17 runs 03D5 CLEAR_THIS_PRINT M_FAIL at frame 2
        if (!g_fail) { g_fail = true; g_failFrame = GFrame(); g_failText = g_lastLabel.empty() ? "M_FAIL" : g_lastLabel; }
        return;
    }
    if (op >= 0x00BA && op <= 0x00BD) g_lastLabel.assign(lab, strnlen(lab, 8));
}
bool PadHeld(int button) { auto it = g_padHold.find(button); return it != g_padHold.end() && it->second > 0; }
void SetKeyPusher(void (*fn)(const char*, bool)) { g_push = fn; }
void SetCmdRunner(void (*fn)(const std::string&)) { g_run = fn; }

bool Cmd(const char* name, const std::string& args) {
    if (std::strncmp(name, "md", 2)) return false;
    auto* ped = Cj();
    if (!std::strcmp(name, "mdkillped")) { if (auto* k = CPools::GetPed(std::atoi(args.c_str()))) { if (std::getenv("NOTSA_MDRIVER_PLOG")) notsa::standalone::Fixups::Log("[mdplog] kill h=%s type=%d model=%d hp=%.0f pos=%.0f,%.0f wanted=%d\n", args.c_str(), (int)k->m_nPedType, (int)k->m_nModelIndex, k->m_fHealth, k->GetPosition().x, k->GetPosition().y, (int)FindPlayerWanted()->m_WantedLevel); if (ped) CWeapon::GenerateDamageEvent(k, ped, WEAPON_PISTOL, 1000, PED_PIECE_TORSO, 0); /* CR13: no forced health 0 / PEDSTATE_DEAD fallback: TS shootDead has none, the ped dies through its death task over the next frames */ } return true; }
    if (!std::strcmp(name, "mdkillcar")) { if (auto* k = CPools::GetVehicle(std::atoi(args.c_str()))) k->BlowUpCar(nullptr, false); return true; }
    if (!std::strcmp(name, "mdpad")) { const auto c = args.find(':'); if (c != std::string::npos) g_padHold[std::atoi(args.c_str())] = std::atoi(args.c_str() + c + 1) + 1; return true; }
    if (!std::strcmp(name, "mdkey")) { const auto c = args.find(':'); if (g_push) g_push(args.c_str() + c + 1, args.compare(0, c, "down") == 0); return true; }
    if (!ped) return true;
    if (!std::strcmp(name, "mdskipcut")) { if (CCutsceneMgr::IsCutsceneProcessing()) CCutsceneMgr::SkipCutscene(); return true; }
    if (!std::strcmp(name, "mdarea")) { const int ar = std::atoi(args.c_str()); CGame::currArea = (eAreaCodes)ar; ped->SetAreaCode((eAreaCodes)ar); CEntryExitManager::SetAreaCodeForVisibleObjects(); return true; } // launch.ts LAUNCH_AREA
    if (!std::strcmp(name, "mdlaunch")) { // launch.ts LAUNCH_VEHICLE: CJ at the wheel of a car of this model (the main script starts these missions with him in one)
        const int model = std::atoi(args.c_str());
        CStreaming::RequestModel(model, STREAMING_GAME_REQUIRED); CStreaming::LoadAllRequestedModels(false);
        const auto pp = ped->GetPosition();
        if (auto* car = CCarCtrl::CreateCarForScript(model, CVector{ pp.x + 3.f, pp.y, pp.z + 1.f }, false)) { CTaskSimpleCarSetPedInAsDriver t(car, true, nullptr); t.ProcessPed(ped); }
        return true;
    }
    if (!std::strcmp(name, "mdwped") || !std::strcmp(name, "mdwveh") || !std::strcmp(name, "mdwother")) { // AD4: actions.ts warp / warpOther (z <= -100 = the ground; a ped lands at z + 1.1, a car at z)
        CEntity* tgt = ped; bool isPed = !std::strcmp(name, "mdwped"); std::string rest = args;
        if (!std::strcmp(name, "mdwveh")) { if (ped->bInVehicle && ped->m_pVehicle) tgt = ped->m_pVehicle; else isPed = true; } // CR3: mdwveh = warp the CAR the player is in; tgt stayed the PED and was static_cast to CVehicle (NaN player position -> script wanted 4 + cop flood, missions 24/25/27-31/67/80 crash)
        if (!std::strcmp(name, "mdwother")) { const int kind = std::atoi(rest.c_str()); rest = rest.substr(rest.find(':') + 1); const int h = std::atoi(rest.c_str()); rest = rest.substr(rest.find(':') + 1); isPed = kind == 0; tgt = isPed ? (CEntity*)CPools::GetPed(h) : (CEntity*)CPools::GetVehicle(h); if (!tgt) return true; }
        float w[3]{}; std::sscanf(rest.c_str(), "%f:%f:%f", w, w + 1, w + 2);
        CStreaming::LoadScene(CVector{ w[0], w[1], 0.f }); CStreaming::LoadSceneCollision(CVector{ w[0], w[1], 0.f });
        const float gz = w[2] <= -100.f ? CWorld::FindGroundZForCoord(w[0], w[1]) : w[2];
        CVehicle* veh = nullptr; if (isPed) { auto* pd = static_cast<CPed*>(tgt); if (pd->bInVehicle) veh = pd->m_pVehicle; } else veh = static_cast<CVehicle*>(tgt);
        if (veh) { CCarCtrl::SetCoordsOfScriptCar(veh, w[0], w[1], gz, 0, 1); veh->m_vecMoveSpeed = CVector{}; veh->m_vecTurnSpeed = CVector{}; if (std::getenv("NOTSA_MDRIVER_ENTERLOG")) g_watchVeh = GetVehiclePool()->GetRef(veh); }
        else { CWorld::Remove(tgt); tgt->SetPosn(CVector{ w[0], w[1], gz + 1.1f }); CWorld::Add(tgt); }
        CStreaming::LoadScene(CVector{ w[0], w[1], gz }); CStreaming::LoadSceneCollision(CVector{ w[0], w[1], gz });
        notsa::standalone::Fixups::Log("[mdriver] %s gf=%u -> %.2f,%.2f,%.2f", name, (unsigned)CTimer::GetFrameCounter(), w[0], w[1], gz);
        return true;
    }
    if (!std::strcmp(name, "mdwarpin")) { // tryEnterCar fallback: the Enter key did not get him in -> the script's own WARP_CHAR_INTO_CAR
        auto* car = CPools::GetVehicle(std::atoi(args.c_str()));
        if (std::getenv("NOTSA_MDRIVER_ENTERLOG")) notsa::standalone::Fixups::Log("[enterlog] mdwarpin ref %s car=%p inVeh=%d sameCar=%d", args.c_str(), (void*)car, (int)ped->bInVehicle, (int)(ped->bInVehicle && ped->m_pVehicle == car));
        if (car && !(ped->bInVehicle && ped->m_pVehicle == car)) { const auto pp = ped->GetPosition(), cp = car->GetPosition(); notsa::standalone::Fixups::Log("[mdriver] mdwarpin: Enter failed: CJ %.1f,%.1f,%.1f hp %.0f state %d task %d; car model %d at %.1f,%.1f,%.1f status %d locks %d driver %d controls %s", pp.x, pp.y, pp.z, ped->m_fHealth, (int)ped->m_nPedState, ped->GetIntelligence()->m_TaskMgr.GetActiveTask() ? (int)ped->GetIntelligence()->m_TaskMgr.GetActiveTask()->GetTaskType() : -1, (int)car->m_nModelIndex, cp.x, cp.y, cp.z, (int)car->GetStatus(), (int)car->m_nDoorLock, car->m_pDriver ? 1 : 0, CPad::GetPad(0)->DisablePlayerControls ? "OFF" : "on"); ped->GetIntelligence()->FlushImmediately(false); CTaskSimpleCarSetPedInAsDriver t(car, true, nullptr); t.ProcessPed(ped); notsa::standalone::Fixups::Log("[mdriver] mdwarpin gf=%u: warped in (the Enter key did not get him in)", (unsigned)CTimer::GetFrameCounter()); }
        return true;
    }
    if (!std::strcmp(name, "mdwarppass")) { // actions.ts entercar of another char: set in as a passenger
        const int h = std::atoi(args.c_str()); auto* car = CPools::GetVehicle(std::atoi(args.c_str() + args.find(':') + 1)); auto* oth = CPools::GetPed(h);
        if (car && oth) { oth->GetIntelligence()->FlushImmediately(false); CTaskSimpleCarSetPedInAsPassenger t(car, TARGET_DOOR_FRONT_RIGHT, true, nullptr); t.ProcessPed(oth); g_buddyRef = h; }
        if (std::getenv("NOTSA_MDRIVER_ENTERLOG")) notsa::standalone::Fixups::Log("[enterlog] mdwarppass ped %d=%p car %d=%p -> inVeh %d same %d model %d", h, (void*)oth, std::atoi(args.c_str() + args.find(':') + 1), (void*)car, oth ? (int)oth->bInVehicle : -1, oth ? (int)(oth->m_pVehicle == car) : -1, oth ? (int)oth->m_nModelIndex : -1);
        return true;
    }
    if (!std::strcmp(name, "mddmgobj")) { // CR22: CObject::ObjectDamage, CJ the damager, pistol (the port's object damage path of a bullet hit)
        if (auto* o = CPools::GetObject(std::atoi(args.c_str()))) { const CVector pos = o->GetPosition(); CVector dir = pos - ped->GetPosition(); dir.Normalise(); o->ObjectDamage(1000.f, &pos, &dir, ped, WEAPON_PISTOL); notsa::standalone::Fixups::Log("[mdriver] mddmgobj %s model %d renderDamaged=%d visible=%d", args.c_str(), (int)o->m_nModelIndex, (int)o->m_bRenderDamaged, (int)o->m_bIsVisible); }
        return true;
    }
    if (!std::strcmp(name, "mdtag")) { // CR22: spray every tag of the area the way the spray can shot does (CWorld::SprayPaintWorld: +8 alpha per hit to the tags within the range), until each is fully tagged
        float r[4]{}; std::sscanf(args.c_str(), "%f:%f:%f:%f", r, r + 1, r + 2, r + 3);
        const CRect area{ std::min(r[0], r[2]), std::min(r[1], r[3]), std::max(r[0], r[2]), std::max(r[1], r[3]) };
        std::vector<CVector> pos; for (auto& t : CTagManager::GetTagsInArea(area)) pos.push_back(t.Entity->GetPosition());
        int shots = 0;
        for (const auto& q : pos) for (int k = 0; k < 40; k++) { CVector o = q, dir{}; if (CWorld::SprayPaintWorld(o, dir, 1.0f, true) != eSprayPaintState::DISCOVERED) break; shots++; }
        notsa::standalone::Fixups::Log("[mdriver] mdtag area %.0f,%.0f..%.0f,%.0f: %zu tags, %d spray hits, now %d%%", r[0], r[1], r[2], r[3], pos.size(), shots, CTagManager::GetPercentageTaggedInArea(area));
        return true;
    }
    float a[6]{}; a[5] = -1.f; std::sscanf(args.c_str(), "%f:%f:%f:%f:%f:%f", a, a + 1, a + 2, a + 3, a + 4, a + 5);
    CVector p{ a[0], a[1], a[2] };
    if (p.z <= -100.f) { CStreaming::LoadScene(CVector{ p.x, p.y, 0.f }); CStreaming::LoadSceneCollision(CVector{ p.x, p.y, 0.f }); p.z = CWorld::FindGroundZForCoord(p.x, p.y) + 1.0f; } // z <= -100 = the ground (SET_CHAR_COORDINATES convention)
    CStreaming::LoadScene(p); CStreaming::LoadSceneCollision(p);
    { char b[200]; std::snprintf(b, sizeof b, "[mdriver] %s gf=%u ped=%.2f,%.2f,%.2f -> %.2f,%.2f,%.2f", name, (unsigned)CTimer::GetFrameCounter(), ped->GetPosition().x, ped->GetPosition().y, ped->GetPosition().z, a[0], a[1], a[2]); notsa::standalone::Fixups::Log("%s", b); }
    if (!std::strcmp(name, "mdped")) { // CWorld::Remove + SetPosn + heading + Add (the headless TS harness's addToWorldAt)
        CWorld::Remove(ped);
        ped->SetPosn(p);
        ped->m_fCurrentRotation = ped->m_fAimingRotation = a[3];
        ped->SetHeading(a[3]);
        CWorld::Add(ped);
        if (a[5] >= 0.f) SetPlayerArea(ped, (int)a[5], p.x, p.y, p.z);
        if (a[4] != 0.f) { TheCamera.SetCameraDirectlyBehindForFollowPed_CamOnAString(); TheCamera.RestoreWithJumpCut(); }
        return true;
    }
    if (!std::strcmp(name, "mdveh")) {
        if (auto* v = ped->GetVehicleIfInOne()) { CCarCtrl::SetCoordsOfScriptCar(v, p.x, p.y, p.z, 0, 1); v->m_vecMoveSpeed = CVector{}; v->m_vecTurnSpeed = CVector{}; }
        return true;
    }
    return false;
}

void Tick(uint64_t clockMs) {
    if (!Enabled() || g_done) return;
    g_clock = clockMs;
    if (Generic()) { TickGeneric(); return; }
    if (!g_loaded) { Load(std::getenv("NOTSA_MDRIVER")); if (const char* o = std::getenv("NOTSA_MDRIVER_OUT")) g_out = std::fopen(o, "w"); }
    if (g_quitIn >= 0) {
        if (g_quitIn-- == 0) { Emit("quit"); if (g_out) { std::fclose(g_out); g_out = nullptr; } g_done = true; }
        return;
    }
    if (!g_started) { g_thread = FindThread(); if (!g_thread) return; g_started = true; g_sameCount = 0; g_lastIp = nullptr; std::fprintf(stderr, "[mdriver] thread %s found\n", g_script.c_str()); }
    else if (g_thread != FindThread()) { std::fprintf(stderr, "[mdriver] thread %s ended: quit in 50 frames\n", g_script.c_str()); g_quitIn = 50; return; }
    // a running step
    for (;;) {
        if (g_wait > 0) { g_wait--; return; }
        if (g_untilLeft > 0) {
            std::stringstream ss(g_until); std::string e, o; double v; ss >> e >> o >> v;
            if (Cmp(Eval(e), o, v)) g_untilLeft = 0; else { g_untilLeft--; return; }
        }
        if (g_qpos >= g_queue.size()) break;
        Dispatch(g_queue[g_qpos++]);
    }
    g_queue.clear(); g_qpos = 0;
    // the stall trigger (mission6Harness.ts playMissionBody): the thread at one IP for N frames outside a cutscene
    if (CCutsceneMgr::IsCutsceneProcessing()) g_sameCount = 0;
    else if (g_thread->m_IP == g_lastIp) g_sameCount++;
    else { g_sameCount = 0; g_lastIp = g_thread->m_IP; }
    if (g_sameCount < g_stall) return;
    const int at = (int)(g_thread->m_IP - g_thread->m_BaseIP);
    g_sameCount = 0; g_lastIp = nullptr;
    for (auto& s : g_steps) {
        bool ok = true;
        for (auto& c : s.conds) { if (!Cmp(Eval(c.expr), c.op, c.val)) { ok = false; break; } }
        if (!ok) continue;
        if (g_answers[at]++ >= g_maxAnswers) { g_failed = true; std::fprintf(stderr, "[mdriver] stop: no progress at ip %x after %d answers (step %s)\n", at, g_maxAnswers, s.name.c_str()); g_quitIn = 0; return; }
        std::fprintf(stderr, "[mdriver] frame %u clock %llu step %s at ip %x\n", (unsigned)CTimer::GetFrameCounter(), (unsigned long long)clockMs, s.name.c_str(), at);
        g_queue = s.actions; g_qpos = 0;
        return;
    }
}
} // namespace notsa::mdriver
#endif
