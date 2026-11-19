# ASM-P4R2g: C++ vs exe where TS diverges (20.11)

1. rng frame 54 (GenerateCarCreationCoors2, 8 vs 13 nodes): every function on the walk is MATCH against the exe (GenerateCarCreationCoors2 itself S22c; now also InitSequence, CCarPathLink::GetNodeCoors, ThisNodeWillLeadIntoADeadEnd, Find2NodesForCarCreation, RecordNodesClosestToCoors, FindNodeClosestToCoors, FindNodeClosestInRegion, CalcDistToAnyConnectingLinks, These2NodesAreAdjacent, IsSphereVisible 2-arg wrapper). The path loader was MATCH earlier. The C++ logic is the exe logic; which of 8/13 the exe gives depends on node data and the rng stream, not decidable from code. Suspect the TS side (start node, link scan order, or node/link data it feeds).
2. CAutomobile::IsDoorClosed / U32: MATCH (angle based via CDoor::IsClosed, jump table decoded).
3. Car entry: the remaining unswept function was CTaskSimpleCarAlign::FixHeading (0x645CB0): FIXED (sum order, NaN, and the exe scales only x,y of the car vector by the dot and subtracts it twice). The other entry tasks were already MATCH/FIXED in earlier slices.
4. CEntryExit ctor coin flip: the exe has no srand; unknown-burglary does rand() < 0x3FFF (-> window 0,0), the C++ does the same. The TS srand(PTR_ID32+days) form has no exe counterpart.

Build: StandaloneRelease relinked OK. Tests/regress not run (brief: no game runs).
