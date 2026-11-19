# ASM-P4RV1b: re-review of old S-series FIXED rows (Tasks/Events), continued
Candidates: ~340 un-reached after P4RV1 (Tasks/Events/Ped/Scripts half). Reviewed 10 with the semantic fixes first (exe re-read at the changed spots): 10 CONFIRMED, 0 WRONG, 0 PARTIAL. No source change, no build.
Notes: ComputePlayerCollisionWithPedResponse re-read in full. Not re-read inside confirmed rows: Swim 80deg literal, Climb dot-order spots, KillPedWithCar pedPosOS dots/shake tail.
Not reached: SwimmingResistance, GangHassleVehicle, Climb::ProcessPed, JetPack (ProcessThrust/ControlInput), ReviveResponse, remaining Compute*Response rows, Scripts/**.
Rows: .notes/asm_sweep/P4RV1b.result.tsv.
