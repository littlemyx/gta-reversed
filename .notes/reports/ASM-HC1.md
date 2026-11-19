# ASM-HC1: executed-but-unverified functions, zone source/game_sa/Entity/** (RYDER_1 hit counts)

Zone rows (exe_addr != "-", verdict NONE, Entity .cpp objs): 64 tsv rows (63 distinct functions; CEntity::Add / IsEntityOccluded / DrawTriangleForMouseRecruitPed already had rows).
Done: 62 compared (2 FIXED, 60 MATCH). SKIP-RENDER: CVehicle::RenderDriverAndPassengers.
No row of a zone class lives outside the Entity .cpp objs.

Fixes
- a71789b11 CEntity::GetIsTouching(CEntity*) (0x5343F0): sum order (dz^2+dy^2)+dx^2.
- dac6bef9f CPed::GetBikeRidingSkill (0x5DF510): clamp is 1.0 (port 1000.0).

Build: StandaloneRelease relinked OK (Entity.cpp, Ped.cpp). Tests/regress not run (no game runs in this slice).
