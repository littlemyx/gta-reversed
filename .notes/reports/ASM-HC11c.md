# ASM-HC11c: compare-strictness / NaN class, continuation of HC11/HC11b

Compared: ~25 inlined float min/max/clamp sites in 12 hit functions and the helper (0x404330/0x420800) sites in Stats (5), Rope, CollisionEventScanner, GangFollower, PointLights. Rows: .notes/asm_sweep/HC11c.result.tsv.

Fixed (NaN / signed-zero only, one commit per function): CAutomobile::ProcessEntityCollision (wheel compression min, `delta <= 0.1` skip now also skips NaN, min(0.2,delta)) 4ce8bcbd6; CAutomobile::VehicleDamage (3 sites) 07c28cd9d; CAutomobile::ProcessBuoyancy (4 sites) a81c04a5a; CBike::ProcessBuoyancy (3 sites) 73a236f12; CBike::ProcessControl wheel counter (exe stores +0 for NaN/-0) b668d9acf; CPlayerPed::SetPlayerMoveBlendRatio 5a84beffd.

MATCH: cTransmission::CalculateDriveAcceleration, cBuoyancy boat wave power, Bike shake/speed mins, PedCreationDistMultiplier, all of CWeather::Update + AddRain, PlayerPed::ProcessControl gun spin, CPhysical clamps, all helper sites in Stats, Rope, CollisionEventScanner, GangFollower, PointLights (operand order confirmed: the last-pushed arg is `a`). Not hit in RYDER_1 (skipped): Prostitute, JetPack, Swim, WalkAlongside, QuadBike, HandleSprintEnergy; rhino extra wheel min in ProcessEntityCollision not compared.

Rules for the TS port: the same as HC11 (explicit ternaries in exe operand order, never Math.min/max). Exe idioms decoded here: `fld K; fcomp v; test ah,0x41; jne keep` = `K > v ? K : v`; `fld K; fcomp st(1); test ah,5; jp keep` = `K < v ? K : v`; `fcom v,0; test 0x41; je keep` = `v > 0 ? v : 0` (NaN -> 0).

Build: StandaloneRelease links clean; no game run (regress left to the gate).
