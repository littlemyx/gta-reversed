# HEADING-AUDIT — CVector::Heading() vs GetRadianAngleBetweenPoints / atan2 callers in Tasks/

Rules: .notes/agent_rules/v2.md (read it first, then .notes/PLAYTEST_AGENT_ENV.md). Worktree: C:/gta-wt/heading-audit.

## Evidence
ORACLE-W2 (.notes/reports/ORACLE-W2.md) found two tasks where the port uses CVector::Heading() but the exe calls
CGeneral::GetRadianAngleBetweenPoints (different sign/axis convention and limiting). Other callers were not checked.
## Task (one family: heading/angle helpers)
1. List every port call of Heading()/GetHeading-style helpers and GetRadianAngleBetweenPoints/LimitRadianAngle/atan2 in source/game_sa/Tasks
   and source/game_sa/Events (script it). For each, find the exe call target(s) at the corresponding address and compare. Table:
   port file:line → port helper → exe target → OK/MISMATCH.
2. Fix MISMATCHes; where wave2_oracle_test already covers the function, confirm the row still passes; add rows where cheap.
## Acceptance
Table in .notes/reports/HEADING-AUDIT.md with 0 unexplained; Release builds; run_all_tests 45/45; soak OK. Report ≤15 lines.
