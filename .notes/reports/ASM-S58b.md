# ASM-S58b - 2026-10-30

17 rows (S58b.tsv): 11 MATCH, 6 DEVIATION (fixed), 0 UNSURE, 0 open. Table: S58b.result.tsv.

## Task 0 - independent re-check of CleanUpThisPed (0x486300) / CleanUpThisVehicle (0x486670): MATCH
Disassembled both from the exe before reading the S58 report.
- Ped: `cmp byte [esi+0x484], 2; jne ret` (0x48632E) - only a MISSION-created ped (PED_MISSION = 2) proceeds; a null ped / any other creator returns WITHOUT touching the counter (je / jne go to 0x486654, past the `dec`). Then SetCharCreatedBy(PED_GAME = 1), then `test [esi+0x478], 0x10` (bit 4 of the 13th flag byte = bKeepTasksAfterCleanUp) -> jump to `dec [0xC0EC24]` (CPopulation::ms_nTotalMissionPeds, 0x48664E). Every other path ends at 0x48664C -> falls into the same `dec`. So: mission ped -> counter decremented on ALL paths (also bKeepTasks), non-mission -> nothing. The current port (early return for non-mission, ScopeGuard decrement after SetCharCreatedBy, bKeep return inside) matches.
- Driver branch matches too: heli (type 3): mission 0x14 unless 0x39 / 0x3A, dest (10000, -10000, 1000), min / max altitude 1000; plane (type 4): mission 0x16, dest (10000, 10000, 1000); sub 0 (automobile) / 9 (bike): JoinCarWithRoadSystem then mission 1 (cruise) unless 0x39 / 0x3A; bl = 0 makes the function return after the group-membership removal ("captain goes down with the ship"), bStayInSamePlace (bit 21 of the flags at +0x46C) cleared before; passenger / on-foot path: EventScriptCommand(TASK_PRIMARY_PRIMARY = 3, sequence(LeaveAnyCar(0, true, false), wander) / wander) unless an event of type 0x20 or the primary task already is TASK_COMPLEX_SEQUENCE (0xF4) / TASK_COMPLEX_WANDER (0x390).
- Vehicle: `cmp byte [esi+0x4A4], 2; jne ret` (MISSION_VEHICLE = 2), then clears bDontApplySpeed (bit 2), bDisableCollisionForce (bit 13), bIsLocked ([0x428] bit 3), RemoveFromInterestingVehicleList, StopPlaybackRecordedCar, SetVehicleCreatedBy(RANDOM = 1). Port identical.
No change needed, so the missions / arrest0 / combat regress for Task 0 was not required on its own; it was run for the S58b edits (see Gate).

## Fixes (exe address, port deviation)
- ClearSpaceForMissionEntity (0x486B00, 87e125449): the passenger removal walk is `i < m_nNumPassengers` re-read every iteration (0x486D2F); RemovePassenger decrements it, so the exe stops early and passengers in higher seats stay in the (then deleted) vehicle. The port visited every seat. m_pDriver is also set to null unconditionally (0x486CF6; SafeCleanUpRef alone leaves the pointer).
- RemoveScriptSearchLight / AddScriptSearchLight (0x493160 / 0x493000, 3529c796f): the exe resets / initialises the slot field by field and never touches m_nId (+4) or the trailing m_TargetSpot / vf64 / vf70; m_bEnableShadow becomes true on remove. The port wrote a whole default struct (id 0 -> every handle of a slot was the same number, so a stale handle matched a new light). CTheScripts::Init (0x4692D0) sets id = 1 and shadow = true, header defaults changed accordingly.
- RemoveScriptCheckpoint (0x4936C0, a98aa47b8): same id reset (m_nId = 0) removed, m_Checkpoint cleared (0x493703).
- HighlightImportantArea (0x485E00, be138cfb6): ground z gets +2.0f (`fadd [0x858CA0]`, not present in the angled version); the min / max tests are `!(a > b)` (fcomp + test ah,0x41 + jne: NaN takes the first arm).
- DrawDebugAngledSquare (0x486990, 5dc0307a3): the exe takes 8 floats by value (callers 0x4883A5, 0x48876A, 0x488B2F), the port declared const CVector2D& (latent ABI crash for the exe's own callers). Now by value.
- RenderAllSearchLights (0x493E30, 585ae319f): SearchLightCone's corona id is 0x216 + slot (`mov ebx, 0x216`, inc per slot); the port passed the bare slot index (collides with corona ids 0..7).

## Notes
- Save / Load: byte layout verified field by field against both exe functions (see table). Not reproducible: for an invisibility entry whose entity type is nothing / vehicle / ped the exe writes two stale stack words; the port writes zeros (scripts only register buildings, objects, dummies).
- CShopping::LoadStats: the CarPaintJobs key is a stale stack slot in the exe; shipped data/shopping.dat has no CarPaintJobs section (CarMods, Clothes, Haircuts, Tattoos, Food, Weapons, Furniture, Gifts, Property), so the branch is unreachable and the port's section-id key (= GetKey 0x49AB30) is kept.
- DrawDebugSquare: the exe draws nothing (ScriptDebugLine3D 0x485DE0 is a bare ret) and does not adjust p4.z; the port stores debug lines that the standalone run build never renders (Render2dStuff skips RenderDebugShit under NOTSA_STANDALONE_RUN). Probes identical; MATCH.
- Same id-reset pattern may exist for the other script-thing arrays (spheres, effect systems): not in this slice, worth a grep of `= tScript*{}` / placement-new in TheScripts.cpp (Init is fine, it resets once).

## Gate
StandaloneRelease (gta_reversed.exe) and StandaloneRW (wave2_oracle_test) built after the last edit. wave2_oracle_test (RW_EXE_ORACLE = GTA_EXE = the compact exe): 1142 functions, PC24 0 / 0, PC53 0 / 0. Regress `missions arrest0 combat` (NOTSA_STANDALONE_NOVSYNC=1, -b the worktree StandaloneRelease): 32 PASS, 0 FAIL. PortableWin64 compile of TheScripts.cpp (TheScripts.h changed; main checkout's compile_commands with the sources redirected to the worktree, output in scratch): 0 errors. v6 pre-commit grep clean on every commit.
