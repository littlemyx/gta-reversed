#include "StdInc.h"

// Run-build-only (NOTSA_STANDALONE_RUN) injector commands of the gameplay regression suite (tools/standalone/gameplay_regress.sh, docs/STANDALONE.md "Gameplay regression suite").
// Called from SDLWrapper.cpp's InjRunCmd (`cmd:NAME[:args]` items of NOTSA_STANDALONE_INPUT) and InjPump (the per-pump tick). Everything here only OBSERVES or PLACES things the way the game's own code does;
// it never changes game logic. Output: `[gr] NAME#n key=value ...` lines in standalone.log (parsed by tools/standalone/gameplay_regress/gr_check.py).
#if defined(NOTSA_STANDALONE_RUN) && defined(NOTSA_USE_SDL3)
#include <SDL3/SDL.h>
#include <string>
#include <cmath>
#include "standalone/Fixups.h"
#include "Pools/Pools.h"
#include "Pickups.h"
#include "Wanted.h"
#include "PedIntelligence.h"
#include "PostEffects.h"
#include "MenuManager.h"
#include "CarCtrl.h"
#include "Population.h"
#include "Enums/eTaskType.h"
#include "TaskComplexKillPedOnFoot.h"

#define GR_LOG(...) notsa::standalone::Fixups::Log(__VA_ARGS__)

