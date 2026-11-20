# ASM-Z12: script command handlers without a verdict (continuation of Z11) + the GetHeading rounding class

Rows: .notes/asm_sweep/Z12.result.tsv (62 MATCH rows covering ~85 opcodes, 10 FIXED rows in 9 functions, no UNSURE).

## Method
The to-do list was rebuilt from T3_SCM_USAGE.tsv minus the Z6/Z8/Z11 names (the Z6 noverdict list was lost in the 20.11 wipe), most-used / most missions first:
GET_CHAR_COORDINATES, IS_CAR_DEAD, SET_CHAR_HEALTH, GIVE_WEAPON_TO_CHAR, PRINT/PRINT_NOW/CLEAR_*, blips, mission audio, LOAD_SCENE, the TASK_* creators,
animation requests, car recordings, SWITCH_START, STORE_CAR_CHAR_IS_IN, SIN/COS, distance getters, relationship / weapon / health setters. Each exe case
(ops.json address -> exedis) was read against the C++ handler: parameter count and order, operand types (fild vs fld), null handling, pool lookups,
constants, rounding. The `Ported/Group*` files cite the exe case in a comment and were traced very carefully (all MATCH); the hand-written upstream
handlers (CharacterCommands, ObjectCommands, TextCommands, ScriptCommands, MathCommands...) matched too, except the heading getters below.

## The one real bug class: CPlaceable::GetHeading is an unrounded FPATAN
`CPlaceable::GetHeading` (0x441DB0) ends with `fpatan; ret`: the value stays in ST0 at extended precision, and the CALLER does the next operation on it.
The port's `GetHeading()` returns a float, so `GetHeading() * k` / `GetHeading() - c` rounds twice (the exe rounds once). For ~1 in 4 headings
the result differs in the last bit. `GetHeadingExt()` (double, FPATAN stays unrounded) already existed and is used in ~15 places; the sites below
were missed. Fix = `(float)X87R(GetHeadingExt() <op> const)` (the pattern of Physical.cpp:2475).

| commit | function | exe |
|---|---|---|
| d67db1cdf | GET_CAR_HEADING | 0x47CBE1 |
| fa8d36606 | GET_CHAR_HEADING | 0x47CAA9 |
| 9416174ad | GET_OBJECT_HEADING (jumps into the car body) | 0x47CCCE -> 0x47CBFC |
| 04247a9b5 | ROTATE_OBJECT | 0x481CA5 |
| 5d926a7c0 | CCam::Process_1rstPersonPedOnPC (4 cases) | 0x50F0AD.. |
| a6f6c73c5 | CCam::Process_Cam_TwoPlayer_InCarAndShooting | 0x51A33E |
| cf119efbb | CCam::Process_FollowCar_SA (3 sites) | 0x5251E7, 0x52527D, 0x52531B |
| fffa342f4 | CCamera::CamControl (2 sites) | 0x52A88D, 0x52AB34 |
| eb496846c | CGameLogic::UpdateSkip | 0x442741 |
| 6180d7722 | CTaskGangHassleVehicle::ControlSubTask | 0x663C3F |

Scan: all 26 `call 0x441db0` sites of the exe (exedis --callers) were looked at; the other callers either `fstp` the result to a float first
(0x458282, 0x5872EA, 0x5E2638, 0x6046C4, 0x46DD3B float argument, 0x663BA1), already use GetHeadingExt (0x548270, 0x64DF04) or the M16 camera
(own M16HeadingExt). Heading effects are one float ulp (~4e-6 deg), so the visible effect is on state hashes / replays, not on gameplay.
Build: StandaloneRelease links after each push (ninja -j4, 3 incremental builds); the portable spelling is X87R(...) on a double operand.
No unit test covers these handlers; no game run (rules).

## Notes
- Operand-type audit (exe `fld`/`fild` on ScriptParams slot vs the port's float/int parameter type, all T3 opcodes): 31 hits, all false positives
  (the disassembly ran into the next case); checked 4 by hand. The earlier open risk (Read<float> of a static INT operand converts instead of
  keeping the raw dword) was not changed.
- Port deviations kept on purpose: STORE_CAR_CHAR_IS_IN (-1 stored-vehicle guard, throws for a char on foot), LOAD/PLAY_MISSION_AUDIO (uint32 slot-1
  vs u8; differs for slot 0 only), SET_CAR_HEALTH & co. skip a null handle (the exe crashes).
- Not reached (no verdict): SET_CAR_PROOFS tail (only the 6-param head read), TASK_ENTER_CAR_AS_DRIVER move-state tail, ~150 of the least used handlers.
