# EXPL-REG: gameplay_oracle_test CExplosion::AddExplosion "PC24 1146 / 1084" - NOT REPRODUCIBLE
- Meaning of "PC24 a / b": summary line `mismatches (strict / excluding NaN-payload-only): PC24 a / b`; run_all_tests.sh parses b (x[2]) as the mismatch count, so it reads the test the same way the agent did.
- StandaloneRW (Debug, cl x86, run_all_tests config), gta_sa_compact.exe oracle, fresh worktree builds, full gameplay_oracle_test (313 functions) each time:
  | tree | AddExplosion PC24 | total PC24 |
  | origin e9a26dc2 | 0 / 0 | 57 / 0 (57 = NaN-payload-only rows: GetCarRoll/Pitch, ResetSuspension, ProgressDoorDamage, Find3rdPersonCamTargetVector) |
  | local 145493f7 (portable merge, has f18db775 NOTSA_FTOL in Explosion.cpp) | 0 / 0 | 57 / 0 |
  | 145493f7 + e9a26dc2 merged (fafe6e2e, local only) | 0 / 0 | 57 / 0 |
  | orchestrator's own copy of build/StandaloneRW/source/gameplay_oracle_test.exe (14:25) | 0 / 0 | - |
  | gate logs C:/gta-scratch/run_all_tests.1233 (14:18) and .10472 (15:23) | 0 / 0 | 57 / 0 |
- No bisect possible: no bad commit exists in e9a26dc2, 145493f7 or their merge. Suspect commits checked by reading: dbdcbc42 (x87d in Explosion.cpp = double on non-portable, tokens unchanged), f18db775 (NOTSA_FTOL = `(T)(...)` on Windows) - both oracle-clean.
- Conclusion: the 1146 / 1084 came from the previous agent's own build (stale objects / different flags / uncommitted state), not from the sources. No code or test change made.
