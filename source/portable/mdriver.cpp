#ifdef NOTSA_MDRIVER
#include "StdInc.h"
#include "mdriver.h"
#include "loadlog.h"
#include "standalone/Fixups.h"
#include "Scripts/TheScripts.h"
#include "Scripts/RunningScript.h"
#include "CutsceneMgr.h"
#include "EntryExitManager.h"
#include "CarEnterExit.h"
#include "CarCtrl.h"
#include "Streaming.h"
#include "Camera.h"
#include "scmtrace.h"
#include "PedType.h"
#include "PedGroups.h"
#include "Tasks/TaskTypes/TaskComplexKillPedOnFoot.h"
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
    if (g_out) { std::fprintf(g_out, "at:%llu\ncmd:%s\n# rec=%d gf=%u\n", (unsigned long long)g_clock, cmd.c_str(), notsa::rr::LoadLogRecFrame(), (unsigned)notsa::rr::LoadLogFrameCounter()); std::fflush(g_out); } // `# rec=` (no colon: the injector skips it) = recorded frame / CTimer frame the action ran in
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
// ---- AD1: generic driver (NOTSA_MDRIVER_TABLE = driver-classes.tsv, NOTSA_MDRIVER_RESULT = result line file) ----
struct Row { uint16_t op; int match; /*0 any 1 plain 2 not*/ std::string name, action; std::vector<std::string> args; int settle; };
std::vector<Row>         g_rows;
struct Rec { const uint8_t* p; uint16_t raw; int cond; };
std::vector<Rec>         g_cur, g_last;      // commands of the root thread: the frame in progress / the last frame that ran it
uint32_t                 g_frame0 = 0;
std::string              g_name = "?", g_stopReason, g_failText, g_lastLabel;
int                      g_stopOp = -1; uint32_t g_stopIp = 0;
bool                     g_pass = false, g_fail = false;
int                      g_passFrame = -1, g_failFrame = -1, g_killFrame = -1000000, g_killSettle = 0;
int                      g_maxFrames = 30000;
bool Generic() { static const bool g = std::getenv("NOTSA_MDRIVER_TABLE") != nullptr; return g; }
const uint8_t*           g_loopCmd = nullptr; int g_loopRow = -1, g_loopN = 0;

