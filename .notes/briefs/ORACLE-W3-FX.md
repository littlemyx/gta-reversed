# ORACLE-W3-FX — game_sa/Fx (+ Fx/Blueprints) classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-fx.
## Evidence
.notes/ORACLE_COVERAGE.md: Fx 136 ported / 12 covered, uncovered A 40, B 4; Fx/Blueprints 74 / 5, A 26, B 4; FxSystem.cpp 16 and
FxManager.cpp 9 uncovered A/B per the coverage JSON. Particle systems: emission rates, key-frame interpolation of particle info,
bounding/culling, frustum tests — what smoke, sparks, blood, explosions look like. Lessons: ORACLE-W3-IK.md (RW maths now exact; real
fixtures), ORACLE-W3-EFFECTS.md, ORACLE-W3-UI.md (stub-recorded draw calls), ORACLE-W3-INPUT.md "hang" (keep the target < 5 min).
## Task
Rows for the A+B uncovered Fx and Fx/Blueprints functions with real fixtures (FxSystemBP with prims and info keys built through the
exe's own loader structures where possible, a FxSystem_c instance, FxManager at its exe address); fix every mismatch (one commit per fix,
asm address and pre-fix failure count in the subject) or skip with reason. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; target < 5 min; run the new target + targets linking the changed TUs; report ≤15 lines with start/end time.
