# ORACLE-W3-COLL — exe-oracle coverage for game_sa/Collision (+ Core) classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-coll; cmake flags
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/ORACLE_COVERAGE.md: Collision 127 ported / 25 covered, uncovered A 31, B 18; Core 91 / 28, uncovered A 49, B 4. Collision
maths decides every physics contact (bit-exact matters most here: x87 precision lessons in .notes/reports/W3-VEH-OPEN.md).
## Task
Rows for Collision A+B, then Core A+B; new target wave3_coll_oracle_test (generator gen_w3_coll.py); geometry fixtures must produce
real hits (game-range spheres/boxes/triangles/lines that intersect) plus edge values — random garbage never hits. Fix every mismatch
(one commit per fix, asm address in the subject, row fails pre-fix) or skip with reason.
## Acceptance
Coverage before/after; 0 PC24 mismatches; hit-rate per row stated (share of cases where a collision is actually found); run_all_tests green.
Report ≤15 lines.
