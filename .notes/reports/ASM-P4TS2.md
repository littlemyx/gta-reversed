# ASM-P4TS2: review of the 14 P4TS1 fixes + the 13 TS-audit rows P4TS1 did not reach

Oracle: gta_sa_compact.exe 1.0. No game run, no tests run (build check only).

Part A (14 P4TS1 fixes): 13 CONFIRMED line by line, 1 WRONG: GET_CLOSEST_CHAR_NODE used FindNodeCoorsForScript; the exe (0x47FB7C) uses CPathNode::GetPosition for the ped node (FindNodeCoorsForScript is only the CAR-node command at 0x47FC76). Fixed, flag logic kept.
Part B (13 rows): FIXED 4 (PedPlacer heading from forward dir, Pizza ctor 5 peds / 0.2, ATM money formula, SimpleVariables french/german swap), MATCH/no effect 4 (Shelter heading, SwatRope order, AttractorManager order, GenericLoad already right), NOT_REACHED 5 (CCam::Process, HandShaker, MissionOps61_90 / FollowPatrolRoute / GoPickUpEntity x87 rounding).

## For TS: TS is wrong here
- Nothing in the 13 rows: TS was right on PedPlacer heading, Pizza ctor, ATM money, language flags (germanGame at +0xED).
- Only note: GET_CLOSEST_CHAR_NODE needs the raw node position (CPathNode::GetPosition); only GET_CLOSEST_CAR_NODE goes through FindNodeCoorsForScript. If TS routes the ped command through FindNodeCoorsForScript, that is wrong.
- CPedAttractorManager lookup order / SwatRope check order: no observable effect (a ped is in one attractor; pure compares), so TS 'cpp-still-differs' there is harmless.
- GenericLoad version compare: the C++ run build already has none (FIX_BUGS only).
