# ASM-S70 (world objects: Door, BouncingPanel, Escalators, Glass, Birds, BreakManager/BreakObject, FallingGlassPane)

Compared 11 of the 42 rows against the exe; 31 rows NOT_REACHED (budget): Birds x3, BreakManager_c x6, BreakObject_c x7, FallingGlassPane x2, Glass x12 (all except WindowRespondsToExplosion), CEscalator::AddThisOne.

Verdicts: 4 FIXED, 7 MATCH, 31 NOT_REACHED.

Fixes
- 267ae380a CBouncingPanel::ProcessPanel 0x6F49A0: angular velocity = speed - (moveForce + turnForce x compPos); the port did (speed - move) + compPos x turn (different fp association; the cross operands are (turnForce, compPos)).
- 79e7f4eaf CDoor::Process 0x6F4040: direction rotation is (cos*a - sin*b, cos*b + sin*a); the port had the first component negated, i.e. the door acceleration sign was wrong for every door with axis 0/2. Axis 1 is not rotated (exe leaves sin on the FPU stack as `ang`).
- 610726bd3 CGlass::WindowRespondsToExplosion 0x71C1A0: length is sqrt((dz^2+dy^2)+dx^2); `dist < 10` ordered (NaN to the >= 10 arm).
- 3b58f3489 CEscalator::SwitchOff 0x717860: the step object slots are cleared (port nulled a loop copy; dangling pointers stayed in m_StepObjects).

MATCH (details in S70.result.tsv): CBouncingPanel::ResetPanel; CDoor::ProcessImpact (jump table, rand*c*0.75+0.75, NaN arms); CEscalator::Update (create/switch-off radius +20/+23, step 0.04, intermediate/bottom/top dir and t, speed 0.016, pool free > n+10); CEscalators::Init/Shutdown/AddOne/Update.

Open: Glass.cpp (BreakGlassPhysically, GeneratePanesForWindow, WindowRespondsToCollision, CarWindscreenShatters, WasGlassHitByBullet, Update, Render*), BreakObject_c/BreakManager_c, Birds, FallingGlassPane, CEscalator::AddThisOne (a prior comment already records a trace of 0x717A53-0x717AD5).
