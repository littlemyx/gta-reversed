# ASM-P4R1b: P4R1 locate review + story-path handlers (CharacterCommands/ObjectCommands/PlayerCommands)

Part A (P4R1 locate forwarding 3c9d4c242): the helpers and all three jump tables (0x4870A8/C8, 0x4873F8, 0x487704) match the exe. One registration bug: the four in-car POINT opcodes (0xEE, 0xF1, 0x100, 0x103) went to LocateCharCarCmd instead of LocateCharCmd (exe case 0x469751) -> fixed (7927abac4). All 24 opcodes registered.

Fixes (function, exe range, before -> after):
- LOCATE_CHAR_IN_CAR_2D/3D (+STOPPED), exe 0x469751/0x486D80: car-helper -> point helper
- GET_CHAR_HEALTH 0x47EA2B: float -> (int)ftol
- GET_CHAR_VELOCITY 0x470D4F: raw -> *50.0f
- SET_CHAR_VELOCITY 0x470CF4: /50 -> *0.02f
- GET_CHAR_SPEED 0x497228: ped -> vehicle speed when in a vehicle
- IS_CHAR_TOUCHING_CHAR 0x47BB17: symmetric vehicle-or-ped -> exe's per-case direction (both cars: either direction)
- GET_CHAR_AREA_VISIBLE 0x47B145: bool -> raw area code
- SET_OBJECT_COORDINATES 0x47D5C9: SetPosn -> ground-z (z<=-100), Teleport(pos,false), ClearSpaceForMissionEntity
- SET_OBJECT_VELOCITY 0x48352B: /50 -> *0.02f

Ported/Group*.cpp handlers were not re-compared (already per-instruction annotated).
