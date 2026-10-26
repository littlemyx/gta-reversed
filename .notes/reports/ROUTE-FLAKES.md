# ROUTE-FLAKES (Windows, rules v5, 2026-10-26 02:27-~03:50)

Both flakes are SCENARIO ARTEFACTS (no port bug found); fixed in the routes, no check loosened. One NEW finding (heap corruption after `cmd:tp` in the soak route) is NOT fixed - see 3.

## 1. soak `until:invehicle TIMEOUT after 8000 ms` (6 of 19 gate soaks 10-23..10-26)
Evidence (gate_soak/1026-0202 and 1025-1600, mark_car_enter.png): the 3-minute walk of soak_route.txt ends at a RANDOM place (pavement, the rock slope above the alley, against a fence, once CJ lying on the road).
The cheat car is placed at `radius + 2` m in front of CJ at CJ's z (CCheat::VehicleCheat, exe 0x43A0E2 ff., already matched to the exe) and then ClearSpaceForMissionEntity/PlaceOnRoadProperly: on a slope or in a fence the Rancher sits half inside the
rock / tilted on the picket fence, CJ's single F cannot reach a door. The route's designed answer (sidestep, second cheat car, F) worked every time (`invehicle ok after 5.4 s`), but the first `until:invehicle:8000` logged a TIMEOUT that soak.sh counts as a failed gate.
Tried and rejected: `cmd:tp:2100:-1790:13.5` before the cheat (pizza-lot spawn point of the combat scenario): it faces the pizzeria fence (tp-3: Rancher on the fence, same miss) AND see 3.
Fix: injector `until:COND:MAXMS:soft` (SDLWrapper.cpp; a miss logs `MISS(soft)`, not `TIMEOUT`); soak_route.txt: attempt 1 and a NEW attempt 2 (sidestep the other way, 5 s, cheat again) use `:soft`, the last attempt keeps the hard gate
(a run where CJ never gets into any car still reports TIMEOUT). docs/STANDALONE.md updated.
Also new run-build diagnostics: `cmd:carinfo[:RANGE]` (player pos/state + every vehicle within range: pos, z-rel, status, driver), `cmd:heal`.

## 2. gameplay_regress combat: hit.damage_event `1 -> 0/0`, car.driver_dies `driver hp=-1` (gate 1025-2244)
From [pc-dmg] of C:/gta-scratch/gr_out/20261025-225027/combat/standalone.log: the 70 s police chase is random and leaves CJ at 0..67 hp (health+armour min over 20 logged runs: 100(never hit, FAIL), 67, 58, 53, 50, 34 x4, 25, 17 x2, 9 x2, 1, 0 x3 ...);
in that run CJ had 1 hp (9 pistol hits of 8 from cop 162B7C20/745C/554C), the scenario's `hitplayer:22:10` (step B) then killed him (`player hp 1 -> 0`, lastDamager = the spawned cop 162B8BA8), wasted -> hospital respawn,
the police car spawned in step C went with the respawn clean-up / the respawned CJ stood elsewhere -> `driver hp=-1 state=-1` (no driver ped). Not gangs-with-leaders: the damagers are cops (ped type 6).
Fix (routes/combat.txt): after the kill step and `until:control`: `cmd:wanted:0`, `cmd:shoot:6:200` (cops that spawned after the kill pass), `cmd:heal` (new run-build command: hp 100, armour 0 = the new-game start), then the A/B/C steps.
Checks untouched (hit_drop_min stays 2).

## 3. FINDING (open): heap corruption (0xc0000374, ntdll fault offset 0xfd71f) in the soak route with `cmd:tp` after the walk
Soak route + `cmd:tp:2100:-1790:13.5; wait:3000; until:control` inserted before `mark:cheat_car`, exe = main checkout gta_reversed.exe of 02:02 (HEAD da5c6902), NOVSYNC, windowed, private data dir:
tp-1 crashed ~10 s after the tp (right after `Reading block (COACH)` / `Created #40 associations for block (coach)`, car drive), tp-2 at the quit sequence (after quit_ask), tp-3 at idle start. 3 of 3 runs, Windows Application log APPCRASH c0000374 x3.
The same exe with the unmodified route: notp-1, notp-2 RESULT OK (and 0 crashes in 19+ gate soaks). So `tp` (CStreaming::LoadScene + LoadSceneCollision + Teleport + LoadScene + LoadAllRequestedModels(false), SDLWrapper.cpp InjRunCmd) 3.5 min into a new game corrupts the process heap;
the gameplay_regress scenarios use the same tp early in a game without crashing. Not investigated further (token/time cap): next step = repro route in scratch (C:/gta-scratch/rf_route_tp.txt = soak_route.txt + the 4 tp lines), cdb with the debug heap (a process started under cdb gets tail/free checks) to catch the first bad write.
Evidence: C:/gta-scratch/rf_out/tp-1..3 (standalone.log, log.log, report.txt).

## Runs (private data dir C:/gta-rf, silent, windowed, NOVSYNC; new build of this worktree; C:/gta-scratch/rf_out/summary.txt)
* Before (gate soaks 10-23..10-26): 6 of 19 with `invehicle TIMEOUT` (1-2 each). Same exe, unmodified route, today: notp-1/2 OK, 0 timeouts.
* After: soak new-s1..s5 RESULT OK, 0 gate timeouts; s4 had one `MISS(soft)` (first F missed, retry `ok after 5.5 s`) = the artefact, handled; combat new-c1..c5 PASS (14/14 each, 0 FAIL) - c1 even had CJ killed in the chase (hp 1 -> 0) and the scenario still passed after the heal.
* NOT reached: the brief's 10 soaks + 6 combat (5 + 5 done: 90-minute cap, GPU shared with REPLAY-DIFF). The 1-in-4..5 miss rate of the first attempt is the old ~30% rate; the retries keep every run green.
