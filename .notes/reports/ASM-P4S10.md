# ASM-P4S10: top tool-flagged REVIEW rows (P4T1), gameplay classes

Queue: 511 rows (.notes/asm_sweep/P4S10.queue.tsv, by P4T1 score). Compared 17 (12 of them pure tool noise: inlined ctors/helpers, constant double-rounding, squared literals). 4 FIXED, 13 MATCH, 494 NOT_REACHED (budget).

- CTaskSimpleFightingControl::MakeAbortable (exe 0x61DD00): collision-normal threshold sqrt2/2 (0.70710677) -> float 0.707104981 (0x863C1C); dot summed y,z,x unrounded. a7d4a313b
- CEventPotentialWalkIntoVehicle::AffectsPed (exe 0x4AE420): SetNewVehicle(veh, 0) -> SetNewVehicle(veh, task m_DirectionGoingRoundCar). b09f914d3
- CEventVehicleCollision::AffectsPed (exe 0x4B2EE0): same SetNewVehicle direction argument. e3a7115cf
- CPlayerPed::DoesPlayerWantNewWeapon (exe 0x609710): jetpack task no longer returns false (exe jetpack branch always falls through to the ped-state check). 94d8369d0

Cosmetic, not changed: CEventDeadPed::AffectsPed (0x4B4830) exe keeps distance components unrounded on the x87 stack.
Build: gta_reversed objects of the 4 changed files compile in StandaloneRelease (full link/tests not run).
Tip: queue rows with exe-address comments in the port body were already compared by earlier passes (mostly noise); unannotated rows (see queue) are the better targets.
