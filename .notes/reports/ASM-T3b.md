# ASM-T3b: static script-command usage of main.scm + script.img (trusted; replaces ASM-T3a)

Tools: `tools/standalone/scmscan/` (`gen_argcounts.py` -> `argcounts.tsv`; `scmscan.py` decoder; `gen_t3_usage.py` -> `.notes/asm_sweep/T3_SCM_USAGE.tsv`).
Data: Steam-independent 1.0 main.scm + script.img (C:/gta-scratch/scmtrace/d), 215 segments (main thread, 135 mission scripts, 79 streamed).

## Argument counts (opcodes 0x0000..0x0A4E, 2639)
- Sources: 1.0 `gta_sa_compact.exe` group handlers (jump table 0x8A6168, 27 x ProcessCommandsNNN; per case: sum of `CollectParameters(n)` pushes
  +1 per ReadTextLabelFromScript/GetPointerToScriptVariable/ReadArrayInformation) = 1969 opcodes with a case body; cross-checked with Sanny Builder
  `sa/sa.json` (library commit 4ea6da3c50e25a4607df826dd1114b290549cfde, raw.githubusercontent.com/sannybuilder/library/<sha>/sa/sa.json, `default` extension).
- argcounts.tsv: both 1615 (exe == db), db 1021 (exe case absent = no handler / delegates arg reading to helpers such as LocateCharCommand, so exe is a
  lower bound; the exe count is NEVER higher than the db, 0 contradictions), exe 1. No source for 0x00CD/0x00CE (not stock commands, unused).
- Special cases confirmed in the exe: START_NEW_SCRIPT 0x004F and START_NEW_STREAMED_SCRIPT 0x0913 = 1 fixed arg + variadic tail (type 0 end; table value -2 = -1-fixed);
  SAVE_STRING_TO_DEBUG_FILE 0x05B6 = 128 RAW bytes, no typed args (exe 0x48FAEC; the DB's "1 arg" would desync).
- Disagreement resolved by decoding: none needed (no exe>db); the 333 exe<db cases are all helper-delegating handlers and decode correctly with the db count.

## Validation
- Decoder resync: 214 of 215 segments decode to their end (ASM-T3a: 12 of 215). The one exception is streamed `aaa.scm` (script.img entry 0, 2048 bytes starting
  `18 ab 00 00 39 02 ..`): not an instruction stream (opcode 0x2B18), never reached; explained, not a count error.
- Traces (full/record.scmtrace, catalin.scmtrace): every distinct X (base, ip) row is a decoded instruction boundary: record 3049 rows, 0 off-boundary,
  catalin 463 rows, 0 off-boundary. One row (`s-1`, no script id) cannot be mapped to a segment.

## Totals (registration as of this branch: implemented 1644, NOP 268, UNIMPLEMENTED 713, UNSUPPORTED 13, unregistered 1; 1571 distinct opcodes used)
| class | registered | used by any script | story missions (100) | side missions/odd jobs/minigame subscripts (35) | main thread only | streamed only | static uses |
|---|---|---|---|---|---|---|---|
| UNIMPLEMENTED | 713 | 4 | 3 | 1 | 0 | 0 | 31 |
| unregistered | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| UNSUPPORTED | 13 | 0 | 0 | 0 | 0 | 0 | 0 |
| NOP | 268 | 14 | 8 | 3 | 3 | 0 | 360 |
Story = mission scripts INTRO1..FINALEC except CPRACE and DSKOOL; "side" = INITIAL/INITIL2/INTRO/NONE/DUAL/SHTR/GRAV/OTB/POOL2/LOWR/ZERO5, CPRACE, DSKOOL,
SHRANGE..BUYPRO1 (gyms, jobs, odd jobs). 709 of the 713 UNIMPLEMENTED commands are never used by the shipped scripts.

## Used but not implemented, ranked by mission reach
1. 0x01A6 IS_CHAR_IN_AREA_ON_FOOT_3D: 5 uses, story m92 CASIN10 + m100 HEIST5, streamed roulette.scm. Compare-flag command (exe calls CharInAreaCheckCommand 0x488B50): likely mission-visible (area trigger gates mission progress).
2. 0x0052 COMMAND_LINE: 20 uses, story m41 CAT1 + m56 WUZI2, side m8 POOL2, streamed bcesar3.scm; exe handler only does CollectParameters(6): cosmetic/debug (a no-op in the exe, safe to register as NOP).
3. 0x01A7 IS_CHAR_IN_AREA_IN_CAR_3D: 1 use, story m51 SCRASH3. Compare-flag command, likely mission-visible.
4. 0x0384 PRINT_STRING_IN_STRING_NOW: 5 uses, side m117 TRUCK, m123 FIRETRU, m124 COPCAR (job HUD text; exe reads 2 labels + prints): likely mission-visible (text), not story.
(The ranking sorts by story reach, then side/main/streamed reach, then static count; see the TSV, columns story_missions/side_missions/main/streamed.)

## NOP commands whose exe handler does more than CollectParameters + return (used by scripts)
Heuristic: case body calls something other than CollectParameters/UpdateCompareFlag, or has >14 instructions; 2 of the 14 used NOP commands:
- 0x018D ADD_CONTINUOUS_SOUND (3 uses: story m23 DRUGS3, m42 CAT2, streamed ammu.scm): exe case also calls 0x464370 (StoreParameters, writes the sound handle out); port NOP leaves the output var unwritten. Audio-only, low risk.
- 0x03AD SWITCH_RUBBISH (5 uses: side m128 BLOOD, streamed blackj.scm, gf_date.scm): exe calls 0x7204C0 (CRubbish visibility toggle). Cosmetic.
The other 12 used NOPs (e.g. CLEAR_ALL_VIEW_VARIABLES 0x086A, SAVE_NEWLINE_TO_DEBUG_FILE 0x03A9, START/STOP_RECORDING_CAR 0x05E9/0x0752, SHUT_PLAYER_UP 0x04E2,
REMOVE_SOUND 0x018E, REGISTER_STREAMED_SCRIPT_INTERNAL 0x0914, DISPLAY_TIMER_BARS 0x06CF ...) have exe cases that only collect args and return.
