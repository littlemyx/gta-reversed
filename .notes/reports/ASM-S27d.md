# ASM-S27d (World.cpp, 2 functions + S27c portable cleanup) - 2026-10-28
Result: 2 compared in full, 2 FIXED (2 commits), 0 UNSURE, 0 NOT REACHED. Table in S27d.result.tsv.
Task 0 (9fb52939): CWorld::Process shift damping `const double damping = std::pow((double)..)` -> `const auto damping = std::pow(X87W((x87d)WORLD_PLAYER_SHIFT_DAMPING), X87W((x87d)CTimer::GetTimeStep()))`; Win32 tokens are pow(double,double) as before (bit-identical), portable widens explicitly, the 53-bit pow result stays a double for the three X87R multiplies. v6 pre-commit grep of the S27c diffs (83a0ad0a, 28fbab83, 7a88dc0f) otherwise clean.
Fixes:
- 4b42f51d ProcessLineOfSightSector (0x56B5E0-0x56B781): objects list is processed with doCameraIgnoreCheck (exe 0x56B6FD), the port passed false.
- fa6511e9 ProcessLineOfSightSectorList (0x566EE0-0x5674DA): tyre-crossing test dot products (line dir . right, wheelCP-pos . right) sum (y+z)+x in the exe, port used DotProduct (z+y+x).
Checked and matching: global save/zero/restore and spread restore rule, list offsets (sector+0/+4, repeat+0/+4/+8), final min compare; per entity: scan code / ignore entity / usesCollision / ped alive-dead-biker-attachedTo rule, camera ignore, spread growth/undo of bbox+spheres, ProcessLineOfSight args, wheel colmodel path, 0.5 threshold.
Gate: Release build OK; StandaloneRW wave2_oracle_test: 1142 functions, mismatches PC24 0/0, PC53 0/0; combat gameplay_regress (NOVSYNC, this worktree's StandaloneRelease): PASS=14.
