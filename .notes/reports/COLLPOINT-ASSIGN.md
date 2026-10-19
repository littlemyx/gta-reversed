# COLLPOINT-ASSIGN report

Agent COLLPOINT-ASSIGN (rules v2), worktree C:/gta-wt/collpoint. Task: every exe `call 0x40FC80` (CColPoint::operator=, partial copy of point, normal, both surface triples, depth; NOT field_C / field_1C / pad 0x26)
must use `CColPoint::AssignLikeExe` in the port; inline struct copies of the exe stay full copies (the global `operator=` stays defaulted).

Call-site list: `exedis.py 0x40FC80 --callers` over gta_sa_compact.exe = 16 sites (no jmp / vtable / operand references). Containing functions that the hook-registry heuristic left empty were resolved by hand
(0x419236 / 0x4193F8 = ProcessColModels 0x4185C0, 0x563806 = ProcessVerticalLineSectorList_FillGlobeColPoints 0x5636A0, 0x567324 / 0x567465 = ProcessLineOfSightSectorList 0x566EE0, 0x5E32C4 = CPed::ProcessEntityCollision 0x5E2530).

| exe site | exe function | port file:line | status |
|---|---|---|---|
| 0x413A65 | CCollision::ProcessDiscCollision 0x413960 | Collision/Collision.cpp:1239 | unchanged (already AssignLikeExe, 40f01968) |
| 0x413A93 | CCollision::ProcessDiscCollision | Collision/Collision.cpp:1253 | unchanged (already) |
| 0x419236 | CCollision::ProcessColModels 0x4185C0, disk x sphere hit | Collision/Collision.cpp:2086 `thisLineCP` | **changed** |
| 0x4193F8 | ProcessColModels, disk x box hit | Collision/Collision.cpp:2121 `thisLineCP` | **changed** |
| 0x563806 | CWorld::ProcessVerticalLineSectorList_FillGlobeColPoints 0x5636A0 (one merged copy, the port has 2 branches) | World.cpp:387, 393 `gaTempSphereColPoints[i]` | **changed** |
| 0x567324 | CWorld::ProcessLineOfSightSectorList 0x566EE0, wheel 1 | World.cpp:1841 `outColPoint` | **changed** |
| 0x567465 | ProcessLineOfSightSectorList, wheel 2 | World.cpp:1869 | **changed** |
| 0x5E32C4 | CPed::ProcessEntityCollision 0x5E2530, removal of a colpoint (shift down loop) | Entity/Ped/Ped.cpp:5932 | **changed** |
| 0x6AD11A | CAutomobile::ProcessEntityCollision 0x6ACE70, `m_wheelColPoint[i]` | Entity/Vehicle/Automobile.cpp:1798 | **changed** |
| 0x6AD3E7 | CAutomobile::ProcessEntityCollision, forklift removal loop | Automobile.cpp:1843 | **changed + port bug fixed** (see below) |
| 0x6AD584 | CAutomobile::ProcessEntityCollision, extra wheel output colpoints | Automobile.cpp:1886 | **changed** |
| 0x6BE121 | CBike::ProcessEntityCollision 0x6BDEA0, driver colpoints appended | Entity/Vehicle/Bike.cpp:1221 | **changed + port bug fixed** (see below) |
| 0x6D01D7 | CTrailer::ProcessEntityCollision 0x6CFFD0, `m_wheelColPoint[i]` | Entity/Vehicle/Trailer.cpp:355 | **changed** |
| 0x6D0312 | CTrailer::ProcessEntityCollision, `m_supportCPs[i]` | Trailer.cpp:390 | **changed** |
| 0x73F4C3 | CWeapon::CheckForShootingVehicleOccupant 0x73F480, save | Weapon.cpp:1568 | **changed** (local starts zeroed, the exe's is stack junk) |
| 0x73F8E9 | CWeapon::CheckForShootingVehicleOccupant, restore | Weapon.cpp:1636 | **changed** |

Inline-copy sites (the Process* functions, `qmemcpy` of 0x2C bytes, e.g. the `CColPoint triCP{ cp }` seed at 0x419236 area, ProcessLineOfSight / ProcessVerticalLine outputs) call nothing and stay full copies.

## Port bugs found at call sites (not only the copy kind)
- **CAutomobile forklift filter 0x6AD38A..0x6AD42F**: the exe erases the SURFACE_CAR_MOVINGCOMPONENT (0x41) contacts from the OUTPUT array `outColPoints` (arg 2, `[esp+0xA0]`, the same array that goes to
  ProcessColModels as sphereCPs), shifting the tail down with the called operator= and re-checking the same index. The port ran `remove_if` over `aAutomobileColPoints` (the exe's global wheel-line array 0xC1BFF8), so the
  filtered contacts stayed in the output and the global was scrambled. Now a loop on `outColPoints`.
- **CBike driver colpoints 0x6BE0FF..0x6BE146**: the exe copies the non-leg (`m_nPieceTypeA != 0`) ProcessColModels results of the driver (array 0xC1C280) into the output, while the port copied from a
  separate zero-initialised `pedCPs` (never written), so with a driver `m_nTestPedCollision` appended nothing (every zero entry has piece 0 = leg). Source is now `pedEntityColPts`. The exe also stops at 31 points
  (`cmp [esp+0x18], 0x1F / jge`), the port used 32: fixed with the same hunk.

## Verification
- physics_oracle_test row `CTrailer::ProcessEntityCollision 0x6CFFD0` (4000 cases): the fixture now seeds field_C / field_1C / pad 0x26 of the trailer's 4 wheel + 2 support colpoints with random junk. With the OLD Trailer.cpp (full copies) the row
  FAILS (PC24 1042 / 4000 mismatches, port zeroes the junk, exe keeps it); with the change PC24 0 / 4000 (PC53 32, informational).
- No cheap oracle rows exist for the other sites: CAutomobile / CBike ProcessEntityCollision, CPed::ProcessEntityCollision, the CWorld sector-list functions and CWeapon::CheckForShootingVehicleOccupant (physics_oracle_weapon.inc
  stubs it) all need fake entity + col model + world fixtures that only the trailer test has; the Weapon copy pair is unobservable (the restore only runs when ProcessLineSphere never hit). These sites follow the asm
  (operand order and destination verified in the disassembly of each site), not an oracle row. Candidate follow-up: a CAutomobile row in physics_oracle_trailer.inc style (needs Automobile.cpp linked there) with
  a forklift model index (530) and an object entity.
- run_all_tests in the worktree (GTA_EXE = main checkout exe): 51/51 ok; physics 108 rows, game_oracle 62, wave3_vehicle 85, wave3_coll 100, gameplay 313, wave3_gameplay 121 all 0 mismatches.
- Process: about 70 tool calls. Trailer used: Co-Authored-By Claude Sonnet 5.5.
