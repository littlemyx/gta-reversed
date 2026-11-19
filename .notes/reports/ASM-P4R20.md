# ASM-P4R20 collision core
Compared 36 functions: 35 MATCH, 1 FIXED, 6 NOT_REACHED (see .notes/asm_sweep/P4R20.result.tsv).
Fix 62fa28f33: CColSphere::IntersectPoint (0x410040) summed float differences x,y,z; exe uses unrounded diffs summed z,x,y.
Not reached: ProcessColModels, ProcessSphereTriangle/TestSphereTriangle/ProcessLineTriangle (stream F SSE2 templates), ProcessSphereBox, ProcessLineBox, TestLineBox_DW, ProcessLineOfSight/VerticalLine/SphereCastVsEntity.
Build: StandaloneRelease links (ninja -j4). No tests / regress run (budget).
