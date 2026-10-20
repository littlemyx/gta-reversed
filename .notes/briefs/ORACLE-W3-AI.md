# ORACLE-W3-AI — CarCtrl, PedIntelligence, Cover, Object classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-ai.
## Evidence
Coverage JSON: CarCtrl.cpp 23 uncovered A/B (traffic AI: steering to nodes, speed, overtaking, joining paths), PedIntelligence.cpp 17
(event scanning, task lookup), Cover.cpp 11 (cover points for cops/gangs in fights), Entity/Object/Object.cpp 16. Lessons:
ORACLE-W3-INPUT.md, ORACLE-W3-PATHS.md (72-area linked-node fixture — reuse for CarCtrl), ORACLE-W3-TASKS.md continuation 2, ORACLE-W3-EVENTS.md.
## Task
Rows for these A+B functions with real fixtures (cars with autopilot and path nodes, peds with intelligence/task manager/event group,
objects with model info); fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with
reason. Partial landing + state file at the cap is fine. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs; report ≤15 lines with start/end time.
