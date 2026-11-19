# ASM-P4R19c: rest of the Events functions

Checked against gta_sa_compact.exe (1.0): ComputeDamageResponse (control flow, constants, ctor args), ComputePersonalityResponseToDamage (dispatch + table only),
ComputeVehiclePotentialCollisionResponse tail, ComputePedCollisionWithPlayerResponse tail (0x4BF19F+), 22 GetEventPriority constants, 17 AffectsPed bodies.

Fix (1): CEventStuckInAir::GetEventPriority (0x4B1600) returned 75 for <= 1 queued events; the exe returns 75 for MORE than one (`cmp [group+0x70],1; jg`).

Open: ComputeBuildingCollisionResponse, per-case args of ComputePersonalityResponseToDamage, head of ComputeVehiclePotentialCollisionResponse, final DoDie tail of
ComputeDamageResponse (0x4C0AA2+), and the remaining nv.tsv Events rows (ctors / Clone / CloneEditable / AffectsPedGroup / ReportCriminalEvent / IsValid).
