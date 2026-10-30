# MISSIONS-CRASH

Symptom: `gameplay_regress.sh missions` crashed in `strcpy_s` in `CPedAttractor::CPedAttractor` (PedAttractor.cpp:83), reached via
`CPopulation::GeneratePedsAtAttractors -> AddPedAtAttractor -> CEventHandler::HandleEvents -> CTaskComplexUseEffect::CreateFirstSubTask -> RegisterPedWithAttractor -> RegisterPed<CPedLookAtAttractor>`.

Root cause: the CPedAttractor ctor (exe 0x5EDFB0, call at 0x5EE15A to 0x821BB5 = sprintf(this+0x84, fx+0x38)) copies the effect's 8-byte
`m_szScriptName` into the 8-byte `m_ScriptName` with an UNBOUNDED copy. The port used `strcpy_s(m_ScriptName, fx->m_szScriptName)`: the destination size is 8,
so any script name that fills all 8 bytes of the (non-terminated, FileLoader copies it with a plain strcpy that spills its NUL into the next byte) source
is rejected by the CRT invalid-parameter handler, which the standalone build turns into a fatal exception (Fixups::OnInvalidParam). Latent since the ctor was ported.

Why it appeared now / why intermittent: 3a2700d83 (ASM-S67) is CORRECT per the exe (event gets the `entity` argument + TASK_COMPLEX_USE_EFFECT). Before it the
event held the new ped as entity, so AffectsPed/IsEffectActive filtered the event out and CTaskComplexUseEffect never reached RegisterPed; now the real attractor
path runs, so the ctor sees every nearby attractor's script name. Which attractor is picked depends on the player position / RNG, hence runs of the
same build pass or crash (missions passed on tip 3644d26d8 in this worktree, crashed in the ASM-S93 runs). It is not a revert candidate.

Fix: `strncpy(m_ScriptName, fx->m_szScriptName, sizeof(m_ScriptName))` - copies the 8 bytes like the exe (the exe's 9th byte NUL lands in the next
pool slot's header; the port does not write outside the member).

Tests: gameplay_regress combat+missions PASS=25 (combat 14, missions 11); wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe) 1142 functions, 0 mismatches.
Note: missions also passed on the unfixed tip 3644d26d8 (flaky: depends on which attractor the population picks), so the fix is verified by analysis of the exe + no regression, not by a deterministic repro.
