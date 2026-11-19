# ASM-P4R1h: review of P4R1g fixes + non-story static-usage handlers

Zone: source/game_sa/Scripts/**. Oracle: gta_sa_compact.exe 1.0.

- Part A: 3 P4R1g fixes CONFIRMED (HAS_CHAR_SPOTTED_CHAR_IN_FRONT, IS_CHAR_ATTACHED_TO_ANY_CAR, PLANE_ATTACK_PLAYER_USING_DOG_FIGHT). UNSURE resolved: AreTextLabelsEqual MATCH (exe strncmp 16); DrawWindow FIXED.
- Part B: 74 non-story handlers listed (C:/gta-scratch/p4r1h/todo.tsv); ~35 compared, 3 fixes in total with Part A; rest NOT_REACHED, helpers not reached.

| function | exe | before -> after |
|---|---|---|
| DRAW_WINDOW | 0x477017-0x477172 | cornerA.y/cornerB.y from CRect top/bottom (swapped) -> p1*sy / p3*sy |
| INCREMENT_INT_STAT_NO_MESSAGE | 0x47B8CE | int read as float bits -> fild |
| DISPLAY_TEXT_WITH_FLOAT | 0x4730F4-0x47322C | precision float (raw bits) -> int32 (fild) |

Also: a signature scan of 555 non-template handlers against sa.json param types found no other int/float mismatch.
