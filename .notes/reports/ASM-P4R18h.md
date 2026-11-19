# ASM-P4R18h
Review of the 3 P4R18g fixes (4852d5f1d, 1062f0c54, 31b06f016): all CONFIRMED against the 1.0 exe (flag tests, operand order, unrounded x87 sums).
Tasks no-verdict rows: 204 GetTaskType rows auto-compared (exe `mov eax,imm;ret` vs port Type from eTaskType.h): all MATCH. 6 not auto-resolved (SeekEntityStandard/RadiusAngleOffset/AnyMeansXYOffset, RunTimedAnim, RunAnimLoopedMiddle, PlayerOnFoot): left open.
18 Clone rows checked by hand (offsets, branches, ctor arg order): all MATCH.
No source changes, no build needed. Rows: .notes/asm_sweep/P4R18h.result.tsv (225 rows).
Not reached: ctors/dtors of story tasks, ControlSubTask/CreateNextSubTask bodies.
