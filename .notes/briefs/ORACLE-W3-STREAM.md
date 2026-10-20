# ORACLE-W3-STREAM — IplStore.cpp + TxdStore.cpp + Streaming.cpp classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-stream.
## Evidence
Coverage JSON: IplStore.cpp 12, TxdStore.cpp 12, Streaming.cpp 10 uncovered A/B — which IPL/TXD slots are requested, removed and kept,
bounding-rect tests that decide what streams in around the player, request-list bookkeeping. The IplStore leak was a playtest-era
finding (fixed by hand). Lessons: ORACLE-W3-MISC.md, ORACLE-W3-CARENV.md (pool fixtures), ORACLE-W3-WORLD.md, ORACLE-W3-PATHS.md.
## Task
Rows for these A+B functions with real fixtures (the IplStore/TxdStore pools at their exe addresses with valid slots and rects,
CStreaming::ms_aInfoForModel entries and request lists in a consistent state); file IO / CdStream calls recorded by stubs on both sides;
fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with reason. Target < 5 min.
HARD budget under 200k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.
