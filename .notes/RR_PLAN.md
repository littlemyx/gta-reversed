# RR_PLAN: deterministic record/replay state-hash harness (milestone 4)

Author: MAC-RR-DESIGN, 2026-10-24. Branch portable/main. Status of the code: section 8 (what exists), section 9 (findings of the first runs), section 10 (slices).
Goal (PORTABILITY_GOAL.md): same input script + seed => the same per-frame hash of player, peds, vehicles, RNG, timer, in the Win32 reference build and in the portable build;
when they differ, name the FIRST diverging frame, subsystem, entity and field. The portable build must also be equal to ITSELF run to run; that comes first, because a
trace that is not reproducible on one build says nothing about two builds.

## 0. Architecture in one picture

```
 script (frame-indexed, virtual ms)         NOTSA_RR=1: virtual clock, sync CdStream, fixed seed, audio off
        |                                              |
  SDLWrapper InjPump  --keys/cmd:rr_*-->  game  --Idle():  AdvanceClock -> CTimer::Update -> CGame::Process -> AudioEngine.Service -> rr::OnFrame()
                                                                                                                      |
                                                    one `F` line per frame (+ `D` lines inside the detail window)  <----+
                                                              |
        trace A (portable)  \                                 v
                             >-- tools/portable/rr_diff.py --> first diverging frame / subsystem / entity / field
        trace B (win32)     /
```

Everything is behind `NOTSA_STATEHASH` (cmake `-DGTASA_STATEHASH=ON`, default OFF). Code: `source/portable/statehash.{h,cpp}` (core), hook lines in `app/app_game.cpp`,
`app/platform/SDLWrapper.cpp`, `game_sa/LoadingScreen.cpp`, `portable/oswrapper_portable.cpp`, `portable/cdstream_portable.cpp`. Tools: `tools/portable/rr_run.sh`, `rr_diff.py`, `rr_route_walk.txt`.

## 1. Determinism sources and how each is pinned

Legend: P = portable build, W = Win32 reference build (StandaloneRelease + -DGTASA_STATEHASH=ON). "pin" = what the harness does; "RR-n" = slice that does it.

