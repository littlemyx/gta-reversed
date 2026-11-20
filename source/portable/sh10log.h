#pragma once
// SH10 RR debug logs (STATEHASH builds only, no behaviour change: no rand draws, only pure reads). Every row: `frame randCount TAG ...`, frame = recorded frame.
//   NOTSA_SH10_STAGES=<file>  NOTSA_SH10_STAGES_FRAMES (3083-3095): S = one top-level stage of Idle / CGame::Process (name, rand before, rand after, draws), T = timing input of the CPopulation::Update test,
//                             P = flag passed to CPopulation::Update, G = updateTimeDelta after the population update (GenerateRandomCars test)
//   NOTSA_SH10_CAM=<file>     NOTSA_SH10_CAM_FRAMES (3085): camera dump after TheCamera.Process and at the end of the frame
//   NOTSA_SH10_CUT=<file>     NOTSA_SH10_CUT_FRAMES (3086-3100): CCutsceneMgr state + per-object model / anim time, after CCutsceneMgr::Update and at the end of the frame
#ifdef NOTSA_STATEHASH
#include "sh7log.h"
namespace notsa::rr {
inline FILE* Sh10StageLog() { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH10_STAGES", "NOTSA_SH10_STAGES_FRAMES", "3083-3095", "# frame randCount S name randBefore randAfter draws | T ... | P popFlag(arg of CPopulation::Update) | G"); return x; }(); return l.Get(); }
inline FILE* Sh10CamLog()   { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH10_CAM", "NOTSA_SH10_CAM_FRAMES", "3085", "# frame randCount CAM where ..."); return x; }(); return l.Get(); }
inline FILE* Sh10CutLog()   { static Sh7Log l = [] { Sh7Log x; x.Init("NOTSA_SH10_CUT", "NOTSA_SH10_CUT_FRAMES", "3086-3100", "# frame randCount C|O where ..."); return x; }(); return l.Get(); }

void Sh13PopPollMark(const char* where); // statehash.cpp (SH13 popcount log)
struct Sh10Stage { const char* name = nullptr; uint64_t before = 0; };
inline Sh10Stage& Sh10Cur() { static Sh10Stage s; return s; }
inline void Sh10End() {
    Sh10Stage& s = Sh10Cur();
    if (!s.name) return;
    Sh13PopPollMark(s.name);
    if (FILE* f = Sh10StageLog()) { const uint64_t a = RandCountNow(); std::fprintf(f, "%d %llu S %s %llu %llu %llu\n", LoadLogRecFrame(), (unsigned long long)a, s.name, (unsigned long long)s.before, (unsigned long long)a, (unsigned long long)(a - s.before)); std::fflush(f); }
    s.name = nullptr;
}
inline void Sh10Mark(const char* name) { Sh10End(); if (Sh10StageLog()) { Sh10Cur().name = name; Sh10Cur().before = RandCountNow(); } }
} // namespace notsa::rr

#define SH10_MARK(name) notsa::rr::Sh10Mark(name)
#define SH10_END() notsa::rr::Sh10End()
#define SH10_ROW(...) do { if (FILE* sh10f_ = notsa::rr::Sh10StageLog()) { std::fprintf(sh10f_, "%d %llu ", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow()); std::fprintf(sh10f_, __VA_ARGS__); std::fputc('\n', sh10f_); } } while (0)

namespace notsa::rr {
inline void Sh10CamDump(const char* where) {
    FILE* f = Sh10CamLog(); if (!f) return;
    const int fr = LoadLogRecFrame(); const unsigned long long rc = RandCountNow();
    auto& c = TheCamera;
    std::fprintf(f, "%d %llu CAM %s activeCam=%u whoInControl=%u modeToGoTo=%d typeOfSwitch=%d cutsceneFinished=%d transitionState=%d justSwitched=%d wideScreen=%d cinema=%d lookingAtPlayer=%d waitInterpol=%d targetEntity=%p fovDuringInter=%.9g gameCamPos=%.9g,%.9g,%.9g camPos=%.9g,%.9g,%.9g\n",
        fr, rc, where, (unsigned)c.m_nActiveCam, (unsigned)c.m_nWhoIsInControlOfTheCamera, (int)c.m_nModeToGoTo, (int)c.m_nTypeOfSwitch, (int)c.m_bCutsceneFinished, (int)c.m_bTransitionState, (int)c.m_bJust_Switched,
        (int)c.m_bWideScreenOn, (int)c.m_bCinemaCamera, (int)c.m_bLookingAtPlayer, (int)c.m_bWaitForInterpolToFinish, (void*)c.m_pTargetEntity, c.m_fFOVDuringInter, c.m_vecGameCamPos.x, c.m_vecGameCamPos.y, c.m_vecGameCamPos.z,
        c.GetPosition().x, c.GetPosition().y, c.GetPosition().z);
    for (int i = 0; i < 3; i++) {
        const CCam& m = c.m_aCams[i];
        std::fprintf(f, "%d %llu CAM %s cam[%d]%s mode=%d source=%.9g,%.9g,%.9g front=%.9g,%.9g,%.9g up=%.9g,%.9g,%.9g fov=%.9g lookingAtVector=%d\n", fr, rc, where, i, i == c.m_nActiveCam ? "*" : "", (int)m.m_nMode,
            m.m_vecSource.x, m.m_vecSource.y, m.m_vecSource.z, m.m_vecFront.x, m.m_vecFront.y, m.m_vecFront.z, m.m_vecUp.x, m.m_vecUp.y, m.m_vecUp.z, m.m_fFOV, (int)m.m_bCamLookingAtVector);
    }
    std::fflush(f);
}
inline void Sh10CutDump(const char* where) {
    FILE* f = Sh10CutLog(); if (!f) return;
    const int fr = LoadLogRecFrame(); const unsigned long long rc = RandCountNow();
    std::fprintf(f, "%d %llu C %s name=%.8s load=%d play=%d running=%d processing=%d skipped=%d timerS=%.9g numObjs=%d ms=%u prevMs=%u camFinishTime=%u camCutsceneFinished=%d\n", fr, rc, where, CCutsceneMgr::ms_cutsceneName,
        (int)CCutsceneMgr::ms_cutsceneLoadStatus, (int)CCutsceneMgr::ms_cutscenePlayStatus, (int)CCutsceneMgr::ms_running, (int)CCutsceneMgr::ms_cutsceneProcessing, (int)CCutsceneMgr::ms_wasCutsceneSkipped,
        CCutsceneMgr::ms_cutsceneTimerS, (int)CCutsceneMgr::ms_numCutsceneObjs, (unsigned)CTimer::m_snTimeInMilliseconds, (unsigned)CTimer::m_snPreviousTimeInMilliseconds, (unsigned)TheCamera.GetCutSceneFinishTime(), (int)TheCamera.m_bCutsceneFinished);
    for (int i = 0; i < (int)CCutsceneMgr::ms_numCutsceneObjs && i < 50; i++) {
        CCutsceneObject* o = CCutsceneMgr::ms_pCutsceneObjects[i];
        if (!o) { std::fprintf(f, "%d %llu O %s i=%d null\n", fr, rc, where, i); continue; }
        float at = -1.f; int nassoc = 0;
        if (RpClump* cl = o->GetRpClump()) { if (CAnimBlendAssociation* a = RpAnimBlendClumpGetFirstAssociation(cl)) { at = a->m_CurrentTime; nassoc = 1; } }
        std::fprintf(f, "%d %llu O %s i=%d model=%d loadName=%.31s animName=%.31s assoc=%d animTime=%.9g pos=%.9g,%.9g,%.9g\n", fr, rc, where, i, (int)o->m_nModelIndex, CCutsceneMgr::ms_cLoadObjectName[i], CCutsceneMgr::ms_cLoadAnimName[i], nassoc, at,
            o->GetPosition().x, o->GetPosition().y, o->GetPosition().z);
    }
    std::fflush(f);
}
} // namespace notsa::rr
#define SH10_CAM(where) notsa::rr::Sh10CamDump(where)
#define SH10_CUT(where) notsa::rr::Sh10CutDump(where)
#else
#define SH10_MARK(name) ((void)0)
#define SH10_END() ((void)0)
#define SH10_ROW(...) ((void)0)
#define SH10_CAM(where) ((void)0)
#define SH10_CUT(where) ((void)0)
#endif
