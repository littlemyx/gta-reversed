# PROBE-SUITE: gameplay regression suite (2026-10-18, Windows host)

`tools/standalone/gameplay_regress.sh` replays the findings of the user's playtests as short scripted scenarios on OUR build (and, for three of them, on the ORIGINAL exe), extracts comparable numbers and prints
PASS / FAIL / KNOWN / SKIP per check. Docs: `docs/STANDALONE.md` "Gameplay regression suite"; rule line in `.notes/PORT_BRIEF.md` "Host: Windows" (run it before declaring a gameplay-affecting change done).
Worktree `C:/gta-scratch/wtps` (branch ps-regress), runs in `C:/gta-scratch/gr_out/{f*,m*,o*}`, mutant exes in `C:/gta-scratch/gr_exe/`.

## What was built
| piece | files |
|---|---|
| runner | `tools/standalone/gameplay_regress.sh` (`-l` list, `-o` also the original exe, `-r` record, `-x exe` / `-b builddir`, `GR_SKIP`, `GR_OUT`): private data dir (links to the Steam assets, no original exe), GPU lock per run (waits for USER-PLAY), the user's `gta_sa.set` saved/restored, silent (`NOTSA_STANDALONE_MUTE`), windowed 960x600 (not `render_firststart`), no Present for scenarios that do not look at pixels, one retry when the SET-UP failed |
| scenarios | `tools/standalone/gameplay_regress/routes/*.txt` (+ `header*.inc`), generated cheat scene (`gen_cheats_scenario.py`), checker `gr_check.py`, thresholds with a `why` in `baseline.json`, open items in `known.tsv`, `mouse_sweep.ps1` (real `mouse_event` side-car) |
| original-exe leg | `orig_run.sh`, `orig/*.scn` (inputdrv scripts), `orig_gr_poll.py` (ReadProcessMemory: player, cops, law cars, pickups; teleports CJ like `cmd:tp`), existing `audio_stats_exe.py` / `windows/cheat_probe_exe.py`; `record_baseline.py` puts the measured numbers into `baseline.json` |
| injector (run build only) | new `source/app/platform/GameplayRegress.cpp` + 5 lines in `SDLWrapper.cpp`: `cmd:snap:NAME` (`[gr] NAME#n A..D` lines), `cmd:spawncop`, `cmd:spawnveh:MODEL:DRIVER:DIST`, `cmd:shootlast`, `cmd:shootdrv`, `cmd:hitplayer`, `cmd:copattack`, `cmd:time`, `cmd:quit`, `until:cutscene|nocutscene`, `vmode:WxH` |

## Scenarios (HEAD, ours; wall clock of the game run)
| scenario | s | checks | replays |
|---|---|---|---|
| combat | 140 | 14 | U5 U6 U7: wanted level in the open street (cops arrive, shoot CJ, any sign of engagement <= 70 s), killed cops die (also those in cars), armed cop on foot drops pickups, damage event hurts CJ, police car driver dies on a lethal damage event |
| audio | 122 | 10 | U12 U13: a car driven into five parked cars + 25 s idle: collision-list max / released (one-shot entries), active sounds, channels, starvation, stuck loops |
| cheats | 143 | 4 | VT-OVERRIDES: 18 typed cheats (toggles pressed twice), state after each = the original's |
| missions | 116 | 11 | U8 U10 U11: opening mission running, `arrest:0` during it (game goes on, mission flag 1 -> 0), Return mounts / dismounts / re-mounts a BMX, enters a car |
| render_vmode | 137 | 10 | U2: live mode changes 960x600 -> 1600x900 -> 2560x1440 -> 960x600 at game hour 10: post-effects front buffer covers the camera raster, camera = mode = window |
| input_mouse | 137 | 6 | U3: real mouse sweeps (3 x 4000 raw px) turn the camera > 360 deg each in one direction, relative mode on |
| render_firststart | 25 | 4 | U1: first start without gta_sa.set, EXCLUSIVE fullscreen: window client = video mode = 800x600 |
| interior_lspd | 67 | 7 | U15 U16: 3 stars + teleport into the LSPD interior: entered, cops present / on the floor / reacting |
Total ~15 min of game time + ~10 s per run for start-up/clean-up = ~17 min without lock contention (the GPU lock was contended by 3-4 other agents during development: wall clock 1-2 h per full run).

## Determinism and tolerances
* Same start state: new game, `until:control`, then the tutorial BMX is mounted and left with Return (`header_free.inc`) which ends the opening mission (otherwise the first move to Grove Street starts the next story cutscene); `NOPRESENT` makes the new game reach control in ~6 s.
* The injector places what a check needs (spawned cop / police car with driver, lethal damage events, teleport, clock) so only the police chase and the audio scene keep the game's randomness. Chase: ANY sign of engagement within 70 s counts (the original: CJ first hurt 13 s after the wanted level, cop cars drained of drivers after 15 s; PT-COMBAT saw cop cars idle for 40 s in the start alley, so the chase runs in the open street at Idlewood). Audio: the thresholds are the original's envelope (collision list max 7, one-shot <= 2, active sounds 37, channels 37, loops 27 in the same scene) with a margin that is far below a leak (mutant: 9 one-shot entries stuck for ever).
* Cheats: the Village People cheat makes peds shoot rockets at CJ, so armour is not compared from there on; vehicles only for the spawn cheats (Rhino, Trashmaster).

