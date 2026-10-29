# ASM-S42b (phase 2: CCarAI::UpdateCarAI + CarEnterExit helpers) - 2026-10-29
Result: 19 rows. CCarAI::UpdateCarAI FIXED (decisions + compares compared, one commit 968e04e1; arithmetic/steering tail 0x41FE13-0x420678 left open = PARTIAL); 18 CCarEnterExit rows NOT_REACHED (budget went into UpdateCarAI). Table: S42b.result.tsv.

Method: decoded both jump tables from the exe (status table 0x42067C, mission table 0x420694 + index bytes 0x420708, tail table 0x42076C, driving-style table 0x42075C) and read every fcom/fcompp with its fxch/test-ah/jump polarity.
Systemic finding: the port compared squared distances with `<=`/`>=`; the exe computes the sqrt'd 2D distance (y*y + x*x) and tests it against the float switch distance with strict `<`/`>` (or `Far < dist`). All sites now use Magnitude2D() with the exe's polarity. The "player" distance uses FindPlayerCoors (vehicle position when driving), not the ped position.

Fixes in 968e04e1 (exe addresses):
- 0x41DAB6: PLANE_ATTACK_PLAYER_POLICE reset destination is (-6000, +10000, 500) (port y was -10000) and the exe also sets mission PLANE_FLYTOCOORS (0x16).
- 0x41DF54: MISSION_BLOCKPLAYER_FARAWAY case was missing (went to `default`); same code as RAMPLAYER_FARAWAY with mission 5.
- 0x41DB72/0x41DF54: EntitiesGoHeadOn gets FindPlayerEntity (vehicle if driving), not the ped.
- 0x41DC92/0x41E07D: BLOCKPLAYER_CLOSE far branch falls back to mission 4 (port 2); the near/stop paths also run MellowOutChaseSpeed + BackToCruisingIfNoWantedLevel for law enforcers (port only after the far branch); the cops timer is ftol(ts*16.666666 + timer) as a whole; speed tests are Magnitude() < 0.05 / 0.04.
- 0x41E88D BLOCKCAR_CLOSE: threshold is FindSwitchDistanceFar (port Close) and the exe sets mission BLOCKCAR_FARAWAY (0x11) first.
- 0x41EE76 KILLPED_FARAWAY: switches to CLOSE when dist < Close (port tested Far, inverted).
- 0x41E642 RAMCAR_CLOSE: BackToCruisingIfNoWantedLevel when the target is the player vehicle (missing); collision speed test Magnitude() < 0.04.
- 0x41EB46 SLOWLY_DRIVE_TOWARDS_PLAYER_1: BackToCruisingIfNoWantedLevel always called after the test (missing).
- 0x41EBB9 SLOWLY_..._2: exe runs two consecutive "leave" tests (dist < 13, then dist > 70 / everybody-back-off / law + wanted 0 / PoliceBackOff() / NoPolice), so TellOccupantsToLeaveCar can run twice; >70 is strict; wanted level read through FindPlayerPed.
- 0x41F12D APPROACHPLAYER_CLOSE: timer threshold is `timer > 2500/20000` (port >=), dist < 10 strict, far test is `!(Far < dist)`.
- 0x41FABB tail: `CCullZones::PoliceAbandonCars` (0x72DD60), port called CamCloseInForPlayer.
- 0x41FAFD/0x41FBBA: exe float thresholds 0.00250000018 (strict >) and 0.000144000005 (strict <).
- 0x41FBDF: stuck-check interval is ((seed & 15) + 40) * 500 (port 500*(seed%16)+40), compared signed.
- 0x41FC57: reverse-while-cruising duration 700 ms (0x2BC), port 750.
- 0x41FD5A: real-physics switch uses `prevTime - t <= 30000` (port <).
- 0x41FDF3: up.z < -0.7 strict and the temp action is WAIT (1), port set REVERSE.

Open (PARTIAL): 0x41FE13-0x420678: steering-to-hit block (exe compares veh |speed| < player |speed|, player |speed| > 0.1, dot > 0, 0.5*M2D(vel) vs M2D(dir) forms, thresholds 12/20/0.8/0.6/0.2 via Magnitude not squared), horn rand 0x4203C1, speed-mult 0x4203F0, water/land 0x420445, gang cruise-speed 0x420559. Also FORWARDANDBACK (0x41ED1C): the exe normalises with a 0.001 length guard and 1/len multiply; the port's Normalized() was not compared. The port's squared-magnitude forms in that region (e.g. `plyrSpeed3DSq >= sq(0.1f)`) very likely have the same sqrt/strictness deviation and need the same treatment.
CarEnterExit (all 18 rows) not started.

Gate: Release build OK; wave2_oracle_test (1142 functions) PC24 0/0, PC53 0/0 (RW target: ninja "no work to do" after the last edit, it does not depend on CarAI.cpp); combat regress PASS=14; PortableWin64 compile of CarAI.cpp.obj: 0 errors; v6 pre-commit grep clean. Trailer uses the brief's Opus 5.5 text.
