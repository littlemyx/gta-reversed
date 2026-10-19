# ORACLE-W3-AUDIO — exe-oracle coverage for game_sa/Audio/Entities (+ Audio root) classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-audio.
## Evidence
.notes/ORACLE_COVERAGE.md: Audio/Entities 316 ported / 12 covered, uncovered A 63, B 81; Audio root 146 / 22, A 37, B 23.
The user's playtest 1 reported mixer breakups when sirens, street noise, voices and gunfire play together, and "all sounds except
radio broke" once (U12/U13 fixed by the collision-audio override; crackle unconfirmed). Audio entity logic (which sound, volume,
frequency, distance attenuation, event → bank/slot) is pure arithmetic over entity state — testable without sound hardware.
Template/lessons: ORACLE-W3-MODELS.md (recording stubs for callbacks), ORACLE-W3-ANIM.md (real fixtures), world_oracle_audio.inc.
## Task
Rows for A+B in Audio/Entities, then Audio root; sound-hardware calls (CAESoundManager::RequestNewSound, CAEAudioHardware) recorded by
stubs on both sides and compared as call logs. Fix every mismatch (one commit per fix, asm address in the subject, row fails pre-fix)
or skip with reason.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green; report ≤15 lines with start/end time.
