# FIX-AUDIT: behaviour-changing switches in the run build + the GetDefaultCopCarModel question (Windows host, 2026-10-17)

Commits: 8e93b9d5 (oracle harness), 367669fe (bugfix switches OFF in the run build + oracle rows), docs/report commit after it. Worktrees C:/gta-scratch/wtfix (Release, Detached, StandaloneRW) and wtfixb (DLL Debug baseline / after), logs `C:/gta-scratch/fa_*.log`.

## Verdict
1. **All 17 `notsa::bugfixes::` definitions (37 call sites) were ON in the run build and each one deviated from the exe.** They are now OFF there (`ReversibleBugFix::Enabled{ detail::kDefaultEnabled }`, false under `NOTSA_STANDALONE_RUN`; the DLL build keeps ON and its object code is unchanged). Before: plates with A-Z/0-9, rear-right light bug fixed, framerate-dependent audio fades and map zoom, stray road-switch areas, climb clamp, ... (table below). Nothing deleted; re-enable per fix with `.Enabled = true` in `Bugs.hpp`.
2. `FIX_BUGS` is **not** defined in the run build (CAM-INIT confirmed: 0 hits in `compile_commands.json` of StandaloneRelease / Detached / RW; CMake adds it only `if (NOT STANDALONE_RUN_BUILD)`); `notsa::IsFixBugs()` is false in all 73 files that use it; `#error` now if both `NOTSA_STANDALONE_RUN` and `FIX_BUGS` are set.
3. **Two deviations kept on purpose** (needed to run, not observable by game logic): the `GenericCrashing` site in `C3dMarker::DeleteMarkerObject` (`m_Mat.Detach()`) and the unconditional upstream `wheelSizeFront/Rear = 0.7f` default of the vehicles.ide parser. Rows in `tools/standalone/standalone_deviations.tsv` and `docs/STANDALONE_FIDELITY.md`.
4. **GetDefaultCopCarModel: a test-harness artefact, not a behaviour difference** (section 1). Fixed in the harness (8e93b9d5): Detached 567 -> 0 mismatches.
5. One more exe difference found by reading the config structs, fixed: `FastLoaderConfig::RenderAtAllTimes` (upstream default `true`, "keep running the front end while minimised") is `false` in the run build, as in the exe (0x748CB2).

