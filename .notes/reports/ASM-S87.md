# ASM-S87 (CAEAudioHardware game-side + CAEAmbienceTrackManager + CAECutsceneTrackManager, phase 3 tier A) - 2026-10-30
Result: 24 compared in full: 19 MATCH, 5 FIXED (5 fix commits), 0 UNSURE, 0 NOT REACHED. Table in S87.result.tsv. (UpdateAmbienceTrackAndVolume, RescaleChannelVolumes, UpdateReverbEnvironment, Query3DSoundEffects, GetBeatInfo = the 5 FIXED.)

## Fixes
| function | exe | what the port did wrong |
|---|---|---|
| CAEAudioHardware::GetBeatInfo | 0x4D8FA0-0x4D948E | rewritten from the asm. The port tested `BeatNumber != 0` (exe: `>= 0`), used unsigned compares, wrote `BeatWindow[10 - i]` / `[10 + i]` (the exe fills [9..0] with the 10 beats before the found index and [10..19] with the index and the 9 after it, times relative to the play time shifted by -50), did not clamp/shift as the exe, and NEVER copied `gBeatInfo` into the caller's struct, so `CAudioEngine::m_BeatInfo` (script beat commands, Group19) stayed stale. Kept from the exe: with no active track the state is cleared and the function returns without the copy |
| CAEAudioHardware::UpdateReverbEnvironment | 0x4D8DA0 | `none_of` test inverted: the exe stores the new environment/depth only when a channel's SetReverbAndDepth returned true |
| CAEAudioHardware::Query3DSoundEffects | 0x4D8490 | on a failed 3D-buffer QueryInterface the exe returns; the port released the buffer and carried on with a null 3D buffer (EAX property sets remain `#if 0`: no EAX on the dsound_al shim) |
| CAEAudioHardware::RescaleChannelVolumes | 0x4D8990 | the unduckable full sum adds the unrounded pow result still on the x87 stack (`fst dword` keeps st0; one rounding to 24 bits), not the stored float gain; the clamp arm returns 0 for NaN |
| CAEAmbienceTrackManager::UpdateAmbienceTrackAndVolume | 0x4D6E60 | sandstorm volume is the inline x87 `fldlg2/fyl2x` log10 (x87::log10), not std::log10; volume/frequency ramps are `a < b ? a : b` (std::min differs for NaN), the `<= -34` test and the underwater frequency pick follow the exe's C0 (below or unordered) flag semantics |

## Checked and equal
PlaySound (incl. the original chIdx > 0x40 off-by-one, percent start via floor+ftol, SetNotInRoom), GetChannelPlayTimes (last channel not written), StopSound, SetChannelPosition, the rest of RescaleChannelVolumes (maxima, 0.5/1.2 release, fade-near-end, master scale, faders, AudioLog10 path), PlayTrack, StartTrackPlayback, GetTrackPlayTime, Pause/ResumeAllSounds, Service (IsForegroundApp is a pure global read; SynchPlayback loop before Service loop; UpdateVirtualChannels arg order); ambience Reset/PlaySpecial/StopSpecial, the whole zone->track table (jump table decoded: 41 tracks, camZ and distance volumes, 11 radio ids, favourite radio), riot/rain/sandstorm tracks, Service state machine (table 0..7, start offset 0 / random 0..600000 / prev + max(elapsed, 7000)); cutscene manager Initialise/Stop/Pause/PreloadBeatTrack (14-entry table dumped from the exe)/Service (table 0..7).

## Not changed on purpose
Box audio zones: the exe leaves the zone position uninitialised (stack garbage) for the distance-volume zones 5/10/13 when the active zone is a box; the port zero-inits. The original frequency fallback to the volume (`m_FreqFactor = volume`) stays behind !FIX_BUGS. EAX paths of Query3DSoundEffects/SetReverbAndDepth stay `#if 0` (backend).

## Tests
wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe, rebuilt after the last edit): 1142 functions, 0 mismatches (PC24 0 / 0, PC53 0 / 0). PortableWin64: AEAudioHardware.cpp and AEAmbienceTrackManager.cpp compile (x64 cl, flags from the main checkout's compile_commands without the PCH, objects in scratch). Pre-commit grep clean. Combat regress: see the result line in the agent report.
