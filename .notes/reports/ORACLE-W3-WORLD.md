# ORACLE-W3-WORLD report

Agent ORACLE-W3-WORLD (rules v3), start 00:41, end 02:2x (2026-10-20), worktree C:/gta-wt/oracle-w3-world (branch oracle-w3-world, rebased on origin/reverse/interior_c 4d4d3602).
New target `wave3_world_oracle_test` (W2 harness: deterministic arena, whole exe data image + scratch block + path-node block compared after every case), rows in `tests/standalone/wave3_oracle_world*.inc`
(`.inc` = fixture, `_a` = World.cpp / World_Helper.cpp, `_b` = PathFind.cpp, `_c` = timer / time cycle / draw / grid ref / common / zones / cull zones).
75 rows (+75 covered sites), PC24 0 mismatches (exit 0). PC53 (CRT default precision, informational) differs in the x87-order rows (CalcScreenCoors, CTimer cycles, ...).

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 3957 | **4032** |
| game_sa root files covered | 1022 | **1097** |
| uncovered A / B in the root files | 337 / 508 | **314 / 457** |
| port bugs fixed (one commit each, asm address + pre-fix failing count in the subject) | | **17 commits** |

## Files covered (rows)
World.cpp: GetCurrentScanCode / GetSector / GetRepeatSector / GetLodPtrList / ResetLineTestOptions / FindPlayerSlotWith{Ped,RemoteVehicle,Vehicle}Pointer; World_Helper.cpp: FindPlayerCoors / Speed / Entity / Train / CentreOfWorld / CentreOfWorld_NoSniperShift / Heading;
PathFind.cpp (31 rows): ThisNodeHasToBeSwitchedOff, IsAreaNodesAvailable, HaveRequestedNodesBeenLoaded, AreNodesLoadedForArea, CountNeighboursToBeSwitchedOff, TidyUpNodeSwitchesAfterMission, AddNodeToList, RemoveNodeFromList, Init, ReInit, CPathNode::GetPosition,
ReturnInteriorNodeIndex, SetPathsNeededAtPosition, StartNewInterior, AddNodeToNewInterior, RemoveLinksToNewInteriorNode, AddInteriorLink, AddInteriorLinkToExternalNode, FindNearestExteriorNodeToInteriorNode, AddDynamicLinkBetween2Nodes_For1Node, SetLinksBridgeLights,
MarkRegionsForCoors, ThisNodeWillLeadIntoADeadEnd, SwitchOffNodeAndNeighbours, SwitchRoadsOffInAreaForOneRegion, SwitchRoadsOffInArea, SwitchPedRoadsOffInArea, RemoveBadStartNode, **CompleteNewInterior with exterior links** (the branch ORACLE-W3-INTERIOR left: real exterior nodes, AddInteriorLinkToExternalNode + both AddDynamicLink calls, 700 cases, 0 mismatches, no fix needed);
Timer.cpp (CTimer::Shutdown / Stop / StartUserPause / EndUserPause / GetCyclesPerMillisecond / GetCyclesPerFrame / GetIsSlowMotionActive), TimeCycle.cpp (9 ambient getters, InitForRestart), Draw.cpp (SetFOV, CalculateAspectRatio), GridRef.cpp (GetArtistBugstarID, GetGridRefPositions),
common.cpp (MakeUpperCase, CalcScreenCoors x2, DoesInfiniteLineTouchScreen, LittleTest), TheZones.cpp (Init), CullZones.cpp (Init, AddTunnelAttributeZone, FindZoneWithStairsAttributeForPlayer, Update).

## Fixture
Players / peds / vehicles with matrices or placements, remote vehicles and the camera in a scratch block (CWorld::Players, PlayerInFocus, bCarsGeneratedAroundCamera, TheCamera at their exe addresses); Players[-1] zeroed (the exe indexes Players[] with a raw id in places).
`ThePaths` (0x96F050) points into a block of 72 areas (64 map + 8 interior) x 10 nodes with random positions, flags, behaviours and real links (24 used link entries per area, the other 16 are free blocks: area 0xFFFF, as the dynamic-link code needs),
car path links, navi links, link lengths; 85% of the areas loaded. PathFind statics (NumNodesGiven, X/Y/ZCoorGiven, ConnectsToGiven, interior-linked-to-exterior tables, ToBeStreamed, node switch boxes, hash table buckets) are set per row. Exe calls: thiscall via `oracle::Fn<R __fastcall(CPathFind*, int, ...)>`.
The asm-vs-source digest for the PathFind group was produced by one read-only helper (sonnet); every item it listed was confirmed or rejected by the oracle.

