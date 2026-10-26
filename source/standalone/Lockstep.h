#pragma once
// REPLAY-DIFF probe (.notes/REPLAY_DIFF_PLAN.md): per-frame state trace + determinism pins of the run build, the counterpart of
// tools/standalone/replay_diff/rd_orig.c (same trace format, same pin bits). Inert unless NOTSA_LOCKSTEP_OUT is set.
//   NOTSA_LOCKSTEP_OUT=<file>  trace file     NOTSA_LOCKSTEP_PIN=<bits> 1 time-derived srand seeds -> constant, 2 CTimer clock = anchor + frame*STEP ms, 4 psTimer/OS_TimeMS = 1000 + frame*STEP
//   pin 8: RNG re-seeded at the arm point (first frame with a player ped)   pin 16: frame counter jumps forward to the next multiple of 0x100000 at the arm point (clock anchor shifted, continuous)
//   pin 32 + NOTSA_LOCKSTEP_RLOG=<n>: log every rand() call ("R fc file:line") of the first n traced frames (noise hunting)
//   pin 64: "D" probe line  128: audio rand() calls draw from a second RNG stream  256: CdStream tick barrier  512: audio off (CAudioEngine::Service() returns; the RR harness NOTSA_RR_NOAUDIO)
//   NOTSA_LOCKSTEP_DUMPSCR=<prefix>: raw script space of traced frames 1..4 to <prefix>.<n> (rd_orig.c: RD_DUMPSCR)
//   NOTSA_LOCKSTEP_STEP=<ms, 16>   NOTSA_LOCKSTEP_MAXF=<frames, 20000>
#ifdef NOTSA_STANDALONE_RUN
#include <cstdint>
#ifdef NOTSA_PORTABLE
// Portable builds: the REPLAY-DIFF harness is a Win32-exe-vs-port tool (Lockstep.cpp is Win32-only, GameRand.h has no hooks there); inert stubs so the shared call sites compile.
namespace notsa::lockstep {
inline void     FrameRead() {}
inline void     Tick() {}
inline bool     NoAudio() { return false; }
inline void     Note(const char*) {}
inline bool     PinTimer() { return false; }
inline uint64_t TimerFn() { return 0; }
inline bool     PsTimer(uint32_t&) { return false; }
} // namespace notsa::lockstep
#else
namespace notsa::lockstep {
void     FrameRead();                                   // the next timer-function call is a "frame read" (CTimer::Initialise)
void     Tick();                                        // one trace line; called from CTimer::Update right before it reads the timer function
bool     NoAudio();                                     // pin bit 512: CAudioEngine::Service() does nothing (the RR harness NOTSA_RR_NOAUDIO; rd_orig.c: ret at 0x507750)
void     Note(const char* what);                        // "X fc what" trace line (ignored by rd_diff.py; render-skip diagnostics of Idle())
bool     PinTimer();                                    // pin bit 2: CTimer::Initialise installs TimerFn
uint64_t TimerFn();                                     // pinned CTimer clock
bool     PsTimer(uint32_t& out);                        // pin bit 4: psTimer / OS_TimeMS value once the frame counter is > 0
uint32_t* OnRand(const char* file, unsigned line);       // GameRand.h g_RandHook (pin bit 32 + NOTSA_LOCKSTEP_RLOG=<frames>): "R fc file:line" per rand() call
void     OnSrand(unsigned& seed, const char* file, unsigned line); // GameRand.h g_SrandHook (installed only with NOTSA_LOCKSTEP_OUT): trace line + pin bit 1 (time-derived seeds of app_game.cpp / LoadingScreen.cpp)
} // namespace notsa::lockstep
#endif // NOTSA_PORTABLE
#endif
