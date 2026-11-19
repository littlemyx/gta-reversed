# ASM-P4R20b: big collision functions

7610612cd (CColSphere::IntersectPoint) re-checked against exe 0x410040: CONFIRMED (unrounded diffs, sum z,x,y, strict r*r > sum).
Compared MATCH: ProcessSphereBox, ProcessLineOfSight, ProcessVerticalLine, SphereCastVsSphere; ProcessColModels (structure/limits/bbox tests) and ProcessLineBox (plane blocks) partially.
No source changes, no commits besides these notes, no build needed.
NOT_REACHED (budget): TestLineBox_DW, PointInTriangle tail, SphereCastVsEntity, stream F triangle primitives, ProcessColModels disk/line/tail instruction-level pass, ProcessLineBox tail.
