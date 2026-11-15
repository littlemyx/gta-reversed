# How an RR reference run starts (SH2, 64-bit SSE build, RYDER_1)  - committed as .notes/STATEHASH_START.md

Exe: `build/PortableWin64RR/bin/gta_reversed.exe` (tools/standalone/statehash/shbuild.sh: PortableWin64 + GTASA_STATEHASH + GTASA_SCMTRACE + GTASA_MDRIVER + GTASA_RR_PORTABLE_MATH, /fp:precise).
Driver script: `tools/standalone/statehash/sh2_run.sh record|replay <tag> [outdir] [limit_s] [dumpframes]`, always under `tools/standalone/gpu_locked.sh -o SH2 ...`. Reference outputs: `C:/gta-scratch/statehash/ryder1/` (record.* = driver run, replay.sh = replay from the recorded input; the two hash files are byte-identical, 25907 frames).

## Command line (exact)
    cd C:/gta-wt/statehash2 && tools/standalone/gpu_locked.sh -o SH2 tools/standalone/statehash/sh2_run.sh record record C:/gta-scratch/statehash/ryder1 7200      # driver ON, writes record.driver
    cd C:/gta-wt/statehash2 && tools/standalone/gpu_locked.sh -o SH2 tools/standalone/statehash/sh2_run.sh replay replay C:/gta-scratch/statehash/ryder1 7200      # driver OFF, input = base route + record.driver
The script runs `gta_reversed.exe` (no arguments) in a data dir (default C:/gta-scratch/statehash/d2: junctions to the game files, copy of the exe) with this environment (all of them are read by the exe):
| var | value | meaning |
|---|---|---|
| NOTSA_RR | 1 | RR mode: virtual clock (20 ms per Idle frame), synchronous CdStream, `srand(NOTSA_RR_SEED)` at GameInit and again in LoadingScreen, zeroed `operator new` / rwMalloc / MemoryMgr, screen size pinned to NOTSA_RR_SCREEN |
| NOTSA_RR_SEED | 12345 | the seed (GameInit, LoadingScreen, and `cmd:rr_start` without argument: `srand(12345)`: LCG state = 12345) |
| NOTSA_RR_STEP_MS | 20 | virtual ms per Idle frame (the loading screens / menus / front end count too) |
| NOTSA_RR_SCREEN | 1280x720 | RsGlobal.maximumWidth/Height pinned for the camera frustum |
| NOTSA_RR_NOAUDIO | 1 | AudioEngine.Service() is not run; script audio samples read as finished |
| NOTSA_RR_GAMEPLAY_RAND | 1 | render / audio rand() draws are isolated (state saved and restored, not counted, not logged): the hash file's randCount and the rand log are the gameplay stream only |
| NOTSA_AUDIO | null | no audio device output (the real DirectSound objects are still created) |
| NOTSA_STANDALONE_INPUT | <outdir>/<tag>.route.txt | the input injector route (see STATEHASH_INPUT.md) |
| NOTSA_STANDALONE_USERDIR | C:/gta-scratch/statehash/ud2 | private user-files dir (emptied before the run: no saved settings / save games) |
| NOTSA_STANDALONE_WINDOWED / NOPRESENT / NOVSYNC / SKIP_VIDEOS / MUTE | 960x600 / 1 / 1 / 1 / 1 | window (mouse coordinates of the menu clicks assume 960x600), no swap, no intro videos, mute |
| NOTSA_RR_TRACE, NOTSA_SH_FILE, NOTSA_SH_RANDLOG, NOTSA_SH_LOADLOG, NOTSA_SCMTRACE (+_COMMIT) | <out>/<tag>.rr.trace / .sh / .rand / .load / .scmtrace | outputs (hash file `frame randCount h_tm h_ck h_rng h_pop h_pl h_cam h_pd h_vh h_ob h_all`, rand log, load-order log, SCM trace); NOTSA_RR_TRACE must be set for rr_start to record |
| NOTSA_SH_DUMP, NOTSA_SH_DUMP_SUBSYS, NOTSA_SH_DUMP_FILE | 1000,5000,15000 / all / <out>/<tag>.dump | verbose dump (check with tools/standalone/statehash/shdump_check.py: 30 hashes, 0 mismatches) |
| NOTSA_MDRIVER, NOTSA_MDRIVER_OUT | tools/standalone/mdriver/ryder1.steps / <out>/<tag>.driver | record mode only: the scripted driver (= web harness playRyder1) and its recorded actions |
(NOTSA_RR_MAXFRAMES + NOTSA_RR_EXIT=1 end a run early; the full run ends by the injected `cmd:quit` 50 frames after the mission thread ends.)

