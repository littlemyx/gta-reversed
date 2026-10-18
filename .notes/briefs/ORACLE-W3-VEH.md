# ORACLE-W3-VEH — exe-oracle coverage for game_sa/Entity/Vehicle classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-veh.
## Evidence
.notes/ORACLE_COVERAGE.md: Entity/Vehicle 488 ported, 123 covered; uncovered A (pure) 44, B (exe globals only) 58. Vehicles are the
most gameplay-visible area with the least coverage. ORACLE-W2 (.notes/reports/ORACLE-W2.md) built the infrastructure: oracle_alloc.h
(deterministic arena), generators gen_w2_rows.py / gen_w2_funcs.py, CleanX87() per case (W2-OPEN).
## Task (one subsystem, classes A and B only)
1. List the A+B uncovered functions in Entity/Vehicle (oracle_coverage.py output). New target wave3_vehicle_oracle_test (or extend an
   existing vehicle test), generated rows where possible; B functions snapshot/restore the exe globals they touch.
2. Every mismatch: fix the port to the exe (one commit per fix, asm address in the subject), or skip-with-reason if a fixture artefact.
3. Mutation check on ≥5 rows. Regenerate .notes/ORACLE_COVERAGE.md.
Do not attempt classes C/S/D. If A+B can't be finished within the cap, land what is done and hand back with a state file.
## Acceptance
Coverage number before/after; 0 mismatches (or each skip explained); run_all_tests 46/46 (new target included); Release builds. Report ≤15 lines.
