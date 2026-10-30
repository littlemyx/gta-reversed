# ASM-S91 (CFileLoader IDE/IPL/DAT parsers, 24 rows) - 2026-10-30
Result: 24 compared in full: 3 FIXED (3 commits), 21 MATCH, 0 UNSURE, 0 NOT REACHED. Table in S91.result.tsv.
Fixes:
- e8ec9691a LoadTexDictionary (0x5B3860-0x5B38B1): stream was closed twice when the txd read failed (exe closes once, then creates an empty dictionary).
- 1e276a9cf LoadAudioZone (0x5B4D70-0x5B4E59): audio zone active flag is `flags != 0` (test/setne) in both box and sphere forms; port used `== 1`.
- 57be74ce8 LoadOcclusionVolume (0x5B4C80-0x5B4D63): the interior-ipl flag only checks "int" at filename[len-7..len-5]; the port required the whole "int.ipl" suffix.
Verified by stack-slot decoding of every sscanf (arg order -> struct offsets): Load2dEffect (all 9 types), LoadObjectInstance, LoadStuntJump, LoadTimeCyclesModifier, LoadCullZone, LoadObject/LoadTimeObject (objType 1/2/3 formats, draw dist <4 reparse).
Note (not changed): LoadTimeObject default objType case is NOTSA_UNREACHABLE; the exe falls through with uninitialised locals (malformed data only).
Gate: Release + StandaloneRW build OK; wave2_oracle_test PC24 0/0, PC53 0/0 (1142 fns); PortableWin64 compile of FileLoader.cpp 0 errors.
