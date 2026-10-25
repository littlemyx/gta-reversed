# GATE-FAILS — two failures in the 2026-10-25 02:41 gate on f38f3690

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh gate-fails on origin/reverse/interior_c.
## Evidence (C:/gta-scratch/run_all_tests.396473/, previous green gate on 3e4d1d67: C:/gta-scratch/run_all_tests.363670/)
1. wave3_tasks_oracle_test: PC24 1 / 1 (was 0 / 0 on 3e4d1d67). The row right after `CTaskSimpleFight::IsHitComboSet 0x4ABDF0`:
   "MISMATCH PC=24 case 12: PORT faulted (0xE0000001) while the exe did not", assert `span subscript out of range` (MSVC span(462)).
   Only commits between the two gates: RD-AB-3 f38f3690 (Streaming.cpp RemoveBuildingsNotInArea, Automobile.cpp ProcessBuoyancy rand,
   lockstep probes) and notes. Find the row, find why it changed (a shared fixture consuming rand()? a real port bug exposed?), fix it.
2. rw_math_stream_test does NOT BUILD (ninja.log): source/standalone/rw/rtquat.cpp(86..97) `'X87W': identifier not found` — the test
   target compiles rtquat.cpp without the Base.h that defines X87W (portable F-loop 3, 75085658). The old binary ran, so run_all_tests
   printed PASS. Fix the include (the RW layer TU must see the macro in every target) so the target builds and passes.
## Acceptance
run_all_tests: both targets ok; Release builds; soak OK if game code changed. HARD budget 150k tokens / 60 min.
Work ONLY in your worktree. Never kill by image name.
