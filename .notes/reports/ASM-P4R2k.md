# ASM-P4R2k
9 functions re-disassembled vs the 1.0 exe: CalcDeltas, CalcDeltasCompressed, SetupKeyFrameCompressed, FindKeyFrame (the 4 P4R2j UNSURE/structure-only rows) and 5 of 10 FrameUpdateCallBack variants (0x4D1680, 0x4D1A50, 0x4D1DB0, 0x4D2B90, 0x4D30A0). All MATCH, 0 fixes, no source changes (no build needed).
Not reached: 0x4D2100, 0x4D2450, 0x4D27F0, 0x4D2E40, 0x4D32D0, 0x4D2E10 and the ClumpData/FindAnimation/Init/CalcTotalTime/Uncompress/accessor rows. See P4R2k.result.tsv.