int GFrame() { return (int)(CTimer::GetFrameCounter() - g_frame0); }
struct Arg { uint32_t bits = 0; int kind = 0; };
const uint8_t* Resolve(const CRunningScript* s, int32_t off) { return off < 0 ? s->m_BaseIP - off : CTheScripts::ScriptSpace.data() + off; }
tScriptParam* LocalAt(const CRunningScript* s, int i) { auto* m = const_cast<CRunningScript*>(s); return m->m_ThisMustBeTheOnlyMissionRunning ? &CTheScripts::LocalVariablesForCurrentMission[i] : &m->m_LocalVars[i]; }
std::vector<Arg> DecodeArgs(const CRunningScript* s, const uint8_t* p, int n) { // the first n arguments of the command at p (SCM typed arguments)
    std::vector<Arg> r; p += 2;
    for (int k = 0; k < n; k++) {
        Arg a; const uint8_t t = *p++;
        switch (t) {
        case 1: a.bits = *(const uint32_t*)p; p += 4; break;
        case 2: a.bits = *(const uint32_t*)(CTheScripts::ScriptSpace.data() + *(const uint16_t*)p); a.kind = 2; p += 2; break;
        case 3: a.bits = LocalAt(s, *(const uint16_t*)p)->uParam; a.kind = 2; p += 2; break;
        case 4: a.bits = (uint32_t)(int32_t)*(const int8_t*)p; p += 1; break;
        case 5: a.bits = (uint32_t)(int32_t)*(const int16_t*)p; p += 2; break;
        case 6: a.bits = *(const uint32_t*)p; a.kind = 1; p += 4; break;
        case 7: case 8: { // array: u16 base, u16 index var, u8 size, u8 flags (bit 7: index var is local)
            const uint16_t base = *(const uint16_t*)p, iv = *(const uint16_t*)(p + 2); const uint8_t fl = p[5];
            const int idx = (int)((fl & 0x80) ? LocalAt(s, iv)->uParam : *(const uint32_t*)(CTheScripts::ScriptSpace.data() + iv));
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
int    AI(const Arg& a) { return (int)a.bits; }

bool LeadsToFail(const CRunningScript* s, const uint8_t* p) { // the code taken when the condition is TRUE: prints M_FAIL within 96 bytes, hopping GOTO / GOSUB (5 hops)
    for (int hop = 0; hop < 5 && p; hop++) {
        for (int i = 0; i + 7 <= 96; i++) { if (!std::memcmp(p + i, "M_FAIL\0", 7)) return true; }
        if ((p[0] == 0x02 || p[0] == 0x50) && p[1] == 0 && p[2] == 1) p = Resolve(s, *(const int32_t*)(p + 3)); else break;
    }
    return false;
}
bool RealWait(size_t i) { // the GOTO_IF_FALSE after the condition at g_last[i] jumps back (a wait loop), or forward to code that does not fail the mission
    size_t j = i + 1; while (j < g_last.size() && g_last[j].cond >= 0) j++;
    if (j >= g_last.size() || (g_last[j].raw & 0x7FFF) != 0x004D) return false;
    const uint8_t* q = g_last[j].p; if (q[2] != 1) return false;
    const uint8_t* t = Resolve(g_thread, *(const int32_t*)(q + 3));
    return t <= q || !LeadsToFail(g_thread, q + 7);
}
bool Likes(ePedType a, ePedType b) { return ((CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_LIKE, a) | CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_RESPECT, a)) & (1u << b)) != 0; }
bool Enemy(ePedType a, ePedType b) { return ((CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_DISLIKE, a) | CPedType::GetPedTypeAcquaintances(ACQUAINTANCE_HATE, a)) & (1u << b)) != 0; }
bool Friendly(CPed* p) { auto* c = FindPlayerPed(); return (int)p->m_nPedType <= 3 || Likes(PED_TYPE_PLAYER1, p->m_nPedType) || (c && c->GetPlayerGroup().GetMembership().IsMember(p)); }
bool KillAllowed(int handle, bool car) {
    if (!car) { auto* p = CPools::GetPed(handle); return p && !p->IsStateDeadForScript() && !Friendly(p); }
    auto* v = CPools::GetVehicle(handle); if (!v) return false;
    std::vector<CPed*> occ; if (v->m_pDriver) occ.push_back(v->m_pDriver); for (auto* pp : v->m_apPassengers) if (pp) occ.push_back(pp);
    for (auto* o : occ) { if (o == Cj() || Friendly(o)) return false; if (!o->IsStateDeadForScript() && !Enemy(PED_TYPE_PLAYER1, o->m_nPedType)) return false; }
    return true;
}
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
        { std::stringstream ss(c[4]); std::string t; while (std::getline(ss, t, ',')) r.args.push_back(t); }
        r.settle = std::atoi(c[5].c_str()); g_rows.push_back(std::move(r));
    }
    g_loaded = true;
}
void WriteResult() {
    const char* rp = std::getenv("NOTSA_MDRIVER_RESULT"); if (!rp) return;
    std::FILE* f = std::fopen(rp, "w"); if (!f) return;
    const bool guardStop = g_stopReason.find("killed a guard") != std::string::npos;
    const char* res = g_pass ? "PASSED" : (g_fail && !guardStop) ? "FAILED" : "STOP";
    const std::string reason = g_pass ? "-" : (g_fail && !guardStop) ? g_failText : (g_stopReason.empty() ? "ended" : g_stopReason);
    char op[8] = "-"; if (g_stopOp >= 0) std::snprintf(op, sizeof op, "%04X", g_stopOp);
    std::fprintf(f, "%s\t%d\t%s\t%x\t%s\t%s\n", res, g_pass ? g_passFrame : (g_fail && !guardStop) ? g_failFrame : GFrame(), g_name.c_str(), g_stopIp, op, reason.c_str());
    std::fclose(f);
}
void Stop(const std::string& reason, int op, uint32_t ip) { if (g_stopReason.empty()) { g_stopReason = reason; g_stopOp = op; g_stopIp = ip; } g_quitIn = 0; std::fprintf(stderr, "[mdriver] stop: %s (op %04X ip %x frame %d)\n", reason.c_str(), op < 0 ? 0 : op, ip, GFrame()); }
void QueueActions(const std::vector<std::string>& acts, int settle) { g_queue = acts; if (settle > 0) g_queue.push_back("wait " + std::to_string(settle)); g_qpos = 0; }
void DispatchRow(const Row& row, const uint8_t* ip, int cop) { // run one table action (Stop() on a failclass / bad verb)
    int need = 0; for (auto& a : row.args) if (a.size() > 1 && a[0] == '$') need = std::max(need, std::atoi(a.c_str() + 1) + 1);
    const auto dec = need ? DecodeArgs(g_thread, ip, need) : std::vector<Arg>{};
    auto F_ = [&](size_t i) -> float { if (i >= row.args.size()) return 0; const auto& a = row.args[i]; if (a[0] == '$') { const size_t n = (size_t)std::atoi(a.c_str() + 1); return n < dec.size() ? AF(dec[n]) : 0.f; } return (float)std::atof(a.c_str()); };
    auto I_ = [&](size_t i) -> int { if (i >= row.args.size()) return 0; const auto& a = row.args[i]; if (a[0] == '$') { const size_t n = (size_t)std::atoi(a.c_str() + 1); return n < dec.size() ? AI(dec[n]) : 0; } return std::atoi(a.c_str()); };
    auto* c = Cj(); const bool inCar = c && c->bInVehicle;
    const auto warp = [&](float x, float y, float z, bool forceVeh) { return std::string((inCar || forceVeh) ? "emit mdveh:" : "emit mdped:") + F(x) + ":" + F(y) + ":" + F(z) + (inCar || forceVeh ? "" : ":0:0"); };
    const std::string& v = row.action;
    if (v == "wait") { QueueActions({}, row.settle); return; }
    if (v == "pedwarp") { QueueActions({ warp(F_(0), F_(1), F_(2), false) }, row.settle); return; }
    if (v == "vehwarp") { QueueActions(inCar ? std::vector<std::string>{ warp(F_(0), F_(1), F_(2), true) } : std::vector<std::string>{}, row.settle); return; }
    if (v == "midpoint") {
        float x, y, z = -100.f;
        if (row.args.size() >= 6) { x = (F_(0) + F_(3)) / 2; y = (F_(1) + F_(4)) / 2; z = (F_(2) + F_(5)) / 2; } else { x = (F_(0) + F_(2)) / 2; y = (F_(1) + F_(3)) / 2; }
        QueueActions({ warp(x, y, z, false) }, row.settle); return;
    }
    if (v == "entercar" || v == "nearest_car") {
        CVehicle* car = nullptr;
        if (v == "entercar") car = CPools::GetVehicle(I_(0));
        else if (c) {
            float best = 50.f;
            for (auto& vv : GetVehiclePool()->GetAllValid()) {
                if (vv.m_pDriver || vv.m_nDoorLock == CARLOCK_LOCKED || vv.m_nDoorLock == CARLOCK_LOCKED_PLAYER_INSIDE || vv.m_nDoorLock == CARLOCK_LOCKOUT_PLAYER_ONLY) continue;
                const float d = (float)(vv.GetPosition() - c->GetPosition()).Magnitude();
                if (d < best) { best = d; car = &vv; }
            }
            if (!car) { Stop("no_car", cop, 0); return; }
        }
        if (!car || !c) { QueueActions({}, row.settle); return; }
        const auto door = CCarEnterExit::GetPositionToOpenCarDoor(car, CAR_DOOR_LF); const CVector l = -car->GetRight();
        QueueActions({ "emit mdped:" + F(door.x + l.x * 2.5f) + ":" + F(door.y + l.y * 2.5f) + ":" + F(car->GetPosition().z + 0.2f) + ":" + F(std::atan2(l.x, -l.y)) + ":0", "wait 10", "key down return", "wait 1", "key up return" }, row.settle);
        return;
    }
    if (v == "kill") { const bool car = cop == 0x0119; QueueActions({ std::string("emit mdkill") + (car ? "car:" : "ped:") + std::to_string(I_(0)) }, row.settle); g_killFrame = GFrame(); g_killSettle = row.settle; return; }
    if (v == "key") {
        std::string kk = row.args.empty() ? "" : row.args[0]; const int n = row.args.size() > 1 ? std::atoi(row.args[1].c_str()) : 10;
        if (!kk.empty() && kk[0] == '$') kk = std::to_string(I_(0));
        const std::string nm = KeyName(kk); if (nm.empty()) { Stop("unmapped-key " + kk, cop, 0); return; }
        QueueActions({ "key down " + nm, "wait " + std::to_string(n), "key up " + nm }, row.settle); return;
    }
    if (v == "skipcut") { QueueActions({ "key down return", "wait 10", "key up return" }, row.settle); return; }
    if (v == "warpto_handle") {
        const int h = I_(0); CVector pos; bool ok = false;
        if (auto* p = CPools::GetPed(h)) { pos = p->GetPosition(); ok = true; } else if (auto* vv = CPools::GetVehicle(h)) { pos = vv->GetPosition(); ok = true; } else if (auto* o = CPools::GetObject(h)) { pos = o->GetPosition(); ok = true; }
        if (!ok) { QueueActions({}, row.settle); return; }
        QueueActions({ warp(pos.x + (std::getenv("NOTSA_MDRIVER_WARPOFF") ? (float)std::atof(std::getenv("NOTSA_MDRIVER_WARPOFF")) : 1.f), pos.y, pos.z + 0.5f, false) }, row.settle); return;
    }
    if (v == "failclass") { Stop(row.args.empty() ? "failclass" : row.args[0], cop, 0); return; }
    Stop("badverb " + v, cop, 0);
}
void ShootDead() { // scripted fights (table header): a hostile kill-ped-on-foot ped aimed at CJ / his group / a ped his side likes is shot dead
    std::vector<int> victims;
    for (auto& p : GetPedPool()->GetAllValid()) {
        if (&p == Cj() || p.IsStateDeadForScript() || Friendly(&p)) continue;
        auto* t = static_cast<CTaskComplexKillPedOnFoot*>(p.GetIntelligence()->FindTaskByType(TASK_COMPLEX_KILL_PED_ON_FOOT));
        if (!t || !t->m_target) continue;
        if (t->m_target == Cj() || Friendly(t->m_target) || ((int)t->m_target->m_nPedType >= 23 && !Enemy(p.m_nPedType, t->m_target->m_nPedType))) victims.push_back(CPools::GetPedRef(&p));
    }
    for (int h : victims) Emit("mdkillped:" + std::to_string(h));
}
void TickGeneric() {
    if (!g_loaded) { LoadTable(std::getenv("NOTSA_MDRIVER_TABLE")); if (const char* o = std::getenv("NOTSA_MDRIVER_OUT")) g_out = std::fopen(o, "w"); if (const char* m = std::getenv("NOTSA_MDRIVER_MAXFRAMES")) g_maxFrames = std::atoi(m); }
    if (!g_cur.empty()) { g_last = std::move(g_cur); g_cur.clear(); }
    if (g_quitIn >= 0) { if (g_quitIn-- == 0) { WriteResult(); Emit("quit"); if (g_out) { std::fclose(g_out); g_out = nullptr; } g_done = true; } return; }
    auto* root = notsa::scmtrace::Root();
    if (!g_started) { if (!root) return; g_started = true; g_thread = root; g_frame0 = CTimer::GetFrameCounter(); g_sameCount = 0; g_lastIp = nullptr; }
    const int fr = GFrame();
    if (root) { char n[9]{}; std::memcpy(n, root->m_szName, 8); g_name = n; }
    if (!root) { std::fprintf(stderr, "[mdriver] mission thread ended at frame %d (pass=%d fail=%d)\n", fr, (int)g_pass, (int)g_fail); g_quitIn = 50; return; }
    if (g_pass && fr - g_passFrame > 1500) { g_quitIn = 0; return; }
    if (g_fail && fr - g_failFrame > 600) { g_quitIn = 0; return; }
    if (fr >= g_maxFrames) { Stop("timeout", -1, (uint32_t)(root->m_IP - root->m_BaseIP)); return; }
    if (fr % 3 == 0 && !CCutsceneMgr::IsCutsceneProcessing() && Cj()) ShootDead();
    for (;;) {
        if (g_wait > 0) { g_wait--; return; }
        if (g_qpos >= g_queue.size()) break;
        const std::string line = g_queue[g_qpos++];
        if (!line.compare(0, 5, "emit ")) Emit(line.substr(5)); else Dispatch(line);
    }
    g_queue.clear(); g_qpos = 0;
    if (CCutsceneMgr::IsCutsceneProcessing()) g_sameCount = 0;
    else if (root->m_IP == g_lastIp) g_sameCount++;
    else { g_sameCount = 0; g_lastIp = root->m_IP; }
    if (g_sameCount < g_stall) return;
    const uint32_t at = (uint32_t)(root->m_IP - root->m_BaseIP);
    g_sameCount = 0; g_lastIp = nullptr;
    // candidates: conditions evaluated false in the last pass of the loop, latest first; the first row in file order that matches any of them wins; kill rows of guards are skipped
    int bestRow = -1, bestOp = -1; size_t bestRec = 0, lastCand = SIZE_MAX;
    for (size_t k = g_last.size(); k-- > 0;) {
        const auto& r = g_last[k]; if (r.cond != 0) continue;
        const int op = r.raw & 0x7FFF; const bool notForm = (r.raw & 0x8000) != 0;
        if (lastCand == SIZE_MAX) lastCand = k;
        for (size_t i = 0; i < g_rows.size(); i++) {
            const auto& w = g_rows[i]; if (w.op != op || !(w.match == 0 || (w.match == 2) == notForm)) continue;
            if (w.action == "kill") { auto h = DecodeArgs(g_thread, r.p, 1); if (h.empty() || !RealWait(k) || !KillAllowed(AI(h[0]), op == 0x0119)) continue; }
            if (bestRow < 0 || (int)i < bestRow) { bestRow = (int)i; bestOp = op; bestRec = k; }
            break;
        }
    }
    if (bestRow < 0) {
        if (lastCand == SIZE_MAX) { Stop("unhandled nocond", -1, at); return; }
        const int op = g_last[lastCand].raw & 0x7FFF; std::string nm = "?"; for (auto& w : g_rows) if (w.op == op) { nm = w.name; break; }
        char b[16]; std::snprintf(b, sizeof b, "%04X", op); Stop(std::string("unhandled ") + b + " " + nm, op, at); return;
    }
    const auto& row = g_rows[bestRow];
    if (g_last[bestRec].p == g_loopCmd && bestRow == g_loopRow) g_loopN++; else { g_loopN = 1; g_loopCmd = g_last[bestRec].p; g_loopRow = bestRow; }
    if (g_loopN > 5) { char b[16]; std::snprintf(b, sizeof b, "loop %04X", bestOp); Stop(b, bestOp, at); return; }
    std::fprintf(stderr, "[mdriver] frame %d ip %x row %d %04X %s -> %s\n", fr, at, bestRow, bestOp, row.name.c_str(), row.action.c_str());
    g_stopIp = at; g_stopOp = bestOp; // the last answered condition (reported when the mission ends without a driver stop)
    DispatchRow(row, g_last[bestRec].p, bestOp);
}
} // namespace

