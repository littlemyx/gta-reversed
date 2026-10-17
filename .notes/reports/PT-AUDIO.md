# PT-AUDIO report: user playtest 1, U12 (mixer breaks in dense scenes) + U13 (all sounds die, the radio keeps playing) (Windows host, 2026-10-18)

Start: `reverse/interior_c` @ e4e0b455 (user's exe: StandaloneRelease @ 66d98061). Evidence: `C:/gta-scratch/playtest1/log.log`, the user's `gta_sa.set` (SFX 64, radio 64, radio auto-select ON, i.e. the radio plays in cars).
Worktree `C:/gta-scratch/pta`, runs/logs `C:/gta-scratch/pta_out/{run1,nofix,fix,nofix2,orig,soak3}`, helper scripts `C:/gta-scratch/pta_*.sh`, `C:/gta-scratch/pta_tools/*.py`.

## Verdict (one line)
**Root cause of U12 + U13: `CAECollisionAudioEntity::UpdateParameters` (exe 0x4DA370, vtable 0x862E64 slot 0) was never ported.** The DLL build kept the exe's through the vtable, the standalone build runs the empty base
`CAEAudioEntity::UpdateParameters`: one-shot entries of the 300-entry collision list were never released, so after ~300 collisions the list is full, and from then on every continuous-contact frame (skid / scrape / water loops)
creates one more endless looped sound (`PlayLoopingCollisionSound` is called again each frame because no LOOPING entry is found, `AddCollisionSoundToList` refuses it, nothing ever stops the sound) -> the 300-sound table and the 47 hardware
channels fill with leaked loops, `RequestNewSound` refuses everything, only the radio (its own streaming channel) survives. Fixed in this report's commit; oracle row 4000/4000 at both FPU modes, mutant caught.

## Evidence

| # | fact | source |
|---|---|---|
| 1 | the user's log has **2234 x "Collision sound list is full"** (`AECollisionAudioEntity.cpp:73`), first at 22:57:25, last 23:00:54 (log clock), i.e. in the last 4 minutes before the crash; nothing else audio-related is logged | `playtest1/log.log` |
| 2 | exe vtable of `CAECollisionAudioEntity` (0x862E64) = `[0x4DA370, 0x504B70]`; the base vtable 0x859988 slot 0 is the empty 0x4D7E30. `0x4DA370` / `0x4da370` appears **nowhere** in `source/` or `tests/` before this fix (`RH_ScopedVirtualClass(CAECollisionAudioEntity, 0x862E64, 1)` had no `RH_ScopedVMTInstall`) | `exedis.py`, grep |
| 3 | the exe's function: events 1/2/3/128 are handled by a jump table (0x4DA47C, byte index table 0x4DA490: event 1 = nothing, 2 -> speed 1.26, 3 -> 0.94, 128 -> 0.56 once `_ftol(ClientVariable) <= timer`); every other event, when `curPlayPos == -1` (the sound ended), finds the list entry whose `Sound == sound` (stride 0x14 from this+0x208) and resets it (EntityA/B/Sound = 0, Status = 0, surfaces = 0xC3, **LoopStopTimeMs untouched**), `dec [this+0x204]` | asm 0x4DA370..0x4DA477 |
| 4 | `PlayLoopingCollisionSound` is reached whenever `GetCollisionSoundStatus` finds no LOOPING entry (`ReportCollision`, repo line ~669-677); with a full list `AddCollisionSoundToList` refuses the new looped sound (`SOUND_REQUEST_UPDATES`, no entity tie), `Service()` only stops LOOPING entries of the list, so the sound is endless | code |
| 5 | **measured, same scripted scene** (`tools/standalone/audio_dense_route.txt`: new game, walk, car cheat + drive, Rhino cheat, 6 x ram / cannon / steer, idle; ~6.5 min): ours WITHOUT the override: collision list at the end **99 used, all one-shot, never released** (366 adds) and growing linearly; ours WITH the override: 0 used at the end, max 6 (255 adds); **original exe** (sampled from outside, `tools/standalone/audio_stats_exe.py`): max 5 used, one-shot <= 2, 3 looping at the end | `pta_out/{nofix2,fix,orig}` |
| 6 | the old run1 (route without a car, no collisions) already showed the leak rate even when idle: 27 one-shot entries stuck out of 60 adds in 5 min | `pta_out/nofix` |

Mixer statistics of the three runs (1 sample/s, `act` = active sounds of the 300-sound table, `chUsed` = hardware channels of 47, `want` = audible sounds without a channel, `refused` = requests that found the table full):

| run | act mean / p95 / max | chUsed max | want | refused | collision list at end / max |
|---|---|---|---|---|---|
| ours, override ported (this fix) | 15.7 / 24 / 37 | 37 | 0 | 0 | 0 / 6 |
| ours, override missing (HEAD) | 17.0 / 29 / 40 | 40 | 0 | 0 | **99** / 99, growing |
| original exe (scene not identical: its car is not left, no walk) | 21.7 / 35 / 44 | 44 | 0 | 0 | 3 / 5 (one-shot <= 2) |

Neither exe gets near the 47 channel limit in these scenes (no starvation), the collision-list behaviour of the fixed port equals the original's. The scene is NOT the user's gang / police scene (wanted level cannot be raised
this early in the game, `OSRBLHH` has no effect, the car cheat does not work from the alley), so the 300-sound exhaustion itself was not reproduced end to end: it follows from (4) and the leak rate (5) at the user's ~2200 refusals.

## What else was checked (no further cause found)
* D-C56 zero-init of the audio objects: all audio globals pass the verifier (`D-C56.md`); `CAECollisionAudioEntity::Initialise` (exe 0x5B9BD0) was `*this = {}` in the port: the exe stores the history = -1 (0xFF bytes), `m_LastBulletHitSoundID` = -1, list entries zero / 0xC3, and leaves the base class (`m_tempSound`, `m_Entity`) alone; the port had `history[0] = 255, rest 0` and re-initialised the base. Fixed together (`ChooseCollisionSoundID` compares against the history). `Reset` (0x4DA320) lacked the `dec m_NumActiveCollisionSounds`, `Reset` / `Service` cleared `LoopStopTimeMs`, which the exe does not: all three now release an entry like the exe.
* Channel allocation / counts: `m_nNumChannels` = 48 (no hardware 3D buffers: `DSCAPS.dwFreeHw3DAllBuffers < 24`), 47 for the sound manager + the stream channel, same code path as the exe (hardware layer untouched). 0 `CreateSoundBuffer` / `Lock` / `Play` failures, 0 `PlaySound` without buffer, over all runs.
* Faders / pause flags (U13 candidates "global volume stuck", "paused flag not undone"): logged every second, `effM = effF = musM = musF = nonStrF = 1.0`, `man = 0`; the stream fader is 0 only while a cutscene runs (`CAudioEngine::Service`) and returns to 1; `timerPause` follows the pause menu. No stuck state in any run.
* FIX-AUDIT change (fades constant per frame instead of `pow(step, timestep)`): the exe's behaviour, and not a mixer-capacity issue.
* `NOTSA_STANDALONE_MUTE` / `NOTSA_AUDIO_TAP` leakage into the user's runs: `play.cmd` sets `NOTSA_STANDALONE_MUTE=0` (the mute needs a value other than empty / `0`) and clears the tap variables; the new `NOTSA_AUDIO_STATS(_EVERY)` were added to its clear list. Nothing of it is active in `C:\gta-play` runs.
* `CAEAudioHardware::GetChannelPlayTimes` skips the LAST channel of the slot (exe quirk, kept): the caller's `new int16[]` garbage is what the last channel (index 46) reports; ours showed `lastChPos = 2991` (heap leftovers), the exe's is whatever its heap held: a sound that lands on channel 46 (only when 46 others are busy) never reports "ended". Original behaviour, not a regression; only reachable in very dense scenes.
* The old "crackle like a loose mini-jack" note (PHASE2_STATUS 2026-10-10): not related to this (the tap found nothing in earlier runs, nothing new here: 0 STALE_SLOT / WRITE_IN_PLAY anomalies in `pta_out/*/tap.log`); still needs the user's ears.

## New tooling (run build only, env-gated, silent)
* `NOTSA_AUDIO_STATS=<file>` (`_EVERY=<frames>`): `source/game_sa/Audio/Hardware/AEAudioStats.h` + `notsa::audio_stats::Service()` (end of `CAESoundManager::Service`): collision-list fill, active / physical sounds, channels, `want`, loops, oldest sound, DS buffers, requests / refused, `PlaySound` outcomes, `CreateSoundBuffer` / `Lock` / `Play` failures with HRESULT, pause flags, faders; a refused request dumps a (bank, sound, event, owner vtable) breakdown. docs/STANDALONE.md.
* `tools/standalone/audio_stats_exe.py`: the same quantities from a running ORIGINAL `gta_sa.exe` (ReadProcessMemory: `CAESoundManager` 0xB62CB0, `CAEAudioHardware` 0xB5F8B8, collision entity 0xB6C14C).
* `tools/standalone/audio_dense_route.txt` (ours, soak.sh format) and `tools/standalone/windows/audio_scenes.txt` (original, inputdrv.exe; `inputdrv.c` got the `lctrl` key).
* `tools/standalone/vtable_override_audit.py`: virtual overrides the exe has and the port has no address for (the class of this bug). Result for the whole tree after this fix: 92 classes with an unmentioned override, 33 after dropping scalar-deleting dtors and `Clone` factories (mostly getters the port inherits from its C++ base), full list in `.notes/reports/PT-AUDIO_vtable_audit.txt`; worth a triage by the owners: `CTrain` slots 5 / 14 / 17 / 18 / 29 (0x6F5530, 0x6F7190, 0x6F5570, 0x6F55C0, 0x6F5930), `CAutomobile` slot 5 (0x6A6590), `CEvent` slots 11 / 14 (0x4ABC30, 0x4AC020), `CEventGunShot` slot 12 (0x4AC6E0), `CEventVehicleHitAndRun` slots 5 / 7, `CEventInAir` / `CEventHitByWaterCannon` slot 4, `CVehicle` slot 49 (0x6B5860), `CTaskComplexEvasiveCower` slots 9 / 10, `CTaskSimpleUninterruptable` slot 6, `CTaskSimpleRunAnim` slot 4. (None of them is audio; not triaged here.)

## Verification
| gate | result |
|---|---|
| Release build (`build/StandaloneRelease`, the user's configuration) | ok |
| oracle row `CAECollisionAudioEntity::UpdateParameters 0x4DA370` (`world_oracle_audio.inc`, fake entity + fake sound, whole-object byte compare, event / pos / timer / ClientVariable random incl. out of range) | 4000 / 4000 at PC24 and PC53; mutant (override disabled) -> 913 mismatches. `source/CMakeLists.txt`: `AECollisionAudioEntity.cpp` added to `world_oracle_test` |
| `run_all_tests.sh` (StandaloneRW, `GTA_EXE` = the oracle exe) | **44 / 44 targets ok** (25 oracle + fp + fade tests, then the 21 rw_* tests under the GPU lock; `world_oracle_test` 117 rows incl. the new one, 0 mismatches) |
| dense scene vs original | collision list = original (above); no refusals, no starvation in both |
| soak 3 min (`soak.sh build/StandaloneRelease 3`, stats + tap on) | `RESULT: OK`, 9800 frames (6.5 min, the default route: new game, walk, car, Rhino, idle), 0 errors, exit via atexit, 25 fps like before. The three scripted dense runs (`fix`, `nofix2`, `orig`) ran 5-6.5 min each, tap: 0 anomalies of every kind (STALE_SLOT, WRITE_IN_PLAY, SETVOL_REJECTED ...) |
