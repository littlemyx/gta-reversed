# ORACLE-W3-INPUT — ControllerConfigManager + Pad, classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-input.
## Evidence
Coverage JSON (tools/standalone/oracle_coverage.py --json): ControllerConfigManager.cpp has 47 uncovered A/B functions (the most of any
file), Pad.cpp 18. Input mapping and pad state decide every control the player presses; playtest 1 had an Enter/extenter swap (U11)
in exactly this area. Lessons: ORACLE-W3-UI.md, ORACLE-W3-SCRIPTS.md, ORACLE-W3-AUDIO.md ("open rows": CRT initialisers).
## Task
Rows for the A+B uncovered functions of ControllerConfigManager.cpp and Pad.cpp with real fixtures (CControllerConfigManager at its exe
address with a populated action/key table; CPad pads with NewState/OldState); fix every mismatch (one commit per fix, asm address and
pre-fix failure count in the subject) or skip with reason. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs; report ≤15 lines with start/end time.
