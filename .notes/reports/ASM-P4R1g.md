# ASM-P4R1g: review of P4R1f fixes + rest of static story-usage handlers

Zone: source/game_sa/Scripts/**. Oracle: gta_sa_compact.exe 1.0.

- Part A: POINT_CAMERA_AT_CHAR (0x47C446) CONFIRMED; IS_CHAR_HEALTH_GREATER (0x47CE23) CONFIRMED.
- Part B1: remaining 65 of the 108 P4R1f handlers compared: 3 fixes. Part B2: Ported/Group*.cpp (255 static-story, never-executed handlers): 15 hottest compared, 0 bugs, 240 NOT_REACHED (the Ported code is already annotated per exe case).

| function | exe | before -> after |
|---|---|---|
| HAS_CHAR_SPOTTED_CHAR_IN_FRONT | 0x47B26F-0x47B2B0 | OurPedCanSeeThisEntity(.., true) -> false |
| IS_CHAR_ATTACHED_TO_ANY_CAR | 0x47A089-0x47A0C5 | ped's own type == vehicle (never) -> m_pAttachedTo is a vehicle |
| PLANE_ATTACK_PLAYER_USING_DOG_FIGHT | 0x474E43-0x474E81 | SetCarMission (also rewrote time-to-start) -> plain mission byte store |

Noise: float >= (NaN only), u8/u16 zero-extension of slot/event ids. UNSURE: AreTextLabelsEqual, DrawWindow corner maths.
