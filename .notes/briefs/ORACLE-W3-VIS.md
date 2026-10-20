# ORACLE-W3-VIS — VisibilityPlugins.cpp + app_light.cpp classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-vis.
## Evidence
Coverage JSON: VisibilityPlugins.cpp 39 uncovered A/B (LOD/fade distances, alpha lists, atomic/clump visibility callbacks,
draw-distance checks — what pops in and how objects fade), app_light.cpp 23 (scene lighting colours per time of day, directional/
ambient light setup — how the world is lit). Both are mostly maths over globals + RW calls. Lessons: ORACLE-W3-IK.md (RW maths exact
now), ORACLE-W3-FX.md (+"Blueprints": compare only the fixture block to stay fast), ORACLE-W3-UI.md / MODELS.md (stub-recorded RW calls).
## Task
Rows for the A+B uncovered functions of both files with real fixtures (camera at its exe address, atomics/clumps with model infos,
TimeCycle colour state for the lights); RW calls recorded by stubs on both sides; fix every mismatch (one commit per fix, asm address and
pre-fix failure count in the subject) or skip with reason. Target < 5 min. HARD budget under 250k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
