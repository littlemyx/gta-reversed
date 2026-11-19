# ASM-P4R12b: never-audited root-file leftovers (gameplay first)

10 functions compared against gta_sa_compact.exe: 9 MATCH, 1 FIXED, 0 UNSURE.

- FIXED CTagManager::GetNearestTag (0x49D160): exe iterates tags last to first with a strict `<`, port iterated first to last (ties resolved to the other tag).
- MATCH: FindNthNodeClosestToCoors, CReplay::CanWeFindPoolIndexForVehicle, CGangs::Initialise (values), CRestart::Initialise, CPad::Clear, GeneratePlateText, CFireManager::Init, C3dMarkers::Init, CPickups::DoMoneyEffects.
- NOT_REACHED (budget): remaining ~55 rows of the list (CullZones/EntryExitManager Init, Shopping Store/RestoreVehicleMods, Cover, Birds, Buoyancy, Messages, ControllerConfigManager, CdStream).
