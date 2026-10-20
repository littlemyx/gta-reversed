# ORACLE-W3-EFFECTS — explosions, fire, projectiles and world-effect root files, classes A+B (slice 4 of game_sa root)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-fxr.
## Evidence
game_sa root files: ~288 A / ~394 B uncovered after ORACLE-W3-UI. This slice: Explosion, Fire / FireManager, ProjectileInfo, Glass,
Rope / Ropes, Cranes, Bridge, Birds, Skidmarks, Rubbish, WaterCannon(s), Coronas maths, Clouds maths, PlantMgr maths (pick by these
names; list exact files and counts). Combat consequences the player sees (explosion damage radius, fire spread, rocket/grenade flight).
Lessons: ORACLE-W3-UI.md (stub-recorded draw calls), ORACLE-W3-PATHS.md, ORACLE-W3-WORLD.md, ORACLE-W3-COLL.md (x87).
## Task
Class A first, then B; real fixtures (fire/explosion/projectile arrays at the exe addresses, entities with matrices); fix every mismatch
(one commit per fix, asm address and pre-fix failure count in the subject) or skip with reason. Partial landing + state file at the cap is
fine. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs; report ≤15 lines with start/end time.