bool Enabled() { static const bool e = std::getenv("NOTSA_MDRIVER") != nullptr || Generic(); return e; }
void OnCommand(const CRunningScript* s, const uint8_t* ptr, uint16_t raw, int cond) {
    if (!Generic() || !g_loaded) return;
    if (s == notsa::scmtrace::Root()) { g_cur.push_back({ ptr, raw, cond }); if (g_cur.size() > 600) g_cur.erase(g_cur.begin(), g_cur.begin() + 300); }
    if (ptr[2] != 9) return;
    const char* lab = (const char*)(ptr + 3); const int op = raw & 0x7FFF;
    if (!std::memcmp(lab, "M_PASS\0", 7)) { if (!g_pass) { g_pass = true; g_passFrame = GFrame(); } return; }
    if (!std::memcmp(lab, "M_FAIL\0", 7)) {
        if (!g_fail) { g_fail = true; g_failFrame = GFrame(); g_failText = g_lastLabel.empty() ? "M_FAIL" : g_lastLabel; }
        if (GFrame() - g_killFrame <= g_killSettle + 5) { g_stopReason = "driver stop: killed a guard"; g_quitIn = 600; }
        return;
    }
    if (op >= 0x00BA && op <= 0x00BD) g_lastLabel.assign(lab, strnlen(lab, 8));
}
void SetKeyPusher(void (*fn)(const char*, bool)) { g_push = fn; }
void SetCmdRunner(void (*fn)(const std::string&)) { g_run = fn; }

bool Cmd(const char* name, const std::string& args) {
    if (std::strncmp(name, "md", 2)) return false;
    auto* ped = Cj();
    if (!std::strcmp(name, "mdkillped")) { if (auto* k = CPools::GetPed(std::atoi(args.c_str()))) { k->m_fHealth = 0.f; k->m_nPedState = PEDSTATE_DEAD; } return true; }
    if (!std::strcmp(name, "mdkillcar")) { if (auto* k = CPools::GetVehicle(std::atoi(args.c_str()))) k->BlowUpCar(nullptr, false); return true; }
    if (!std::strcmp(name, "mdkey")) { const auto c = args.find(':'); if (g_push) g_push(args.c_str() + c + 1, args.compare(0, c, "down") == 0); return true; }
    if (!ped) return true;
    float a[5]{}; std::sscanf(args.c_str(), "%f:%f:%f:%f:%f", a, a + 1, a + 2, a + 3, a + 4);
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
