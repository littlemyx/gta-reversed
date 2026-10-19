# ORACLE-W3-GAMEPLAY — exe-oracle coverage for gameplay root files, classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-gp; configure with
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/ORACLE_COVERAGE.md: game_sa root files 3458 ported / 855 covered; uncovered A 396, B 611 — too big for one agent.
This slice: the gameplay-rule files the player feels directly — Weapon*.cpp, WeaponInfo, Wanted, Pickup(s), Darkel, Gangs/GangWars,
PedGroup*, Population (non-streaming helpers), Garages/Garage, Stats, PlayerInfo, Clock/Weather helpers (pick by these names;
list the exact files and function counts in the report).
Template and lessons: .notes/reports/ORACLE-W3-VEH.md, W3-VEH-OPEN.md, ORACLE-W3-PED.md, W3-PED-OPEN.md (edge-value rows, CleanX87,
x87 register precision, fixture wiring, no `_exit` hacks).
## Task
New target wave3_gameplay_oracle_test (generator gen_w3_gp.py), rows for class A then B functions of the slice; fix every mismatch
(one commit per fix, asm address in the subject; each fix's row fails on the pre-fix code) or skip with a reason.
Stop at the cap with a clean partial landing + state file if the slice is bigger than one agent.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests 48/48; Release builds. Report ≤15 lines.