| # | source | where it enters | pin | status |
|---|---|---|---|---|
| 1 | wall clock behind CTimer | `GetOSWPerformanceTime()` (P: steady_clock; W: QueryPerformanceCounter in `oswrapper_win.cpp`) -> `CTimer::Update` delta, `CTimer::GetCurrentTimeInCycles`, `OS_TimeMS` | **virtual clock**: with `NOTSA_RR=1` `GetOSWPerformanceTime()` returns a counter that advances only by `NOTSA_RR_STEP_MS` (default 20 ms) per `Idle()` / `FrontendIdle()` (`rr::AdvanceClock`). Every consumer (CTimer, game clock, `CTimer::GetTimeInMS` based AI timers, scripts' TIMERA/TIMERB, weather) becomes a pure function of the frame count. W: the same 3-line hook in `oswrapper_win.cpp`. | P done (RR-1); W RR-3 |
| 2 | frame time / frame limiter | `Idle()`'s 14 ms busy wait on `GetCurrentTimeInCycles` (spins for ever on a frozen clock), the present pacer (vsync / timed limiter, MAC-W2), `NOTSA_STANDALONE_NOVSYNC` | virtual step >= 14 ms satisfies the lock by construction; with RR the busy wait is skipped (`if (!rr::Active())`). The wall-time presenter only paces the host, it never feeds game time (the game reads only the virtual clock). NOVSYNC=1 in the run script. | P done |
| 3 | streaming thread timing | `CdStream` thread (P: `cdstream_portable.cpp`; W: `CdStreamInfo.cpp` / overlapped I/O) completes reads at arbitrary moments; `CStreaming::Update` reacts to what finished | **synchronous reads**: in RR mode `CdStreamRead` reads inline and the request is complete when the call returns. When a model becomes "loaded" is then a function of the game's own call sequence. W: same inline read (RR-3). `CStreaming::LoadAllRequestedModels` points of the injector (`tp`, `spawn*`) stay deterministic automatically. | P done |
| 4 | audio thread / hardware state | AEAudioHardware (DirectSound over OpenAL on P; real DirectSound on W): playing-channel queries, buffer play cursors advance on a steady clock (`dsound_null` / OpenAL) and the audio entities consult them (`IsSoundPlaying`, speech finished) and call `rand()` on the game thread | P: `NOTSA_AUDIO=null` removes the device but the software buffers still advance on the steady clock => **candidate divergence source #1** (to be measured, section 9). Pin = make the null device's cursors a function of the virtual clock (RR-2). W: run with audio disabled / `-nosound`-equivalent in the variant, or the same virtual-cursor change in `dsound` wrapper (RR-3). | measure |
| 5 | input timing | injector pump is wall-clock driven (`GetTickCount64`) | script clock = virtual clock (`InjNowMs`): `wait:N` = N virtual ms = N/20 frames; `until:COND` barriers evaluate once per pump (once per frame) and time out in virtual ms. Keys are SDL events fed in `ProcessEvents` before `Idle`, so a key lands in a fixed frame. Real keyboard/mouse are ignored in an RR run (no human input). | P done; W RR-3 (same SDLWrapper code, shared) |
| 6 | RNG: seed | `GameInit`: `srand(RsTimer())`; `CLoadingScreen::Init`: `srand(QueryPerformanceCounter)`; `CPostEffects::Grain` `srand(time)` (visual, but it RESEEDS the one global RNG every frame when the grain effect is on) | `GameInit` / loading screen: `srand(NOTSA_RR_SEED)`; `cmd:rr_start:SEED` re-seeds again at record start. Grain: reads the virtual clock => deterministic, but reseeding from a time makes it a gameplay-visible effect only while night-vision/infrared is on (not in the first route). | P done (2 sites); Grain audit RR-2 |
| 7 | RNG: shared state across threads | W standalone run: one global `g_RandSeed` (GameRand.h) used by ALL threads (CdStream, audio) => racy; P: per-thread LCG (`msvc_rand.h`) like the exe's `_tiddata` | W: a thread that calls `rand()` (audio) perturbs the game stream. RR-3 must run the W variant with a thread-local state (the exe's real behaviour) or prove no off-thread caller. Record the state in every frame (`rng=`) so a stolen draw shows at the exact frame. | open |
| 8 | **pointer-derived values** | `NOTSA_PTR_ID32(p)` (Base.h): corona / rope / shadow / crime ids (compare only, harmless), but also **`srand(NOTSA_PTR_ID32(this) + ucDays)`** in `CEntity::ProcessLightsForEntity`/ENEX branch (`Entity.cpp:1056`, reseeds the global RNG when an entity with a timed ENEX effect is created/processed), `(NOTSA_PTR_ID32(this) + CTimer::GetTimeInMS()) & 0xFFF` (light flicker phase, visual), `PPTriPlantBuffer` seeds | ASLR: macOS always randomises image slide, heap and stack; `posix_spawn(POSIX_SPAWN_DISABLE_ASLR)` fixes image + stack but **malloc/mmap addresses still vary** (measured, section 9), so the pool base differs per run => low 32 bits differ. Win32: the heap (pools are `malloc`ed) is randomised by the OS too. Pin = under `NOTSA_STATEHASH` redefine `NOTSA_PTR_ID32(p)` as a stable id (pool slot index for objects of a pool, offset from the image base otherwise). | open, RR-2 (**highest value**) |
| 9 | uninitialised memory | `new`/`malloc` without zeroing; struct padding hashed by accident | the hash feeds explicit typed fields only, never raw memory. Detection: run the same script with `MallocScribble=1 MallocPreScribble=1` (P) / page-heap fill (W); a trace difference means a read of uninitialised heap that matters. | RR-2 |
| 10 | pointer-order iteration / hash containers | `std::unordered_map<Ptr,...>`, `std::set<T*>` iteration; pool iteration is slot order (index), checked | the hash walks pools by SLOT (`GetAt(i)`), never by address. Grep audit for `unordered_*`/`std::map<.*\*` in gameplay code (RR-2). | partly checked |
| 11 | float mode | P: clang arm64/x64 float (`-ffp-contract=off`, soft x87 for transcendentals, MAC-F4a); W: MSVC x87 PC24 | not pinnable; it is THE expected divergence. Two-level hash (exact / quantised 1/256) separates "rounding-level" from "behavioural". Divergence is localised to the first frame/field; Stream F work lowers it. | by design |
| 12 | threads other than streaming/audio | none in gameplay (CdStream, AEStream, AEMP3 loader); `std::async`/jobs: none found in game_sa | audit with a TSan run of the route (RR-2) | open |
| 13 | file system / time-of-day / locale | save slot listing, `time()` for save game dates (`crt.h` localtime), `gta_sa.set` prefs | route starts "New Game" with prefs from a fresh `NOTSA_STANDALONE_USERDIR`; no save is read. Wall-clock dates only reach save files and the HUD clock (game clock is virtual). | ok |
| 14 | window/GL state | HiDPI size, video mode, GL driver | rendering never feeds back into the simulation, EXCEPT camera visibility culling that decides which entities are processed/streamed (frustum, LOD distance) => the **window size/aspect is part of the determinism input**: RR runs fix `NOTSA_STANDALONE_WINDOWED` size (record it in the header: RR-2) | RR-2 |
| 15 | script (CLEO-free) timing | main.scm threads wake by CTimer ms | follows from 1 | ok |
| 16 | game start state | the new-game intro cutscene length depends on streaming/IO and on Escape-skip timing | route skips the intro; barrier `until:control` re-synchronises the script on game state, and the harness starts hashing at `cmd:rr_start` (frame 0 = first frame after control). Pre-start history may differ in CTimer values; the header stores the CTimer ms at start and the diff tool offers `--rebase-timer` (RR-2) if needed. | measure |

