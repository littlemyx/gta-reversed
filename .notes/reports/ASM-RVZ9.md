# ASM-RVZ9: independent exe review of HC8e
CONFIRMED against exe (instruction level): CPickup::Update disabled branch (537dd640a; reachable identically via bDisabled bit), GenerateRoadBlocks (a76d08a7f), CCam::Process climb reset (de8c74e2d), CPlane::ProcessFlyingCarStuff smoke dist (b2dbb0178), FollowPointRoute fallback loop (064d905f9). None WRONG.
Extra: FollowPointRoute main loop (0x66E3C3-0x66E513) re-read: FIXED b84af3045 (sqMag (z2+x2)+y2, ped-proj dist z,y,x, NaN dot/length skip). Dot, Normalise, proj, best^2 compare matched.
Build StandaloneRelease OK; no tests/regress run (left to the gate).