## Detection proof (reverted fixes, local mutant exes; HEAD = all PASS)
| mutant (reverted) | scenario | result |
|---|---|---|
| 414cac8a pool zero fill + 81885320 CarDrive ctor bits | combat | FAIL `car.driver_dies` (driver hp 0, state DRIVING, task CarDrive), FAIL `chase.no_zombie_cops` (9 cops shot, 3 dead, 2 at 0 hp not dying), FAIL `chase.player_shot_at` (CJ never hurt in 70 s) = the U5/U6/U7 signature |
| 9856866e CAECollisionAudioEntity::UpdateParameters (never ported) | audio | FAIL `coll_list_max` (16) and `coll_list_released` (14 one-shot entries stuck after 25 s idle; HEAD: max 3, 0 at the end, 26 additions in the scene) |
| 5d60eddd ResetIP | missions | FAIL: AV in `CRunningScript::Process` right after `arrest:0` (no_crash, ran_to_end, all later checks) |
| 30873a46 Return key = numpad Enter | missions | FAIL `bike.enter_return`, `bike.reenter_return`, `car.enter_return` (and every scenario that uses `header_free` reports SETUP: the tutorial BMX cannot be mounted) |
| 15973267 SetVideoMode resize | render_vmode | FAIL `vm_1/vm_2.front_covers_camera` (1600x900: front buffer 1024x1024; 2560x1440: 2048x1024 = one mode step behind, the cause of the squares) |
| 35d35285 SDL relative mouse mode | input_mouse | FAIL `relative_mode_on` and `sweep1_rotates` (150 deg per sweep instead of 1253) |

## Things the suite itself found or showed
* **U15 reproduced and then confirmed fixed**: on the exe before PT2-INTERIOR's fix `interior_lspd.cops_on_floor` failed (cops +1.3 / +1.4 / +2.5 m above CJ 4 s after entering, 0 later; it was listed in `known.tsv`); on HEAD with the fix the offsets are 0 and the check passes, `known.tsv` is empty. U16 (`cops_react`) passed on both: at 3 stars the cops inside engage and hurt CJ.
* `CGameLogic::Update` busted branch: `NOTSA_UNREACHABLE()` in the wanted-level fee `switch` (`GameLogic.cpp:733`) is hit when the player is arrested with 0 stars (exe 0x4431CA: `default` = fee 100); harmless in the run build (logs, continues), but the comment in the source already says it is reachable. Not touched (outside this task); the busted scenario uses 1 star.
* One crash in 8 cheat runs (flaky, not reproduced in the next 4): `rw::d3d9::instanceMesh` null deref while rendering a skinned ped right after the second Beach Party press (stack: `SkinPipeRender` <- `RpAtomicRender` <- `CRenderer::RenderOneNonRoad`), logged in `gr_out/f5/cheats`. The suite reports a crash as FAIL with the top frames (not retried).
* `key:f` does not enter a Rhino (tank) from the side in either exe; Return enters the cars used by the scenarios. The opening mission auto-completes when CJ mounts the tutorial BMX (also a spawned BMX).

## Limits / not done
* The original-exe leg is exclusive fullscreen, switches the desktop mode and needs the foreground (inputdrv): it is opt-in (`-o`), ~+12 min. `C:/gta-perf/px/inputdrv.exe` was built before `lctrl` existed (the rams of the original audio scene did nothing): rebuild with `windows/build_tools.sh` or set `GR_INPUTDRV`.
* `orig_gr_poll.py` sees the cops of the original only through law vehicles / `copsFoot`; its ped-pool scan returned 0 peds in every original run (cause not found: the vehicle pool, read the same way, works), so "cops on foot" is not available for the original and the original numbers are taken from the player's health and the law vehicles' drivers.
* `known.tsv` / `baseline.json` thresholds are calibrated on this PC (one original run each); `render_vmode` needs a display offering 1600x900 and 2560x1440 modes (otherwise the step is SKIP).
* Pixel look (the dark squares themselves, ghosting of peds, corpse pose U4, AA U17) is not judged by the suite; the invariant behind U2 is.

## Final verification
* HEAD (e7ae95a5 + this change, Release exe built in the worktree): `gameplay_regress.sh -o` run `f13`: 65 of 66 checks PASS; the 66th (`combat.chase.no_zombie_cops`) failed on a flaw of the check itself (the corpses of the shot cops had already vanished from the 60 m window when the second sample was taken) and was fixed to look at both samples (kill_2 / kill_6; re-evaluated offline on the f13 logs: PASS, and the mutant's logs still FAIL). The audio scene was then strengthened (five parked cars: 26 additions instead of 5-16) and re-run on HEAD (`f14`: PASS) and on the audio mutant (`mA2`: FAIL).
* The original-exe legs of `-o` ran end to end inside the runner (combat: police near after 2 s, first sign 18 s; cheats: 18 states compared, 0 differ; audio: envelope printed next to the thresholds).
* Net effect on the user's PC while developing: the desktop mode was switched by the original-exe legs and by `render_firststart` (exclusive 800x600); every run restored the user's `gta_sa.set`. (One orphaned empty `gpu.lock` directory, created when I killed a runner in the middle of acquiring the lock, was removed by hand after 26 min.)
