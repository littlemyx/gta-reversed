# ORACLE-W3-WORLD — exe-oracle coverage for world/physics root files, classes A+B (slice 2 of game_sa root)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-world.
## Evidence
.notes/ORACLE_COVERAGE.md: game_sa root files still have ~346 A / ~540 B uncovered after ORACLE-W3-GAMEPLAY. This slice: World.cpp,
PathFind.cpp, Zone*/TheZones, CullZones, General.cpp, Timer/Clock helpers not yet covered, Placeable/Matrix helpers in root, Shadows maths
helpers, WaterLevel, Weather (pick by these names; list exact files and counts in the report). The SetupPaths exterior-node branch left
by ORACLE-W3-INTERIOR (needs real ThePaths nodes with links) belongs here.
Lessons: ORACLE-W3-SCRIPTS.md, ORACLE-W3-CAMERA.md, ORACLE-W3-INTERIOR.md, ORACLE-W3-COLL.md, ORACLE-W3-AUDIO.md ("open rows").
## Task
Class A first, then B; real fixtures (path nodes with links, zones, sectors with entity lists); fix every mismatch (one commit per fix,
asm address and pre-fix failure count in the subject) or skip with reason. Partial landing + state file at the cap is fine. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs (the orchestrator runs the full gate);
report ≤15 lines with start/end time.
