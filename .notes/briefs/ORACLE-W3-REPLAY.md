# ORACLE-W3-REPLAY — Replay.cpp classes A+B

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-replay.
## Evidence
Coverage JSON: Replay.cpp 27 uncovered A/B — the replay buffer: record/playback of vehicle/ped/world state blocks, buffer bookkeeping,
interpolation of stored matrices, camera modes, packet sizes. Exercised whenever the player saves/plays a replay; also run passively
(recording) every frame in the exe. Lessons: ORACLE-W3-STREAM.md, ORACLE-W3-MISC.md, ORACLE-W3-GP2.md, TRAP-AUDIT.md (W2_PORT_FAULT_IS_DIFF=1).
## Task
Rows for the A+B functions with real fixtures (the replay buffers at their exe addresses with valid packet streams, a vehicle/ped
to record from); fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject; consumer audit per v4 rule
2; soak before landing a fix per v4 rule 3). Target < 5 min. HARD budget under 250k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
