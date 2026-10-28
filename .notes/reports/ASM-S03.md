# ASM-S03 (Tasks/TaskTypes: DragPedFromCar .. FleePoint) - 2026-10-28
Result: 30 of 43 compared in full (9 FIXED in 8 commits, 21 MATCH, 0 UNSURE as a verdict), 13 NOT_REACHED (listed in S03.result.tsv; hand over to S03b: the 8 CTaskComplexEnterCar functions, FleeAnyMeans::ControlSubTask, FleeEntity x2, FleePoint::ComputeTargetPoint / ControlSubTask).
Fixes (exe address, one line each):
- 1c0a7974 CTaskComplexCarDriveWander::SetUpCar (0x63CB60): engine-broken branch also cleared bIsLawEnforcer (exe clears only bEngineOn).
- 8800bfdf CTaskComplexEnterCarAsPassengerWait::ControlSubTask (0x640730): moved-target test in extended precision, (dz2+dy2)+dx2.
- 07970a61 CTaskComplexEvasiveDiveAndGetUp::CreateNextSubTask (0x656000): exe limits the rotation first and then subtracts pi/2; the port subtracted before limiting.
- af46349e CTaskComplexExtinguishFires::ControlSubTask (0x698330): m_fireToExt = nearest fire when the go-to-point subtask is retargeted (store was missing).
- d9fa05a5 CTaskComplexFacial::ControlSubTask (0x690FC0): restarting the running facial task's timer is unguarded (StartTimer now StartUnguarded).
- 36cf9929 CTaskComplexFallToDeath::CreateNextSubTask (0x679270): both vertical-speed tests inverted (in-air to land: |z| >= 0.1 picks KO anim; land to restart in-air only when |z| >= 0.01).
- 61ebe2b0 CTaskComplexFallToDeath::CalcFall (0x6796C0): shuffle uses rand() & 3; all three line-of-sight tests were negated.
- b76ae577 CTaskComplexFleePoint::CreateNextSubTask (0x65D150): danger test is |fleeTo - fleeFrom|^2 vs safeDist^2 (port used the ped position), extended precision, NaN counts as inside.
UNSURE (not changed): CalcFall fall-position z adds (0.375+pz)+dz*1.5 with unspilled extended intermediates (last-bit only). DriveFireTruck::CreateFirstSubTask/ControlSubTask test ped->m_pVehicle in the exe, the task vehicle in the port (differs only when the task vehicle died; exe would crash).
Consumer audit: no signature or layout change; TaskSimpleFacial::StartTimer has the single caller CTaskComplexFacial::ControlSubTask.
Gate: see the handback message (Release + RW builds, wave2_oracle_test, gameplay_regress combat).
