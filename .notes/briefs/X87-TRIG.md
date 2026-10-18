# X87-TRIG — systematic check of asin/acos/atan call targets in x87-converted code

Rules: .notes/agent_rules/v2.md (read it first, then .notes/PLAYTEST_AGENT_ENV.md). Worktree: C:/gta-wt/x87-trig, branch from reverse/interior_c HEAD.

## Evidence
PT2-CORPSE (.notes/reports/PT2-CORPSE.md) found CTaskSimpleDead::ProcessPed (0x630600) calling 0x821E70 = _CIasin (unicorn: f(0.5)=0.5236),
while the port used acos. _CIacos = 0x822380. 689 sites were converted from x87 inline code; only a few acos sites were re-checked.

## Task (one function family: CRT trig intrinsics)
1. Find every exe call to the CRT trig entry points (_CIasin 0x821E70, _CIacos 0x822380, and atan/atan2/_CIatan2/_CIsin/_CIcos/_CItan/_CIsqrt
   entry points — identify their addresses first, prove each with a unicorn/oracle evaluation like PT2-CORPSE did).
2. Map each call site (exe function containing it) to the port function (hook registry / RH_ScopedInstall address) and check the port uses
   the same math function at that point. Script it (python, capstone over gta_sa_compact.exe); output a table site → exe fn → port file:line → OK/MISMATCH.
3. Fix every MISMATCH in the port (run build behaviour must equal the exe). Add an oracle row where the function is already oracle-testable.
## Acceptance
Table in .notes/reports/X87-TRIG.md with 0 unexplained rows; Release builds; run_all_tests.sh 44/44; 3-min soak OK. Commit script under tools/standalone/.
Report ≤15 lines (numbers in a table) as the hand-back; full report on disk.
