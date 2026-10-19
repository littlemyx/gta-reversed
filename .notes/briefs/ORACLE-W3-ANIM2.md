# ORACLE-W3-ANIM2 — Animation classes S/C on the ORACLE-W3-ANIM fixture

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-anim2; cmake flags
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/reports/ORACLE-W3-ANIM.md: A/B done (62 rows, 6 port bugs incl. CAnimBlendLink::begin() writing past CAnimBlendClumpData —
heap corruption in the game). Next candidates on the same real fixture: CAnimBlendAssociation SetCurrentTime, SyncAnimation, Start,
Init variants, constructors, UpdateTime, UpdateBlend, UpdateTimeStep; CAnimBlendClumpData / RpAnimBlendClumpUpdateAnimations paths;
remaining Animation S/C/D functions reachable without file IO.
## Task
Rows for these (class S/C, fake vtables where needed), fix every mismatch (one commit per fix, asm address in the subject, row fails
pre-fix) or skip with reason. Then: does the begin() bug explain the SKIN-FLAKY crash (.notes/reports/SKIN-FLAKY.md — instanceMesh
null meshHeader after INTRO1B cutscene load) or playtest-1 U8 (bike pedalling animation broken after a cutscene)? Reason from the code
paths only (no long game loops); state CONFIRMED/PLAUSIBLE/UNRELATED with evidence.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green; report ≤15 lines.