## 2. Script format, recorder, player

* Format: the existing injector (`NOTSA_STANDALONE_INPUT`, `source/app/platform/SDLWrapper.cpp`): `wait:N`, `key:K`, `down:K`/`up:K`, `mouse:x,y`, `until:COND[:MAXMS]`, `cmd:NAME[:args]` (tp, spawn*, wanted, hour, health ...), `mark:NAME`.
  In an RR run (`NOTSA_RR=1`) every time in it is VIRTUAL ms, so scripts are frame-indexed (20 ms = 1 frame at the default step). Barriers (`until`) sync on game state, not on time.
* New commands (RR-1): `cmd:rr_start[:SEED]` (open the trace, seed the RNG, frame 0 starts at the NEXT frame), `cmd:rr_stop`, `cmd:rr_stat`. `cmd:quit` exists (GameplayRegress).
  Planned (RR-2): `cmd:rr_seed:N`, `cmd:rr_pad:FRAME:...` for analog sticks, `cmd:tp` variants that do not call `LoadAllRequestedModels` out of band.
* Recorder from live input (RR-4): `NOTSA_RR_RECORD=<file>` in a NORMAL (non-virtual-clock) session logs, at the pump, per game frame the injector-equivalent events (SDL key down/up with the CTimer frame counter, mouse deltas, pad axes) and `cmd`s from debug menus; the output is a script in the injector's format with `waitframes:N`
  lines (new item = N virtual steps; converts to `wait:20*N`). A recorded session is NOT frame-exact (the live session had real time, streaming races and audio), but its INPUT is, and replaying it under `NOTSA_RR=1` is by definition deterministic. This is the "record" half; the "replay" half is the injector under the virtual clock.
* Player: the existing injector; no new player is needed.

## 3. Hash content and trace format

One line per recorded frame, text, ASCII, `key=value`, space separated; all numbers hex or decimal, floats ONLY as bit patterns inside hashes (endianness independent: FNV-1a 64 over explicit little-endian bytes of 32-bit words).

