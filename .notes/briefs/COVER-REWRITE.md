# COVER-REWRITE — rewrite CCover::FindAndReserveCoverPoint and FindCoordinatesCoverPoint from the asm

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh cover-rewrite.
## Evidence (.notes/reports/ORACLE-W3-AI.md)
Rows exist behind W3A_PENDING=1 in wave3_ai_oracle_test: FindAndReserveCoverPoint 483/2500, FindCoordinatesCoverPoint 113/2500
PC24 mismatches. The port bodies are structural reconstructions, not translations — cops/gangs pick different cover than in the exe.
## Task (two functions)
Rewrite both from the disassembly line by line (operand order, x87 precision, loop bounds, tie-breaking), then a line-by-line review
pass against the asm (one read-only helper allowed). Enable the rows by default. One commit per function with asm address and the
pre-fix count. Aim under 250k tokens.
## Acceptance
Both rows on by default with 0 PC24 mismatches; wave3_ai + review_oracle_test (links CoverPoint.cpp) green; report ≤15 lines with start/end time.
