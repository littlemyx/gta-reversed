# REPLAY-2 — CompressFromFullMatrix 0x59BAD0 and CPlayerInfo::operator= (0x45DEF0 -> 0x45C4B0)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh replay-2.
## Evidence (.notes/reports/ORACLE-W3-REPLAY.md)
StorePedUpdate still differs in 107/1200 cases, all in the words written by CompressFromFullMatrix 0x59BAD0 (no row of its own; used by
the replay and by every compressed-matrix store). Store/RestorePlayerInfoVariables (W3R_PLAYERINFO=1) deep-copy through
CPlayerInfo::operator= 0x45DEF0 -> 0x45C4B0, which needs a real CPlayerInfo/CWanted fixture.
## Task (two functions)
Rows for CompressFromFullMatrix (matrices from real rotations + edge values; x87 precision lessons from ORACLE-W3-COLL / W3-VEH-OPEN)
and for CPlayerInfo::operator= (real CPlayerInfo + CWanted fixture, W3-PED-OPEN's WireWanted); fix every mismatch (one commit per fix,
asm address and pre-fix failure count; consumer audit: every CompressFromFullMatrix caller; soak before landing). Then turn W3R_PEDUPDATE
and W3R_PLAYERINFO rows on by default. Target < 5 min. HARD budget under 200k tokens.
## Acceptance
Both rows PC24 0; the two gated replay rows on by default and 0; report ≤15 lines with start/end time.
