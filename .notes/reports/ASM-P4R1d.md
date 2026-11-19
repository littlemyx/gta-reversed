# ASM-P4R1d: review of the P4R1c fixes + non-Ported script command handlers on the story path

Oracle: gta_sa_compact.exe 1.0. Zone: source/game_sa/Scripts/Commands/*.cpp outside Ported/.

Part A (independent review): SET_CHAR_HEALTH, SET_OBJECT_HEADING, SET_OBJECT_ANIM_CURRENT_TIME, SHAKE_CAM, DO_FADE, SET_CHAR_ROTATION: all 6 CONFIRMED against the exe, no change.

Part B: ~60 handlers compared (hot first; plus 8 sampled Basic template handlers), 3 fixes. Build: changed .cpp compiled + full StandaloneRelease link OK.

| Function | Exe | Before -> after |
|---|---|---|
| CAMERA_SET_VECTOR_MOVE | 0x476F9D | time param float -> int32 (fild), cast to float |
| SET_CURRENT_CHAR_WEAPON | 0x47D4A8 | weapon type 0: slot scan matched all empty slots (last wins) -> slot 0 only, as exe |
| LOAD_SCENE_IN_DIRECTION | 0x47B7E4 | heading in degrees passed raw -> * 0x8595EC (deg->rad) before CRenderer::RequestObjectsInDirection |

Notes: the ~80 Basic template handlers (compare/arith/assign/CSET) were only sampled (8), not read one by one. LOCATE_CHAR_*_OBJECT_*: port uses the generic LocateChar2D/3D; same inclusive box test; only edge case is bInVehicle with null m_pVehicle (port IsInVehicle needs both). Not reached: the remaining TODO tail (counts < 10) of the story list.
