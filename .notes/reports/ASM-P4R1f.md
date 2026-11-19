# ASM-P4R1f: review of d2d0482f7 + P4R1e tail + story handlers not executed by the autodriver

Zone: source/game_sa/Scripts/Commands/*.cpp outside Ported/. Oracle: gta_sa_compact.exe 1.0.

- Part A: d2d0482f7 WARP_CHAR_INTO_CAR_AS_PASSENGER (exe 0x489E0F): CONFIRMED (door starts 0, replaced only for seat >= 0; ctor(veh, door, utility 0) then warp flag byte +0x1C = 1).
- Part B1: the P4R1e tail list (82 handlers, C:/gta-scratch/p4r/p4r1e_tail.txt): all compared, 1 bug.
- Part B2: static main.scm usage (T3_SCM_USAGE.tsv, story missions > 0, implemented, handler in Commands/*.cpp outside Ported/, not executed in story traces, no MATCH/FIXED row): 108 opcodes, first ~38 compared hottest-first (story missions, then static_count), 1 bug; the remaining ~70 (< 2 story missions each: text/draw/conversation tail) NOT_REACHED.

| function | exe | before -> after |
|---|---|---|
| POINT_CAMERA_AT_CHAR | 0x47C446-0x47C4C1 | TakeControl target: player ped -> null, other ped -> ped (port had it inverted) |
| IS_CHAR_HEALTH_GREATER | 0x47CE23-0x47CE67 | float param, `health >= p` -> int param (fild), strict `health > p` |

Noise (not changed): IS_FLOAT_*_GREATER_OR_EQUAL (`!(a<b)` vs `a>=b`, NaN only), PRINT_WITH_NUMBER flag zero-extended u16.
