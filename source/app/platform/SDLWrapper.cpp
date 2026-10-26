#include "StdInc.h"

#ifdef NOTSA_USE_SDL3
#include <SDL3/SDL.h>
#include "SDLWrapper.hpp"
#include <bindings/imgui_impl_sdl3.h>
#include <WindowedMode.hpp>
#include "platform/win/VideoMode.h"
#include "PostEffects.h"
#include "UIRenderer.h"
// One injector implementation for both builds: the standalone run build (NOTSA_STANDALONE_RUN) and the ASI/DLL build with -DGTASA_INPUT_INJECT=ON (NOTSA_INPUT_INJECT)
#if defined(NOTSA_STANDALONE_RUN) || defined(NOTSA_INPUT_INJECT)
#define NOTSA_HAS_INPUT_INJECTOR 1
#endif
#ifdef NOTSA_STANDALONE_RUN
#include "standalone/Fixups.h"
#endif
#include <algorithm>
#if defined(NOTSA_STANDALONE_RUN)
#define NOTSA_INJ_LOG(...) notsa::standalone::Fixups::Log(__VA_ARGS__)
#else
#include <cstdarg>
static void InjLogV(const char* fmt, ...) {
    char buf[256];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);
    NOTSA_LOG_INFO("[inject] {}", buf);
}
#define NOTSA_INJ_LOG(...) InjLogV(__VA_ARGS__)
#endif

static bool IsInFullscreen()
{
    // TODO: Currently with our hacky windowed mode, looks like there's no
    // good way to gather this information.
    return false;
}


