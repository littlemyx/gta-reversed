#pragma once
// REPLAY-DIFF probe (.notes/REPLAY_DIFF_PLAN.md): per-frame state trace + determinism pins of the run build, the counterpart of
// tools/standalone/replay_diff/rd_orig.c (same trace format, same pin bits). Inert unless NOTSA_LOCKSTEP_OUT is set.
//   NOTSA_LOCKSTEP_OUT=<file>  trace file     NOTSA_LOCKSTEP_PIN=<bits> 1 time-derived srand seeds -> constant, 2 CTimer clock = anchor + frame*STEP ms, 4 psTimer/OS_TimeMS = 1000 + frame*STEP
//   NOTSA_LOCKSTEP_STEP=<ms, 16>   NOTSA_LOCKSTEP_MAXF=<frames, 20000>
#ifdef NOTSA_STANDALONE_RUN
#include <cstdint>
namespace notsa::lockstep {
void     FrameRead();                                   // the next timer-function call is a "frame read" (CTimer::Initialise)
void     Tick();                                        // one trace line; called from CTimer::Update right before it reads the timer function
bool     PinTimer();                                    // pin bit 2: CTimer::Initialise installs TimerFn
uint64_t TimerFn();                                     // pinned CTimer clock
bool     PsTimer(uint32_t& out);                        // pin bit 4: psTimer / OS_TimeMS value once the frame counter is > 0
void     OnSrand(unsigned& seed, const char* file);     // trace line + pin bit 1 (time-derived seeds of app_game.cpp / LoadingScreen.cpp)
} // namespace notsa::lockstep
#endif
