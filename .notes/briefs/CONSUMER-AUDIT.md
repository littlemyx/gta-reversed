# CONSUMER-AUDIT — retrospective: every oracle-wave fix that changed shared-state layout/indexing/meaning, checked against all readers

Rules: .notes/agent_rules/v4.md (rule 2 is the point of this task). Worktree via tools/standalone/windows/new_worktree.sh consumer-audit.
## Evidence
REGRESS-POLICE (.notes/reports/REGRESS-POLICE.md): 1ef99baa fixed CPathFind::MarkRegionsForCoors to the exe's ToBeStreamed[x*8+y]
layout, correct in isolation, but UpdateStreaming / LoadSceneForPathNodes still read the old layout → wrong node areas streamed → no
traffic, no police. The oracle row passed. Since 2026-10-18 ~250 port fixes landed from the wave-2/3 agents (git log --grep with an asm
address in the subject; reports .notes/reports/ORACLE-W*.md, W3-*.md, ZONEINFO.md, LINK-CALLEES section of TRAP-AUDIT.md ...).
## Task (systematic, script first)
1. List the fix commits since 67087158 that touch port code (git log --format, path filter source/). For each diff classify with a script +
   a quick read: does it change (a) how a shared global/array/pool slot/struct field is indexed, laid out, sized or what its value means,
   (b) a function's contract (return value meaning, out-param, side effect on globals), or (c) only local arithmetic? Most are (c).
2. For every (a)/(b): grep all readers/writers/callers of that state or function in source/ and check each against the exe's convention
   (read the asm at the caller's address where unsure). List them in a table. Fix any caller still on the old convention (one commit per
   fix, asm address in the subject).
3. Soak + gameplay_regress before landing (v4 rule 3) if any fix.
## Acceptance
Table: fix commit → class → state/function → callers checked → OK/FIXED; 0 unexplained (a)/(b) rows; report ≤15 lines with start/end time.
HARD budget under 250k tokens; partial landing + state file at the cap is fine (the table is the deliverable).