```
#RR 1 build=<tag> seed=<n> step_ms=<n> float=<portable|win32>
F <n> gf=<CTimer frame counter> rng=<holdrand hex8> tm=<h16> ck=<h16> pop=<h16> pl=<e16>/<q16> cam=<e16>/<q16> pd=<count>:<e16>/<q16> vh=<count>:<e16>/<q16> ob=<count>:<e16>/<q16> all=<e16>/<q16>
```
* `e` = exact hash (floats by bit pattern). `q` = quantised hash (floats rounded to 1/256 first, non-finite -> sentinel), so "close enough" is a separate, explicit second signal.
* Subsystems: `tm` CTimer (frame counter, ms, non-clipped ms, pause ms, time step bits, scale, paused); `ck` game clock h/m/s/day + weather; `rng` the thread RNG state (P: `msvc_crt::HoldRand`, W: `g_RandSeed`); `pop` ped/car counters (CPopulation, CCarCtrl), streaming memory/requests, pool occupancy; `pl` player ped (all ped fields + wanted level, chaos, money); `cam` camera position/forward; `pd` every ped IN POOL SLOT ORDER (slot, model, type, state, move state, health, armour, matrix axes + position (or heading), move/turn speed, rotation, aim rotation, vehicle slot, weapon+ammo, task-type chain of the primary tree); `vh` every vehicle (physical fields + health, gas, brake, steer, driver slot, autopilot mission/temp action, colours); `ob` every dynamic object (physical fields). `all` folds the nine.
* Detail window (`NOTSA_RR_DETAIL=A-B`, recorded-frame numbers): for those frames the trace also holds `D <n> <kind> [<slot>] name=hex ...` lines, one per entity (`ped`/`veh`/`obj`) and one each for `timer`, `clock`, `rng`, `pop`, `player`, `camera`; float fields are named `f.*` and hold the bit pattern. Per-entity/field localisation = run the SAME script again with `NOTSA_RR_DETAIL=<first diverging frame>` on both builds (cheap: the runs are reproducible) and diff. A single frame's detail is ~100 KB, a full level-1 trace is ~300 B/frame (~20 KB/min at 50 fps).
* Header `float=`/`build=` identify the build; the diff tool refuses nothing, it just prints both headers.

Why not a binary format: traces are diffed by humans and by `git`/`diff`; the size is small; a text line is also what the Windows host can produce with `fprintf` on any compiler.

## 4. Win32 neutrality

* `GTASA_STATEHASH` defaults to OFF: `NOTSA_STATEHASH` is undefined in every normal build, so every added line is preprocessed away. Proof method = `tools/standalone/w0a_guardlines.py check --base <commit>` (TEXT OK per TU: the post-preprocessor text with `NOTSA_STATEHASH` undefined is identical) and, where the TU contains a function body, a wine/MSVC code-byte A/B of the changed TUs (STREAM_C_PLAN method).
* The Windows reference traces come from a variant: `StandaloneRelease` + `-DGTASA_STATEHASH=ON`. Argument that the variant's game state IS the reference: (a) the only code added to the simulation path is `rr::AdvanceClock()` / `rr::OnFrame()` in `Idle()` and the clock / seed / stream pins of section 1, all of which are the point of the experiment (they REMOVE environmental nondeterminism, they do not change any game function); (b) `OnFrame` only READS game state (it takes const views, allocates its own buffers, does not call any game function that mutates, does not touch `rand()` - it reads the state word); (c) acceptance for the variant: with the pins DISABLED (`NOTSA_RR` unset, trace only) its behaviour equals the non-variant on the gameplay_regress suite, and a single run of the variant with `NOTSA_RR=1` produces a trace equal to itself (self-determinism, same test as RR-1 on P). (d) Compiler effects: adding a TU and 3 call sites can change inlining/layout of the surrounding functions in an optimised MSVC build; the x87 results of a function do not depend on its layout (the code is /fp:precise per statement), so this cannot move a float; it can move stack/heap addresses, which is exactly the pointer-id issue of row 8 and is neutralised there.
* Unchanged non-variant: ON/OFF equality of the plain build is the guardlines TEXT OK (section above); no CMake change when the option is OFF other than the `option()` line.

## 5. Diff tool