## Timeline of the base route (tools/standalone/gameplay_regress/routes/header.inc, then 4 lines)
Script clock = virtual ms since the first injector pump (`wait:N` adds N, `until:COND` holds the clock until the condition is true).
1. Boot: the exe starts in the front end. `until:menu` (main menu drawing, ok at script 0 ms), `wait:1500`, mouse (472,248), Enter (selects Start Game), `wait:3000`, mouse (236,214), Enter (New Game), `wait:3500`, Enter (confirm), then the new game loads (`mark newgame_loading`), `until:control` (CJ has control, no cutscene, no fade: true after 0 ms at script 15450 ms), `wait:3000` -> script 18460 ms. The opening mission thread is still running (the "intro" in front of Grove Street; CJ at 2239.7, -1261.9, 23.9).
2. `wait:600`, then in ONE injector pump (before the Idle of that frame, CTimer frame counter 962, rr virtual ms ~13380 of CTimer): `cmd:time:6:30` (game clock 06:30), `cmd:rr_start` (opens the outputs, `srand(12345)` = LCG state 12345, recorded frame counter = 0), `mission:24` (MissionDebugModule free start of main.scm mission 24 = script GUNS1 = RYDER_1: fails the running opening mission first, then starts the mission script at offset 747935 and reads data/script/main.scm again).
3. Recorded frame 0 = the next Idle: CTimer frame counter 963 (0x3c3), CTimer ms 13400, game clock 06:30:01, weather new/old = 2 / 14 (not forced: whatever the new game booted into), CJ model 0 at (2239.72314, -1261.92322, 23.9375), 3 peds / 6 vehicles in the pools (npedpool 3, nvehpool 6), stmem 0x24f1800, rng state after the frame 0 draws; frame 0 hash line `0 362629 1d19156b69f05d31 0bfc19883ac2de21 ...`.
4. Frames 1.. : the driver (record mode) / the recorded route (replay mode) plays the mission; it ends when the mission thread GUNS1 ends (recorded frame 25863: `X 25863 .. 2d 4e`, `E 25863` in the SCM trace, identical frame and the same terminal command sequence as the 32-bit run); quit is emitted at frame 25906; the last hash line is frame 25906.
The outputs of record and replay: hash file identical (md5 5f2ed03be8965f7398918385e9dfad39), SCM trace body identical (6297525 lines, md5 of lines 4.. 5e362b56e3aa...), rand log columns callidx/frame/value identical (the caller column - up to 5 return addresses relative to StableId - differs between the two runs in some rows, stack words outside the exe module are ASLR dependent; not analysed further).

## Rand counting - where our files and the TS port differ
* randCount in the hash file / callidx in the rand log are cumulative since PROCESS START (main thread only, gameplay stream only because of NOTSA_RR_GAMEPLAY_RAND=1). 362139 calls are made before the first recorded frame (rows with frame -1: boot, loading, menus' non-isolated draws, new-game init, the mission start of the same pump is counted in frame 0), frame 0 itself makes 490, so hash-file randCount(frame 0) = 362629.
  TS counts from the first call of frame 0: `tsCount(k) = ourRandCount(k) - 362139` (our rand log: subtract 362139 from callidx; rows with frame -1 are the pre-record calls). The LCG state at the first call of frame 0 is 12345 on both sides (`srand(12345)` by rr_start; the hash file `rng` column is the state AFTER the frame's draws).
* Everything else the TS side asked for: frames are 20 ms of virtual time each, the same in both.

## Main loop order of one frame in our build (for comparing with the TS main.ts order)
InjPump (injector events of this script ms: keys, warps, rr_start, mission) -> Idle: AdvanceClock (+20 ms virtual) -> CTimer::Update -> CGame::Process [CPad::UpdatePads, **CStreaming::Update first** (requests + the conversion of finished loads), CCutsceneMgr, front end, zones, ... world / peds / vehicles / scripts ...] -> AudioEngine.Service (skipped, NOAUDIO) -> **OnFrame: hash of the frame** -> render (isolated rand, not hashed).
Our streaming is NOT "synchronous at the end of each frame": see LOADORDER.md (it runs at the START of CGame::Process and finishes loads over one or more later frames).
