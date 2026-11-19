# ASM-P4R11: rand-formula sweep 3

Review of 1bf1e7799 (CVehicleRecording::SetRecordingToPointClosestToCoors, 0x45A1E0): CONFIRMED, `mov [esp+8], 0x497423FE` = 999999.875f.

Sites checked against the exe: 28 functions (~110 draw sites; see P4R11.result.tsv). 3 fixes, all one class: the port used the float
`CGeneral::RandomBool(p)` (`range(0,100) <= p`) where the exe uses the int roll `ftol((rand&0xFFFF)/32768*100) < p`, or `rand/32767 < 0.2f`.
- e629899ba CPopCycle::FindNewPedType 0x60FBD0 (police-station 70%)
- b3015a4a8 CTaskSimpleSitIdle::ProcessPed 0x637A60 (5% look, 25% closest-ped roll)
- 93afb7744 CTaskComplexInvestigateDisturbance::CreateFirstSubTask 0x66C820 (20% say-context)
Draw order/formulas verified MATCH: ShotInfo, Explosion (AddExplosion, Update), CarGenerator, Birds, Entity effects, footsteps, ProjectileInfo, GangWars (attack/defend), Population (couple, AddPed, AddToPopulation), tasks (BeInCouple, CarDrive, FleePoint, SmartFleePoint, GangHassleVehicle, Facial).
Not reached (~250 sites, most already carry exe-address comments from earlier sweeps): CarCtrl bulk, Automobile, Cam, Weapon, Clouds, remaining tasks.
Build: StandaloneRelease ninja -j4 link OK (4 cpps). No game runs, no tests run.
