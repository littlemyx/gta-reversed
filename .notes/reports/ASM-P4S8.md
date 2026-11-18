# ASM-P4S8

No C++ changes (no function needed a fix).

- CVehicle::UpdateTractorLink (exe 0x6E0050): MATCH.
- CPhysical::ApplyFriction other overload (exe 0x545980): MATCH, all four branches.
- CTaskSimplePlayerOnFoot::ProcessPlayerWeapon (exe 0x6859A0): NOT_REACHED; first ~470 of 1772 insns (to 0x686300) match. The P4T1 SetTask/AddSubTasks hint is a naming artifact (0x681AF0 is SetTask).
- CVehicle::FlyingControl (exe 0x6D85F0): NOT_REACHED.

Nothing to forward to the TS port.
