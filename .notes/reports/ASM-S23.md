# ASM-S23 (Population.cpp, 33 functions) - 2026-10-26 18:05 .. see hand-back
Result: 20 compared (9 FIXED in 8 commits, 11 MATCH), 1 UNSURE note, 13 NOT REACHED (token budget: AddPedAtAttractor, ConvertToRealObject,
AddToPopulation, GeneratePedsAtAttractors, GeneratePedsAtStartOfGame, ManageObject, ManageDummy, ManagePopulation, RemovePedsIfThePoolGetsFull,
ConvertAllObjectsToDummyObjects, PopulateInterior, LoadSpecificDriverModelsForCar, Update) -> slice is PARTIAL; Update/ManagePopulation/AddToPopulation are the valuable rest.
Fixes (all with exe addresses in the commit messages; table in S23.result.tsv):
- CreateWaitingCoppers: missing siren flag + rand, swapped RNG order (distance before heading), ped z missing +1, no-car-model skip.
- AddPedInCar: cop cars now pass the COP TYPE to AddPed (SWAT/FBI/army cops got no weapons/armour), literal ped types, criminal UpdatePedCount order.
- AddPed: cheat weapon picks (rand()%5, int gang rolls), dildo ammo 0, golf-club cheat target.
- TestSafeForRealObject: sector range/clamp/scan-code semantics. AddDeadPedInFrontOfCar: camera check + null ignore slots. LoadGroup, ManagePed delta, PlaceMall order.
Consumer audit: CanCameraSeeAPedHere only used by AddDeadPedInFrontOfCar; PedPlacement ignore-list overload only used there; LoadGroup shared by ped/car groups
(same asm shape, capacities 21/23 verified). UsesSiren flag bit verified at 0x42D bit 7.
Not verified: no oracle target exists for Population (D/S classes); gate = Release build + soak (see hand-back). UNSURE (not changed): ManagePed removal-distance
thresholds are written as dist/PCDM <= thr (exe: PCDM*cam*thr < dist) - 1-ulp / equality differences only.
