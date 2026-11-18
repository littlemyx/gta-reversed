# ASM-P4S10c: review of cf31c2587 + continuation of the unannotated P4S10 queue

Part A: cf31c2587 CTaskComplexInWater::CreateFirstSubTask (0x635200) - CONFIRMED line by line (Die on bDrownsInWater SET; Find(false) == CPedIntelligence::FindTaskByType 0x600EE0; ctor args for Die/Swim/RunAnim verified).
Part B: 24 more queue rows compared (+ the Part A row): 21 MATCH, 1 FIXED, 1 NOISE (GetNearestFire sum order), rest already annotated. Remaining ~213 rows of the 250 NOT_REACHED (budget).

- CTaskComplexHitPedWithCar::CreateNextSubTask (exe 0x657AF0-0x657B53): sub-task type 414 (TASK_SIMPLE_HIT_BEHIND) now also finishes the task (was returning nullptr) -> added case.
