# ORACLE-W3-MODELS — exe-oracle coverage for game_sa/Models classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-models.
## Evidence
.notes/ORACLE_COVERAGE.md: Models 202 ported / 10 covered; uncovered A 67, B 28 (model infos: vehicle, ped, weapon, clump, atomic,
time/lod models — data every system reads). Template/lessons: .notes/reports/ORACLE-W3-ANIM.md (real fixtures), ORACLE-W3-TASKS.md
continuation 2 (link the TUs the port calls, byte masks, W2_FAULTLOG=1), ORACLE-W3-COLL.md.
## Task
Rows for the A+B uncovered Models functions with real model-info fixtures (CModelInfo arrays at the exe addresses where the function
reads them); fix every mismatch (one commit per fix, asm address in the subject, row fails pre-fix) or skip with reason.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green; report ≤15 lines with start/end time.
