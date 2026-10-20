# ZONEINFO (2026-10-21, rules v3, 00:29 - 01:05)

Question: which port callers of `CTheZones::GetZoneInfo(const CZone*)` (nullptr for zone-info index 0) / `ZoneInfoArray` subscripts differ from the exe, which uses `&ZoneInfoArray[navZone.m_ZoneInfoIndex]` (0x11 per entry) with no index-0 special case?
The point overload `GetZoneInfo(CVector, CZone**)` 0x572400 matches the exe (asm checked; callers CarCtrl:8936, CarGenerator:137, PopCycle:376, Streaming:1280/1307, GangWars StartOffensiveGangWar unaffected).

| site (port) | exe | exe behaviour | result |
|---|---|---|---|
| GangWars.cpp AddKillToProvocation (GetZoneInfo, was :106) | 0x443950 (0x44398C..96) | `ZoneInfoArray[idx].GangStrength[pedType-7]`, no index-0 skip | CHANGED (port: null deref for index 0) |
| GangWars.cpp UpdateTerritoryUnderControlPercentage (:1188) | 0x443DE0 | index 0 skipped (`test ax,ax; je` 0x443E13); loop over the count word 0xBA3794 only | check unchanged; loop CHANGED (port walked all 380 slots incl. stale ones) |
| GangWars.cpp PickZoneToAttack (:694/:711/:731) | 0x443B00 | index 0 skipped (0x443B45, 0x443B94) | unchanged |
| GangWars.cpp CanPlayerStartAGangWarHere (:154) | 0x443F80 | unchecked | already fixed in LINK-CALLEES (bbe7c686) |
| GangWars.cpp StartOffensiveGangWar (:835), point overload | 0x446050 / 0x572400 | same | unchanged |
| Radar.cpp DrawRadarGangOverlay (:1705) | 0x586650 | index 0 skipped (0x5866D4) | unchanged |
| ZoneCommands.cpp SET_ZONE_POPULATION_TYPE / DEALER_STRENGTH / GANG_STRENGTH / POPULATION_RACE / NO_COPS | 0x46ECC0 / 0x46ED23 / 0x46B4DC / 0x4718CD / 0x47962B | missing label skipped (`jl`); else unchecked `ZoneInfoArray[idx]` (index 0 written) | CHANGED (port ignored writes to zone info 0) |
| ZoneCommands.cpp GET_ZONE_DEALER_STRENGTH / GET_ZONE_GANG_STRENGTH | 0x46ED64 / 0x46B535 | no label check: zone -1's index word = TotalNumberOfNavigationZones (0xBA3794) | CHANGED (port: NOTSA_UNREACHABLE) |
| ZoneCommands.cpp SET_ZONE_FOR_GANG_WARS_TRAINING | 0x475222 | no label check, same -1 behaviour | CHANGED (port ignored the command) |
| TheZones.cpp SetZoneRadarColours / FillZonesWithGangColours / AssignZoneInfo..., GangWarsSaveStructure, PopCycle | - | direct `ZoneInfoArray[...]` | unchanged (no helper) |

Rows (PC24, `W2_PORT_FAULT_IS_DIFF=1`, exe oracle ON), pre-fix -> post-fix:
* wave3_gameplay `CGangWars::AddKillToProvocation 0x443950 (zone info index 0 included)` (new, 400 cases, zones with indices 0..3, random strengths): 64 -> 0.
* logic_oracle `CGangWars::UpdateTerritoryUnderControlPercentage 0x443DE0` (stale zone info indices past the count half of the time, 3000 cases): 2657 -> 0.
* ZoneCommands handlers sit in an anonymous namespace and the exe ones inside the CRunningScript::ProcessCommands switch: not oracle-testable; only compiled (ZoneCommands.cpp.obj).
* Post-fix: logic_oracle_test 277 checks 0 failures 0 mismatches; wave3_gameplay_oracle_test 123 checks 0/0.

Commits: GangWars.cpp, ZoneCommands.cpp, the two test files (one commit each).
