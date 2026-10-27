# ASM-S20f - CPed::ProcessControl and CPed::CPed

Rules v6; worktree C:/gta-wt/asm-s20f (removed at the end). Result table: `.notes/asm_sweep/S20f.result.tsv`. Budget respected (~100k tokens).

| verdict | count |
|---|---|
| MATCH | 1 (ProcessControl 0x5E8CD0, whole function to the ret at 0x5E9530; all 13 float/double constants read from the exe) |
| FIXED | 1 (CPed::CPed) |
| UNSURE | 1 (m_roadRageWith: exe stores one byte at +0x460, port nulls the pointer; harmless) |
| NOT_REACHED | 0 functions; Constructor 0x5E8057-0x5E8110 (inlined CAE*Audio sub-object ctors) not compared |

## Fix (de1107a2, Ped.cpp, 0x5E82D9-0x5E830F)
- bHasACamera (bit 17 of the flags word at +0x46C): exe does `rand()&3; neg al; sbb eax,eax; inc eax`, i.e. true when the low two bits are ZERO (25 % of peds); the port used `% 4 != 0` (75 %). Same single rand() call, only the polarity changes.
- Everything else in the constructor matches: every field store/offset, flag words 0x46C=0x80002000|cam, 0x470=0x6100000, 0x474/0x478 defaults, weapon loop, skill/style 1/4, Facial (slot 3) and StandStill(0,true,false,8) tasks, wobble/removal-dist/script-brain init, bounty-cheat hate event.

## Gate
Release build OK (full rebuild, 0 errors). No oracle target covers CPed::CPed or ProcessControl (class D); gameplay_oracle/run_all_tests not run (orchestrator gate). soak.sh 7 min RESULT: OK (0 errors, 11100 frames). PortableWin64 not run (no shared header touched).
