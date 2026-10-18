# SWIM-DUCK — four open angle/trig deviations found by HEADING-AUDIT's review pass (unverified)

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh swim-duck.
## Evidence (.notes/reports/HEADING-AUDIT.md, OPEN section — found by a read-only pass, NOT re-verified)
- CTaskSimplePlayerOnFoot::PlayerControlDucked 0x687F30: sin/cos in swapped slots
- CTaskSimpleSwim::ProcessControlInput 0x688A90: same, plus the 2pi wrap
- CTaskSimpleSwim::ProcessControlAI 0x689640: 3D instead of 2D distance
- CTaskComplexSeekEntity... SeekPosCalculatorRadiusAngleOffset 0x6946F0: null matrix
## Task
Verify each against the asm first (a claim may be wrong — say so), fix the confirmed ones, one commit per function with the asm address.
Line-by-line review of each touched function (helper sub-agent allowed). Repro where cheap (NOVSYNC, injector: swim / duck movement probes).
## Acceptance
Each item CONFIRMED+fixed or REFUTED with asm evidence; Release builds; run_all_tests 45/45; soak OK. Report ≤15 lines.
