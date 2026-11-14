#ifdef NOTSA_MDRIVER
#include "StdInc.h"
#include "mdriver.h"
#include "Scripts/TheScripts.h"
#include "Scripts/RunningScript.h"
#include "CutsceneMgr.h"
#include "EntryExitManager.h"
#include "CarEnterExit.h"
#include "CarCtrl.h"
#include "Streaming.h"
#include "Camera.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>

namespace notsa::mdriver {
namespace {
struct Cond { std::string expr, op; double val; };
struct Step { std::string name; std::vector<Cond> conds; std::vector<std::string> actions; };
std::vector<Step>        g_steps;
std::string              g_script;           // thread name
int                      g_stall = 600, g_maxAnswers = 12;
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
    if (g_out) { std::fprintf(g_out, "at:%llu\ncmd:%s\n", (unsigned long long)g_clock, cmd.c_str()); std::fflush(g_out); }
    char name[32]{}; std::sscanf(cmd.c_str(), "%31[^:]", name);
    if (!Cmd(name, cmd.size() > std::strlen(name) ? cmd.substr(std::strlen(name) + 1) : "") && g_run) g_run(cmd);
}
std::string F(double v) { char b[40]; std::snprintf(b, sizeof b, "%.9g", (double)(float)v); return b; }
// the ped put at (x,y,z) with heading h (the player walked there)
void PedTo(float x, float y, float z, float h, bool cam) { Emit("mdped:" + F(x) + ":" + F(y) + ":" + F(z) + ":" + F(h) + ":" + (cam ? "1" : "0")); }
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
        if (!std::strncmp((const char*)s->m_szName, g_script.c_str(), 8)) return s;
    }
    return nullptr;
}
} // namespace

bool Enabled() { static const bool e = std::getenv("NOTSA_MDRIVER") != nullptr; return e; }
void SetKeyPusher(void (*fn)(const char*, bool)) { g_push = fn; }
void SetCmdRunner(void (*fn)(const std::string&)) { g_run = fn; }

bool Cmd(const char* name, const std::string& args) {
    if (std::strncmp(name, "md", 2)) return false;
    auto* ped = Cj();
    if (!std::strcmp(name, "mdkey")) { const auto c = args.find(':'); if (g_push) g_push(args.c_str() + c + 1, args.compare(0, c, "down") == 0); return true; }
    if (!ped) return true;
    float a[5]{}; std::sscanf(args.c_str(), "%f:%f:%f:%f:%f", a, a + 1, a + 2, a + 3, a + 4);
    const CVector p{ a[0], a[1], a[2] };
    CStreaming::LoadScene(p); CStreaming::LoadSceneCollision(p);
    if (!std::strcmp(name, "mdped")) { // CWorld::Remove + SetPosn + heading + Add (the headless TS harness's addToWorldAt)
        CWorld::Remove(ped);
        ped->SetPosn(p);
        ped->m_fCurrentRotation = ped->m_fAimingRotation = a[3];
        ped->SetHeading(a[3]);
        CWorld::Add(ped);
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