#ifdef NOTSA_HAS_INPUT_INJECTOR
// Test-only input injector: NOTSA_STANDALONE_INPUT="wait:3000;key:return;wait:500;down:w;wait:2000;up:w;..." (also a path to a file with the same text).
// Times are ms of wall clock; `wait:N` advances the script clock by N after the previous action; `key:K` = down now + up 150 ms later;
// NOTSA_INPUT_INJECT (cmake -DGTASA_INPUT_INJECT=ON) enables it in the ASI/DLL build too (logs through spdlog instead of Fixups).
// `until:COND[:MAXMS]` is a barrier: the script clock stops until COND holds (or MAXMS, default 60000, elapsed), then continues from there, so everything after it is
// relative to the moment the game was actually ready (deterministic car entry etc.). COND = control (player ped exists, no menu / cutscene / fade, pad not disabled)
// | invehicle (the player ped is in a vehicle) | onfoot (the player ped is not in a vehicle) | menu (main menu idle and rendering). Logs "[until] COND ok|TIMEOUT after N ms".
// `label:NAME` / `skipif:COND:NAME` (COND as above, evaluated once, no waiting): when COND holds the script jumps forward to `label:NAME` (the clock jumps with it, i.e.
// the skipped waits are not waited), e.g. a second car-entry attempt that is skipped when the first one worked.
// `vmode:IDX` applies video mode IDX the way the menu's resolution screen does (m_nPrefsVideoMode, SetVideoMode, CPostEffects::DoScreenModeDependentInitializations): regression
// tests of live mode changes (PLAYTEST-1 U2). `mark:NAME` logs a progress marker; newlines also separate items. K = return|escape|up|down|left|right|space|tab|lshift|lctrl|f1..f12|<single char>.
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <set>
#include <fstream>
#ifdef NOTSA_STANDALONE_RUN
#include "Pools/Pools.h"
#include "Pickups.h"
#include "Wanted.h"
#include "PedIntelligence.h"
#endif
#ifdef NOTSA_STANDALONE_RUN
void RequestMarkScreenshot(const char* name); // standalone/rw/camera.cpp
void NotsaRwMsaaState(char* buf, size_t n);   // standalone/rw/engine.cpp: selected/max level + the live swap chain's multisample type
bool NotsaGrCmd(const char* name, const std::string& args); // GameplayRegress.cpp (gameplay regression suite)
void NotsaGrTick();
#endif
#ifdef NOTSA_PORTABLE
namespace notsa::portable { bool AudioCmd(const char* name, const std::string& args); } // portable/audio_cmd.cpp (MAC-W3)
#include "portable/app_pad.h"    // MAC-W2: gamepad feed + injector commands padattach / padbtn / padaxis / padstat / menustat
#include "portable/app_window.h" // MAC-W2: HiDPI, mouse mode, present pacing; injector commands presentstat / windowstat / fakeocclude
#endif
#ifdef NOTSA_STATEHASH
#include "portable/statehash.h" // RR-1: virtual script clock + cmd:rr_*
#endif
struct NotsaInjectMissionInfo { int32 offset; uint8 bytes[16]; };
bool NotsaInjectStartMission(int32 missionId, NotsaInjectMissionInfo* info); // toolsmenu/DebugModules/Script/MissionDebugModule.cpp
namespace {
#ifdef NOTSA_STATEHASH
// the script clock source: wall clock, or (NOTSA_RR=1) the frame-stepped virtual clock of the state-hash harness, so `wait:N` / `until:` count game frames, not host time
uint64_t InjNowMs() { return notsa::rr::Active() ? notsa::rr::VirtualMs() : (uint64_t)GetTickCount64(); }
#define NOTSA_INJ_NOW_MS() InjNowMs()
#else
#define NOTSA_INJ_NOW_MS() GetTickCount64()
#endif
struct InjEv { uint64_t t; bool down; SDL_Keycode key; int mx = -1, my = -1; int vmode = -1; int vw = 0, vh = 0; int aa = -1; int mission = -1; std::string mark; std::string probe; std::string cmd; std::string until; uint64_t maxMs = 0; bool soft = false; std::string skipIf, label; size_t skipTo = 0; };
std::vector<InjEv> s_InjEvents;
size_t             s_InjNext = 0;
bool               s_InjInit = false;
uint64_t           s_InjStart = 0;
int64_t            s_InjShift = 0;     // total ms the script clock was held by `until:` barriers (minus the waits skipped by `skipif:`)
bool               s_InjHolding = false;
std::map<SDL_Keycode, std::pair<uint64_t, uint64_t>> s_InjKeyDown; // key -> (real ms, CTimer frame) of its last injected down; first == UINT64_MAX once released
uint64_t           s_InjHoldBegin = 0; // real clock (GetTickCount64 - start) when the current hold began

// `probe:NAME` logs one line "[probe] NAME k=v ..." with the game state the cheat / override comparisons read (the same fields tools/standalone/windows/cheat_probe_exe.py reads from the ORIGINAL exe)
void InjProbe(const std::string& name) {
    auto* ped = FindPlayerPed();
    auto* w   = FindPlayerWanted();
    std::string cheats, weap, veh;
    static bool s_OffsLogged = false;
    if (ped && w && !s_OffsLogged) { // the offsets tools/standalone/windows/cheat_probe_exe.py hard-codes for the original exe
        s_OffsLogged = true;
        NOTSA_INJ_LOG("[probe-offsets] health=%#x armour=%#x weapons=%#x weaponStride=%#x totalAmmo=%#x vehicle=%#x modelIdx=%#x wanted=%#x chaos=%#x players0=%#x money=%#x vehStride=%#x",
            (unsigned)((char*)&ped->m_fHealth - (char*)ped), (unsigned)((char*)&ped->m_fArmour - (char*)ped), (unsigned)((char*)&ped->m_aWeapons[0] - (char*)ped),
            (unsigned)((char*)&ped->m_aWeapons[1] - (char*)&ped->m_aWeapons[0]), (unsigned)((char*)&ped->m_aWeapons[0].m_TotalAmmo - (char*)&ped->m_aWeapons[0]),
            (unsigned)((char*)&ped->m_pVehicle - (char*)ped), (unsigned)((char*)&ped->m_nModelIndex - (char*)ped), (unsigned)((char*)&w->m_WantedLevel - (char*)w),
            (unsigned)((char*)&w->m_ChaosLevel - (char*)w), 0xB7CD98u, (unsigned)((char*)&CWorld::Players[0].m_nMoney - (char*)&CWorld::Players[0]), (unsigned)sizeof(CHeli));
    }
    for (int i = 0; i < (int)TOTAL_CHEATS; i++) { if (CCheat::m_aCheatsActive[i]) cheats += std::to_string(i) + ","; }
    if (ped) {
        for (int i = 0; i < 13; i++) { weap += std::to_string((int)ped->m_aWeapons[i].m_Type) + ":" + std::to_string(ped->m_aWeapons[i].m_TotalAmmo) + ","; }
    }
    std::multiset<int> models;
    for (auto& v : GetVehiclePool()->GetAllValid()) { models.insert((int)v.m_nModelIndex); }
    for (auto m : models) { veh += std::to_string(m) + ","; }
    char buf[1500];   // Fixups::Log is wvsprintfA: no floats, 1024 characters at most -> format here, log in pieces
    std::snprintf(buf, sizeof buf, "wl=%d chaos=%d hp=%.1f armour=%.1f money=%d cheated=%d timesCheated=%.0f cheatsOn=[%s] weap=[%s] pveh=%d nveh=%u veh=[%s] weather=%d/%d ts=%.3f hour=%d",
        w ? (int)w->m_WantedLevel : -1, w ? (int)w->m_ChaosLevel : -1, ped ? ped->m_fHealth : -1.f, ped ? ped->m_fArmour : -1.f, CWorld::Players[0].m_nMoney,
        (int)CCheat::m_bHasPlayerCheated, CStats::GetStatValue(STAT_TIMES_CHEATED), cheats.c_str(), weap.c_str(),
        ped && ped->bInVehicle && ped->m_pVehicle ? (int)ped->m_pVehicle->m_nModelIndex : -1, (unsigned)models.size(), veh.c_str(),
        (int)CWeather::ForcedWeatherType, (int)CWeather::NewWeatherType, CTimer::GetTimeScale(), (int)CClock::GetGameClockHours());
    for (size_t off = 0, part = 0; off < std::strlen(buf); off += 800, ++part) {
        char piece[820];
        std::snprintf(piece, sizeof piece, "%.800s", buf + off);
        NOTSA_INJ_LOG("[probe] %s%s %s", name.c_str(), part ? "(cont)" : "", piece);
    }
}

bool InjCond(const std::string& c) {
    if (c == "control") {
        auto* ped = FindPlayerPed();
        auto* pad = CPad::GetPad(0);
        return ped && pad && !FrontEndMenuManager.m_bMenuActive && !CCutsceneMgr::IsRunning() && !pad->bPlayerSafe && !pad->ArePlayerControlsDisabled() && !TheCamera.GetFading();
    }
    if (c == "invehicle") {
        auto* ped = FindPlayerPed();
        return ped && ped->bInVehicle && ped->m_pVehicle;
    }
    if (c == "onfoot") { // the player is out of any vehicle (the leave-vehicle animation has finished): gates the Return key presses of the gameplay regression missions route instead of a fixed wait
        auto* ped = FindPlayerPed();
        return ped && !ped->bInVehicle;
    }
    if (c == "nomission") { // no mission is running (the opening mission ends one script poll after CJ is on the tutorial bike): gates the dismount in routes/header_free.inc (CHEATS-ARMOUR: under load the dismount came before the script saw CJ in the bike -> SETUP mission=1)
        return !CTheScripts::IsPlayerOnAMission();
    }
    if (c == "cutscene") { // a cutscene is running (the new-game intro): `until:cutscene` replaces a fixed wait for its loading (gameplay regression suite)
        return CCutsceneMgr::IsRunning() && !FrontEndMenuManager.m_bMenuActive;
    }
    if (c == "nocutscene") {
        return !CCutsceneMgr::IsRunning() && !FrontEndMenuManager.m_bMenuActive && FindPlayerPed() != nullptr;
    }
    if (c == "menu") { // the main menu is up and drawing frames (start-up takes ~75 s on a display whose vsync-locked Present is slow: the splash/loading screens are frame-count loops)
        return gGameState == GAME_STATE_FRONTEND_IDLE && FrontEndMenuManager.m_bMenuActive && CTimer::GetFrameCounter() > 3;
    }
    return true; // unknown condition: do not block
}

SDL_Keycode InjKey(const std::string& n) {
    static const std::pair<const char*, SDL_Keycode> k[] = { { "return", SDLK_RETURN }, { "escape", SDLK_ESCAPE }, { "up", SDLK_UP }, { "down", SDLK_DOWN },
        { "left", SDLK_LEFT }, { "right", SDLK_RIGHT }, { "space", SDLK_SPACE }, { "tab", SDLK_TAB }, { "lshift", SDLK_LSHIFT }, { "lctrl", SDLK_LCTRL },
        { "enter", SDLK_RETURN } };
    for (auto& [name, v] : k) { if (n == name) return v; }
    if (n.size() >= 2 && n[0] == 'f' && isdigit((unsigned char)n[1])) return SDLK_F1 + (std::atoi(n.c_str() + 1) - 1);
    return n.empty() ? 0 : (SDL_Keycode)tolower((unsigned char)n[0]);
}
void InjInit() {
    s_InjInit = true;
    const char* env = std::getenv("NOTSA_STANDALONE_INPUT");
    if (!env) return;
    std::string text = env;
    if (std::ifstream f{ env }) { text.assign(std::istreambuf_iterator<char>(f), {}); }
    std::replace(text.begin(), text.end(), '\n', ';');
    std::stringstream ss(text);
    uint64_t t = 0;
    for (std::string item; std::getline(ss, item, ';');) {
        const auto c = item.find(':');
        if (c == std::string::npos) continue;
        const auto cmd = item.substr(0, c), arg = item.substr(c + 1);
        if (cmd == "wait") { t += std::atoll(arg.c_str()); }
        else if (cmd == "key") { s_InjEvents.push_back({ t, true, InjKey(arg) }); s_InjEvents.push_back({ t + 150, false, InjKey(arg) }); t += 150; }
        else if (cmd == "mouse") { // mouse:x,y  (window pixels): SDL_EVENT_MOUSE_MOTION, e.g. to select a menu item the real pointer hovers over
            int x = 0, y = 0;
            std::sscanf(arg.c_str(), "%d,%d", &x, &y);
            InjEv ev{ t, true, 0 };
            ev.mx = x; ev.my = y;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "vmode") { // vmode:IDX
            InjEv ev{ t, true, 0 };
            ev.vmode = std::atoi(arg.c_str());
            if (arg.find('x') != std::string::npos) { std::sscanf(arg.c_str(), "%dx%d", &ev.vw, &ev.vh); ev.vmode = 0; } // vmode:WxH = the first video mode of that size (host independent)
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "aa") { // aa:LEVEL (menu value 1 = off, 2.. = "1".. in the Anti Aliasing option)
            InjEv ev{ t, true, 0 };
            ev.aa = std::atoi(arg.c_str());
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "until") { // until:COND[:MAXMS[:soft]]  (soft: a miss logs "MISS", not "TIMEOUT": the next step handles it, e.g. a retry of the car entry; soak.sh counts only TIMEOUT as a failed gate)
            InjEv ev{ t, true, 0 };
            const auto c2 = arg.find(':');
            ev.until = arg.substr(0, c2);
            ev.maxMs = c2 == std::string::npos ? 60000 : (uint64_t)std::atoll(arg.c_str() + c2 + 1);
            ev.soft = arg.size() >= 5 && arg.compare(arg.size() - 5, 5, ":soft") == 0;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "label") { // label:NAME  (target of skipif)
            InjEv ev{ t, true, 0 };
            ev.label = arg;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "skipif") { // skipif:COND:LABEL
            InjEv ev{ t, true, 0 };
            const auto c2 = arg.find(':');
            if (c2 == std::string::npos) continue;
            ev.skipIf = arg.substr(0, c2);
            ev.label = arg.substr(c2 + 1);
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "mission") { // mission:N  starts main.scm mission N (ids of toolsmenu MissionDebugModule: 2 Intro, 11 Big Smoke, 12 Ryder ...) like its "Missions Starter" window; repro of mission bugs
            InjEv ev{ t, true, 0 };
            ev.mission = std::atoi(arg.c_str());
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "kill" || cmd == "arrest") { // kill:0 / arrest:0  the player ped enters PEDSTATE_DEAD / PEDSTATE_ARRESTED: CGameLogic::Update runs the "wasted" / "busted" flow (e.g. a mission must fail and clean up: DoDeathArrestCheck)
            InjEv ev{ t, true, 0 };
            ev.mission = cmd == "kill" ? 100000 : 100001;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "cmd") { // cmd:NAME[:args]  run-build diagnostics actions (probe), see InjRunCmd
            InjEv ev{ t, true, 0 };
            ev.cmd = arg;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "mark") { // mark:NAME  logs "[mark] NAME" when the script clock reaches it (progress markers for tools/standalone/soak.sh)
            InjEv ev{ t, true, 0 };
            ev.mark = arg;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "probe") { // probe:NAME  logs the game state (see InjProbe)
            InjEv ev{ t, true, 0 };
            ev.probe = arg;
            s_InjEvents.push_back(ev);
        }
        else if (cmd == "down" || cmd == "up") { s_InjEvents.push_back({ t, cmd == "down", InjKey(arg) }); }
    }
    std::stable_sort(s_InjEvents.begin(), s_InjEvents.end(), [](auto& a, auto& b) { return a.t < b.t; });
    for (auto& ev : s_InjEvents) { // resolve `skipif:COND:LABEL` to the index of `label:LABEL` (the first one after the skipif)
        if (ev.skipIf.empty()) continue;
        ev.skipTo = s_InjEvents.size();
        for (size_t i = &ev - s_InjEvents.data() + 1; i < s_InjEvents.size(); i++) {
            if (s_InjEvents[i].skipIf.empty() && s_InjEvents[i].label == ev.label) { ev.skipTo = i; break; }
        }
    }
    s_InjStart = NOTSA_INJ_NOW_MS();
    NOTSA_INJ_LOG("input injector: %u events", (unsigned)s_InjEvents.size());
}

#ifdef NOTSA_STANDALONE_RUN
// NOTSA_STANDALONE_PROBE=1: once a second (wall clock) log the player's vitals / wanted level and the nearest peds (type, health, weapon, active task chain) and the live pickups.
// Diagnostics for the combat playtest findings (police idle, no damage, no weapon drops); run build only.
static void ProbeDump() {
    static const bool on = std::getenv("NOTSA_STANDALONE_PROBE") != nullptr;
    static uint64_t   last = 0;
    static float      lastHp = -1.f, lastArm = -1.f;
    if (!on) return;
    if (gGameState != GAME_STATE_IDLE || FrontEndMenuManager.m_bMenuActive || CTimer::GetFrameCounter() < 5) return;
    {
        static char lastCheat[32] = {};
        if (std::strncmp(lastCheat, CCheat::m_CheatString, 30) != 0) {
            std::strncpy(lastCheat, CCheat::m_CheatString, 30);
            NOTSA_INJ_LOG("[pc-cheatstr] t=%lu '%s'", (unsigned long)GetTickCount(), lastCheat);
        }
    }
    auto* pl = FindPlayerPed();
    if (!pl) return;
    if (pl->m_fHealth != lastHp || pl->m_fArmour != lastArm) {
        NOTSA_INJ_LOG("[pc-dmg] t=%lu player hp %d -> %d armour %d -> %d lastDamager=%p weap=%d", (unsigned long)GetTickCount(), NOTSA_FTOL(int32, lastHp), NOTSA_FTOL(int32, pl->m_fHealth), NOTSA_FTOL(int32, lastArm), NOTSA_FTOL(int32, pl->m_fArmour), (void*)pl->m_pLastEntityDamage, (int)pl->m_nLastWeaponDamage);
        lastHp = pl->m_fHealth; lastArm = pl->m_fArmour;
    }
    {
        static int lastChaos = -12345, lastWl = -12345, lastMaxWl = -12345;
        auto* ww = FindPlayerWanted();
        const int wl = ww ? (int)ww->GetWantedLevel() : -1, ch = ww ? ww->m_ChaosLevel : -1, mx = (int)CWanted::MaximumWantedLevel;
        if (wl != lastWl || ch != lastChaos || mx != lastMaxWl) {
            NOTSA_INJ_LOG("[pc-wanted] t=%lu wanted %d -> %d chaos %d -> %d maxWanted %d -> %d maxChaos %d", (unsigned long)GetTickCount(), lastWl, wl, lastChaos, ch, lastMaxWl, mx, (int)CWanted::MaximumChaosLevel);
            lastWl = wl; lastChaos = ch; lastMaxWl = mx;
        }
    }
    {
        static bool once = false;
        if (!once) {
            once = true;
#pragma warning(suppress: 4200 4838)
            NOTSA_INJ_LOG("[offs] CVehicle: autoPilot=%x gas=%x brake=%x steerAngle=%x driver=%x | autoPilot: mission=%x temp=%x style=%x cruise=%x speed=%x target=%x",
                (unsigned)offsetof(CVehicle, m_autoPilot), (unsigned)offsetof(CVehicle, m_GasPedal), (unsigned)offsetof(CVehicle, m_BrakePedal), (unsigned)offsetof(CVehicle, m_fSteerAngle), (unsigned)offsetof(CVehicle, m_pDriver),
                (unsigned)offsetof(CAutoPilot, m_nCarMission), (unsigned)offsetof(CAutoPilot, m_nTempAction), (unsigned)offsetof(CAutoPilot, m_nCarDrivingStyle), (unsigned)offsetof(CAutoPilot, m_nCruiseSpeed), (unsigned)offsetof(CAutoPilot, m_speed), (unsigned)offsetof(CAutoPilot, m_TargetEntity));
        }
    }
    const uint64_t now = GetTickCount64();
    if (now - last < 1000) return;
    last = now;
    auto* w = FindPlayerWanted();
    auto  pos = pl->GetPosition();
    auto& aw  = pl->GetActiveWeapon();
    auto* pt  = pl->GetIntelligence()->GetTaskManager().GetActiveTask();
    NOTSA_INJ_LOG("[pc] t=%lu frame=%u player pos=(%d,%d,%d) hp=%d arm=%d state=%d wanted=%d chaos=%d backoff=%d/%d/%d weap=%d(ammo %d) task=%d",
        (unsigned long)GetTickCount(), (unsigned)CTimer::GetFrameCounter(), NOTSA_FTOL(int32, pos.x), NOTSA_FTOL(int32, pos.y), NOTSA_FTOL(int32, pos.z), NOTSA_FTOL(int32, pl->m_fHealth), NOTSA_FTOL(int32, pl->m_fArmour), (int)pl->m_nPedState,
        w ? (int)w->GetWantedLevel() : -1, w ? w->m_ChaosLevel : -1, w ? (int)w->m_bPoliceBackOff : -1, w ? (int)w->m_bPoliceBackOffGarage : -1, w ? (int)w->m_bEverybodyBackOff : -1,
        (int)aw.GetType(), (int)aw.GetTotalAmmo(), pt ? (int)pt->GetTaskType() : -1);
    {
        auto* pd  = pl->GetPlayerData();
        auto* pad = CPad::GetPad(0);
        NOTSA_INJ_LOG("[pc]   flags canBeDamaged=%d invuln=%d bulletproof=%d meleeproof=%d collproof=%d | padSafe=%d padSafeCut=%d ctrlDisabled=%d | cutscene=%d fading=%d inVeh=%d mission=%d",
            pd ? (int)pd->m_bCanBeDamaged : -1, (int)pl->physicalFlags.bInvulnerable, (int)pl->physicalFlags.bBulletProof, (int)pl->physicalFlags.bMeleeProof, (int)pl->physicalFlags.bCollisionProof,
            (int)pad->bPlayerSafe, (int)pad->bPlayerSafeForCutscene, (int)pad->ArePlayerControlsDisabled(), (int)CCutsceneMgr::IsRunning(), (int)TheCamera.GetFading(), (int)pl->bInVehicle, (int)CTheScripts::IsPlayerOnAMission());
    }
    {
        int nPeds = 0, nVehs = 0;
        for (auto& pp : GetPedPool()->GetAllValid()) { (void)pp; nPeds++; }
        for (auto& vv : GetVehiclePool()->GetAllValid()) { (void)vv; nVehs++; }
        NOTSA_INJ_LOG("[pc]   pop: densMul=%d peds(pool)=%d vehs(pool)=%d totalPeds=%d civ=%d gang=%d cop=%d inUseMax=%d | popcycle other=%d cops=%d gangs=%d zoneType=%d | randCars=%d lawCars=%d missCars=%d carDens=%d | noGang=%d onlyGang=%d noCops=%d moreCars=%d polStation=%d",
            NOTSA_FTOL(int32, CPopulation::PedDensityMultiplier * 100.f), nPeds, nVehs, (int)CPopulation::ms_nTotalPeds, (int)CPopulation::ms_nTotalCivPeds, (int)CPopulation::ms_nTotalGangPeds, (int)CPopulation::ms_nNumCop, (int)CPopulation::MaxNumberOfPedsInUse,
            NOTSA_FTOL(int32, CPopCycle::m_NumOther_Peds * 10.f), NOTSA_FTOL(int32, CPopCycle::m_NumCops_Peds * 10.f), NOTSA_FTOL(int32, CPopCycle::m_NumGangs_Peds * 10.f), (int)CPopCycle::m_nCurrentZoneType,
            (int)CCarCtrl::NumRandomCars, (int)CCarCtrl::NumLawEnforcerCars, (int)CCarCtrl::NumMissionCars, NOTSA_FTOL(int32, CCarCtrl::CarDensityMultiplier * 100.f),
            (int)CPopulation::m_bDontCreateRandomGangMembers, (int)CPopulation::m_bOnlyCreateRandomGangMembers, (int)CPopulation::m_bDontCreateRandomCops, (int)CPopulation::m_bMoreCarsAndFewerPeds, (int)CPopulation::bInPoliceStation);
    }
    int shown = 0, total = 0;
    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (&ped == pl) continue;
        const float d = (ped.GetPosition() - pos).Magnitude();
        if (d > 60.f) continue;
        total++;
        if (shown >= 20) continue;
        shown++;
        auto& mgr = ped.GetIntelligence()->GetTaskManager();
        char chain[160] = {};
        int  n = 0;
        for (auto* t = mgr.GetActiveTask(); t && n < 140; t = t->GetSubTask()) {
            n += std::snprintf(chain + n, sizeof(chain) - n, "%d>", (int)t->GetTaskType());
        }
        auto& pw = ped.GetActiveWeapon();
        NOTSA_INJ_LOG("[pc]   pedpos %p pos10=(%d,%d,%d) area=%d currArea=%d type=%d contact=%p contactModel=%d contactArea=%d", (void*)&ped, NOTSA_FTOL(int32, ped.GetPosition().x * 10.f), NOTSA_FTOL(int32, ped.GetPosition().y * 10.f), NOTSA_FTOL(int32, ped.GetPosition().z * 10.f), (int)ped.GetAreaCode(), (int)CGame::currArea, (int)ped.m_nPedType, (void*)ped.m_pContactEntity, ped.m_pContactEntity ? (int)ped.m_pContactEntity->m_nModelIndex : -1, ped.m_pContactEntity ? (int)ped.m_pContactEntity->GetAreaCode() : -1);
        NOTSA_INJ_LOG("[pc]   ped %p model=%d type=%d d=%d hp=%d st=%d weap=%d(ammo %d, slot %d) tasks=%s evq=%d evhi=%d cur=%d tmpTask=%d perTask=%d",
            (void*)&ped, (int)ped.m_nModelIndex, (int)ped.m_nPedType, NOTSA_FTOL(int32, d), NOTSA_FTOL(int32, ped.m_fHealth), (int)ped.m_nPedState, (int)pw.GetType(), (int)pw.GetTotalAmmo(), (int)ped.m_nActiveWeaponSlot, chain,
            (int)ped.GetEventGroup().GetNumEventsInQueue(), ped.GetEventGroup().GetHighestPriorityEvent() ? (int)ped.GetEventGroup().GetHighestPriorityEvent()->GetEventType() : -1,
            ped.GetEventHandlerHistory().GetCurrentEvent() ? (int)ped.GetEventHandlerHistory().GetCurrentEvent()->GetEventType() : -1,
            mgr.GetTemporaryEventResponseTask() ? (int)mgr.GetTemporaryEventResponseTask()->GetTaskType() : -1, mgr.GetPresistentEventResponseTask() ? (int)mgr.GetPresistentEventResponseTask()->GetTaskType() : -1);
    }
    {
        int loaded = 0;
        for (size_t i = 0; i < 64; i++) { if (ThePaths.IsAreaLoaded(i)) loaded++; }
        const auto nn = ThePaths.FindNodeClosestToCoors(pos, PATH_TYPE_VEH);
        NOTSA_INJ_LOG("[pc]   paths: areasLoaded=%d closestVehNode(area=%d node=%d)", loaded, (int)nn.m_wAreaId, (int)nn.m_wNodeId);
    }
    {
        int shownV = 0;
        for (auto& veh : GetVehiclePool()->GetAllValid()) {
            const float d = (veh.GetPosition() - pos).Magnitude();
            if (d > 100.f || shownV >= 6) continue;
            shownV++;
            if (veh.IsAutomobile()) {
                auto& am = static_cast<CAutomobile&>(veh);
                NOTSA_INJ_LOG("[pc]   vehy %p z=%d up.z=%d contactWheels=%d physFlags=%08x vehFlags=%08x status=%d nCollided=%d c0=%p c1=%p health=%d fire=%p engineOn=%d", (void*)&veh, NOTSA_FTOL(int32, veh.GetPosition().z), NOTSA_FTOL(int32, veh.GetUp().z * 100.f), (int)am.m_nNumContactWheels,
                    (unsigned)veh.m_nPhysicalFlags, (unsigned)*(const uint32*)&veh.vehicleFlags, (int)veh.GetStatus(), (int)veh.m_nNumEntitiesCollided, (void*)veh.m_apCollidedEntities[0], (void*)veh.m_apCollidedEntities[1], NOTSA_FTOL(int32, veh.m_fHealth), (void*)veh.m_pFire, (int)veh.vehicleFlags.bEngineOn);
            }
            NOTSA_INJ_LOG("[pc]   vehx %p apSpeed=%d maxTraffic=%d style=%d obstruct=%p target=%p gas=%d brake=%d hbrake=%d steer=%d speedScale=%u", (void*)&veh, NOTSA_FTOL(int32, veh.m_autoPilot.m_speed * 100.f), NOTSA_FTOL(int32, veh.m_autoPilot.m_fMaxTrafficSpeed * 100.f), (int)veh.m_autoPilot.m_nCarDrivingStyle,
                (void*)veh.m_autoPilot.m_ObstructingEntity, (void*)veh.m_autoPilot.m_TargetEntity, NOTSA_FTOL(int32, veh.m_GasPedal * 100.f), NOTSA_FTOL(int32, veh.m_BrakePedal * 100.f), (int)veh.vehicleFlags.bIsHandbrakeOn, NOTSA_FTOL(int32, veh.m_fSteerAngle * 100.f), (unsigned)veh.m_autoPilot.m_nSpeedScaleFactor);
            NOTSA_INJ_LOG("[pc]   veh %p model=%d d=%d speed=%d law=%d status=%d mission=%d temp=%d cruise=%d ctrl=%x driver=%d(type %d) dest=(%d,%d) nodes(cur %d/%d next %d/%d)", (void*)&veh, (int)veh.m_nModelIndex, NOTSA_FTOL(int32, d), NOTSA_FTOL(int32, veh.m_vecMoveSpeed.Magnitude() * 100.f),
                (int)veh.IsLawEnforcementVehicle(), (int)veh.GetStatus(), (int)veh.m_autoPilot.m_nCarMission, (int)veh.m_autoPilot.m_nTempAction, (int)veh.m_autoPilot.m_nCruiseSpeed, (unsigned)veh.m_autoPilot.m_nCarCtrlFlags,
                veh.m_pDriver ? 1 : 0, veh.m_pDriver ? (int)veh.m_pDriver->m_nPedType : -1, NOTSA_FTOL(int32, veh.m_autoPilot.m_vecDestinationCoors.x), NOTSA_FTOL(int32, veh.m_autoPilot.m_vecDestinationCoors.y), (int)veh.m_autoPilot.m_currentAddress.m_wAreaId, (int)veh.m_autoPilot.m_currentAddress.m_wNodeId, (int)veh.m_autoPilot.m_startingRouteNode.m_wAreaId, (int)veh.m_autoPilot.m_startingRouteNode.m_wNodeId);
        }
    }
    int pk = 0;
    for (auto& p : CPickups::GetAllActivePickups()) { pk++; (void)p; }
    NOTSA_INJ_LOG("[pc]   peds within 60m: %d (shown %d), active pickups: %d", total, shown, pk);
}
#endif
#ifdef NOTSA_STANDALONE_RUN
// cmd:wanted:N | cmd:tp:X:Y:Z | cmd:rig:DX:DY:HEADING | cmd:rigpush:VX:VY | cmd:nullcol:RANGE:MINHEIGHT | cmd:rigstate | cmd:give:WEAPON:AMMO | cmd:armour:N | cmd:health:N | cmd:log:TEXT  (run build diagnostics; the real cheat / script paths are exercised by typed keys instead whenever possible)
static CVehicle* s_InjRig[2] = {}; // truck, trailer of cmd:rig
static void InjRunCmd(const std::string& c) {
    auto* pl = FindPlayerPed();
    char name[32] = {}; float a = 0, b = 0, d = 0; int w = 0, n = 0;
    std::sscanf(c.c_str(), "%31[^:]", name);
    const std::string args = c.size() > strlen(name) ? c.substr(strlen(name) + 1) : "";
#ifdef NOTSA_PORTABLE
    if (::notsa::portable::AudioCmd(name, args)) { return; } // MAC-W3: cmd:radio:ID / cmd:track:ID / cmd:audiostat (audio runs on the native Mac)
    if (::notsa::portable::InputCmd(name, args) || ::notsa::portable::WindowCmd(name, args)) { return; } // MAC-W2
#endif
#ifdef NOTSA_STATEHASH
    if (notsa::rr::Cmd(name, args)) { return; } // RR-1: cmd:rr_start[:SEED] / rr_stop / rr_stat
#endif
    if (NotsaGrCmd(name, args)) { return; } // gameplay regression suite commands (GameplayRegress.cpp: snap, spawncop, spawnveh, shootlast, shootdrv, hitplayer, time, quit)
    NOTSA_INJ_LOG("[cmd] %s", c.c_str());
    if (!pl) return;
    if (!std::strcmp(name, "wanted")) { pl->CheatWantedLevel((eWantedLevel)std::atoi(args.c_str())); }
    else if (!std::strcmp(name, "heal")) { pl->m_fHealth = 100.f; pl->m_fArmour = 0.f; } // heal: the player's health back to the new-game 100 (hp stays what a scenario step assumes after a random chase / fight)
    else if (!std::strcmp(name, "carinfo")) { // carinfo[:RANGE=20]  [carinfo] lines: the player's position/state and every vehicle within RANGE (ROUTE-FLAKES: where a cheat car lands relative to CJ)
        int rg = 20; std::sscanf(args.c_str(), "%d", &rg);
        const auto pp = pl->GetPosition();
        NOTSA_INJ_LOG("[carinfo] player pos=(%.1f,%.1f,%.1f) head=%.0f state=%d inVeh=%d", pp.x, pp.y, pp.z, RadiansToDegrees(pl->GetHeading()), (int)pl->m_nPedState, (int)pl->bInVehicle);
        for (auto& v : GetVehiclePool()->GetAllValid()) {
            const auto vp = v.GetPosition();
            if ((vp - pp).Magnitude() > (float)rg) continue;
            NOTSA_INJ_LOG("[carinfo] veh %p model %d pos=(%.1f,%.1f,%.1f) dist=%.1f z-rel=%.1f status=%d driver=%p moveSpd=%.2f", (void*)&v, (int)v.m_nModelIndex, vp.x, vp.y, vp.z, (vp - pp).Magnitude(), vp.z - pp.z, (int)v.GetStatus(), (void*)v.m_pDriver, v.m_vecMoveSpeed.Magnitude());
        }
    }
    else if (!std::strcmp(name, "tp")) {
        std::sscanf(args.c_str(), "%f:%f:%f", &a, &b, &d);
        const CVector p{ a, b, d };
        CStreaming::LoadScene(p); CStreaming::LoadSceneCollision(p);
        pl->Teleport(p, false);
        CStreaming::LoadScene(p); CStreaming::LoadAllRequestedModels(false);
    }
    else if (!std::strcmp(name, "give")) { std::sscanf(args.c_str(), "%d:%d", &w, &n); CStreaming::RequestModel(CWeaponInfo::GetWeaponInfo((eWeaponType)w)->m_nModelId1, STREAMING_GAME_REQUIRED); CStreaming::LoadAllRequestedModels(false); pl->GiveWeapon((eWeaponType)w, n, false); pl->SetCurrentWeapon((eWeaponType)w); }
    else if (!std::strcmp(name, "cheats")) { // dump the cheat dispatch table
        for (int i = 0; i < 24; i++) { NOTSA_INJ_LOG("[cheat] %d fn=%p hash=%08x active=%d", i, (void*)CCheat::m_aCheatFunctions[i], (unsigned)CCheat::m_aCheatHashKeys[i], (int)CCheat::m_aCheatsActive[i]); }
    }
    else if (!std::strcmp(name, "cheat")) { CCheat::ApplyCheat((eCheats)std::atoi(args.c_str())); }
    else if (!std::strcmp(name, "spawnped")) { // spawnped:MODEL:PEDTYPE  a ped 6 m in front of the player (standing, no tasks beyond the default)
        int mdl = 7, ty = 4;
        std::sscanf(args.c_str(), "%d:%d", &mdl, &ty);
#ifdef NOTSA_PORTABLE
        if (mdl < 0 || mdl >= (int)CModelInfo::NUM_MODEL_INFOS || !CModelInfo::GetModelInfo(mdl) || CModelInfo::GetModelInfo(mdl)->GetModelType() != MODEL_INFO_PED) { NOTSA_INJ_LOG("[cmd] spawnped model %d is not a ped model (no model info): skipped", mdl); return; }
#endif
        CStreaming::RequestModel((eModelID)mdl, STREAMING_GAME_REQUIRED); CStreaming::LoadAllRequestedModels(false);
        const auto p = pl->GetPosition() + pl->GetForward() * 6.f;
        auto* np = CPopulation::AddPed((ePedType)ty, (eModelID)mdl, p, false);
        NOTSA_INJ_LOG("[cmd] spawned ped %p model %d type %d at (%d,%d,%d)", (void*)np, mdl, ty, NOTSA_FTOL(int32, p.x), NOTSA_FTOL(int32, p.y), NOTSA_FTOL(int32, p.z));
    }
    else if (!std::strcmp(name, "shootnear")) { // shootnear:RANGE  lethal damage event on every non-player ped within range
        int rg = 15; std::sscanf(args.c_str(), "%d", &rg);
        for (auto& ped : GetPedPool()->GetAllValid()) {
            if (&ped == pl || (ped.GetPosition() - pl->GetPosition()).Magnitude() > (float)rg) continue;
            const auto ok = CWeapon::GenerateDamageEvent(&ped, pl, WEAPON_AK47, 500, PED_PIECE_TORSO, 0);
            NOTSA_INJ_LOG("[cmd] shot ped %p type %d -> %d", (void*)&ped, (int)ped.m_nPedType, (int)ok);
        }
    }
    else if (!std::strcmp(name, "shoot")) { // shoot:PEDTYPE:RANGE  lethal weapon damage event (the path bullets take) on every ped of that type within range of the player
        int ty = 6, rg = 40;
        std::sscanf(args.c_str(), "%d:%d", &ty, &rg);
        for (auto& ped : GetPedPool()->GetAllValid()) {
            if (&ped == pl || (int)ped.m_nPedType != ty || (ped.GetPosition() - pl->GetPosition()).Magnitude() > (float)rg) continue;
            const auto ok = CWeapon::GenerateDamageEvent(&ped, pl, WEAPON_AK47, 500, PED_PIECE_TORSO, 0);
            NOTSA_INJ_LOG("[cmd] shot ped %p type %d hp %d -> GenerateDamageEvent=%d", (void*)&ped, ty, NOTSA_FTOL(int32, ped.m_fHealth), (int)ok);
        }
    }
    else if (!std::strcmp(name, "spawnmany")) { // spawnmany:N:RADIUS:ANGLEOFFSET_DEG  N peds on a ring around the player (models cycle through male/female skins), standing
        int cnt = 8, rad = 5, off = 0;
        std::sscanf(args.c_str(), "%d:%d:%d", &cnt, &rad, &off);
#ifdef NOTSA_PORTABLE
        static const int models[] = { 7, 9, 10, 12, 13, 20, 21, 22, 23, 40, 41, 64, 66, 85, 87, 90, 92 }; // STAB1: 65 is a hole in peds.ide (no model info): RequestModel(65) dereferences null in the exe too
#else
        static const int models[] = { 7, 9, 10, 12, 13, 20, 21, 22, 23, 40, 41, 64, 65, 85, 87, 90, 92 };
#endif
        for (int i = 0; i < cnt; i++) {
            const int mdl = models[i % (int)std::size(models)];
#ifdef NOTSA_PORTABLE
            if (mdl < 0 || mdl >= (int)CModelInfo::NUM_MODEL_INFOS || !CModelInfo::GetModelInfo(mdl) || CModelInfo::GetModelInfo(mdl)->GetModelType() != MODEL_INFO_PED) { NOTSA_INJ_LOG("[cmd] spawnmany %d model %d is not a ped model (no model info): skipped", i, mdl); continue; }
#endif
            CStreaming::RequestModel((eModelID)mdl, STREAMING_GAME_REQUIRED); CStreaming::LoadAllRequestedModels(false);
            const float ang = DegreesToRadians((float)off) + (float)i * 6.2831853f / (float)cnt;
            const auto p = pl->GetPosition() + CVector{ std::cos(ang) * (float)rad, std::sin(ang) * (float)rad, 0.f };
            auto* np = CPopulation::AddPed(PED_TYPE_CIVMALE, (eModelID)mdl, p, false);
            NOTSA_INJ_LOG("[cmd] spawnmany %d model %d ped %p at (%d,%d,%d)", i, mdl, (void*)np, NOTSA_FTOL(int32, p.x), NOTSA_FTOL(int32, p.y), NOTSA_FTOL(int32, p.z));
        }
    }
    else if (!std::strcmp(name, "killall")) { // killall:WEAPON:PIECE:DIR:RANGE  lethal damage event with the given weapon/piece/direction on every non-player ped within range
        int wp = WEAPON_AK47, pc = PED_PIECE_TORSO, dir = 0, rg = 15;
        std::sscanf(args.c_str(), "%d:%d:%d:%d", &wp, &pc, &dir, &rg);
        for (auto& ped : GetPedPool()->GetAllValid()) {
            if (&ped == pl || (ped.GetPosition() - pl->GetPosition()).Magnitude() > (float)rg) continue;
            const auto ok = CWeapon::GenerateDamageEvent(&ped, pl, (eWeaponType)wp, 500, (ePedPieceTypes)pc, (uint8)dir);
            NOTSA_INJ_LOG("[cmd] killall ped %p model %d weap %d piece %d dir %d -> %d", (void*)&ped, (int)ped.m_nModelIndex, wp, pc, dir, (int)ok);
        }
    }
    else if (!std::strcmp(name, "lift")) { // lift:RANGE:H  raise every non-player ped within range by H metres (they fall; high falls kill)
        int rg = 15, h = 30;
        std::sscanf(args.c_str(), "%d:%d", &rg, &h);
        for (auto& ped : GetPedPool()->GetAllValid()) {
            if (&ped == pl || (ped.GetPosition() - pl->GetPosition()).Magnitude() > (float)rg) continue;
            auto p = ped.GetPosition(); p.z += (float)h;
            ped.SetPosn(p);
            NOTSA_INJ_LOG("[cmd] lift ped %p model %d to z=%d", (void*)&ped, (int)ped.m_nModelIndex, NOTSA_FTOL(int32, p.z));
        }
    }
    else if (!std::strcmp(name, "dumpdead")) {
#define DEADLOG(...) do { char dl_[480]; std::snprintf(dl_, sizeof dl_, __VA_ARGS__); NOTSA_INJ_LOG("%s", dl_); } while (0) // dumpdead:RANGE:FULL  per dead/dying ped: pose, slope IK, anims and skin bone matrices (max axis-length deviation, bone distance from the root)
        int rg = 30, full = 0;
        std::sscanf(args.c_str(), "%d:%d", &rg, &full);
        for (auto& ped : GetPedPool()->GetAllValid()) {
            if (&ped == pl || (ped.GetPosition() - pl->GetPosition()).Magnitude() > (float)rg) continue;
            if (ped.m_nPedState != PEDSTATE_DEAD && ped.m_nPedState != PEDSTATE_DIE) continue;
            const auto& m = ped.GetMatrix();
            DEADLOG("[dead] ped %p model %d state %d hp %d pos=(%.2f,%.2f,%.2f) fwd=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) n578=(%.2f,%.2f,%.2f) slopePitch=%.3f slopeRoll=%.3f bSlope=%d standing=%d wasStanding=%d surf=%d coll=%d",
                (void*)&ped, (int)ped.m_nModelIndex, (int)ped.m_nPedState, NOTSA_FTOL(int32, ped.m_fHealth), m.GetPosition().x, m.GetPosition().y, m.GetPosition().z, m.GetForward().x, m.GetForward().y, m.GetForward().z, m.GetUp().x, m.GetUp().y, m.GetUp().z,
                ped.field_578.x, ped.field_578.y, ped.field_578.z, ped.m_pedIK.m_fSlopePitch, ped.m_pedIK.m_fSlopeRoll, (int)ped.m_pedIK.bSlopePitch, (int)ped.bIsStanding, (int)ped.bWasStanding, (int)ped.m_nContactSurface, (int)ped.GetUsesCollision());
            for (auto* a = RpAnimBlendClumpGetFirstAssociation(ped.GetRpClump()); a; a = RpAnimBlendGetNextAssociation(a)) {
                DEADLOG("[dead]   anim grp %d id %d blend %.3f delta %.3f time %.3f speed %.2f flags %04x", (int)a->m_AnimGroupId, (int)a->m_AnimId, a->m_BlendAmount, a->m_BlendDelta, a->m_CurrentTime, a->m_Speed, (unsigned)a->m_Flags);
            }
            if (auto* hier = GetAnimHierarchyFromSkinClump(ped.GetRpClump())) {
                auto* arr = RpHAnimHierarchyGetMatrixArray(hier);
                const int nn = hier->numNodes;
                float maxDev = 0.f, maxDist = 0.f; int devBone = -1, distBone = -1;
                for (int i = 0; i < nn; i++) {
                    const auto& mm = arr[i];
                    const auto L = [](const RwV3d& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); };
                    const float dv = std::max({ std::abs(L(mm.right) - 1.f), std::abs(L(mm.up) - 1.f), std::abs(L(mm.at) - 1.f) });
                    const float ds = L(mm.pos);
                    if (!(dv <= maxDev)) { maxDev = dv; devBone = hier->nodeInfo[i].id; }
                    if (ds > maxDist) { maxDist = ds; distBone = hier->nodeInfo[i].id; }
                    if (full) DEADLOG("[dead]   bone %d id %d pos=(%.2f,%.2f,%.2f) r=(%.2f,%.2f,%.2f) u=(%.2f,%.2f,%.2f) a=(%.2f,%.2f,%.2f)", i, (int)hier->nodeInfo[i].id, mm.pos.x, mm.pos.y, mm.pos.z, mm.right.x, mm.right.y, mm.right.z, mm.up.x, mm.up.y, mm.up.z, mm.at.x, mm.at.y, mm.at.z);
                }
                DEADLOG("[dead]   bones=%d maxAxisDev=%.3f (bone id %d) maxDist=%.2f (bone id %d)", nn, maxDev, devBone, maxDist, distBone);
            }
        }
#undef DEADLOG
    }
    else if (!std::strcmp(name, "rig")) { // rig:DX:DY:HEADING_DEG  a Linerunner + Articulated trailer attached like ATTACH_TRAILER_TO_CAB, truck at player+(DX,DY), heading in degrees (0 = +Y), trailer 14 m behind
        float hd = 0.f; std::sscanf(args.c_str(), "%f:%f:%f", &a, &b, &hd);
        for (const auto m : { MODEL_LINERUN, MODEL_ARTICT1 }) { CStreaming::RequestModel(m, STREAMING_GAME_REQUIRED); }
        CStreaming::LoadAllRequestedModels(false);
        const float rad = DegreesToRadians(hd);
        const CVector fwd{ -std::sin(rad), std::cos(rad), 0.f };
        const auto tp = pl->GetPosition() + CVector{ a, b, 1.5f };
        auto* truck = CCarCtrl::CreateCarForScript(MODEL_LINERUN, tp, false);
        auto* trl = CCarCtrl::CreateCarForScript(MODEL_ARTICT1, tp - fwd * 14.f, false);
        if (truck && trl) {
            for (auto* v : { truck, trl }) { v->SetHeading(rad); v->m_vecMoveSpeed = CVector{}; v->m_vecTurnSpeed = CVector{}; }
            truck->SetPosn(tp); trl->SetPosn(tp - fwd * 14.f);
            const auto ok = trl->SetTowLink(truck, true);
            s_InjRig[0] = truck; s_InjRig[1] = trl;
            NOTSA_INJ_LOG("[cmd] rig truck %p trailer %p SetTowLink=%d tractor=%p towed=%p", (void*)truck, (void*)trl, (int)ok, (void*)truck->m_pVehicleBeingTowed, (void*)trl->m_pTowingVehicle);
        } else { NOTSA_INJ_LOG("[cmd] rig: spawn failed truck=%p trailer=%p", (void*)truck, (void*)trl); }
    }
    else if (!std::strcmp(name, "rigpush")) { // rigpush:VX:VY  set the move speed of the rig vehicles (0.3 = 15 m/s)
        std::sscanf(args.c_str(), "%f:%f", &a, &b);
        for (auto* v : s_InjRig) if (v) { v->m_vecMoveSpeed = CVector{ a, b, 0.f }; }
        NOTSA_INJ_LOG("[cmd] rigpush (%d,%d)/100 truck=%p trailer=%p", NOTSA_FTOL(int32, a * 100.f), NOTSA_FTOL(int32, b * 100.f), (void*)s_InjRig[0], (void*)s_InjRig[1]);
    }
    else if (!std::strcmp(name, "nullcol")) { // nullcol:RANGE:MINHEIGHT  drop the col data (not the col model) of every building model within range of the player that is taller than MINHEIGHT (walls / houses, roads stay): the playtest-2 condition, a building whose col model has no data
        int rg = 40; float mh = 3.f; std::sscanf(args.c_str(), "%d:%f", &rg, &mh);
        int nulled = 0, already = 0, seen = 0;
        for (auto& bld : GetBuildingPool()->GetAllValid()) {
            if ((bld.GetPosition() - pl->GetPosition()).Magnitude() > (float)rg) continue;
            auto* mi = CModelInfo::GetModelInfo(bld.m_nModelIndex);
            auto* cm = mi ? mi->GetColModel() : nullptr;
            if (!cm) continue;
            seen++;
            if (!cm->m_pColData) { already++; continue; }
            if (cm->GetBoundingBox().m_vecMax.z - cm->GetBoundingBox().m_vecMin.z <= mh) continue;
            cm->m_pColData = nullptr; nulled++;
        }
        NOTSA_INJ_LOG("[cmd] nullcol range %d: %d building models seen, %d already without col data, %d nulled", rg, seen, already, nulled);
    }
    else if (!std::strcmp(name, "rigstate")) {
        for (auto* v : s_InjRig) if (v) NOTSA_INJ_LOG("[cmd] rig veh %p model %d pos=(%d,%d,%d) speed*100=%d status=%d towing=%p towed=%p nColl=%d", (void*)v, (int)v->m_nModelIndex, NOTSA_FTOL(int32, v->GetPosition().x), NOTSA_FTOL(int32, v->GetPosition().y), NOTSA_FTOL(int32, v->GetPosition().z), NOTSA_FTOL(int32, v->m_vecMoveSpeed.Magnitude() * 100.f), (int)v->GetStatus(), (void*)v->m_pTowingVehicle, (void*)v->m_pVehicleBeingTowed, (int)v->m_nNumEntitiesCollided);
    }
    else if (!std::strcmp(name, "hour")) { CClock::SetGameClock((uint8)std::atoi(args.c_str()), 0, 0); } // hour:H  game clock H:00 (heat haze post effect needs >= 10, PLAYTEST-1 U2/U17 checks)
    else if (!std::strcmp(name, "armour")) { pl->m_fArmour = (float)std::atoi(args.c_str()); }
    else if (!std::strcmp(name, "health")) { pl->m_fHealth = (float)std::atoi(args.c_str()); }
}
#endif
void InjPump() {
    if (!s_InjInit) InjInit();
#ifdef NOTSA_STANDALONE_RUN
    ProbeDump();
    NotsaGrTick();
#endif
    const uint64_t real = NOTSA_INJ_NOW_MS() - s_InjStart;
    // script clock: frozen at the barrier's time while it holds, shifted by the total hold time afterwards
    const auto clock = [&]() -> uint64_t { return s_InjHolding ? s_InjEvents[s_InjNext].t : (uint64_t)((int64_t)real - s_InjShift); };
    while (s_InjNext < s_InjEvents.size() && s_InjEvents[s_InjNext].t <= clock()) {
        const auto& ev = s_InjEvents[s_InjNext];
        if (!ev.until.empty()) {
            if (!s_InjHolding) { s_InjHolding = true; s_InjHoldBegin = real; }
            const bool ok = InjCond(ev.until), timeout = !ok && real - s_InjHoldBegin >= ev.maxMs;
            if (!ok && !timeout) break; // keep holding, retry next pump
            NOTSA_INJ_LOG("[until] %s %s after %u ms (script %u ms)", ev.until.c_str(), ok ? "ok" : ev.soft ? "MISS(soft)" : "TIMEOUT", (unsigned)(real - s_InjHoldBegin), (unsigned)ev.t);
            s_InjShift += (int64_t)(real - s_InjHoldBegin);
            s_InjHolding = false;
            s_InjNext++;
            continue;
        }
        if (!ev.skipIf.empty()) {
            const bool jump = InjCond(ev.skipIf);
            NOTSA_INJ_LOG("[skipif] %s %s -> %s", ev.skipIf.c_str(), jump ? "true, skipping to" : "false, continuing at", ev.label.c_str());
            if (jump && ev.skipTo < s_InjEvents.size()) {
                s_InjShift -= (int64_t)(s_InjEvents[ev.skipTo].t - ev.t); // the script clock jumps with the script position
                s_InjNext = ev.skipTo;
            } else {
                s_InjNext++;
            }
            continue;
        }
        if (!ev.cmd.empty()) {
            const std::string c = ev.cmd;
            s_InjNext++;
#ifdef NOTSA_STANDALONE_RUN
            InjRunCmd(c);
#endif
            continue;
        }
        if (!ev.label.empty()) { s_InjNext++; continue; }
        if (!ev.probe.empty()) { s_InjNext++; InjProbe(ev.probe); continue; }
        if (ev.vmode >= 0) {
            s_InjNext++;
            int vm = ev.vmode;
            if (ev.vw > 0) { // vmode:WxH -> the first video mode of that size (host independent)
                vm = -1;
                for (int i = 0; i < RwEngineGetNumVideoModes(); i++) {
                    RwVideoMode info{};
                    RwEngineGetVideoModeInfo(&info, i);
                    if (info.width == ev.vw && info.height == ev.vh) { vm = i; break; }
                }
                if (vm < 0) { NOTSA_INJ_LOG("[vmode] no video mode %dx%d at %u ms", ev.vw, ev.vh, (unsigned)clock()); continue; }
            }
            if (vm < RwEngineGetNumVideoModes()) {
                FrontEndMenuManager.m_nPrefsVideoMode = vm;
                SetVideoMode(vm);
                CPostEffects::DoScreenModeDependentInitializations();
            }
            NOTSA_INJ_LOG("[vmode] %d at %u ms", vm, (unsigned)clock());
            continue;
        }
        if (ev.aa >= 0) {
            s_InjNext++;
            // what the menu's Anti Aliasing option does on accept (CMenuManager::ProcessPCMenuOptions MENU_ACTION_ANTIALIASING)
            auto& mm = FrontEndMenuManager;
            mm.m_nDisplayAntialiasing = ev.aa;
            if (mm.m_nDisplayAntialiasing != mm.m_nPrefsAntialiasing) {
                mm.m_nPrefsAntialiasing = mm.m_nDisplayAntialiasing;
                RwD3D9ChangeMultiSamplingLevels(mm.m_nDisplayAntialiasing);
                SetVideoMode(mm.m_nPrefsVideoMode);
                mm.SaveSettings();
            }
            char st[200]{};
#ifdef NOTSA_STANDALONE_RUN
            NotsaRwMsaaState(st, sizeof(st));
#endif
            NOTSA_INJ_LOG("[aa] %d at %u ms: %s", ev.aa, (unsigned)clock(), st);
            continue;
        }
        if (ev.mission >= 100000) {
            s_InjNext++;
            if (auto* p = FindPlayerPed()) { if (ev.mission == 100000) { p->m_fHealth = 0.f; p->m_nPedState = PEDSTATE_DEAD; } else { p->m_nPedState = PEDSTATE_ARRESTED; } }
            NOTSA_INJ_LOG("[kill] player %s", ev.mission == 100000 ? "killed" : "arrested");
            continue;
        }
        if (ev.mission >= 0) {
            // like the "Missions Starter" window: a running mission is failed first (its code lives in the shared MissionBlock), the new one starts once FailCurrentMission is back to 0
            if (CTheScripts::IsPlayerOnAMission() && CTheScripts::FailCurrentMission == 0) {
                if (CCutsceneMgr::HasLoaded()) {
                    CCutsceneMgr::DeleteCutsceneData();
                }
                CTheScripts::FailCurrentMission = 2;
                NOTSA_INJ_LOG("[mission] a mission is running: failing it first");
            }
            if (CTheScripts::FailCurrentMission > 0 || CTheScripts::IsPlayerOnAMission()) {
                break; // retry on a later pump
            }
            s_InjNext++;
            NotsaInjectMissionInfo mi{};
            const bool ok = NotsaInjectStartMission(ev.mission, &mi);
            NOTSA_INJ_LOG("[mission] start %d: %s (script offset %d, first bytes %02x %02x %02x %02x %02x %02x %02x %02x)", ev.mission, ok ? "ok" : "failed", mi.offset, mi.bytes[0], mi.bytes[1], mi.bytes[2], mi.bytes[3], mi.bytes[4], mi.bytes[5], mi.bytes[6], mi.bytes[7]);
            continue;
        }
        if (ev.mark.empty() && ev.mx < 0 && ev.vmode < 0 && ev.aa < 0 && ev.key != 0) {
            // A key must stay down for at least 150 ms of wall clock AND two game frames (CTimer frame counter; 1 s cap in case it does not advance) after it was really injected: when a hitch/low fps makes the pump run late,
            // the scheduled down and up (150 ms apart) would otherwise be pushed in the same pump, the game sees no press at all (soak: ESC lost, quit never opened).
            auto& st = s_InjKeyDown[ev.key];
            if (ev.down) {
                st = { real, (uint64_t)CTimer::GetFrameCounter() };
            } else if (st.first != UINT64_MAX && (real - st.first < 150 || ((uint64_t)CTimer::GetFrameCounter() < st.second + 2 && real - st.first < 1000))) {
                break; // retry on a later pump
            } else {
                st.first = UINT64_MAX;
            }
        }
        s_InjNext++;
        const uint64_t now = clock();
        SDL_Event e{};
        if (!ev.mark.empty()) {
            NOTSA_INJ_LOG("[mark] %s at %u ms t=%lu", ev.mark.c_str(), (unsigned)now, (unsigned long)GetTickCount()); // t= is the same clock as the standalone memlog lines
#ifdef NOTSA_STANDALONE_RUN
            RequestMarkScreenshot(ev.mark.c_str()); // tools/standalone/soak.sh: one mark_<name>.bmp at the next Present (needs NOTSA_STANDALONE_SCREENSHOT)
#endif
            continue;
        }
        if (ev.mx >= 0) {
            e.type     = SDL_EVENT_MOUSE_MOTION;
#ifdef NOTSA_PORTABLE
            e.motion.x = notsa::portable::MouseFromGame((float)ev.mx); // `mouse:x,y` is in game pixels, SDL positions are window points
            e.motion.y = notsa::portable::MouseFromGame((float)ev.my);
#else
            e.motion.x = (float)ev.mx;
            e.motion.y = (float)ev.my;
#endif
            SDL_PushEvent(&e);
            NOTSA_INJ_LOG("injected mouse %d,%d at %u ms", ev.mx, ev.my, (unsigned)now);
            continue;
        }
        e.type         = ev.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.key      = ev.key;
        e.key.down     = ev.down;
        e.key.scancode = SDL_GetScancodeFromKey(ev.key, nullptr);
        SDL_PushEvent(&e);
        NOTSA_INJ_LOG("injected key %d %s at %u ms", (int)ev.key, ev.down ? "down" : "up", (unsigned)now);
    }
}
} // namespace