`tools/portable/rr_diff.py A B` (exit 0 same / 1 diverged / 2 unreadable). Output: first divergence (exact) with the subsystems (and count mismatches), whether the QUANTISED hashes diverge at the same frame ("behavioural") or never ("rounding drift only") or later (first behavioural frame is reported separately), and, if both traces have `D` lines for that frame, the first entity and field of each differing subsystem with decoded values and delta. Missing detail -> prints the exact command to re-run with `RR_DETAIL=<frame>`. Host-tested by `tools/portable/rr_diff_test.py` (valid case + 6 mutations).

## 6. Self-determinism protocol (RR-1 acceptance)

`for tag in a b: tools/portable/rr_run.sh $tag 240` then `rr_diff.py a.trace b.trace` must print IDENTICAL; any divergence is bisected with the detail window and the cause is a finding (section 9). Controls: `MallocScribble`, `NOTSA_RR_STEP_MS=33`, other seed (must DIFFER from the first seed's trace, proving the hash actually sees the RNG), no-RR run (must differ: proves the pins do something).

## 7. What is deliberately NOT in the trace
Rendering output, audio samples, GPU state, UI/HUD text, file I/O results, log lines. The question is "is the simulation the same", not "does it look the same" (the screenshot tools cover that).

(sections 8 to 10 are appended below by the run results)

## 8. What exists (RR-1, landed on portable/main)

| piece | file | note |
|---|---|---|
| option | `source/CMakeLists.txt` | `GTASA_STATEHASH` (OFF) -> `-DNOTSA_STATEHASH` |
| core | `source/portable/statehash.{h,cpp}` | config from env, virtual clock, FNV-1a hashing (exact + quantised), per-subsystem hashing, detail window, trace writer, `rr::Cmd` |
| hooks | `app_game.cpp` (Idle: `AdvanceClock`, skip the 14 ms wait, `OnFrame`; FrontendIdle: `AdvanceClock`; GameInit seed), `win/WinMain.cpp` (frame-limiter gate), `SDLWrapper.cpp` (`InjNowMs`, `cmd:rr_*`), `LoadingScreen.cpp` (seed), `oswrapper_portable.cpp` (`GetOSWPerformanceTime`), `cdstream_portable.cpp` (inline reads) | every added line is inside `#ifdef NOTSA_STATEHASH` |
| lock file | `app_platform.cpp` | `NOTSA_LOCK_FILE=<path>`: the single-instance lock was machine wide (`/tmp/gta_sa_reversed.lock`), a second agent's game made every other start exit 0 at once (found the hard way) |
| tools | `tools/portable/rr_run.sh`, `rr_selftest.sh`, `rr_diff.py`, `rr_diff_test.py`, `rr_route_walk.txt` | |

(addition to section 8: `NOTSA_RR_NOAUDIO=1` skips `AudioEngine.Service()`; RR mode also zeroes fresh pool slots (`Pool.h`) and every RenderWare allocation (`statehash.cpp PinRwHeap`); debugging aids: `NOTSA_RR_LOG_FRAMES=N` + `NOTSA_RR_LOG_ENTS=1` (one log line per Idle frame), `NOTSA_RR_PROBE=A-B` / `NOTSA_RR_PROBE_SLOT` (hash of all vehicles at named points of `CWorld::Process`), `NOTSA_RR_RANDLOG=file` + `NOTSA_RR_RANDLOG_FRAMES=A-B` (every `rand()` draw with a 5-deep call stack, resolve with `nm`), `NOTSA_RR_DUMP_VEH=<idle frame>` (raw bytes of every vehicle), `cmd:rr_stat:<tag>`, `cmd:rr_offsets`.)

## 9. Findings of the first runs (portable, M5 Max, route `rr_route_walk.txt` = menu, New Game, control, 6 standing peds + a police car, 900 recorded frames = 18 virtual seconds)

A run takes 5 s wall clock (about 330 game frames per second). Every row below was measured by running the exe N times from the same script and comparing traces with `rr_diff.py`.

### 9.1 What is reproducible now
After the pins of section 1 the following are IDENTICAL in every run: virtual time, CTimer (frame counter, ms), game clock, weather, the RNG state word at every frame, population counters, streaming memory/request counters, the number and slots of peds/vehicles, the player, the camera, and (with `NOTSA_RR_LOG_FRAMES`) the per-frame state of the first 400 frames of the game (6 of 6 runs equal, after the audio fix).

### 9.2 Nondeterminism sources found (in the order they were found and removed)
| # | source | evidence | status |
|---|---|---|---|
| F1 | the single-instance lock `/tmp/gta_sa_reversed.lock` is machine wide | a second agent's game made every other start exit 0 after 0.6 s ("InjectedHooksMain finished" and nothing else) | `NOTSA_LOCK_FILE` (portable) |
| F2 | the main loop gates `Idle()` on the frame limiter (`GAME_STATE_IDLE` case in `WinMain.cpp`: `v9_1 < v9_2` with `GetCurrentTimeInCycles`), and `Idle()` busy-waits 14 ms on the same clock | on a clock that moves only inside `Idle()` the game hangs in `ProcessEvents` (hang dump: `SDL_WaitEventTimeoutNS` <- `ProcessGameLogic`) | RR: `Active()` bypasses the gate; the wait is skipped; one virtual step >= 14 ms per frame |
| F3 | **audio hardware time**: the software DirectSound device advances play/write cursors on the steady clock; `CAEWeatherAudioEntity::UpdateParameters` (via `CAESoundManager::Service`) draws `CGeneral::RandomBool` only when its sound is in a certain playing state | rand() draw log of two runs: identical state sequence but one `RandomBool` site executed once vs twice in frame 2, i.e. the order of consumers of the one RNG differs; the RNG state at Idle frame 3 / 172 had two values; population counters (`peds 9 vs 8`, `rcars 1 vs 0`) differed at recorded frame 0 | FIXED for portable: manual-pump mixer advanced by the virtual clock (`dsal::RrPump`); 6 of 6 runs equal afterwards. Cross-build alternative: `NOTSA_RR_NOAUDIO=1` |
| F4 | pointer-derived values: `srand(NOTSA_PTR_ID32(this) + days)` (`Entity.cpp:1056`, timed ENEX effects) reseeds the ONE game RNG from an object address; ASLR cannot be pinned on macOS (`POSIX_SPAWN_DISABLE_ASLR` fixes image + stack, malloc/mmap addresses still differ per run: measured) | static reading + the ASLR experiment; not yet seen to fire in the route (no timed ENEX built in the first 18 s) | FIXED in RR builds: `NOTSA_PTR_ID32` -> `rr::StableId` (pool tag + slot index) |
| F5 | **uninitialised member read**: `CBike::m_vecAveGroundNormal` (Bike.h:48) is never initialised in the constructor and only written by `ProcessControlCollisionCheck`; a parked bike that takes the `bSkipPhysics` path (physics skipped for settled/abandoned bikes) reads it at `Bike.cpp:2239` (tilt force) -> `m_vecTurnSpeed` gets a value from stale bytes (0.096 in one run, 1e-22 noise in others) -> the BMX at the start position moved differently in about half of the runs; first divergence at recorded frame 0 in 5 of 12 runs | found with the in-frame probes (`pre_control` equal, `post_control1` different; the bike's raw bytes: only pointers + TurnSpeed/wheel fields differ) and confirmed: with pool slots zeroed the probe shows `avgn=0,0,0` and `tv=0` in 6 of 6 runs | RR builds zero fresh pool slots. **Real bug, also in the exe unless its pool memory happens to be zero**: fix = initialise the member in the constructor (value to be taken from the exe constructor by the Windows host's oracle; zero is what a fresh heap page gives) |
| F6 | **held prop gets NaN / garbage**: the cigarette `CJ_CIGGY` (model 1485, obj slot 63) held by a smoking ped (`CTaskSimpleHoldEntity::ProcessPed`, `Bone matrix = RpHAnimHierarchyGetMatrixArray(hier)[animIndex]`) has NaN/inf/garbage position and matrix at recorded frame 135/136; diagnostic output: `animIndex=18 bonetag=24 pos=(nan,nan,nan) nodes=32` | about half of the runs; in 1 of 12 the NaN position crashes the game: SIGSEGV null deref in `CPhysical::RemoveAndAdd()` <- `CWorld::Process` (sector index from a NaN position). After zeroing RW allocations the NaN mode is deterministic (zero quaternion keyframes -> NaN on normalise) and so is the other mode: the remaining nondeterminism is ONE BINARY CHOICE: whether the ped's skeleton was animated before the prop reads the bone matrix | OPEN. Not removed by: zeroed stack (`-ftrivial-auto-var-init=zero`), `MallocScribble`/`MallocPreScribble`, `MallocNanoZone=0`, image ASLR off, no present, no audio, zeroed RW heap. Next: probe `ped->bCalledPreRender`/anim-block load state (`CAnimManager` block streaming) at the frame the task starts; compare the two modes with the detail window at frame 135 |
| F7 | TSan | a `-fsanitize=thread` build of the whole game boots 20x slower (150 s without reaching the menu keys); not usable as a quick check | dropped; use per-thread audit instead |

Self-determinism table (same exe, same route, N runs; "distinct" = number of different trace files):
| configuration | N | distinct | note |
|---|---|---|---|
| first RR run (clock + sync stream + seed) | 2 | 2 | rng/pop/peds/vehicles differ at frame 0 (F3) |
| + virtual audio mixer | 12 | 7 | all start at the BMX (F5) except one at frame 175 (objects, rounding only) |
| + pool slots zeroed | 12 | 5 | 7 identical; the rest diverge at frame 136 in obj 63 (F6) |
| + RW heap zeroed | 16 | 7 | 9 identical = the NaN mode (84 diagnostic lines each); the others vary |
| + `NOTSA_RR_NOAUDIO=1` | 12 | 3 | bimodal: mode A 6 identical, mode B 5 identical, 1 odd |
Controls: another seed (`RR_SEED=777`) diverges at frame 0 in `rng` only; another step (`RR_STEP_MS=33`) diverges in every subsystem: the hash sees the RNG and the clock. The host test `rr_diff_test.py` has 12 checks (valid case + 8 mutations of a synthetic trace).

### 9.3 Cost
`rr::OnFrame` hashes ~130 entities and writes ~400 bytes per frame: the whole 18-second route runs in the same 5 s as without it. Trace size 400 B/frame (300 KB for 900 frames), detail frames ~100 KB each.

### 9.4 Win32 neutrality, measured
`tools/portable/statehash_neutral.py` (preprocesses each touched shared TU with `NOTSA_STATEHASH` undefined and compares with the base commit, line markers and `__LINE__` literals ignored): OK for app_game.cpp, WinMain.cpp, SDLWrapper.cpp, LoadingScreen.cpp, Entity.cpp (macro in Base.h), World.cpp, Bike.cpp (probes reverted), Ped.cpp (Pool.h), TaskSimpleHoldEntity.cpp. Portable-only files (statehash, cdstream_portable, oswrapper_portable, dsound_al, msvc_rand.h, app_platform) are never compiled by the Windows build. NOT done: the wine/MSVC code-byte A/B (to run on the Windows host with the variant OFF).

## 10. Slices

All slices: one sonnet agent, <= 100 min, own build dir, `rr_selftest.sh` + `rr_diff_test.py` + `statehash_neutral.py` green before landing, report <= 15 lines.

| id | scope | acceptance |
|---|---|---|
| RR-2 | portable self-determinism to 100%: F6 (find why the ped skeleton is or is not animated when the prop reads it; check `bCalledPreRender`, `CAnimManager` block load state, `CPed::UpdateRpHAnim`, whether the first read happens in the frame the task is created), then F5 constructor fix proposal (value from the exe constructor; ask the Windows host through PORTABLE_SYNC), NaN guard proposal for `CPhysical::RemoveAndAdd`; audit remaining `NOTSA_PTR_ID32`/`(uint32)ptr` uses that reach RNG or game state; grep `unordered_*`/`std::map<T*>`/`std::sort` on pointers in gameplay code; MallocScribble + zero-stack variants must give the SAME trace as the plain run | `rr_selftest.sh` 30 runs identical (plain), 10 runs identical with `MallocScribble=1 MallocPreScribble=1` and with a zero-init-stack build; the three routes of RR-7 |
| RR-3 | Win32 variant: (a) CMake: `statehash.cpp` into the Windows target when `GTASA_STATEHASH`; make it build there (RNG word from `notsa::g_RandSeed`; the file already has the `#elif` branches; replace `dsound_al.h`/`mach` includes with `#ifdef NOTSA_PORTABLE`); (b) hooks: `GetOSWPerformanceTime` in `oswrapper_win.cpp` (virtual clock), `CdStreamRead` inline read in `CdStreamInfo.cpp`, `RwMalloc_` zeroing in `standalone/rw/engine.cpp`, pool zeroing already shared, `NOTSA_RR_NOAUDIO` (audio off on both sides), thread-local rand state for the Windows standalone run (row 7); (c) `tools/standalone/windows/rr_run.cmd`; (d) neutrality: guardlines/preprocess + wine A/B with the option OFF | the Windows host produces `win32.trace` from the same route and seed, and `win32.trace` is self-identical in 10 runs |
| RR-4 | recorder: `NOTSA_RR_RECORD=<file>` in a normal session writes the injector script (key down/up per CTimer frame as `waitframes:N`, pad axes, mouse deltas, teleports/spawns from the debug menu); `waitframes:` item in the injector | record 60 s of play, replay under `NOTSA_RR=1` twice: identical traces; the replayed player ends within 1 m of the recorded one in the SAME session type (documented limit: streaming races of the live session) |
| RR-5 | portable vs Win32 comparison: run `rr_diff.py win32.trace portable.trace` on the first route, expect divergence from float differences; classify the first divergences by subsystem/field with the detail window and hand them to Stream F as a list (file:function), with the quantised-level gap reported separately; trace compression/store under `.notes/rr_traces/` (git-ignored) | a table "frame of first exact divergence / frame of first quantised divergence" per route; every exact divergence before the quantised one is attributed to a float op (or fixed) |
| RR-6 | gate: `tools/portable/rr_selftest.sh` (3 runs, 20 s) as a step of the Mac orchestrator's gate; `portable_tests.cmake` entry for `rr_diff_test.py`; a golden-trace check: a committed 100-frame trace of the portable build (`tests/portable/rr_golden_walk.trace`, regenerated on purpose) so an unintended simulation change in a portable commit is visible as a diff | gate fails on a mutation of any hashed subsystem (e.g. flip one `rand()`) |
| RR-7 | more routes: drive (enter a car, accelerate, turn, crash), combat (spawn cop, wanted level 2, shoot), mission start (`cmd:mission:N` for a short early mission), 5-minute soak with traffic/pedestrians in full density (spawn commands removed, ambient population only); each route has a `rr_route_*.txt` and a golden self-determinism result | each route: 10 identical runs of 60+ s of game time, hash window >= 3000 frames |

Order: RR-2 first (nothing downstream is meaningful before the portable build agrees with itself), then RR-3 and RR-4 in parallel (disjoint zones: Windows host files vs injector + recorder), RR-6 with RR-2, RR-5 when RR-3 lands, RR-7 any time after RR-2.

## CONTINUE (for the next agent)
1. Start from `portable/main` (62433a54 or later), build dir `build/PortableMacRR` (cmake `-DGTASA_STATEHASH=ON`, copy of an existing portable build dir with the path fixed by sed; 33 s full rebuild), `tools/portable/rr_selftest.sh 90` (expect: 2 of 3 traces identical, bimodal, see F6).
2. F6: `RR_DETAIL=134-136 NOTSA_RR_NOAUDIO=1 tools/portable/rr_run.sh x 60` in a NaN-mode run and an OK-mode run, then `rr_diff.py`; add probes (`notsa::rr::Probe`) into `CPed::PreRender`, `CTaskSimpleHoldEntity::ProcessPed`, `CPed::UpdateRpHAnim` and print `bCalledPreRender`, the hierarchy pointer's `matrices` first float, and the anim association count of the ped that holds obj 1485.
3. Do not mask F5/F6 in the trace (the hash must see them); the pins only make the unspecified bytes the same on both sides.
