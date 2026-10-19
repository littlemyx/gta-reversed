# COLLPOINT-ASSIGN — every exe call site of CColPoint::operator= 0x40FC80 must use the exe's partial copy

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh collpoint.
## Evidence (.notes/reports/ORACLE-W3-COLL.md)
The exe's CColPoint::operator= (0x40FC80) copies point, normal, both surface triples and depth, NOT field_C / field_1C / pad 0x26.
Where the exe CALLS 0x40FC80 the port must copy the same way (CColPoint::AssignLikeExe); where the exe copies inline (Process*
functions) it copies all 0x2C bytes — so a global operator= change is wrong (it broke game_oracle_test). Only ProcessDiscCollision
(0x413A65/0x413A93) is fixed; the vehicle ProcessEntityCollision call sites are not.
## Task (one function family)
Script: list every `call 0x40FC80` in the exe → containing function → port function (hook registry) → the port's copy at that point.
Switch each to AssignLikeExe; leave inline-copy sites alone. Verify with existing oracle rows (physics, wave3_vehicle, wave3_coll) and
add a row where a cheap one exists.
## Acceptance
Table site → port file:line → changed/unchanged (reason); run_all_tests green; report ≤15 lines.
