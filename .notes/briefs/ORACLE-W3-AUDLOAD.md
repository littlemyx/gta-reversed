# ORACLE-W3-AUDLOAD — Audio/Loaders (AEMP3BankLoader, AEDataStream, + other loader A/B) classes A+B

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-audload.
## Evidence
Coverage JSON: Audio/Loaders/AEMP3BankLoader.cpp 10 and AEDataStream.cpp 9 uncovered A/B, plus any other Audio/Loaders A/B left —
bank slot bookkeeping, sound offsets/lengths inside a bank, request queues, stream read/seek position maths (which sample plays from
where). File IO (CdStream / fread / CreateFile) recorded or served by stubs on both sides from an in-memory fake bank file.
Lessons: ORACLE-W3-AUDIO2.md (audio hardware target, link real TUs), ORACLE-W3-AUDIO.md (CRT initialisers), TRAP-AUDIT.md.
## Task
Rows for the A+B functions; fix every mismatch (one commit per fix, asm address and pre-fix failure count; consumer audit; soak before
landing a fix). Target < 5 min. HARD budget under 250k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
