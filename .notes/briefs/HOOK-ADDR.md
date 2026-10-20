# HOOK-ADDR — audit every hook-registry address: does it point at the start of the function it claims?

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh hook-addr.
## Evidence
ORACLE-W3-VIS (.notes/reports/ORACLE-W3-VIS.md): CVisibilityPlugins::SetClumpForAllAtomicsFlag was hooked at 0x732307 — a bare `ret`
inside the previous function — instead of the real entry 0x7322F0. In the DLL build such a hook patches the wrong bytes; in the run build
the address only labels the function (oracle coverage, sync tooling) — so a wrong address silently hides a function from every check.
## Task (one script + fixes)
Script over all RH_Scoped*Install sites (oracle_coverage.py already parses them): for each address check with capstone on
gta_sa_compact.exe that it is a function entry (preceded by ret/int3/padding or a jump target of a call; not inside another function's
body per the IDA/Ghidra function list if available; first instruction plausible: push ebp / sub esp / mov / etc.). Flag: mid-function,
on a ret, on padding, duplicate addresses for different port functions, addresses outside .text. For each flagged site decide with the
asm whether the address or the port mapping is wrong; fix the address (one commit per file, list addresses in the subject).
## Acceptance
Table site → address → verdict (OK / FIXED old→new / EXPLAINED) with 0 unexplained; script committed under tools/standalone/; HARD budget
under 250k tokens; report ≤15 lines with start/end time.
