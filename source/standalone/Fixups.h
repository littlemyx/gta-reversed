#pragma once

// Standalone run build: process-level logging and diagnostics (standalone.log next to the exe). Historically also "hooks as fixups" (see .notes/P2A_DESIGN.md); since slice F3
// of .notes/DETACH_DATA_PLAN.md the run build has no original data image, no hook registry and no trap stubs: this header keeps only Log / Fatal / the diagnostics.
// It is intentionally dependency free: it is included from Base-level headers (PluginBase.h, RHManager.h).

#ifdef NOTSA_STANDALONE_RUN
#include <cstddef>
#include <cstdint>
#include <reversiblehooks/NoHookRegistry.h>

#ifdef NOTSA_VERIFY_GLOBALS
namespace notsa::globals { void DumpAtBoot(); } // source/standalone/GlobalsVerify.cpp
#endif

namespace notsa::standalone {
namespace Fixups {
//! Process diagnostics: atexit / purecall / invalid-parameter / terminate / abort / CRT-report hooks that log the cause (and a stack through the exception filter) before the
//! process ends, plus the env-driven sampler. Call once, first thing in the run WinMain.
void InstallDiagnostics();

//! Log (to `standalone.log` next to the exe, the debugger and stderr) and terminate the process
[[noreturn]] void Fatal(const char* fmt, ...);
//! Append a line to the log
void Log(const char* fmt, ...);
//! D7 self-check: log the x87 precision control (`_controlfp(0,0) & _MCW_PC`: 0x00000 = 24 bit, 0x10000 = 53 bit, 0x20000 = 64 bit), the rounding/exception
//! masks and the sticky `_statusfp()` flags, tagged with `where`. The original runs at PC=53 (CRT) until D3D9 CreateDevice lowers the thread to PC=24.
void LogFpuState(const char* where);
//! NOTSA_STANDALONE_MEMLOG line set (playerlog / clocklog / memlog): the part that reads game state. Defined in Fixups.cpp (game lib); the PCH-less rw_* unit tests link a no-op stub.
void LogGameStateTrace(int frame, unsigned long crtHeapBytes, unsigned long crtHeapBlocks, unsigned long committedKB);
//! A2 (.notes/DETACH_DATA_PLAN.md): boot dump of the detached globals for tools/standalone/verify_globals.py. WinMain uses it right before the game starts;
//! it is `((void)0)` unless built with NOTSA_VERIFY_GLOBALS (no code at all: the default build's object code is unchanged).
#ifdef NOTSA_VERIFY_GLOBALS
#define NOTSA_VERIFY_GLOBALS_BOOT() ::notsa::globals::DumpAtBoot()
#else
#define NOTSA_VERIFY_GLOBALS_BOOT() ((void)0)
#endif
} // namespace Fixups

} // namespace notsa::standalone
#endif
