# ASM-P4TS3: P4TS2 review + last 5 TS-audit rows

Oracle: gta_sa_compact.exe 1.0. Part A: all 5 P4TS2 fixes re-read line by line against the exe: CONFIRMED (no commit needed).
- GET_CLOSEST_CHAR_NODE 0x47FB7C: ped node position = CPathNode::GetPosition (0x420A10); compare flag true / false (zeros).
- PlacePedAtEffect: the heading vector is fx+0x28 (ComputeEffectForwardDir 0x5E9780; the call at 0x5EA47C feeds edi/ebx -> GetRadianAngleBetweenPoints at 0x5EA56B).
- CPedPizzaAttractor ctor 0x5EE6E0: push 5, 1.0, 30000, 3000, 0.2 (0x86C458), 0.1 x3.
- UseAttractor ATM 0x63276E: int16 money += 20 - 20 * (int)((rand & 0xFFFF) * 2^-15 * -50).
- SimpleVariablesSaveStructure::Construct 0x5D1E2A: german (B9B7ED) -> +0xED, french (B9B7EE) -> +0xEC; Extract reads the same slots.

Part B (5 rows):
- CHandShaker::Process 0x50D930: FIXED (4 deviations, all as TS said): (1) scale*=slow only for strictly same-sign non-zero ang/vel (port: signbit equality, multiplied for zeros); (2) the 3 motion draws go z,y,x; (3) twitch test is GetRandomNumberInRange(1, ftol(freq*twitchT)) == 2 (port: 1-arg range == 1); (4) twitch vector draws z,y,x. Scale uses span = max-min then |ang/lim|*span+min (not std::lerp).
- CCam::Process ped-cam-in-car: TS-only runtime addition (Camera.ts aimFromCar); the exe has no weapon-mode read in the vehicle branch of CamControl either (see tsv), C++ already matches. No change.
- MissionOps61_90 IsPointInAngledAreaOriginal, FollowPatrolRoute::CreateFirstSubTask, GoPickUpEntity::CreateFirstSubTask: the port already carries the PC24 chains (x87d / X87R, commits d65c1985e, 2aa955846); step-by-step identical to the TS. MATCH.

## For TS: TS is wrong here
- Nothing wrong in the numeric chains. Only the audit rows were stale (C++ tip already x87d); Camera.ts aimFromCar is a TS-only deviation (the exe has no such branch), keep it documented as DEVIATION.

Build: HandShaker.cpp compiled + StandaloneRelease linked (ninja, EXIT 0). No game run.
