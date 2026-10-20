# ORACLE-W3-AUDIO2 — AEAudioHardware + remaining audio-entity A/B (ped speech, vehicle audio)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-audio2.
## Evidence
Coverage JSON: Audio/Hardware/AEAudioHardware.cpp 19 uncovered A/B (channel/bank-slot bookkeeping, volumes, frequency scaling,
effects-volume/music-volume maths — the mixer the user heard break up in playtest 1), Audio/Entities/AEPedSpeechAudioEntity.cpp 10 and
AEVehicleAudioEntity.cpp 9 left after ORACLE-W3-AUDIO. Lessons: ORACLE-W3-AUDIO.md (+ "open rows": CRT initialisers, recording stubs
for sound hardware), ORACLE-W3-LINK-CALLEES section in TRAP-AUDIT.md (link the callee TUs / exe-forward), W2_PORT_FAULT_IS_DIFF=1.
## Task
Rows for these A+B functions; DirectSound / OpenAL / stream calls recorded by stubs on both sides; run with W2_PORT_FAULT_IS_DIFF=1;
fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with reason. Target < 5 min.
HARD budget under 250k tokens; partial landing + state file at the cap is fine.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
