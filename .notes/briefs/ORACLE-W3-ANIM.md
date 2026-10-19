# ORACLE-W3-ANIM — exe-oracle coverage for game_sa/Animation + Plugins/RpAnimBlendPlugin classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-anim; cmake flags
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/ORACLE_COVERAGE.md: Animation 124 ported / 19 covered, uncovered A 36, B 3; RpAnimBlendPlugin 39 / 1, uncovered A 5, B 18.
Playtest 1 had a broken bike-pedalling animation after a cutscene (U8, believed fixed, never proven by a row). Blend maths is partly
covered already (see existing rows). Lessons: .notes/reports/W3-VEH-OPEN.md (x87 precision), ORACLE-W3-COLL.md (fixtures that
actually hit), ORACLE-W3-TASKS.md continuation 2 (link the TUs the port calls; byte masks; W2_FAULTLOG=1).
## Task
Rows for the A+B uncovered functions; anim fixtures must be real (an anim hierarchy with sequences/keyframes, a blend association
list on a clump) so blend/update paths run — garbage never exercises them. Fix every mismatch (one commit per fix, asm address in the
subject, row fails pre-fix) or skip with reason.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green; report ≤15 lines.
