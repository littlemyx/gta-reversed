// SCMTRACE-1: reference SCM execution trace of one mission thread tree (the RYDER_1 mission thread and the threads it starts), for the TS port's lockstep comparison.
// Exists ONLY with -DGTASA_SCMTRACE=ON (needs -DGTASA_STATEHASH=ON too: virtual clock, sync streaming, fixed RNG). Without it the hook lines are preprocessed away and no TU of this module is compiled.
// Env: NOTSA_SCMTRACE=<file> (trace is written only when set), NOTSA_SCMTRACE_COMMIT=<sha> (header text), NOTSA_RR_STEP_MS (header text).
// Format: see the header comment block written to the file (`# columns`) and the README next to the recorded traces.
#pragma once
#ifdef NOTSA_SCMTRACE
#include <cstdint>
class CRunningScript;

namespace notsa::scmtrace {
bool Enabled();
//! the NEXT CTheScripts::StartNewScript is the root of the traced tree (mission `missionId` of main.scm, started by the input injector `mission:N`)
void ArmRoot(int32_t missionId);
//! a script was started (CTheScripts::StartNewScript); traced when armed or when started by a traced thread
void OnStart(CRunningScript* s);
//! a script ends (CRunningScript::ShutdownThisScript)
void OnEnd(CRunningScript* s);
//! the executing command evaluated a condition: `state` is the value after the NOT bit (CRunningScript::UpdateCompareFlag)
void OnCond(bool state);
bool Traced(const CRunningScript* s);
//! the root (mission) thread while it is alive, else nullptr
CRunningScript* Root();
void Begin(CRunningScript* s);
void End(CRunningScript* s);
//! runs one command `f()` of script `s` (a CRunningScript::ProcessOneCommand call) and logs it when `s` is in the traced tree
template<class F> auto Run(CRunningScript* s, F&& f) {
    if (!Enabled() || !Traced(s)) { return f(); }
    Begin(s);
    auto r = f();
    End(s);
    return r;
}
} // namespace notsa::scmtrace
#endif
