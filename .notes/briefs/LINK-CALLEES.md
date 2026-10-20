# LINK-CALLEES — link (or exe-forward) the callee TUs that cut 26 oracle rows short

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh link-callees.
## Evidence (.notes/reports/TRAP-AUDIT.md)
26 wave2/wave3 rows fault on the PORT side when the port calls into a TU the test target does not link (CMemoryMgr::MoveMemory,
CPedType::GetPedFlag, CRadar::ClearBlipForEntity, CHudColours::GetIntColour, ...): the rest of the function after the call is never
compared in 2-95 % of cases per row. TRAPAUDIT lines (W2_PORT_FAULT_IS_DIFF=1) list the callee return addresses.
## Task
For each callee: link its TU into the affected target(s) (preferred, when it builds cleanly in the W2 base), or forward the port call to the
exe function inside the test (the ORACLE-W3-AI CFormation trick). Re-run each affected row with the switch on: port-only faults must drop
to 0 (or to an explained remainder); any new mismatch that appears is a real port bug → fix (one commit per fix, asm address and pre-fix
count). Keep each target under 5 min (oracle_fast helpers if needed). HARD budget under 250k tokens.
## Acceptance
Table row → callee → linked/forwarded → port-only faults before/after → new mismatches fixed; all affected targets PC24 0; report ≤15 lines.