## 1. GetDefaultCopCarModel (567 PC24 mismatches in StandaloneDetached, 0 in StandaloneRW)
Not codegen, not /arch, not x87, not a bugfix switch, not detached globals (the oracle test forces address-mode globals, `NOTSA_ORACLE_ADDRESS_GLOBALS`, so port and exe read the same memory in every config). Evidence:
* The 567 were two rows (misc 81 + pop 486 of 3000 cases each), all with `ignoreLvpd1Model == 0`; every failing case had the port take the bike-cop branch (`got = ms_DefaultCopBikeModel`) while the exe did not (`exe` = the level's car, or -1).
* The exe (0x407C50) does `mov esi,[esp+0xC] ... test esi,esi` on the **whole 32-bit argument dword**; the port's `bool` reads a byte.
* The harness called `oracle::Fn<int __cdecl(bool)>(0x407C50)(ig)`. Under /O2 (Release / Detached) the caller emits `mov al,[ecx]; push eax` (disassembled in the old `logic_oracle_test.exe` at 0xDA36A0): only `al` is written, the upper 24 bits of the pushed dword are old register contents. The exe's own callers always push a clean `push 0/1`. Under /Od (StandaloneRW) the compiler widens the bool, hence no garbage there.
* Fix: declare the parameter `int` and pass `(int)ig` (both rows). Detached result: both rows 0 / 0 mismatches (3000 cases, PC24 and PC53). The rule (bool/char/short arguments to exe functions that test the dword must be widened by the caller) is documented in `tests/standalone/game_oracle.h`. The other 53 `Fn<... bool ...>` sites pass today because their exe functions read the byte only.
* No shipped-build difference: the port function reads a `bool` it receives from port callers (clean).

## 2. Inventory (run build = StandaloneRelease / StandaloneDetached; "before" = state at 2e3b9135)
`notsa::bugfixes::*` (Bugs.hpp), state in the run build before / after:

| switch | uses | what ON changes | exe behaviour | observable in play | before | after |
|---|---|---|---|---|---|---|
| CPool_DestructOnClear | Pool.h Flush/Clear | element destructors at pool shutdown | CPools::ShutDown 0x5519F0 frees storage only | quit-time AV (BUG-REFS) | ON, forced OFF in 3f468b99 | OFF |
| CCustomCarPlateMgr_GeneratePlateText_MissingLettersAndDigits | CustomCarPlateMgr.cpp 441/447 | letters A-Z, digits 0-9 | 0x6FD5B0: A-W, 0-8 | **yes, every plate** (and the RNG stream) | ON | OFF; oracle row equal, ON fails (2747/3000) |
| CDamageManager_GetLightStatus_IncorrectStatusCheckForLightRR | Vehicle.cpp 6709 | rear-right light uses the RR status | uses the rear-left status | **yes** (damaged lights) | ON | OFF |
| GenericFrameRate (12 sites) | AEVehicleAudioEntity gas pedal / nitro / rotor / dummy engine / fades, MenuManager_Input map zoom (x2), + bike bell (made switchable here) | `pow(step, timestep)` / wall-clock scaled steps instead of the constant step | constant step per frame (e.g. 0x4F9B08 bell `-= 1.5`) | yes, at timestep != 1 (audio fade lengths, map-zoom speed) | ON (bell unconditional) | OFF |
| CAEVehicleAudioEntity_PlayBicycleSound_VolumeFix / PlayAircraftSound_VolumeFix | AEVehicleAudioEntity 4393 / 3271 | `volume += m_EventVolume` | 0x4F9710 / 0x4F93C0 do not add it | yes (loudness) | ON | OFF |
| CAECollisionAudioEntity_PlayLoopingCollisionSound_InvalidSurfaceType | AECollisionAudioEntity 240 | valid-surface check (BMX `AE_SURFACE_TYPE_*`) | indexes g_surfaceInfos with it (OOB) | BMX collision loops | ON | OFF |
| CPathFind_SwitchRoadsOffInArea_StrayAreas (2) | PathFind.cpp 2412/2432 | isCars filter, toggle == back-to-original, correct compaction index | 0x452C80 without these (R* index bug) | yes, after missions (path switches) | ON | OFF |
| CCarCtrl_RemoveDistantCars_UseAfterFree (2) | CarCtrl.cpp 4452/4462 | remove first, iterate after | 0x42CD10 reads `veh` after `PossiblyRemoveVehicle` deleted it (slot memory intact) | ordering of pool slots / roadblock ped creation only | ON | OFF |
| CTaskSimpleClimb_ProcessPed_SettleSpeedFrameRate | TaskSimpleClimb 162 | clamp the settle speed to 0.2 | no clamp | yes at > 60 fps (the exe's Idle lock still allows ~71 fps) | ON | OFF |
| PS2CoronaRendering | Coronas.cpp 427 | wet-road reflection z | RwIm2DGetNearScreenZ | slight (depth of reflections) | ON | OFF |
| AnimBlendSequence_SetName_SetBoneTagFlag | AnimBlendSequence 126 | clears the bone-tag flag | 0x4D0C50 only stores the hash | on animation load only | ON | OFF; oracle row equal, ON fails (1521/3000) |
| AESound_UpdatePlayTime_DivisionByZero | AESound 176 | skip `% 0` | 0x4EF2E0 `idiv` (#DE) | no (crash only) | ON | OFF |
| CTaskComplexLeaveCarAndFlee_MissingNullCheckForVehicleOnFlee | LeaveCarAndFlee 53 | null check | 0x63C0A0 derefs | no (crash only) | ON | OFF |
| GenericUB (2) | PedGroupIntelligence 198/237 | un-alias primary / event-response allocator | double delete if aliased (the exe never aliases) | no | ON | OFF |
| GenericOOB | Formation 21 | loop bound 8 instead of 24 | 0x699FA0 loops 24 (reads past the array, unreachable) | no | ON | OFF |
| GenericCrashing (5) | 3dMarker 82, TagManager 128/150/177, Weather 558 | null checks | derefs null (crash) | no | ON | OFF, **except 3dMarker (kept ON, section 3)** |

Not switches but compiled into the run build (all checked):

| item | state in the run build | exe |
|---|---|---|
| `FIX_BUGS` / `notsa::IsFixBugs()` (73 files) | undefined / false (+ `#error` guard) | same |
| `bFixBugs` (Automobile.cpp 2305) | false (from FIX_BUGS) | same |
| `FastLoaderConfig` (all `No*` flags, `SaveGameToLoad = -2`) | defaults = vanilla flow; `Load()` is only called from dllmain.cpp (never in the run exe) | same |
| `FastLoaderConfig::RenderAtAllTimes` | upstream `true` -> **now `false`** in the run build | WinMain 0x748CB2 skips the front-end idle while minimised |
| `MiscConfig` (`SaveDirectoryPath`, `LoadSavesWithMismatchingVersion`) | defaults, `Load()` never called | same |
| `NOTSA_VANILLA_COLLISIONS` (defined in Collision.cpp), `NOTSA_DEBUG_SLOW_COLLISION_OFF` (undefined), `s_DebugSettings` (Collision / Clouds / Coronas / Weapon: normal paths, `NoShotDelay` false) | vanilla | same |
| `NOTSA_DEBUG`, `_DEBUG` extras, ImGui/UIRenderer, `USE_ADDITIONAL_CHEATS` (declarations; reachable from the ImGui cheat module only), `IMPLEMENT_UNSUPPORTED_OPCODES` (13 extra III/VC opcodes the retail SCM never uses), `MORE_LANGUAGES` (extra GXT names), `NOTSA_ASPECT_RATIO_SCALE` (undefined), `TRACY_ENABLE` (undefined), `EXTRA_DEBUG_FEATURES` | inert for retail data / off | same |
| `NOTSA_WINDOWED_MODE` (compile option, needed) and env `NOTSA_STANDALONE_WINDOWED` / `MUTE` / `SKIP_VIDEOS` | the first adds one `RwCameraClear(Z)` after `InitialiseCoreDataAfterRW` (WinMain 269) and the windowed device; the env switches are off for the user (documented rows) | exclusive fullscreen |

Unconditional NOTSA guards without a switch (about 25 found by grep: null checks in `BoneNode.cpp`, `IKChain.cpp`, `Vehicle.cpp 6812`, `PedAttractor.cpp 144`, `TheScripts.cpp 1059`, `CharacterCommands.cpp 519`; `InteriorManager_c.cpp 280` (stop at capacity), `Population.cpp 210` (stop at the last group), `TagManager::GetNearestTag` returns null, `FileLoader.cpp` `\r` filter / `%u`, `CPopCycle::FindNewPedType` initial `outPedMI`, ...): they turn a crash / out-of-bounds write of the exe into a no-op or are inert for valid data; none changes a state the exe reaches. Only the bike-bell `pow` (above, now gated) and the vehicles.ide wheel-size default (kept, section 3) were observable. The remaining ~800 "NOTSA" comments are structure notes (std containers, lambdas), not behaviour changes; I did not read all of them.

## 3. Kept deviations (reason: the port cannot run reliably without them)
* `C3dMarker::DeleteMarkerObject` keeps `m_Mat.Detach()` (source `#ifdef NOTSA_STANDALONE_RUN` branch, a `#line` keeps the DLL object code identical). The exe's inlined `C3dMarkers::Init` (0x724E40) only zeroes the matrix attach pointer; the port's `rng::fill(m_aMarkerArray, C3dMarker{})` is `CMatrix::operator=`, which writes 64 bytes through the stale `m_pAttachMatrix` into the freed RwFrame (at every game restart after a marker existed): heap corruption the exe does not have.
* `FileLoader.cpp` vehicles.ide parser: `wheelSizeFront/Rear{ 0.7f }` (upstream NOTSA fix of the "Win11 24H2" bug): the exe reads uninitialised stack when the line has fewer fields; the value depends on the Windows version.

## 4. Verification (base 2e3b9135 + this diff)
| gate | result |
|---|---|
| DLL Debug build (`gta_reversed.asi`, wtfixb), baseline vs after, `obj_code_sig.py` (NORM_STRINGS, 1137 objects) | builds; **1136 identical**; the 1 difference is `AEVehicleAudioEntity.cpp.obj` = the bike bell now written with `notsa::step_down_to(.., GenericFrameRate)` (same value with the switch ON). `3dMarker.cpp` (`#line`) and the `FastLoader.hpp` users are identical |
| StandaloneRelease `gta_reversed.exe` | builds (1184 steps, 0 failures) |
| StandaloneDetached `gta_reversed.exe` + `logic_oracle_test` | builds; `GetDefaultCopCarModel` rows 0 / 0; new rows `GeneratePlateText` 0 and `SetName` 0 (3000 cases each); mutation (`LOGIC_ORACLE_FIXES_ON=1`: the two switches ON at run time) fails 2747 and 1521 |
| StandaloneRW (Debug) 42 `*_test` + `gta_reversed.exe` | builds (1840 steps, 0 failures) |
| run_all_tests.sh build/StandaloneRW (GPU lock) | section 5 |
| soak 3 min (StandaloneRelease, silent, 960x600, own data dir) | `RESULT: OK`, 0 errors, 10200 frames, last marker `quit_ask`, exit via atexit, 24.9 fps, 1 gate timeout (`invehicle`, retried by the route) |

## 5. Test run
`tools/standalone/run_all_tests.sh build/StandaloneRW` (SKIP_BUILD=1, under the GPU lock, log `C:/gta-scratch/run_all_tests.124085`): **42 tests: 42 ok, 0 SKIP, 0 fail** (logic_oracle_test 229 rows = the former 227 + GeneratePlateText + SetName, 0 mismatches; audio_fade_oracle 6000, pool_oracle 1.9 M, rw_skin_cpu_oracle 240000, ...). Debug config, so it covers the switches-OFF state in every oracle (the pop / audio / vehicle oracles ran with the fixes ON before; they are not affected: the tested paths are equal for timestep 1 / valid inputs).
Final soak after the last change (`RenderAtAllTimes`, Release exe of a7946b29 + diff): `RESULT: OK`, errors 0, 9800 frames, atexit exit, last marker `quit_ask` (first soak with the bugfix change: 10200 frames, also OK).

## 6. Open / notes
* The fixes could be offered as an opt-in (env `NOTSA_FIXES=name,...` read at start-up) if the user wants e.g. the climb clamp or the full plate range; not done (no request). Candidates with a player-facing benefit: `CTaskSimpleClimb_..._SettleSpeedFrameRate` (death on a wall above 60 fps; the exe has it too), plates, light RR.
* `Pool.h::Flush` still memsets the freed storage (0xDD) in the run build (harmless, BUG-REFS).
* Oracle harness rule: pass `bool`/`char`/`short` arguments to exe functions as `int` unless the exe function reads only the byte.
