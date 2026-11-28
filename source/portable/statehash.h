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
//   SH1 (.notes/STATEHASH_SPEC.md): NOTSA_SH_FILE=<file> per-frame hash file; NOTSA_SH_DUMP=<f,f..> NOTSA_SH_DUMP_SUBSYS=<tm,..|all> NOTSA_SH_DUMP_FILE=<file> verbose dump; NOTSA_SH_RANDLOG=<file> rand() call log (all need NOTSA_RR_TRACE + cmd:rr_start for per-frame output)
// Injector commands (SDLWrapper.cpp InjRunCmd): cmd:rr_start[:SEED], cmd:rr_stop, cmd:rr_stat.
// Trace format: see RR_PLAN.md section 3 and tools/portable/rr_diff.py.
#pragma once
#ifdef NOTSA_STATEHASH
#include <cstdint>
#include <cstdio>
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
//! RenderWare allocations (rwMalloc) return zeroed memory from now on; call right after the RW engine was initialised (GameInit)
void PinHeap();
//! NOTSA_RR_NOAUDIO=1 (with NOTSA_RR=1): Idle() does not call AudioEngine.Service()
bool NoAudio();
bool AudioStubs();
//! true while a trace is being recorded
bool Recording();
//! RR-3 diagnostics: logs `rrm <name> rng=<state> idle=<n>` when NOTSA_RR_RNGMARKS=1 (bisecting where the RNG state of two builds first differs)
void RngMark(const char* name);
//! debug probe (NOTSA_RR_PROBE=A-B in Idle frames): logs a hash of all vehicles + the state of vehicle slot NOTSA_RR_PROBE_SLOT at a named point inside the frame (bisecting where two runs diverge)
void Probe(const char* tag);
//! (declared in msvc_rand.h) logs a draw of the game thread's rand() when NOTSA_RR_RANDLOG is set
void RandHook(uint32_t stateBefore);
//! (Win32 single-state build, declared in GameRand.h) same, with the source location of the rand() macro use
void RandHookLoc(uint32_t stateBefore, const char* file, unsigned line);
//! CGeneral::GetRandomNumber records its own return address here (the wrapper hides its caller); read by RandHookLoc
void SetWrapCaller(const void* ra);
//! (declared in Base.h for NOTSA_PTR_ID32) stable replacement of an object address: pool tag + slot index for pool objects, else the offset from this module's code
uint32_t StableId(const void* p);
//! SH5 NOTSA_SH_AUDIOLOG scope (RAII, see statehash.cpp): one row per audio event / sound request of the recorded frames NOTSA_SH_AUDIOLOG_FRAMES; no-op when the env var is unset or not recording
struct AudioLogScope {
    AudioLogScope(char kind, int event, const void* audioEntity, int bank = -1, int sound = -1, float x = 0.f);
    ~AudioLogScope();
    AudioLogScope(const AudioLogScope&) = delete;
    bool m_on = false; char m_kind = 0; int m_event = 0, m_bank = 0, m_sound = 0, m_seq = 0, m_depth = 0; const void* m_ae = nullptr; float m_x = 0.f; uint64_t m_rc0 = 0; uint64_t m_ra[4] = {};
};
//! SH4 NOTSA_SH_VEHLOG PossiblyRemoveVehicle rows (STATEHASH only; no behaviour change)
bool VehLogWants(const void* veh);
void VehLogPrv(const void* veh, const std::string& kv);
//! SH5 NOTSA_SH_WALKLOG=<file> (+ NOTSA_SH_WALKLOG_FRAMES, default 54): the FILE* when recording one of the frames, else null (CCarCtrl::GenerateCarCreationCoors2 rows)
std::FILE* WalkLogFile();
//! SH5 NOTSA_SH_BOOTLOG row ('F' per frame / 'S' at rr_start)
void BootLogRow(char kind);
//! BOOTLOG NOTSA_BOOTLOG_CAM: camera row of one boot frame (before rr_start)
void BootLogFrame();
//! BOOTLOG2 (NOTSA_BOOTLOG_CUT / NOTSA_BOOTLOG_CAM2): log only
void BootLogWorld();
void BootLogProc(const void* entity);
void BootLogStream(int modelId, int flags);
//! RAII scope for NOTSA_RR_GAMEPLAY_RAND=1 (.notes/STATEHASH_SPEC.md sections 6, 8): rand state restored at scope exit, calls inside are not counted / logged
struct RandIsolate { RandIsolate(); ~RandIsolate(); RandIsolate(const RandIsolate&) = delete; bool m_on = false; uint32_t m_saved = 0; };
} // namespace notsa::rr
#endif
