# ASM-S14 (41 TaskSimple* functions) - 2026-10-27
Result: 18 compared (4 FIXED in 4 commits, 14 MATCH, 0 UNSURE), 23 NOT REACHED (token budget; table in S14.result.tsv).
Fixes (exe address, one line each):
- 976739d8 CTaskSimpleRunTimedAnim::ProcessPed (0x61BC4C-0x61BC62): the exe starts the timer unconditionally; CTaskTimer::Start refused a negative int32 duration, so the timer stayed off and the task never ended (sunbathe with timeLeft()+1000 < 0).
- 53e0bf4a CTaskSimpleKillPedWithCar::DamageCarBonnet (0x653D3B-0x653E2A): the bonnet gets the PED's move speed +/- right*0.1 + up*0.5 (up added on both coin results; port subtracted it on tails and used the object's own speed).
- a1135a60 CTaskSimpleKillPedWithCar::ProcessPed (0x6564FE-0x656A66): ped-in-car-space used TransformPoint (car->world + car pos) instead of dot products; going-forward is dot>0 (port >=); back-lifted branch uses max.z (port max.x); nose-up divisor chosen by travel direction (port by max.y<=0); last branch is carDir FROM_FRONT + RUNOVERBYCAR (port FROM_BACK + RAMMEDBYCAR); strict -0.2 / 0.1; shake and force float order.
- 7910ea4e CTaskSimplePlayHandSignalAnim::StartAnim (0x61B11C-0x61B134): the right hand object played the LHAND group's anim; the exe uses ANIM_GROUP_RHAND.
MATCH: Land::ProcessPed, OnEscalator::ProcessPed, PickUpEntity ctor-1, PlayHandSignalAnim::ProcessPed, RunAnim StartAnim/ProcessPed, RunAnimLoopedMiddle Ctor2/StartAnim/ProcessPed, RunNamedAnim StartAnim/Clone/ProcessPed/OffsetPedPosition, RunTimedAnim::StartAnim.
NOT REACHED: Jump x4, PickUpBike x3, PlayerOnFire, PlayerOnFoot x7, Say, SetPedAsAutoDriver, ShakeFist x2, SitDown x2, SitIdle x2.
Note: CGeneral::DoCoinFlip (rand >= 16383) vs the exe's inlined rand()/32767 > 0.5 (rand >= 16384) differ only at rand()==16383; not fixed.
Consumers: fixes are local to the tasks' own update (RunTimedAnim subclasses Chat/HandsUp/LookAbout/Sunbathe only gain a working timer on negative durations).
Gate: no oracle row covers these functions (only KillPedWithCar Clone/Destructor, skipped); Release build, soak: see below.
