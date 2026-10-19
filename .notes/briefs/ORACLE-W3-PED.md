# ORACLE-W3-PED — exe-oracle coverage for game_sa/Entity/Ped classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-ped; configure with
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/ORACLE_COVERAGE.md: Entity/Ped 232 ported, 30 covered; uncovered A (pure) 33, B (exe globals only) 44.
Template: the vehicle wave — .notes/reports/ORACLE-W3-VEH.md, W3-VEH-OPEN.md (generator gen_w3_veh.py, wave3_vehicle_oracle_test,
w3_skip.tsv). Lessons: rows must mix game-range values with edge values (NaN, inf, 3e38, denormals) — garbage floats hide bugs;
x87 residue → CleanX87 per case; the exe keeps intermediates on the x87 stack (port: double locals for register values, float for spills).
## Task (one subsystem, A+B only)
New target wave3_ped_oracle_test (generator gen_w3_ped.py), rows for the A+B uncovered Entity/Ped functions; every mismatch fixed
(one commit per fix, asm address in the subject) or skipped with a reason; each fix's row must fail on the pre-fix code (mutant).
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests 47/47; Release builds. Report ≤15 lines.
