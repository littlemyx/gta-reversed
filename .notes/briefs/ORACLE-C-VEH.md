# ORACLE-C-VEH — first class-C wave: game_sa/Entity/Vehicle functions with indirect (vtable) calls

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-veh.
## Evidence
.notes/ORACLE_COVERAGE.md: class C = the exe function's call closure contains indirect (vtable) calls; ~1175 such functions are
uncovered game-wide, Entity/Vehicle has 86. A/B is nearly exhausted. Existing pieces: wave2 handled embedded vptrs in task fixtures
(.notes/reports/ORACLE-W2.md, "embedded vptr artefact"), ORACLE-W3-INTERIOR / IsTag used fake model-info vtables, ORACLE-W3-AI forwarded
calls to the exe inside a test.
## Task
1. Infrastructure (reusable, document it in the report): a fixture helper that builds a vehicle object twice — on the exe side with the
   EXE's vtable pointers (CAutomobile/CBike/... vtables at their exe addresses) and on the port side with the port's — and compares
   field-by-field excluding the vptr words; virtual callees run their real code on each side (or are recorded by stubs when they leave
   the closure, e.g. audio/render).
2. Rows for ~40 of the 86 Vehicle class-C functions, gameplay-relevant first (damage, handling-derived getters, door/component state,
   ProcessControl helpers that are not class D). Fix every mismatch (one commit per fix, asm address and pre-fix count; consumer audit;
   soak before landing a fix). Target < 5 min.
## Acceptance
Helper documented; coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time. HARD budget under 300k tokens;
partial landing + state file at the cap is fine.