namespace {
CPed*     s_LastCop    = nullptr;
CVehicle* s_LastVeh    = nullptr;
CPed*     s_LastDriver = nullptr;
float     s_CamPrev    = 0.f;       // previous camera heading (rad)
double    s_CamAccum   = 0.0;       // SIGNED accumulated camera heading (rad): a mouse sweep in one direction keeps growing past 2*pi
bool      s_CamInit    = false;
int       s_SnapSeq    = 0;
unsigned  s_HitEvents  = 0;         // number of times a hit / fall reaction task appeared in the player's task chain (rising edges, counted on every pump, so a reaction between two snapshots is not missed)
bool      s_HitPrev    = false;

float CamHeading() {
    const auto f = TheCamera.GetForward();
    return std::atan2(-f.x, f.y);
}

// is `t` (or a sub task) one of the "got hit" reactions?
bool IsHitTask(eTaskType t) {
    return (t >= TASK_SIMPLE_HIT_BACK && t <= TASK_SIMPLE_HIT_WALL) || t == TASK_SIMPLE_HIT_BEHIND || t == TASK_SIMPLE_HIT_HEAD || t == TASK_COMPLEX_FALL_AND_GET_UP;
}

std::string Chain(CPed& ped, bool* hit = nullptr) {
    std::string s;
    int         n = 0;
    for (auto* t = ped.GetIntelligence()->GetTaskManager().GetActiveTask(); t && n < 8; t = t->GetSubTask(), n++) {
        if (hit && IsHitTask(t->GetTaskType())) *hit = true;
        s += std::to_string((int)t->GetTaskType()) + ">";
    }
    return s.empty() ? "-" : s;
}

bool PedHasTask(CPed& ped, eTaskType type) {
    for (auto* t = ped.GetIntelligence()->GetTaskManager().GetActiveTask(); t; t = t->GetSubTask()) {
        if (t->GetTaskType() == type) return true;
    }
    return false;
}

void Snap(const std::string& name) {
    auto* pl = FindPlayerPed();
    auto* w  = FindPlayerWanted();
    const auto id = name + "#" + std::to_string(++s_SnapSeq);
    {
        const CVector pos = pl ? pl->GetPosition() : CVector{};
        bool hit = false;
        const auto ch = pl ? Chain(*pl, &hit) : std::string("-");
        auto* pad = CPad::GetPad(0);
        const bool ctl = pl && pad && !FrontEndMenuManager.m_bMenuActive && !CCutsceneMgr::IsRunning() && !pad->bPlayerSafe && !pad->ArePlayerControlsDisabled() && !TheCamera.GetFading();
        GR_LOG("[gr] %s A t=%lu frame=%u pl=%d x=%d y=%d z=%d head=%d hp=%d arm=%d pst=%d wl=%d chaos=%d mission=%d fail=%d invh=%d vmodel=%d ptasks=%s phit=%d hitevents=%u ctl=%d cut=%d fade=%d menu=%d relmouse=%d cam10=%d camraw10=%d money=%d vspd=%d",
            id.c_str(), (unsigned long)GetTickCount(), (unsigned)CTimer::GetFrameCounter(), pl ? 1 : 0, (int)pos.x, (int)pos.y, (int)pos.z, pl ? (int)(RadiansToDegrees(pl->GetHeading())) : 0,
            pl ? (int)pl->m_fHealth : -1, pl ? (int)pl->m_fArmour : -1, pl ? (int)pl->m_nPedState : -1, w ? (int)w->GetWantedLevel() : -1, w ? (int)w->m_ChaosLevel : -1,
            (int)CTheScripts::IsPlayerOnAMission(), (int)CTheScripts::FailCurrentMission, pl ? (int)pl->bInVehicle : 0, pl && pl->m_pVehicle ? (int)pl->m_pVehicle->m_nModelIndex : -1,
            ch.c_str(), (int)hit, s_HitEvents, (int)ctl, (int)CCutsceneMgr::IsRunning(), (int)TheCamera.GetFading(), (int)FrontEndMenuManager.m_bMenuActive,
            PSGLOBAL(sdlWindow) ? (int)SDL_GetWindowRelativeMouseMode(PSGLOBAL(sdlWindow)) : -1, (int)(s_CamAccum * 572.9578), (int)(CamHeading() * 572.9578), (int)FindPlayerInfo().m_nMoney, pl && pl->m_pVehicle ? (int)(pl->m_pVehicle->m_vecMoveSpeed.Magnitude() * 1000.f) : -1);
    }
    if (pl && GetPedPool() && GetVehiclePool()) { // (the menu / start-up has no pools yet: only the video part D is logged)
        int peds = 0, cops = 0, copsDead = 0, copsDeadAlive = 0, copsCar = 0, copsFoot = 0, copsEngaged = 0, copsFootEngaged = 0, nearCop = 99999, law = 0, lawDrv = 0, nearLaw = 99999, copsHurtInCar = 0;
        std::string copdz; // vertical offset (decimetres) from the player of up to 8 living cops within 40 m: cops standing on top of CJ (playtest-2 U15) show up as big positive values
        int copsNear40 = 0, copsStack = 0; // copsStack: living cops standing ABOVE CJ (horizontal distance < 1.5 m, > 0.5 m higher): the U15 symptom. Cops on another level of the station (+1.4 m at the balcony nodes) are the exe's own placement (LSPD-POP)
        const CVector pos = pl ? pl->GetPosition() : CVector{};
        for (auto& ped : GetPedPool()->GetAllValid()) {
            if (&ped == pl) continue;
            const float d = (ped.GetPosition() - pos).Magnitude();
            if (d > 60.f) continue;
            peds++;
            if (ped.m_nPedType != PED_TYPE_COP) continue;
            cops++;
            const bool dead = ped.m_fHealth <= 0.f;
            if (dead) {
                copsDead++;
                if (ped.m_nPedState != PEDSTATE_DEAD && ped.m_nPedState != PEDSTATE_DIE) copsDeadAlive++; // hp 0 but the ped never started dying: the "events never reach car occupants" signature
                if (ped.bInVehicle && ped.m_nPedState != PEDSTATE_DEAD && ped.m_nPedState != PEDSTATE_DIE) copsHurtInCar++;
                continue;
            }
            nearCop = std::min(nearCop, (int)d);
            if (d <= 40.f) { if ((ped.GetPosition() - pos).Magnitude2D() < 1.5f && ped.GetPosition().z - pos.z > 0.5f) copsStack++; copsNear40++; if (copsNear40 <= 8) copdz += std::to_string((int)((ped.GetPosition().z - pos.z) * 10.f)) + ","; }
            (ped.bInVehicle ? copsCar : copsFoot)++;
            // "engaged": acts against the player: pursuit / arrest / kill tasks
            const bool eng = PedHasTask(ped, TASK_COMPLEX_POLICE_PURSUIT) || PedHasTask(ped, TASK_COMPLEX_ARREST_PED) || PedHasTask(ped, TASK_SIMPLE_ARREST_PED) || PedHasTask(ped, TASK_COMPLEX_COP_ARREST_PED) ||
                             PedHasTask(ped, TASK_COMPLEX_KILL_PED_ON_FOOT) || PedHasTask(ped, TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED) || PedHasTask(ped, TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE) ||
                             PedHasTask(ped, TASK_SIMPLE_GANG_DRIVEBY) || PedHasTask(ped, TASK_SIMPLE_USE_GUN);
            if (eng) { copsEngaged++; if (!ped.bInVehicle) copsFootEngaged++; }
        }
        for (auto& veh : GetVehiclePool()->GetAllValid()) {
            const float d = (veh.GetPosition() - pos).Magnitude();
            if (d > 100.f || !veh.IsLawEnforcementVehicle()) continue;
            law++;
            if (veh.m_pDriver) lawDrv++;
            nearLaw = std::min(nearLaw, (int)d);
        }
        int pk = 0, wpk = 0, pnew = 0; // pnew: pickups that were not active at the previous snapshot (a dead armed ped on foot drops its weapons)
        static std::vector<bool> prevActive;
        std::vector<bool> nowActive(CPickups::aPickUps.size(), false);
        for (size_t i = 0; i < CPickups::aPickUps.size(); i++) {
            if (CPickups::aPickUps[i].m_nPickupType != PICKUP_NONE) { nowActive[i] = true; if (i >= prevActive.size() || !prevActive[i]) pnew++; }
        }
        prevActive = std::move(nowActive);
        for (auto& p : CPickups::GetAllActivePickups()) { pk++; if (p.m_nPickupType == PICKUP_ONCE || p.m_nPickupType == PICKUP_ONCE_TIMEOUT || p.m_nPickupType == PICKUP_ONCE_TIMEOUT_SLOW || p.m_nPickupType == PICKUP_ONCE_FOR_MISSION) wpk++; }
        GR_LOG("[gr] %s B peds60=%d cops=%d copsDead=%d copsDeadAlive=%d copsCar=%d copsFoot=%d copsEngaged=%d copsFootEngaged=%d nearCop=%d law100=%d lawDrv=%d nearLaw=%d pickups=%d pnew=%d pickupsOnce=%d area=%d cops40=%d copdz=%s copstk=%d",
            id.c_str(), peds, cops, copsDead, copsDeadAlive, copsCar, copsFoot, copsEngaged, copsFootEngaged, nearCop, law, lawDrv, nearLaw, pk, pnew, wpk, (int)CGame::currArea, copsNear40, copdz.empty() ? "-" : copdz.c_str(), copsStack);
    }
    if (pl && GetPedPool() && GetVehiclePool()) {
        // the last spawned car and its driver (spawnveh) + the last spawned cop (spawncop)
        const bool vOk = s_LastVeh && GetVehiclePool()->IsObjectValid(s_LastVeh);
        const bool dOk = s_LastDriver && GetPedPool()->IsObjectValid(s_LastDriver);
        const bool cOk = s_LastCop && GetPedPool()->IsObjectValid(s_LastCop);
        const auto dying = [](CPed* p) { return p->m_nPedState == PEDSTATE_DIE || p->m_nPedState == PEDSTATE_DEAD; };
        GR_LOG("[gr] %s C veh=%d vdriver=%d vmodel=%d drv=%d dhp=%d dst=%d ddead=%d dincar=%d dtasks=%s cop=%d chp=%d cst=%d cdead=%d ctasks=%s",
            id.c_str(), (int)vOk, vOk && s_LastVeh->m_pDriver ? 1 : 0, vOk ? (int)s_LastVeh->m_nModelIndex : -1, (int)dOk, dOk ? (int)s_LastDriver->m_fHealth : -1, dOk ? (int)s_LastDriver->m_nPedState : -1, dOk ? (int)dying(s_LastDriver) : -1,
            dOk ? (int)s_LastDriver->bInVehicle : -1, dOk ? Chain(*s_LastDriver).c_str() : "-", (int)cOk, cOk ? (int)s_LastCop->m_fHealth : -1, cOk ? (int)s_LastCop->m_nPedState : -1, cOk ? (int)dying(s_LastCop) : -1, cOk ? Chain(*s_LastCop).c_str() : "-");
    }
    {
        RECT cr{};
        HWND hw = (HWND)PSGLOBAL(window);
        if (hw) GetClientRect(hw, &cr);
        RwCamera* cam = Scene.m_pRwCamera;
        RwRaster* cr_ = cam ? RwCameraGetRaster(cam) : nullptr;
        RwRaster* fb  = CPostEffects::pRasterFrontBuffer;
        GR_LOG("[gr] %s D maxw=%d maxh=%d cam=%dx%d front=%dx%d client=%dx%d hour=%d min=%d",
            id.c_str(), (int)RsGlobal.maximumWidth, (int)RsGlobal.maximumHeight, cr_ ? (int)RwRasterGetWidth(cr_) : -1, cr_ ? (int)RwRasterGetHeight(cr_) : -1,
            fb ? (int)RwRasterGetWidth(fb) : -1, fb ? (int)RwRasterGetHeight(fb) : -1, (int)(cr.right - cr.left), (int)(cr.bottom - cr.top), (int)CClock::GetGameClockHours(), (int)CClock::GetGameClockMinutes());
    }
}
} // namespace

