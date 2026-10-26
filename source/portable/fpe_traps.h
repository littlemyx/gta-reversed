// MAC-FPTRAP: optional floating-point trap audit of the native macOS build (CMake option GTASA_FPE_TRAPS, OFF by default, portable only).
// With the option ON the main thread and the game / audio threads run with FPCR.IOE / DZE (/ OFE) set: an invalid operation (0/0, inf-inf, sqrt(-1),
// float->int of NaN / out of range), a division by zero (or an overflow) raises SIGILL on Apple silicon (ESR EC 0x2C). The handler logs the PC,
// the exception kind and a frame-pointer backtrace once per distinct PC, counts every hit, and either aborts (NOTSA_FPE_MODE=abort) or lets the
// instruction complete with the trap masked and re-arms the thread a couple of milliseconds later (NOTSA_FPE_MODE=log, default).
// Env: NOTSA_FPE_MODE=log|abort, NOTSA_FPE_EXC=<subset of "izo"> (default "iz"), NOTSA_FPE_LOG=<file> (default stderr),
//      NOTSA_FPE_SUMMARY=<file> (hit table, rewritten every 5 s and at exit), NOTSA_FPE_NOWHITELIST=1, NOTSA_FPE_BT=<frames> (default 10).
#pragma once

#if defined(NOTSA_FPE_TRAPS) && defined(NOTSA_PORTABLE) && defined(__APPLE__) && defined(__aarch64__)
namespace notsa::fpe {
void Init();                          // main(): install the handlers, arm the calling thread, start the re-arm helper
void InstallHandlers();               // idempotent; call again after anything else replaced SIGILL / SIGFPE (InstallCrashHandler)
void ArmCurrentThread(const char* name); // game / audio worker threads call this first thing
void DisarmCurrentThread();           // before the thread function returns
} // namespace notsa::fpe
#define NOTSA_FPE_INIT()          ::notsa::fpe::Init()
#define NOTSA_FPE_INSTALL()       ::notsa::fpe::InstallHandlers()
#define NOTSA_FPE_ARM_THREAD(n)   ::notsa::fpe::ArmCurrentThread(n)
#define NOTSA_FPE_DISARM_THREAD() ::notsa::fpe::DisarmCurrentThread()
#else
#define NOTSA_FPE_INIT()          ((void)0)
#define NOTSA_FPE_INSTALL()       ((void)0)
#define NOTSA_FPE_ARM_THREAD(n)   ((void)0)
#define NOTSA_FPE_DISARM_THREAD() ((void)0)
#endif
