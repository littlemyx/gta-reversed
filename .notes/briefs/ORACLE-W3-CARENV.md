# ORACLE-W3-CARENV — CustomCarEnvMapPipeline.cpp classes A+B (car paint reflections/specular)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-carenv.
## Evidence
Coverage JSON: Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp 24 uncovered A/B — env-map/specular material plugin data,
the per-material reflection coefficients, env-map frame matrix maths, shininess/spec tables: how car paint shines. Lessons:
ORACLE-W3-VIS.md (RW calls as recording stubs, light colours), ORACLE-W3-MODELS.md (SetEnvMapCoeffCB unsigned bug), ORACLE-W3-IK.md (RW
maths exact), ORACLE-W3-FX.md "Blueprints" (compare only the fixture block).
## Task
Rows for the A+B uncovered functions with real fixtures (RpMaterial with the plugin extension at the exe's plugin offset, env-map
atomic/frame, the pipeline's global tables at their exe addresses); fix every mismatch (one commit per fix, asm address and pre-fix failure
count in the subject) or skip with reason. Target < 5 min. HARD budget under 250k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
