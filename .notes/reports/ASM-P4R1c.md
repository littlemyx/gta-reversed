# ASM-P4R1c: review of P4R1b + more story-path handlers (Commands/*.cpp except CarCommands and Ported/)

Part A: P4R1b commits 23115b968, cf0c4b0b6 (and 8cd3e0223 LOCATE point routing): all CONFIRMED against the exe. GET_OFFSET_FROM_CAR_IN_WORLD_COORDS (0x489831): MATCH (Ported/Group10.cpp, untouched).

Part B: 6 real bugs in ~40 handler cases checked (zone narrowed: CarCommands + Ported/Group*.cpp left to the other agent).
- SET_CHAR_HEALTH 0x47E87D: float param -> int32 (fild)
- SET_OBJECT_HEADING 0x47CCEE: + CWorld::Remove / UpdateRwFrame / CWorld::Add
- SET_OBJECT_ANIM_CURRENT_TIME 0x470C81: m_Speed -> m_BlendHier->m_fTotalTime
- SHAKE_CAM 0x465EEB, DO_FADE 0x47C7C3: int param * 0.001f (was float / 1000)
- SET_CHAR_ROTATION 0x470DAA: + CWorld::Remove(ped) before SetOrientation
