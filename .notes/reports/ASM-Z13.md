# ASM-Z13: script handlers on the story-mission path without an asm verdict (continuation of Z12)

Rows: .notes/asm_sweep/Z13.result.tsv (35 MATCH rows covering ~50 opcodes, 5 FIXED rows / 6 opcodes). StandaloneRelease links after each push (ninja -j4); no unit test covers these handlers; no game run.

## Item 1: int operand where the handler reads a float (Z12 open risk)
`script::Read<float>` converts a static INT operand by value (the exe keeps the raw dword). A scan of main.scm + script.img (scmscan decode, parameter types from sa.json) found
153,820 float-typed operands: 104,795 float literals, ~49k variables, 0 int literals (not even 0). Nothing in the story or streamed scripts can hit the difference, so the port was left unchanged.
Variables are raw bits in both (GetGlobal<float>/GetLocal<float>). The reverse case (float literal into an int parameter) was already fixed (raw bits, SWEET4).

## Item 2: Z12 "not reached"
SET_CAR_PROOFS tail (bit masks match PhysicalFlags) and TASK_ENTER_CAR_AS_DRIVER move-state tail (offset 0x14, [0x8D237C] consumed and reset to 6, time -2/-1 mapping): MATCH.

## Item 3: real bugs (one commit each)
| commit | opcode | exe | bug |
|---|---|---|---|
| 31ca9b75a | IS_CHAR_SHOOTING_IN_AREA | 0x480148 | corners went through an unsorted CRect; m39/m42/m81 pass flipped corners (x1 > x2), so the shot was never detected. Exe: CPlaceable::IsWithinArea sorts |
| f294480d6 | MARK_OBJECT_AS_NO_LONGER_NEEDED, DONT_REMOVE_OBJECT | 0x47D78B, 0x47D813 | mission-cleanup removal must use the script handle even for a stale object |
| 2eac021b7 | GET_CAR_CHAR_IS_USING | 0x47379B | Find(false) searches the primary slots; exe uses FindActiveTaskByType |
| c821191fe | IS_CHAR_GETTING_IN_TO_A_CAR | 0x47ADB9 | exe: (driver or passenger) and NOT go-to-door; port OR-ed the go-to-door task (m18, m89, valet) |
| ec0e527ec | START_SCRIPT_FIRE | 0x47FEEC | z <= -100 auto-ground was missing |

Classes worth re-checking elsewhere: a CRect built straight from script corners (only IS_CHAR_SHOOTING_IN_AREA, DrawWindow and the unsupported IS_PLAYER_IN_AREA_2D take one),
`Find<T>(false)` vs FindActiveTaskByType (all Scripts/** uses grepped, the others are deliberate), cleanup removal through a pointer instead of the handle (Ported/* use the handle; the unregistered
CharacterCommands RemoveCharElegantly keeps the old form but is dead code). The exe-wide `cmp [0x859014]` (-100.0f) audit: every opcode that auto-grounds was checked; the rest were false hits from neighbouring cases.

## Not reached
About 1000 T3 opcodes have no name hit in the sweep files; most are the Basic arithmetic/compare templates and the Ported/Group* cases (they cite their exe case). Least-used hand-written handlers left:
text setters (0x341, 0x345, 0x349, 0x3E0), camera attach 0x67A-0x67E, crane commands, conversation nodes.
