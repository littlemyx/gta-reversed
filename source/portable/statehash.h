// RR-1 (.notes/RR_PLAN.md): deterministic record/replay state-hash harness. Everything here exists ONLY when NOTSA_STATEHASH is defined (cmake -DGTASA_STATEHASH=ON);
// without it the hook lines in the game sources are preprocessed away and no TU of this module is compiled, so the reference (Win32) build stays bit-identical.
//
// Environment (read once, at first use):
//   NOTSA_RR=1                 pin the run: virtual clock (CTimer & friends advance by a fixed step per Idle/FrontendIdle), synchronous CdStream reads, fixed RNG seed,
//                              the input injector's script clock = virtual clock (wait:N = N virtual ms = N/step frames)
//   NOTSA_RR_STEP_MS=20        virtual milliseconds per game frame (>= 14: Idle() spins until 14 ms of CTimer time have passed)
//   NOTSA_RR_SEED=12345        value of srand() at process start (GameInit / loading screen) and the default seed of `cmd:rr_start`
//   NOTSA_RR_TRACE=<file>      trace file, written from `cmd:rr_start[:SEED]` until `cmd:rr_stop` / NOTSA_RR_MAXFRAMES / exit
//   NOTSA_RR_MAXFRAMES=N       stop recording (and, with NOTSA_RR_EXIT=1, exit the game) after N recorded frames
//   NOTSA_RR_DETAIL=A-B        recorded frames A..B (inclusive) also get one `D` line per entity with every hashed field (bit patterns)
//   NOTSA_RR_TAG=<text>        free text stored in the trace header (build name)
// Injector commands (SDLWrapper.cpp InjRunCmd): cmd:rr_start[:SEED], cmd:rr_stop, cmd:rr_stat.
// Trace format: see RR_PLAN.md section 3 and tools/portable/rr_diff.py.
#pragma once
#ifdef NOTSA_STATEHASH
#include <cstdint>
#include <string>

namespace notsa::rr {
//! NOTSA_RR=1 (virtual clock, sync streaming, fixed seed)
bool Active();
//! seed from NOTSA_RR_SEED (default 12345)
uint32_t Seed();
//! virtual clock in ns (Active() only)
uint64_t VirtualNs();
//! virtual clock in ms since process start (the injector's script clock)
uint64_t VirtualMs();
//! advance the virtual clock by one frame; called at the top of Idle() and FrontendIdle()
void AdvanceClock();
//! called once per game frame at the end of the simulation part of Idle() (after CGame::Process and AudioEngine.Service)
void OnFrame();
//! injector commands; true when `name` was one of ours
bool Cmd(const char* name, const std::string& args);
//! true while a trace is being recorded
bool Recording();
//! debug probe (NOTSA_RR_PROBE=A-B in Idle frames): logs a hash of all vehicles + the state of vehicle slot NOTSA_RR_PROBE_SLOT at a named point inside the frame (bisecting where two runs diverge)
void Probe(const char* tag);
//! (declared in msvc_rand.h) logs a draw of the game thread's rand() when NOTSA_RR_RANDLOG is set
void RandHook(uint32_t stateBefore);
//! (declared in Base.h for NOTSA_PTR_ID32) stable replacement of an object address: pool tag + slot index for pool objects, else the offset from this module's code
uint32_t StableId(const void* p);
} // namespace notsa::rr
#endif
