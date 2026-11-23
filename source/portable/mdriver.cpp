#ifdef NOTSA_MDRIVER
#include "StdInc.h"
#include "mdriver.h"
#include "Garages.h"
#include "UserDisplay.h"
#include "MenuSystem.h"
#include "ProjectileInfo.h"
#include "Pickups.h"
#include "Explosion.h"
#include "GangWars.h"
#include "TheZones.h"
#include "PedGroup.h"
#include "Tasks/TaskTypes/TaskComplexBeInGroup.h"
#include "Events/EventScriptCommand.h"
#include "Tasks/TaskTypes/TaskComplexEnterCarAsPassenger.h"
#include "Tasks/TaskTypes/TaskSimpleCarDrive.h"
#include "Tasks/TaskComplexSequence.h"
#include "Events/EventGroupEvent.h"
#include "Events/EventLeaderEnteredCarAsDriver.h"
#include "PedGroupIntelligence.h"
#include "Radar.h"
#include "loadlog.h"
#include "standalone/Fixups.h"
#include "Scripts/TheScripts.h"
#include "Scripts/RunningScript.h"
#include "CutsceneMgr.h"
#include "EntryExitManager.h"
#include "CarEnterExit.h"
#include "TagManager.h"
#include "CarCtrl.h"
#include "VehicleRecording.h"
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
#include "Tasks/TaskTypes/TaskSimpleGangDriveBy.h"
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

extern std::array<CMenuSystem::Menu*, 2> MenuNumber; // MenuSystem.cpp
struct NotsaInjectMissionInfo { int32 offset; uint8 bytes[16]; };
extern int32 NotsaLastInjectedMission; bool NotsaInjectStartMission(int32 missionId, NotsaInjectMissionInfo* info); // MissionDebugModule.cpp (CR33)
namespace notsa::mdriver {
namespace {
bool Generic();
struct Cond { std::string expr, op; double val; };
struct Step { std::string name; std::vector<Cond> conds; std::vector<std::string> actions; };
std::vector<Step>        g_steps;
std::string              g_script;           // thread name
std::map<int, int>       g_nearCnt; /* CR32 */
int                      g_watchVeh = -1, g_buddyRef = -1, g_stall = 600, g_maxAnswers = 12; int g_nextStall = 0; int g_dangerUntil = 0, g_vhpPrev = -1, g_dropWin = 0; const void* g_vhpVeh = nullptr; /* CR31 urgency */ int g_photoPed = -1, g_photoHold = 0; bool g_photoEarly = false;
bool                     g_loaded = false, g_started = false, g_done = false, g_failed = false;
std::FILE*               g_out = nullptr;
void                   (*g_push)(const char*, bool) = nullptr;
void                   (*g_run)(const std::string&) = nullptr;
CRunningScript*          g_thread = nullptr;
int                      g_sameCount = 0;
const uint8*             g_lastIp = nullptr; std::map<const uint8*, int> g_ipSeen; // CR46: frame each wake ip was last seen (a loop over a few wait sites is one stall)
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
// CR26 (20 LOS SEPULCROS): the car part of SET_GROUP_MEMBER (Group13_15.cpp SetGroupMember, exe 0x4923BA): the group's leader is in a vehicle with a free passenger seat => the group's response to CEventLeaderEnteredCarAsDriver (TASK_GROUP_ENTER_CAR: the follower gets the enter-car-as-passenger script task and, once, the default task sequence {CarDrive, previous default}: that is what makes IS_CHAR_SITTING_IN_CAR (TASK_SIMPLE_CAR_DRIVE) true for a group member)
void GroupEnterLeaderCar(CPed* ped, int idx) {
    if (!ped || idx < 0 || idx >= 8) return;
    auto& group = CPedGroups::ms_groups[idx]; auto& mem = group.GetMembership();
    CPed* const leader = mem.GetLeader(); if (!leader) return;
    CVehicle* veh = leader->bInVehicle ? leader->m_pVehicle : nullptr; if (!veh) return;
    bool hasFreeSeat = false; for (int32 i = 0; i < (int32)veh->m_nMaxPassengers; i++) if (!veh->m_apPassengers[i]) { hasFreeSeat = true; break; }
    if (!hasFreeSeat) return;
    CEventLeaderEnteredCarAsDriver leaderEvent{ veh }; leaderEvent.ComputeResponseTaskType(&group);
    auto& intel = group.GetIntelligence();
    switch ((int16)leaderEvent.m_TaskId) {
    case TASK_GROUP_ENTER_CAR_AND_PERFORM_SEQUENCE: { CEventGroupEvent groupEvent{ leader, new CEventLeaderEnteredCarAsDriver{ veh } }; intel.AddEvent(&groupEvent); break; }
    case TASK_GROUP_ENTER_CAR: {
        CTaskComplexEnterCarAsPassenger enterTask{ veh, 0, true }; intel.SetScriptCommandTask(ped, enterTask);
        if (!ped->bHasGroupDriveTask) { ped->bHasGroupDriveTask = true; CTaskComplexSequence seq; seq.AddTask(new CTaskSimpleCarDrive{ veh, nullptr, true }); if (const auto def = intel.GetTaskDefault(ped)) seq.AddTask(def->Clone()); intel.SetDefaultTask(ped, seq); }
        break; }
    default: break;
    }
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
struct Rec { const uint8_t* p; uint16_t raw; int cond; int waitSeq; std::vector<Arg> args; bool hasBranch = false, jumped = false, waitAfter = false, orSite = false; const uint8_t *target = nullptr, *fall = nullptr; uint32_t vref[2] = { 0, 0 }; /* CR22: identity of operand 0 / 1 when it is a variable: (type << 16) | offset-or-index, else 0 */ };
using RecP = std::shared_ptr<Rec>;
static const size_t g_bigCap = std::getenv("NOTSA_MDRIVER_BIGCAP") ? (size_t)std::atoi(std::getenv("NOTSA_MDRIVER_BIGCAP")) : 400; // CR39: the window of the last conditional commands
CVehicle* NearestCarOfModel(CPed* c, int model, float best); // CR39
std::vector<int> ModelsWantedAfterCarLocate(const uint8_t* p); // CR49
bool CoordLeashLive(); // CR42
bool PeekFootLocateXY(const uint8_t* p, float& x, float& y);
std::set<std::string> g_enexNames; // CR27: text literals the mission compares an entry exit name with (IS_VAR_TEXT_LABEL16_EQUAL_TO_TEXT_LABEL after GET_NAME_OF_ENTRY_EXIT_CHAR_USED): the entrance the stage wants
CEntryExit* g_enterEx = nullptr; // CR27: the entry exit the `enterarea` verb goes through (an interior blip's), instead of searching by area
bool g_auxMode = false; // CR27: Decide() over an aux (streamed script) thread answers only with warps / key presses (no kills of the streamed script's peds)
std::map<const uint8_t*, RecP> g_deadConds; // CR21
CVector g_lastBlipPos; bool g_haveLastBlip = false; // CR48: the position of the last ADD_BLIP_FOR_COORD the root script ran (the radar slot counters do not order blips of different slots)
std::map<const uint8_t*, RecP> g_flagConds; // CR22: every plain `var == number` condition seen (IS_INT_VAR/LVAR_EQUAL_TO_NUMBER): the flag tests of the fail-flag guard
std::set<uint32_t> g_timerVars; std::set<uint32_t> g_areaVars; int g_enterArea = -1; // CR23: variables written by GET_AREA_VISIBLE (077E); the interior id a compare of one of them asks for
std::map<uint32_t, int> g_coordVeh; struct SphSearch { int veh; int model; float radius; }; std::map<uint32_t, SphSearch> g_sphSearch; SphSearch g_sphCur{}; // CR24: GET_CAR_COORDINATES x var -> vehicle handle; GET_RANDOM_CAR_IN_SPHERE (073E) result var -> {the vehicle whose coordinates are the centre, model, radius}
std::map<uint32_t, std::array<float, 4>> g_tagAreas; float g_tagRect[4]{}; // CR22: destination variable -> area of the last GET_PERCENTAGE_TAGGED_IN_AREA (0702) that wrote it; the area of the picked decision
std::deque<RecP>         g_recsBig;          // CR26: the last 400 conditional commands (the 64-condition window is too short for a pass of ~100 conditions)
std::deque<RecP>         g_recs;             // the last 64 conditional commands (KEEP)
std::vector<RecP>        g_group; bool g_fallPending = false; // CR19: the conditions of the last GOTO_IF_FALSE, and whether it fell through (the next command decides waitAfter)
std::vector<RecP>        g_pending;          // the conditions since the last GOTO_IF_FALSE (max 12)
int                      g_waitSeq = 0;
std::map<int, int>       g_arity;            // opcode -> operand count (tools/standalone/scmscan/argcounts.tsv, exe derived; -1 variadic)
uint32_t                 g_frame0 = 0;
std::string              g_name = "?", g_stopReason, g_failText, g_lastLabel;
int                      g_stopOp = -1; uint32_t g_stopIp = 0;
bool                     g_pass = false, g_fail = false, g_endedFail = false, g_relaunched = false; int g_relaunchWait = 0;
int                      g_passFrame = -1, g_failFrame = -1, g_endFrame = -1, g_killFrame = -1000000, g_killSettle = 0, g_wantedClrFrame = -1000000, g_scriptWantedFrame = -1000000;
int                      g_maxFrames = 30000, g_fightClock = 0, g_fightKills = 0;
struct HurtNow { int h = 0; int hp = 0; } g_hurtNow; // CR37: a false `N > GET_CHAR_HEALTH var` seen in the pass: the ped is hurt at the next tick (37: the scripted biker punches CJ to death 550 frames after the stage starts, the 600-frame stall answer is too late)
std::set<int>            g_enemyTypes;       // ped types shot for going after CJ himself (TS enemyTypes)
// CR26 (58): a photo test of the retained passes with a GET_CAMERA_FOV (0801 into a local) within 2 KB before it = the script wants a ZOOMED shot (`N > fov`)
bool PhotoWantsZoom() { return std::any_of(g_recs.begin(), g_recs.end(), [](const RecP& q) { if ((q->raw & 0x7FFF) != 0x04C5 || !g_thread) return false; const uint8_t* lo = std::max((const uint8_t*)g_thread->m_BaseIP, q->p - 0x800); for (const uint8_t* b = lo; b + 3 <= q->p; b++) if (b[0] == 0x01 && b[1] == 0x08 && b[2] == 0x03) return true; return false; }); }
// CR26 (21 DOBERMAN: `IF NOT IS_PLAYER_IN_INFO_ZONE($8, 'GLN1') ... WAIT`): the zone label of a 0583 / 0154-like command whose second operand is an 8-char text label, and the centre of the first info zone with that label
bool ZoneNameOf(const Rec& r, char (&nm)[9]) {
    const uint8_t* q = r.p + 2; const uint8_t t = *q++;
    switch (t) { case 1: case 6: q += 4; break; case 2: case 3: case 5: q += 2; break; case 4: q += 1; break; default: return false; }
    if (*q++ != 9) return false;
    std::memcpy(nm, q, 8); nm[8] = 0; return true;
}
bool InfoZoneCentre(const char* nm, float& x, float& y) {
    for (auto& z : CTheZones::GetNavigationZones()) if (std::strncmp(z.m_InfoLabel, nm, 8) == 0) { x = ((float)z.m_fX1 + (float)z.m_fX2) / 2.f; y = ((float)z.m_fY1 + (float)z.m_fY2) / 2.f; return true; }
    return false;
}
std::map<const uint8_t*, int> g_attemptsAt;  // actions per stall ip
int g_pbNearUntil = 0; // CR44
std::map<std::string, int>    g_noProgress;  // (ip, row, args) -> times
bool Generic() { static const bool g = std::getenv("NOTSA_MDRIVER_TABLE") != nullptr; return g; }
bool Ad3() { static const bool g = std::getenv("NOTSA_MDRIVER_AD3") != nullptr; return g; }
float KillFarM() { static const float g = std::getenv("NOTSA_MDRIVER_KILLFARM") ? (float)std::atof(std::getenv("NOTSA_MDRIVER_KILLFARM")) : 800.f; return g; } // CR48 (89 INTENSIVE CARE: a victim 539 m away is reachable; the chased driver of 70 is 1288-1531 m away): 400 -> 800
float MovingAreaM() { static const float g = std::getenv("NOTSA_MDRIVER_MOVINGAREAM") ? (float)std::atof(std::getenv("NOTSA_MDRIVER_MOVINGAREAM")) : 0.f; return g; } // CR48 (opt-in distance of the car to the area under which the moving driver is waited for; 70 passes without the rule, 42 shifted 660 frames with it)

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
static bool g_walkThrough = false; // CR49: the walk of an OR-group guard site follows the nested IF chain of its true branch
const bool g_oldWalkFail = std::getenv("NOTSA_MDRIVER_OLDWALKFAIL") != nullptr; // CR25 off-switch: restores the walk past following conditions
bool g_walkRet = false; // CR39: the last WalkFail ended in a RETURN of the walked level (a site inside a GOSUB'd subroutine returns to its caller, that is no mission failure)
bool WalkFail(const CRunningScript* s, const uint8_t* p, int hops, bool sub, std::set<const uint8_t*>& seen) {
    for (int n = 0; n < 200 && InSpace(p); n++) {
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (!seen.insert(p).second) return false;
        if (op == 0x0001) return false;
        if ((op == 0x00D6 || op == 0x004D) && !g_oldWalkFail && !g_walkThrough) return false; // CR25: the true branch ends at its first condition / GOTO_IF_FALSE (14: the dealers IS_CHAR_DEAD waits continue into later IF chains that reach an M_FAIL print further on)
        if (op == 0x0051) { if (!sub) g_walkRet = true; return !sub; }
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
        if (op == 0x0001 || op == 0x0051 || op == 0x00D6 || op == 0x004D || (op == 0x0050 && std::getenv("NOTSA_MDRIVER_NOGOSUBFLAG"))) return; // CR41 (59 JIZZY: `IS_CHAR_DEAD @67 -> GOSUB cleanup, PRINT_NOW SYN_F_5, @37 = 1`, the loop head is `WHILE @37 == 0`): a GOSUB returns, the straight line goes on past it
        const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return;
        if ((op == 0x0004 || op == 0x0006) && cnt >= 2) { uint32_t ref; int32_t v; if (OperandVar(ops[0], ref) && OperandInt(ops[1], v)) out.push_back({ ref, v }); }
        if (op >= 0x00BA && op <= 0x00BD) out.push_back({ 0xFFFFFFFFu, 0x7FFFFFFF }); // CR24: a text print on the path (the fail text of the CR21 / 11 guard shape: `IS_CHAR_DEAD -> print text, flag = 1`)
        if (op == 0x0002) { int32_t off; if (cnt < 1 || !OperandInt(ops[0], off) || hops >= 8) return; FlagSets(s, Resolve(s, off), hops + 1, out, seen); return; }
        p = e;
    }
}
bool RetFlagProgress(const CRunningScript* s, const Rec& r) { // CR44 (66 MIKE TORENO: `IS_CHAR_DEAD @52 -> REMOVE_BLIP, @94 = 1, RETURN`; `@94 == 1` is a stage-progress test, its true branch no fail): a guard that comes only from a RETURN is none when the branch sets a flag and a `flag == value` test of the mission goes on
    if (!r.fall || g_flagConds.empty() || std::getenv("NOTSA_MDRIVER_NORETFLAG")) return false;
    std::vector<std::pair<uint32_t, int32_t>> sets; std::set<const uint8_t*> seen; FlagSets(s, r.fall, 0, sets, seen);
    if (!std::getenv("NOTSA_MDRIVER_NORETPRINT") && std::any_of(sets.begin(), sets.end(), [](auto& q) { return q.first == 0xFFFFFFFFu; })) return false; // CR47 (72 AIR RAID: `IS_CHAR_DEAD @55 (Zero) -> @148 = 0, @150 = 1, PRINT_NOW ZER1_25, RETURN`; the caller's `@150 == 1` goes to M_FAIL, but another `@150 == 1` test is no failure and made this a progress flag; the kill answer killed the ally): a fail text on the branch (the CR24 print shape) is never a progress flag
    for (auto& [ref, val] : sets) { if (ref == 0xFFFFFFFFu) continue; for (auto& [pp, f] : g_flagConds) { if (!f->vref[0] || f->vref[0] != ref || !f->hasBranch || (f->raw & 0x8000)) continue; const auto v = ArgInt(*f, 1); if (!v || (int32_t)*v != val) continue; std::set<const uint8_t*> sn; if (!WalkFail(s, f->fall, 0, false, sn)) return true; } }
    return false;
}
bool RetCounterProgress(const CRunningScript* s, const Rec& r) { // CR49 (92 SAINT MARK'S BISTRO: `IS_CHAR_DEAD @93 -> ADD_VAL_TO_INT_LVAR @152 1, @94 = 1, RETURN`, the main loop is `WHILE @152 != 19`): a guard that comes only from a RETURN is none when the branch counts (ADD_VAL_TO_INT_*) and prints nothing
    if (!r.fall || std::getenv("NOTSA_MDRIVER_NORETCOUNT")) return false;
    bool add = false;
    for (const uint8_t* p = r.fall; InSpace(p);) {
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (op == 0x0051) break; if (op == 0x0001 || op == 0x0050 || op == 0x00D6 || op == 0x004D) break;
        if (op >= 0x00BA && op <= 0x00BF) return false; // a text on the branch: a fail print
        const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return false;
        if ((op == 0x0008 || op == 0x000A) && cnt >= 2) { int32_t v; if (OperandInt(ops[1], v) && v > 0) add = true; }
        if (op == 0x0002) { if (cnt < 1 || !ops[0]) return false; int32_t off; if (!OperandInt(ops[0], off)) return false; p = Resolve(s, off); continue; }
        p = e;
    }
    return add;
}
bool OneShotWarning(const CRunningScript* s, const Rec& r) { // CR48 (79 N.O.E.: `IF IS_CHAR_IN_AREA_2D(Area 51) AND IN_ANY_PLANE AND NOT message displayed -> PRINT_NOW AREA51, @180 = 1`; the flag is only ever tested `== 0`): the true branch prints a text and sets flags that no `flag == value` test reads = a one-shot warning, no stage step
    if (!r.hasBranch || !r.fall) return false;
    std::vector<std::pair<uint32_t, int32_t>> sets; // the straight-line fall-through of the true branch through the following conditions (`IN_ANY_PLANE OR HELI`, `NOT IS_MESSAGE_BEING_DISPLAYED`) up to the first wait / goto / call / return
    for (const uint8_t* p = r.fall; InSpace(p) && sets.size() < 8;) {
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (op == 0x0001 || op == 0x0002 || op == 0x0050 || op == 0x0051) break;
        const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) break;
        if ((op == 0x0004 || op == 0x0006) && cnt >= 2) { uint32_t ref; int32_t v; if (OperandVar(ops[0], ref) && OperandInt(ops[1], v)) sets.push_back({ ref, v }); }
        if (op >= 0x00BA && op <= 0x00BD) sets.push_back({ 0xFFFFFFFFu, 0x7FFFFFFF });
        p = e; }
    if (std::none_of(sets.begin(), sets.end(), [](auto& q) { return q.first == 0xFFFFFFFFu; })) return false;
    bool any = false;
    for (auto& [ref, val] : sets) { if (ref == 0xFFFFFFFFu) continue; if (val == 0) return false; bool guard = false;
        for (auto& [pp, f] : g_flagConds) { if (f->vref[0] != ref) continue; const auto v = ArgInt(*f, 1); if (!v) continue;
            if ((int32_t)*v == 0 && f->p < r.p && r.p - f->p <= 0x60) guard = true; // the `flag == 0` guard right before the area test
            if ((int32_t)*v == val && f->p >= r.fall && f->p - r.fall < 0x400) return false; } // the flag is consumed right after: a stage step
        if (!guard) return false; any = true; }
    return any;
}
bool FlagGuard(const CRunningScript* s, const Rec& r, bool noPrint = false) {
    if (!r.fall || g_flagConds.empty() || std::getenv("NOTSA_MDRIVER_NOFLAGGUARD")) return false;
    std::vector<std::pair<uint32_t, int32_t>> sets; std::set<const uint8_t*> seen; FlagSets(s, r.fall, 0, sets, seen);
    if (!noPrint && !std::getenv("NOTSA_MDRIVER_FLAGNOPRINT") && std::none_of(sets.begin(), sets.end(), [](auto& q) { return q.first == 0xFFFFFFFFu; })) return false; /* CR44: the no-print variant (65 T-Bone) broke 52 / 23: opt-in via NOTSA_MDRIVER_FLAGNOPRINT */ // CR24: the flag guard is the `fail text + flag` shape: without a print on the true branch the flag is an ordinary state variable (14: `@43 = 1` after the dealers die)
    for (auto& [ref, val] : sets) for (auto& [pp, f] : g_flagConds) {
        if (!f->vref[0] || f->vref[0] != ref || !f->hasBranch) continue;
        const auto v = ArgInt(*f, 1); if (!v || (int32_t)*v != val) continue;
        std::set<const uint8_t*> sn; if (WalkFail(s, f->fall, 0, false, sn)) return true;
    }
    if (!std::getenv("NOTSA_MDRIVER_NOLOOPFLAG")) // CR27 (26 ROBBING UNCLE SAM: `IS_CHAR_DEAD $528 -> @37 = 1, print`; the main loop is `WHILE @37 == 0`, its exit reaches the fail): the flag test is `var == W` with W != the value set, and its FALSE branch (the GOTO_IF_FALSE after it) reaches the M_FAIL print
        for (auto& [ref, val] : sets) for (auto& [pp, f] : g_flagConds) {
            if (!f->vref[0] || f->vref[0] != ref || ref == 0xFFFFFFFFu) continue;
            const auto v = ArgInt(*f, 1); if (!v || (int32_t)*v == val) continue;
            const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(f->p, ops, cnt); if (!e || !InSpace(e) || (*(const uint16_t*)e & 0x7FFF) != 0x004D || e[2] != 1) continue;
            const uint8_t* tgt = Resolve(s, *(const int32_t*)(e + 3)); if (!InSpace(tgt)) continue;
            std::set<const uint8_t*> sn; if (WalkFail(s, tgt, 0, false, sn)) return true;
        }
    return false;
}
enum class Verdict { Wait, GoesOn, Guard, Unknown };
Verdict VerdictOf(const CRunningScript* s, const Rec& r) {
    if (!r.hasBranch) return Verdict::Unknown;
    if (r.jumped && r.target < r.p) return Verdict::Wait;
    std::set<const uint8_t*> seen; if (r.orSite) g_walkThrough = true; const bool wfv = WalkFail(s, r.fall, 0, false, seen); g_walkThrough = false; if (wfv) return Verdict::Guard;
    return FlagGuard(s, r) ? Verdict::Guard : Verdict::GoesOn; // CR22
}

// CR22: static scan of the mission's code at its start: every singleton-group plain IS_CHAR_DEAD site and `var == number` flag test (the dynamic window only knows the conditions already run: 11 BIG SMOKE shoots Ryder/Smoke at f6621, long before the
// IS_CHAR_DEAD(@65) guard at ip 4cc6 ran). Linear decode of the code from the base ip (stops at the first undecodable command); the handle operand is re-read from the variable when a site is used.
void StaticScan(const CRunningScript* s) {
    if (std::getenv("NOTSA_MDRIVER_NOSTATICSCAN")) return;
    const uint8_t* p = s->m_BaseIP; RecP pend; bool single = false; int n = 0, nd = 0, nf = 0;
    std::vector<RecP> orPend; bool inOr = false; // CR49 (88 DON PEYOTE: `IS_CHAR_DEAD girl1 OR IS_CHAR_DEAD girl2` -> fail print): the plain IS_CHAR_DEAD members of an OR group are guard sites too (any member true takes the fall-through)
    while (InSpace(p) && n++ < 60000) {
        const uint16_t w = *(const uint16_t*)p; const int op = w & 0x7FFF; if (op > 0x0AFF) break;
        const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) break;
        if (Arity(op) < 0 && InSpace(e) && *e == 0) e++; // variadic: the END operand
        if (op == 0x004D) { if (inOr && !std::getenv("NOTSA_MDRIVER_NOORGUARD")) { for (auto& q : orPend) { q->hasBranch = true; q->jumped = false; q->fall = p + 7; q->target = p + 7; q->orSite = true; g_deadConds[q->p] = q; nd++; } } orPend.clear(); inOr = false; if (pend) { pend->hasBranch = true; pend->jumped = false; pend->fall = p + 7; pend->target = p + 7; if ((pend->raw & 0x7FFF) == 0x0118 || (pend->raw & 0x7FFF) == 0x0119) { g_deadConds[pend->p] = pend; nd++; } else { g_flagConds[pend->p] = pend; nf++; } } pend.reset(); single = false; p = e; continue; }
        if ((op == 0x08F9 || op == 0x05AD) && cnt >= 2 && ops[1] && InSpace(ops[1]) && *ops[1] == 0x0E && ops[1][1] > 0 && ops[1][1] <= 8) g_enexNames.insert(std::string((const char*)ops[1] + 2, strnlen((const char*)ops[1] + 2, ops[1][1]))); // CR27
        if (op == 0x0652 && cnt >= 1 && ops[0]) { int32_t sv = 0; if (OperandInt(ops[0], sv) && sv == STAT_LUNG_CAPACITY && !std::getenv("NOTSA_MDRIVER_NOLUNG")) CStats::SetStatValue(STAT_LUNG_CAPACITY, 100.f); } // CR39 (56 AMPHIBIOUS ASSAULT: the mission ends at once with \"you need more practice underwater\" unless GET_INT_STAT 225 > 50): the player has trained his lungs
        if (op == 0x00D6) { int32_t v; single = cnt >= 1 && OperandInt(ops[0], v) && v == 0; inOr = cnt >= 1 && OperandInt(ops[0], v) && v >= 21 && v <= 28; orPend.clear(); pend.reset(); p = e; continue; }
        if (inOr && !(w & 0x8000) && (op == 0x0118 || (op == 0x0119 && !std::getenv("NOTSA_MDRIVER_NOCARGUARD")))) { auto r = std::make_shared<Rec>(); r->p = p; r->raw = w; r->cond = -1; r->waitSeq = 0; r->args = DecodeArgs(s, p, 1); for (int k = 0; k < 2 && k < cnt; k++) { uint32_t ref; if (OperandVar(ops[k], ref)) r->vref[k] = ref; } orPend.push_back(r); p = e; continue; }
        if (single && !(w & 0x8000) && (op == 0x0118 || op == 0x0038 || op == 0x0039 || (op == 0x0119 && !std::getenv("NOTSA_MDRIVER_NOCARGUARD")))) {
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
bool HostileScriptPed(CPed* p) { return std::getenv("NOTSA_MDRIVER_HURTANY") || HatesSide((int)p->m_nPedType) || (int)p->m_nPedType == (int)PED_TYPE_MISSION1; } // CR38: a ped the hurt answer may hurt: hates CJ's side, or a MISSION1 ped (the script's own enemy: 37's biker, type 24 model 284); never a neutral civilian (22: Denise)
bool ProtectedPed(CPed* p) { auto* c = FindPlayerPed(); return (int)p->m_nPedType <= 3 || LikesSide((int)p->m_nPedType) || (c && c->GetPlayerGroup().GetMembership().IsMember(p)); }
std::map<const uint8_t*, RecP> g_healthConds;
std::map<uint32_t, int> g_carHealthVars; // CR44: destination variable of GET_CAR_HEALTH (0227) -> the car handle
std::map<uint32_t, int> g_objHealthVars; // CR37: destination variable of GET_OBJECT_HEALTH (071E) -> the object handle
std::map<uint32_t, int> g_healthVars; // CR35: destination variable of GET_CHAR_HEALTH -> the char handle read (a later `N > var` wait = the char is hurt below N)
std::map<uint32_t, int> g_speedVars; // CR34: destination variable of GET_CAR_SPEED -> the car handle read (a later `var > N` wait = the car goes faster than N)
struct FollowHold { int tgt = -1; CVector off; int left = 0; } g_follow; // CR47 (76 HIGHJACK: the stage wants the bike inside a 3 x 4 m box behind the moving truck for 3.5 s): CJ's vehicle keeps its place relative to a moving vehicle for `left` frames
struct AirHold { int h = -1; float z = 0.f; int left = 0; float vx = 0.f, vy = 0.f; } g_airHold; // CR49 (91 FREEFALL: the Dodo warped to the plane that flies 200 m up fell into the sea): an aircraft the driver warped into the air keeps its altitude (vertical speed zeroed, z reset) for `left` frames
struct SpeedHold { int h = -1; float mps = 0.f; int left = 0; } g_speedHold; // CR34: the car kept at that speed (m/s) along its heading for `left` more frames
std::set<int> g_fixCars; // CR34: vehicle handles the carfix answer repaired: kept repaired every frame afterwards (the player takes the car to the Pay n Spray whenever random traffic bumps it again; 33)
std::set<int> g_usedCars; // CR27: vehicle handles the driver itself entered / drove to a place (the mission car): never wrecked by a kill answer
bool ProtectedHandle(int h) { // actions.ts protectedHandle
    if (auto* p = CPools::GetPed(h)) return p == Cj() || ProtectedPed(p);
    auto* v = CPools::GetVehicle(h); if (!v) return false;
    std::vector<CPed*> occ; if (v->m_pDriver) occ.push_back(v->m_pDriver); for (auto* pp : v->m_apPassengers) if (pp) occ.push_back(pp);
    for (auto* o : occ) { if (o == Cj() || ProtectedPed(o) || (o->m_fHealth > 0.f && !HatesSide((int)o->m_nPedType) && !((int)o->m_nPedType == (int)PED_TYPE_MISSION1 && !std::getenv("NOTSA_MDRIVER_NOMISSION1FOE")))) return true; } // CR39 (47: the chopper pilot, type 24 model 283, is the mission's own enemy)
    return false;
}
bool IsPlayerGuard(const Rec& r) {
    const int op = r.raw & 0x7FFF; if (op != 0x0118 && op != 0x0119) return false;
    auto* ped = Cj(); const auto h = ArgInt(r, 0); if (!ped || !h) return false;
    return op == 0x0118 ? CPools::GetPed((int)*h) == ped : ped->m_pVehicle && CPools::GetVehicle((int)*h) == ped->m_pVehicle;
}
// CR22: the handles the script fails the mission for when they die (a plain IS_CHAR_DEAD site, singleton group, whose true branch reaches the fail text directly or through a fail flag): never killed by the driver, whatever the site that asks for the kill
bool CreatesCharBeforeReturn(const CRunningScript* s, const uint8_t* p) { // CR43: the straight line from p (gotos not followed) holds a CREATE_CHAR / CREATE_CHAR_INSIDE_CAR / CREATE_CHAR_AS_PASSENGER before the RETURN: a respawn subroutine
    for (int n = 0; n < 40 && InSpace(p); n++) { const int op = *(const uint16_t*)p & 0x7FFF; if (op == 0x0051) return false; if (op == 0x009A || op == 0x0129 || op == 0x01C8) return true; const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return false; p = e; }
    return false;
}
static std::set<int> g_guardedPed, g_guardedCar; // CR49 (92 SAINT MARK'S BISTRO: the IS_CAR_DEAD guard of the old stage car 8194 protected the ped with the same number 8194 from the kill answer): ped and vehicle handles are numbers of different pools
const std::set<int>& Guarded() {
    static std::set<int> out; static int lastFr = -1; static std::map<const uint8_t*, std::pair<size_t, bool>> cache;
    const int fr = (int)CTimer::GetFrameCounter(); if (fr == lastFr) return out; lastFr = fr; out.clear(); g_guardedPed.clear(); g_guardedCar.clear();
    if (std::getenv("NOTSA_MDRIVER_NOGUARDPROTECT") || !g_thread) return out;
    for (auto& [site, r] : g_deadConds) {
        if ((r->raw & 0x8000) || ((r->raw & 0x7FFF) != 0x0118 && (r->raw & 0x7FFF) != 0x0119)) continue; // CR27: IS_CAR_DEAD sites too (26: the mission's truck)
        auto& ce = cache[site]; const size_t gen = g_flagConds.size() + g_deadConds.size();
        if (ce.first != gen + 1) { g_walkRet = false; { const bool vg = VerdictOf(g_thread, *r) == Verdict::Guard; ce = { gen + 1, vg && !(g_walkRet && (((r->raw & 0x7FFF) == 0x0119 && !std::getenv("NOTSA_MDRIVER_NOSUBRET")) || ((r->raw & 0x7FFF) == 0x0118 && !std::getenv("NOTSA_MDRIVER_NOSUBRETCHAR") && (CreatesCharBeforeReturn(g_thread, r->fall) || RetFlagProgress(g_thread, *r) || RetCounterProgress(g_thread, *r))))) /* CR43 (64 PIER 69: `IS_CHAR_DEAD @48` of the respawn subroutine: CREATE_CHAR, RETURN: no failure; the roof gunman was never killed) */ }; } if (std::getenv("NOTSA_MDRIVER_PLOG") && ce.second) notsa::standalone::Fixups::Log("[mdplog] guard site ip=%x flagconds=%zu h=%d", (unsigned)(site - g_thread->m_BaseIP), g_flagConds.size(), [&] { const auto da_ = DecodeArgs(g_thread, site, 1); return da_.empty() ? -1 : (int)da_[0].bits; }()); }
        if (ce.second && std::getenv("NOTSA_MDRIVER_GUARDLOG")) { std::set<const uint8_t*> sn; const bool wf = WalkFail(g_thread, r->fall, 0, false, sn); notsa::standalone::Fixups::Log("[guardlog] site %x guard: walkfail=%d flagguard=%d waitSeq=%d/%d", (unsigned)(site - g_thread->m_BaseIP), (int)wf, (int)FlagGuard(g_thread, *r), r->waitSeq, g_waitSeq); }
        if (!ce.second) continue; if (r->waitSeq > 0 && r->waitSeq < g_waitSeq - 600 && !std::getenv("NOTSA_MDRIVER_NOSTALEGUARD")) continue; /* CR42 (52 SNAIL TRAIL: IS_CHAR_DEAD @78 is a fail site of the escort stage (the reporter must live) and the kill target of the sniper stage; the old stage's site has not run for 600+ waits) */ const auto da = DecodeArgs(g_thread, site, 1); if (!da.empty()) { out.insert((int)da[0].bits); ((r->raw & 0x7FFF) == 0x0119 ? g_guardedCar : g_guardedPed).insert((int)da[0].bits); if (std::getenv("NOTSA_MDRIVER_PLOG")) { static std::set<std::pair<const uint8_t*, int>> logged; if (logged.insert({ site, (int)da[0].bits }).second) notsa::standalone::Fixups::Log("[mdplog] guarded handle %d by site ip=%x", (int)da[0].bits, (unsigned)(site - g_thread->m_BaseIP)); } }
    }
    return out;
}
const std::set<int>& GuardedOf(bool car) { Guarded(); return (car && !std::getenv("NOTSA_MDRIVER_MIXGUARD")) ? g_guardedCar : (!car && !std::getenv("NOTSA_MDRIVER_MIXGUARD")) ? g_guardedPed : Guarded(); }
bool KillRefusal(const CRunningScript* s, const Rec& r) { // true = the driver must not kill for this condition
    const int op = r.raw & 0x7FFF; if ((op != 0x0118 && op != 0x0119) || (r.raw & 0x8000)) return false;
    const auto h = ArgInt(r, 0); if (!h) return false;
    if (ProtectedHandle((int)*h) || GuardedOf(op == 0x0119).count((int)*h)) return true;
    if (op == 0x0119 && g_usedCars.count((int)*h) && !std::getenv("NOTSA_MDRIVER_NOUSEDCAR")) return true; // CR27 (26): a PLAIN false IS_CAR_DEAD of a car the driver entered / moved is no kill order (the escalation of a wait group still may wreck it: 89)
    const auto v = VerdictOf(s, r); return !(v == Verdict::Wait || v == Verdict::GoesOn);
}
bool NotInCarGuard(const CRunningScript* s, const Rec& r) {
    const int op = r.raw & 0x7FFF; if (!(r.raw & 0x8000) || (op != 0x00DB && op != 0x00DF)) return false;
    if (r.waitAfter && !r.jumped && !std::getenv("NOTSA_MDRIVER_NOWAITNOTGUARD")) return false; // CR22: the true branch of the NOT-in-car test is the WAIT itself (`WHILE NOT IS_CHAR_IN_CAR`, 13): the script waits for the car, it is no guard
    if (r.fall && InSpace(r.fall + 8) && !std::getenv("NOTSA_MDRIVER_NOAPPROACH")) { const uint8_t* q = r.fall; if ((*(const uint16_t*)q & 0x7FFF) == 0x00D6) q += 4; const int nop = *(const uint16_t*)q & 0x7FFF; if (!(*(const uint16_t*)q & 0x8000) && nop >= 0x0202 && nop <= 0x0205) return false; } // CR47 (70 PUNCTURE WOUNDS: `IF NOT IS_CHAR_IN_ANY_CAR cj THEN LOCATE_CHAR_ANY_MEANS_CAR_2D cj, car, 5`: the stage after the strip wants CJ out of his car and next to the chased one; the test counted as a never-exit guard): a not-in-car test whose true branch goes straight into a locate of CJ at a car is the on-foot approach
    const auto v = VerdictOf(s, r); return v == Verdict::Guard || v == Verdict::GoesOn;
}
bool DropsObjectBehind(const Rec& r) { // CR47 (70 PUNCTURE WOUNDS: `IF IS_BUTTON_PRESSED pad 17 -> GET_OFFSET_FROM_CAR_IN_WORLD_COORDS(CJ's car, 0, -3, -0.5), CREATE_OBJECT (the spike strip)`): a plain button test whose true branch puts an object at an offset of a car
    if ((r.raw & 0x7FFF) != 0x00E1 || (r.raw & 0x8000) || !r.hasBranch || !r.fall || std::getenv("NOTSA_MDRIVER_NOSTRIPAHEAD")) return false;
    bool off = false;
    for (const uint8_t* p = r.fall; InSpace(p);) { const int op = *(const uint16_t*)p & 0x7FFF; if (op == 0x0001 || op == 0x0051 || op == 0x004D || op == 0x00D6 || op == 0x0002) return false; if (op == 0x0407) off = true; if (op == 0x0107) return off; const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return false; p = e; }
    return false;
}
bool ButtonGuard(const Rec& r) { // CR24 (10 BEEFY BARON): a plain IS_BUTTON_PRESSED whose true branch ends in the mission's failure (directly or through a fail flag) is a guard: the driver never presses that button (here: the RC plane's self-destruct after 4 s of hovering; the mission is passed by surviving the timer)
    if ((r.raw & 0x7FFF) != 0x00E1 || (r.raw & 0x8000) || !g_thread || std::getenv("NOTSA_MDRIVER_NOBUTTONGUARD")) return false;
    if (!r.hasBranch || !r.fall) return false;
    bool skipSeen = false; int hops = 0;
    for (const uint8_t* p = r.fall; InSpace(p);) { // the true branch only: the straight-line commands up to the first wait / branch / call / return; a print of M_FAIL on it = the press fails the mission (10: self-destruct). VerdictOf walked through the following conditions and flagged 46's passing press too
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (op == 0x040D || op == 0x0729) skipSeen = true; // CR36 (45 KING IN EXILE): CLEAR_MISSION_AUDIO / TASK_USE_MOBILE_PHONE (put the phone away) on the way
        if (op == 0x0002 && hops < 4 && !std::getenv("NOTSA_MDRIVER_NOSKIPBUTTON")) { // CR36: follow an unconditional GOTO of the true branch (mission scripts: a negative offset is local)
            const uint8_t* gq = p + 2; bool gmf = false, gI = false; int32_t gv = 0; const uint8_t* gn = SkipOperand(gq, gmf, gI, gv);
            if (!gn || !gI) return false; const uint8_t* nx = Resolve(g_thread, gv); if (!nx || !InSpace(nx)) return false; hops++; p = nx; continue; }
        if (op == 0x0050 && !(hops > 0 && skipSeen) && !std::getenv("NOTSA_MDRIVER_NOEXITBUTTON")) { // CR46 (71 BACK TO SCHOOL: `IF IS_BUTTON_PRESSED pad 15` = leave the school: GOSUB hud, DO_FADE t,0, ... cleanup): a GOSUB of the true branch is stepped over; a fade-out on the way = an exit button
            const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0; const uint8_t* nx = SkipOperand(q, mf, isI, iv); if (!nx) return false; p = nx; continue; }
        if (op == 0x016A && !std::getenv("NOTSA_MDRIVER_NOEXITBUTTON")) { const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0; const uint8_t* n1 = SkipOperand(q, mf, isI, iv); if (!n1) return false; bool mf2 = false, i2 = false; int32_t v2 = 1; const uint8_t* n2 = SkipOperand(n1, mf2, i2, v2); if (!n2) return false; if (*n1 == 4) { i2 = true; v2 = *(const int8_t*)(n1 + 1); } else if (*n1 == 5) { i2 = true; v2 = *(const int16_t*)(n1 + 1); } if (i2 && v2 == 0) return true; p = n2; continue; }
        if (op == 0x0001 || op == 0x0051 || op == 0x004D || op == 0x0050 || op == 0x00D6 || op == 0x0002) return hops > 0 && skipSeen && !std::getenv("NOTSA_MDRIVER_NOSKIPBUTTON"); // CR36 (45 KING IN EXILE: `IF IS_BUTTON_PRESSED pad 15` of the phone call stage: the true branch silences the mission audio, puts the phone away and jumps to the cleanup = the 'hang up' button; the call ends by itself and passes, the press ended the mission thread without a pass): a skip / hang-up button is no answer
        const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0; const int want = Arity(op); int count = 0;
        for (;;) { if (want >= 0 && count >= want) break; if (!InSpace(q)) return false; const uint8_t* nx = SkipOperand(q, mf, isI, iv); if (!nx) break; if (mf && op >= 0x00BA && op <= 0x00BD) return true; q = nx; count++; }
        p = q;
    }
    return false;
}
bool LocateGuard(const CRunningScript* s, const Rec& r) { // CR28 (24 HOME INVASION: `IF OR(HAS_CHAR_BEEN_DAMAGED_BY_CHAR owner cj, LOCATE_CHAR_ANY_MEANS_CHAR_3D cj owner 1 m)` wakes the sleeping owner, the true branch is the alarm and the mission's failure): a plain locate / area condition a warp row would answer, whose TRUE branch reaches the fail text, is never answered
    if ((r.raw & 0x8000) || !s || std::getenv("NOTSA_MDRIVER_NOLOCATEGUARD")) return false;
    bool warp = false; for (auto& row : g_rows) if (row.match != 2 && row.op == (r.raw & 0x7FFF) && (row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint" || row.action == "warpto_handle")) { warp = true; break; }
    if (!warp) return false;
    { const int lop = r.raw & 0x7FFF; if (lop == 0x01AD && !std::getenv("NOTSA_MDRIVER_NOCARLOCFLAG") && FlagGuard(s, r, true)) return true; } // CR46 (76 HIGHJACK: `LOCATE_CAR_2D truck, -1914.6 -1187.7, 5` while the stage is below 3 sets the stage to 6 = the truck got away, the ladder fails the mission; the vehicle-warp answer put the truck on that spot four times): a car locate whose true branch sets a flag that leads to the failure is never answered (without the print the CR24 flag guard requires)
    if (r.fall && !std::getenv("NOTSA_MDRIVER_NOTRAPGUARD")) for (auto& o : g_recs) if (o->fall == r.fall && o->waitSeq == r.waitSeq && o->raw == 0x051A) return true; // the same OR group also holds HAS_CHAR_BEEN_DAMAGED_BY_CHAR: a wake-up trap (the owner wakes when he is hit OR CJ comes within 1 m), its true branch is an alarm state machine the fail walk does not follow
    return false;
}
bool TrackerOnly(const Rec& r) { // CR46 (69 CUSTOMS FAST TRACK: `@39 == 4 AND IS_CHAR_IN_ANY_CAR cj AND NOT IS_CAR_DEAD @82` of a shared subroutine only stores the car CJ sits in; the nearest_car answer teleported CJ out of the crane cab): a plain false CJ-in-car test whose TRUE branch is nothing but STORE_CAR_CHAR_IS_IN / variable assignments up to the next jump is a passive tracker, never answered
    const int rop = r.raw & 0x7FFF; if ((r.raw & 0x8000) || (rop != 0x00DB && rop != 0x00DF) || !g_thread || !r.hasBranch || !r.fall || !std::getenv("NOTSA_MDRIVER_TRACKER")) return false; // opt-in: on by default it turned 56's nearest_car answer into a wait (56 regressed); 69 does not pass either way
    int n = 0;
    for (const uint8_t* p = r.fall; InSpace(p);) {
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (op == 0x0002 || op == 0x004D || op == 0x00D6 || op == 0x0001 || op == 0x0050 || op == 0x0051 || op == 0x004C) return n > 0;
        if (!(op == 0x03C0 || op == 0x0811 || (op >= 0x0004 && op <= 0x0007) || op == 0x0084 || op == 0x0085 || op == 0x0086 || op == 0x0087)) return false;
        const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0; const int want = Arity(op); int count = 0;
        for (;;) { if (want >= 0 && count >= want) break; if (!InSpace(q)) return false; const uint8_t* nx = SkipOperand(q, mf, isI, iv); if (!nx) return false; q = nx; count++; }
        p = q; n++;
    }
    return false;
}
bool GovernorOnly(const Rec& r) { // CR46 (76 HIGHJACK: `LOCATE_CHAR_ANY_MEANS_CAR_2D cj, truck, 20` of the cruise-speed subroutine: the true branch only calls SET_CAR_CRUISE_SPEED): a plain car locate that only tunes a speed is no stage blocker
    const int rop = r.raw & 0x7FFF; if ((r.raw & 0x8000) || rop < 0x0202 || rop > 0x0207 || !g_thread || !r.hasBranch || !r.fall || std::getenv("NOTSA_MDRIVER_NOGOVERNOR")) return false;
    int n = 0;
    for (const uint8_t* p = r.fall; InSpace(p);) {
        const int op = *(const uint16_t*)p & 0x7FFF;
        if (op == 0x0002 || op == 0x004D || op == 0x00D6 || op == 0x0001 || op == 0x0050 || op == 0x0051 || op == 0x004C) return n > 0;
        if (!(op == 0x00AD || op == 0x00AE || (op >= 0x0004 && op <= 0x0007) || op == 0x0084 || op == 0x0085 || op == 0x0086 || op == 0x0087)) return false;
        const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0; const int want = Arity(op); int count = 0;
        for (;;) { if (want >= 0 && count >= want) break; if (!InSpace(q)) return false; const uint8_t* nx = SkipOperand(q, mf, isI, iv); if (!nx) return false; q = nx; count++; }
        p = q; n++;
    }
    return false;
}
bool TyreTrap(const Rec& r) { // CR47 (70 PUNCTURE WOUNDS: `IS_CHAR_IN_ANGLED_AREA_IN_CAR_2D cj, strip -> BURST_CAR_TYRE` of CJ's own car): a plain area test about CJ himself whose true branch (the next 30 commands, no wait / return / goto) bursts a tyre is a trap, never answered
    const int o = r.raw & 0x7FFF; if ((r.raw & 0x8000) || !r.hasBranch || !r.fall || !(o >= 0x05F6 && o <= 0x0601) || !g_thread || std::getenv("NOTSA_MDRIVER_NOTYRETRAP")) return false;
    const auto h = ArgInt(r, 0); if (!h || CPools::GetPed((int)*h) != Cj()) return false;
    int n = 0; for (const uint8_t* p = r.fall; InSpace(p) && n < 30; n++) { const int op = *(const uint16_t*)p & 0x7FFF; if (op == 0x0001 || op == 0x0051 || op == 0x0002) return false; if (op == 0x04FE) return true; const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return false; p = e; }
    return false;
}
bool MovingMissionCar() { auto* me = Cj(); for (auto& vv : GetVehiclePool()->GetAllValid()) if (vv.m_fHealth > 0.f && vv.m_pDriver && vv.m_pDriver != me && vv.IsMissionVehicle() && vv.m_vecMoveSpeed.Magnitude() * 50.f > 3.f) return true; return false; }
bool AnyGuard(const Rec& r) { return IsPlayerGuard(r) || KillRefusal(g_thread, r) || NotInCarGuard(g_thread, r) || ButtonGuard(r) || LocateGuard(g_thread, r) || TrackerOnly(r) || TyreTrap(r) || (DropsObjectBehind(r) && !MovingMissionCar()); } // CR47: the strip button is no answer once the chased car stands still (70: the stage went on to the on-foot approach)

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
    const bool fl = row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint" || row.action == "projectile";
    std::vector<RArg> out;
    for (auto& a : row.args) {
        RArg r;
        if (a.size() > 1 && a[0] == '$' && std::all_of(a.begin() + 1, a.end(), [](char c) { return std::isdigit((unsigned char)c); })) {
            const size_t ai = (size_t)std::atoi(a.c_str() + 1); const auto v = (fl || (row.action == "explode" && ai >= 1) || (row.action == "smashobj" && ai <= 3)) ? ArgFloat(rec, ai) : ArgInt(rec, ai); // CR36: explode = (type int, 6 floats)
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
std::string LocatedOther(const Rec& r);
bool g_bigCall = false; // CR45: Decide() runs over the big window
Dec Decide(const std::vector<RecP>& loop, int attempt, bool wide = false) {
    Dec d;
    // CR19: the wait group. The last condition group of the pass fell straight into a WAIT and is an OR (00D6 21..27: `WHILE OR(NOT a, NOT b, ...)`): the thread waits until EVERY member is false. The NOT-form members that are
    // false are already satisfied (a is true), so they are no candidates (NOT IS_CHAR_SITTING_IN_CAR false = CJ sits: the Enter key got him OUT of the car, 12 13 15 16 58); the NOT-form members that are TRUE
    // are the blockers (a is false): the plain row of their opcode (entercar / vehwarp / pedwarp) answers them, after the false conditions found no row.
    std::set<const uint8_t*> satisfied; std::vector<RecP> blockers;
    if (!loop.empty() && !std::getenv("NOTSA_MDRIVER_NOORGROUP")) {
        auto lrp = loop.back(); // CR25 (14, stage `WHILE OR(NOT CJ at house, NOT Sweet at house)` with an IF ladder as the loop body): the wait group is not the LAST condition group of the pass: the last group of the pass that fell into the WAIT is the loop's own wait condition
        if (!(lrp->hasBranch && !lrp->jumped && lrp->waitAfter) && !std::getenv("NOTSA_MDRIVER_NOWAITGRP2")) for (size_t k = loop.size(); k-- > 0;) if (loop[k]->hasBranch && !loop[k]->jumped && loop[k]->waitAfter) { lrp = loop[k]; break; }
        const auto& lr = lrp;
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
    if (std::getenv("NOTSA_MDRIVER_BIKELOG") && !wide) for (auto& r : loop) if (r->cond == 0 && (r->raw & 0x7FFF) == 0x0118 && !(r->raw & 0x8000)) { const auto h = ArgInt(*r, 0); const auto v = VerdictOf(g_thread, *r); auto* pe = h ? CPools::GetPed((int)*h) : nullptr; notsa::standalone::Fixups::Log("[kilrefuse] ip %x ped %d type %d model %d hp %.0f protected %d guarded %d verdict %d", (unsigned)(r->p - g_thread->m_BaseIP), h ? (int)*h : -1, pe ? (int)pe->m_nPedType : -1, pe ? (int)pe->m_nModelIndex : -1, pe ? pe->m_fHealth : -1.f, h ? (int)ProtectedHandle((int)*h) : -1, h ? (int)Guarded().count((int)*h) : -1, (int)v); }
    // CR35 (35 RACE TOURNAMENT): a locate whose target coordinate is a sentinel (|z| or |x|,|y| >= 20000: the script's `99999.0` = no more way points of the racer) can never be true: answering it warped the racer to z 99999 over and over
    const auto sentinelLocate = [](const Rec& r) { const int op = r.raw & 0x7FFF; if (!((op >= 0x00EC && op <= 0x00F1) || (op >= 0x00FE && op <= 0x0103) || (op >= 0x01AD && op <= 0x01B0)) || std::getenv("NOTSA_MDRIVER_NOSENTINEL")) return false; for (size_t i = 1; i <= 3; i++) if (const auto f = ArgFloat(r, i); f && std::fabs(*f) >= 20000.0) return true; return false; };
    // CR35 (29 WRONG SIDE OF THE TRACKS): a locate about a TRAIN is never answered (it runs to the place on its tracks: CR31 trainwait), so it must not shadow the other false conditions of the pass (the four Vagos' IS_CHAR_DEAD tests were never reached: three 600-frame stalls were spent on the train)
    const auto trainLocate = [](const Rec& r) { if (std::getenv("NOTSA_MDRIVER_NOTRAINWAIT") || std::getenv("NOTSA_MDRIVER_NOTRAINSKIP")) return false; const auto o = LocatedOther(r); if (o.compare(0, 2, "1:")) return false; auto* tv = CPools::GetVehicle(std::atoi(o.c_str() + 2)); return tv && tv->IsTrain(); };
    // CR36 (41 LOCAL LIQUOR STORE: the stage chases three robbers in cars and ends with "You lost the robbers" when the distance variables (`300 > dist`, GET_DISTANCE_BETWEEN_COORDS of CJ and a robber, TRUE in the pass) pass 300 m; the answer to an unrelated accuracy locate (LOCATE_CHAR_ANY_MEANS_2D at a far place) put CJ 1100 m away = lost): a place locate whose centre lies farther than 0.9 N from the ped that a TRUE `N > dist` test of the pass measures (N 100..1000, the ped found as in the distance fetch: at that distance from CJ) is not a candidate
    const auto leashActive = []() { if (std::getenv("NOTSA_MDRIVER_NOLEASHLOCATE")) return false; auto* me = Cj(); if (!me) return false; // CR36: a TRUE `N > dist` test (N 100..1000) of the pass measures the distance from CJ to a ped: CJ must not be moved far away (the area entrance walk-in of 41 lost the robbers the same way)
        for (auto& g : g_recsBig) { if (g->waitSeq < g_waitSeq - 3 || g->raw != 0x0023 || g->cond != 1 || g->args.size() < 2) continue; const float lim = AF(g->args[0]), dv = AF(g->args[1]); if (!(lim >= 100.f && lim <= 1000.f) || !(dv > 0.3f * lim && dv < lim)) continue;
            for (auto& pd : GetPedPool()->GetAllValid()) { if (&pd == me || pd.m_fHealth <= 0.f) continue; if (std::fabs((pd.GetPosition() - me->GetPosition()).Magnitude() - dv) < 1.5f) return true; } }
        return false; };
    const auto leashLocate = [](const Rec& r) { const int op = r.raw & 0x7FFF; if (!((op >= 0x00EC && op <= 0x00F1) || (op >= 0x00FE && op <= 0x0103)) || std::getenv("NOTSA_MDRIVER_NOLEASHLOCATE")) return false; auto* me = Cj(); if (!me) return false;
        const auto lx = ArgFloat(r, 1), ly = ArgFloat(r, 2); if (!lx || !ly) return false;
        for (auto& g : g_recsBig) { if (g->waitSeq < g_waitSeq - 3 || g->raw != 0x0023 || g->cond != 1 || g->args.size() < 2) continue; const float lim = AF(g->args[0]), dv = AF(g->args[1]); if (!(lim >= 100.f && lim <= 1000.f) || !(dv > 0.3f * lim && dv < lim)) continue;
            for (auto& pd : GetPedPool()->GetAllValid()) { if (&pd == me || pd.m_fHealth <= 0.f) continue; if (std::fabs((pd.GetPosition() - me->GetPosition()).Magnitude() - dv) >= 1.5f) continue;
                const float dx = (float)*lx - pd.GetPosition().x, dy = (float)*ly - pd.GetPosition().y; if (std::sqrt(dx * dx + dy * dy) > 0.9f * lim) { notsa::standalone::Fixups::Log("[mdriver] leashlocate: the locate at %.0f,%.0f is %.0f m from ped %d (leash %.0f): not a candidate", *lx, *ly, (double)std::sqrt(dx * dx + dy * dy), CPools::GetPedRef(&pd), (double)lim); return true; } } }
        return false; };
    const auto foreignCarLoc = [](const Rec& r) { if ((r.raw & 0xFFFF) != 0x0204 || !std::getenv("NOTSA_MDRIVER_FOREIGNCARLOC")) return false; const auto h0 = ArgInt(r, 0); auto* tc = h0 ? CPools::GetPed((int)*h0) : nullptr; return tc && tc != Cj() && tc->bInVehicle && tc->m_fHealth > 0.f; }; // CR42 (60 OUTRIDER): LOCATE_CHAR_IN_CAR_CAR_2D of the van's driver (not CJ) is not an answer for CJ: no candidate (the roadblock peds' IS_CHAR_DEAD of the widened window answers)
    const auto onFootInCarArea = [](const Rec& r) { const int o = r.raw & 0x7FFF; if ((r.raw & 0x8000) || !(o == 0x05F8 || o == 0x05FB || o == 0x05FE || o == 0x0601) || std::getenv("NOTSA_MDRIVER_NOONFOOTAREA")) return false; const auto h = ArgInt(r, 0); auto* pd = h ? CPools::GetPed((int)*h) : nullptr; return pd && pd != Cj() && !pd->bInVehicle; }; // CR47 (70 PUNCTURE WOUNDS: after the spike strip stopped the chased car its driver walks; the strip's `IS_CHAR_IN_ANGLED_AREA_IN_CAR_2D(driver)` stays false and was answered 8 times before the stage's own on-foot approach): an in-car area test of ANOTHER ped who is not in a vehicle can not be answered by a warp
    for (size_t k = loop.size(); k-- > 0;) { const auto& r = loop[k]; if (r->cond == 0 && !AnyGuard(*r) && !satisfied.count(r->p) && !sentinelLocate(*r) && !trainLocate(*r) && !leashLocate(*r) && !foreignCarLoc(*r) && !onFootInCarArea(*r)) falses.push_back(r); }
    if (falses.size() > 1) { const auto gv = std::stable_partition(falses.begin(), falses.end(), [](const RecP& r) { return !GovernorOnly(*r); }); if (gv != falses.begin()) falses.erase(gv, falses.end()); } // CR46: the speed governors of the pass are answered only when nothing else is false
    if (falses.size() > 1 && !std::getenv("NOTSA_MDRIVER_NOGUARDNEAR") && !g_auxMode) { // CR43 (57 THE DA NANG THANG: the strip area (-2375..-2379 x 1537..1556) wakes the grenadier, whose grenade lands 6.7 m from the refugee the script fails the mission for ("One of the refugees died!"); the other false area of the pass (the north zone) wakes him as well, far from the refugee): a false plain area / locate of CJ whose centre lies within 12 m of a living char the script fails the mission for (a Guarded() handle) is answered after the other false conditions
        std::vector<CVector> gp; for (const int gh : Guarded()) if (auto* gpd = CPools::GetPed(gh); gpd && gpd != Cj() && gpd->m_fHealth > 0.f) gp.push_back(gpd->GetPosition());
        if (!gp.empty()) {
            const auto nearGuard = [&](const RecP& r) { const int op = r->raw & 0x7FFF; if ((r->raw & 0x8000) || !(op == 0x00A3 || op == 0x00A4 || (op >= 0x00EC && op <= 0x00F1) || (op >= 0x00FE && op <= 0x0103))) return false; const auto h0 = ArgInt(*r, 0); if (!h0 || EntityOfHandle((int)*h0, op) != (CEntity*)Cj()) return false;
                float cx = 0, cy = 0; if (op == 0x00A3 || op == 0x00A4) { const size_t o2 = op == 0x00A3 ? 3 : 4; const auto x1 = ArgFloat(*r, 1), y1 = ArgFloat(*r, 2), x2 = ArgFloat(*r, o2), y2 = ArgFloat(*r, o2 + 1); if (!x1 || !y1 || !x2 || !y2) return false; cx = (float)(*x1 + *x2) * 0.5f; cy = (float)(*y1 + *y2) * 0.5f; } else { const auto x1 = ArgFloat(*r, 1), y1 = ArgFloat(*r, 2); if (!x1 || !y1) return false; cx = (float)*x1; cy = (float)*y1; }
                return std::any_of(gp.begin(), gp.end(), [&](const CVector& g) { return std::hypot(g.x - cx, g.y - cy) < 12.f; }); };
            std::stable_partition(falses.begin(), falses.end(), [&](const RecP& r) { return !nearGuard(r); });
        }
    }
    // CR45 (53 MOUNTAIN CLOUD BOYS: the ambush stage spawns six gunmen; the pass has six plain false IS_CHAR_DEAD tests, but the tail-most false of the pass is the trigger area of the ambush (CJ in area AND in a car), so the answer was a vehicle warp into the area and Woozie was shot in the car): with five or more plain false IS_CHAR_DEAD tests of live peds from ONE site (an array loop) left in the window they go before every other false condition of the window
    if (falses.size() > 4 && !wide && !g_bigCall && !std::getenv("NOTSA_MDRIVER_NOKILLSET1")) {
        const auto isKill = [](const RecP& r) { if ((r->raw & 0xFFFF) != 0x0118) return false; const auto h = ArgInt(*r, 0); auto* pe = h ? CPools::GetPed((int)*h) : nullptr; return pe && pe != Cj() && pe->m_fHealth > 0.f && (int)pe->GetActiveWeapon().m_Type >= 22 && (int)pe->GetActiveWeapon().m_Type <= 34 /* an armed (firearm) gunman */; };
        std::map<const uint8_t*, int> siteN; { std::set<std::pair<const uint8_t*, int>> seenSH; for (auto& c : falses) if (isKill(c) && seenSH.insert({ c->p, (int)*ArgInt(*c, 0) }).second) siteN[c->p]++; } /* distinct handles per site */ const bool loopKills = std::any_of(siteN.begin(), siteN.end(), [](auto& kv) { return kv.second >= 5; }); /* one site tested over an array of peds (a loop), not separate tests */
        if (blockers.empty() && loopKills) for (size_t ri = 0; ri < g_rows.size(); ri++) { const auto& kr = g_rows[ri]; if (kr.op != 0x0118 || kr.match != 1 || kr.action != "kill") continue;
            std::vector<RecP> ks; for (auto& c : falses) if (isKill(c) && siteN[c->p] >= 5) ks.push_back(c); const auto& c = ks[(size_t)attempt % ks.size()]; const auto a = ResolveArgs(kr, *c); if (!a || a->empty()) break;
            d.act = true; d.row = &kr; d.rowIdx = (int)ri; d.cond = c; d.args = *a; d.op = 0x0118; return d; }
    }
    if (falses.size() > 1 && !std::getenv("NOTSA_MDRIVER_NOTIMERVAR")) { const auto tz = [&](const RecP& r) { return (r->raw & 0x7FFF) == 0x0038 && !(r->raw & 0x8000) && g_timerVars.count(r->vref[0]) && ArgInt(*r, 1) && *ArgInt(*r, 1) == 0; }; if (std::any_of(falses.begin(), falses.end(), [&](const RecP& r) { return !tz(r); })) falses.erase(std::remove_if(falses.begin(), falses.end(), tz), falses.end()); } // CR39 (47)
    if (std::getenv("NOTSA_MDRIVER_FALSELOG")) { std::string fs; for (auto& f : falses) { char b[24]; std::snprintf(b, sizeof b, "%x ", (unsigned)f->raw); fs += b; } for (auto& r : loop) if (r->cond == 0 && ((r->raw & 0x7FFF) == 0x0119 || (r->raw & 0x7FFF) == 0x0118)) { char b[48]; std::snprintf(b, sizeof b, "[119 h=%d guard=%d] ", ArgInt(*r, 0) ? (int)*ArgInt(*r, 0) : -1, (int)AnyGuard(*r) * 1000 + (int)IsPlayerGuard(*r) * 100 + (int)KillRefusal(g_thread, *r) * 10 + (int)NotInCarGuard(g_thread, *r) + (int)VerdictOf(g_thread, *r) * 10000 + (int)Guarded().count(ArgInt(*r, 0) ? (int)*ArgInt(*r, 0) : -1) * 100000 + (int)ProtectedHandle(ArgInt(*r, 0) ? (int)*ArgInt(*r, 0) : -1) * 1000000); fs += b; } { std::string bs; for (auto& b : blockers) { char t[40]; std::snprintf(t, sizeof t, "%x@%x ", (unsigned)b->raw, (unsigned)(b->p - g_thread->m_BaseIP)); bs += t; } notsa::standalone::Fixups::Log("[mdriver] falses: %s loop=%zu blockers: %s", fs.c_str(), loop.size(), bs.c_str()); } }
    // CR43 (60 OUTRIDER: the roadblock stage clears a flag per live gunman / car: `@176 = 1; FOR i: IF NOT IS_CHAR_DEAD ped[i] -> @176 = 0; IF NOT IS_CAR_DEAD car[i] -> @176 = 0`, the van is let through when `@176 == 1` and is shot by the gunmen while it waits): a TRUE NOT-form IS_CHAR_DEAD / IS_CAR_DEAD in the passes (a ped / car that lives) whose true branch sets a variable to 0 while a plain `var == N` (N != 0) compare of that variable is FALSE in the same passes: those peds die and those cars are wrecked in one answer (never a buddy or a fail guard)
    if (!wide && !g_auxMode && !std::getenv("NOTSA_MDRIVER_NOFLAGKILL") && g_thread) {
        std::set<uint32_t> want; for (auto& r : g_recsBig) { if (r->waitSeq < g_waitSeq - 3 || r->cond != 0 || (r->raw & 0x8000) || ((r->raw & 0x7FFF) != 0x0038 && (r->raw & 0x7FFF) != 0x0039) || !r->vref[0]) continue; const auto v = ArgInt(*r, 1); if (v && *v != 0) want.insert(r->vref[0]); }
        if (!want.empty()) {
            std::vector<int> hs; std::set<int> seenH;
            for (auto& q : g_recsBig) {
                if (q->waitSeq < g_waitSeq - 3 || q->cond != 1 || (q->raw != 0x8118 && q->raw != 0x8119) || !q->hasBranch || q->jumped || !q->fall) continue;
                const auto h = ArgInt(*q, 0); if (!h || *h <= 0) continue; const int hh = (int)*h; if (seenH.count(q->raw == 0x8118 ? hh : -hh)) continue;
                std::vector<std::pair<uint32_t, int32_t>> sets; std::set<const uint8_t*> seen; FlagSets(g_thread, q->fall, 0, sets, seen);
                bool clears = false; for (auto& [ref, val] : sets) if (ref != 0xFFFFFFFFu && val == 0 && want.count(ref)) clears = true;
                if (!clears) continue;
                if (q->raw == 0x8118) { auto* pe = CPools::GetPed(hh); if (!pe || pe == Cj() || pe->m_fHealth <= 0.f || ProtectedPed(pe) || GuardedOf(false).count(hh)) continue; seenH.insert(hh); hs.push_back(hh); }
                else { auto* ve = CPools::GetVehicle(hh); if (!ve || ve->m_fHealth <= 0.f) continue; bool prot = false; std::vector<CPed*> occ; if (ve->m_pDriver) occ.push_back(ve->m_pDriver); for (auto* pp : ve->m_apPassengers) if (pp) occ.push_back(pp); for (auto* o : occ) if (o == Cj() || ProtectedPed(o) || (o->m_fHealth > 0.f && !HatesSide((int)o->m_nPedType) && (int)o->m_nPedType != (int)PED_TYPE_MISSION1)) prot = true; if (prot || GuardedOf(false).count(hh)) continue; seenH.insert(-hh); hs.push_back(-hh); } /* a car handle is passed negated (a ped and a car may share a pool ref) */
            }
            if (!hs.empty()) {
                static const Row kKs{ 0x8118, 2, "NOT IS_CHAR_DEAD / IS_CAR_DEAD true: a flag the script clears while the char / car lives", "killset", {}, 30 };
                d.act = true; d.row = &kKs; d.rowIdx = -30; d.cond = nullptr; d.op = 0x0118; d.args.clear(); for (const int hh : hs) { RArg a; a.n = hh; d.args.push_back(a); }
                notsa::standalone::Fixups::Log("[mdriver] flagkill: %zu chars / cars whose live state clears a flag the pass waits for", hs.size()); return d;
            }
        }
    }
    if (falses.empty() && blockers.empty()) { d.reason = "no condition evaluated false in the loop (the guards left out)"; return d; }
    // CR22: built-in verbs for opcodes the TS table has no row for (proposed rows: see C:/gta-scratch/cr22/FINDINGS.txt FOR TS). They come before every table row (a wait row of another false condition of the pass must not shadow them).
    static const Row kDmg{ 0x0366, 1, "HAS_OBJECT_BEEN_DAMAGED", "damageobj", { "$0" }, 30 };
    static const Row kDmgW{ 0x08FF, 1, "HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON", "damageobjw", { "$0", "$1" }, 30 }; // CR49 (86 YOU'VE HAD YOUR CHIPS: the nine shooting-gallery objects have to be hit by a shotgun)
    static const Row kExp{ 0x0356, 1, "IS_EXPLOSION_IN_AREA", "explode", { "$0", "$1", "$2", "$3", "$4", "$5", "$6" }, 60 }; // CR36 (42 SMALL TOWN BANK: the vault wall waits for an explosion in a 2 m box)
    static const Row kAtt{ 0x07AB, 1, "IS_TRAILER_ATTACHED_TO_CAB", "attachtrailer", { "$0", "$1" }, 60 }; // CR37 (43 TANKER COMMANDER: the trailer has to be hitched to the cab)
    static const Row kSmash{ 0x0716, 1, "IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED", "smashobj", { "$0", "$1", "$2", "$3", "$4", "$5", "$6" }, 30 }; // CR38 (47 ARE YOU GOING TO SAN FIERRO?: the three pot plants have to be damaged)
    static const Row kObjLoc{ 0x04E6, 1, "LOCATE_OBJECT_3D", "objto", { "$0", "$1", "$2", "$3" }, 60 }; // CR39 (50 DECONSTRUCTION: the toilet object has to be pushed into the pit: the locate box is 2.9 x 5 m)
    static const Row kWant{ 0x010F, 1, "IS_WANTED_LEVEL_GREATER", "setwanted", {}, 60 }; // CR46 (66 MIKE TORENO: the stage waits `IS_WANTED_LEVEL_GREATER player 0`: the cops chase starts when CJ is wanted)
    static const Row kHorn{ 0x0122, 1, "IS_PLAYER_PRESSING_HORN", "horn", {}, 60 }; // CR39 (49 WEAR FLOWERS IN YOUR HAIR: the hydraulics stage waits for the horn)
    static const Row kWater{ 0x04AD, 1, "IS_CHAR_IN_WATER", "swimgo", {}, 90 }; // CR39 (56 AMPHIBIOUS ASSAULT: CJ has to swim to the dock)
    static const Row kCarDmg{ 0x051C, 1, "HAS_CAR_BEEN_DAMAGED_BY_CHAR", "cardmg", { "$0", "$1" }, 30 }; // CR43 (62 TORENO'S LAST FLIGHT: the landed helicopter has to be damaged by CJ)
    static const Row kPho{ 0x04C5, 1, "HAS_CHAR_BEEN_PHOTOGRAPHED", "photo", { "$0" }, 30 }; // CR25 (58 PHOTO OPPORTUNITY)
    static const Row kTag{ 0x0038, 0, "int compare of the GET_PERCENTAGE_TAGGED_IN_AREA result", "tagarea", {}, 60 };
    static const Row kFix{ 0x03C9, 0, "NOT IS_CAR_VISIBLY_DAMAGED", "carfix", { "$0" }, 30 }; // CR34 (33 MANAGEMENT ISSUES)
    static const Row kSpd{ 0x0021, 0, "var > N of a GET_CAR_SPEED result", "carspeed", {}, 30 }; // CR34 (33 MANAGEMENT ISSUES)
    static const Row kPrj{ 0x02EE, 1, "IS_PROJECTILE_IN_AREA", "projectile", { "$0", "$1", "$2", "$3", "$4", "$5" }, 40 }; // CR26 (22 BURNING DESIRE)
    static const Row kGrp{ 0x06EE, 1, "IS_GROUP_MEMBER", "joingroup", { "$0", "$1" }, 30 }; // CR26 (20 LOS SEPULCROS)
    if (!std::getenv("NOTSA_MDRIVER_NOBUILTIN")) {
        const auto tagCond = [&](const RecP& c) { // an integer compare of the result variable of a GET_PERCENTAGE_TAGGED_IN_AREA: the area it measured (a false plain compare, or the TRUE NOT-form compare of the wait group: `WHILE NOT $27 == 100`)
            const int op = c->raw & 0x7FFF; const bool intCmp = (op >= 0x0018 && op <= 0x0041) || (op >= 0x04A3 && op <= 0x04B7) || (op >= 0x07D6 && op <= 0x07D9); if (!intCmp) return false;
            for (int k = 0; k < 2; k++) if (c->vref[k]) if (auto it = g_tagAreas.find(c->vref[k]); it != g_tagAreas.end()) { for (int j = 0; j < 4; j++) g_tagRect[j] = it->second[j]; return true; }
            return false;
        };
        if (!std::getenv("NOTSA_MDRIVER_NOHURT")) for (auto& [hp_, c] : g_healthConds) { if (std::getenv("NOTSA_MDRIVER_HPLOG") && g_healthVars.count(c->vref[1])) { const auto it_ = g_healthVars.find(c->vref[1]); auto* pe_ = it_ != g_healthVars.end() ? CPools::GetPed(it_->second) : nullptr; const auto th_ = ArgInt(*c, 0); notsa::standalone::Fixups::Log("[hplog] healthcond ip %x cond %d waitSeq %d/%d thr %d ped %d type %d model %d hp %.0f hates %d likes %d", (unsigned)(c->p - g_thread->m_BaseIP), c->cond, c->waitSeq, g_waitSeq, th_ ? (int)*th_ : -1, it_ != g_healthVars.end() ? it_->second : -1, pe_ ? (int)pe_->m_nPedType : -1, pe_ ? (int)pe_->m_nModelIndex : -1, pe_ ? pe_->m_fHealth : -1.f, pe_ ? (int)HatesSide((int)pe_->m_nPedType) : -1, pe_ ? (int)LikesSide((int)pe_->m_nPedType) : -1); } if (c->cond != 0 || c->waitSeq < g_waitSeq - 3) continue; // CR35 OPT-IN NOTSA_MDRIVER_HURT (37 REUNITING THE FAMILIES, not enough alone: the decision came 1000 frames late): the scripted bike enemy punches CJ to death until his health (GET_CHAR_HEALTH -> @232) is below 901: `901 > @232`): a false `N > var` (IS_NUMBER_GREATER_THAN_INT_LVAR 001B) of a GET_CHAR_HEALTH result = that char is hurt below N (a script ped that is neither CJ nor protected)
            if (std::getenv("NOTSA_MDRIVER_HPLOG") && (c->raw & 0x7FFF) == 0x001B) notsa::standalone::Fixups::Log("[hplog] cand raw %x vref %x %x known=%d", c->raw, c->vref[0], c->vref[1], (int)g_healthVars.count(c->vref[1]));
            if (c->raw != 0x001B || !c->vref[1]) continue; auto it = g_healthVars.find(c->vref[1]); if (it == g_healthVars.end()) continue; const auto thr = ArgInt(*c, 0); auto* pp = CPools::GetPed(it->second); if (!thr || !pp || pp == Cj() || pp->m_fHealth <= 0.f || ProtectedHandle(it->second) || *thr < 2 || !HostileScriptPed(pp)) continue; // CR38: only a ped hostile to CJ (22 BURNING DESIRE: Denise, a neutral civilian, was hurt below 2 = the mission failed)
            static const Row kHp{ 0x001B, 0, "N > GET_CHAR_HEALTH result", "hurt", {}, 30 };
            d.act = true; d.row = &kHp; d.rowIdx = -16; d.cond = c; d.op = 0x001B; d.args.clear(); for (const float f : { (float)it->second, (float)*thr - 1.f }) { RArg a; a.n = f; d.args.push_back(a); } return d; }
        if (std::getenv("NOTSA_MDRIVER_CARSPEED")) for (auto& c : falses) { /* opt-in: regressed 18 SWEET'S GIRL (answered before the table row that passes) */ // CR34 (33: `GET_CAR_SPEED car @v ... IF @v > 10.0` of the pier run): a false plain `speed var > N` (IS_FLOAT_[L]VAR_GREATER_THAN_NUMBER 0020 / 0021): CJ's car (the car the variable was read from, with CJ in it) is kept at N + 4 m/s along its heading for 700 frames (long enough for the next answers to put it where the script also wants it)
            const int op = c->raw & 0x7FFF; if ((c->raw & 0x8000) || (op != 0x0020 && op != 0x0021) || !c->vref[0]) continue; auto it = g_speedVars.find(c->vref[0]); if (it == g_speedVars.end()) continue; const auto thr = ArgFloat(*c, 1); if (!thr || *thr < 1.0 || *thr > 40.0) continue; auto* v = CPools::GetVehicle(it->second); if (!v || !Cj() || !Cj()->bInVehicle || Cj()->m_pVehicle != v) continue;
            d.act = true; d.row = &kSpd; d.rowIdx = -15; d.cond = c; d.op = op; d.args.clear(); for (const float f : { (float)it->second, (float)*thr + 4.f }) { RArg a; a.n = f; d.args.push_back(a); } return d; }
        if (!std::getenv("NOTSA_MDRIVER_NOCARFIX")) for (auto& c : falses) { // CR34 (33 MANAGEMENT ISSUES: random traffic bumped the mission car on its way, the stage waits for `NOT IS_CAR_VISIBLY_DAMAGED car` (+ the car health), the Pay n Spray cannot do it, SET_CAN_RESPRAY_CAR 0): a false NOT-form IS_CAR_VISIBLY_DAMAGED is answered by repairing that car (CVehicle::Fix: the Pay n Spray outcome)
            if (c->raw != 0x83C9) continue; auto a2 = ResolveArgs(kFix, *c); if (!a2 || a2->empty() || !CPools::GetVehicle((int)(*a2)[0].n)) continue;
            d.act = true; d.row = &kFix; d.rowIdx = -14; d.cond = c; d.args = *a2; d.op = 0x03C9; return d; }
        for (auto& c : falses) {
            const int op = c->raw & 0x7FFF; if (c->raw & 0x8000) continue;
            // CR35 (39 BADLANDS): a photo test answered 3 times that is still false is not the blocker when another false condition of the pass can be answered (the shot is consumed by the script's own read, a later gate of the same AND chain stays false: here a LOCATE_CHAR_IN_CAR_3D, CJ on foot): the later answers go to the other conditions
            static std::map<const uint8_t*, int> s_photoAns; const bool photoSpent = !std::getenv("NOTSA_MDRIVER_NOPHOTOSPENT") && op == 0x04C5 && s_photoAns[c->p] >= 3 && falses.size() > 1;
            if ((op == 0x001B || op == 0x001A) && c->vref[1] && !std::getenv("NOTSA_MDRIVER_NOCARHP")) { const auto lim = ArgInt(*c, 0); const auto hv = g_carHealthVars.find(c->vref[1]); auto* tv = hv != g_carHealthVars.end() ? CPools::GetVehicle(hv->second) : nullptr; // CR44 (62 TORENO'S LAST FLIGHT: the stage reads GET_CAR_HEALTH of the helicopter, `(health - 250) / 10`, and crashes it when `1 > var`): a plain false `1 > var` of a var that holds a car's health: the aircraft is shot down (cardmg: last damager CJ, health 255)
                if (lim && *lim <= 1 && tv && (tv->IsSubHeli() || tv->IsSubPlane()) && tv->m_fHealth > 260.f) { bool prot = false; if (tv->m_pDriver && (tv->m_pDriver == Cj() || ProtectedPed(tv->m_pDriver))) prot = true; for (auto* pp : tv->m_apPassengers) if (pp && (pp == Cj() || ProtectedPed(pp))) prot = true; if (!prot) { d.act = true; d.row = &kCarDmg; d.rowIdx = -31; d.cond = c; d.op = op; d.args = { RArg{ false, (double)hv->second, {} } }; return d; } } }
            if (op == 0x051C && !std::getenv("NOTSA_MDRIVER_NOCARDMG")) { auto a2 = ResolveArgs(kCarDmg, *c); const auto ch = ArgInt(*c, 1); auto* tv = a2 && a2->size() == 2 ? CPools::GetVehicle((int)(*a2)[0].n) : nullptr; const auto vd = VerdictOf(g_thread, *c); // the char is CJ (or any: -1), the car lives and has no buddy in it, the true branch is no fail
                if (tv && (tv->IsSubHeli() || tv->IsSubPlane()) /* aircraft only: the gunfire answer changed 59 / 52 / 30 / 89 */ && tv->m_fHealth > 0.f && ch && (*ch == -1 || CPools::GetPed((int)*ch) == Cj()) && (vd == Verdict::Wait || vd == Verdict::GoesOn)) { bool prot = false; if (tv->m_pDriver && (tv->m_pDriver == Cj() || ProtectedPed(tv->m_pDriver))) prot = true; for (auto* pp : tv->m_apPassengers) if (pp && (pp == Cj() || ProtectedPed(pp))) prot = true; if (!prot) { d.act = true; d.row = &kCarDmg; d.rowIdx = -31; d.cond = c; d.args = *a2; d.op = op; return d; } } }
            if (op == 0x04C5 && !photoSpent && !std::getenv("NOTSA_MDRIVER_NOPHOTO")) { auto a2 = ResolveArgs(kPho, *c); if (a2) { s_photoAns[c->p]++; d.act = true; d.row = &kPho; d.rowIdx = -6; d.cond = c; d.args = *a2; d.op = op; return d; } }
            if (op == 0x06EE && !std::getenv("NOTSA_MDRIVER_NOJOINGROUP")) { auto a2 = ResolveArgs(kGrp, *c); if (a2 && a2->size() == 2 && CPools::GetPed((int)(*a2)[0].n) && CPools::GetPed((int)(*a2)[0].n) != Cj()) { d.act = true; d.row = &kGrp; d.rowIdx = -8; d.cond = c; d.args = *a2; d.op = op; return d; } }
            if (op == 0x02EE && !std::getenv("NOTSA_MDRIVER_NOPROJ")) { auto a2 = ResolveArgs(kPrj, *c); if (a2 && a2->size() == 6) { d.act = true; d.row = &kPrj; d.rowIdx = -12; d.cond = c; d.args = *a2; d.op = op; d.reason = "";
                // every distinct false projectile area of the loop gets its molotov in the same frame (the script counts them in one pass)
                for (auto& q : falses) if (q != c && (q->raw & 0x7FFF) == 0x02EE && !(q->raw & 0x8000)) if (auto a3 = ResolveArgs(kPrj, *q); a3 && a3->size() == 6) for (auto& x : *a3) d.args.push_back(x);
                return d; } }
            if (op == 0x0356 && !std::getenv("NOTSA_MDRIVER_NOEXPLODE")) { auto a2 = ResolveArgs(kExp, *c); if (a2 && a2->size() == 7) { d.act = true; d.row = &kExp; d.rowIdx = -16; d.cond = c; d.args = *a2; d.op = op; return d; } } // CR36: IS_EXPLOSION_IN_AREA(type, box) is false: an explosion of that type at the box centre
            if (op == 0x04E6 && !std::getenv("NOTSA_MDRIVER_NOOBJTO")) { auto a2 = ResolveArgs(kObjLoc, *c); const auto ox = ArgFloat(*c, 1), oy = ArgFloat(*c, 2), oz = ArgFloat(*c, 3); if (a2 && a2->size() == 4 && ox && oy && oz && CPools::GetObject((int)(*a2)[0].n)) { d.act = true; d.row = &kObjLoc; d.rowIdx = -19; d.cond = c; d.args = *a2; d.args[1].n = *ox; d.args[2].n = *oy; d.args[3].n = *oz; d.op = op; return d; } } // CR39: LOCATE_OBJECT_3D(obj, box) is false: the object is put at the centre of the box (a stand-in for the push)
            if (op == 0x010F && std::getenv("NOTSA_MDRIVER_WANTSTAR") && g_thread) { const auto wl = ArgInt(*c, 1); const auto vd = VerdictOf(g_thread, *c); if (wl && *wl >= 0 && *wl < 6 && (vd == Verdict::Wait || vd == Verdict::GoesOn) && FindPlayerWanted() && (int)FindPlayerWanted()->m_WantedLevel <= *wl) { d.act = true; d.row = &kWant; d.rowIdx = -40; d.cond = c; d.op = op; d.args.clear(); RArg a; a.n = (double)(*wl + 1); d.args.push_back(a); return d; } } // CR46: a plain false IS_WANTED_LEVEL_GREATER (neither a fail guard) is answered by raising the wanted level one above the limit
            if (op == 0x0122 && !std::getenv("NOTSA_MDRIVER_NOHORN")) { d.act = true; d.row = &kHorn; d.rowIdx = -21; d.cond = c; d.args.clear(); d.op = op; return d; } // CR39: IS_PLAYER_PRESSING_HORN is false: the horn is held for 30 frames
            if (op == 0x04AD && !std::getenv("NOTSA_MDRIVER_NOSWIM")) { float sx = 0, sy = 0; if (PeekFootLocateXY(c->p, sx, sy)) { d.act = true; d.row = &kWater; d.rowIdx = -23; d.cond = c; d.op = op; d.args.clear(); for (const float f : { sx, sy }) { RArg a; a.n = f; d.args.push_back(a); } return d; } } // CR39: IS_CHAR_IN_WATER(CJ) is false and the script goes on with a foot locate: CJ leaves his vehicle and is put there
            if (op == 0x07AB && !std::getenv("NOTSA_MDRIVER_NOATTACH")) { auto a2 = ResolveArgs(kAtt, *c); if (a2 && a2->size() == 2) { d.act = true; d.row = &kAtt; d.rowIdx = -17; d.cond = c; d.args = *a2; d.op = op; return d; } } // CR37: IS_TRAILER_ATTACHED_TO_CAB(trailer, cab) is false: the cab is put at the trailer hitch, the trailer scans for it
            if (op == 0x001B && !(c->raw & 0x8000) && c->vref[1] && !std::getenv("NOTSA_MDRIVER_NOOBJHP") && ArgInt(*c, 0) && (*ArgInt(*c, 0) <= 1 || (*ArgInt(*c, 0) >= 900 && attempt >= 5 && (std::getenv("NOTSA_MDRIVER_HP999ANY") || ([&] { const auto hv = g_objHealthVars.find(c->vref[1]); auto* ho = hv != g_objHealthVars.end() ? CPools::GetObject(hv->second) : nullptr; return ho && ho->objectFlags.bIsTargetable; }() /* CR44: only an object the script made targettable (57); 50 holds a plain object */)) && (VerdictOf(g_thread, *c) == Verdict::Wait || VerdictOf(g_thread, *c) == Verdict::GoesOn) && !std::getenv("NOTSA_MDRIVER_NOOBJHP999") && (std::getenv("NOTSA_MDRIVER_NOHP999LAST") || std::none_of(falses.begin(), falses.end(), [](const RecP& q) { return (q->raw & 0x7FFF) == 0x0366 && !(q->raw & 0x8000); }))))) { /* CR44 (50 DECONSTRUCTION: the stage holds `999 > health` AND the real HAS_OBJECT_BEEN_DAMAGED tests of the next objects; the 999 answer repeated at the same object shadowed them): the 999 answer only when no plain false 0366 is left */ /* CR43 (57 THE DA NANG THANG: stage 4 loops on `999 > object health` of the blipped, targettable object) */ // CR38: only `1 > health` (the object has to be destroyed; 26 ROBBING UNCLE SAM reads `5 > health` of a crate that is not to be shot, the damage answer shadowed the real one) // CR37 (42 SMALL TOWN BANK: `IF 1 > health` of the vault wall's second object, read with GET_OBJECT_HEALTH): the object is damaged as by a bullet (CJ the damager), like HAS_OBJECT_BEEN_DAMAGED
                if (auto it = g_objHealthVars.find(c->vref[1]); it != g_objHealthVars.end() && CPools::GetObject(it->second)) { d.act = true; d.row = &kDmg; d.rowIdx = -2; d.cond = c; d.op = op; d.args = { RArg{ false, (double)it->second, {} } }; return d; } }
            if (op == 0x0716 && !std::getenv("NOTSA_MDRIVER_NOSMASH")) { auto a2 = ResolveArgs(kSmash, *c); if (a2 && a2->size() == 7) { d.act = true; d.row = &kSmash; d.rowIdx = -18; d.cond = c; d.args = *a2; d.op = op; /* every other distinct false object test of the pass too (47: ~44 pot plants, the mission is timed: one 630-frame round per plant arrests the Truth) */ std::set<std::string> seen; seen.insert(std::to_string((*a2)[0].n) + "," + std::to_string((*a2)[1].n)); if (!std::getenv("NOTSA_MDRIVER_NOSMASHALL")) for (auto& q : falses) if (q != c && (q->raw & 0x7FFF) == 0x0716 && !(q->raw & 0x8000)) if (auto a3 = ResolveArgs(kSmash, *q); a3 && a3->size() == 7 && seen.insert(std::to_string((*a3)[0].n) + "," + std::to_string((*a3)[1].n)).second) for (auto& x : *a3) d.args.push_back(x); return d; } } // CR38: IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED is false: CJ damages / breaks the object the test finds
            if (op == 0x08FF && !std::getenv("NOTSA_MDRIVER_NOOBJWEAP")) { auto a2 = ResolveArgs(kDmgW, *c); if (a2 && a2->size() == 2 && (*a2)[1].n >= 22.0 && (*a2)[1].n <= 34.0 /* a hand firearm: 50 DECONSTRUCTION tests rocket (35) and satchel (39) damage and passes without an answer */ && CPools::GetObject((int)(*a2)[0].n)) { d.act = true; d.row = &kDmgW; d.rowIdx = -41; d.cond = c; d.args = *a2; d.op = op; /* every other distinct false object test of the pass too (86: nine targets, one pass) */ std::set<int> seen; seen.insert((int)(*a2)[0].n); for (auto& q : falses) if (q != c && (q->raw & 0x7FFF) == 0x08FF && !(q->raw & 0x8000)) if (auto a3 = ResolveArgs(kDmgW, *q); a3 && a3->size() == 2 && CPools::GetObject((int)(*a3)[0].n) && seen.insert((int)(*a3)[0].n).second) for (auto& x : *a3) d.args.push_back(x); return d; } } // CR49: HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON(obj, weapon) is false: CJ damages the object with that weapon
            if (op == 0x0366) { auto a2 = ResolveArgs(kDmg, *c); if (a2 && wide && (a2->empty() || !CPools::GetObject((int)(*a2)[0].n))) continue; /* CR23: a record of an older pass: the object may be gone */ if (a2) { d.act = true; d.row = &kDmg; d.rowIdx = -2; d.cond = c; d.args = *a2; d.op = op; return d; } }
            if (tagCond(c)) { d.act = true; d.row = &kTag; d.rowIdx = -3; d.cond = c; d.op = op; return d; }
        }
        if (!std::getenv("NOTSA_MDRIVER_NOZONEGO")) for (size_t k = loop.size(); k-- > 0;) { // CR26 (21 DOBERMAN): the stage waits for the player to be in an info zone (a plain false IS_PLAYER_IN_INFO_ZONE, or its TRUE NOT form straight before a WAIT): the player (with his car) goes to the zone's centre
            const auto& r = loop[k]; if ((r->raw & 0x7FFF) != 0x0583 || AnyGuard(*r)) continue;
            const bool plainFalse = !(r->raw & 0x8000) && r->cond == 0, notBlocker = (r->raw & 0x8000) && r->cond == 1 && r->waitAfter && !r->jumped; if (!plainFalse && !notBlocker) continue;
            char nm[9]; float zx, zy; if (!ZoneNameOf(*r, nm) || !InfoZoneCentre(nm, zx, zy)) continue;
            static Row kZone{ 0x0583, 0, "IS_PLAYER_IN_INFO_ZONE", "zonego", {}, 60 };
            d.act = true; d.row = &kZone; d.rowIdx = -9; d.cond = r; d.op = 0x0583; d.args.clear(); for (const float f : { zx, zy, -100.f }) { RArg a; a.n = f; d.args.push_back(a); }
            notsa::standalone::Fixups::Log("[mdriver] zonego %s -> %.1f,%.1f", nm, zx, zy); return d;
        }
        if (!std::getenv("NOTSA_MDRIVER_NOGANGWAR")) for (size_t k = loop.size(); k-- > 0;) { // CR26 (21 DOBERMAN): the stage waits for a gang war (the TRUE NOT form of IS_GANG_WAR_GOING_ON straight before a WAIT, or the plain false one): the player provokes it the way the game does, by killing gang members in the zone (CGangWars::AddKillToProvocation: 3 kills > 2.1)
            const auto& r = loop[k]; if ((r->raw & 0x7FFF) != 0x087A || AnyGuard(*r)) continue;
            const bool plainFalse = !(r->raw & 0x8000) && r->cond == 0, notBlocker = (r->raw & 0x8000) && r->cond == 1 && r->waitAfter && !r->jumped; if (!plainFalse && !notBlocker) continue;
            static Row kGw{ 0x087A, 0, "IS_GANG_WAR_GOING_ON", "gangwar", {}, 60 };
            d.act = true; d.row = &kGw; d.rowIdx = -10; d.cond = r; d.op = 0x087A; d.args.clear(); return d;
        }
        static const Row kEnter{ 0x0039, 1, "int compare of the GET_AREA_VISIBLE result", "enterarea", {}, 200 };
        bool blockAnswerable = false; // CR25 (14): the loop's own wait group has a TRUE NOT-form member a warp row answers (CJ / Sweet not at the house): the stage waits for THAT, not for the area compare of an IF of the loop body
        if (!std::getenv("NOTSA_MDRIVER_NOBLOCKFIRST")) for (auto& b : blockers) for (auto& row : g_rows) if (row.match != 2 && row.op == (b->raw & 0x7FFF) && (row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint" || row.action == "entercar" || row.action == "warpto_handle") && ResolveArgs(row, *b)) blockAnswerable = true;
        if (!blockAnswerable && blockers.empty() && !std::getenv("NOTSA_MDRIVER_NOLOOSECAR")) for (auto& r : loop) { // CR28 (24 HOME INVASION: the escort stage is an IF ladder, `IF NOT IS_CHAR_IN_CAR cj @car` is TRUE all the time and its body only re-adds the car blip; the `GET_AREA_VISIBLE == 8` IF of another stage step is false): a TRUE NOT IS_CHAR_IN_CAR of CJ for a script car var outside the wait group is the real blocker, not the area compare
            if (r->raw != 0x80DB || r->cond != 1 || AnyGuard(*r)) continue;
            for (auto& row : g_rows) if (row.match != 2 && row.op == 0x00DB && row.action == "entercar" && ResolveArgs(row, *r)) { blockers.push_back(r); blockAnswerable = true; break; }
            if (blockAnswerable) break;
        }
        if (!blockAnswerable && !std::getenv("NOTSA_MDRIVER_NODISTFETCH")) for (auto& c : falses) { // CR30 (22 BURNING DESIRE: the buddy follow stage tests `20 > distance` (IS_NUMBER_GREATER_THAN_FLOAT_LVAR, the var from GET_DISTANCE_BETWEEN_COORDS between CJ and the buddy)): the compared distance value identifies the ped at that distance from CJ; it is brought next to CJ
            if (c->raw != 0x0023 || c->cond != 0 || c->args.size() < 2 || AnyGuard(*c)) continue; auto* me = Cj(); if (!me) continue;
            const float lim = AF(c->args[0]), dv = AF(c->args[1]); if (!(dv > lim) || !(dv < 2000.f)) continue;
            CPed* hit = nullptr; for (auto& pd : GetPedPool()->GetAllValid()) { if (&pd == me || pd.m_fHealth <= 0.f) continue; const float dd = (pd.GetPosition() - me->GetPosition()).Magnitude(); if (std::fabs(dd - dv) < 1.5f) { if (hit) { hit = nullptr; break; } hit = &pd; } }
            if (!hit) continue; static std::map<int, int> distTry; if (++distTry[CPools::GetPedRef(hit)] > 2) continue; /* a ped brought twice and the compare still false: not the answer (19 CESAR: 8 futile warps) */ static const Row kDist{ 0x0023, 0, "distance to CJ over the limit", "distfetch", {}, 60 };
            d.act = true; d.row = &kDist; d.rowIdx = -15; d.cond = c; d.op = 0x0023; RArg ha; ha.n = CPools::GetPedRef(hit); d.args = { ha }; return d;
        }
        if (!blockAnswerable && !std::getenv("NOTSA_MDRIVER_NOTRAINNEAR")) for (auto& c : falses) { // CR41 (52 SNAIL TRAIL: `300 > @121`, @121 = the int distance from CJ to the followed train): a false plain `N > var` (001B, N >= 100) whose compared value equals the distance from CJ to a train (+-60 m, the nearest match): CJ joins that train, beside the tracks
            if (c->raw != 0x001B || c->cond != 0 || c->args.size() < 2 || AnyGuard(*c)) continue; auto* me = Cj(); if (!me) continue;
            const float lim = (float)(int32_t)c->args[0].bits, dv = (float)(int32_t)c->args[1].bits; /* int compare: the variable holds an int (AF would read its bits as a float) */ if (!(lim >= 100.f) || !(dv >= lim) || !(dv < 20000.f)) continue;
            if (std::getenv("NOTSA_MDRIVER_TRAINLOG")) for (auto& vv : GetVehiclePool()->GetAllValid()) if (vv.IsTrain()) notsa::standalone::Fixups::Log("[trainlog] lim %.0f dv %.0f CJ %.0f,%.0f train %d at %.0f,%.0f d %.0f hp %.0f", lim, dv, me->GetPosition().x, me->GetPosition().y, (int)vv.m_nModelIndex, vv.GetPosition().x, vv.GetPosition().y, (vv.GetPosition() - me->GetPosition()).Magnitude2D(), vv.m_fHealth);
            CVehicle* hit = nullptr; float bestd = 60.f; for (auto& vv : GetVehiclePool()->GetAllValid()) { if (!vv.IsTrain() || vv.m_fHealth <= 0.f) continue; const float dd = std::fabs((vv.GetPosition() - me->GetPosition()).Magnitude2D() - dv); if (dd < bestd) { bestd = dd; hit = &vv; } }
            if (!hit) continue; static int trainTry = 0; if (++trainTry > 12) continue; static const Row kTN{ 0x001B, 0, "distance to a train over the limit", "trainnear", {}, 90 };
            d.act = true; d.row = &kTN; d.rowIdx = -25; d.cond = c; d.op = 0x001B; RArg ha; ha.n = GetVehiclePool()->GetRef(hit); d.args = { ha }; return d;
        }
        for (auto& c : falses) { // CR23 (16 NINES AND AK'S: `WHILE @86 != 15` after the Binco blip): a false plain `var == N` of a GET_AREA_VISIBLE result variable: walk in through the entrance of interior N (an entry exit whose interior side is in area N)
            const int op = c->raw & 0x7FFF; if ((c->raw & 0x8000) || (op != 0x0038 && op != 0x0039) || std::getenv("NOTSA_MDRIVER_NOENTERAREA")) continue;
            if (!(c->vref[0] && g_areaVars.count(c->vref[0]))) continue;
            const auto n = ArgInt(*c, 1); if (!n || *n < 0) continue;
            if (blockAnswerable) continue;
            if (leashActive()) continue; // CR36
            if ((int)CGame::currArea == (int)*n && !std::getenv("NOTSA_MDRIVER_NOAREASAME")) continue; // CR37 (41 LOCAL LIQUOR STORE: `@43 == 0` with CJ already in area 0: the script reuses the variable as a mode flag after the robbers died, walking in or out answers nothing; the pickups of the stage are the wait)
            if (*n == 0 && c->jumped && c->target && InSpace(c->target) && (*(const uint16_t*)c->target & 0x7FFF) == 0x0051 && !std::getenv("NOTSA_MDRIVER_LEAVEANY")) continue; // CR24 (14): `GET_AREA_VISIBLE == 0` whose false branch skips to the RETURN of a helper sub (`IF @78 == 0` of a sub run every pass while CJ is inside the interior) is no wait: it must not drag CJ out of the interior
            if (!c->waitAfter && !std::getenv("NOTSA_MDRIVER_NOENTERLOOSE")) { // CR28 (24 HOME INVASION: the false `GET_AREA_VISIBLE == 8` is an IF of the main loop (the burglary timer display), its false branch is no wait; the stage's own plain false `LOCATE_CHAR_IN_CAR_3D` at the house is the blocker): an area compare that is no wait goes after the plain false conditions a warp / enter-car row answers
                bool other = false; for (auto& f2 : falses) { if (f2 == c || (f2->raw & 0x8000)) continue; if (f2->raw == 0x0214 || f2->raw == 0x0491) { other = true; break; } /* the pickup of the stage (22: the molotovs; the `pickupgo` verb answers it after a wait) */ for (auto& row : g_rows) if (row.match != 2 && row.op == (f2->raw & 0x7FFF) && (row.action == "pedwarp" || row.action == "vehwarp" || row.action == "midpoint" || row.action == "entercar" || row.action == "warpto_handle") && ResolveArgs(row, *f2)) { other = true; break; } if (other) break; }
                if (other) continue;
            }
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
    if (!blockers.empty() && !std::getenv("NOTSA_MDRIVER_NOKILLFIRST") && std::any_of(falses.begin(), falses.end(), [](const RecP& c) { return !(c->raw & 0x8000) && !LocatedOther(*c).empty(); })) { // CR25 (15 DRIVE-THRU: `WHILE OR(NOT dead @89, NOT dead @90)` waits for the Ballas in the Voodoo to die; the false `LOCATE_CAR_3D(Voodoo, spot)` of the body was answered by warping the Voodoo to the spot, which the script takes as the escape and fails): before ANOTHER entity is warped to a place, the live hostile NOT-dead blockers of the wait group are killed
        for (size_t i = 0; i < g_rows.size(); i++) { const auto& kr = g_rows[i]; if (kr.op != 0x0118 || kr.match != 1 || kr.action != "kill") continue;
            for (auto& b : blockers) { if (b->raw != 0x8118) continue; const auto a = ResolveArgs(kr, *b); if (!a || a->empty()) continue; const int h = (int)(*a)[0].n; auto* pp = CPools::GetPed(h);
                if (!pp || pp == Cj() || pp->m_fHealth <= 0.f || ProtectedHandle(h) || GuardedOf(false).count(h)) continue;
                if (!pp->bInVehicle && !std::getenv("NOTSA_MDRIVER_NOKILLFIRSTFOOT")) continue; // CR47 (70: the chased driver stands beside his stopped car, the stage wants CJ on foot next to it; the rule shot him): the kill-first rule is for the occupants of a car
                if (Cj() && (pp->GetPosition() - Cj()->GetPosition()).Magnitude() > KillFarM() && !std::getenv("NOTSA_MDRIVER_NOKILLFAR")) { notsa::standalone::Fixups::Log("[mdriver] killfar(first) refused h=%d dist=%.0f", h, (pp->GetPosition() - Cj()->GetPosition()).Magnitude()); continue; } // CR47 (70 PUNCTURE WOUNDS: the chased car's driver 1300 m away was killed by this rule at the first stall; the strip tests on him need him alive): a victim far out of reach is no part of the kill-first rule
                d.act = true; d.row = &kr; d.rowIdx = (int)i; d.cond = b; d.args = *a; d.op = 0x0118; return d; }
            break; }
    }
    // CR46 (70 PUNCTURE WOUNDS: the chase stage tests `IS_CHAR_IN_CAR cj, @39` (plain, false: CJ walked away from his car) next to a speed-governor locate `LOCATE_CHAR_ANY_MEANS_CAR_2D cj, chased car`; table order answered the locate 7 times (CJ on foot to the moving car) and never the car): a prerequisite condition of CJ sitting in a named live car, with CJ on foot, is answered before every position / proximity row
    const bool areaFalse = !std::getenv("NOTSA_MDRIVER_NOENTERAREA") && std::any_of(falses.begin(), falses.end(), [](const RecP& q) { const int o = q->raw & 0x7FFF; return !(q->raw & 0x8000) && o >= 0x05FC && o <= 0x0601; }); // CR47 (76 HIGHJACK: CJ sits on the bike, the stage's `IS_CHAR_IN_CAR cj, truck` is false and an angled area of the next group was answered first: it skipped the route's ambushes)
    if (!wide && !g_auxMode && blockers.empty() && Cj() && ((!Cj()->bInVehicle && !std::getenv("NOTSA_MDRIVER_NOENTERFIRST") && std::any_of(falses.begin(), falses.end(), [](const RecP& q) { const int o = q->raw & 0x7FFF; return !(q->raw & 0x8000) && o >= 0x0202 && o <= 0x0207; }) /* only against a car locate (23 regressed with the char locate 00F2 in the pass) */) || areaFalse)) {
        for (auto& c : falses) { if ((c->raw & 0xFFFF) != 0x00DB) continue; const auto h0 = ArgInt(*c, 0), h1 = ArgInt(*c, 1); if (!h0 || !h1 || EntityOfHandle((int)*h0, 0x00DB) != (CEntity*)Cj()) continue; auto* ve = CPools::GetVehicle((int)*h1); if (!ve || ve->m_fHealth <= 0.f || (GuardedOf(true).count((int)*h1) && !areaFalse) || (Cj()->bInVehicle && ve == Cj()->m_pVehicle)) continue;
            for (size_t ri = 0; ri < g_rows.size(); ri++) { const auto& kr = g_rows[ri]; if (kr.op != 0x00DB || kr.match != 1 || kr.action != "entercar") continue; const auto a = ResolveArgs(kr, *c); if (!a) break; d.act = true; d.row = &kr; d.rowIdx = (int)ri; d.cond = c; d.args = *a; d.op = 0x00DB; return d; } }
    }
    for (int pass = 0; pass < 2; pass++) {
    if (pass == 1 && blockers.empty()) break;
    for (size_t i = 0; i < g_rows.size(); i++) {
        const auto& row = g_rows[i];
        if (g_auxMode && !(row.action == "pedwarp" || row.action == "vehwarp" || row.action == "key")) continue; // CR27
        if (pass == 0 && !blockers.empty() && row.action == "wait") continue; // CR19: a wait row does not shadow the blockers of the wait group
        for (auto& c : falses) {
            if (!Matches(row, *c)) continue;
            struct Opt { RecP c; std::optional<std::vector<RArg>> args; }; std::vector<Opt> opts; std::set<std::string> seen;
            for (auto& q : falses) { if (!Matches(row, *q)) continue; auto a = ResolveArgs(row, *q); if (seen.insert(RKey(a)).second) opts.push_back({ q, a }); }
            if (row.action == "midpoint" && opts.size() > 1 && row.op >= 0x05FC && row.op <= 0x0601 && !std::getenv("NOTSA_MDRIVER_NOAREAORDER")) std::reverse(opts.begin(), opts.end()); // CR47 (76 HIGHJACK: four angled areas are the checkpoints of the truck's route in script order, the last one (`@75 < 2`) is the end of the route and sets the stage to 3; the first answer went to that last one and skipped the ambushes of the other three): several distinct false angled areas are answered in script order (falses holds the pass last to first)
            auto& pick = opts[(size_t)attempt % opts.size()];
            if ((row.action == "warpto_handle" || row.action == "pedwarp" || row.action == "vehwarp") && pass == 0 && !g_auxMode && pick.args && !std::getenv("NOTSA_MDRIVER_NOSPENTWARP")) { const auto sk = g_noProgress.find(std::to_string(g_stopIp) + ":" + std::to_string((int)i) + ":" + RKey(pick.args.value())); if ((sk != g_noProgress.end() && sk->second >= 3) || CoordLeashLive()) { bool other = false; for (auto& q : falses) if (q != pick.c && (q->raw & 0x7FFF) == 0x0118 && !(q->raw & 0x8000) && !AnyGuard(*q)) other = true; /* the later row is a plain IS_CHAR_DEAD kill (11 / 37 / 23 regressed with any other condition) */ if (other) break; } } // CR41 (59 JIZZY: the chase stage's `CJ near the driver` throttle IF, answered by warps that cannot keep up with the moving car, shadowed the plain `IS_CHAR_DEAD driver` kill row 98 until the 8-answer stop): a warp row answered 3 times with the same arguments at this wake ip without progress is spent while another false condition of the pass is left for the later rows
            if (row.action == "vehwarp" && pass == 0 && !g_auxMode && (attempt % 2) == 0 && !std::getenv("NOTSA_MDRIVER_NOKEYBEFOREWARP") && std::any_of(falses.begin(), falses.end(), [](const RecP& q) { return (q->raw & 0xFFFF) == 0x00E1 && !AnyGuard(*q); }) && std::any_of(g_rows.begin(), g_rows.end(), [](const Row& w) { return w.action == "key" && w.op == 0x00E1; })) break; // CR46 (73 SUPPLY LINES: the stage's `IS_BUTTON_PRESSED 16` (start the RC plane) is a plain false test of the pass; the vehicle-warp rows of the enemy-car proximity tests came first and dragged the hostile cars onto the parked plane): on every even attempt the button row is tried before a vehicle warp
            if (row.action == "key" && row.op == 0x00E1 && pass == 0 && std::getenv("NOTSA_MDRIVER_KEYKILLFIRST")) { bool killLeft = false; for (auto& q : falses) if (q != pick.c && (q->raw & 0x7FFF) == 0x0118 && !(q->raw & 0x8000) && !AnyGuard(*q)) killLeft = true; if (killLeft) break; } // CR44 (63 YAY KA-BOOM-BOOM: the key row 97 (the skip-cutscene button of an AND group that can not become true) shadowed the plain IS_CHAR_DEAD kill row 98 after the warps were spent): a plain false kill test of the pass goes before a button poll
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
    if (!std::getenv("NOTSA_MDRIVER_NOAUDWAIT")) { // CR39 (49 WEAR FLOWERS IN YOUR HAIR: `WHILE NOT HAS_MISSION_AUDIO_FINISHED` of a spoken line): the line ends by itself (3 s on the game clock), the driver waits instead of stopping
        static const Row kAud{ 0x03D2, 1, "HAS_MISSION_AUDIO_FINISHED", "wait", {}, 30 };
        for (size_t k = loop.size(); k-- > 0;) { const auto& r = loop[k]; const int rop = r->raw & 0x7FFF; if (rop == 0x03D2 || rop == 0x03D0) { d.act = true; d.row = &kAud; d.rowIdx = -22; d.cond = r; d.op = rop; d.args.clear(); return d; } }
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
    const bool lawSkip = (int)FindPlayerWanted()->m_WantedLevel > 0 && !std::getenv("NOTSA_MDRIVER_NOLAWCAR"); // CR39 (50 DECONSTRUCTION): with a wanted level the nearest car was a police car (busted); a police car is no car for a mission
    for (auto& vv : GetVehiclePool()->GetAllValid()) {
        if (vv.m_pDriver || (lawSkip && (vv.vehicleFlags.bIsLawEnforcer || vv.m_nModelIndex == 596 || vv.m_nModelIndex == 597 || vv.m_nModelIndex == 598 || vv.m_nModelIndex == 599 || vv.m_nModelIndex == 427 || vv.m_nModelIndex == 490 || vv.m_nModelIndex == 523 || vv.m_nModelIndex == 528)) || vv.m_nDoorLock == CARLOCK_LOCKED || vv.m_nDoorLock == CARLOCK_LOCKED_PLAYER_INSIDE || vv.m_nDoorLock == CARLOCK_LOCKOUT_PLAYER_ONLY) continue;
        const float dd = (float)(vv.GetPosition() - c->GetPosition()).Magnitude();
        if (dd < best) { best = dd; car = &vv; }
    }
    return car;
}
// CR39 (50 DECONSTRUCTION: `IF IS_CHAR_IN_ANY_CAR CJ / STORE_CAR_CHAR_IS_IN_NO_SAVE / IS_CAR_MODEL car 524` wants the cement truck; the nearest car was a police car -> busted): the model of an IS_CAR_MODEL (0137) that follows the 00DF test in the same condition list, -1 when none
// CR39 (50 DECONSTRUCTION: `LOCATE_CHAR_ANY_MEANS_3D CJ pit / IS_CHAR_IN_MODEL CJ 524 / IS_CAR_STOPPED / heading`): the model of an IS_CHAR_IN_MODEL (00DD, CJ) that follows a locate in the same condition list, -1 when none
int ModelWantedAfterLocate(const uint8_t* p) {
    if (!p || !InSpace(p) || (*(const uint16_t*)p & 0x7FFF) != 0x00FE) return -1;
    const uint8_t* q = p; bool mf = false, isI = false; int32_t iv = 0;
    for (int k = 0; k < 6 && q && InSpace(q); k++) {
        const uint16_t op = *(const uint16_t*)q & 0x7FFF; const int n = (op == 0x004D || op == 0x00D6) ? 1 : op == 0x00FE ? 8 : op == 0x00DD ? 2 : -1;
        if (n < 0) return -1;
        const uint8_t* a = q + 2; const uint8_t* second = nullptr;
        for (int i = 0; i < n && a && InSpace(a); i++) { if (i == 1) second = a; a = SkipOperand(a, mf, isI, iv); }
        if (op == 0x00DD && second) { if (*second == 1) return *(const int32_t*)(second + 1); if (*second == 5) return *(const int16_t*)(second + 1); if (*second == 4) return *(const int8_t*)(second + 1); return -1; }
        q = a;
    }
    return -1;
}
// CR49 (85 EXPLOSIVE SITUATION: `LOCATE_CHAR_IN_CAR_3D CJ crate / .. / IS_CHAR_IN_MODEL CJ 406 OR IS_CHAR_IN_MODEL CJ 486`: the crates are smashed with a Dumper / Dozer): every model of an IS_CHAR_IN_MODEL (00DD) within 14 commands after the LOCATE_CHAR_IN_CAR_3D (0100) at p
std::vector<int> ModelsWantedAfterCarLocate(const uint8_t* p) {
    std::vector<int> out; if (!p || !InSpace(p) || (*(const uint16_t*)p & 0x7FFF) != 0x0100) return out;
    const uint8_t* q = p;
    for (int k = 0; k < 14 && q && InSpace(q); k++) {
        const int op = *(const uint16_t*)q & 0x7FFF; const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(q, ops, cnt); if (!e) break;
        if (k > 0 && (op == 0x0100 || op == 0x00FE || op == 0x0002)) break; // the next test of another kind / a jump ends the group
        if (op == 0x00DD && cnt >= 2 && ops[1]) { const uint8_t* sc = ops[1]; int v = -1; if (*sc == 1) v = *(const int32_t*)(sc + 1); else if (*sc == 5) v = *(const int16_t*)(sc + 1); else if (*sc == 4) v = *(const int8_t*)(sc + 1); if (v >= 0) out.push_back(v); }
        q = e;
    }
    return out;
}
// CR40 (51 555 WE TIP: `WHILE NOT IS_CHAR_DEAD guard / GET_DEAD_CHAR_PICKUP_COORDS guard @x @y @z / AND(@x > a, @y > b, @z > c, d > @x, e > @y, f > @z)`, else the mission fails): the centre x, y of the box the dead ped's pickup coordinates must lie in, found within 16 commands after the IS_CHAR_DEAD at p
bool PeekDeadBox(const uint8_t* p, float* box /* x0 x1 y0 y1 z0 z1 (z: -1e9 / 1e9 when the script gives none) */) {
    if (!p || !InSpace(p) || (*(const uint16_t*)p & 0x7FFF) != 0x0118) return false;
    int lv[3]{ -1, -1, -1 }; float lo[3]{ NAN, NAN, NAN }, hi[3]{ NAN, NAN, NAN };
    for (int k = 0; k < 16 && p && InSpace(p); k++) {
        const int op = *(const uint16_t*)p & 0x7FFF; const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return false;
        if (op == 0x04A5) { const uint8_t* a = p + 2; bool mf = false, isI = false; int32_t iv = 0; for (int i = 0; i < 4 && a && InSpace(a); i++) { if (i >= 1 && *a == 3) lv[i - 1] = *(const uint16_t*)(a + 1); a = SkipOperand(a, mf, isI, iv); } }
        else if ((op == 0x0021 || op == 0x0023) && cnt >= 2 && ops[0] && ops[1]) {
            const uint8_t* lvp = op == 0x0021 ? ops[0] : ops[1]; const uint8_t* fp = op == 0x0021 ? ops[1] : ops[0];
            if (*lvp == 3 && *fp == 6) for (int i = 0; i < 3; i++) if (lv[i] >= 0 && lv[i] == (int)*(const uint16_t*)(lvp + 1)) (op == 0x0021 ? lo : hi)[i] = *(const float*)(fp + 1);
        }
        p = e;
    }
    if (std::isnan(lo[0]) || std::isnan(hi[0]) || std::isnan(lo[1]) || std::isnan(hi[1])) return false;
    for (int i = 0; i < 3; i++) { box[2 * i] = std::isnan(lo[i]) ? -1e9f : lo[i]; box[2 * i + 1] = std::isnan(hi[i]) ? 1e9f : hi[i]; }
    return true;
}
// CR39 (56 AMPHIBIOUS ASSAULT): the x, y of the first LOCATE_CHAR_ON_FOOT_2D (00ED) within 14 commands after p
bool PeekFootLocateXY(const uint8_t* p, float& x, float& y) {
    for (int k = 0; k < 14 && p && InSpace(p); k++) {
        const int op = *(const uint16_t*)p & 0x7FFF; const uint8_t* ops[3]{}; int cnt = 0; const uint8_t* e = CmdOps(p, ops, cnt); if (!e) return false;
        if (op == 0x00ED && cnt >= 3 && ops[1] && ops[2] && *ops[1] == 6 && *ops[2] == 6) { x = *(const float*)(ops[1] + 1); y = *(const float*)(ops[2] + 1); return true; }
        p = e;
    }
    return false;
}
int ModelWantedAfterInCarTest(const uint8_t* p) {
    if (!p || !InSpace(p) || (*(const uint16_t*)p & 0x7FFF) != 0x00DF) return -1;
    const uint8_t* q = p + 2; bool mf = false, isI = false; int32_t iv = 0;
    q = SkipOperand(q, mf, isI, iv); // CJ operand
    for (int k = 0; k < 6 && q && InSpace(q); k++) {
        const uint16_t op = *(const uint16_t*)q & 0x7FFF; int n = 0;
        if (op == 0x03C0 || op == 0x0137) n = 2; else if (op == 0x00D6 || op == 0x004D) n = 1; else return -1;
        const uint8_t* a = q + 2; const uint8_t* second = nullptr;
        for (int i = 0; i < n && a && InSpace(a); i++) { if (i == 1) second = a; a = SkipOperand(a, mf, isI, iv); }
        if (op == 0x0137 && second) { if (*second == 1) return *(const int32_t*)(second + 1); if (*second == 5) return *(const int16_t*)(second + 1); if (*second == 4) return *(const int8_t*)(second + 1); return -1; }
        q = a;
    }
    return -1;
}
CVehicle* NearestCarOfModel(CPed* c, int model, float best) {
    CVehicle* car = nullptr;
    for (auto& vv : GetVehiclePool()->GetAllValid()) {
        if ((int)(int16_t)vv.m_nModelIndex != model || vv.m_pDriver || vv.m_fHealth <= 0.f || vv.vehicleFlags.bIsLawEnforcer) continue;
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
CVehicle* SphereTarget(int centreHandle, int model) { // CR24: the car the game's own GET_RANDOM_CAR_IN_SPHERE_NO_SAVE would pick for a sphere centred on this vehicle (appearance 1/2, not a law enforcer, wanted model, CanBeDeleted: the mission's decoy is refused), nearest first
    auto* cv = CPools::GetVehicle(centreHandle); CVehicle* best = nullptr; float bd = 1e9f;
    if (cv) for (auto& vv : GetVehiclePool()->GetAllValid()) {
        if (&vv == cv || (vv.GetVehicleAppearance() != 1 && vv.GetVehicleAppearance() != 2) || vv.vehicleFlags.bIsLawEnforcer || (model >= 0 && (int)(int16_t)vv.m_nModelIndex != model) || !vv.CanBeDeleted()) continue;
        const float dd = (vv.GetPosition() - cv->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; best = &vv; }
    }
    return best;
}
bool MovingTrainNear(CPed* c) { for (auto& tv : GetVehiclePool()->GetAllValid()) if (tv.IsTrain() && tv.m_vecMoveSpeed.Magnitude() * 50.f > 3.f && (tv.GetPosition() - c->GetPosition()).Magnitude() < 80.f) return true; return false; } // CR35
// CR41 (62 TORENO'S LAST FLIGHT: `NOT locate CJ within R of the heli / its pilot`; the answers to the far destination locate and to the stage's pickup warped CJ 1.4..2 km from the heli = "Too late!"): a warp of CJ to (x, y) that lands outside 0.9 R of the ped (00F2..00F4 / 0104..0106, R >= 150) or car (0202..0207, R >= 30) the pass holds a `NOT locate CJ within R of it` leash on is not made: the target flies on, CJ stays with it (leashkeep)
bool CoordLeashLive() { // CR42 (61 ICE COLD KILLA): the pass holds a coordinate leash on CJ (a NOT locate of CJ of a point, 00EC..00F1 / 00FE..0103, R >= 150): the warp rows (answers that move CJ) give way to a plain IS_CHAR_DEAD kill row of the pass (Jizzy has to die; warps after the moving point never end the chase)
    if (std::getenv("NOTSA_MDRIVER_NOCOORDLEASH") || !Cj()) return false;
    for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; const bool d3 = rop >= 0x00FE && rop <= 0x0103, d2 = rop >= 0x00EC && rop <= 0x00F1; if (r->waitSeq < g_waitSeq - 3 || !(r->raw & 0x8000) || (!d2 && !d3) || r->args.size() < 3 || r->args[1].kind != 2 || r->args[2].kind != 2) continue; /* CR42: a MOVING point only (x y are variables); a fixed point is a place (20 LOS SEPULCROS) */ const auto h0 = ArgInt(*r, 0), rr = ArgFloat(*r, d3 ? 4 : 3); if (h0 && rr && *rr >= 150.0 && (CEntity*)CPools::GetPed((int)*h0) == (CEntity*)Cj()) return true; }
    return false;
}
bool LeashBlocksWarp(const Dec& d, CPed* c, float x, float y) {
    if (!c || std::getenv("NOTSA_MDRIVER_NOPEDLEASHWARP")) return false;
    const CVector wp{ x, y, 0.f };
    if (!std::getenv("NOTSA_MDRIVER_NOCOORDLEASH")) for (auto& r : g_recsBig) { // CR42 (61 ICE COLD KILLA): the same for a coordinate leash (`NOT locate CJ x y [z] R..`, 00EC..00F1 / 00FE..0103, R >= 150; the point is the fleeing Jizzy): a warp landing outside 0.9 R of the point is not made (the stale answer to an earlier stage's locate pulled CJ into the club interior = "Jizzy got away")
        const int rop = r->raw & 0x7FFF; const bool d3 = rop >= 0x00FE && rop <= 0x0103, d2 = rop >= 0x00EC && rop <= 0x00F1; if (r->waitSeq < g_waitSeq - 3 || !(r->raw & 0x8000) || (!d2 && !d3) || r->args.size() < 3 || r->args[1].kind != 2 || r->args[2].kind != 2) continue; /* CR42: a MOVING point only (x y are variables); a fixed point is a place (20 LOS SEPULCROS) */
        if (!std::getenv("NOTSA_MDRIVER_NOGROUPLEASH") && std::any_of(g_recsBig.begin(), g_recsBig.end(), [&](const RecP& q) { return q->fall == r->fall && q->waitSeq == r->waitSeq && q->p[-4] == 0xD6 && q->p[-3] == 0x00 && q->p[-2] == 0x04 && q->p[-1] >= 21 && q->p[-1] <= 27; })) continue; /* CR45 (66 MIKE TORENO: `OR(CJ at the blip, NOT CJ within 600 of the start)`): a NOT locate that is a member of an OR group is an alternative that ends the wait, no leash */
        const auto h0 = ArgInt(*r, 0), lx = ArgFloat(*r, 1), ly = ArgFloat(*r, 2), rr = ArgFloat(*r, d3 ? 4 : 3); if (!h0 || !lx || !ly || !rr || *rr < 150.0 || (CEntity*)CPools::GetPed((int)*h0) != (CEntity*)c) continue;
        const double dd = std::sqrt((*lx - x) * (*lx - x) + (*ly - y) * (*ly - y)); if (dd > 0.9 * *rr) { notsa::standalone::Fixups::Log("[mdriver] coordleashwarp: the warp to %.0f,%.0f lands %.0f m from the leash point %.0f,%.0f (R %.0f): not made", (double)x, (double)y, dd, *lx, *ly, *rr); { static std::map<std::string, int> s_lw2; const auto key = std::to_string(g_stopIp) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); if (++s_lw2[key] <= 60) --g_noProgress[key]; } return true; }
    }
    for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; if (r->waitSeq < g_waitSeq - 3 || !(r->raw & 0x8000) || !((rop >= 0x00F2 && rop <= 0x00F4) || (rop >= 0x0104 && rop <= 0x0106) || (rop >= 0x0202 && rop <= 0x0207))) continue;
        const bool carL = rop >= 0x0202; const auto h0 = ArgInt(*r, 0), h1 = ArgInt(*r, 1), rr = ArgFloat(*r, 2); if (!h0 || !h1 || !rr || *rr < (carL ? 30.0 : 150.0) || (CEntity*)CPools::GetPed((int)*h0) != (CEntity*)c) continue;
        CPhysical* lp = carL ? (CPhysical*)CPools::GetVehicle((int)*h1) : (CPhysical*)CPools::GetPed((int)*h1); if (!lp || lp == (CPhysical*)c || (carL ? ((CVehicle*)lp)->m_fHealth : ((CPed*)lp)->m_fHealth) <= 0.f || (c->bInVehicle && c->m_pVehicle == (CVehicle*)lp)) continue;
        if ((lp->GetPosition() - wp).Magnitude2D() > 0.9 * *rr) { notsa::standalone::Fixups::Log("[mdriver] leashwarp: the warp to %.0f,%.0f lands %.0f m from the leash target %d (R %.0f): not made", (double)x, (double)y, (double)(lp->GetPosition() - wp).Magnitude2D(), (int)*h1, *rr); { static std::map<std::string, int> s_lw; const auto key = std::to_string(g_stopIp) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); if (++s_lw[key] <= 60) --g_noProgress[key]; /* a blocked answer is no futile answer: the 8-answer stop does not count it (the chase goes on, leashkeep) */ } return true; }
    }
    return false;
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
        if (d.cond) { const auto o = LocatedOther(*d.cond); if (!o.empty()) {
            if (!o.compare(0, 2, "1:") && !std::getenv("NOTSA_MDRIVER_NOPLAYBACKWAIT")) if (auto* pv = CPools::GetVehicle(std::atoi(o.c_str() + 2)); pv && (pv->IsSubHeli() || pv->IsSubPlane()) && pv->m_fHealth > 0.f && pv != (c ? c->m_pVehicle : nullptr) && CVehicleRecording::IsPlaybackGoingOnForCar(pv)) { // CR41 (62 TORENO'S LAST FLIGHT: `LOCATE_CAR_3D(heli, landing pad)` of the helicopter that flies a recorded path (the script's own playback): the warp put it at the pad, the playback went on from there and the pilot left; CJ cannot keep up): a helicopter / plane in a recorded playback is waited for (it reaches the place by itself; a car in a playback is still warped: 23, 74)
                static std::map<int, int> s_pbWait; if (++s_pbWait[std::atoi(o.c_str() + 2)] <= (std::getenv("NOTSA_MDRIVER_NOPBFAST") ? 40 : 80)) { if (!std::getenv("NOTSA_MDRIVER_NOPBFAST")) { /* CR44 (62: the heli lands at 15179 and leaves 300 frames later, the next decision was up to 600 frames away): the next decision after 120 frames, the wait is no no-progress answer */ if ((pv->GetPosition() - CVector(F_(0), F_(1), F_(2))).Magnitude() < 350.f) { if (g_nextStall == 0) g_nextStall = 60; g_pbNearUntil = (int)CTimer::GetFrameCounter() + 900; } const auto pbKey = std::to_string(g_stopIp) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); --g_noProgress[pbKey]; } notsa::standalone::Fixups::Log("[mdriver] playbackwait: the located car %s is in a recorded playback: waited for, not warped", o.c_str()); QueueActions({}, row.settle); return; } }
            if (!o.compare(0, 2, "1:") && !std::getenv("NOTSA_MDRIVER_NOTRAINWAIT")) if (auto* tv = CPools::GetVehicle(std::atoi(o.c_str() + 2)); tv && tv->IsTrain()) { notsa::standalone::Fixups::Log("[mdriver] trainwait: the locate is about a train (%s): it runs to the place on its tracks, not warped", o.c_str()); QueueActions({}, row.settle); return; } // CR31 (29 WRONG SIDE OF THE TRACKS: `LOCATE_CAR_2D(train, ..)` was answered by warping the train 2 km along its line = derailed, mission failed): a train is waited for
            if (!o.compare(0, 2, "1:") && std::getenv("NOTSA_MDRIVER_LOCLOG")) if (auto* sv = CPools::GetVehicle(std::atoi(o.c_str() + 2))) { auto* dr = sv->m_pDriver; std::string tk; if (dr) { auto& tm = dr->GetIntelligence()->m_TaskMgr; for (int k = 0; k < 5; k++) for (CTask* t = tm.m_aPrimaryTasks[k]; t; t = t->GetSubTask()) tk += std::to_string((int)t->GetTaskType()) + (t->GetSubTask() ? ">" : "") + ","; } notsa::standalone::Fixups::Log("[mdriver] locatedcar %s at %.0f,%.0f,%.0f spd %.3f st %d mission %d driver %d (cj %d) tasks %s", o.c_str(), sv->GetPosition().x, sv->GetPosition().y, sv->GetPosition().z, sv->m_vecMoveSpeed.Magnitude(), (int)sv->GetStatus(), (int)sv->m_autoPilot.m_nCarMission, dr ? (int)dr->m_nModelIndex : -1, (int)(dr == c), tk.c_str()); } // CR42 debug
            if (!o.compare(0, 2, "1:") && !std::getenv("NOTSA_MDRIVER_NOSELFDRIVEWAIT")) if (auto* sv = CPools::GetVehicle(std::atoi(o.c_str() + 2)); sv && sv->m_pDriver && sv->m_pDriver != c && sv->m_pDriver->m_fHealth > 0.f && sv != (c ? c->m_pVehicle : nullptr) && !CVehicleRecording::IsPlaybackGoingOnForCar(sv) && sv->m_pDriver->GetIntelligence()->m_TaskMgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY)) { // CR42 (51 555 WE TIP: the escorted ped drives the car to the place by his own script task sequence (pause, drive, leave car); the vehwarp of the located car teleported it mid sequence): a located car with a living AI driver on a primary task is waited for (3 answers per car)
                static std::map<int, int> s_sd; if (++s_sd[std::atoi(o.c_str() + 2)] <= 3) { notsa::standalone::Fixups::Log("[mdriver] selfdrivewait: the located car %s has an AI driver on a task: waited for, not warped", o.c_str()); { static std::map<std::string, int> s_sd2; const auto key = std::to_string(g_stopIp) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); if (++s_sd2[key] <= 60) --g_noProgress[key]; } QueueActions({}, row.settle); return; } }
            if (c && !o.compare(0, 2, "0:") && (cop & 0x7FFF) == 0x00FE && !std::getenv("NOTSA_MDRIVER_NOGROUPLOC")) { // CR49 (88 DON PEYOTE: `LOCATE CJ box AND LOCATE girl1 box AND LOCATE girl2 box`, one answer per 660-frame round moved one of them and the girls walked off again): every false 00FE of the pass about the same box is answered at once: the other peds are moved there, CJ (with his car) last
                std::set<std::string> others; bool cjIn = false;
                for (auto& r : g_recsBig) { if (r->waitSeq < g_waitSeq - 3 || r->raw != 0x00FE || r->cond != 0) continue; const auto x = ArgFloat(*r, 1), y = ArgFloat(*r, 2), z = ArgFloat(*r, 3), h0 = ArgInt(*r, 0); if (!x || !y || !z || !h0) continue; if (std::fabs(*x - F_(0)) > 1.0 || std::fabs(*y - F_(1)) > 1.0 || std::fabs(*z - F_(2)) > 3.0) continue; auto* pp = CPools::GetPed((int)*h0); if (!pp || pp->m_fHealth <= 0.f) continue; if (pp == c) cjIn = true; else others.insert(std::to_string((int)*h0)); }
                if (others.size() >= 2 || (others.size() >= 1 && cjIn)) { std::vector<std::string> ga; for (auto& h : others) ga.push_back("emit mdwother:0:" + h + ":" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2))); if (cjIn) ga.push_back("emit mdwveh:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2))); notsa::standalone::Fixups::Log("[mdriver] grouploc: %zu peds%s are moved into the same locate box at once", others.size(), cjIn ? " and CJ" : ""); QueueActions(ga, row.settle); return; }
            }
            std::vector<std::string> acts{ "emit mdwother:" + o + ":" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) };
            if (!o.compare(0, 2, "1:") && c && !std::getenv("NOTSA_MDRIVER_NOLEASHJOIN")) { // CR30 (23 GRAY IMPORTS, chase): the car that is moved to the place carries a ped the pass keeps in a `ped near CJ` leash locate (00F2..00F4 / 0104..0106, plain or NOT, either operand order): CJ rides along, the leash (the script fails the mission when CJ is far from the dealer) stays true
                auto* mv = CPools::GetVehicle(std::atoi(o.c_str() + 2)); bool leash = false;
                if (mv) for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; if (r->waitSeq < g_waitSeq - 3 || !((rop >= 0x00F2 && rop <= 0x00F4) || (rop >= 0x0104 && rop <= 0x0106) || (rop >= 0x0202 && rop <= 0x0207 && !std::getenv("NOTSA_MDRIVER_NOCARLEASHJOIN")))) continue; const auto h0 = ArgInt(*r, 0), h1 = ArgInt(*r, 1); if (!h0 || !h1) continue; if (rop >= 0x0202) { if ((CEntity*)CPools::GetPed((int)*h0) == (CEntity*)c && CPools::GetVehicle((int)*h1) == mv) { leash = true; break; } continue; } /* CR41 (62 TORENO'S LAST FLIGHT: `LOCATE_CHAR_ANY_MEANS_CAR_3D CJ heli` about the very car that is moved, in either form): CJ rides along */ const bool cjSecond = EntityOfHandle((int)*h1, rop) == (CEntity*)c; if (!cjSecond && EntityOfHandle((int)*h0, rop) != (CEntity*)c) continue; auto* lp = CPools::GetPed((int)(cjSecond ? *h0 : *h1)); if (lp && lp != c && lp->bInVehicle && lp->m_pVehicle == mv) { leash = true; break; } }
                if (leash) acts.push_back("emit mdwveh:" + F(F_(0) + 6.f) + ":" + F(F_(1)) + ":" + F(F_(2)));
            }
            if (!o.compare(0, 2, "0:") && c && !std::getenv("NOTSA_MDRIVER_NOPEDJOIN")) { // CR30 (22 BURNING DESIRE: Denise is led home, the stage also tests `20 > distance(CJ, Denise)`): the pass holds a float compare of a value equal to the distance between CJ and the ped that is moved (the var of a GET_DISTANCE_BETWEEN_COORDS): CJ joins the ped at the place, so the distance test stays true
                auto* mp = CPools::GetPed(std::atoi(o.c_str() + 2)); bool dist = false;
                if (mp && mp != c) { const float dd = (mp->GetPosition() - c->GetPosition()).Magnitude(); for (auto& r : g_recsBig) { if (r->waitSeq < g_waitSeq - 3 || (r->raw != 0x0021 && r->raw != 0x0023) || r->args.size() < 2) continue; const float dv = AF(r->args[r->raw == 0x0021 ? 0 : 1]); if (dv > 20.f && std::fabs(dv - dd) < 1.5f) { dist = true; break; } } }
                if (dist) acts.push_back("emit mdwveh:" + F(F_(0) + 3.f) + ":" + F(F_(1)) + ":" + F(F_(2)));
            }
            if (c && !std::getenv("NOTSA_MDRIVER_NOWARPJOIN")) { // CR35 (39 BADLANDS: the snitch's drive to its next way point was answered by warping him 370 m away, the stage fails when CJ lost him for 30 s): a ped or car that was within 400 m of CJ and lands more than 15 m from him is followed by CJ (the chase the player's side of the warp amounts to)
                const bool isPedO = !o.compare(0, 2, "0:"); const int ho = std::atoi(o.c_str() + 2); CPhysical* mo = isPedO ? (CPhysical*)CPools::GetPed(ho) : (CPhysical*)CPools::GetVehicle(ho);
                if (mo && mo != (CPhysical*)c && isPedO && ((CPed*)mo)->bInVehicle && (int)((CPed*)mo)->m_nPedType < 24 /* a driving civilian-type ped target only (a mission buddy of type 24+ is moved without CJ: 11): a ped on foot or a bare car that is moved stays moved (11 14 20 23 28 passed with other frames) */ && !(c->bInVehicle && c->m_pVehicle == (CVehicle*)mo) && !(!isPedO && ((CVehicle*)mo)->IsTrain()) && (std::getenv("NOTSA_MDRIVER_NOJOINSELF") || !(c->bInVehicle && ((CPed*)mo)->m_pVehicle == c->m_pVehicle) /* CR49 (88 DON PEYOTE: the girl rides in CJ's own car; the warp moved it all, the join moved CJ and the girls 36 m off again) */)) {
                    const CVector mp = mo->GetPosition(), tp{ F_(0), F_(1), mo->GetPosition().z };
                    if ((mp - c->GetPosition()).Magnitude() < 400.f && (tp - c->GetPosition()).Magnitude2D() > 15.f && std::none_of(acts.begin() + 1, acts.end(), [](const std::string& a) { return a.rfind("emit mdwveh:", 0) == 0; })) { acts.push_back("emit mdwveh:" + F(F_(0) + (std::getenv("NOTSA_MDRIVER_JOINOFF") ? (float)std::atof(std::getenv("NOTSA_MDRIVER_JOINOFF")) : 36.f /* CR42 (52 SNAIL TRAIL: the guard that spawns 12 m from the place spots CJ within 30 m): 5 -> 36 */)) + ":" + F(F_(1)) + ":" + F(F_(2))); if (!std::getenv("NOTSA_MDRIVER_NOWARPFAST")) g_nextStall = 90; /* the chase goes on: the next way point is checked after 90 frames, not 600 */ notsa::standalone::Fixups::Log("[mdriver] warpjoin: CJ follows the moved %s %s", isPedO ? "ped" : "car", o.c_str()); }
                }
            }
            QueueActions(acts, row.settle); return; } } // LOCATE_CAR_3D(car 2049, ...) is satisfied by moving THAT car
        if (v == "vehwarp" && c && c->bInVehicle && c->m_pVehicle && c->m_pVehicle->IsSubPlane() && c->m_pVehicle->m_fHealth > 0.f && F_(2) - CWorld::FindGroundZForCoord(F_(0), F_(1)) < 20.f && c->m_pVehicle->GetPosition().z - CWorld::FindGroundZForCoord(c->m_pVehicle->GetPosition().x, c->m_pVehicle->GetPosition().y) > 20.f && !std::getenv("NOTSA_MDRIVER_NOLANDLOC")) { // CR50 (92 SAINT MARK'S BISTRO: `LOCATE_CHAR_IN_CAR_3D` of the airstrip in the flying Shamal, then `IS_CAR_STOPPED_IN_AREA_3D`): a vehicle warp of CJ's flying plane to a point on the ground is a landing (wheels down, at rest, repaired), the belly landing of the plain warp burnt the plane (1000 -> 0 in 60 frames)
            notsa::standalone::Fixups::Log("[mdriver] landloc: the ground target of the flying plane is a landing"); QueueActions({ "emit mdwland:" + F(F_(0)) + ":" + F(F_(1)) + ":-100" }, row.settle); return; }
        if (v == "vehwarp" && c && c->bInVehicle && c->m_pVehicle && c->m_pVehicle->IsSubPlane() && d.cond && (cop & 0x7FFF) == 0x0100 && !std::getenv("NOTSA_MDRIVER_NOAIRLEAD")) { // CR49 (91 FREEFALL: the in-car locate is a checkpoint that flies away with the target plane, 1.7 units per frame): the plane arrives at the box with the box's own velocity (taken from the last two passes of the same test) and the heading of that velocity
            const Rec* prev = nullptr; for (size_t k = g_recsBig.size(); k-- > 0;) { const auto& r = g_recsBig[k]; if (r->p == d.cond->p && r->waitSeq < d.cond->waitSeq - 1 && r->waitSeq >= d.cond->waitSeq - 12) { prev = r.get(); break; } }
            if (prev) { const auto x1 = ArgFloat(*prev, 1), y1 = ArgFloat(*prev, 2), x2 = ArgFloat(*d.cond, 1), y2 = ArgFloat(*d.cond, 2); const double dt = (double)(d.cond->waitSeq - prev->waitSeq);
                if (x1 && y1 && x2 && y2 && dt > 0.0) { const float vx = (float)((*x2 - *x1) / dt), vy = (float)((*y2 - *y1) / dt); if (std::hypot(vx, vy) > 0.2f && std::hypot(vx, vy) < 3.f) { const float hd = std::atan2(-vx, vy); notsa::standalone::Fixups::Log("[mdriver] airlead: the box moves %.2f,%.2f per frame: the plane arrives with that velocity, heading %.2f", (double)vx, (double)vy, (double)hd); QueueActions({ "emit mdwveh:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) + ":" + F(vx) + ":" + F(vy) + ":0:" + F(hd) }, row.settle); return; } } }
        }
        if (v == "vehwarp" && c && d.cond && (cop & 0x7FFF) == 0x0100 && ArgInt(*d.cond, 0) && CPools::GetPed((int)*ArgInt(*d.cond, 0)) == c && !std::getenv("NOTSA_MDRIVER_NOINMODEL2")) { // CR49: LOCATE_CHAR_IN_CAR_3D followed by IS_CHAR_IN_MODEL(CJ, m) (one or an OR group): CJ gets into the nearest car of one of those models first, then the car is put at the locate point
            const auto wms = ModelsWantedAfterCarLocate(d.cond->p);
            if (!wms.empty() && !(c->bInVehicle && c->m_pVehicle && std::find(wms.begin(), wms.end(), (int)c->m_pVehicle->m_nModelIndex) != wms.end())) {
                CVehicle* mcar = nullptr; float bd = 1.0e9f; for (const int wm : wms) if (auto* cand = NearestCarOfModel(c, wm, 1000.f)) { const float dd = (cand->GetPosition() - c->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; mcar = cand; } }
                if (mcar) { std::vector<std::string> acts; acts = EnterCarActions(mcar); acts.push_back("emit mdwveh:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2))); notsa::standalone::Fixups::Log("[mdriver] inmodel2: CJ gets into a car of model %d first", (int)mcar->m_nModelIndex); QueueActions(acts, row.settle); return; }
            }
        }
        if (v == "pedwarp" && c && d.cond && (cop & 0x7FFF) == 0x00FE && ArgInt(*d.cond, 0) && CPools::GetPed((int)*ArgInt(*d.cond, 0)) == c && !std::getenv("NOTSA_MDRIVER_NOINMODEL")) { // CR39: the locate is followed by IS_CHAR_IN_MODEL(CJ, model): CJ gets into the nearest free car of that model first, then the car is put at the locate point
            const int wm = ModelWantedAfterLocate(d.cond->p); auto* mcar = wm >= 0 ? NearestCarOfModel(c, wm, 600.f) : nullptr;
            if (mcar) { std::vector<std::string> acts; if (!(c->bInVehicle && c->m_pVehicle == mcar)) acts = EnterCarActions(mcar); acts.push_back("emit mdwveh:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2))); QueueActions(acts, row.settle); return; }
        }
        if (c && (v == "pedwarp" || v == "vehwarp") && LeashBlocksWarp(d, c, F_(0), F_(1))) { QueueActions({}, row.settle); return; } // CR41 (62 TORENO'S LAST FLIGHT)
        if (v == "pedwarp" && c && c->bInVehicle) { // CR10: actions.ts pedwarp on an ON_FOOT locate while CJ is in a vehicle: get out first (Enter, up to 300 frames; getOnFoot), then warp him (mission 77: 8x mdwveh on LOCATE_CHAR_ON_FOOT_3D)
            static const std::set<int> onFoot = { 0xE4, 0xE7, 0xEA, 0xED, 0xF0, 0xF3, 0xF6, 0xF9, 0xFC, 0xFF, 0x102, 0x105, 0x197, 0x19A, 0x19C, 0x19F, 0x1A1, 0x1A4, 0x1A6, 0x1A9, 0x1FD, 0x200, 0x203, 0x206, 0x27C, 0x27F, 0x282, 0x285, 0x288 };
            bool notInCarSib = false; if (!onFoot.count(cop & 0x7FFF) && d.cond && !std::getenv("NOTSA_MDRIVER_NONOTINCARSIB")) for (auto& r : g_recsBig) if (r->raw == 0x80DF && r->fall == d.cond->fall && r->waitSeq >= g_waitSeq - 3 && ArgInt(*r, 0) && CPools::GetPed((int)*ArgInt(*r, 0)) == c) { notInCarSib = true; break; } // CR42 (61 ICE COLD KILLA: the pickup stage is `locate CJ 1.2 m AND NOT IS_CHAR_IN_ANY_CAR CJ` of one condition group; the answer warped CJ with his car, the car rolled down the slope and burned): the locate is answered on foot like an ON_FOOT locate
            if (onFoot.count(cop & 0x7FFF) || notInCarSib) { QueueActions({ "key down return", "wait 1", "key up return", "waituntil invehicle == 0 300", "emit mdwveh:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) }, row.settle); return; } // CR11: after a 300-frame timeout CJ may still be in the vehicle: TS warp() then moves the CAR; mdwveh does that (ped warp when he is out)
        }
        one(warp(F_(0), F_(1), F_(2), v == "vehwarp")); return;
    }
    if (v == "joingroup") {
        std::vector<std::string> jacts; // CR39 (53 MOUNTAIN CLOUD BOYS: Woozie falls out of the group when CJ warps away; the group takes a follower only near its leader, the script re-recruits him when CJ is within 6 m): the ped comes next to CJ first
        if (auto* jp = CPools::GetPed(I_(0)); jp && c && jp != c && !std::getenv("NOTSA_MDRIVER_NOJOINFETCH") && (jp->GetPosition() - c->GetPosition()).Magnitude() > 10.f) { const CVector cp = c->GetPosition(); const CVector fw = c->bInVehicle && c->m_pVehicle ? c->m_pVehicle->GetForward() : c->GetForward(); const CVector q = cp - fw * 2.f; jacts.push_back("emit mdwother:0:" + std::to_string(I_(0)) + ":" + F(q.x) + ":" + F(q.y) + ":" + F(cp.z)); }
        jacts.push_back("emit mdjoingrp:" + std::to_string(I_(0)) + ":" + std::to_string(I_(1))); QueueActions(jacts, row.settle); return; } // CR26: IS_GROUP_MEMBER(ped, group) is false: the ped joins the group (what SET_GROUP_MEMBER does for a recruit)
    if (v == "nearfetch") { QueueActions({ "emit mdwother:0:" + std::to_string((int)F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) + ":" + F(F_(3)) }, row.settle); return; } // CR32
    if (v == "zonego") { one(warp(F_(0), F_(1), F_(2), false)); return; } // CR26
    if (v == "gangwar") { QueueActions({ "emit mdprovoke:3" }, row.settle); return; } // CR26
    if (v == "pickupgo") { // CR26: HAS_PICKUP_BEEN_COLLECTED(pickup) is false: the player walks onto the pickup (on foot, at its position)
        const int pi = CPickups::GetActualPickupIndex(tPickupReference{ I_(0) });
        if (pi < 0) { QueueActions({}, row.settle); return; }
        const CVector pp = CPickups::aPickUps[pi].GetPosn(); if (LeashBlocksWarp(d, Cj(), pp.x, pp.y)) { QueueActions({}, row.settle); return; } one(warp(pp.x, pp.y, pp.z, false)); return;
    }
    if (v == "weaponpickup") { // CR28: HAS_CHAR_GOT_WEAPON(cj, w) is false: the player walks onto the nearest live pickup whose model is the weapon's
        const auto mdl = CPickups::ModelForWeapon((eWeaponType)I_(0)); const auto* me = Cj(); const CPickup* best = nullptr; float bd = 1e9f;
        if (me && mdl > 0) for (auto& pk : CPickups::GetAllActivePickups()) { if (pk.m_nModelIndex != mdl || pk.m_nFlags.bDisabled) continue; const float dd = (pk.GetPosn() - me->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; best = &pk; } }
        if (!best) { QueueActions({}, row.settle); return; }
        const CVector pp = best->GetPosn(); one(warp(pp.x, pp.y, pp.z, false)); return;
    }
    if (v == "projectile") { // CR26: IS_PROJECTILE_IN_AREA(box) is false: a molotov is thrown into the area (the box centre); one per distinct area
        std::string a; for (size_t i = 0; i + 5 < d.args.size(); i += 6) { if (!a.empty()) a += "|"; a += F(0.5f * (F_(i) + F_(i + 3))) + "," + F(0.5f * (F_(i + 1) + F_(i + 4))) + "," + F(0.5f * (F_(i + 2) + F_(i + 5))); }
        QueueActions({ "emit mdproj:" + a }, row.settle); return;
    }
    if (v == "attachtrailer") { QueueActions({ "emit mdattach:" + std::to_string(I_(0)) + ":" + std::to_string(I_(1)) }, row.settle); return; } // CR37
    if (v == "explode") { // CR36: the script command ADD_EXPLOSION at the centre of the tested box (type -1 = any: a grenade)
        QueueActions({ "emit mdexplode:" + std::to_string((int)F_(0)) + ":" + F(0.5f * (F_(1) + F_(4))) + ":" + F(0.5f * (F_(2) + F_(5))) + ":" + F(0.5f * (F_(3) + F_(6))) }, row.settle); return;
    }
    if (v == "blipgo") { one(warp(F_(0), F_(1), F_(2), false)); return; } // CR26
    if (v == "midpoint") {
        float x, y, z = -100.f;
        if (d.args.size() >= 6) { x = (F_(0) + F_(3)) / 2; y = (F_(1) + F_(4)) / 2; z = (F_(2) + F_(5)) / 2; } else { x = (F_(0) + F_(2)) / 2; y = (F_(1) + F_(3)) / 2; }
        if (d.args.size() == 4 && d.cond && cop >= 0x05F6 && cop <= 0x05FB && !std::getenv("NOTSA_MDRIVER_NOANGLEDCENTRE")) { // CR28 (23 GRAY IMPORTS): the angled rectangle (x1,y1)-(x2,y2) has the AB segment as ONE side and extends `width` to one side (exe CalculateAngledAreaCorners: D = A + (sin a, -cos a) * width, a = angle(A,B) + pi/2): the middle of AB lies ON the border (float noise decides), the centre is half a width in from it
            if (const auto w = ArgFloat(*d.cond, 5)) { const float a = CGeneral::GetRadianAngleBetweenPoints(F_(0), F_(1), F_(2), F_(3)) + std::numbers::pi_v<float> / 2.f; x += std::sin(a) * (float)*w * 0.5f; y -= std::cos(a) * (float)*w * 0.5f; }
        }
        if (d.args.size() == 6 && d.cond && cop >= 0x05FC && cop <= 0x0601 && !std::getenv("NOTSA_MDRIVER_NOANGLEDCENTRE")) { // 3D variants: x1 y1 z1 x2 y2 z2 width (width = argument 7)
            if (const auto w = ArgFloat(*d.cond, 7)) { const float a = CGeneral::GetRadianAngleBetweenPoints(F_(0), F_(1), F_(3), F_(4)) + std::numbers::pi_v<float> / 2.f; x += std::sin(a) * (float)*w * 0.5f; y -= std::cos(a) * (float)*w * 0.5f; }
        }
        if (d.cond && !std::getenv("NOTSA_MDRIVER_NOAREAOTHER")) { // CR29 (22 BURNING DESIRE: `IS_CHAR_IN_AREA_3D(Denise, interior box)` in the OR group that lets the stage go on, CJ standing at the exit locate): the area test is about ANOTHER char (a group member): that ped is moved into the area, not CJ (who would leave his own locate)
            static const std::set<int> charArea = { 0x00A3, 0x00A4, 0x01A1, 0x01A2, 0x01A3, 0x01A4, 0x01A5, 0x01A6, 0x01A7, 0x01A8, 0x01A9, 0x01AA, 0x05F6, 0x05F7, 0x05F8, 0x05F9, 0x05FA, 0x05FB, 0x05FC, 0x05FD, 0x05FE, 0x05FF, 0x0600, 0x0601 };
            if (charArea.count(cop & 0x7FFF)) if (const auto h = ArgInt(*d.cond, 0)) if (auto* oth = CPools::GetPed((int)*h); oth && oth != c) { if (oth->bInVehicle && oth->m_pVehicle && oth->m_pVehicle->m_pDriver == oth && oth->m_pVehicle->IsMissionVehicle() && oth->m_pVehicle->m_vecMoveSpeed.Magnitude() * 50.f > 3.f && !std::getenv("NOTSA_MDRIVER_NOMOVINGAREA") && (oth->m_pVehicle->GetPosition() - CVector(x, y, z)).Magnitude() < MovingAreaM()) { QueueActions({}, 60); return; } /* CR47 (70 PUNCTURE WOUNDS: the chased car's driver must pass the spike strip's area; the warp of his moving car onto it threw him out): the driver of a moving mission car is waited for, not warped */ QueueActions({ "emit mdwother:0:" + std::to_string((int)*h) + ":" + F(x) + ":" + F(y) + ":" + F(z) }, row.settle); return; }
        }
        if (d.args.size() == 6 && d.cond && cop >= 0x05FC && cop <= 0x0601 && c && c->bInVehicle && c->m_pVehicle && !std::getenv("NOTSA_MDRIVER_NOFOLLOWAREA")) { // CR47 (76 HIGHJACK: `IS_CHAR_IN_ANGLED_AREA_IN_CAR_3D` of a box behind the moving truck, held for 3.5 s): the box moves with a vehicle (a moving vehicle of the area's centre, not CJ's own): CJ's vehicle follows it for 200 frames after the warp
            CVehicle* best = nullptr; float bd = 20.f; for (auto& vv : GetVehiclePool()->GetAllValid()) { if (&vv == c->m_pVehicle || vv.m_fHealth <= 0.f || vv.m_vecMoveSpeed.Magnitude() * 50.f < 3.f) continue; const float dd = std::hypot(vv.GetPosition().x - x, vv.GetPosition().y - y); if (dd < bd) { bd = dd; best = &vv; } }
            if (best) { QueueActions({ warp(x, y, z, false), "emit mdfollow:" + std::to_string(GetVehiclePool()->GetRef(best)) + ":200" }, row.settle); return; }
        }
        one(warp(x, y, z, false)); return;
    }
    if (v == "entercar" || v == "nearest_car") {
        CVehicle* car = nullptr;
        if (v == "nearest_car" && c && c->bInVehicle && c->m_pVehicle && cop == 0x00DF && d.cond && !(d.cond->raw & 0x8000) && !std::getenv("NOTSA_MDRIVER_NOBUDDYCAR")) { // CR45 (66 MIKE TORENO: `AND(CJ in a car, buddy @40 in a car, buddy @53 in a car)`, CJ already drives): the test is about ANOTHER live ped that stands outside: it is set in as a passenger of CJ's car (nearest_car answered CJ's own test or a free car)
            const auto hh = ArgInt(*d.cond, 0); auto* oth = hh ? CPools::GetPed((int)*hh) : nullptr;
            if (oth && oth != c && oth->m_fHealth > 0.f && !oth->bInVehicle) { QueueActions({ "emit mdwarppass:" + std::to_string((int)*hh) + ":" + std::to_string(CPools::GetVehicleRef(c->m_pVehicle)) }, row.settle); return; }
        }
        if (v == "entercar") { car = CPools::GetVehicle(I_(0)); if (!car) { Stop("driver action failed: entercar " + std::to_string(I_(0)) + ": no such car", cop, 0); return; } }
        else if (c) { if (cop == 0x00DF && d.cond && !std::getenv("NOTSA_MDRIVER_NOMODELCAR")) { const int wm = ModelWantedAfterInCarTest(d.cond->p); if (wm >= 0) car = NearestCarOfModel(c, wm, 400.f); } if (!car) car = NearestFreeCar(c, 50.f); if (!car && !std::getenv("NOTSA_MDRIVER_NOMISSIONCAR")) { float bd = 300.f; for (auto& vv : GetVehiclePool()->GetAllValid()) { if (!vv.IsMissionVehicle() || vv.m_pDriver || vv.m_fHealth <= 0.f) continue; const float dd = (float)(vv.GetPosition() - c->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; car = &vv; } } } /* CR39 (56 AMPHIBIOUS ASSAULT: the boat the script created is further than 50 m) */ if (!car && (int)FindPlayerWanted()->m_WantedLevel > 0 && !std::getenv("NOTSA_MDRIVER_NOLAWCAR")) car = NearestFreeCar(c, 150.f); if (!car && CoordLeashLive() && !std::getenv("NOTSA_MDRIVER_NOCOORDCAR")) { QueueActions({ "emit mdlaunch:492" }, row.settle); notsa::standalone::Fixups::Log("[mdriver] nearest_car: no free car and a coordinate leash is live: CJ gets a new car (492)"); return; } /* CR42 (61: the chase needs a car, none within 300 m) */ if (!car && cop == 0x00DF && v == "nearest_car" && d.cond && !(d.cond->raw & 0x8000) && c == FindPlayerPed() && !std::getenv("NOTSA_MDRIVER_NOSPAWNCAR")) { static int s_spawned = 0; if (s_spawned < 3) { ++s_spawned; QueueActions({ "emit mdlaunch:492" }, row.settle); notsa::standalone::Fixups::Log("[mdriver] nearest_car: no car anywhere near and the stage needs CJ in one: CJ gets a new car (492), %d", s_spawned); return; } } /* CR49 (88 DON PEYOTE, 90 THE MEAT BUSINESS: the script never creates the car CJ has to drive; a player takes one) */ if (!car) { Stop("by nature: no_car", cop, 0); return; } if ((int)FindPlayerWanted()->m_WantedLevel > 0 && !std::getenv("NOTSA_MDRIVER_NOLAWCAR")) g_wantedClrFrame = GFrame(); /* CR39 (50): a stopped CJ in a car with cops around is busted: the wanted level is dropped for the next 900 frames, as after the driver's own kills */ if (std::getenv("NOTSA_MDRIVER_CARLOG")) notsa::standalone::Fixups::Log("[mdriver] nearest_car: model %d law=%d wanted=%d dist %.0f", (int)car->m_nModelIndex, (int)car->vehicleFlags.bIsLawEnforcer, (int)FindPlayerWanted()->m_WantedLevel, (double)(car->GetPosition() - c->GetPosition()).Magnitude()); }
        if (!car || !c) { QueueActions({}, row.settle); return; }
        if (v == "nearest_car" && LeashBlocksWarp(d, c, car->GetPosition().x, car->GetPosition().y)) { QueueActions({}, row.settle); return; } // CR42 (61): the walk / warp to a far free car would leave a leash target (a coordinate leash too)
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
        if (auto* p = CPools::GetPed(h); p && p != c && c && (p->GetPosition() - c->GetPosition()).Magnitude() > KillFarM() && p->bInVehicle && p->m_pVehicle && p->m_pVehicle->IsMissionVehicle() && p->m_pVehicle->m_vecMoveSpeed.Magnitude() * 50.f > 3.f && !std::getenv("NOTSA_MDRIVER_NOKILLFAR")) { notsa::standalone::Fixups::Log("[mdriver] killfar refused h=%d dist=%.0f", h, (p->GetPosition() - c->GetPosition()).Magnitude()); QueueActions({}, 30); return; } // CR47 (70 PUNCTURE WOUNDS: the chased car's driver, 1300 m away and in his car, was killed at the first stall; the strip tests on him need him alive): the driver of a car out of reach is not killed
        if (auto* p = CPools::GetPed(h)) { if (p != c) cmd = "emit mdkillped:" + std::to_string(h); } else if (CPools::GetVehicle(h)) cmd = "emit mdkillcar:" + std::to_string(h);
        if (cmd.empty()) { QueueActions({}, 0); return; } // kill: no such char or car (TS returns without settling)
        std::vector<std::string> kacts{ cmd };
        if (d.cond && (d.cond->raw & 0x7FFF) == 0x0118 && !(d.cond->raw & 0x8000) && cmd.rfind("emit mdkillped", 0) == 0 && !std::getenv("NOTSA_MDRIVER_NODEADBOX")) { float bx[6]; if (PeekDeadBox(d.cond->p, bx)) { const float cx0 = (bx[0] + bx[1]) * 0.5f, cy0 = (bx[2] + bx[3]) * 0.5f; CStreaming::LoadScene(CVector{ cx0, cy0, 0.f }); CStreaming::LoadSceneCollision(CVector{ cx0, cy0, 0.f }); float bestx = cx0, besty = cy0, bestz = (bx[4] + bx[5]) * 0.5f - 1.1f, bd = 1e30f; // the point of the box (10 x 10 grid) whose ground lies inside the z range, nearest to the centre
        for (int gi = 1; gi < 10; gi++) for (int gj = 1; gj < 10; gj++) { const float px = bx[0] + (bx[1] - bx[0]) * gi / 10.f, py = bx[2] + (bx[3] - bx[2]) * gj / 10.f; CStreaming::LoadSceneCollision(CVector{ px, py, 0.f }); const float gz = CWorld::FindGroundZForCoord(px, py); if (gz + 1.1f < bx[4] || gz + 1.1f > bx[5]) continue; const float dd = (px - cx0) * (px - cx0) + (py - cy0) * (py - cy0); if (dd < bd) { bd = dd; bestx = px; besty = py; bestz = gz; } }
        notsa::standalone::Fixups::Log("[mdriver] deadbox: ped %d -> %.1f,%.1f,%.1f (box %.0f..%.0f %.0f..%.0f %.0f..%.0f)", h, bestx, besty, bestz, bx[0], bx[1], bx[2], bx[3], bx[4], bx[5]);
        kacts.insert(kacts.begin(), "emit mdwother:0:" + std::to_string(h) + ":" + F(bestx) + ":" + F(besty) + ":" + F(bestz)); } } // CR40
        if (d.cond && (d.cond->raw & 0x7FFF) == 0x0118 && !(d.cond->raw & 0x8000) && cmd.rfind("emit mdkillped", 0) == 0 && c && !std::getenv("NOTSA_MDRIVER_NOKILLGROUP")) { // CR36 (41 LOCAL LIQUOR STORE: `AND(dead robber1, dead robber2, dead robber3)` of the stage; the robbers flee in cars, one answer per 600-frame stall lets the third one drive out of the 300 m the script allows): the false IS_CHAR_DEAD tests of the SAME condition group (the same fall-through) are answered in the same action when the first target is out of CJ's reach (> 100 m)
            auto* t0 = CPools::GetPed(h); if (t0 && (t0->GetPosition() - c->GetPosition()).Magnitude() > 100.f) { std::set<int> done{ h };
                for (auto& q : g_recsBig) { if (q->waitSeq < g_waitSeq - 3 || q->raw != 0x0118 || q->cond != 0 || q->fall != d.cond->fall || !ArgInt(*q, 0)) continue; const int h2 = (int)*ArgInt(*q, 0); if (!done.insert(h2).second) continue; auto* p2 = CPools::GetPed(h2); if (!p2 || p2 == c || p2->m_fHealth <= 0.f || ProtectedHandle(h2) || GuardedOf(false).count(h2)) continue; kacts.push_back("emit mdkillped:" + std::to_string(h2)); notsa::standalone::Fixups::Log("[mdriver] killgroup: also ped %d", h2); } } }
        if (d.cond && (d.cond->raw & 0x7FFF) == 0x0118 && !(d.cond->raw & 0x8000) && cmd.rfind("emit mdkillped", 0) == 0 && c && std::getenv("NOTSA_MDRIVER_KILLALL")) { // CR43 (64 PIER 69: the roof gunmen @48..@50 have to die within the script's 40 s clock (two 600-frame stalls); the kill answer picked another false IS_CHAR_DEAD of the pass and the clock ran out): opt-in: every other distinct false plain IS_CHAR_DEAD of the last passes (never a guard / buddy; the true branch no fail) dies in the same answer
            std::set<int> done{ h }; for (auto& q : g_recsBig) { if (q->waitSeq < g_waitSeq - 3 || q->raw != 0x0118 || q->cond != 0 || !ArgInt(*q, 0)) continue; const int h2 = (int)*ArgInt(*q, 0); if (!done.insert(h2).second) continue; auto* p2 = CPools::GetPed(h2); if (!p2 || p2 == c || p2->m_fHealth <= 0.f || ProtectedHandle(h2) || GuardedOf(false).count(h2)) continue; const auto vq = VerdictOf(g_thread, *q); if (vq != Verdict::Wait && vq != Verdict::GoesOn) continue; kacts.push_back("emit mdkillped:" + std::to_string(h2)); notsa::standalone::Fixups::Log("[mdriver] killall: also ped %d", h2); } }
        QueueActions(kacts, row.settle); g_killFrame = GFrame(); g_killSettle = row.settle;
        if (d.cond && (d.cond->raw & 0x7FFF) == 0x0118 && !(d.cond->raw & 0x8000) && cmd.rfind("emit mdkillped", 0) == 0 && std::count_if(g_recsBig.begin(), g_recsBig.end(), [&](const RecP& q) { return q->waitSeq >= g_waitSeq - 3 && (q->raw == 0x0118) && q->cond == 0 && ArgInt(*q, 0) && (int)*ArgInt(*q, 0) != h; }) >= 2 && c && CPools::GetPed(h) && (CPools::GetPed(h)->GetPosition() - c->GetPosition()).Magnitude() < 50.f /* the killed char is near CJ (20: chars 80 and 1400 m away) */ && !std::getenv("NOTSA_MDRIVER_NOKILLFAST") && MovingTrainNear(c)) g_nextStall = 60; /* only while a moving train is next to CJ (the targets ride past: 18 and 89 changed their frames without it) */ /* CR35 (29): the pass tests several other chars dead (the four Vagos): the next one is answered after 60 frames, not 600 */
        return;
    }
    if (v == "key" && d.cond && DropsObjectBehind(*d.cond) && c && c->bInVehicle && c->m_pVehicle) { // CR47: the object is dropped 3 m behind CJ's car: CJ's car first goes onto the road AHEAD of the moving chased car (45 m along its heading, standing still), the press follows, the strip lies across its way
        CVehicle* best = nullptr; float bd = 6000.f; for (auto& vv : GetVehiclePool()->GetAllValid()) { if (&vv == c->m_pVehicle || vv.m_fHealth <= 0.f || !vv.m_pDriver || vv.m_pDriver == c || !vv.IsMissionVehicle() || vv.m_vecMoveSpeed.Magnitude() * 50.f < 3.f) continue; const float dd = (float)(vv.GetPosition() - c->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; best = &vv; } }
        if (best) {
            const float ahead = std::getenv("NOTSA_MDRIVER_STRIPAHEAD") ? (float)std::atof(std::getenv("NOTSA_MDRIVER_STRIPAHEAD")) : 22.f; const CVector q = best->GetPosition() + best->GetForward() * ahead; const int n = d.args.size() > 1 && !d.args[1].str ? (int)d.args[1].n : 8;
            notsa::standalone::Fixups::Log("[mdriver] strip ahead: chased vehicle %d (%.0f m/s) at %.0f,%.0f; CJ's car goes to %.0f,%.0f", GetVehiclePool()->GetRef(best), (double)(best->m_vecMoveSpeed.Magnitude() * 50.f), (double)best->GetPosition().x, (double)best->GetPosition().y, (double)q.x, (double)q.y);
            QueueActions({ "emit mdwveh:" + F(q.x) + ":" + F(q.y) + ":-100:0:0:0:" + F(best->GetHeading()), "wait 2", "emit mdpad:" + std::string("17") + ":" + std::to_string(n), "key down lctrl", "wait " + std::to_string(n), "key up lctrl" }, row.settle); g_nextStall = 200; return;
        }
    }
    if (v == "key") {
        std::string kk = d.args.empty() ? "" : d.args[0].str ? d.args[0].s : std::to_string((int)d.args[0].n); const int n0 = d.args.size() > 1 && !d.args[1].str ? (int)d.args[1].n : 8;
        std::string nm = KeyName(kk); int n = n0;
        if (kk == "16" && !std::getenv("NOTSA_MDRIVER_NOACCELKEY") && ((c && c->bInVehicle) || (FindPlayerInfo().m_pRemoteVehicle != nullptr))) { nm = "w"; if (std::getenv("NOTSA_MDRIVER_ACCELHOLD")) n = std::atoi(std::getenv("NOTSA_MDRIVER_ACCELHOLD")); } // CR47 (73 SUPPLY LINES: IS_BUTTON_PRESSED 16 = the cross button; in a vehicle / in remote mode that is VEHICLE_ACCELERATE (W), the RC plane's throttle; the space key is the handbrake): the real key follows the driving binding
        if (nm.empty() && !kk.empty() && std::isdigit((unsigned char)kk[0]) && !std::getenv("NOTSA_MDRIVER_NOPADONLY")) { QueueActions({ "emit mdpad:" + kk + ":" + std::to_string(n), "wait " + std::to_string(n) }, row.settle); return; } // CR27: a pad button without a keyboard binding (13 = SELECT): the pad state alone (IS_BUTTON_PRESSED reads it)
        if (nm.empty()) { Stop("unmapped-key " + kk, cop, 0); return; }
        if (!kk.empty() && std::isdigit((unsigned char)kk[0])) { QueueActions({ "emit mdpad:" + kk + ":" + std::to_string(n), (kk == "16" && (CMenuSystem::MenuInUse[0] || CMenuSystem::MenuInUse[1]) && !std::getenv("NOTSA_MDRIVER_NOMENUACCEPT")) ? "emit mdmenu:0" : "wait 0", "key down " + nm, "wait " + std::to_string(n), "key up " + nm }, row.settle); return; } // CR4: numeric pad button: the pad state itself, as TS (PadInput.joy[field] = 255)
        QueueActions({ "key down " + nm, "wait " + std::to_string(n), "key up " + nm }, row.settle); return;
    }
    if (v == "skipcut") { QueueActions(CCutsceneMgr::IsCutsceneProcessing() ? std::vector<std::string>{ "emit mdskipcut:0" } : std::vector<std::string>{}, row.settle); return; }
    if (v == "distfetch" && !d.args.empty()) { // CR30: see Decide (kDist): the ped comes next to CJ
        if (auto* oth = CPools::GetPed((int)d.args[0].n); oth && c && oth != c) { const CVector cp = c->GetPosition(); const bool veh = oth->bInVehicle && oth->m_pVehicle; const CVector fw = c->bInVehicle && c->m_pVehicle ? c->m_pVehicle->GetForward() : c->GetForward(); const CVector q = cp - fw * (veh ? 8.f : 2.f);
            QueueActions({ "emit mdwother:0:" + std::to_string((int)d.args[0].n) + ":" + F(q.x) + ":" + F(q.y) + ":" + F(cp.z) }, row.settle); return; }
        QueueActions({}, 0); return;
    }
    if (v == "trainnear" && !d.args.empty()) { // CR41: see Decide (kTN): CJ (with his vehicle) joins the train, beside the tracks, with the train's velocity
        if (auto* tv = CPools::GetVehicle((int)d.args[0].n); tv && c) { const CVector tp = tv->GetPosition(), q = tp + tv->GetRight() * 14.f; const auto sp = tv->m_vecMoveSpeed; const CVector fw = tv->GetForward();
            if (c->bInVehicle && c->m_pVehicle) QueueActions({ "emit mdwveh:" + F(q.x) + ":" + F(q.y) + ":-100:" + F(sp.x) + ":" + F(sp.y) + ":" + F(sp.z) + ":" + F(std::atan2(-fw.x, fw.y)) }, row.settle);
            else QueueActions({ "emit mdwped:" + F(q.x) + ":" + F(q.y) + ":-100" }, row.settle);
            notsa::standalone::Fixups::Log("[mdriver] trainnear: CJ joins the train at %.0f,%.0f (speed %.1f)", (double)q.x, (double)q.y, (double)(sp.Magnitude() * 50.f)); return; }
        QueueActions({}, 0); return;
    }
    if (v == "warpto_handle") {
        const int h = I_(0); CEntity* e = EntityOfHandle(h, cop);
        if (!e) { QueueActions({}, 0); return; }
        if (c && d.cond && cop == 0x0204 && !(d.cond->raw & 0x8000) && std::getenv("NOTSA_MDRIVER_FOREIGNCARLOC")) { const auto h0 = ArgInt(*d.cond, 0); auto* tc = h0 ? CPools::GetPed((int)*h0) : nullptr; if (tc && tc != c && tc->bInVehicle) { /* CR42 (60 OUTRIDER: `LOCATE_CHAR_IN_CAR_CAR_2D(van driver, escort car, 50)`: the tested char is the van's driver, not CJ; the answer warped CJ's bike beside the escort car, the van arrived at the ambush and was destroyed): not CJ's business: the van drives the route itself, the answer waits */ notsa::standalone::Fixups::Log("[mdriver] foreigncarloc: LOCATE_CHAR_IN_CAR_CAR_2D of ped %d (not CJ): no warp of CJ", (int)*h0); static std::map<std::string, int> s_fc; const auto key = std::to_string(g_stopIp) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); if (++s_fc[key] <= 60) --g_noProgress[key]; QueueActions({}, row.settle); return; } }
        const bool futileSelf = e == (CEntity*)c && c && d.cond && !(d.cond->raw & 0x8000) && !std::getenv("NOTSA_MDRIVER_NOFETCH2") && g_noProgress[std::to_string(g_stopIp) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args)] >= 2; // CR30 (23 GRAY IMPORTS): the plain locate `ped near CJ` answered twice by a CJ warp onto himself (the target of the row IS CJ): the ped has to come to CJ
        if (e == (CEntity*)c && c && d.cond && ((d.cond->raw & 0x8000) || futileSelf) && !std::getenv("NOTSA_MDRIVER_NOFETCH")) { // only for the TRUE NOT-form blocker of a wait group (a plain false helper test of the buddy helper in 11 must not move the buddy) // CR24 (14 CLEANING THE HOOD: `NOT LOCATE_CHAR_ANY_MEANS_CHAR_3D(Sweet, CJ, 30 m)`): the target of the locate IS CJ and the tested char is another one: that char is the one that has to move (to CJ), as LocatedOther does for the place locates; CJ warping onto himself changes nothing
            const auto h0 = ArgInt(*d.cond, 0);
            if (h0) if (auto* oth = CPools::GetPed((int)*h0); oth && oth != c) {
                const CVector cp = c->GetPosition(); const bool veh = oth->bInVehicle && oth->m_pVehicle; const CVector fw = c->bInVehicle && c->m_pVehicle ? c->m_pVehicle->GetForward() : c->GetForward();
                const CVector q = cp - fw * (veh ? 8.f : 2.f);
                QueueActions({ "emit mdwother:0:" + std::to_string((int)*h0) + ":" + F(q.x) + ":" + F(q.y) + ":" + F(cp.z) }, row.settle); return;
            }
        }
        const auto pos = e->GetPosition();
        if (!std::getenv("NOTSA_MDRIVER_NOLEASHWARP") && c && e != (CEntity*)c) { // CR35 (29 WRONG SIDE OF THE TRACKS: the stage tests `ped near the front carriage` (165 m) while a guard `NOT locate CJ within 200 m of the train` ends the mission; the answer warped CJ to the front carriage 360 m from the train = failed): a warp of CJ that lands outside 0.9 R of a car the pass holds a NOT-locate leash on (CJ the first operand, a car 0202..0207, R >= 30) is not made
            for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; const bool pedL = ((rop >= 0x00F2 && rop <= 0x00F4) || (rop >= 0x0104 && rop <= 0x0106)) && !std::getenv("NOTSA_MDRIVER_NOPEDLEASHWARP"); /* CR41 (62 TORENO'S LAST FLIGHT: `NOT locate CJ within R of Toreno's helicopter pilot` (R >= 150); the answer to the far destination locate warped CJ 2 km from the heli = "Too late!") */ if (r->waitSeq < g_waitSeq - 3 || !(r->raw & 0x8000) || ((rop < 0x0202 || rop > 0x0207) && !pedL)) continue; const auto h0 = ArgInt(*r, 0), h1 = ArgInt(*r, 1), rr = ArgFloat(*r, 2); if (!h0 || !h1 || !rr || *rr < (pedL ? 150.0 : 30.0) || (CEntity*)CPools::GetPed((int)*h0) != (CEntity*)c) continue;
                auto* lped = pedL ? CPools::GetPed((int)*h1) : nullptr; auto* lv = pedL ? (lped && lped != c ? (lped->bInVehicle && lped->m_pVehicle ? lped->m_pVehicle : nullptr) : nullptr) : CPools::GetVehicle((int)*h1); if (pedL && lped && lped != c && !lv && lped->m_fHealth > 0.f) { if ((lped->GetPosition() - pos).Magnitude2D() > 0.9 * *rr) { notsa::standalone::Fixups::Log("[mdriver] leashwarp: the warp onto handle %d lands %.0f m from the leash ped %d (R %.0f): not made", h, (double)(lped->GetPosition() - pos).Magnitude2D(), (int)*h1, *rr); QueueActions({}, row.settle); return; } continue; }
                if (!lv || lv == (CVehicle*)e || lv->m_fHealth <= 0.f) continue; if ((lv->GetPosition() - pos).Magnitude2D() > 0.9 * *rr) { notsa::standalone::Fixups::Log("[mdriver] leashwarp: the warp onto handle %d lands %.0f m from the leash car %d (R %.0f): not made", h, (double)(lv->GetPosition() - pos).Magnitude2D(), (int)*h1, *rr); QueueActions({}, row.settle); return; } }
        }
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
        if (c && c->bInVehicle && c->m_pVehicle && e->GetIsTypePed() && !std::getenv("NOTSA_MDRIVER_NOPEDCLEAR")) { // CR50 (88 DON PEYOTE: the warp of CJ's car +0.5 m beside a PED (the girl on foot among the enemy cars) overlapped other cars: the car lost 690 health in 60 frames and burnt): when the old +0.5 m spot is not clear for the car, the first clear spot around the ped (behind / left / right / ahead of CJ's car heading, 6 m) is used
            auto* ocm = c->m_pVehicle->GetColModel();
            if (ocm) {
                const auto ob = ocm->GetBoundingBox(); const float ol = (ob.m_vecMax.y - ob.m_vecMin.y) * 0.5f, ow = (ob.m_vecMax.x - ob.m_vecMin.x) * 0.5f, rad = std::max(ol, ow) + 0.5f;
                const auto Clear = [&](const CVector& q) { return !CWorld::TestSphereAgainstWorld(CVector{ q.x, q.y, q.z + 0.6f }, rad, c->m_pVehicle, false, true, false, true, false, false) && !CWorld::TestSphereAgainstWorld(CVector{ q.x, q.y, q.z + 1.4f }, 1.0f, c->m_pVehicle, true, false, false, false, false, false); };
                if (!Clear(CVector{ pos.x + 0.5f, pos.y, pos.z })) {
                    const CVector fw = c->m_pVehicle->GetForward(), rt = c->m_pVehicle->GetRight(); const float dl = ol + 1.5f, dw = ow + 1.5f;
                    const CVector cand[8] = { pos - fw * dl, pos - rt * dw, pos + rt * dw, pos + fw * dl, pos - fw * (dl + 4.f), pos - rt * (dw + 4.f), pos + rt * (dw + 4.f), pos + fw * (dl + 4.f) };
                    for (const auto& q : cand) { if (!Clear(q)) continue; notsa::standalone::Fixups::Log("[mdriver] warpto_handle: clear spot beside ped %d (%.2f,%.2f) -> (%.2f,%.2f)", h, pos.x, pos.y, q.x, q.y); one(warp(q.x, q.y, q.z, false)); return; }
                }
            }
        }
        notsa::standalone::Fixups::Log("[mdriver] warpto_handle: no clear spot, +0.5 m");
        one(warp(pos.x + 0.5f, pos.y, pos.z, false)); return;
    }
    if (v == "photo") { QueueActions({ "emit mdphoto:" + std::to_string(I_(0)) }, row.settle); if (!std::getenv("NOTSA_MDRIVER_NOEARLY") && !std::getenv("NOTSA_MDRIVER_NOZOOM") && PhotoWantsZoom()) g_nextStall = 90, g_photoEarly = true; /* CR26: a timed (zoomed) photo condition: the next decision after 90 frames, not 600 (the walker's task ends sooner) */ return; } // CR25: HAS_CHAR_BEEN_PHOTOGRAPHED(ped): the camera shot of the ped (the flag CWeapon::TakePhotograph sets for a ped in frame, in range and in line of sight)
    if (v == "carspeed") { QueueActions({ "emit mdspeed:" + std::to_string(I_(0)) + ":" + F(F_(1)) }, row.settle); return; } // CR34
    if (v == "hurt") { QueueActions({ "emit mdhurt:" + std::to_string(I_(0)) + ":" + std::to_string(I_(1)) }, row.settle); return; } // CR35
    if (v == "carfix") { QueueActions({ "emit mdfixcar:" + std::to_string(I_(0)) }, row.settle); return; } // CR34
    if (v == "smashobj") { // CR38: for each (x, y, z, radius, model, smashed, damaged) set: CJ is warped to the object (an IPL dummy far from him is no CObject yet; the load scene streams it in), then the object is damaged
        std::vector<std::string> acts;
        for (size_t k = 0; k + 7 <= d.args.size(); k += 7) {
            const auto w = warp(F_(k), F_(k + 1), F_(k + 2), false); if (!w.empty()) acts.push_back(w);
            acts.push_back("emit mdsmash:" + F(F_(k)) + ":" + F(F_(k + 1)) + ":" + F(F_(k + 2)) + ":" + F(F_(k + 3)) + ":" + std::to_string(I_(k + 4)) + ":" + std::to_string(I_(k + 5)) + ":" + std::to_string(I_(k + 6)));
        }
        QueueActions(acts, row.settle); return; }
    if (v == "cardmg" && !d.args.empty()) { // CR43: see Decide (kCarDmg): CJ shoots the car: the car's last damager is CJ and its health drops to 255 (a mission that reads the health for the wreck: 62's helicopter crashes below 260)
        if (auto* tv = CPools::GetVehicle((int)d.args[0].n); tv && c) { tv->m_pLastDamageEntity = c; if (tv->m_fHealth > 255.f) tv->m_fHealth = 255.f; notsa::standalone::Fixups::Log("[mdriver] cardmg: car %d model %d health %.0f damaged by CJ", (int)d.args[0].n, (int)tv->m_nModelIndex, (double)tv->m_fHealth); }
        QueueActions({}, row.settle); return;
    }
    if (v == "killset") { // CR43: see Decide (flagkill): every listed ped dies, every listed car is wrecked
        std::vector<std::string> acts; for (auto& a : d.args) { const int h = (int)a.n; if (h < 0) acts.push_back("emit mdkillcar:" + std::to_string(-h)); else if (auto* p = CPools::GetPed(h); p && p != c) acts.push_back("emit mdkillped:" + std::to_string(h)); }
        QueueActions(acts, row.settle); if (!acts.empty()) { g_killFrame = GFrame(); g_killSettle = row.settle; if (!std::getenv("NOTSA_MDRIVER_NOFLAGKILLFAST")) g_nextStall = 40; /* the script clears the flag at the FIRST live member of its loop: the next one shows in the next pass */ } return;
    }
    if (v == "setwanted") { if (auto* w = FindPlayerWanted()) { w->SetWantedLevel((eWantedLevel)std::clamp(I_(0), 1, 6)); } QueueActions({ "wait 30" }, row.settle); return; } // CR46
    if (v == "horn") { QueueActions({ "emit mdpad:-100:30", "wait 30" }, row.settle); return; } // CR39
    if (v == "swimgo") { std::vector<std::string> acts; if (c && c->bInVehicle) { acts = { "key down return", "wait 1", "key up return", "waituntil invehicle == 0 300" }; } if (!c || !c->bInVehicle) acts.push_back("emit mdswim:" + F(F_(0)) + ":" + F(F_(1))); QueueActions(acts, row.settle); return; } // CR39 (CJ leaves the boat in the water: he swims)
    if (v == "objto") { QueueActions({ "emit mdobjto:" + std::to_string(I_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) + ":" + F(F_(3)) }, row.settle); return; } // CR39
    if (v == "planeland") { QueueActions({ "emit mdwland:" + F(F_(0)) + ":" + F(F_(1)) + ":" + F(F_(2)) }, row.settle); return; } // CR49 (91 FREEFALL: `IS_CAR_STOPPED_IN_AREA_3D plane box OR IS_CHAR_IN_AREA_3D CJ box`: land the plane)
    if (v == "damageobjw") { std::vector<std::string> acts; for (size_t k = 0; k + 2 <= d.args.size(); k += 2) acts.push_back("emit mddmgobj:" + std::to_string((int)d.args[k].n) + ":" + std::to_string((int)d.args[k + 1].n)); QueueActions(acts, row.settle); return; } // CR49
    if (v == "damageobj") { QueueActions({ "emit mddmgobj:" + std::to_string(I_(0)) }, row.settle); return; } // CR22: HAS_OBJECT_BEEN_DAMAGED(obj): CJ damages the object (16 NINES AND AK'S shooting range)
    if (v == "tosphere") { // CR24: the vehicle at the centre of a GET_RANDOM_CAR_IN_SPHERE search is moved next to the nearest car the game's own search would accept (appearance 1/2, not a law enforcer, wanted model, CanBeDeleted: the mission's decoy is refused), 8 m above it
        CVehicle* best = SphereTarget(g_sphCur.veh, g_sphCur.model);
        if (!best) { QueueActions({}, row.settle); return; }
        // the search needs the car within the radius NOW (warp 1: on top of it); the script holds the remote vehicle for 4 s before the bomb button counts, and a cruising car is gone by then: warp 2 puts the vehicle where the car will be at the end of the hold (position + velocity * 4.3 s), the next decision comes 128 frames later
        const auto bp = best->GetPosition(); const CVector vel = best->m_vecMoveSpeed * 50.f * 4.3f; const bool moving = vel.Magnitude() > 3.f && !std::getenv("NOTSA_MDRIVER_NOINTERCEPT");
        std::vector<std::string> acts{ "emit mdwother:1:" + std::to_string(g_sphCur.veh) + ":" + F(bp.x) + ":" + F(bp.y) + ":" + F(bp.z + 3.f) };
        if (moving) { acts.push_back("wait 8"); acts.push_back("emit mdtosphere:" + std::to_string(g_sphCur.veh) + ":" + std::to_string(g_sphCur.model)); }
        QueueActions(acts, 0); g_nextStall = 128; return;
    }
    if (v == "tagarea") { QueueActions({ "emit mdtag:" + F(g_tagRect[0]) + ":" + F(g_tagRect[1]) + ":" + F(g_tagRect[2]) + ":" + F(g_tagRect[3]) }, row.settle); return; } // CR22: the script waits for the tagged percentage of an area: spray every tag of it (13 TAGGING UP TURF)
    if (v == "enterarea") { // CR23: stand on the entrance marker of the nearest exterior entry exit (area 0) whose linked interior side is in area g_enterArea (CEntryExitManager does the rest: fade, area switch)
        auto* me = Cj(); CEntryExit* best = nullptr; float bd = 1e30f;
        if (me) for (auto& ex : CEntryExitManager::GetPool()->GetAllValid()) { if (g_enterArea == 0 ? ((int)ex.m_nArea != (int)CGame::currArea || ex.m_nArea == 0 || !ex.m_pLink || (int)ex.m_pLink->m_nArea != 0) /* leave: the exit marker of the interior CJ is in */ : ((int)ex.m_nArea != 0 || !ex.m_pLink || (int)ex.m_pLink->m_nArea != g_enterArea)) continue; const auto c = ex.GetPosition(); const float dd = (c - me->GetPosition()).Magnitude(); const float key = dd + (g_enexNames.count(std::string(ex.m_szName, strnlen(ex.m_szName, 8))) || g_enexNames.count(std::string(ex.m_pLink ? ex.m_pLink->m_szName : "", ex.m_pLink ? strnlen(ex.m_pLink->m_szName, 8) : 0)) ? 0.f : 1e6f) + (ex.bEnableAccess || std::getenv("NOTSA_MDRIVER_NOENABLEDENEX") ? 0.f : 5e5f); /* CR32 (32 MADD DOGG'S RHYMES: the nearer MADDOGS gate is switched off, the script enabled MDDOGS) */ if (key < bd) { bd = key; best = &ex; } } // CR27: an entrance whose name the mission compares with wins over the nearest one
        if (g_enterEx) { best = g_enterEx; g_enterEx = nullptr; bd = 0.f; }
        if (!best) { QueueActions({}, 0); return; }
        const auto c = best->GetPosition(); float dx = c.x - best->m_vecExitPos.x, dy = c.y - best->m_vecExitPos.y; const float len = std::max(0.01f, (float)std::hypot(dx, dy));
        notsa::standalone::Fixups::Log("[mdriver] enterarea %d: entry exit %.8s at %.1f,%.1f,%.1f (%.0f m from CJ)", g_enterArea, best->m_szName, c.x, c.y, c.z, bd);
        std::vector<std::string> acts; if (me && me->bInVehicle && !std::getenv("NOTSA_MDRIVER_NOEXITFIRST")) acts = { "key down return", "wait 1", "key up return", "waituntil invehicle == 0 300" }; // CR24 (14: CJ drives Sweet's car to the crack den): an entry exit is entered on foot, the warp of a driver only moves the car: get out first
        acts.push_back("pedwarp " + F(c.x) + " " + F(c.y) + " " + F(c.z));
        QueueActions(acts, row.settle); return;
    }
    if (v == "failclass" && d.cond && (cop == 0x04E5 || cop == 0x04E6) && !d.args.empty() && d.args[0].str && d.args[0].s == "object-must-move" && !std::getenv("NOTSA_MDRIVER_NOOBJMOVE")) { // CR29 (26 ROBBING UNCLE SAM: `LOCATE_OBJECT_3D(crate, offset of the truck, ..)`): the script object is put at the centre of the box (the way warpOther moves a car for LOCATE_CAR_3D)
        const auto h = ArgInt(*d.cond, 0); const bool d3 = cop == 0x04E6; const auto x = ArgFloat(*d.cond, 1), y = ArgFloat(*d.cond, 2), z = d3 ? ArgFloat(*d.cond, 3) : std::optional<double>(-100.0);
        if (h && x && y && z && CPools::GetObject((int)*h)) { QueueActions({ "emit mdwobj:" + std::to_string((int)*h) + ":" + F((float)*x) + ":" + F((float)*y) + ":" + F((float)*z) }, 60); return; }
    }
    if (v == "failclass" && d.cond && (cop == 0x04A9 || cop == 0x04AB) && !(d.cond->raw & 0x8000) && c && !std::getenv("NOTSA_MDRIVER_NOANYKIND")) { // CR49 (93 MISAPPROPRIATION: `IS_CHAR_IN_ANY_HELI CJ`): CJ gets into the nearest helicopter (plane) in the world, or a new one is put under him
        const bool heli = cop == 0x04A9; CVehicle* best = nullptr; float bd = 2500.f;
        for (auto& vv : GetVehiclePool()->GetAllValid()) { if (vv.m_fHealth <= 0.f || (vv.m_pDriver && vv.m_pDriver != c) || (heli ? !vv.IsSubHeli() : !vv.IsSubPlane())) continue; const float dd = (vv.GetPosition() - c->GetPosition()).Magnitude(); if (dd < bd) { bd = dd; best = &vv; } }
        static int s_spawnedKind = 0;
        if (best) { if (!(c->bInVehicle && c->m_pVehicle == best)) { notsa::standalone::Fixups::Log("[mdriver] anykind: CJ is put into the nearest %s (model %d, %.0f m)", heli ? "helicopter" : "plane", (int)best->m_nModelIndex, (double)bd); QueueActions({ "emit mdwarpin:" + std::to_string(GetVehiclePool()->GetRef(best)) }, row.settle); return; } }
        else if (s_spawnedKind < 2) { ++s_spawnedKind; notsa::standalone::Fixups::Log("[mdriver] anykind: no %s anywhere: a new one is put under CJ", heli ? "helicopter" : "plane"); QueueActions({ "emit mdlaunch:" + std::string(heli ? "487" : "513") }, row.settle); return; }
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
            case TASK_SIMPLE_GANG_DRIVEBY: if (std::getenv("NOTSA_MDRIVER_DBLOG")) { static uint32_t s_lastDb = 0; if (CTimer::GetFrameCounter() - s_lastDb > 90) { s_lastDb = CTimer::GetFrameCounter(); auto* te0 = static_cast<CTaskSimpleGangDriveBy*>(t)->m_pTargetEntity; notsa::standalone::Fixups::Log("[dblog] frame %u attacker h=%d model %d type %d inveh=%d vmodel %d | target %s h=%d model %d type %d | CJ hp %.0f vhp %.0f", (unsigned)CTimer::GetFrameCounter(), CPools::GetPedRef(ped), (int)ped->m_nModelIndex, (int)ped->m_nPedType, (int)ped->bInVehicle, ped->bInVehicle && ped->m_pVehicle ? (int)ped->m_pVehicle->m_nModelIndex : -1, te0 ? (te0->GetIsTypePed() ? "ped" : "veh/other") : "none", te0 && te0->GetIsTypePed() ? CPools::GetPedRef(static_cast<CPed*>(te0)) : -1, te0 ? (int)te0->m_nModelIndex : -1, te0 && te0->GetIsTypePed() ? (int)static_cast<CPed*>(te0)->m_nPedType : -1, Cj() ? (double)Cj()->m_fHealth : -1.0, Cj() && Cj()->bInVehicle && Cj()->m_pVehicle ? (double)Cj()->m_pVehicle->m_fHealth : -1.0); } } /* CR50 debug */
            if (!std::getenv("NOTSA_MDRIVER_NODRIVEBYKILL") && (std::getenv("NOTSA_MDRIVER_DRIVEBYALWAYS") || (Cj() && Cj()->m_fHealth < 70.f /* only when the shooting hurts CJ: 11 BIG SMOKE lets the Ballas of the first drive-by live */))) { auto* te = static_cast<CTaskSimpleGangDriveBy*>(t)->m_pTargetEntity; if (te && te->GetIsTypePed()) tg = static_cast<CPed*>(te); else if (std::getenv("NOTSA_MDRIVER_DRIVEBYVEH") && te && te->GetIsTypeVehicle() && static_cast<CVehicle*>(te)->m_pDriver) tg = static_cast<CVehicle*>(te)->m_pDriver; /* (opt-in: 11 BIG SMOKE failed when the gunmen of cars that shoot at a car were shot dead) */ } break; // CR49 (86 YOU'VE HAD YOUR CHIPS: the Admiral's passengers drive-by CJ with TASK_DRIVE_BY; the kill-ped tasks were the only attackers the driver shot dead)
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
    const std::set<int>& guarded = GuardedOf(false);
    for (auto& p : GetPedPool()->GetAllValid()) {
        if (&p == me || p.m_fHealth <= 0.f || ProtectedPed(&p) || guarded.count(CPools::GetPedRef(&p))) continue;
        auto* tg = KillTarget(&p); if (!tg || !IsBuddyTarget(tg, &p, me) || LikesSide((int)p.m_nPedType)) continue;
        if (std::getenv("NOTSA_MDRIVER_PLOG")) notsa::standalone::Fixups::Log("[mdplog] shootdead: attacker h=%d type=%d model=%d target h=%d type=%d model=%d hp=%.0f tgtkill=%p", CPools::GetPedRef(&p), (int)p.m_nPedType, (int)p.m_nModelIndex, CPools::GetPedRef(tg), (int)tg->m_nPedType, (int)tg->m_nModelIndex, tg->m_fHealth, (void*)KillTarget(tg));
        victims.push_back(CPools::GetPedRef(&p)); if (tg == me) g_enemyTypes.insert((int)p.m_nPedType); g_fightKills++;
    }
    for (int h : victims) Emit("mdkillped:" + std::to_string(h));
}
// CR27 (12 RYDER, 19 CESAR VIALPANDO): the awaited global is written by a STREAMED script thread (barber.scm: the chair + haircut menu, carmod1.scm: the mod-shop menu), whose locate / button conditions nobody answers. Every streamed-script thread's conditions are recorded
// like the root's (AuxBegin/AuxEnd, called from scmtrace::Run for threads outside the traced tree); when the root wait stands (the wait row answered twice) and such a thread writes a global the root's pass compares, Decide() runs over THAT thread's last passes.
struct AuxState { std::deque<RecP> recs; std::vector<RecP> group, pending; bool fallPending = false; int waitSeq = 0; std::map<uint16_t, bool> writes; bool dyn = false; std::set<uint16_t> dynWrites; };
Dec g_fastDec; const uint8_t* g_fastIp = nullptr; int g_fastLeft = 0; bool g_fastNext = false; // CR30: fast key repeats (25 CATALYST: a timed hit task needs one press every ~50 frames, not one per 600-frame stall)
std::map<const CRunningScript*, AuxState> g_aux; bool g_auxTouched = false, g_auxDone = false; std::map<uint32_t, int> g_auxVisits; std::string g_auxTag; bool g_auxAnswered = false, g_auxEarly = false;
const uint8_t* g_auxIp = nullptr; uint16_t g_auxRaw = 0;
bool IntCompareOp(int op) { return (op >= 0x0018 && op <= 0x0041) || (op >= 0x04A3 && op <= 0x04B7) || (op >= 0x07D6 && op <= 0x07D9); }
bool WritesGlobal(const CRunningScript* t, uint16_t off) { // a command of the thread's code whose FIRST operand is the global variable `off` (not a compare): a static byte scan of the streamed script
    auto& st = g_aux[t]; if (st.dyn) return st.dynWrites.count(off) > 0; // CR29: a thread of the traced tree (started by the mission: lowrider game thread of main.scm): the globals it was SEEN writing (scmtrace G rows)
    if (auto it = st.writes.find(off); it != st.writes.end()) return it->second;
    int32_t size = 0; for (auto& si : CTheScripts::StreamedScripts.m_aScripts) if (si.m_StreamedScriptMemory && si.m_StreamedScriptMemory == t->m_BaseIP) size = si.m_SizeInBytes;
    bool w = false; const uint8_t* b = t->m_BaseIP;
    for (int32_t i = 0; i + 5 <= size && !w; i++) if (b[i + 2] == 2 && *(const uint16_t*)(b + i + 3) == off) { const int op = *(const uint16_t*)(b + i) & 0x7FFF; if (op > 0 && op < 0x0200 && !IntCompareOp(op) && op != 0x0001 && op != 0x0002 && op != 0x004D && op != 0x0050 && op != 0x00D6) w = true; }
    return st.writes[off] = w;
}
static float g_passStat0 = 0.f;
static std::map<int,int> g_padHold; // CR4: button -> frames left
static std::string ShooterDesc(CEntity* e) { // CR18: debug: model, vehicle (model, driver or passenger), distance to CJ, active weapon of the last damager of CJ's car
    if (!e || e->GetType() != ENTITY_TYPE_PED) return "-"; auto* p = static_cast<CPed*>(e); char b[160]; auto* me = Cj();
    std::snprintf(b, sizeof b, "ped%d/inveh=%d(%d,%s)/d=%.1f/wpn=%d", (int)p->m_nModelIndex, (int)p->bInVehicle, p->m_pVehicle ? (int)p->m_pVehicle->m_nModelIndex : -1, p->m_pVehicle ? (p->m_pVehicle->m_pDriver == p ? "drv" : "pax") : "-", me ? (p->GetPosition() - me->GetPosition()).Magnitude() : -1.f, (int)p->GetActiveWeapon().m_Type);
    std::string o = b; o += " pedtype=" + std::to_string((int)p->m_nPedType) + " wanted=" + std::to_string(FindPlayerWanted() ? (int)FindPlayerWanted()->m_WantedLevel : -1); auto& tm = p->GetIntelligence()->m_TaskMgr; for (int ti = 0; ti < 5; ti++) if (auto* t = tm.GetTaskPrimary(ti)) { o += " P" + std::to_string(ti) + ":"; for (auto* tt = t; tt; tt = tt->GetSubTask()) o += std::to_string((int)tt->GetTaskType()) + ">"; } // CR34: tasks of the shooter
    return o;
}
void TickGeneric() {
    for (auto it = g_padHold.begin(); it != g_padHold.end();) { if (it->second > 0) --it->second; it = it->second > 0 ? std::next(it) : g_padHold.erase(it); }
    if (!g_loaded) { LoadTable(std::getenv("NOTSA_MDRIVER_TABLE")); LoadArity(); if (const char* o = std::getenv("NOTSA_MDRIVER_OUT")) g_out = std::fopen(o, "w"); if (const char* m = std::getenv("NOTSA_MDRIVER_MAXFRAMES")) g_maxFrames = std::atoi(m); }
    if (static const char* st = std::getenv("NOTSA_MDRIVER_STALL")) g_stall = std::atoi(st); // CR16: per-run override of the stall threshold (TS stallFrames = 600 frames of 33.33 ms = 20 s; 1000 frames of the 20 ms RR step are the same game time; allmissions.sh sets it for mission 46)
    if (g_quitIn >= 0) { if (g_quitIn-- == 0) { WriteResult(); Emit("quit"); if (g_out) { std::fclose(g_out); g_out = nullptr; } g_done = true; } return; }
    auto* root = notsa::scmtrace::Root();
    if (!g_started) { if (!root) return; g_started = true; StaticScan(root); if (!std::getenv("NOTSA_MDRIVER_NOCITY")) CStats::SetStatValue(STAT_CITY_UNLOCKED, 2.f); /* CR10: a mission started cold stands where the story would have opened the cities: SF/LV closed = 4 stars on arrival (SetPlayerWantedLevelForForbiddenTerritories), cops attack */ g_passStat0 = CStats::GetStatValue(STAT_MISSIONS_PASSED); g_thread = root; g_frame0 = CTimer::GetFrameCounter(); g_sameCount = 0; g_lastIp = nullptr; }
    const int fr = GFrame();
    if (g_follow.left > 0) { // CR47
        auto* me = Cj(); auto* tv = CPools::GetVehicle(g_follow.tgt); CVehicle* mv = me && me->bInVehicle ? me->m_pVehicle : nullptr;
        if (!tv || !mv || tv->m_fHealth <= 0.f || mv->m_pAttachedTo || tv == mv) g_follow.left = 0;
        else { g_follow.left--; const CVector q = tv->GetPosition() + tv->GetRight() * g_follow.off.x + tv->GetForward() * g_follow.off.y + tv->GetUp() * g_follow.off.z; mv->SetPosn(q); mv->m_vecMoveSpeed = tv->m_vecMoveSpeed; mv->m_vecTurnSpeed = CVector{}; }
    }
    if (g_airHold.left > 0) { g_airHold.left--; if (auto* v = CPools::GetVehicle(g_airHold.h); v && v->m_fHealth > 0.f && Cj() && Cj()->bInVehicle && Cj()->m_pVehicle == v) { v->m_vecMoveSpeed.z = 0.f; if (g_airHold.vx != 0.f || g_airHold.vy != 0.f) { v->m_vecMoveSpeed.x = g_airHold.vx; v->m_vecMoveSpeed.y = g_airHold.vy; } v->m_vecTurnSpeed = CVector{}; CVector pp = v->GetPosition(); pp.z = g_airHold.z; v->SetPosn(pp); } else g_airHold.left = 0; }
    // CR50 (92 SAINT MARK'S BISTRO: after the recorded-car cutscene the script puts the Shamal at z 200 with speed 30 and gives CJ the stick; nobody flies, the plane fell into the sea in 400 frames, before the 600-frame driver round answered the landing area): a plane that CJ sits in, that flies >30 m above the ground, falls and has no recorded-car playback is held at the current altitude for 1500 frames (the driver does not fly).
    if (g_airHold.left <= 0 && !std::getenv("NOTSA_MDRIVER_NOFALLHOLD")) if (auto* me7 = Cj(); me7 && me7->bInVehicle && me7->m_pVehicle && me7->m_pVehicle->IsSubPlane() && me7->m_pVehicle->m_fHealth > 0.f && !CVehicleRecording::IsPlaybackGoingOnForCar(me7->m_pVehicle) && me7->m_pVehicle->m_vecMoveSpeed.z < -0.02f) { auto* v7 = me7->m_pVehicle; const float z7 = v7->GetPosition().z; if (z7 - CWorld::FindGroundZForCoord(v7->GetPosition().x, v7->GetPosition().y) > 30.f) { g_airHold.h = GetVehiclePool()->GetRef(v7); g_airHold.z = z7; g_airHold.left = 1500; g_airHold.vx = g_airHold.vy = 0.f; notsa::standalone::Fixups::Log("[mdriver] fallhold: the falling plane is held at z %.0f", (double)z7); } }
    if (g_speedHold.left > 0) { g_speedHold.left--; if (auto* v = CPools::GetVehicle(g_speedHold.h); v && v->m_fHealth > 0.f) { const CVector f = v->GetForward(); v->m_vecMoveSpeed.x = f.x * g_speedHold.mps / 50.f; v->m_vecMoveSpeed.y = f.y * g_speedHold.mps / 50.f; } else g_speedHold.left = 0; }
    if (!g_fixCars.empty()) for (const int h : g_fixCars) if (auto* v = CPools::GetVehicle(h); v && v->vehicleFlags.bIsDamaged && v->m_fHealth > 0.f && !std::getenv("NOTSA_MDRIVER_NOKEEPFIX")) { v->Fix(); v->m_fHealth = 1000.f; }
    if (root) { char n[9]{}; std::memcpy(n, root->m_szName, 8); g_name = n; }
    if (!root) { // the thread ended (TS: !alive(script)): PASSED / FAILED, or STOP when a kill of the driver was just before it
        g_endFrame = fr;
        if (!g_pass && CStats::GetStatValue(STAT_MISSIONS_PASSED) > g_passStat0) { g_pass = true; g_passFrame = fr; } // CR6: TS checks the stat at thread end too (87 78: cutscene-only, the stat rises in the thread's last command)
        if (!g_pass && fr - g_killFrame <= g_killSettle + 5) { g_stopReason = "driver stop: killed a guard"; g_stopOp = -1; }
        else if (!g_pass && !g_fail && !g_relaunched && NotsaLastInjectedMission >= 0 && !std::getenv("NOTSA_MDRIVER_NORELAUNCH")) { // CR33 (34 HOUSE PARTY: the first visit plays the intro cutscene, prints the hint and ENDS without M_PASS / M_FAIL, setting a global ($7756) that the second visit reads): a mission that ended with neither M_PASS nor M_FAIL is a staged mission, the player starts it again (once; the globals stay)
            if (g_relaunchWait++ < 300 && (CTheScripts::IsPlayerOnAMission() || CTheScripts::FailCurrentMission != 0)) return;
            if (!CTheScripts::IsPlayerOnAMission() && CTheScripts::FailCurrentMission == 0) {
                g_relaunched = true; g_relaunchWait = 0; notsa::standalone::Fixups::Log("[mdriver] relaunch frame %d: mission %d ended without M_PASS / M_FAIL: started again", fr, (int)NotsaLastInjectedMission);
                NotsaInjectMissionInfo mi{}; NotsaInjectStartMission(NotsaLastInjectedMission, &mi);
                g_started = false; g_thread = nullptr; g_recs.clear(); g_recsBig.clear(); g_attemptsAt.clear(); g_queue.clear(); g_qpos = 0; g_wait = 0; g_untilLeft = 0; g_sameCount = 0; g_lastIp = nullptr; g_ipSeen.clear(); g_endFrame = -1; g_lastLabel.clear();
                return;
            }
            g_endedFail = true;
        }
        else if (!g_pass) g_endedFail = true;
        notsa::standalone::Fixups::Log("[mdriver] STAT_MISSIONS_PASSED %.1f at start %.1f", CStats::GetStatValue(STAT_MISSIONS_PASSED), g_passStat0); std::fprintf(stderr, "[mdriver] mission thread ended at frame %d (pass=%d fail=%d)\n", fr, (int)g_pass, (int)g_fail); g_quitIn = 50; return;
    }
    if (g_pass && fr - g_passFrame > 1500) { g_quitIn = 0; return; }
    if (g_fail && fr - g_failFrame > 600) { g_quitIn = 0; return; }
    if (fr >= g_maxFrames) { Stop("budget: " + std::to_string(g_maxFrames) + " frames", -1, (uint32_t)(root->m_IP - root->m_BaseIP)); return; }
    if (!g_pass && CStats::GetStatValue(STAT_MISSIONS_PASSED) > g_passStat0) { g_pass = true; g_passFrame = fr; } // CR4: TS drive.ts: PASSED = STAT_MISSIONS_PASSED rose (0x0318 SET_LATEST_MISSION_PASSED; missions 87 78 print no M_PASS)
    if (!g_pass && (fr - g_killFrame <= 900 || fr - g_wantedClrFrame <= 900) && !std::getenv("NOTSA_MDRIVER_KEEPKILLWANTED") && fr - g_scriptWantedFrame > 120) if (auto* w = FindPlayerWanted()) if (w->m_WantedLevel != eWantedLevel::WANTED_CLEAN) w->SetWantedLevel(eWantedLevel::WANTED_CLEAN); // CR25 (15 DRIVE-THRU): the driver's own `kill` (a stand-in for shooting the hostile peds) is reported as a murder by witnesses: the one star got CJ, who stood still in his car, ARRESTED 250 frames later (HAS_DEATHARREST_BEEN_EXECUTED -> the script's fail path). The star the kill raised is cleared for 900 frames
    if (g_hurtNow.h) { auto* pp = CPools::GetPed(g_hurtNow.h); if (pp && pp != Cj() && pp->m_fHealth > (float)g_hurtNow.hp && !ProtectedHandle(g_hurtNow.h)) Emit("mdhurt:" + std::to_string(g_hurtNow.h) + ":" + std::to_string(g_hurtNow.hp)); g_hurtNow = {}; } // CR37
    if (!std::getenv("NOTSA_MDRIVER_NOFIGHT") && ++g_fightClock % 3 == 0 && !CCutsceneMgr::IsCutsceneProcessing() && Cj()) ShootDead();
    if (static const char* clog = std::getenv("NOTSA_MDRIVER_CARLOG"); clog && Cj() && fr % 100 == 0) { // CR47 debug (off by default): NOTSA_MDRIVER_CARLOG=h1,h2: position / speed / heading of those vehicles (or peds, h tried as a vehicle first)
        std::string l = clog; size_t i = 0; auto* me = Cj(); const auto cp = me->GetPosition();
        while (i < l.size()) { const size_t j = l.find(',', i); const int h = std::atoi(l.substr(i, j == std::string::npos ? std::string::npos : j - i).c_str()); i = j == std::string::npos ? l.size() : j + 1;
            if (auto* v = CPools::GetVehicle(h)) notsa::standalone::Fixups::Log("[mdcar] f%d veh %d model %d pos %.0f,%.0f,%.0f spd %.1f hp %.0f dist-to-CJ %.0f driver %d", fr, h, (int)v->m_nModelIndex, v->GetPosition().x, v->GetPosition().y, v->GetPosition().z, v->m_vecMoveSpeed.Magnitude() * 50.f, v->m_fHealth, (v->GetPosition() - cp).Magnitude(), v->m_pDriver ? 1 : 0);
            else if (auto* pd = CPools::GetPed(h)) notsa::standalone::Fixups::Log("[mdcar] f%d ped %d model %d pos %.0f,%.0f,%.0f hp %.0f dist-to-CJ %.0f inveh %d", fr, h, (int)pd->m_nModelIndex, pd->GetPosition().x, pd->GetPosition().y, pd->GetPosition().z, pd->m_fHealth, (pd->GetPosition() - cp).Magnitude(), pd->bInVehicle ? 1 : 0);
            else notsa::standalone::Fixups::Log("[mdcar] f%d h %d gone", fr, h); }
    }
    if (static const bool plog = std::getenv("NOTSA_MDRIVER_PLOG") != nullptr; plog && Cj() && fr % 50 == 0) { // CR8: CJ state every 50 frames (debug)
        auto* me = Cj(); const auto pp = me->GetPosition(); auto* v = me->bInVehicle ? me->m_pVehicle : nullptr; auto* w = FindPlayerWanted();
        notsa::standalone::Fixups::Log("[mdplog] f%d cj=%.1f,%.1f,%.1f hp=%.0f st=%d inv=%d veh=%d(%.0f hp, st %d, drown %d) wanted=%d lastDmg=%d wpn=%d\n", fr, pp.x, pp.y, pp.z, me->m_fHealth, (int)me->m_nPedState, (int)me->bInVehicle,
            v ? (int)v->m_nModelIndex : -1, v ? v->m_fHealth : 0.f, v ? (int)v->GetStatus() : 0, v ? (int)v->vehicleFlags.bIsDrowning : 0, w ? (int)w->m_WantedLevel : -1, me->m_pLastEntityDamage ? (int)me->m_pLastEntityDamage->GetType() : -1, (int)me->m_nLastWeaponDamage);
        for (int gi = 0; gi < CGarages::NumGarages; gi++) { auto& g = CGarages::GetGarage(gi); if (g.m_nType != PAYNSPRAY || g.CalcDistToGarageRectangleSquared(pp.x, pp.y) > 900.f) continue; notsa::standalone::Fixups::Log("[mdplog] garage %d door=%d inside=%d outside=%d sprayable=%d NoResprays=%d free=%d money=%d spd=%.3f wantedBackOff=%d\n", gi, (int)g.m_nDoorState, (int)g.IsStaticPlayerCarEntirelyInside(), (int)g.IsPlayerOutsideGarage(0.0f), v ? (int)CGarages::IsCarSprayable(v) : -1, (int)CGarages::NoResprays, (int)CGarages::RespraysAreFree, (int)CWorld::Players[CWorld::PlayerInFocus].m_nMoney, v ? v->m_vecMoveSpeed.Magnitude() : -1.f, w ? (int)w->m_bPoliceBackOffGarage : -1); }
    }
    if (static const bool tl = std::getenv("NOTSA_MDRIVER_TRAINLOG") != nullptr; tl && root && Cj() && fr % 30 == 0) { // CR35: debug: every train carriage (handle, position, speed, health) next to CJ's position
        std::string o; for (auto& vv : GetVehiclePool()->GetAllValid()) if (vv.IsTrain()) { char b[96]; std::snprintf(b, sizeof b, " %d@%.0f,%.0f,%.0f/%.1f/%.0f%s", GetVehiclePool()->GetRef(&vv), vv.GetPosition().x, vv.GetPosition().y, vv.GetPosition().z, vv.m_vecMoveSpeed.Magnitude() * 50.f, vv.m_fHealth, vv.m_pDriver ? "d" : ""); o += b; }
        if (fr % 120 == 0) { int n = 0; for (auto& pp : GetPedPool()->GetAllValid()) { if (&pp == Cj() || (pp.GetPosition() - Cj()->GetPosition()).Magnitude() > 80.f || n++ >= 8) continue; notsa::standalone::Fixups::Log("[trainlog]   ped %d hp %.0f pos %.0f,%.0f,%.0f %s", CPools::GetPedRef(&pp), pp.m_fHealth, pp.GetPosition().x, pp.GetPosition().y, pp.GetPosition().z, ShooterDesc(&pp).c_str()); } }
        const auto cp = Cj()->GetPosition(); notsa::standalone::Fixups::Log("[trainlog] f%d cj=%.0f,%.0f,%.0f inveh=%d trains:%s", fr, cp.x, cp.y, cp.z, (int)Cj()->bInVehicle, o.c_str());
    }
    if (static const char* lgl = std::getenv("NOTSA_MDRIVER_LOCLOG"); lgl && root && Cj() && fr % (std::getenv("NOTSA_MDRIVER_LOCLOGSTEP") ? std::max(1, std::atoi(std::getenv("NOTSA_MDRIVER_LOCLOGSTEP"))) : 50) == 0) { // CR30: debug (no control flow): position of the ped / car held by each mission local of the env list "101,54" (the dealer chase of 23); a value >= 100000 is a global byte offset + 100000
        std::string o; for (const char* q = lgl; *q;) { const int li = std::atoi(q); while (*q && *q != ',') q++; if (*q == ',') q++; int hv = 0; if (li >= 100000) { if (GlobalOk(li - 100000)) std::memcpy(&hv, CTheScripts::ScriptSpace.data() + (li - 100000), 4); } else hv = LocalAt(root, li)->iParam; char t[128]; if (auto* pd = CPools::GetPed(hv)) { const auto pp = pd->GetPosition(); std::snprintf(t, sizeof t, " L%d=ped(m%d %.0f,%.0f,%.0f hp%.0f st%d dmg%d,%d%s)", li, (int)pd->m_nModelIndex, pp.x, pp.y, pp.z, pd->m_fHealth, (int)pd->m_nPedState, pd->m_pLastEntityDamage ? (int)pd->m_pLastEntityDamage->GetType() : -1, (int)pd->m_nLastWeaponDamage, pd->bInVehicle ? " inveh" : ""); o += t; } else if (auto* vh = CPools::GetVehicle(hv)) { const auto pp = vh->GetPosition(); std::snprintf(t, sizeof t, " L%d=veh(%.0f,%.0f,%.0f hp%.0f drv%d spd%.1f st%d mis%d tgt%.0f,%.0f)", li, pp.x, pp.y, pp.z, vh->m_fHealth, vh->m_pDriver ? 1 : 0, vh->m_vecMoveSpeed.Magnitude() * 50.f, (int)vh->GetStatus(), (int)vh->m_autoPilot.m_nCarMission, vh->m_autoPilot.m_vecDestinationCoors.x, vh->m_autoPilot.m_vecDestinationCoors.y); o += t; if (vh->m_pDriver) { std::string tk; auto& tm = vh->m_pDriver->GetIntelligence()->m_TaskMgr; for (int ti = 0; ti < 5; ti++) if (auto* tt = tm.GetTaskPrimary(ti)) { tk += " P" + std::to_string(ti) + ":"; for (auto* x = tt; x; x = x->GetSubTask()) tk += std::to_string((int)x->GetTaskType()) + ">"; } o += " drvtasks" + tk; } } }
        const auto cp = Cj()->GetPosition(); notsa::standalone::Fixups::Log("[loclog] f%d cj=%.0f,%.0f,%.0f hp%.0f%s", fr, cp.x, cp.y, cp.z, (double)Cj()->m_fHealth, o.c_str());
    }
    if (fr % 20 == 0 && (g_wait == 0 || !std::getenv("NOTSA_MDRIVER_NOSMALLLEASH")) && g_qpos >= g_queue.size() && g_untilLeft == 0 && root && Cj() && !CCutsceneMgr::IsCutsceneProcessing() && !std::getenv("NOTSA_MDRIVER_NOLEASHKEEP")) { // CR30 (23 GRAY IMPORTS, the dealer chase): the pass holds a `NOT (CJ within R m of ped X)` fail guard (00F2..00F4 / 0104..0106, CJ the first operand, R >= 150 m: the script fails the mission when CJ lost the moving target) and X has driven off to more than 0.6 R: CJ joins X (the warp the player's chase amounts to), behind it
        for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; if (r->waitSeq < g_waitSeq - 3 || !(r->raw & 0x8000) || !((rop >= 0x00F2 && rop <= 0x00F4) || (rop >= 0x0104 && rop <= 0x0106) || (rop >= 0x0202 && rop <= 0x0207 && !std::getenv("NOTSA_MDRIVER_NOCARKEEP")))) continue; // CR32: 0202..0207 = the same guard about a CAR (29 WRONG SIDE OF THE TRACKS: `NOT locate CJ within 200 m of the train` ends the mission when the train ran off)
            const bool carOp = rop >= 0x0202 && rop <= 0x0207;
            const auto h0 = ArgInt(*r, 0), h1 = ArgInt(*r, 1), rr = ArgFloat(*r, 2); if (g_wait != 0 && (!carOp || !rr || *rr >= 150.0)) continue; /* CR34: during the settle of an answer only the small car leash (the answer may have moved the target away: 33) acts */
            if (!h0 || !h1 || !rr || *rr < (carOp && !std::getenv("NOTSA_MDRIVER_NOSMALLLEASH") ? 30.0 : 150.0) /* CR35: the ped leash stays at R >= 150 (a small ped leash changed 11) */ /* CR34 (33 MANAGEMENT ISSUES: `NOT locate CJ within 35 m of the lead car` counts 5 s down to the mission's failure) */ || (carOp ? (CEntity*)CPools::GetPed((int)*h0) : EntityOfHandle((int)*h0, rop)) != (CEntity*)Cj()) continue;
            CPhysical* x = carOp ? (CPhysical*)CPools::GetVehicle((int)*h1) : (CPhysical*)CPools::GetPed((int)*h1); if (!x || x == (CPhysical*)Cj() || (carOp ? ((CVehicle*)x)->m_fHealth : ((CPed*)x)->m_fHealth) <= 0.f) continue; const CVector xp = x->GetPosition(), cp = Cj()->GetPosition();
            static const double s_leashFrac = std::getenv("NOTSA_MDRIVER_LEASHFRAC") ? std::atof(std::getenv("NOTSA_MDRIVER_LEASHFRAC")) : 0.15; // CR46 (68 TEST DRIVE: `NOT locate CJ within 150 m of the test car` while the car drives its route at 9 m/s and its playback speed / stage boxes follow CJ: a 90 m leash left CJ too far behind for the 3 m boxes; the car warped onto a box snapped back to its recording and the mission failed 3 frames later). 0.15 R = 22 m for a road car (the first form of the leash, 0.6 R, stays for trains, aircraft and the ped leash)
            const bool roadCar = carOp && !((CVehicle*)x)->IsTrain() && !((CVehicle*)x)->IsSubPlane() && !((CVehicle*)x)->IsSubHeli() && !std::getenv("NOTSA_MDRIVER_NOCARLEASH15");
            if ((xp - cp).Magnitude2D() <= (roadCar ? s_leashFrac : 0.6) * *rr) break;
            if (carOp && Cj()->bInVehicle && Cj()->m_pVehicle == (CVehicle*)x) break;
            const CVector fw = carOp ? ((CVehicle*)x)->GetForward() : (((CPed*)x)->bInVehicle && ((CPed*)x)->m_pVehicle ? ((CPed*)x)->m_pVehicle->GetForward() : ((CPed*)x)->GetForward()); const CVector q = carOp && ((CVehicle*)x)->IsTrain() ? xp + ((CVehicle*)x)->GetRight() * 14.f : xp - fw * 12.f; // a train: beside the tracks, not on them
            notsa::standalone::Fixups::Log("[mdriver] leashkeep frame %d: CJ %.0f m from the target ped, joins it at %.0f,%.0f", fr, (double)(xp - cp).Magnitude2D(), (double)q.x, (double)q.y);
            if (carOp && ((CVehicle*)x)->IsTrain() && ((CVehicle*)x)->m_vecMoveSpeed.Magnitude() * 50.f > 3.f && !std::getenv("NOTSA_MDRIVER_NOTRAINMATCH")) { const auto sp = ((CVehicle*)x)->m_vecMoveSpeed; Emit("mdwveh:" + F(q.x) + ":" + F(q.y) + ":-100:" + F(sp.x) + ":" + F(sp.y) + ":" + F(sp.z) + ":" + F(std::atan2(-fw.x, fw.y))); break; } // CR35 (29): the car joins a moving train with the train's velocity and heading (a car warped at rest every 40 frames cannot keep Smoke's aim on the train)
            Emit("mdwveh:" + F(q.x) + ":" + F(q.y) + ":-100"); break; }
    }
    if (fr % 10 == 0 && g_wait == 0 && g_qpos >= g_queue.size() && g_untilLeft == 0 && root && Cj() && !CCutsceneMgr::IsCutsceneProcessing() && !std::getenv("NOTSA_MDRIVER_NOCOORDLEASH")) { // CR42 (61 ICE COLD KILLA: `NOT LOCATE_CHAR_ANY_MEANS_3D CJ x y z 150 150 150` where x y z are Jizzy's coordinates, refreshed every frame, fails the mission while the driver idles and he drives off): a coordinate leash = a NOT locate of CJ (00EC..00F1 2D / 00FE..0103 3D, CJ the first operand, R >= 150) of a point that moved more than 0.6 R away: CJ joins the point (the nearest car within 15 m of it, behind it, when there is one)
        for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; const bool d3 = rop >= 0x00FE && rop <= 0x0103, d2 = rop >= 0x00EC && rop <= 0x00F1; if (r->waitSeq < g_waitSeq - 3 || !(r->raw & 0x8000) || (!d2 && !d3) || r->args.size() < 3 || r->args[1].kind != 2 || r->args[2].kind != 2) continue; /* CR42: a MOVING point only (x y are variables); a fixed point is a place (20 LOS SEPULCROS) */
            const auto h0 = ArgInt(*r, 0), lx = ArgFloat(*r, 1), ly = ArgFloat(*r, 2), rr = ArgFloat(*r, d3 ? 4 : 3); if (!h0 || !lx || !ly || !rr || *rr < 150.0 || EntityOfHandle((int)*h0, rop) != (CEntity*)Cj()) continue;
            const CVector cp = Cj()->GetPosition(); const float dist = std::sqrt((float)((*lx - cp.x) * (*lx - cp.x) + (*ly - cp.y) * (*ly - cp.y))); if (std::getenv("NOTSA_MDRIVER_LOCLOG")) notsa::standalone::Fixups::Log("[coordleash] f%d point %.0f,%.0f dist %.0f", fr, *lx, *ly, (double)dist); if (dist <= 0.4 * *rr) break;
            CVector q{ (float)*lx, (float)*ly, 0.f }; CVehicle* tv = nullptr; float bd = 15.f; for (auto& vv : GetVehiclePool()->GetAllValid()) { if (vv.m_fHealth <= 0.f || (Cj()->bInVehicle && Cj()->m_pVehicle == &vv)) continue; const float dd = (float)std::sqrt((vv.GetPosition().x - *lx) * (vv.GetPosition().x - *lx) + (vv.GetPosition().y - *ly) * (vv.GetPosition().y - *ly)); if (dd < bd) { bd = dd; tv = &vv; } }
            if (tv) q = tv->GetPosition() - tv->GetForward() * 12.f;
            if (tv && Cj()->bInVehicle && !std::getenv("NOTSA_MDRIVER_NOCOORDRAM")) { const CVector fw = tv->GetForward(), sp = tv->m_vecMoveSpeed; q = tv->GetPosition() - fw * 7.f; notsa::standalone::Fixups::Log("[mdriver] coordleash frame %d: CJ (in a car) joins the car at the leash point, 7 m behind, 8 m/s faster (a ram)", fr); Emit("mdwveh:" + F(q.x) + ":" + F(q.y) + ":-100:" + F(sp.x + fw.x * 0.16f) + ":" + F(sp.y + fw.y * 0.16f) + ":" + F(sp.z) + ":" + F(std::atan2(-fw.x, fw.y))); break; }
            notsa::standalone::Fixups::Log("[mdriver] coordleash frame %d: CJ %.0f m from the leash point %.0f,%.0f (R %.0f), joins it at %.0f,%.0f%s", fr, (double)dist, *lx, *ly, *rr, (double)q.x, (double)q.y, tv ? " (behind the car there)" : "");
            Emit("mdwveh:" + F(q.x) + ":" + F(q.y) + ":-100"); break; }
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
        static float s_php = -1.f; if (Cj()->m_fHealth != s_php) { if (s_php >= 0.f && Cj()->m_fHealth < s_php) notsa::standalone::Fixups::Log("[hplog] f%d CJ hp %.0f->%.0f pos=%.1f,%.1f,%.1f wpn=%d fire=%d inveh=%d drown=%d water=%d shooter=%s", fr, s_php, Cj()->m_fHealth, Cj()->GetPosition().x, Cj()->GetPosition().y, Cj()->GetPosition().z, (int)Cj()->m_nLastWeaponDamage, Cj()->m_pFire ? 1 : 0, (int)Cj()->bInVehicle, (int)Cj()->bIsDrowning, (int)Cj()->physicalFlags.bSubmergedInWater, ShooterDesc(Cj()->m_pLastEntityDamage).c_str()); s_php = Cj()->m_fHealth; } // CR35: CJ himself
        static float s_hp = -1.f; auto* v = Cj()->bInVehicle ? Cj()->m_pVehicle : nullptr;
        if (v && v->m_fHealth != s_hp) { if (s_hp >= 0.f && v->m_fHealth < s_hp) notsa::standalone::Fixups::Log("[hplog] f%d hp %.0f->%.0f pos=%.1f,%.1f,%.1f spd=%.3f water=%d drown=%d fire=%d wpn=%d dmgEnt=%d inv=%d shooter=%s", fr, s_hp, v->m_fHealth, v->GetPosition().x, v->GetPosition().y, v->GetPosition().z, v->m_vecMoveSpeed.Magnitude(), (int)v->physicalFlags.bTouchingWater, (int)v->vehicleFlags.bIsDrowning, v->m_pFire ? 1 : 0, (int)v->m_nLastWeaponDamageType, v->m_pLastDamageEntity ? (int)v->m_pLastDamageEntity->GetType() : -1, (int)v->physicalFlags.bInvulnerable, ShooterDesc(v->m_pLastDamageEntity).c_str()); s_hp = v->m_fHealth; }
    }
    if (root && Cj() && fr % 5 == 0 && g_wait == 0 && g_qpos >= g_queue.size() && g_untilLeft == 0 && !CCutsceneMgr::IsCutsceneProcessing() && std::getenv("NOTSA_MDRIVER_RETALIATE")) { /* opt-in: broke 26 (army guards killed) */ // CR34 (33 MANAGEMENT ISSUES: the mission's own guards (ped type MISSION1+) shoot CJ's car while the stage waits for the player to arrive; the car burns down and the passenger the script keeps alive dies): a player returns the fire. 4 health drops of CJ (or his car) from the same hostile script ped = the ped is killed (never a buddy / protected ped)
        static CPhysical* s_t = nullptr; static float s_hp = -1.f; static CPed* s_sh = nullptr; static int s_hits = 0;
        CPhysical* t = Cj()->bInVehicle && Cj()->m_pVehicle ? (CPhysical*)Cj()->m_pVehicle : (CPhysical*)Cj(); const float hp = Cj()->bInVehicle && Cj()->m_pVehicle ? Cj()->m_pVehicle->m_fHealth : Cj()->m_fHealth;
        if (t != s_t) { s_t = t; s_hp = hp; }
        else if (hp < s_hp - 0.5f) {
            CEntity* dm = Cj()->bInVehicle && Cj()->m_pVehicle ? Cj()->m_pVehicle->m_pLastDamageEntity : Cj()->m_pLastEntityDamage;
            if (dm && dm->GetType() == ENTITY_TYPE_PED && dm != Cj() && (int)static_cast<CPed*>(dm)->m_nPedType >= (int)PED_TYPE_MISSION1) { auto* sp = static_cast<CPed*>(dm); if (sp != s_sh) { s_sh = sp; s_hits = 0; } s_hits++; }
            s_hp = hp;
            if (s_sh && s_hits >= 4) { const int h = CPools::GetPedRef(s_sh); if (s_sh->m_fHealth > 0.f && !ProtectedHandle(h)) { notsa::standalone::Fixups::Log("[mdriver] retaliate frame %d: ped %d (model %d, type %d) shot CJ's %s %d times: killed", fr, h, (int)s_sh->m_nModelIndex, (int)s_sh->m_nPedType, Cj()->bInVehicle ? "car" : "ped", s_hits); Emit("mdkillped:" + std::to_string(h)); } s_hits = 0; s_sh = nullptr; }
        } else if (hp > s_hp) s_hp = hp;
    }
    if (static const bool xl = std::getenv("NOTSA_MDRIVER_ENEXLOG") != nullptr; xl && Cj() && fr % 25 == 0 && fr > (std::getenv("NOTSA_MDRIVER_ENEXFROM") ? std::atoi(std::getenv("NOTSA_MDRIVER_ENEXFROM")) : 16400) && fr < 99999) { // CR23: debug: why does standing on an entrance marker not start the transition
        auto* me = Cj(); const auto pp = me->GetPosition(); std::string tk; auto& tm = me->GetIntelligence()->m_TaskMgr; for (int ti = 0; ti < 5; ti++) if (auto* t = tm.GetTaskPrimary(ti)) { tk += " P" + std::to_string(ti) + ":"; for (auto* tt = t; tt; tt = tt->GetSubTask()) tk += std::to_string((int)tt->GetTaskType()) + ">"; }
        for (auto& ex : CEntryExitManager::GetPool()->GetAllValid()) { if (std::strncmp(ex.m_szName, std::getenv("NOTSA_MDRIVER_ENEXNAME") ? std::getenv("NOTSA_MDRIVER_ENEXNAME") : "CSCHP", std::strlen(std::getenv("NOTSA_MDRIVER_ENEXNAME") ? std::getenv("NOTSA_MDRIVER_ENEXNAME") : "CSCHP")) || ex.m_nArea != 0) continue; const auto c = ex.GetPosition(); if ((c - pp).Magnitude() > 30.f) continue;
            notsa::standalone::Fixups::Log("[enexlog] f%d cj=%.2f,%.2f,%.2f inArea=%d access=%d vis=%d on=%d off=%d clock=%d:%d state=%d active=%p inCtl=%d pstate=%d ctlOff=%d gstate=%d disabled=%d cutscene=%d area=%d burg=%d bhe=%d tasks=%s", fr, pp.x, pp.y, pp.z, (int)ex.IsInArea(pp), (int)ex.bEnableAccess, (int)ex.IsVisibleByTime(), (int)ex.m_nTimeOn, (int)ex.m_nTimeOff, (int)CClock::GetGameClockHours(), (int)CClock::GetGameClockMinutes(), (int)CEntryExitManager::ms_exitEnterState, (void*)CEntryExitManager::mp_Active, (int)me->IsPedInControl(), (int)me->m_nPedState, (int)CPad::GetPad(0)->DisablePlayerControls, (int)CGameLogic::GameState, (int)CEntryExitManager::ms_bDisabled, (int)CCutsceneMgr::ms_cutsceneProcessing, (int)CGame::currArea, (int)ex.bBurglaryAccess, (int)CEntryExitManager::ms_bBurglaryHousesEnabled, tk.c_str()); }
    }
    if (static const char* li = std::getenv("NOTSA_MDRIVER_LOGINTS"); li && root && fr % 100 == 0) { // CR28: debug (log only): "n,n,..": the int value of those script locals of the root thread; "g<n>" = a global (byte offset)
        std::string o; for (const char* q = li; *q;) { const bool g = *q == 'g'; if (g) q++; const int n = std::atoi(q); while (*q && *q != ',') q++; if (*q == ',') q++; if (g) { if (GlobalOk(n)) { int32_t v; std::memcpy(&v, CTheScripts::ScriptSpace.data() + n, 4); o += " g" + std::to_string(n) + "=" + std::to_string(v); } } else o += " @" + std::to_string(n) + "=" + std::to_string(LocalAt(root, n)->iParam); }
        notsa::standalone::Fixups::Log("[logints] f%d%s", fr, o.c_str());
    }
    if (static const char* ll = std::getenv("NOTSA_MDRIVER_LOGLOCALS"); ll && root && fr % 60 == 0 && Cj()) { // CR23: debug: "n,n,..": script locals that hold a ped or vehicle handle: model, position, distance to CJ; and (CARSLOG=1) the vehicles within 150 m of CJ by model
        std::string o; const auto pp = Cj()->GetPosition();
        for (const char* q = ll; *q;) { const int li = std::atoi(q); while (*q && *q != ',') q++; if (*q == ',') q++; const int h = LocalAt(root, li)->iParam; if (auto* pe = CPools::GetPed(h)) { const auto p2 = pe->GetPosition(); o += " L" + std::to_string(li) + "=ped" + std::to_string((int)pe->m_nModelIndex) + "@" + std::to_string((int)p2.x) + "," + std::to_string((int)p2.y) + " d" + std::to_string((int)(p2 - pp).Magnitude()) + (pe->m_fHealth > 0 ? "" : "X") + (pe->bInVehicle ? "v" : "") + (pe->bInVehicle && pe->m_pVehicle ? std::string("[car ") + std::to_string((int)pe->m_pVehicle->GetPosition().x) + "," + std::to_string((int)pe->m_pVehicle->GetPosition().y) + " m" + std::to_string((int)pe->m_pVehicle->m_autoPilot.m_nCarMission) + " t" + std::to_string(pe->GetIntelligence()->m_TaskMgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY) ? (int)pe->GetIntelligence()->m_TaskMgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY)->GetTaskType() : -1) + "]" : std::string("")); } else if (auto* ve = CPools::GetVehicle(h)) { const auto p2 = ve->GetPosition(); o += " L" + std::to_string(li) + "=veh" + std::to_string((int)ve->m_nModelIndex) + "@" + std::to_string((int)p2.x) + "," + std::to_string((int)p2.y) + " d" + std::to_string((int)(p2 - pp).Magnitude()) + (ve->m_fHealth > 0 ? "" : "X") + " spd" + std::to_string((int)(ve->m_vecMoveSpeed.Magnitude() * 100)); } else o += " L" + std::to_string(li) + "=-"; }
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
            if (std::getenv("NOTSA_MDRIVER_CARLOGNEAR") && fr >= std::atoi(std::getenv("NOTSA_MDRIVER_CARLOGNEAR"))) for (auto& vv : GetVehiclePool()->GetAllValid()) { if (&vv == cr || (vv.GetPosition() - cr->GetPosition()).Magnitude() > 120.f) continue; auto* dd = vv.m_pDriver; notsa::standalone::Fixups::Log("[carlog]   near: ref %d model %d at %.0f,%.0f hp %.0f st %d drv %d%s spd %.2f", GetVehiclePool()->GetRef(&vv), (int)vv.m_nModelIndex, vv.GetPosition().x, vv.GetPosition().y, (double)vv.m_fHealth, (int)vv.GetStatus(), dd ? (int)dd->m_nModelIndex : -1, vv.IsMissionVehicle() ? " M" : "", (double)vv.m_vecMoveSpeed.Magnitude()); for (auto& pp2 : GetPedPool()->GetAllValid()) { if (&pp2 == Cj() || (pp2.GetPosition() - cr->GetPosition()).Magnitude() > 100.f) continue; notsa::standalone::Fixups::Log("[carlog]   nearped: ref %d model %d at %.0f,%.0f hp %.0f type %d wpn %d%s", GetPedPool()->GetRef(&pp2), (int)pp2.m_nModelIndex, pp2.GetPosition().x, pp2.GetPosition().y, (double)pp2.m_fHealth, (int)pp2.m_nPedType, (int)pp2.m_nActiveWeaponSlot, pp2.bInVehicle ? " v" : ""); } } // CR42 debug
            notsa::standalone::Fixups::Log("[carlog] f%d car=%.1f,%.1f,%.1f spd=%.3f st=%d drv=%d dmode=%d tasks=%s ip=%x", fr, cr->GetPosition().x, cr->GetPosition().y, cr->GetPosition().z, cr->m_vecMoveSpeed.Magnitude(), (int)cr->GetStatus(), dr ? (int)dr->m_nModelIndex : -1, (int)cr->m_autoPilot.m_nCarMission, tk.c_str(), (unsigned)(root->m_IP - root->m_BaseIP));
        }
    }
    if (static const char* ag = std::getenv("NOTSA_MDRIVER_AUXLOG"); ag && root) { // debug (no control flow): every change of the listed globals
        static std::map<int, int32_t> last; for (const char* c = ag; *c;) { const int off = std::atoi(c); while (*c && *c != ',') c++; if (*c == ',') c++; if (!GlobalOk(off)) continue; int32_t v; std::memcpy(&v, CTheScripts::ScriptSpace.data() + off, 4); auto it = last.find(off); if (it == last.end()) last[off] = v; else if (it->second != v) { notsa::standalone::Fixups::Log("[auxchg] f%d $%d %d -> %d (menus in use %d,%d)", fr, off, it->second, v, (int)CMenuSystem::MenuInUse[0], (int)CMenuSystem::MenuInUse[1]); it->second = v; } }
    }
    if (!std::getenv("NOTSA_MDRIVER_NOGOD")) if (auto* me = Cj()) { // CR4: TS drive.ts driverFrame: CJ and the vehicle he drives are invulnerable every frame (AUTODRIVE_GOD=0 off)
        me->physicalFlags.bInvulnerable = true;
        if (me->bInVehicle && me->m_pVehicle) me->m_pVehicle->physicalFlags.bInvulnerable = true;
    }
    if (fr % 30 == 0 && std::getenv("NOTSA_MDRIVER_CJLOG")) if (auto* me9 = Cj()) notsa::standalone::Fixups::Log("[cjlog] frame %d hp %.0f pos %.1f %.1f %.1f veh=%d anim state %d inv=%d lastdmg ent=%d type=%d model=%d wpn=%d fire=%d vmodel=%d vhp=%.0f vinv=%d", fr, (double)me9->m_fHealth, (double)me9->GetPosition().x, (double)me9->GetPosition().y, (double)me9->GetPosition().z, (int)me9->bInVehicle, (int)me9->m_nPedState, (int)me9->physicalFlags.bInvulnerable, me9->m_pLastEntityDamage ? 1 : 0, me9->m_pLastEntityDamage ? (int)me9->m_pLastEntityDamage->GetType() : -1, me9->m_pLastEntityDamage ? (int)me9->m_pLastEntityDamage->m_nModelIndex : -1, (int)me9->m_nLastWeaponDamage, (int)(me9->m_pFire != nullptr), me9->bInVehicle && me9->m_pVehicle ? (int)me9->m_pVehicle->m_nModelIndex : -1, me9->bInVehicle && me9->m_pVehicle ? (double)me9->m_pVehicle->m_fHealth : -1.0, me9->bInVehicle && me9->m_pVehicle ? (int)me9->m_pVehicle->physicalFlags.bInvulnerable : -1); // CR49 debug
    if (fr % 90 == 0 && std::getenv("NOTSA_MDRIVER_CJLOG") && std::getenv("NOTSA_MDRIVER_PEDSLOG")) if (auto* me8 = Cj()) for (auto& pp : GetPedPool()->GetAllValid()) { if (&pp == me8 || (pp.GetPosition() - me8->GetPosition()).Magnitude() > 150.f) continue; notsa::standalone::Fixups::Log("[pedslog] frame %d ped %d model %d type %d hp %.0f pos %.1f %.1f %.1f inveh=%d sameveh=%d dist %.0f", fr, CPools::GetPedRef(&pp), (int)pp.m_nModelIndex, (int)pp.m_nPedType, (double)pp.m_fHealth, (double)pp.GetPosition().x, (double)pp.GetPosition().y, (double)pp.GetPosition().z, (int)pp.bInVehicle, (int)(pp.bInVehicle && pp.m_pVehicle == me8->m_pVehicle), (double)(pp.GetPosition() - me8->GetPosition()).Magnitude()); } // CR49 debug
    if (fr % 30 == 0 && !std::getenv("NOTSA_MDRIVER_NOURGENT")) { // CR31 (26 ROBBING UNCLE SAM: army fire on the truck CJ sits in): the vehicle of CJ lost >= 45 health in each of 3 consecutive 30-frame windows = danger for 120 frames (shorter patience, CJ drives to the destination blip)
        auto* me2 = Cj(); auto* v2 = me2 && me2->bInVehicle ? me2->m_pVehicle : nullptr;
        if (!v2) { g_vhpVeh = nullptr; g_vhpPrev = -1; g_dropWin = 0; }
        else { const int h2 = (int)v2->m_fHealth; if (g_vhpVeh == v2 && g_vhpPrev >= 0 && h2 <= g_vhpPrev - 45) g_dropWin++; else g_dropWin = 0; g_vhpVeh = v2; g_vhpPrev = h2; if (g_dropWin >= 3) { if (fr >= g_dangerUntil) notsa::standalone::Fixups::Log("[mdriver] danger frame %d: the vehicle keeps losing health (%d)", fr, h2); g_dangerUntil = fr + 120; } }
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
    else if (root->m_IP == g_lastIp) { g_sameCount++; g_ipSeen[root->m_IP] = fr; }
    else if (g_lastIp && g_ipSeen.count(root->m_IP) && fr - g_ipSeen[root->m_IP] < g_stall && std::getenv("NOTSA_MDRIVER_IPRING")) { g_sameCount++; g_lastIp = root->m_IP; g_ipSeen[root->m_IP] = fr; } // CR46 (72 AIR RAID: the wake ip alternated 158e / 104 / 797 / 14d9 for 6500 frames and the stall counter never reached 600): a wake ip seen within the last stall window is the same loop
    else { g_ipSeen[root->m_IP] = fr; g_sameCount = 0; g_lastIp = root->m_IP; if (g_fastLeft > 0 && g_fastIp != root->m_IP) { g_fastLeft = 0; if (g_fastNext) g_nextStall = 0; g_fastNext = false; } }
    if (g_sameCount >= g_stall) for (auto& r : g_recs) if (r->waitSeq >= g_waitSeq - 3 && (r->raw & 0x7FFF) == 0x010F && r->cond == 0) { if (auto* w = FindPlayerWanted()) if (w->m_WantedLevel != eWantedLevel::WANTED_CLEAN) { w->SetWantedLevel(eWantedLevel::WANTED_CLEAN); g_sameCount = 0; return; } break; } // CR11: as drive.ts:309 (at the stall threshold, then no decide this round; level only): // CR9: the thread waits for the wanted level to drop (SWEET4 ip 0x45e7 after the second pay n spray stage; the exe decays above 1 star only in the countryside, a real player drives into the pay n spray = ClearWantedLevelAndGoOnParole 0x44AC16): clear it, as the TS port does implicitly
    bool urgentTimer = false; // CR31 (25 CATALYST: the 90 s script timer runs while the driver waits 600 frames at every decision): a countdown clock on the screen shortens the patience to 150 frames; a vehicle of CJ that keeps losing health (army fire, 26) to 90 (NOTSA_MDRIVER_NOURGENT)
    if (std::getenv("NOTSA_MDRIVER_TIMERURGENT")) { // opt-in: on by default it broke 20 LOS SEPULCROS (FAILED TWAR646) and moved 74, and did not make 25 pass
        const auto& ck = CUserDisplay::OnscnTimer.m_Clock;
        urgentTimer = ck.m_nVarId != 0 && !CUserDisplay::OnscnTimer.m_bPaused && ck.m_nTimerDirection == eTimerDirection::DECREASE;
    }
    bool chaseNear = false; if (g_nextStall == 0 && g_sameCount >= 150 && std::getenv("NOTSA_MDRIVER_CHASEFAST")) for (auto& r : g_recs) { const int rop = r->raw & 0x7FFF; if (r->waitSeq >= g_waitSeq - 3 && r->cond == 0 && !(r->raw & 0x8000) && ((rop >= 0x00F2 && rop <= 0x00F4) || (rop >= 0x0104 && rop <= 0x0106))) { chaseNear = true; break; } } // CR44 (67 ZEROING IN: the chase stage's `CJ near the car's driver` locate is false while the blip fades, the script fails after ~500 frames, the first stall is 600): a false CJ-near-char locate decides after 200 frames
    if (g_nextStall > 0 ? g_sameCount < g_nextStall : g_sameCount < (fr < g_dangerUntil ? std::min(g_stall, 90) : urgentTimer ? std::min(g_stall, 150) : fr < g_pbNearUntil ? std::min(g_stall, 60) : chaseNear ? std::min(g_stall, 200) : g_stall)) return;
    const uint8_t* ipPtrEarly = root->m_IP;
    const bool earlyAux = g_auxEarly; g_auxEarly = false; // CR27: the early re-decision after an aux answer answers the aux thread only
    const bool earlyPhoto = g_photoEarly; g_photoEarly = false; // CR26: the early re-decision after a zoomed photo answers a photo condition only (no wait / kill escalation of a half-evaluated pass)
    const bool fastRep = g_fastLeft > 0 && g_fastIp == ipPtrEarly && g_fastNext; g_fastNext = false;
    if (fastRep) { g_fastLeft--; g_nextStall = 0; g_sameCount = 0; notsa::standalone::Fixups::Log("[mdriver] fastkey frame %d ip %x left %d", fr, (unsigned)(root->m_IP - root->m_BaseIP), g_fastLeft); DispatchRow(g_fastDec); if (g_fastLeft > 0) { g_nextStall = 30; g_fastNext = true; } return; } // CR30: the same key again (the pass' button condition is unchanged after the press)
    g_nextStall = 0; // CR24: a verb may ask for an early next decision (the bomb run of the remote vehicle: the script's 4 s hold timer)
    const uint32_t at = (uint32_t)(root->m_IP - root->m_BaseIP);
    const uint8_t* ipPtr = root->m_IP;
    g_sameCount = 0;
    std::vector<RecP> loop; for (auto& r : g_recs) if (r->waitSeq >= g_waitSeq - 3) loop.push_back(r);
    if (std::getenv("NOTSA_MDRIVER_BIKELOG")) { std::string o; for (auto& r : loop) if (r->cond == 0) { o += " " + std::to_string((unsigned)(r->p - root->m_BaseIP)) + "/" + std::to_string(r->raw & 0x7FFF); } notsa::standalone::Fixups::Log("[decide] frame %d attempt %d loop %zu falses:%s", fr, g_attemptsAt[ipPtr], loop.size(), o.c_str()); }
    Dec dn; bool nearRec = false; int nearH = -1;
    if (g_attemptsAt[ipPtr] >= (g_nearCnt.empty() ? 4 : 1) && !std::getenv("NOTSA_MDRIVER_NONEARPED")) for (size_t k = g_recsBig.size(); k-- > 0;) { const auto& c = g_recsBig[k]; const int op = c->raw & 0x7FFF; if (c->cond != 0 || (c->raw & 0x8000) || !((op >= 0x00F2 && op <= 0x00F4) || (op >= 0x0104 && op <= 0x0106)) || c->waitSeq < g_waitSeq - 3) continue; // CR32 (27 OG LOC): a plain false `CJ near ped X` (CJ the first operand; the stage waits for the player to catch up with X, here a recorded-path car that parks at the end of its leg): after the first answer failed, CJ joins X (X is not moved: it belongs to the script's path)
                const auto h0 = ArgInt(*c, 0), h1 = ArgInt(*c, 1);
                if (h0 && h1 && CPools::GetPed((int)*h0) == Cj()) if (auto* x = CPools::GetPed((int)*h1); x && x != Cj() && x->m_fHealth > 0.f && (!g_nearCnt.empty() || (x->bInVehicle && x->m_pVehicle && x->m_pVehicle->m_vecMoveSpeed.Magnitude() < 0.02f && (!Cj()->bInVehicle || Cj()->m_pVehicle != x->m_pVehicle)))) { static Row kNear{ op, 0, "LOCATE_CHAR_CHAR (CJ near ped)", "zonego", {}, 60 }, kFetch{ op, 0, "LOCATE_CHAR_CHAR (ped to CJ)", "nearfetch", {}, 60 }; const CVector xp = x->GetRealPosition(); const int nc = g_nearCnt.count((int)*h1) ? g_nearCnt[(int)*h1] : 0; /* CR33: operator[] created the entry, so g_nearCnt was never empty and every later answer was replaced */ nearH = (int)*h1; const bool nz = !std::getenv("NOTSA_MDRIVER_NONEARZ"); /* CR33: the ped's own height, not the ground under it (11: the ground z there is the freeway bridge above the ped) */ // the 1st answer: CJ joins the ped (a script-driven car that parks); the later ones: the ped joins CJ (a buddy that must come along to the place the other conditions ask for)
                    dn.act = true; dn.row = nc == 0 ? &kNear : &kFetch; dn.rowIdx = -13; dn.cond = c; dn.op = op; dn.args.clear(); if (nc == 0) for (const float f : { xp.x + 5.f, xp.y, nz ? xp.z : -100.f }) { RArg ra; ra.n = f; dn.args.push_back(ra); } else { const CVector cp = Cj()->GetPosition(); for (const float f : { (float)*h1, cp.x + 3.f, cp.y, nz ? cp.z - 1.f : -100.f }) { RArg ra; ra.n = f; dn.args.push_back(ra); } }
                    notsa::standalone::Fixups::Log("[mdriver] nearped: CJ joins ped %d at %.0f,%.0f", (int)*h1, (double)xp.x, (double)xp.y); nearRec = true; break; } }
    Dec d = Decide(loop, g_attemptsAt[ipPtr]);
    if (d.act && d.row->action == "midpoint" && d.cond && (d.cond->raw & 0x7FFF) == 0x00A3 && g_haveLastBlip && d.args.size() == 4 && g_thread && OneShotWarning(g_thread, *d.cond) && !std::getenv("NOTSA_MDRIVER_NOWARNAREA")) { // CR48: the answer would put CJ's plane into a warning area (Area 51: SAMs): the destination blip of the script is the answer when it lies outside the box
        const float bx0 = std::min((float)d.args[0].n, (float)d.args[2].n), bx1 = std::max((float)d.args[0].n, (float)d.args[2].n), by0 = std::min((float)d.args[1].n, (float)d.args[3].n), by1 = std::max((float)d.args[1].n, (float)d.args[3].n);
        bool blipLive = false; for (auto& t : CRadar::ms_RadarTrace) if (t.m_nBlipType == BLIP_COORD && !t.HasSprite() && (t.m_vPosition - g_lastBlipPos).Magnitude2D() < 2.f) blipLive = true; // the blip still exists (the stage removes it when it is done)
        if (blipLive && (g_lastBlipPos.x < bx0 || g_lastBlipPos.x > bx1 || g_lastBlipPos.y < by0 || g_lastBlipPos.y > by1)) {
            static Row kWarn{ 0, 0, "destination blip (the area answer is a one-shot warning)", "blipgo", {}, 120 };
            notsa::standalone::Fixups::Log("[mdriver] warnarea frame %d: the area test is a one-shot warning, CJ goes to the script's last blip %.0f,%.0f", fr, g_lastBlipPos.x, g_lastBlipPos.y);
            d.row = &kWarn; d.rowIdx = -7; d.cond = nullptr; d.op = 0; d.args.clear();
            for (const float f : { g_lastBlipPos.x, g_lastBlipPos.y, std::fabs(g_lastBlipPos.z) < 0.5f ? -100.f : g_lastBlipPos.z }) { RArg ra; ra.n = f; d.args.push_back(ra); }
        }
    }
    if ((!d.act || d.row->action == "wait") && g_attemptsAt[ipPtr] >= 0 && !std::getenv("NOTSA_MDRIVER_NOCLOCKSKIP")) for (auto& r : g_recsBig) { // CR32 (33 MANAGEMENT ISSUES: `WAIT UNTIL hour > 21`, the hour copied by GET_TIME_OF_DAY into a local; the idle player is arrested long before the game clock gets there): a false `local > N` whose local holds the current game hour is answered by the clock moving to N+1:00 (the player waits for the night, or sleeps)
        if (r->waitSeq < g_waitSeq - 3 || (r->raw & 0x7FFF) != 0x0019 || (r->raw & 0x8000) || r->cond != 0) continue;
        if (!std::getenv("NOTSA_MDRIVER_CLOCKGUARD") && VerdictOf(g_thread, *r) == Verdict::Guard) continue; /* CR34 (33: `IF hour > 21` jumps FORWARD when false and its true branch is the mission's failure (a time limit): moving the clock made the mission fail at f5553) */
        const auto v = ArgInt(*r, 0), n = ArgInt(*r, 1); if (!v || !n || *v != (int)CClock::GetGameClockHours() || *n < *v || *n > 22) continue;
        notsa::standalone::Fixups::Log("[mdriver] clockskip frame %d: the wait is for hour > %d (now %d): the game clock goes to %d:00", fr, (int)*n, (int)*v, (int)*n + 1); CClock::SetGameClock((uint8)(*n + 1), 0, CClock::GetGameClockDays()); QueueActions({}, 60); return; }
    if ((!d.act || d.row->action == "wait") && !std::getenv("NOTSA_MDRIVER_NOWIDE") && g_recs.size() > loop.size()) { // CR23 (16 NINES AND AK'S): the loop window (the last 3 passes) is one frame of a script that tests ONE array element per pass (@42 = 0..4, 5 passes per revolution) and the stall frame is phase-locked to the slot of an object that no longer exists: the answer is the plain `wait` of the end-of-stage locate, the target that can be shot is only in older passes. Rule: when the narrow window gives only `wait` (or nothing), look at every retained pass of the stalled loop (the 64 records, all from the stall period) and take a non-wait answer from there.
        const std::vector<RecP> wide(g_recs.begin(), g_recs.end());
        Dec d2 = Decide(wide, g_attemptsAt[ipPtr], true);
        if (d2.act && d2.row->action != "wait") { std::fprintf(stderr, "[mdriver] frame %d widened window (%zu -> %zu records): %s\n", fr, loop.size(), wide.size(), d2.row->name.c_str()); d = d2; }
    }
    if (earlyPhoto && !(d.act && d.row->action == "photo")) return;
    if (!d.act) { Stop(d.reason, d.op, at); return; }
    if (d.act && fr < g_dangerUntil && d.row->action != "blipgo" && !std::getenv("NOTSA_MDRIVER_NOFLEE")) { // CR31 (26): CJ is driving a vehicle that is being shot to pieces: whatever the pass asks, a player drives off to the destination blip (the newest coordinate blip of the radar) -- the vehicle is moved there
        auto* me3 = Cj(); const tRadarTrace* bb = nullptr;
        for (auto& t : CRadar::ms_RadarTrace) if (t.m_nBlipType == BLIP_COORD && !t.HasSprite() && (!bb || t.m_nCounter >= bb->m_nCounter)) bb = &t;
        if (bb && me3 && me3->bInVehicle && (me3->GetPosition() - bb->m_vPosition).Magnitude2D() > 30.f) {
            static Row kFlee{ 0, 0, "destination blip (the vehicle is under fire)", "blipgo", {}, 120 };
            d.row = &kFlee; d.rowIdx = -16; d.cond = nullptr; d.op = 0; d.args.clear();
            for (const float f : { bb->m_vPosition.x, bb->m_vPosition.y, std::fabs(bb->m_vPosition.z) < 0.5f ? -100.f : bb->m_vPosition.z }) { RArg a; a.n = f; d.args.push_back(a); }
        }
    }
    if (d.act && d.row->action == "wait" && g_attemptsAt[ipPtr] >= 1 && !std::getenv("NOTSA_MDRIVER_NOESCALATE")) { // CR21: the wait row answered this wake ip before and the thread still stands: a TRUE `NOT IS_CHAR_DEAD(x)` of the pass is a wait-until-dead (89: the paramedic passengers of the three ambulances stay alive once the cruising ambulances moved on; the plain 0118 rows only see the drivers): kill that live non-buddy char
        for (const int kop : { 0x0118, 0x0119 }) { // CR23 (16 NINES AND AK'S, last stage: `WHILE NOT IS_CAR_DEAD(@83)`): the car form too (the plain 0119 kill row wrecks the car; a car with a buddy / CJ in it is refused by ProtectedHandle)
        if (kop == 0x0119 && std::getenv("NOTSA_MDRIVER_NOCARESCALATE")) break;
        const Row* kr = nullptr; int kri = -1; for (size_t i = 0; i < g_rows.size(); i++) if (g_rows[i].op == kop && g_rows[i].match == 1 && g_rows[i].action == "kill") { kr = &g_rows[i]; kri = (int)i; break; }
        if (kr && d.row == kr) break;
        if (kr) for (size_t k = loop.size(); k-- > 0;) {
            const auto& r = loop[k]; if (r->raw != (0x8000 | kop) || r->cond != 1 || AnyGuard(*r)) continue;
            if (kop == 0x0118 && !(r->waitAfter && !r->jumped) && std::getenv("NOTSA_MDRIVER_PEDWAITONLY")) continue; // CR47 (84 FENDER KETCHUP: the escalation killed the guard of the stunt car; opt-in: see FINDINGS)
            if (kop == 0x0119 && !(r->waitAfter && !r->jumped)) continue; // the car form only when the TRUE branch IS the WAIT of its group (`WHILE NOT IS_CAR_DEAD(x)`, 16): elsewhere `NOT IS_CAR_DEAD` is a plain if (10: the RC plane @34 whose death fails the mission)
            const auto a2 = ResolveArgs(*kr, *r); if (!a2 || a2->empty()) continue;
            const int h = (int)(*a2)[0].n;
            if (kop == 0x0118 && !std::getenv("NOTSA_MDRIVER_NOPHOTO") && std::any_of(g_recs.begin(), g_recs.end(), [&](const RecP& q) { const auto qh = ArgInt(*q, 0); return (q->raw & 0x7FFF) == 0x04C5 && qh && (int)*qh == h; })) continue; // CR25 (58): a ped the script wants PHOTOGRAPHED is no wait-until-dead (`NOT IS_CHAR_DEAD(x) AND HAS_CHAR_BEEN_PHOTOGRAPHED(x)`)
            if (kop == 0x0118) { auto* pp = CPools::GetPed(h); if (!pp || pp == Cj() || pp->m_fHealth <= 0.f || ProtectedHandle(h) || GuardedOf(false).count(h)) continue; }
            else { auto* vv = CPools::GetVehicle(h); if (!vv || vv->m_fHealth <= 0.f || ProtectedHandle(h) || (Cj() && Cj()->m_pVehicle == vv)) continue; }
            d.row = kr; d.rowIdx = kri; d.cond = r; d.args = *a2; d.op = kop; break;
        }
        if (d.row == kr) break;
        }
    }
    if (d.act && d.row->action == "midpoint" && d.args.size() == 6 && Cj() && Cj()->bInVehicle && Cj()->m_pVehicle && Cj()->m_pVehicle->IsSubPlane() && !std::getenv("NOTSA_MDRIVER_NOPLANELOW")) { // CR49 (91 FREEFALL: `IS_CHAR_IN_AREA_3D` of a low box (z 7..14) over the ground while CJ is in a flying plane: the midpoint warp put the plane on the ground and wrecked it): no warp, the flight goes on
        const float zc = (float)((d.args[2].n + d.args[5].n) * 0.5), xc = (float)((d.args[0].n + d.args[3].n) * 0.5), yc = (float)((d.args[1].n + d.args[4].n) * 0.5);
        if (zc - CWorld::FindGroundZForCoord(xc, yc) < 20.f) { static Row kPlaneLow{ 0, 0, "low area box while flying: the plane lands there", "planeland", {}, 60 }; d.row = &kPlaneLow; d.rowIdx = -42; d.args.clear(); for (const float f : { xc, yc, -100.f }) { RArg ra; ra.n = f; d.args.push_back(ra); } notsa::standalone::Fixups::Log("[mdriver] planelow: the low box is a landing: the plane is put down there, wheels down"); }
    }
    if (d.act && d.row->action == "pedwarp" && g_attemptsAt[ipPtr] >= 1 && Cj() && !std::getenv("NOTSA_MDRIVER_NOINMODEL2")) { // CR49 (85 EXPLOSIVE SITUATION, crate stage: the pass also holds `LOCATE_CHAR_IN_CAR_3D CJ crate / IS_CHAR_IN_MODEL CJ 406 OR 486` and the on-foot locates answered first never move the script): a plain false LOCATE_CHAR_IN_CAR_3D of CJ whose condition group wants a car model CJ is not in is answered by the vehicle row (CJ gets into that car, the car goes to the crate)
        const Row* vr = nullptr; int vri = -1; for (size_t i = 0; i < g_rows.size(); i++) if (g_rows[i].op == 0x0100 && g_rows[i].match == 1 && g_rows[i].action == "vehwarp") { vr = &g_rows[i]; vri = (int)i; break; }
        if (vr) for (size_t k = g_recsBig.size(); k-- > 0;) { const auto& r = g_recsBig[k]; if (r->waitSeq < g_waitSeq - 3 || r->raw != 0x0100 || r->cond != 0) continue;
            const auto h0 = ArgInt(*r, 0); if (!h0 || CPools::GetPed((int)*h0) != Cj()) continue;
            const auto wms = ModelsWantedAfterCarLocate(r->p); if (wms.empty()) continue;
            const auto a2 = ResolveArgs(*vr, *r); if (!a2 || a2->size() < 3) continue;
            d.row = vr; d.rowIdx = vri; d.cond = r; d.args = *a2; d.op = 0x0100;
            if (const auto rx = ArgFloat(*r, 4); rx && *rx >= 2.0 && !std::getenv("NOTSA_MDRIVER_NOEDGEWARP")) d.args[0].n += *rx * (std::getenv("NOTSA_MDRIVER_EDGEF") ? std::atof(std::getenv("NOTSA_MDRIVER_EDGEF")) : 0.75); /* the centre of a crate is inside its collision: the vehicle lands at the edge of the box */
            break; }
    }
    if (d.act && d.row->action != "kill" && d.row->action != "wait" && g_attemptsAt[ipPtr] >= 1 && !std::getenv("NOTSA_MDRIVER_NOTIMERKILL")) { // CR48 (85 EXPLOSIVE SITUATION: the stage loop fails at `timer <= 1.8 s AND NOT IS_CHAR_DEAD(phone ped)`; the answers are warps to twelve locates, the timer runs out): a decreasing on-screen timer, the answer did not move the script and the retained passes (400 records, the narrow window misses it) hold a plain false IS_CHAR_DEAD(x) of a live non-buddy char: he dies (the stage fails when the timer ends)
        const auto& ck = CUserDisplay::OnscnTimer.m_Clock;
        if (ck.m_nVarId != 0 && !CUserDisplay::OnscnTimer.m_bPaused && ck.m_nTimerDirection == eTimerDirection::DECREASE) {
            const Row* kr = nullptr; int kri = -1; for (size_t i = 0; i < g_rows.size(); i++) if (g_rows[i].op == 0x0118 && g_rows[i].match == 1 && g_rows[i].action == "kill") { kr = &g_rows[i]; kri = (int)i; break; }
            std::vector<RecP> tk; for (auto& r : g_recsBig) if (r->waitSeq >= g_waitSeq - 3) tk.push_back(r);
            if (kr) for (size_t k = tk.size(); k-- > 0;) {
                const auto& r = tk[k]; if (r->raw != 0x0118 || r->cond != 0) continue;
                const auto a2 = ResolveArgs(*kr, *r); if (!a2 || a2->empty()) continue;
                const int h = (int)(*a2)[0].n; auto* pp = CPools::GetPed(h); if (!pp || pp == Cj() || pp->m_fHealth <= 0.f || ProtectedHandle(h) || GuardedOf(false).count(h)) continue;
                notsa::standalone::Fixups::Log("[mdriver] timerkill frame %d: a countdown runs, ped %d: plain IS_CHAR_DEAD is false", fr, h);
                d.row = kr; d.rowIdx = kri; d.cond = r; d.args = *a2; d.op = 0x0118; break; }
        }
    }
    if (d.act && d.row->action == "wait" && g_attemptsAt[ipPtr] >= 2 && !std::getenv("NOTSA_MDRIVER_NOBIGWIN") && g_recsBig.size() > g_recs.size()) { // CR26 (20 LOS SEPULCROS: a pass of ~100 conditions, the 64-condition window holds only its last third; the stage test `IS_CHAR_SITTING_IN_CAR` / the three IS_CHAR_DEAD of the pass' first half are outside it): the wait row answered this wake ip twice and the thread still stands: decide again over the whole last 3 passes (up to 400 conditions); a non-wait answer wins
        std::vector<RecP> big; for (auto& r : g_recsBig) if (r->waitSeq >= g_waitSeq - 3) big.push_back(r);
        g_bigCall = true; Dec d3 = Decide(big, g_attemptsAt[ipPtr]); g_bigCall = false;
        if (d3.act && d3.row->action != "wait") { notsa::standalone::Fixups::Log("[mdriver] frame %d big window (%zu -> %zu records): %s", fr, loop.size(), big.size(), d3.row->name.c_str()); d = d3; }
    }
    if (d.act && d.row->action == "wait" && d.cond && (d.cond->raw & 0x8000) && d.cond->cond == 0 && g_attemptsAt[ipPtr] >= 2 && Cj() && !std::getenv("NOTSA_MDRIVER_NOLEAVELOC")) { // CR45 (68 TEST DRIVE: `NOT LOCATE_CHAR_ANY_MEANS_3D CJ (-1656,1210,12) 7 7 7`, the stage waits for CJ to step out of the start place): a NOT place locate of CJ that is false twice (CJ stands inside) is answered by moving CJ just outside the box
        const int rop = d.cond->raw & 0x7FFF; const bool d3 = rop >= 0x00FE && rop <= 0x0103, d2 = rop >= 0x00EC && rop <= 0x00F1; const auto h0 = ArgInt(*d.cond, 0);
        if ((d2 || d3) && h0 && CPools::GetPed((int)*h0) == Cj()) { const auto cx = ArgFloat(*d.cond, 1), cy = ArgFloat(*d.cond, 2), rx = ArgFloat(*d.cond, d3 ? 4 : 3);
            if (cx && cy && rx && *rx < 100.0) { static Row kLeave{ 0, 0, "NOT locate of CJ: CJ steps out of the box", "pedwarp", {}, 60 }, kLeaveV{ 0, 0, "NOT locate of CJ: CJ drives out of the box", "vehwarp", {}, 60 }; d.row = Cj()->bInVehicle ? &kLeaveV : &kLeave; d.rowIdx = -31; d.args.clear(); for (const double v : { *cx + *rx + 8.0, *cy, -100.0 }) { RArg ra; ra.n = v; d.args.push_back(ra); }
                notsa::standalone::Fixups::Log("[mdriver] leaveloc: CJ is inside the NOT locate at %.0f,%.0f (%.0f): CJ steps out", *cx, *cy, *rx); } }
    }
    if (d.act && d.row->action == "nearest_car" && d.cond && g_attemptsAt[ipPtr] == 0 && g_recsBig.size() > loop.size() && !std::getenv("NOTSA_MDRIVER_NONEARCARBIG")) { // CR45 (68 TEST DRIVE: the tail of a >150-condition pass holds `IS_CHAR_IN_ANY_CAR buddy` of a dialogue IF; the real blocker, a locate of CJ at the mission's blip, lies outside the 64-condition window): a car answer about ANOTHER char is no first answer: look at the last 3 passes; a non-car answer wins
        const auto hh = ArgInt(*d.cond, 0); auto* oth = hh ? CPools::GetPed((int)*hh) : nullptr;
        if (oth && oth != Cj()) { std::vector<RecP> big; for (auto& r : g_recsBig) if (r->waitSeq >= g_waitSeq - 3) big.push_back(r);
            g_bigCall = true; Dec d3 = Decide(big, g_attemptsAt[ipPtr]); g_bigCall = false; if (d3.act && d3.row->action != "wait" && d3.row->action != "nearest_car") { notsa::standalone::Fixups::Log("[mdriver] frame %d nearest_car of another char: big window (%zu -> %zu records): %s", fr, loop.size(), big.size(), d3.row->name.c_str()); d = d3; } }
    }
    if (d.act && d.row->action == "vehwarp" && (g_attemptsAt[ipPtr] % 2) == 1 && g_recsBig.size() > loop.size() && !std::getenv("NOTSA_MDRIVER_NOKEYBEFOREWARP")) { // CR46 (73 SUPPLY LINES: the stage's `IS_BUTTON_PRESSED 16` (the RC plane's boost; its true branch drains a meter) is a plain false test at the start of the pass, outside the 64-condition window; the vehicle-warp answers of the enemy-car proximity tests dragged hostile cars onto the parked plane every 660 frames): on every second vehicle-warp answer the button test of the whole last passes is pressed first
        std::vector<RecP> kb; for (auto& r : g_recsBig) if (r->waitSeq >= g_waitSeq - 3 && (r->raw & 0xFFFF) == 0x00E1 && r->cond == 0 && !AnyGuard(*r)) kb.push_back(r);
        if (!kb.empty()) { Dec d3 = Decide(kb, 3); if (d3.act && d3.row->action == "key") { notsa::standalone::Fixups::Log("[mdriver] frame %d key before a vehicle warp: %s", fr, d3.row->name.c_str()); d = d3; } }
    }
    if (d.act && !std::getenv("NOTSA_MDRIVER_NOAUX")) { // CR27: see AuxState: the root's wait compares a global that a streamed script thread writes: answer THAT thread's conditions (locate / button of the menu) with the same rows; a non-wait answer wins. Once such a thread was answered it goes first (the root's own warps would pull CJ away from it); when the root's pass no longer compares a global of it, it is done (g_auxDone)
        std::set<uint16_t> gl; for (auto& r : g_recsBig) { if (r->waitSeq < g_waitSeq - 3 || !IntCompareOp(r->raw & 0x7FFF)) continue; for (int k = 0; k < 2; k++) if ((r->vref[k] >> 16) == 2) gl.insert((uint16_t)(r->vref[k] & 0xFFFF)); }
        bool auxRel = false; for (auto* t = CTheScripts::pActiveScripts; t && !auxRel && !gl.empty(); t = t->m_pNext) { auto it = g_aux.find(t); if (it == g_aux.end() || it->second.recs.empty()) continue; for (const auto g : gl) auxRel = auxRel || WritesGlobal(t, g); }
        if (g_auxTouched && !auxRel) { g_auxTouched = false; g_auxDone = true; }
        const bool auxTrig = d.row->action == "wait" ? g_attemptsAt[ipPtr] >= 2 : (g_auxTouched || (d.row->action == "key" && g_attemptsAt[ipPtr] >= 2 && !std::getenv("NOTSA_MDRIVER_NOAUXKEY"))); // CR29: a root key answer repeated at the same wake ip (19: the stage presses SELECT, the lowrider game thread that holds $4068 polls TRIANGLE) also hands over to the aux thread
        for (auto* t = CTheScripts::pActiveScripts; t && auxRel && auxTrig && !gl.empty(); t = t->m_pNext) {
            auto it = g_aux.find(t); if (it == g_aux.end() || it->second.recs.empty()) continue;
            bool wr = false; for (const auto g : gl) wr = wr || WritesGlobal(t, g); if (!wr) continue;
            std::vector<RecP> al; for (auto& r : it->second.recs) if (r->waitSeq >= it->second.waitSeq - 3) al.push_back(r);
            if (const char* lg = std::getenv("NOTSA_MDRIVER_AUXLOG")) { // debug (no control flow): the false conditions of the aux thread's pass (ip/opcode/NOT/args) + the globals "off,off,.." of the env value
                std::string o; for (auto& r : al) { if (r->cond != 0) continue; char q[96]; std::snprintf(q, sizeof q, " %x/%04X%s(", (unsigned)(r->p - t->m_BaseIP), r->raw & 0x7FFF, (r->raw & 0x8000) ? "!" : ""); o += q; for (size_t k = 0; k < r->args.size() && k < 4; k++) { std::snprintf(q, sizeof q, "%s%d", k ? "," : "", (int)r->args[k].bits); o += q; } o += ")"; }
                std::string gv; for (const char* c = lg; *c;) { const int off = std::atoi(c); while (*c && *c != ',') c++; if (*c == ',') c++; int32_t v = 0; if (GlobalOk(off)) std::memcpy(&v, CTheScripts::ScriptSpace.data() + off, 4); gv += " $" + std::to_string(off) + "=" + std::to_string(v); }
                notsa::standalone::Fixups::Log("[auxlog] f%d falses:%s |%s", fr, o.c_str(), gv.c_str());
            }
            // the thread's own state = which conditions of its passes are false (a big state machine loop has one WAIT): the n-th decision in the same state rotates through the answers (accept first, cancel next: a menu whose accept leads nowhere is left by its cancel button)
            uint32_t ak = 7; { std::set<uint32_t> fs; for (auto& r : al) if (r->cond == 0) fs.insert((uint32_t)(r->p - t->m_BaseIP)); for (const auto f : fs) ak = ak * 1000003u + f; } // the state = the set of conditions that are false in its last passes
            const int visit = g_auxVisits[ak]++;
            g_auxMode = true; Dec d4 = Decide(al, visit + 3); g_auxMode = false; // +3: no key-deferral (a streamed script polls its buttons while other conditions of its pass are false too)
            if (d4.act && d4.row->action == "key" && (CMenuSystem::MenuInUse[0] || CMenuSystem::MenuInUse[1]) && !std::getenv("NOTSA_MDRIVER_NOMENUACCEPT")) { // CR27: a menu is up: the accept button (CROSS = 16) first, then the others
                std::vector<RecP> opts; std::set<int> sb; for (size_t k = al.size(); k-- > 0;) { const auto& r = al[k]; const auto b = ArgInt(*r, 1); if (r->raw != 0x00E1 || r->cond != 0 || !b) continue; if (sb.insert((int)*b).second) opts.push_back(r); }
                std::stable_sort(opts.begin(), opts.end(), [](const RecP& a, const RecP& b) { return (*ArgInt(*a, 1) != 16) < (*ArgInt(*b, 1) != 16); });
                if (!opts.empty()) { const auto& pick = opts[(size_t)visit % opts.size()]; if (auto a2 = ResolveArgs(*d4.row, *pick)) { d4.args = *a2; d4.cond = pick; } }
            }
            if (d4.act && d4.row->action != "wait") { g_auxTag = ":aux" + std::to_string(ak); g_auxAnswered = true; g_auxTouched = true; }
            if (d4.act && d4.row->action != "wait") { d = d4; break; }
        }
    }
    if (earlyAux && !g_auxAnswered) return;
    if (d.act && d.row->action == "wait" && !std::getenv("NOTSA_MDRIVER_NOPICKUP")) { // CR26 (22 BURNING DESIRE): the same wait answer was given twice in a row at this wake ip (no progress in between) and a plain HAS_PICKUP_BEEN_COLLECTED is false in the last passes: the player walks onto the pickup (a wait row after progress, as in 18, leaves it alone: its pickup is not the stage's)
        const auto wk = std::to_string(at) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); const auto wi = g_noProgress.find(wk);
        if (wi != g_noProgress.end() && wi->second >= 2) {
            static const Row kPck{ 0x0214, 1, "HAS_PICKUP_BEEN_COLLECTED", "pickupgo", { "$0" }, 60 };
            std::vector<RecP> big; for (auto& r : g_recsBig) if (r->waitSeq >= g_waitSeq - 3) big.push_back(r);
            for (size_t k = big.size(); k-- > 0;) { const auto& r = big[k]; if ((r->raw & 0x7FFF) != 0x0214 || (r->raw & 0x8000) || r->cond != 0 || AnyGuard(*r)) continue;
                auto a2 = ResolveArgs(kPck, *r); if (!a2 || a2->empty()) continue; d.row = &kPck; d.rowIdx = -11; d.cond = r; d.args = *a2; d.op = 0x0214; break; }
            if (d.row->action == "wait" && !std::getenv("NOTSA_MDRIVER_NOWEAPONPICKUP")) { // CR28 (22 BURNING DESIRE: `HAS_CHAR_GOT_WEAPON cj 42` (fire extinguisher) while the stage's weapon pickup lies in the world): the player walks onto the nearest pickup of that weapon's model
                static const Row kWp{ 0x0491, 1, "HAS_CHAR_GOT_WEAPON", "weaponpickup", { "$1" }, 60 };
                for (size_t k = big.size(); k-- > 0;) { const auto& r = big[k]; if ((r->raw & 0x7FFF) != 0x0491 || (r->raw & 0x8000) || r->cond != 0 || AnyGuard(*r)) continue;
                    auto a2 = ResolveArgs(kWp, *r); if (!a2 || a2->empty()) continue; d.row = &kWp; d.rowIdx = -13; d.cond = r; d.args = *a2; d.op = 0x0491; break; }
            }
        }
    }
    if (d.act && d.row->action == "wait" && g_attemptsAt[ipPtr] >= 2 && !std::getenv("NOTSA_MDRIVER_NOBLIPGO")) { // CR26 (19 CESAR VIALPANDO: stage 19 waits for a flag the mod-shop script sets once the garage opens; the only open task is the destination blip): the wait row answered this wake ip twice and the thread still stands: CJ (with his car) goes to the mission's coordinate blip (the newest BLIP_COORD of the radar)
        const tRadarTrace* best = nullptr;
        for (auto& t : CRadar::ms_RadarTrace) if (t.m_nBlipType == BLIP_COORD && !t.HasSprite() && (!best || t.m_nCounter >= best->m_nCounter)) best = &t;
        if (g_haveLastBlip && !std::getenv("NOTSA_MDRIVER_NOLASTBLIP")) for (auto& t : CRadar::ms_RadarTrace) if (t.m_nBlipType == BLIP_COORD && !t.HasSprite() && (t.m_vPosition - g_lastBlipPos).Magnitude2D() < 2.f && std::fabs(t.m_vPosition.z - g_lastBlipPos.z) < 50.f) { best = &t; break; } // CR48: the blip the script created last
        auto* me = Cj();
        if (best && me && (me->GetPosition() - best->m_vPosition).Magnitude2D() > 6.f) {
            static Row kBlip{ 0, 0, "destination blip (no condition of the pass is answerable)", "blipgo", {}, 120 };
            d.row = &kBlip; d.rowIdx = -7; d.cond = nullptr; d.op = 0; d.args.clear();
            for (const float f : { best->m_vPosition.x, best->m_vPosition.y, std::fabs(best->m_vPosition.z) < 0.5f ? -100.f : best->m_vPosition.z }) { RArg a; a.n = f; d.args.push_back(a); }
        }
    }
    if (g_auxDone && !CMenuSystem::MenuInUse[0] && !CMenuSystem::MenuInUse[1]) g_auxDone = false;
    if (g_auxDone && d.act && !g_auxAnswered && (CMenuSystem::MenuInUse[0] || CMenuSystem::MenuInUse[1]) && !std::getenv("NOTSA_MDRIVER_NOMENULEAVE")) { // CR27 (12 RYDER: the haircut is done, the shop script still holds CJ in its menu): after an aux answer, a menu still up and the root's own answer repeated (a warp / enter-area twice at this wake ip, or the wait row twice): the player leaves the menu with the cancel button (TRIANGLE = 15)
        const auto wk = std::to_string(at) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); const auto wi = g_noProgress.find(wk);
        if (d.row->action == "wait" ? g_attemptsAt[ipPtr] >= 2 : (wi != g_noProgress.end() && wi->second >= 2)) for (auto& kr : g_rows) if (kr.op == 0x00E1 && kr.match == 1 && kr.action == "key") {
            d.row = &kr; d.rowIdx = -14; d.cond = nullptr; d.op = 0x00E1; d.args.clear(); for (const double v : { 15.0, 10.0 }) { RArg a; a.n = v; d.args.push_back(a); } break; }
    }
    if (d.act && (d.row->action == "pedwarp" || d.row->action == "vehwarp") && !std::getenv("NOTSA_MDRIVER_NOBLIPENEX")) { // CR27 (12 RYDER, stage `NOT CJ at the shop counter` while the counter's coordinates are not written yet): the same warp answered twice at this wake ip, its target is neither the blip nor the blip's entry marker (30 m), and a coordinate blip leads through an entry exit (SET_BLIP_ENTRY_EXIT: its coordinates are inside an interior): CJ walks out of the interior he is in / in through that entrance instead
        const auto wk = std::to_string(at) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args); const auto wi = g_noProgress.find(wk);
        if (wi != g_noProgress.end() && wi->second >= 2) {
            const tRadarTrace* bt = nullptr; for (auto& t : CRadar::ms_RadarTrace) if (t.m_nBlipType == BLIP_COORD && !t.HasSprite() && t.m_pEntryExit && (!bt || t.m_nCounter >= bt->m_nCounter)) bt = &t;
            const auto far2d = [&](const CVector& a, float x, float y) { return std::hypot(a.x - x, a.y - y) > 30.f; };
            if (bt && d.args.size() >= 2 && !d.args[0].str && !d.args[1].str && far2d(bt->m_vPosition, (float)d.args[0].n, (float)d.args[1].n) && far2d(bt->m_pEntryExit->GetPosition(), (float)d.args[0].n, (float)d.args[1].n)) { static Row kEnter2{ 0, 0, "interior blip: through its entry exit", "enterarea", {}, 200 }; if (CGame::currArea != 0) { g_enterArea = 0; g_enterEx = nullptr; } else g_enterEx = bt->m_pEntryExit; d.row = &kEnter2; d.rowIdx = -13; d.cond = nullptr; d.op = 0; d.args.clear(); }
        }
    }
    g_attemptsAt[ipPtr]++;
    const std::string key = std::to_string(at) + ":" + std::to_string(d.rowIdx) + ":" + RKey(d.args) + g_auxTag; g_auxTag.clear();
    const int n = ++g_noProgress[key];
    g_stopIp = at; g_stopOp = d.op; // the last answered condition (reported when the mission ends without a driver stop)
    if (d.row->action == "failclass") { DispatchRow(d); return; }
    if (n > 8) { char b[16]; std::snprintf(b, sizeof b, "%04X", d.op); Stop("the driver acted " + std::to_string(n - 1) + " times (" + d.row->action + ") and the condition stays false: " + b + " " + d.row->name, d.op, at); return; }
    std::fprintf(stderr, "[mdriver] frame %d ip %x row %d %04X %s -> %s\n", fr, at, d.rowIdx, d.op, d.row->name.c_str(), d.row->action.c_str());
    if (nearRec && d.act && ((d.row->action == "kill" && !d.args.empty() && (int)d.args[0].n == nearH) || (!g_nearCnt.empty() && d.row->action != "wait"))) { g_nearCnt[nearH]++; d = dn; } // only over a kill answer that did not help (the near-ped locate is in the pass, the ped is alive)
    if (std::getenv("NOTSA_MDRIVER_WLOG")) { auto* w = FindPlayerWanted(); notsa::standalone::Fixups::Log("[mdwlog] frame %d wanted %d chaos %d maxWanted %d maxChaos %d policeBackOff %d everybodyBackOff %d", fr, w ? (int)w->GetWantedLevel() : -1, w ? (int)w->m_ChaosLevel : -1, (int)CWanted::MaximumWantedLevel, (int)CWanted::MaximumChaosLevel, w ? (int)w->m_bPoliceBackOff : -1, w ? (int)w->m_bEverybodyBackOff : -1); } // CR47 debug (off by default)
    notsa::standalone::Fixups::Log("[mdriver] decide frame %d ip %x row %d %04X %s -> %s args %s cond-args %zu a0=%d cj=%d", fr, at, d.rowIdx, d.op, d.row->name.c_str(), d.row->action.c_str(), RKey(d.args).c_str(), d.cond ? d.cond->args.size() : (size_t)0, (d.cond && !d.cond->args.empty()) ? (int)d.cond->args[0].bits : -1, Cj() ? CPools::GetPedRef(Cj()) : -1); // CR16
    if (d.row->action == "kill" && !d.args.empty()) if (auto* kp = CPools::GetPed((int)d.args[0].n)) notsa::standalone::Fixups::Log("[mdriver] kill target h=%d model %d type %d hp %.0f pos %.0f,%.0f,%.0f (CJ %.0f,%.0f)", (int)d.args[0].n, (int)kp->m_nModelIndex, (int)kp->m_nPedType, kp->m_fHealth, kp->GetPosition().x, kp->GetPosition().y, kp->GetPosition().z, Cj() ? Cj()->GetPosition().x : 0.f, Cj() ? Cj()->GetPosition().y : 0.f); // CR27 debug log
    if ((d.row->action == "entercar" || d.row->action == "vehwarp") && !d.args.empty() && d.cond && !d.cond->args.empty()) { const int ch = (int)(int32_t)d.cond->args[0].bits; if (CPools::GetVehicle(ch)) g_usedCars.insert(ch); } // CR27: the car of an entercar / `LOCATE_CAR` vehwarp answer
    DispatchRow(d);
    if (d.row->action == "midpoint" && g_nextStall == 0 && d.cond && !std::getenv("NOTSA_MDRIVER_NOAREAFAST")) { // CR46 (76 HIGHJACK: four angled areas (CJ in a car) are the checkpoints of a truck's route and the truck finishes its route 4000 frames after the stage start; each answer was followed by 660 idle frames): when the pass holds two or more distinct plain false angled-area conditions, the next one is decided after 90 frames
        std::set<const uint8_t*> ar; for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; if (r->waitSeq < g_waitSeq - 3 || r->cond != 0 || (r->raw & 0x8000) || rop < 0x05FC || rop > 0x0601) continue; ar.insert(r->p); }
        if (ar.size() >= 2) g_nextStall = std::getenv("NOTSA_MDRIVER_AREAFAST") ? std::atoi(std::getenv("NOTSA_MDRIVER_AREAFAST")) : 90; }
    if (d.row->action == "warpto_handle" && g_nextStall == 0 && !d.args.empty() && !std::getenv("NOTSA_MDRIVER_NOCARLEASHFAST")) { // CR43 (62 TORENO'S LAST FLIGHT: the answer warped CJ to the helicopter the pass holds a `NOT locate CJ car 100 100 50` leash on; the helicopter lands, takes off and is out of the box 300 frames later, the next decision is 600 frames away): the next decision after 60 frames (CJ is at the leash car)
        const int th = (int)d.args[0].n; const bool movingCar = !std::getenv("NOTSA_MDRIVER_NOMOVINGFAST") && CPools::GetVehicle(th) && CPools::GetVehicle(th)->m_vecMoveSpeed.Magnitude() > 0.03f; /* CR46 (76 HIGHJACK: CJ was warped to the moving truck every 660 frames; the truck is 800 m away 90 frames later): a plain locate of a MOVING car is re-decided after 60 frames too */ for (auto& r : g_recsBig) { const int rop = r->raw & 0x7FFF; if (r->waitSeq < g_waitSeq - 3 || (!(r->raw & 0x8000) && !movingCar) || rop < 0x0202 || rop > 0x0207) continue; const auto h1 = ArgInt(*r, 1); if (h1 && (int)*h1 == th) { g_nextStall = 60; break; } }
    }
    if (d.row->action == "key" && d.op == 0x00E1 && !g_auxAnswered && !earlyAux && !std::getenv("NOTSA_MDRIVER_NOFASTKEY") && !(d.cond && DropsObjectBehind(*d.cond)) && g_attemptsAt[ipPtr] >= 3 && g_nextStall == 0) { g_fastDec = d; g_fastIp = ipPtr; g_fastLeft = 20; g_fastNext = true; g_nextStall = 45; } // CR30 (25): a button poll answered after the key deferral: repeat the press every 30 frames while the thread stays at the wake ip (20 repeats at most)
    if (g_auxAnswered) { g_auxAnswered = false; if (g_nextStall == 0 && !std::getenv("NOTSA_MDRIVER_NOAUXFAST")) { g_nextStall = 90; g_auxEarly = true; } } // CR27: a menu has several steps: the next decision after 90 frames, not 600
}
} // namespace

bool Enabled() { static const bool e = std::getenv("NOTSA_MDRIVER") != nullptr || Generic(); return e; }
void OnCommand(const CRunningScript* s, const uint8_t* ptr, uint16_t raw, int cond) {
    if (static const int cl0 = std::getenv("NOTSA_MDRIVER_CMDLOG") ? std::atoi(std::getenv("NOTSA_MDRIVER_CMDLOG")) : -1; cl0 >= 0 && GFrame() >= cl0 && GFrame() <= cl0 + 4 && g_loaded) notsa::standalone::Fixups::Log("[cmdlog] f%d thread %.8s ip %x raw %04X cond %d", GFrame(), s->m_szName, (unsigned)(ptr - s->m_BaseIP), (unsigned)raw, cond); // CR43: debug (log only): every command of the frames cl0..cl0+4 (a crash: the last line is the command)
    if (!Generic() || !g_loaded) return;
    if (s != notsa::scmtrace::Root() && g_started) AuxTree(const_cast<CRunningScript*>(s), ptr, raw, cond);
    if (s == notsa::scmtrace::Root()) { // the StallSink: conditions with their state + operands, WAIT count, where the GOTO_IF_FALSE after them led
        const int op = raw & 0x7FFF;
        if (op == 0x018A && InSpace(ptr) && !std::getenv("NOTSA_MDRIVER_NOLASTBLIP")) { const auto a = DecodeArgs(s, ptr, 3); if (a.size() == 3) { g_lastBlipPos = CVector{ AF(a[0]), AF(a[1]), AF(a[2]) }; g_haveLastBlip = true; } } // CR48 (79 N.O.E.: the blip of the landing box was not the one with the highest slot counter)
        if ((op == 0x010D || op == 0x010E) && !std::getenv("NOTSA_MDRIVER_NOSCRIPTWANTED")) g_scriptWantedFrame = GFrame(); // CR47 (66 MIKE TORENO: the stage's own ALTER_WANTED_LEVEL_NO_DROP 3 was cleared the next frame by the CR25 window of the driver's earlier kill, so the chase machine (`wanted > 0` then `wanted == 0`) never saw a star): a wanted level the SCRIPT raises stays for 120 frames (NOTSA_MDRIVER_NOSCRIPTWANTED=1: off)
        if (g_fallPending) { if (op == 0x0001) for (auto& r : g_group) r->waitAfter = true; g_fallPending = false; g_group.clear(); } // CR19: a group that fell through straight into a WAIT
        if (op == 0x0001) g_waitSeq++;
        else if (op == 0x004D) {
            const bool jumped = s->m_IP != ptr + 7;
            for (auto& r : g_pending) { r->hasBranch = true; r->jumped = jumped; r->fall = ptr + 7; r->target = jumped ? s->m_IP : ptr + 7; }
            if (!jumped) { g_group = g_pending; g_fallPending = true; }
            g_pending.clear();
        } else if ((op == 0x014E || op == 0x03C3) && !std::getenv("NOTSA_MDRIVER_NOTIMERVAR")) { // CR39 (47 ARE YOU GOING TO SAN FIERRO?): DISPLAY_ONSCREEN_TIMER[_WITH_STRING] var: remember the variable (the `var == 0` time-up check of a countdown is not a blocker while another condition of the pass is false)
            if (InSpace(ptr)) { uint32_t ref = 0; if (OperandVar(ptr + 2, ref)) g_timerVars.insert(ref); }
        } else if (op == 0x077E || (op == 0x09E8 && !std::getenv("NOTSA_MDRIVER_NOCHARAREA"))) { // CR23: GET_AREA_VISIBLE var: remember the destination variable (a later `var == N` wait = be in interior N). CR27 (22 BURNING DESIRE): GET_CHAR_AREA_VISIBLE char var too (the helper sub of the mission reads the player's area with it)
            if (InSpace(ptr)) { uint32_t ref = 0; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; if (op == 0x09E8) q = SkipOperand(q, mf, isI, iv); if (q && OperandVar(q, ref)) g_areaVars.insert(ref); }
        } else if (op == 0x0702) { // CR22: GET_PERCENTAGE_TAGGED_IN_AREA x1 y1 x2 y2 -> var: remember the area and the destination variable (a later `var == 100` wait = spray these tags)
            if (InSpace(ptr)) {
                const uint8_t* ops5[5]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0;
                for (; k < 5 && q && InSpace(q); k++) { ops5[k] = q; q = SkipOperand(q, mf, isI, iv); }
                uint32_t ref = 0; const auto a = DecodeArgs(s, ptr, 4);
                if (k == 5 && a.size() == 4 && OperandVar(ops5[4], ref)) { g_tagAreas[ref] = { AF(a[0]), AF(a[1]), AF(a[2]), AF(a[3]) }; }
            }
        } else if (op == 0x0226) { // CR35: GET_CHAR_HEALTH char var
            if (InSpace(ptr)) { const uint8_t* o[2]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0; for (; k < 2 && q && InSpace(q); k++) { o[k] = q; q = SkipOperand(q, mf, isI, iv); } uint32_t ref = 0; const auto a = DecodeArgs(s, ptr, 1); if (k == 2 && a.size() == 1 && OperandVar(o[1], ref)) { g_healthVars[ref] = (int)(int32_t)a[0].bits; if (std::getenv("NOTSA_MDRIVER_HPLOG")) notsa::standalone::Fixups::Log("[hplog] GET_CHAR_HEALTH ped %d -> var %x", (int)(int32_t)a[0].bits, ref); } }
        } else if (op == 0x0227) { // CR44: GET_CAR_HEALTH car var
            if (InSpace(ptr)) { const uint8_t* o[2]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0; for (; k < 2 && q && InSpace(q); k++) { o[k] = q; q = SkipOperand(q, mf, isI, iv); } uint32_t ref = 0; const auto a = DecodeArgs(s, ptr, 1); if (k == 2 && a.size() == 1 && OperandVar(o[1], ref)) g_carHealthVars[ref] = (int)(int32_t)a[0].bits; }
        } else if (op == 0x071E) { // CR37: GET_OBJECT_HEALTH object var
            if (InSpace(ptr)) { const uint8_t* o[2]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0; for (; k < 2 && q && InSpace(q); k++) { o[k] = q; q = SkipOperand(q, mf, isI, iv); } uint32_t ref = 0; const auto a = DecodeArgs(s, ptr, 1); if (k == 2 && a.size() == 1 && OperandVar(o[1], ref)) g_objHealthVars[ref] = (int)(int32_t)a[0].bits; }
        } else if (op == 0x02E3) { // CR34: GET_CAR_SPEED car var: remember which car the variable was read from
            if (InSpace(ptr)) { const uint8_t* o[2]{}; const uint8_t* q = ptr + 2; bool mf = false, isI = false; int32_t iv = 0; int k = 0; for (; k < 2 && q && InSpace(q); k++) { o[k] = q; q = SkipOperand(q, mf, isI, iv); } uint32_t ref = 0; const auto a = DecodeArgs(s, ptr, 1); if (k == 2 && a.size() == 1 && OperandVar(o[1], ref)) g_speedVars[ref] = (int)(int32_t)a[0].bits; }
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
            g_recs.push_back(r); if (g_recs.size() > 64) g_recs.pop_front(); g_recsBig.push_back(r); if (g_recsBig.size() > g_bigCap) g_recsBig.pop_front();
            if (raw == 0x001B) g_healthConds[ptr] = r; if (raw == 0x001B && cond == 0 && r->vref[1] && !std::getenv("NOTSA_MDRIVER_NOHURT") && !std::getenv("NOTSA_MDRIVER_NOHURTNOW")) { const auto it = g_healthVars.find(r->vref[1]); const auto thr = ArgInt(*r, 0); auto* hp0 = it != g_healthVars.end() ? CPools::GetPed(it->second) : nullptr; if (it != g_healthVars.end() && thr && *thr >= 2 && hp0 && HostileScriptPed(hp0)) g_hurtNow = { it->second, (int)*thr - 1 }; } // CR37 // CR35: every `N > var` site seen (the pass is longer than the 400-record window)
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
bool AuxBegin(CRunningScript* s) {
    static const bool off = std::getenv("NOTSA_MDRIVER_NOAUX") != nullptr;
    if (off || !Generic() || !g_loaded || !g_started || !s || s == notsa::scmtrace::Root()) return false;
    const uint8_t* b = s->m_BaseIP, *sp = CTheScripts::ScriptSpace.data(); if (!b || (b >= sp && b < sp + CTheScripts::ScriptSpace.size())) return false; // streamed scripts only
    g_auxIp = s->m_IP; g_auxRaw = *(const uint16_t*)g_auxIp; return true;
}
static void AuxRecord(CRunningScript* s, const uint8_t* ptr, uint16_t raw, int cond) {
    auto& st = g_aux[s]; const int op = raw & 0x7FFF;
    if (st.fallPending) { if (op == 0x0001) for (auto& r : st.group) r->waitAfter = true; st.fallPending = false; st.group.clear(); }
    if (op == 0x0001) st.waitSeq++;
    else if (op == 0x004D) {
        const bool jumped = s->m_IP != ptr + 7;
        for (auto& r : st.pending) { r->hasBranch = true; r->jumped = jumped; r->fall = ptr + 7; r->target = jumped ? s->m_IP : ptr + 7; }
        if (!jumped) { st.group = st.pending; st.fallPending = true; }
        st.pending.clear();
    } else if (cond >= 0) {
        auto r = std::make_shared<Rec>(); r->p = ptr; r->raw = raw; r->cond = cond; r->waitSeq = st.waitSeq;
        const int n = Arity(op); r->args = DecodeArgs(s, ptr, n >= 0 ? n : 10);
        st.recs.push_back(r); if (st.recs.size() > 200) st.recs.pop_front();
        st.pending.push_back(r); if (st.pending.size() > 12) st.pending.erase(st.pending.begin());
    }
}
void AuxEnd(CRunningScript* s, int cond) { AuxRecord(s, g_auxIp, g_auxRaw, cond); }
// CR29 (19 CESAR VIALPANDO): a thread of the traced tree other than the root (the mission starts main.scm's LOWGAME thread, which writes the global the stage waits on and polls its own buttons) is an aux thread too: its conditions are recorded like a streamed script's (called from OnCommand), the globals it writes are learnt from the scmtrace G rows
void AuxTree(CRunningScript* s, const uint8_t* ptr, uint16_t raw, int cond) {
    static const bool off = std::getenv("NOTSA_MDRIVER_NOAUX") != nullptr || std::getenv("NOTSA_MDRIVER_NOAUXTREE") != nullptr;
    if (off || !s || s == notsa::scmtrace::Root()) return;
    g_aux[s].dyn = true; AuxRecord(s, ptr, raw, cond);
}
void AuxGlobalWrite(const CRunningScript* s, uint32_t off) { if (s && s != notsa::scmtrace::Root() && off < 0x10000) { auto it = g_aux.find(s); if (it != g_aux.end() && it->second.dyn) it->second.dynWrites.insert((uint16_t)off); } }
void AuxForget(const CRunningScript* s) { g_aux.erase(s); }
bool PadHeld(int button) { auto it = g_padHold.find(button); return it != g_padHold.end() && it->second > 0; }
void SetKeyPusher(void (*fn)(const char*, bool)) { g_push = fn; }
void SetCmdRunner(void (*fn)(const std::string&)) { g_run = fn; }

bool Cmd(const char* name, const std::string& args) {
    if (std::strncmp(name, "md", 2)) return false;
    auto* ped = Cj();
    if (!std::strcmp(name, "mdkillped")) { if (auto* k = CPools::GetPed(std::atoi(args.c_str()))) { if (std::getenv("NOTSA_MDRIVER_PLOG")) notsa::standalone::Fixups::Log("[mdplog] kill h=%s type=%d model=%d hp=%.0f pos=%.0f,%.0f wanted=%d\n", args.c_str(), (int)k->m_nPedType, (int)k->m_nModelIndex, k->m_fHealth, k->GetPosition().x, k->GetPosition().y, (int)FindPlayerWanted()->m_WantedLevel); if (ped) CWeapon::GenerateDamageEvent(k, ped, WEAPON_PISTOL, 1000, PED_PIECE_TORSO, 0); /* CR13: no forced health 0 / PEDSTATE_DEAD fallback: TS shootDead has none, the ped dies through its death task over the next frames */ } return true; }
    if (!std::strcmp(name, "mdhurt")) { int h = 0, hp = 0; std::sscanf(args.c_str(), "%d:%d", &h, &hp); if (auto* k = CPools::GetPed(h)) { if (k->m_fHealth > (float)hp) k->m_fHealth = (float)hp; notsa::standalone::Fixups::Log("[mdriver] mdhurt ped %d health -> %d type %d model %d", h, hp, (int)k->m_nPedType, (int)k->m_nModelIndex); } return true; } // CR35
    if (!std::strcmp(name, "mdkillcar")) { if (auto* k = CPools::GetVehicle(std::atoi(args.c_str()))) { if (std::getenv("NOTSA_MDRIVER_KILLLOG")) notsa::standalone::Fixups::Log("[mdriver] mdkillcar %s model %d hp %.0f status %d canBeDamaged %d fixed %d type %d", args.c_str(), (int)k->m_nModelIndex, k->m_fHealth, (int)k->GetStatus(), (int)k->vehicleFlags.bCanBeDamaged, (int)k->physicalFlags.bDisableCollisionForce, (int)k->GetType()); k->BlowUpCar(nullptr, false); if (std::getenv("NOTSA_MDRIVER_KILLLOG")) notsa::standalone::Fixups::Log("[mdriver]   after: hp %.0f status %d", k->m_fHealth, (int)k->GetStatus()); } return true; }
    if (!std::strcmp(name, "mdpad")) { const auto c = args.find(':'); if (c != std::string::npos) g_padHold[std::atoi(args.c_str())] = std::atoi(args.c_str() + c + 1) + 1; return true; }
    if (!std::strcmp(name, "mdmenu")) { // CR27: the player's CROSS press on a menu (CMenuSystem::InputStandardMenu): the selected row (the first selectable one when none) becomes the accepted row of every menu in use
        for (int id = 0; id < CMenuSystem::MENU_COUNT; id++) {
            auto* m = MenuNumber[id]; if (!CMenuSystem::MenuInUse[id] || !m || m->m_nNumRows == 0) continue;
            int r = m->m_nSelectedRow; if (r < 0 || r >= m->m_nNumRows || !m->m_abRowSelectable[r]) { r = -1; for (int k = 0; k < m->m_nNumRows; k++) if (m->m_abRowSelectable[k] && m->m_aaacRowTitles[0][k][0]) { r = k; break; } if (r < 0) continue; m->m_nSelectedRow = (int8)r; }
            m->m_nAcceptedRow = (int8)r;
        }
        return true;
    }
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
        float w[7]{}; const int nw = std::sscanf(rest.c_str(), "%f:%f:%f:%f:%f:%f:%f", w, w + 1, w + 2, w + 3, w + 4, w + 5, w + 6); // CR35: optional 4..7: the speed (x:y:z, units/frame) and heading the car keeps after the warp (a car that joins a moving train goes on alongside it)
        CStreaming::LoadScene(CVector{ w[0], w[1], 0.f }); CStreaming::LoadSceneCollision(CVector{ w[0], w[1], 0.f });
        const float gz = w[2] <= -100.f ? CWorld::FindGroundZForCoord(w[0], w[1]) : w[2];
        CVehicle* veh = nullptr; if (isPed) { auto* pd = static_cast<CPed*>(tgt); if (pd->bInVehicle) veh = pd->m_pVehicle; } else veh = static_cast<CVehicle*>(tgt);
        if (veh) { if (GetVehiclePool()->GetRef(veh) == g_airHold.h) g_airHold.left = 0; /* CR50: a new warp of the held aircraft ends the old hold */ CCarCtrl::SetCoordsOfScriptCar(veh, w[0], w[1], gz, 0, 1); veh->m_vecMoveSpeed = CVector{}; veh->m_vecTurnSpeed = CVector{}; if ((veh->IsSubPlane() || veh->IsSubHeli()) && !std::getenv("NOTSA_MDRIVER_NOAIRHOLD") && ped->bInVehicle && ped->m_pVehicle == veh) { const float gnd = CWorld::FindGroundZForCoord(w[0], w[1]); if (gz - gnd > 30.f || (veh->IsSubPlane() && gz - gnd > 0.5f && !std::getenv("NOTSA_MDRIVER_NOLOWHOLD"))) { g_airHold.h = GetVehiclePool()->GetRef(veh); g_airHold.z = gz; g_airHold.left = gz - gnd > 30.f ? 1500 : 240; g_airHold.vx = nw >= 7 ? w[3] : 0.f; g_airHold.vy = nw >= 7 ? w[4] : 0.f; notsa::standalone::Fixups::Log("[mdriver] airhold: the aircraft is held at z %.0f for 1500 frames", gz); } } /* CR49 */ if (nw >= 7) { veh->SetHeading(w[6]); veh->m_vecMoveSpeed = CVector{ w[3], w[4], w[5] }; } if (std::getenv("NOTSA_MDRIVER_ENTERLOG")) g_watchVeh = GetVehiclePool()->GetRef(veh); }
        else { CWorld::Remove(tgt); tgt->SetPosn(CVector{ w[0], w[1], gz + 1.1f }); CWorld::Add(tgt); }
        CStreaming::LoadScene(CVector{ w[0], w[1], gz }); CStreaming::LoadSceneCollision(CVector{ w[0], w[1], gz });
        notsa::standalone::Fixups::Log("[mdriver] %s gf=%u -> %.2f,%.2f,%.2f", name, (unsigned)CTimer::GetFrameCounter(), w[0], w[1], gz);
        return true;
    }
    if (!std::strcmp(name, "mdwland")) { // CR49: the plane CJ flies is put on the ground of the box, wheels down, at rest, and kept repaired
        float w[3]{}; std::sscanf(args.c_str(), "%f:%f:%f", w, w + 1, w + 2);
        if (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle->IsSubPlane()) { auto* pl = static_cast<CPlane*>(ped->m_pVehicle);
            CStreaming::LoadScene(CVector{ w[0], w[1], 0.f }); CStreaming::LoadSceneCollision(CVector{ w[0], w[1], 0.f });
            const float gz = (w[2] <= -100.f ? CWorld::FindGroundZForCoord(w[0], w[1]) : w[2]) + 1.0f;
            g_airHold.left = 0; CCarCtrl::SetCoordsOfScriptCar(pl, w[0], w[1], gz, 0, 1); pl->m_vecMoveSpeed = CVector{}; pl->m_vecTurnSpeed = CVector{}; pl->SetGearDown(); pl->Fix(); pl->m_fHealth = 1000.f; g_fixCars.insert(GetVehiclePool()->GetRef(pl));
            notsa::standalone::Fixups::Log("[mdriver] mdwland gf=%u -> %.2f,%.2f,%.2f", (unsigned)CTimer::GetFrameCounter(), w[0], w[1], gz); }
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
        if (car && oth && !std::getenv("NOTSA_MDRIVER_NOGROUPCAR") && (std::getenv("NOTSA_MDRIVER_NOFARPASS") || (oth->GetPosition() - car->GetPosition()).Magnitude() < 30.f /* CR49 (90 THE MEAT BUSINESS: the buddy is 1 km away, the group response walks for ever): a far buddy is set in directly */)) if (auto* cj0 = Cj(); cj0 && cj0->bInVehicle && cj0->m_pVehicle == car) for (int gi = 0; gi < 8; gi++) if (CPedGroups::ms_activeGroups[gi] && CPedGroups::ms_groups[gi].GetMembership().IsFollower(oth) && CPedGroups::ms_groups[gi].GetMembership().GetLeader() == cj0) { // CR26: a member of CJ's group, CJ at the wheel: the group's own enter-car response (as SET_GROUP_MEMBER) instead of a warp: the follower then has the CarDrive task (IS_CHAR_SITTING_IN_CAR)
            GroupEnterLeaderCar(oth, gi); g_buddyRef = h; if (std::getenv("NOTSA_MDRIVER_ENTERLOG")) notsa::standalone::Fixups::Log("[enterlog] mdwarppass ped %d: group %d enter-car response", h, gi); return true; }
        if (car && oth) { oth->GetIntelligence()->FlushImmediately(false); eTargetDoor door = TARGET_DOOR_FRONT_RIGHT; if (!std::getenv("NOTSA_MDRIVER_NOSEATPICK")) { /* CR45 (66 MIKE TORENO: two buddies must sit in CJ's car; both were set in at the front right seat, the second one pushed the first out): the first free passenger seat */ static const eTargetDoor doors[3] = { TARGET_DOOR_FRONT_RIGHT, TARGET_DOOR_REAR_LEFT, TARGET_DOOR_REAR_RIGHT }; for (int si = 0; si < 3 && si < (int)car->m_nMaxPassengers; si++) if (!car->m_apPassengers[si] || car->m_apPassengers[si] == oth) { door = doors[si]; break; } }
            CTaskSimpleCarSetPedInAsPassenger t(car, door, true, nullptr); t.ProcessPed(oth); g_buddyRef = h; }
        if (std::getenv("NOTSA_MDRIVER_ENTERLOG")) notsa::standalone::Fixups::Log("[enterlog] mdwarppass ped %d=%p car %d=%p -> inVeh %d same %d model %d", h, (void*)oth, std::atoi(args.c_str() + args.find(':') + 1), (void*)car, oth ? (int)oth->bInVehicle : -1, oth ? (int)(oth->m_pVehicle == car) : -1, oth ? (int)oth->m_nModelIndex : -1);
        return true;
    }
    if (!std::strcmp(name, "mdtosphere")) { // CR24: the intercept warp of the remote vehicle: where the nearest qualifying car will be in 4.3 s (position + velocity * 4.3 s, 3 m up)
        const int h = std::atoi(args.c_str()); const int model = std::atoi(args.c_str() + args.find(':') + 1);
        if (auto* best = SphereTarget(h, model)) { const CVector p = best->GetPosition() + best->m_vecMoveSpeed * 50.f * 4.3f; notsa::standalone::Fixups::Log("[mdriver] mdtosphere: car %d at %.1f,%.1f,%.1f speed %.1f -> %.1f,%.1f,%.1f", (int)best->m_nModelIndex, best->GetPosition().x, best->GetPosition().y, best->GetPosition().z, best->m_vecMoveSpeed.Magnitude() * 50.f, p.x, p.y, p.z); return Cmd("mdwother", "1:" + std::to_string(h) + ":" + F(p.x) + ":" + F(p.y) + ":" + F(p.z + 3.f)); }
        return true;
    }
    if (!std::strcmp(name, "mdprovoke")) { // CR26: the kills that provoke a gang war (CGangWars::AddKillToProvocation is what a killed gang ped calls; only a zone with that gang's strength counts)
        const int n = std::atoi(args.c_str()); for (int i = 0; i < n; i++) { CGangWars::AddKillToProvocation(PED_TYPE_GANG1); CGangWars::AddKillToProvocation(PED_TYPE_GANG3); }
        notsa::standalone::Fixups::Log("[mdriver] mdprovoke %d: provocation %.1f bCanTriggerOnMission %d war %d", n, CGangWars::Provocation, (int)CGangWars::bCanTriggerGangWarWhenOnAMission, (int)CGangWars::bGangWarsActive);
        return true;
    }
    if (!std::strcmp(name, "mdproj")) { // CR26: a molotov (CProjectileInfo::AddProjectile, CJ the creator) at each given point
        std::string a = args; size_t pos = 0;
        while (pos < a.size()) { size_t e = a.find('|', pos); if (e == std::string::npos) e = a.size(); float x = 0, y = 0, z = 0; if (std::sscanf(a.substr(pos, e - pos).c_str(), "%f,%f,%f", &x, &y, &z) == 3) { const bool ok = CProjectileInfo::AddProjectile(ped, WEAPON_MOLOTOV, CVector{ x, y, z }, 0.f, nullptr, nullptr); notsa::standalone::Fixups::Log("[mdriver] mdproj molotov at %.1f,%.1f,%.1f -> %d", x, y, z, (int)ok); } pos = e + 1; }
        return true;
    }
    if (!std::strcmp(name, "mdjoingrp")) { // CR26: SET_GROUP_MEMBER(group, ped) without the car part: be-in-group task event, AddFollower, group.Process()
        const int ph = std::atoi(args.c_str()); const int gh = std::atoi(args.c_str() + args.find(':') + 1);
        const int idx = CTheScripts::GetActualScriptThingIndex(gh, SCRIPT_THING_PED_GROUP);
        if (auto* k = CPools::GetPed(ph); k && idx >= 0 && idx < 8) {
            CEventScriptCommand ev{ TASK_PRIMARY_PRIMARY, new CTaskComplexBeInGroup{ idx, false }, false };
            k->GetIntelligence()->m_eventGroup.Add(&ev, false);
            auto& grp = CPedGroups::ms_groups[idx]; auto& mem = grp.GetMembership();
            if (mem.CountMembersExcludingLeader() >= 7 && mem.GetLeader() && mem.GetLeader()->IsPlayer()) mem.RemoveNFollowers(1);
            mem.AddFollower(k); grp.Process(); GroupEnterLeaderCar(k, idx);
            notsa::standalone::Fixups::Log("[mdriver] mdjoingrp ped %d group %d (idx %d)", ph, gh, idx);
        }
        return true;
    }
    if (!std::strcmp(name, "mdphoto")) {
        if (auto* k = CPools::GetPed(std::atoi(args.c_str()))) {
            k->bHasBeenPhotographed = true; notsa::standalone::Fixups::Log("[mdriver] mdphoto %s", args.c_str());
            // CR26 (58 PHOTO OPPORTUNITY): a script that also tests GET_CAMERA_FOV (`N > fov`: the zoomed shot) needs the shot repeated with the camera zoomed in, in the frames before its timed condition (the walker's task) ends: hold the photographed flag and the zoomed FOV for 600 frames (the script's own 04C5 read clears the flag each pass)
            if (!std::getenv("NOTSA_MDRIVER_NOZOOM") && PhotoWantsZoom()) { g_photoPed = std::atoi(args.c_str()); g_photoHold = 600; } // a GET_CAMERA_FOV (0801 into a local) within 2 KB before a photo test
        }
        return true;
    }
    if (!std::strcmp(name, "mdwobj")) { // CR29: move a script object to x:y:z (z <= -100 = keep its height); the object stops
        int h = 0; float w[3]{}; std::sscanf(args.c_str(), "%d:%f:%f:%f", &h, w, w + 1, w + 2);
        if (auto* o = CPools::GetObject(h)) { const CVector old = o->GetPosition(); CWorld::Remove(o); o->SetPosn(CVector{ w[0], w[1], w[2] <= -100.f ? old.z : w[2] }); o->m_vecMoveSpeed = CVector{}; o->m_vecTurnSpeed = CVector{}; CWorld::Add(o); notsa::standalone::Fixups::Log("[mdriver] mdwobj gf=%u obj %d model %d %.2f,%.2f,%.2f -> %.2f,%.2f,%.2f", (unsigned)CTimer::GetFrameCounter(), h, (int)o->m_nModelIndex, old.x, old.y, old.z, w[0], w[1], w[2]); }
        return true;
    }
    if (!std::strcmp(name, "mdfollow")) { // CR47: mdfollow:<vehicle handle>:<frames>
        int h = -1, n = 0; std::sscanf(args.c_str(), "%d:%d", &h, &n); auto* tv = CPools::GetVehicle(h); auto* me = Cj();
        if (tv && me && me->bInVehicle && me->m_pVehicle && me->m_pVehicle != tv) { const CVector df = me->m_pVehicle->GetPosition() - tv->GetPosition(); g_follow = { h, CVector{ DotProduct(df, tv->GetRight()), DotProduct(df, tv->GetForward()), DotProduct(df, tv->GetUp()) }, n }; notsa::standalone::Fixups::Log("[mdriver] mdfollow: CJ's vehicle follows vehicle %d at %.1f,%.1f,%.1f for %d frames", h, (double)g_follow.off.x, (double)g_follow.off.y, (double)g_follow.off.z, n); }
        return true; }
    if (!std::strcmp(name, "mdspeed")) { // CR34: hold the car at the speed
        int h = -1; float m = 0.f; std::sscanf(args.c_str(), "%d:%f", &h, &m); g_speedHold = { h, m, 700 }; notsa::standalone::Fixups::Log("[mdriver] mdspeed car %d held at %.1f m/s", h, (double)m);
        return true; }
    if (!std::strcmp(name, "mdfixcar")) { // CR34: CVehicle::Fix + full health (what the script command FIX_CAR does)
        if (auto* v = CPools::GetVehicle(std::atoi(args.c_str()))) { g_fixCars.insert(std::atoi(args.c_str())); v->Fix(); v->m_fHealth = 1000.f; notsa::standalone::Fixups::Log("[mdriver] mdfixcar %d model %d", std::atoi(args.c_str()), (int)v->m_nModelIndex); }
        return true; }
    if (!std::strcmp(name, "mdattach")) { // CR37: the cab goes to the trailer's hitch (same heading, tow bar on the hitch); CTrailer::ProcessAI -> ScanForTowLink (exe 0x6CF030) then hitches it by itself
        int th = 0, ch = 0; std::sscanf(args.c_str(), "%d:%d", &th, &ch);
        auto* tr = CPools::GetVehicle(th); auto* cab = CPools::GetVehicle(ch);
        if (tr && cab && tr != cab && !(tr->m_pTowingVehicle == cab)) {
            CWorld::Remove(cab);
            auto& m = cab->GetMatrix(); const auto& tm = tr->GetMatrix();
            m.GetRight() = tm.GetRight(); m.GetForward() = tm.GetForward(); m.GetUp() = tm.GetUp();
            CVector hitch{}, bar{};
            const bool ok = tr->GetTowHitchPos(hitch, false, tr) && cab->GetTowBarPos(bar, false, tr);
            if (ok) cab->SetPosn(cab->GetPosition() + (hitch - bar));
            cab->m_vecMoveSpeed = CVector{}; cab->m_vecTurnSpeed = CVector{}; tr->m_vecMoveSpeed = CVector{}; tr->m_vecTurnSpeed = CVector{};
            CWorld::Add(cab);
            notsa::standalone::Fixups::Log("[mdriver] mdattach gf=%u trailer %d cab %d ok=%d hitch %.2f,%.2f,%.2f bar %.2f,%.2f,%.2f trailer status %d", (unsigned)CTimer::GetFrameCounter(), th, ch, (int)ok, hitch.x, hitch.y, hitch.z, bar.x, bar.y, bar.z, (int)tr->GetStatus());
        }
        return true;
    }
    if (!std::strcmp(name, "mdexplode")) { // CR36: CExplosion::AddExplosion as the script command ADD_EXPLOSION does (no creator, sound, no camera shake)
        int t = -1; float x = 0, y = 0, z = 0; std::sscanf(args.c_str(), "%d:%f:%f:%f", &t, &x, &y, &z);
        if (!std::getenv("NOTSA_MDRIVER_NOEXPLOSIONPROOF")) for (auto& pd : GetPedPool()->GetAllValid()) { // the player throws the grenade from a distance in the game: the peds near the box (CJ, the buddy) are explosion proof, the explosion only has to be SEEN by the script's area test
            if (pd.m_fHealth > 0.f && (pd.GetPosition() - CVector{ x, y, z }).Magnitude() < 25.f) pd.physicalFlags.bExplosionProof = true; }
        CExplosion::AddExplosion(nullptr, nullptr, t < 0 ? EXPLOSION_GRENADE : (eExplosionType)t, CVector{ x, y, z }, 0, true, -1.0f, false);
        notsa::standalone::Fixups::Log("[mdriver] mdexplode gf=%u type %d at %.2f,%.2f,%.2f", (unsigned)CTimer::GetFrameCounter(), t, x, y, z);
        return true;
    }
    if (!std::strcmp(name, "mdsmash")) { // CR38: the object IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED (0x46D56F) finds is damaged as by a bullet (CJ the damager); `smashed` also marks it broken
        float x = 0, y = 0, z = 0, r = 0; int m = 0, sm = 0, dm = 0; std::sscanf(args.c_str(), "%f:%f:%f:%f:%d:%d:%d", &x, &y, &z, &r, &m, &sm, &dm);
        const int model = m < 0 ? (int)CTheScripts::UsedObjectArray[-m].nModelIndex : m; const CVector pos{ x, y, z <= -100.f ? CWorld::FindGroundZForCoord(x, y) : z };
        CEntity* e = CWorld::FindNearestObjectOfType(model, pos, r, false, false, false, false, true, true);
        if (e && e->GetType() == ENTITY_TYPE_OBJECT) { auto* o = static_cast<CObject*>(e); const CVector op = o->GetPosition(); CVector dir = op - ped->GetPosition(); dir.Normalise(); o->ObjectDamage(1000.f, &op, &dir, ped, WEAPON_PISTOL);
            if (sm && !o->objectFlags.bIsBroken) o->objectFlags.bIsBroken = true;
            notsa::standalone::Fixups::Log("[mdriver] mdsmash model %d at %.1f,%.1f,%.1f: renderDamaged=%d visible=%d broken=%d", model, op.x, op.y, op.z, (int)o->m_bRenderDamaged, (int)o->m_bIsVisible, (int)o->objectFlags.bIsBroken); }
        else notsa::standalone::Fixups::Log("[mdriver] mdsmash model %d at %.1f,%.1f,%.1f: no object found", model, x, y, z);
        return true;
    }
    if (!std::strcmp(name, "mdswim")) { // CR39: CJ is put into the water near x,y: the nearest ring sample (8..60 m) where the water is deeper than 2 m
        float x = 0, y = 0; if (std::sscanf(args.c_str(), "%f:%f", &x, &y) == 2) { bool done = false;
            for (float r = 8.f; r <= 60.f && !done; r += 4.f) for (int k = 0; k < 16 && !done; k++) { const float px = x + r * std::cos(k * 0.3927f), py = y + r * std::sin(k * 0.3927f); CStreaming::LoadScene(CVector{ px, py, 0.f }); CStreaming::LoadSceneCollision(CVector{ px, py, 0.f }); float wl = 0.f; if (!CWaterLevel::GetWaterLevel(px, py, 5.f, wl, false, nullptr)) continue; const float gz = CWorld::FindGroundZForCoord(px, py); if (gz > wl - 2.f) continue;
                CWorld::Remove(ped); ped->SetPosn(CVector{ px, py, wl + 0.6f }); CWorld::Add(ped); notsa::standalone::Fixups::Log("[mdriver] mdswim -> %.1f,%.1f water %.1f ground %.1f", px, py, wl, gz); done = true; } }
        return true;
    }
    if (!std::strcmp(name, "mdobjto")) { // CR39: CObject placed at x,y,z (the pushed toilet of 50 DECONSTRUCTION), at rest
        int h = 0; float x = 0, y = 0, z = 0; if (std::sscanf(args.c_str(), "%d:%f:%f:%f", &h, &x, &y, &z) == 4) if (auto* o = CPools::GetObject(h)) { o->SetPosn(CVector{ x, y, z }); o->ResetMoveSpeed(); o->ResetTurnSpeed(); o->UpdateRwFrame(); notsa::standalone::Fixups::Log("[mdriver] mdobjto %d model %d -> %.2f,%.2f,%.2f", h, (int)o->m_nModelIndex, x, y, z); }
        return true;
    }
    if (!std::strcmp(name, "mddmgobj")) { // CR22: CObject::ObjectDamage, CJ the damager, pistol (the port's object damage path of a bullet hit)
        const char* wc = std::strchr(args.c_str(), ':'); const eWeaponType dw = wc ? (eWeaponType)std::atoi(wc + 1) : WEAPON_PISTOL; // CR49: optional ":weapon"
        if (auto* o = CPools::GetObject(std::atoi(args.c_str()))) { const CVector pos = o->GetPosition(); CVector dir = pos - ped->GetPosition(); dir.Normalise(); o->ObjectDamage(1000.f, &pos, &dir, ped, dw); if (!o->m_bRenderDamaged && o->m_bIsVisible) { o->ObjectDamage(1.0e5f, &pos, &dir, ped, dw); } /* CR39: 50 DECONSTRUCTION portakabin (colDamMult 0.01, breakable): a bulldozer hit is > 150 after the multiplier, a pistol 1000 is not */ notsa::standalone::Fixups::Log("[mdriver] mddmgobj %s model %d renderDamaged=%d visible=%d", args.c_str(), (int)o->m_nModelIndex, (int)o->m_bRenderDamaged, (int)o->m_bIsVisible); }
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
    if (g_photoHold > 0) { g_photoHold--; if (auto* k = CPools::GetPed(g_photoPed)) { k->bHasBeenPhotographed = true; TheCamera.GetActiveCam().m_fFOV = 15.f; if (g_photoHold == 0) notsa::standalone::Fixups::Log("[mdriver] photo hold ended"); } else g_photoHold = 0; } // CR26: see mdphoto
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
