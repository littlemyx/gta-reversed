# ORACLE-W3-PATHS — route finding and the rest of World/WaterLevel/Shadows maths

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-paths.
## Evidence
.notes/reports/ORACLE-W3-WORLD.md: not started — CPathFind::DoPathSearch, ComputeRoute, FindNextNodeWandering (route finding for
every car and ped that drives/walks to a target), the World sector-list template functions (~35), WaterLevel (B 8), Shadows maths (A3/B6).
ORACLE-W3-WORLD built a 72-area node fixture with real links (tests/standalone/wave3_oracle_world*.inc) — reuse it.
## Task
Route-finding rows first (start/target nodes inside the fixture graph, so real routes come out; state the share of cases that find a
route), then WaterLevel, Shadows maths, then the sector templates. Fix every mismatch (one commit per fix, asm address and pre-fix
failure count in the subject) or skip with reason. Partial landing + state file at the cap is fine. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new rows' target + targets linking the changed TUs; report ≤15 lines with start/end time.
