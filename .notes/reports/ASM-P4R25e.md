# ASM-P4R25e
5 of the 6 rows compared against the 1.0 exe; the Racing head remains open.
- MATCH: TestCollisionBetween2MovingRects, CompleteNewInterior, CPathFind::Init, SteerAICarBlockingPlayerForwardAndBack.
- FIXED: CWeapon::Update - reload-anim audio test (`rload*0.001f`, `cur-step`) and the `cur/total < 0.9` progress check were rounded to float in the port; exe keeps them in x87 registers (0x73DD25-0x73DDD4).
- PARTIAL: SteerAICarWithPhysicsFollowPath_Racing - tail 0x4361F6-0x436537 MATCH; head 0x435830-0x4361F6 NOT_REACHED (budget).