// per injector pump (every frame): accumulate the signed camera heading so "the camera keeps rotating in one direction past 360 degrees" can be measured
void NotsaGrTick() {
    if (auto* p = FindPlayerPed(); p && p->GetIntelligence()) {
        bool hit = false;
        (void)Chain(*p, &hit);
        if (hit && !s_HitPrev) s_HitEvents++;
        s_HitPrev = hit;
    }
    if (!FindPlayerPed()) { s_CamInit = false; return; }
    const float h = CamHeading();
    if (!s_CamInit) { s_CamInit = true; s_CamPrev = h; return; }
    float d = h - s_CamPrev;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    s_CamAccum += d;
    s_CamPrev = h;
}

// returns true if `name` is one of ours
bool NotsaGrCmd(const char* name, const std::string& args) {
    auto* pl = FindPlayerPed();
    if (!std::strcmp(name, "snap")) { Snap(args.empty() ? "snap" : args); return true; }
    if (!std::strcmp(name, "quit")) { GR_LOG("[gr] quit requested"); RsGlobal.quit = true; return true; } // leave the main loop (normal shutdown path), so a finished scenario does not idle until the wall-clock limit
    if (!pl) return std::strcmp(name, "spawncop") == 0 || std::strcmp(name, "spawnveh") == 0 || std::strcmp(name, "shootlast") == 0 || std::strcmp(name, "shootdrv") == 0 || std::strcmp(name, "hitplayer") == 0 || std::strcmp(name, "copattack") == 0 || std::strcmp(name, "time") == 0;
#ifndef NOTSA_PORTABLE // (portable has the same command in portable/play_cmd.cpp)
    if (!std::strcmp(name, "savemenu")) { FrontEndMenuManager.m_bIsSaveDone = true; pl->m_vecMoveSpeed = CVector{}; pl->m_vecTurnSpeed = CVector{}; GR_LOG("[gr-cmd] savemenu requested (m_bIsSaveDone = 1)"); return true; } // what ACTIVATE_SAVE_MENU does: the main loop then opens the save-game menu (FID-MAC W1 round trip)
#endif
    if (!std::strcmp(name, "money")) { FindPlayerInfo().m_nMoney = std::atoi(args.c_str()); GR_LOG("[gr-cmd] money %d", FindPlayerInfo().m_nMoney); return true; } // money:N  the player's money (may be negative or 0: CGameLogic fee tests)
    if (!std::strcmp(name, "spawncop")) { // spawncop[:MODEL[:DIST]]  an armed LAPD cop (ped type COP) DIST m in front of the player, standing
        int mdl = 280; float dist = 6.f;
        std::sscanf(args.c_str(), "%d:%f", &mdl, &dist);
        CStreaming::RequestModel((eModelID)mdl, STREAMING_GAME_REQUIRED); CStreaming::LoadAllRequestedModels(false);
        const auto p = pl->GetPosition() + pl->GetForward() * dist;
        s_LastCop = CPopulation::AddPed(PED_TYPE_COP, (eModelID)mdl, p, false);
        std::string wp;
        if (s_LastCop) for (int i = 0; i < 13; i++) { if (s_LastCop->m_aWeapons[i].m_Type != WEAPON_UNARMED) wp += std::to_string((int)s_LastCop->m_aWeapons[i].m_Type) + ":" + std::to_string(s_LastCop->m_aWeapons[i].m_TotalAmmo) + ","; }
        GR_LOG("[gr-cmd] spawncop %p model %d weapons [%s]", (void*)s_LastCop, mdl, wp.c_str());
        return true;
    }
    if (!std::strcmp(name, "spawnveh")) { // spawnveh:MODEL[:DRIVER=1[:DIST=8]]  a car (script-created) DIST m ahead, with the model's usual driver (cop in a police car) when DRIVER=1
        int mdl = 597, drv = 1; float dist = 8.f;
        std::sscanf(args.c_str(), "%d:%d:%f", &mdl, &drv, &dist);
        CStreaming::RequestModel((eModelID)mdl, STREAMING_GAME_REQUIRED);
        CStreaming::RequestModel(MODEL_LAPD1, STREAMING_GAME_REQUIRED); CStreaming::RequestModel(MODEL_MALE01, STREAMING_GAME_REQUIRED);
        CStreaming::LoadAllRequestedModels(false);
        auto p = pl->GetPosition() + pl->GetForward() * dist;
        bool found = false;
        const float gz = CWorld::FindGroundZFor3DCoord({ p.x, p.y, p.z + 2.f }, &found, nullptr);
        if (found) p.z = gz + 1.f;
        s_LastVeh = CCarCtrl::CreateCarForScript(mdl, p, false);
        s_LastDriver = nullptr;
        if (s_LastVeh && drv) {
            s_LastDriver = CPopulation::AddPedInCar(s_LastVeh, true, -1, 0, false, false);
            if (s_LastDriver) s_LastVeh->m_autoPilot.m_nCarMission = MISSION_CRUISE;
        }
        GR_LOG("[gr-cmd] spawnveh %p model %d driver %p at (%d,%d,%d)", (void*)s_LastVeh, mdl, (void*)s_LastDriver, (int)p.x, (int)p.y, (int)p.z);
        return true;
    }
    if (!std::strcmp(name, "shootlast") || !std::strcmp(name, "shootdrv")) { // lethal weapon damage event (the path bullets take) on the last spawned cop / the last spawned car's driver
        auto* target = !std::strcmp(name, "shootlast") ? s_LastCop : s_LastDriver;
        const bool ok = target && GetPedPool()->IsObjectValid(target) && CWeapon::GenerateDamageEvent(target, pl, WEAPON_AK47, 500, PED_PIECE_TORSO, 0);
        GR_LOG("[gr-cmd] %s ped %p -> GenerateDamageEvent=%d", name, (void*)target, (int)ok);
        return true;
    }
    if (!std::strcmp(name, "copattack")) { // copattack  the last spawned cop attacks the player (kill-ped-on-foot with his pistol, best accuracy): real bullets, real hit reactions
        if (s_LastCop && GetPedPool()->IsObjectValid(s_LastCop)) {
            s_LastCop->m_nWeaponAccuracy = 100;
            s_LastCop->SetCurrentWeapon(WEAPON_PISTOL); // (a fresh cop holds the nightstick: KillPedOnFoot would walk up to him and melee)
            s_LastCop->GetIntelligence()->GetTaskManager().SetTask(new CTaskComplexKillPedOnFoot(pl), TASK_PRIMARY_PRIMARY);
        }
        GR_LOG("[gr-cmd] copattack cop %p", (void*)s_LastCop);
        return true;
    }
    if (!std::strcmp(name, "hitplayer")) { // hitplayer[:WEAPON=22[:DAMAGE=10]]  a pistol-type damage event on the player from the last spawned cop (what a bullet that hits does)
        int w = WEAPON_PISTOL, dmg = 10;
        std::sscanf(args.c_str(), "%d:%d", &w, &dmg);
        CPed* by = s_LastCop && GetPedPool()->IsObjectValid(s_LastCop) ? s_LastCop : nullptr;
        const bool ok = CWeapon::GenerateDamageEvent(pl, by, (eWeaponType)w, dmg, PED_PIECE_TORSO, 0);
        GR_LOG("[gr-cmd] hitplayer weapon %d dmg %d by %p -> %d", w, dmg, (void*)by, (int)ok);
        return true;
    }
    if (!std::strcmp(name, "time")) { // time:H[:M]  set the game clock (the heat haze post effect starts at hour 10)
        int h = 12, m = 0;
        std::sscanf(args.c_str(), "%d:%d", &h, &m);
        CClock::SetGameClock((uint8)h, (uint8)m, 0);
        return true;
    }
    return false;
}
#endif
