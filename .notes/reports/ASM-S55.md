# ASM-S55 (PedPlacement, PedStats, PedStuckChecker, PedTaskPair, PedType, Pickup.cpp, Pickups.cpp) - 2026-10-29
Result: 40 rows: 12 FIXED (12 commits), 26 MATCH, 0 UNSURE, 2 OPEN (RenderPickUpText 0x455000, DoPickUpEffects 0x455720: not compared, budget). Table: S55.result.tsv.
Fixes (exe address, one line each):
- 6b81666e3 FindZCoorForPed (0x616920): ground accepted only above -99.0 (port: >=).
- 11c38b02d IsPositionClearOfCars(CPed*) (0x616A40): non-AUTOMOBILE sub-type or big vehicles get the colmodel test, a normal automobile is returned as hit (port: the reverse, null).
- 10d0e06b8 PickUpShouldBeInvisible (0x454D20): cutscene always hides; kill-frenzy model hides on mission/frenzy/!KillFrenzy; PICKUP_2P hides when player 2 exists; second German spot (2509,1208); coop test was reversed.
- a780bb6af GiveUsAPickUpObject (0x4567E0): objectFlags bit 1 is the pickup's bEmpty (port: bCollisionProcessed); UpdateRwMatrix before UpdateRwFrame, no SetHeading; out ref cleared first; null Create bails; signed ammo/5.
- 890522581 / 1e2a1cd0f PickedUpHorseShoe / PickedUpOyster: rewards go to Players[0] (0xB7CE50).
- 3153e61e2 PictureTaken (0x456A70): a photo pays 100 to Players[0] (port paid 100000 to the focused player); z,y,x distance sum.
- 435d5ca9f ModifyStringLabelForControlSetting (0x454B70): 2-char label skipped (jle 2).
- 7c64bf7de TestForPickupsInBubble (0x456450): strict sqrt((dz^2+dy^2)+dx^2) < r.
- 3af399f91 GivePlayerGoodiesWithPickUpMI (0x4564F0): playerId 0 uses the focused ped (jle).
- 0f93b4be9 / 8f92c874a DoCollectableEffects / DoMineEffects: z,y,x distance sum, exe association of the intensity product; mine corona far clip 60.0 (port 40.0).
MATCH highlights: CPedStuckChecker::TestPedStuck (all arms, constants, probe flags, Normalise 0x59C910), PedStats/PedType load/init, ExtractAmmo, RemovePickUpsInArea, ReInit, RemoveMissionPickUps.
Notes: PedType::LoadPedData keyword compare / initial pedType and PedStats name copy differ only for malformed data files (not changed). CPickups::Update 0x458DE0 is outside the slice list; CPickup::Update was covered by S31g.
Open: RenderPickUpText, DoPickUpEffects.