#ifndef NOTSA_STANDALONE_RUN
// DLL build only: NOTSA_STANDALONE_SCREENSHOT=<k> writes frame_<n>.bmp (back buffer, 24 bit) every k-th ProcessEvents call (first NOTSA_STANDALONE_SCREENSHOT_MAX=30 files)
static void InjDumpBackBuffer() {
    static int s_Every = -2, s_Frame = 0, s_Written = 0;
    if (s_Every == -2) {
        const char* e = std::getenv("NOTSA_STANDALONE_SCREENSHOT");
        s_Every = e ? (std::atoi(e) > 0 ? std::atoi(e) : 1) : -1;
    }
    const char* mx = std::getenv("NOTSA_STANDALONE_SCREENSHOT_MAX");
    if (s_Every < 0 || s_Written >= (mx ? std::atoi(mx) : 30)) {
        return;
    }
    // NOTSA_STANDALONE_SCREENSHOT_MS=<ms>: wall-clock cadence instead of every k-th call (ProcessEvents runs far more often than frames while loading)
    static const uint64_t s_Ms = std::getenv("NOTSA_STANDALONE_SCREENSHOT_MS") ? (uint64_t)std::atoll(std::getenv("NOTSA_STANDALONE_SCREENSHOT_MS")) : 0;
    static uint64_t       s_Last = 0;
    if (s_Ms) {
        const auto now = GetTickCount64();
        if (now - s_Last < s_Ms) {
            return;
        }
        s_Last = now;
    } else if ((s_Frame++ % s_Every) != 0) {
        return;
    }
    auto* dev = (IDirect3DDevice9*)RwD3D9GetCurrentD3DDevice();
    if (!dev) {
        return;
    }
    IDirect3DSurface9 *bb = nullptr, *sys = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) {
        return;
    }
    D3DSURFACE_DESC d{};
    bb->GetDesc(&d);
    if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(dev->GetRenderTargetData(bb, sys))) {
        D3DLOCKED_RECT lr{};
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            char name[64];
            std::snprintf(name, sizeof(name), "frame_%d.bmp", s_Written++);
            if (FILE* f = std::fopen(name, "wb")) {
                const uint32_t rowBytes = (d.Width * 3 + 3) & ~3u, imgSize = rowBytes * d.Height;
                uint8_t        hdr[54] = { 'B', 'M' };
                auto           put32   = [&](int o, uint32_t v) { std::memcpy(hdr + o, &v, 4); };
                put32(2, 54 + imgSize); put32(10, 54); put32(14, 40); put32(18, d.Width); put32(22, d.Height);
                hdr[26] = 1; hdr[28] = 24; put32(34, imgSize);
                std::fwrite(hdr, 1, 54, f);
                std::vector<uint8_t> row(rowBytes, 0);
                for (int y = (int)d.Height - 1; y >= 0; y--) {
                    const uint8_t* src = (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch;
                    for (UINT x = 0; x < d.Width; x++) {
                        if (d.Format == D3DFMT_R5G6B5) {
                            const uint16_t v = ((const uint16_t*)src)[x];
                            row[x * 3 + 0] = (uint8_t)((v & 31) * 255 / 31); row[x * 3 + 1] = (uint8_t)(((v >> 5) & 63) * 255 / 63); row[x * 3 + 2] = (uint8_t)(((v >> 11) & 31) * 255 / 31);
                        } else {
                            row[x * 3 + 0] = src[x * 4 + 0]; row[x * 3 + 1] = src[x * 4 + 1]; row[x * 3 + 2] = src[x * 4 + 2];
                        }
                    }
                    std::fwrite(row.data(), 1, rowBytes, f);
                }
                std::fclose(f);
            }
            sys->UnlockRect();
        }
    }
    if (sys) sys->Release();
    bb->Release();
}
#endif
#endif

