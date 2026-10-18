# W2-OPEN — the three open 1-in-60 oracle mismatches left by ORACLE-W2

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh w2-open.
## Evidence (.notes/reports/ORACLE-W2.md, skip table rows marked OPEN; rows are in tests/standalone w2_skip.tsv)
- CTaskSimpleUseGun::ControlGunMove (0x61E0C0 area) and CTaskSimpleDuck::ControlDuckMove: 1 of 60 special-input cases differs
  (port 17.9 / exe -113.0); ordinary inputs match.
- CTaskComplexFacial::CreateNextSubTask: 1 of 60 special-input cases leaves m_RequestB 0 (port) vs -1 (exe).
## Task (three small functions)
For each: find the exact input of the differing case (the generator tests/standalone/tools/gen_w2_*.py), evaluate the exe path for it
in the asm (x87 compare/NaN/inf semantics, signed/unsigned compares, uninitialised member reads, fallthrough), fix the port to the exe
semantics, un-skip the row so wave2_oracle_test covers it. If a difference is a fixture artefact impossible in the game, prove it and keep
the skip with the reason.
## Acceptance
3 items each FIXED (row un-skipped, 0 mismatches) or EXPLAINED; mutation check (revert fix → row fails); run_all_tests 45/45. Report ≤15 lines.