## Port bugs fixed (rows failed pre-fix; counts are PC24 failing cases of the first run on the unfixed code)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| FindPlayerCentreOfWorld | 0x56E250 | remote vehicle comes first; vehicle looked up for the player IN FOCUS (inlined FindPlayerVehicle(-1)); port used FindPlayerVehicle(playerId, true) | 120/800 |
| FindPlayerHeading(Ext) | 0x56E450 | remote vehicle used whenever set (port needed bInVehicle) | 91/800 |
| MakeUpperCase(dest, src) | 0x7186E0 | returns the START of dest (port: the end) | 289/300 |
| CalcScreenCoors(in, out, sx, sy) | 0x71DA00 | scale = (W * (1/z)) stored as float, then / fov * 70 (port: W * (1/z/fov*70), 1 ulp) | 446/1200 |
| CGridRef::GetArtistBugstarID | 0x71D650 | returns a pointer to an empty string in range (port: null) | 60/200 |
| CPathFind::Init | 0x44D080 | also stores 0 to the global 0x96F02C | 53/60 |
| CPathFind::ReturnInteriorNodeIndex | 0x451300 | not found: {0xFFFF, intId >> 16} (port: node 0xFFFF) | 181/400 |
| CPathFind::MarkRegionsForCoors | 0x44DB60 | ToBeStreamed[x*8+y] (port [y*8+x]); pos -/+ radius kept in extended precision | 385/500 |
| CPathFind::AddDynamicLinkBetween2Nodes_For1Node | 0x44E000 | free-block search stops at the first area-0xFFFF entry (port: first VALID link); signed base compare; dynamic slot free when < 0 | 629/800 |
| CPathFind::ThisNodeWillLeadIntoADeadEnd | 0x44D310 | candidate behaviour 0 or > 10 (port: == 4) | 89/600 |
| CPathFind::SwitchOffNodeAndNeighbours | 0x452160 | linked node skipped against its ORIGINAL state with bBackToOriginal; neighbours of the LINKED node counted | 147/800 |
| SwitchRoadsOffInAreaForOneRegion / SwitchRoadsOffInArea / SwitchPedRoadsOffInArea | 0x452820 / 0x452C80 / 0x452F00 | NaN bound skips the node / keeps the stored box (port let NaN pass) | 3/700, 65/700, 31/400 |
| CPathFind::SetLinksBridgeLights | 0x44D960 | strict bounds (NaN rejects), links walked directly (no 10-bit CCarPathLinkAddress); pre-fix the row could not run (port called the unlinked CRect::IsPointInside), so no failing count | n/a |
| CTheZones::Init | 0x572670 | clears bytes 0..0x1D of every zone, +0x1E = 0 / 3 (map), +0x1F untouched (port: whole CZone{}); first commit used offsets 8 too low, the follow-up commit fixes it | 60/60 |
| CCullZones::Update | 0x72DEC0 | CurrentFlags_Player / _Camera are sign-extended 16-bit words (movsx) | 92/800 + 1/800 |

## Rows that pass without a change
CWorld sector accessors / scan code / ResetLineTestOptions / FindPlayerSlotWith*, FindPlayerCoors / Speed / Entity / Train / NoSniperShift, CTimer x7 (fixture: dividers below 2^24), CTimeCycle getters x9 / InitForRestart, CDraw x2, GetGridRefPositions, CalcScreenCoors(VV), LittleTest,
ThisNodeHasToBeSwitchedOff, IsAreaNodesAvailable, HaveRequestedNodesBeenLoaded, AreNodesLoadedForArea, CountNeighboursToBeSwitchedOff, TidyUpNodeSwitchesAfterMission, AddNodeToList, RemoveNodeFromList, ReInit, CPathNode::GetPosition, SetPathsNeededAtPosition, StartNewInterior,
AddNodeToNewInterior, RemoveLinksToNewInteriorNode, AddInteriorLink, AddInteriorLinkToExternalNode, FindNearestExteriorNodeToInteriorNode, RemoveBadStartNode, CompleteNewInterior (with exterior links), CCullZones::Init / AddTunnelAttributeZone / FindZoneWithStairsAttributeForPlayer.

## Observed, not changed (fixture limits / not game states)
- CTimer::GetCyclesPerFrame: with a divider above 2^24 the exe's `fild * 20.0f` is not rounded like the port's float product (exe product exact, 7/400 and more with a wider range); real dividers are cycles per ms (< 2^24), the row stays below.
- DoesInfiniteLineTouchScreen: with denormal / tiny (1e-20) origin / direction components the exe spills x87 temporaries as floats in the right/bottom-edge helper 0x71DB30 and the result differs (2/2500); the row excludes denormal / tiny specials (not screen coordinates). Not changed beyond the earlier port.
- FindPlayerHeading / FindPlayerCentreOfWorld_NoSniperShift index Players[] with the RAW id in the exe (id -1 reads Players[-1]); every caller passes 0, the rows use 0/1 (port normalises -1 to the focus).
- AddNodeToList: the exe masks the bucket with `& 0x1FF`, the port uses `% 512` (differs for a negative distance only; distances are non-negative).
- FindNearestExteriorNodeToInteriorNode: the exe ignores `this` (uses &ThePaths); same for every caller.
- CTheZones::m_CurrLevel is a byte in the port and a dword in the exe (upper bytes are 0 in the game; the row zeroes them).
- CPathFind::AddInteriorLink / AddInteriorLinkToExternalNode: rows stay inside the arrays (the exe writes past the end of the fixed arrays, the hardened std::array of the port asserts).

## Not covered
- World.cpp sector-list templates (FindObjects*SectorList, ProcessLineOfSightSector*, CallOffChase*, CastShadowSectorList ... ~35 B/S): need real sector lists of entities with collision models (not started: time cap).
- PathFind.cpp: DoPathSearch (0x4515D0), ComputeRoute (0x452760), FindNextNodeWandering (0x451B70) (digest says same logic, only minor deviations: resolve order, address vs node-id compare, walk-back stores), UpdateStreaming / Shutdown / LoadPathFindData (file IO / allocator).
- CTheZones::Update (function-local static `CountSeconds` in the port, an exe global), Shadows.cpp (A 3 / B 6), WaterLevel.cpp (B 8), TimeCycle::Update / SetConstantParametersForPostFX, Clock::Update, common.cpp AsciiToGxtChar (GxtChar.cpp not linked), CalcScreenCoors needs no more.

## Gates
`wave3_world_oracle_test` 75 rows PC24 0 mismatches, exit 0. Targets linking the changed TUs (PathFind.cpp, World_Helper.cpp, common.cpp, GridRef.cpp, TheZones.cpp, CullZones.cpp): `wave3_interior_oracle_test`, `physics_oracle_test` run on the rebased tree, see the hand-back; the orchestrator runs the full run_all_tests.
