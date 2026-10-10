/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "ExeRecip.h"

class CTimer {
public:
    /*!
    * Thanks guys for figuring this out for me!
    * 
    * So basically, a timestep is just fraction of the frametime (timestep = frametime / TIMESTEP_PER_SECOND)
    * this timestep is used basically everywhere to calculate physics, etc.
    */
    static constexpr float TIMESTEP_PER_SECOND = 50.f;                         //!< Number of steps/second
    static constexpr float TIMESTEP_LEN_IN_MS  = 1000.f / TIMESTEP_PER_SECOND; //!< How long (in ms) a timestep is

    typedef uint64(__cdecl* TimerFunction_t)();
    static inline NOTSA_GLOBAL(ms_fnTimerFunction, 0xB7CB28, (TimerFunction_t), {}); // Izzotop: Added in commit 50ce121c. That should be just timerDef.

    // class variables
    static inline NOTSA_GLOBAL(m_sbEnableTimeDebug, 0xB7CB40, (bool), {});
    static inline NOTSA_GLOBAL(bSkipProcessThisFrame, 0xB7CB89, (bool), {});
    static inline NOTSA_GLOBAL(bSlowMotionActive, 0xB7CB88, (bool), {});
    static inline NOTSA_GLOBAL(game_FPS, 0xB7CB50, (float), {});

    static inline NOTSA_GLOBAL(m_CodePause, 0xB7CB48, (bool), {});
    static inline NOTSA_GLOBAL(m_UserPause, 0xB7CB49, (bool), {});
    static inline NOTSA_GLOBAL(m_FrameCounter, 0xB7CB4C, (uint32), {});
    static inline NOTSA_GLOBAL(ms_fTimeStepNonClipped, 0xB7CB58, (float), {});
    static inline NOTSA_GLOBAL(ms_fTimeStep, 0xB7CB5C, (float), {});
    static inline NOTSA_GLOBAL(m_snTimerDivider, 0xB7CB2C, (uint32), {});

    static inline NOTSA_GLOBAL(ms_fOldTimeStep, 0xB7CB54, (float), {});
    static inline NOTSA_GLOBAL(ms_fSlowMotionScale, 0xB7CB60, (float), {});

    // game speed
    static inline NOTSA_GLOBAL(ms_fTimeScale, 0xB7CB64, (float), {});
    static inline NOTSA_GLOBAL(m_snTimeInMillisecondsPauseMode, 0xB7CB7C, (uint32), {});
    static inline NOTSA_GLOBAL(m_snTimeInMillisecondsNonClipped, 0xB7CB80, (uint32), {});
    static inline NOTSA_GLOBAL(m_snPreviousTimeInMillisecondsNonClipped, 0xB7CB68, (uint32), {});
    static inline NOTSA_GLOBAL(m_snTimeInMilliseconds, 0xB7CB84, (uint32), {});
    static inline NOTSA_GLOBAL(m_snRenderStartTime, 0xB7CB38, (uint64), {});
    static inline NOTSA_GLOBAL(m_snRenderPauseTime, 0xB7CB30, (uint64), {});
    static inline NOTSA_GLOBAL(m_snRenderTimerPauseCount, 0xB7CB44, (uint32), {});

    // Freshly from R*:
    static inline NOTSA_GLOBAL(m_snPPPPreviousTimeInMilliseconds, 0xB7CB6C, (uint32), {});
    static inline NOTSA_GLOBAL(m_snPPPreviousTimeInMilliseconds, 0xB7CB70, (uint32), {});
    static inline NOTSA_GLOBAL(m_snPPreviousTimeInMilliseconds, 0xB7CB74, (uint32), {});
    static inline NOTSA_GLOBAL(m_snPreviousTimeInMilliseconds, 0xB7CB78, (uint32), {});

public:
    static void InjectHooks();

    static void   Initialise();
    static void   Shutdown();
    static void   Suspend();
    static void   Resume();
    static void   Stop();
    static void   StartUserPause();
    static void   EndUserPause();
    static uint32 GetCyclesPerMillisecond();
    static uint32 GetCyclesPerFrame();
    static uint32 GetCurrentTimeInCycles();
    static bool   GetIsSlowMotionActive();
    static void   UpdateVariables(float timeElapsed);
    static void   Update();

    static float GetTimestepPerSecond() { return TIMESTEP_PER_SECOND; }

    // Inlined funcs
    // They could have used functions with a longer name, ex:
    // GetTimeInMillisecond, we have shorter GetTimeInMS
    static float  GetTimeScale() { return ms_fTimeScale; }
    static void   SetTimeScale(float ts) { ms_fTimeScale = ts; }
    static void   ResetTimeScale() { ms_fTimeScale = 1.0f; }

    static float  GetTimeStep() { return ms_fTimeStep; }
    static void   SetTimeStep(float ts) { ms_fTimeStep = ts; }
    static void   UpdateTimeStep(float ts) { ms_fTimeStep = std::max(ts, 0.00001f); }
    static float  GetTimeStepInSeconds() { return ms_fTimeStep * ExeRecip(TIMESTEP_PER_SECOND); } // exe: ms_fTimeStep * 0.02f (0x858B38; 132 inlined sites, no fdiv)
    static float  GetTimeStepInMS() { return GetTimeStepInSeconds() * 1000.0f; } // pattern: CTimer::ms_fTimeStep * 0.02f * 1000.0f

    static float  GetTimeStepNonClipped() { return ms_fTimeStepNonClipped; }
    static float  GetTimeStepNonClippedInSeconds() { return ms_fTimeStepNonClipped * ExeRecip(TIMESTEP_PER_SECOND); }
    static float  GetTimeStepNonClippedInMS() { return GetTimeStepNonClippedInSeconds() * 1000.0f; }
    static void   SetTimeStepNonClipped(float ts) { ms_fTimeStepNonClipped = ts; }

    static uint32 GetFrameCounter() { return m_FrameCounter; }
    static void   SetFrameCounter(uint32 fc) { m_FrameCounter = fc; }

    static uint32 GetTimeInMS() { return m_snTimeInMilliseconds; }
    static void   SetTimeInMS(uint32 t) { m_snTimeInMilliseconds = t; }

    static uint32 GetTimeInMSNonClipped() { return m_snTimeInMillisecondsNonClipped; }
    static void   SetTimeInMSNonClipped(uint32 t) { m_snTimeInMillisecondsNonClipped = t; }

    static uint32 GetTimeInMSPauseMode() { return m_snTimeInMillisecondsPauseMode; }
    static void   SetTimeInMSPauseMode(uint32 t) { m_snTimeInMillisecondsPauseMode = t; }

    static uint32 GetPreviousTimeInMS() { return m_snPreviousTimeInMilliseconds; }
    static void   SetPreviousTimeInMS(uint32 t) { m_snPreviousTimeInMilliseconds = t; }

    static bool GetIsPaused() { return m_UserPause || m_CodePause; }
    static bool GetIsUserPaused() { return m_UserPause; }
    static bool GetIsCodePaused() { return m_CodePause; }
    static void SetCodePause(bool pause) { m_CodePause = pause; }

    // NOTSA section

    static bool HasTimePointPassed(uint32 timeMs) { return GetTimeInMS() >= timeMs; }
    static bool IsTimeInRange(uint32 fromMs, uint32 toMs) { return HasTimePointPassed(fromMs) && !HasTimePointPassed(toMs); }
};

uint64 GetMillisecondTime();
bool EachFrames(auto count) { return (CTimer::GetFrameCounter() & count) == 0; }
