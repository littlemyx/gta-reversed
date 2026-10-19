# W3-VEH-OPEN — finish ORACLE-W3-VEH: 3 open rows + remaining A/B vehicle functions

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh w3-veh-open; configure with
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON (sccache -Zi breaks on the librw pdb).
## Evidence
.notes/reports/ORACLE-W3-VEH.md and ORACLE-W3-VEH-state.md (remaining list + generator commands, gen_w3_veh.py, w3_skip.tsv).
Rows that run only with W3_OPEN=1 and differ in 1-2 cases: CBike::DoSoftGroundResistance, CVehicle::CanVehicleBeDamaged,
CAutomobile::DoSoftGroundResistance. Remaining uncovered in Entity/Vehicle: A 5, B 12.
## Task
1. The 3 open rows: find the differing input, read the asm path for it, fix the port or prove a fixture artefact (W2-OPEN found one:
   x87 stack residue — CleanX87 per case). Un-skip.
2. The remaining 17 A/B functions: rows or skip-with-reason.
## Acceptance
3 open rows FIXED or EXPLAINED; coverage number before/after; wave3_vehicle_oracle_test 0 mismatches; run_all_tests 46/46; Release builds. Report ≤15 lines.
