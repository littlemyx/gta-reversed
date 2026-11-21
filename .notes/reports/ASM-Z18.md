# ASM-Z18: review of the Z17 fix, the two Z17 UNSURE rows, oracle-only gameplay rows (oracle: gta_sa_compact.exe via exedis)

## A. Independent review (C:/gta-scratch/review_results.tsv)
| sha | function | verdict |
|---|---|---|
| 5418f304c | CPedGeometryAnalyser::LiesInsideBoundingBox (0x5F3880) | CONFIRMED: the stack walk gives (dz^2 + dy^2) + dx^2 against r*r; fcompp + test ah,0x41 + jne leaves for r^2 <= sum and for NaN; the fix matches |

## B. Z17 UNSURE rows, re-derived from the exe
- CCarCtrl::FindGhostRoadHeight (0x422370): MATCH. dist1 is stored as float, dist2 stays in st0, result ((dist1*z2) + (dist2*z1)) / (dist2 + dist1), z1 from the current node, z2 from the starting route node, early-outs return 0.0.
- CCarCtrl::FindPercDependingOnDistToLink (0x422620): MATCH. raw int16 * 0.125 for the link position, dy^2 + dx^2, the first compare (< 5.0) uses the unrounded sqrt, the others the float store (>= 15.0 gives 1.0), then (d - 5) * 0.05 + 0.5 returned unrounded; all constants decoded.

## C. Oracle-only gameplay rows compared
Result: .notes/asm_sweep/Z18.result.tsv, 87 rows, all MATCH, no source change (so no link was needed; the source tree is the Z17 linked tip). Groups: CWorld::FindPlayer*, CPopulation predicates and FindSpecificDriverModelForCar_ToUse (byte + jump tables), CPathFind region / interior / hash-list helpers, CMatrix (ForceUpVector, SetRotateZOnly, ...), CCover (Find2HighestPoints, DoesCoverPointStillProvideCover), CDamageManager (all 13, jump tables and the panel multiplier table decoded), CDarkel (8), CStats (9, incl. the two record functions and the SafeToShowThisStat tables), CTheZones (7), CWanted (5), CCarCtrl::SteerAICarParkPerpendicular / TestWhetherToFirePlaneGuns / ChooseBoatModel, two task helpers.

Notes (no behaviour effect): CWanted::Reset / ClearWantedLevelAndGoOnParole additionally call the NOP ResetPolicePursuit; CPathFind::FindLinkBetweenNodes bounds its search by the link count (the exe loops unbounded); CTheZones::FindZone compares C strings, the exe 8 raw bytes.

## Rows left
About 930 oracle-only gameplay rows have no verdict; ~566 of them are CTask* / CEvent* Constructor / Clone / Destructor / FinishAnimCB rows (field-copy order is the likely slip class), the rest is CarCtrl steering (SteerAICarTowardsPointInEscort, WeaveThroughObjectsSectorList), CCheat, Shopping, VehicleModelInfo, Entity, Object, Pickup, PedGroup helpers. That is well over 20, so the callee fallback was not needed.
