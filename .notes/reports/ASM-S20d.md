# ASM-S20d - Ped.cpp large functions (10 listed, 6 reached)

Rules v6; worktree C:/gta-wt/asm-s20d (removed at the end). Result table: `.notes/asm_sweep/S20d.result.tsv`. Stopped at the token budget.

| verdict | count |
|---|---|
| MATCH | 3 (KillPedWithCar, PositionPedOutOfCollision, DoFootLanded + its two helpers 0x5E3630/0x5E37C0) |
| FIXED | 3 |
| UNSURE | 0 |
| NOT_REACHED | 4 (PreRenderAfterTest, ProcessEntityCollision, Constructor, ProcessControl) |

## Fixes (one commit each, Ped.cpp)
- PositionAnyPedOutOfCollision 0x5E13C0 (8e8485df): was a different algorithm and never found a point. Exe: 15x15 grid, step +0.5 from pos-3.5 (x reset per row), skips points hitting buildings/objects (port skipped the free ones), closest point per class (int dist, start 999); the vehicle-class z offset reads the colmodel of the last vehicle test result (edi).
- ProcessBuoyancy 0x5E1FA0 (f8579c74): the discarded rand() at 0x5E2160 was commented out (RNG sequence shifted for every ped in water); mass*0.008f instead of /125 (3 sites); pow(0.9,dt) kept unrounded for the speed multiply.
- PlayFootSteps 0x5E57F0 (3911d3f9): skater right-foot test `<=` (jp lets equal through); skate volume uses the x87 log10 (fyl2x) times 20.

## Notes
- Consumer audit: all changes are local to the function bodies (no signature or shared-state change).
- Gate: Release build OK; no oracle target covers these Ped functions; soak.sh 7 min RESULT: OK (0 errors, 10600 frames).
