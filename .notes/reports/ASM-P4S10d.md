# ASM-P4S10d: review of 4b43a2a6c + P4S10 queue continuation

Part A: CTaskComplexHitPedWithCar::CreateNextSubTask (0x657AF0-0x657B53) CONFIRMED (type 414 finishes; hurt/fall branches and hit-side table also ok). CTaskComplexGoToPointAnyMeans::CreateSubTask follow-node tail CONFIRMED argument by argument (3.0/2.0/-1/true).
Part B: 16 more queue rows compared: 15 MATCH (incl. 1 with float-vs-x87 compare noise), 1 NOISE (LookForCarAlarms sum order), 0 FIXED. GetCameraTargetPos not reached; the rest (~197 rows) NOT_REACHED (budget).
No port changes, so no build was needed (no .cpp changed).
