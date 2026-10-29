# ASM-S43 (phase 2: CarGenerator, Cheat, Clothes, scanners, conversations, cover ...) - 2026-10-29
Result: 40 rows = 3 compared (1 DEVIATION fixed, 2 MATCH), 37 NOT_REACHED (budget went to S42d). Table: S43.result.tsv.
- CCarGenerator::CheckIfWithinRangeOfAnyPlayers (0x6F2F40-0x6F3187): the three tests take the exe's NaN arms (NaN fails 'dist < lim', passes 'dist >= lim - 20' and 'dot <= 0'). Boat/occlusion/area logic MATCH.
- MATCH: CCarGenerator::SwitchOn, CCarGenerator::CheckForBlockage.
- Open: CarGenerator DoInternalProcessing + Process (spawn decisions, highest value), Cheat::VehicleCheat, CollisionEventScanner, Conversations, Cover, CurrentVehicle, CustomCarPlateMgr, Clothes.
