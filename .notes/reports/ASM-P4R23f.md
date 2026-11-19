# ASM-P4R23f: review of P4R23e + ped/world rows with exe comments and no verdict

Reviewed the P4R23e fix (FindDistanceToNearestPedOfType): correct (strict <, float best, start 1e7).
Compared 16 more functions (Garage, PathFind, Pad, CarCtrl weave, Stats, Pickups, Restart, EntryExit): 15 MATCH, 1 FIXED.
Fix: CPathFind::ReturnInteriorNodeIndex (exe 0x451300) took a leading unused arg; exe has (intId, nodeId) after the hidden return pointer (38152c0bf). No callers in the port.
Not reached: WeaveThroughPeds/Objects, TestCollisionBetween2MovingRects_OnlyFrontBumper, FindNodeOrientationForCarPlacement, SwitchRoadsOffInArea*, CompleteNewInterior, FindParkingNodeInArea, AddDynamicLinkBetween2Nodes_For1Node, BuildRotatedDoorMatrix, ManageAllPopulation.
