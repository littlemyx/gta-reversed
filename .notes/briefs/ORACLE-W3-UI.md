# ORACLE-W3-UI — exe-oracle coverage for the HUD/radar/text root files, classes A+B (slice 3 of game_sa root)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-ui.
## Evidence
game_sa root files: ~310 A / ~436 B uncovered after ORACLE-W3-PATHS. This slice is what the player reads on screen: Radar.cpp
(minimap transforms, blips, zoom), Hud.cpp, Messages.cpp (help/brief/big messages, number insertion), Text.cpp / TextLoader (GXT
lookup, string helpers), Font.cpp (string width/wrap maths), Sprite2d maths, MenuManager helpers (settings values, not drawing).
RenderWare/draw calls are recorded by stubs on both sides (ORACLE-W3-MODELS / AUDIO technique). List exact files and counts.
Lessons: ORACLE-W3-PATHS.md, ORACLE-W3-SCRIPTS.md, ORACLE-W3-CAMERA.md, ORACLE-W3-AUDIO.md ("open rows").
## Task
Class A first, then B; real fixtures (a loaded GXT key table, blip array, font data at the exe addresses); fix every mismatch (one commit
per fix, asm address and pre-fix failure count in the subject) or skip with reason. Partial landing + state file at the cap is fine.
Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs; report ≤15 lines with start/end time.
