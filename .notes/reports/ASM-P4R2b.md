# ASM-P4R2b: story-path callees outside Scripts/ (continued)

Compared 38 functions (CEventHandler Compute*Response x20, CWorld Add/Remove/ClearPeds/ClearCars, CStreaming RequestModel/RemoveModel, CVehicleRecording x5, CPickups x8): 34 MATCH, 4 FIXED (3 commits), noise only otherwise. Not reached: CCarCtrl steering (SteerAICar*, JoinCarWithRoadSystem*), CPickups Update/GenerateNewOne/DoPickUpEffects, VehicleRecording Interpolate/SaveOrRetrieve/SkipForward (Interpolate/RestoreInfo* already carry exe-address fixes), task constructors.

| function | exe range | before -> after |
|---|---|---|
| CEventHandler::ComputeVehiclePotentialPassiveCollisionResponse | 0x4B96D0-0x4B979D | with a ped group the walk-round-car move state: RUN(5) -> SPRINT(6) |
| CVehicleRecording::RequestRecordingFile | 0x45A020-0x45A055 | request then rec->Remove() (freed loaded data + removed model) -> request only; not-found => index 0 |
| CVehicleRecording::HasRecordingFileBeenLoaded | 0x45A060-0x45A097 | not-found => false -> entry 0 data test |
| CPickups::GetNewUniquePickupIndex | 0x456A30-0x456A65 | wrap at ==-1 -> wrap at unsigned >= 0xFFFE |

INTENTIONAL: PausePlayback/ChangeCarPlaybackToUseAI guard the not-found case (exe indexes slot 16, out of bounds).
