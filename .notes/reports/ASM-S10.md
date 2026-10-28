# ASM-S10 (simple tasks: LeanOnVehicle .. CarGetIn)

Scope: .notes/asm_sweep/S10.tsv, 41 rows. Compared 16, fixed 2, match 14, not reached 25 (budget; CarAlign, CarCloseDoorFromInside, CarDrive*, CarFallOut, CarGetIn ProcessPed/SetPedPosition and the SetPedPosition/ComputeAnimID helpers).

| Function | Verdict | Commit |
|---|---|---|
| CTaskSimpleAnim::MakeAbortable (0x61A790) | FIXED: named-anim script-event check only for IMMEDIATE / dontInterrupt aborts | dcd465dc |
| CTaskSimpleBikeJacked::ProcessPed (0x64C970) | FIXED: second-anim blend delta 100.0; driver-exit audio via ped's vehicle (null-checked) | 094234f8 |
| LeanOnVehicle::ProcessPed, ArrestPed ProcessPed/StartAnim, BeHit StartAnim/ProcessPed, BeHitWhileMoving ProcessPed (+StartAnim head to 0x6200F4), BeKickedOnGround StartAnim/ProcessPed, BikeJacked dtor, CarCloseDoorFromOutside ProcessPed, CarDriveTimed ProcessPed, CarForcePedOut ProcessPed, CarGetIn StartAnim | MATCH | - |

Open: BeHitWhileMoving::StartAnim tail 0x6200F4+ (unconscious timer) not re-read; the 25 NOT_REACHED rows (see S10.result.tsv).
