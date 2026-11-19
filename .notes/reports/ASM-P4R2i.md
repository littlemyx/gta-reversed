# ASM-P4R2i
Part A: 684911aa6 CEntity::CreateEffects ENEX heading CONFIRMED against exe 0x533A07-0x533A87.
Part B (revised): cutscene clock (Update_overlay accumulation 0x5B17B8: float step*0.02f, *1000, ftol; matches port under PC24; HasCutsceneFinished/Update already MATCH) and NPC enter-car timing (GoToPoint::ProcessPed, CloseDoorFromInside::ProcessPed, GoToCarDoorAndStandStill ctor/MakeAbortable/CreateSubTask all MATCH; rest already FIXED). 0 source changes, so no build. The GUNS1 2-frame / Ryder 3-frame divergence is not in these C++ functions as compiled; likely TS-side (RR timer path, route/anim timing). Camera list NOT_REACHED.
See P4R2i.result.tsv.
