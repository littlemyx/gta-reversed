# ASM-P4R20c: collision tails

Compared instruction by instruction against exe 1.0: TestLineBox_DW (all), PointInTriangle tail, ProcessLineBox tail, SphereCastVsEntity (all), ProcessColModels line block + sphere-CP world transform: all MATCH, 0 source changes, no build needed.
Stream F triangle primitives (TestSphereTriangle, ProcessSphereTriangle, ProcessLineTriangle): not re-read; float fast path is guarded by RangeGuard and falls back to F24 (PC24-exact emulation), validated by F1 against the exe x87 code (1M cases per row, 0 hard mismatches) - so the SSE2 rounding cannot flip an edge hit outside the guarded range.
NOT_REACHED (budget): ProcessColModels disk block and the A-triangles/boxes vs B-spheres tail from 0x419A5E.
