# W3-PED-OPEN — finish ORACLE-W3-PED items 1 and 3

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh w3-ped-open; configure with
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/reports/ORACLE-W3-PED.md and ORACLE-W3-PED-state.md ("Remaining", items 1, 3, 4).
## Task
1. Item 1: fixture wiring for m_pedSpeech (EnablePedSpeech, EnablePedSpeechForScriptSpeech, CanPedHoldConversation) and a real CWanted
   (SetWantedLevel, SetWantedLevelNoDrop, CheatWantedLevel — gameplay-critical: wanted level and police response); hand rows for
   operator delete(void*) 0x5E4760 and CanIKReachThisTarget 0x609F80. Fix every mismatch (one commit per fix, asm address in the subject).
2. Item 3: improve the fixture so IsHidden / ControlButtonSprint / FlagToDestroyWhenNextProcessed have mostly non-faulting cases.
3. Item 4: check SpawnFlyingComponent's int argument range; if the harness no longer needs the `_exit` at the end of the W3_PED_ONLY target
   (wave1_oracle_test.cpp), remove it — the run must exit cleanly by itself, or explain precisely why it cannot.
## Acceptance
Rows un-skipped with 0 PC24 mismatches; coverage before/after; run_all_tests 47/47; Release builds. Report ≤15 lines.
