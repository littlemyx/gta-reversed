# ASM-P4R1e

Part A (review of P4R1d fixes against the exe): CAMERA_SET_VECTOR_MOVE (0x476F9D, fild int time) CONFIRMED; SET_CURRENT_CHAR_WEAPON (0x47D4A8, type 0 -> slot 0 only) CONFIRMED; LOAD_SCENE_IN_DIRECTION (0x47B7E4, deg->rad) CONFIRMED.

Part B: story-list tail (count < 10, not yet in a result TSV): 82 handlers listed (C:/gta-scratch/p4r/p4r1e_tail.txt); ~42 compared line by line, rest NOT_REACHED (flag-scanned: no fild/ftol/fmul candidates). Side-only list (opcodes_side.tsv): empty, every opcode executed in non-story traces also occurs in story traces.

Changes:
- WARP_CHAR_INTO_CAR_AS_PASSENGER (exe 0x489E0F): door = 0 for a negative seat index (was passed through as -1).