namespace notsa {
namespace SDLWrapper {
bool Initialize() {
    return true;
}

void Terminate() {

}

void ProcessEvents() {
    ZoneScoped;
#ifdef NOTSA_HAS_INPUT_INJECTOR
    InjPump();
#ifndef NOTSA_STANDALONE_RUN
    InjDumpBackBuffer();
#endif
#endif

#ifdef NOTSA_PORTABLE
    notsa::portable::UpdateMouseMode(); // MAC-W2: relative in game, absolute pointer in the front-end menu
#endif
    // Now process events
    const auto* const imCtx = ImGui::GetCurrentContext();
    const auto* const imIO  = imCtx ? &imCtx->IO : nullptr;
    for (SDL_Event e; SDL_PollEvent(&e);) {
#ifndef NOTSA_STANDALONE_RUN // no imgui backend in the standalone run build (UIRenderer stub)
        if (imIO) {
            ImGui_ImplSDL3_ProcessEvent(&e);
        }
#endif

#ifdef NOTSA_PORTABLE
        if (notsa::portable::PadOnEvent(e)) { // MAC-W2: gamepad hot plug
            continue;
        }
#endif
        switch (e.type) {
        case SDL_EVENT_QUIT: {
            RsGlobal.quit = true;
            continue;
        }
        case SDL_EVENT_WINDOW_RESIZED: {
            const auto w = e.window.data1,
                       h = e.window.data2;

            NOTSA_LOG_DEBUG(
                "SDL: Window resized: {} x {}",
                w, h
            );

            continue;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            if (notsa::ui::UIRenderer::GetInstance().IsActive()) {
                break;
            }
            static CVector2D s_MousePos{};

            // Use desktop cursor speed if it's windowed, accelerate ingame otherwise.
            if (IsInFullscreen() && FrontEndMenuManager.m_bMenuActive) {
                s_MousePos.x += e.motion.xrel * CCamera::m_fMouseAccelHorzntl * 100.f;
                s_MousePos.y += e.motion.yrel * CCamera::m_fMouseAccelVertical * 100.f;
            } else {
#ifdef NOTSA_PORTABLE
                s_MousePos.x = notsa::portable::MouseToGame(e.motion.x); // window points -> game (drawable) pixels
                s_MousePos.y = notsa::portable::MouseToGame(e.motion.y);
#else
                s_MousePos.x = e.motion.x;
                s_MousePos.y = e.motion.y;
#endif
            }
            FrontEndMenuManager.m_nMousePosWinX = NOTSA_FTOL(int32, s_MousePos.x);
            FrontEndMenuManager.m_nMousePosWinY = NOTSA_FTOL(int32, s_MousePos.y);
            break;
        }
        }

        if (CPad::ProcessEvent(e, imIO && imIO->WantCaptureMouse, imIO && imIO->WantCaptureKeyboard)) {
            continue;
        }

        //NOTSA_LOG_DEBUG("SDL: Unprocessed event: {}", e.type);
    }
}
}; // namespace SDLWrapper
}; // namespace notsa
#endif
