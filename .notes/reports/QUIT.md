# QUIT: "newest StandaloneRelease exe hangs on quit" (agent QUIT, 2026-10-11)

Verdict: NOT a shutdown regression. No hang in the quit path reproduced; every observed "no exit after quit_sent" is a route/state failure that the soak report did not distinguish from a hang.

## Reproduction (short route: boot > until:control > 3 s walk > quit; plus the full soak route, private data dir/prefix, MUTE on)
| exe | runs | clean quits (atexit) | no exit |
| build/StandaloneRelease 01:19 (HEAD f9b8b892 + AUDIO/PERF WIP) | 1 short + 14 full-route | 15 | 0 |
| build/StandaloneRelease 02:39 (newest, all WIP) | 12 full-route | 11 | 1 (route failure, see below) |
| build/StandaloneDetached 01:17 (real C++ globals, static dtors) | 3 full-route | 3 | 0 |
No bisect was needed (nothing to bisect); the worktree build (/private/tmp/claude-501/pwQ) was started and abandoned, removed.

## What the "hangs" were (all runs in /tmp/dB04-soak*, /tmp/d3n-out, /tmp/dq-out)
Of 9 historical soaks with quit_sent and no atexit, 8 kept logging frames after quit_sent (game thread alive, process not in shutdown):
* dB04-soak10 (01:29, the "newest unpoisoned exe" of D-B04): CJ was killed during the idle phase, quit_* screenshots show the hospital respawn scene; the pause menu does not open there, ESC/RETURN/DOWN/RETURN are swallowed, the game runs on (7300 frames after quit_sent).
* dq-out/E_4 (mine, 02:39 exe): the START GAME click missed, the game sat in OPTIONS at 250 fps for 10 min (6 [until] TIMEOUTs); ESC in the frontend never quits.
* the same signature in d3n r132/r122/r123/r241, d3y, d3z (old runs).
The one silent one, dB04-soak4 (23:27, ALL-894-ranges poison, exe of ~23:19, before MUTE): game thread stuck before any shutdown log line (no "Shutdown pool started", no atexit). D-B06 documents the all-range poison hang as the game thread spinning on strays of unconverted globals (BoundingBoxMargin 0x8D22B0, CurDistForCam 0x8CCB84, AnimManager refs), i.e. a poison-coverage issue, not MUTE/quit.
The MUTE commit (4e2fb21d), TESTFIX LogGameStateTrace, B01..B05/B11 do not change the quit path in address mode (NOTSA_GLOBAL are references there, no static destructors); 29 clean quits on exes that contain them confirm it.

## Fix (tools only)
* tools/standalone/soak_route.txt: `until:control:90000` before the quit ESC (no pause menu while CJ is wasted/busted).
* tools/standalone/soak.sh report: prints "gate timeouts: N", and after quit_sent without atexit distinguishes "QUIT NOT EFFECTIVE (game kept running N frames)" from "SHUTDOWN HANG (no frame after quit_sent)".
* Left alone: the START GAME mouse click (472,248 / 236,214 then RETURN) misses now and then under host load (E_4); a later route fix could gate on a menu-state condition.
Not done: D-B06 boot-dump poison run (not trivial: needs the detached build + the stray-range exclusions, see D-B06.md).

Commit: 303fbdde (tools only). Worktree + build dir + private prefixes removed.
