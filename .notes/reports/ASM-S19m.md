# ASM-S19m (Vehicle.cpp upgrade / atomic rows; 15 rows)

Complete. 15 rows compared: 3 DEVIATION fixed, 12 MATCH, 0 NOT_REACHED. Result table: S19m.result.tsv.

| Function | Result |
|---|---|
| RemoveReplacementUpgrade 0x6D39E0 | The exe hands `RemoveObjectsCB` the address of its own `frameId` argument slot (the callback stores the atomic id there). The port passed `&frameOfUpgrade`, so after removing an atomic the frame pointer was the atomic id and `CopyObjectsCB` attached the stock clones to it. Now a scratch int32. |
| ProcessBoatControl 0x6DBCE0 preamble | `bIsDrowning` is set in the drowning branch and the exe jumps over the clear at 0x6DBF03; the port cleared it unconditionally at the end of the block, so the audio / automobile readers never saw it. Also `ts*mass*0.0008f` unrounded in the `>=` / NaN compare. |
| DoHeadLightBeam 0x6E0E20 | Render only. The point is `MultiplyMatrixWithVector` (translation inside the x87 expression); `2x*right.x`, the z,y,x camera dot, `1-|dot|`, `*32` (to `_ftol`), `lightNormal.z`, `lightPos.x/y`, `right*0.05` y/z, `normal*3` (v2.x) and `normal*0.2` x/y are unrounded, the other products are float spills. Written from the stack traffic by hand; no oracle covers it. |
| MATCH | AddReplacementUpgrade, RemoveWinch (unused, exe bug kept unreachable), RemoveVehicleUpgrade, AddUpgrade, AddVehicleUpgrade, SetupUpgradesAfterLoad, SetVehicleAtomicVisibilityCB (Object+Frame), RemoveUpgradeCB, RemoveObjectsCB (Object+Frame), CopyObjectsCB, RemoveAllUpgradesCB. |

Tests: see ASM-S33.md (one build, shared run).
