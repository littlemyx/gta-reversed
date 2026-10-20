# TRAP-AUDIT — re-run every wave2/wave3 oracle row with port faults counted as differences

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh trap-audit.
## Evidence
ORACLE-W3-GP2 continuation (.notes/reports/ORACLE-W3-GP2.md): the W2 harness treats a port fault at EIP 0x400000 (an unlinked symbol,
e.g. NOTSA_UNREACHABLE / an assert / a trap the exe does not have) like a fixture limit and skips the case. CShopping::FindItem trapped
on an absent key where the exe returns -1 — hidden by that rule. `w2::g_portFaultIsDiff` (wave2_oracle_common.inc, default false) now
lets a row count it as a mismatch. Every wave2/wave3 target may hide more such cases: "both sides fault" and "port-only fault" are
currently indistinguishable in the summaries.
## Task (one systematic pass)
1. Add a global switch (env W2_PORT_FAULT_IS_DIFF=1) that sets g_portFaultIsDiff for every row; print per row: cases where ONLY the port
   faulted (with the port EIP / symbol) vs both faulted.
2. Run every wave2/wave3 target with it (they are CPU-only; run them in parallel at low priority, ≤4 at a time). List every row with
   port-only faults.
3. For each: is the input reachable in the game? If yes, fix the port to the exe behaviour (one commit per fix, asm address and
   pre-fix count); if no (garbage the game never produces), keep it and note why. Rows stay default-off for the switch unless fixed.
## Acceptance
Table row → port-only faults → FIXED / UNREACHABLE (why); all targets still PC24 0 with the switch off; report ≤15 lines with start/end time.
HARD budget under 250k tokens; partial landing + state file at the cap is fine.
