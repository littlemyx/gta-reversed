# ORACLE-W3-AUDIO2 report

Agent ORACLE-W3-AUDIO2 (rules v3), start 01:05, end ~01:50 (2026-10-21), worktree C:/gta-wt/oracle-w3-audio2, branch oracle-w3-audio2 (based on origin 11f04cb9). Logs C:/gta-scratch/w3audio2/.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game (tool count) | 4659 | **4685** (+26) |
| Audio/Hardware/AEAudioHardware.cpp uncovered A/B | 19 / 0 | **0 / 0** |
| Audio/Entities/AEPedSpeechAudioEntity.cpp uncovered A/B | 1 / 9 | 1 / 5 (4 rows) |
| Audio/Entities/AEVehicleAudioEntity.cpp uncovered A/B | 2 / 7 | 1 / 5 (3 rows) |
| port bugs fixed | | **4 functions** |

## New target `wave3_audiohw_oracle_test` (19 rows, PC24 0 mismatches)
AEAudioHardware.cpp (+ AEMP3BankLoader.cpp) is linked for real, so the CAEAudioHardware recording stubs of `wave3_oracle_audio.inc` are compiled out (`W3_AUDIOHW`); rows in `tests/standalone/wave3_oracle_audiohw.inc`. `this` is the exe's own global (0xB5F8B8, same address on both sides, in the compared data image), the bank / track loaders it points to are garbage objects of the fixture (3 bank slots with random headroom, DVD path). All 19 closures are pure (loader / stream-thread forwards run the real code), so no DirectSound / OpenAL stubs were needed: Constructor (zeroed object: the exe writes only a subset, the port also writes the other NSDMI defaults, all zero in the exe), AllocateChannels, RequestVirtualChannelSoundInfo (vch < 300: the exe ignores vch >= 300, the port asserts), GetNumAvailableChannels, SetChannelVolume, SetChannelFrequencyScalingFactor, SetChannelFlags, GetSoundHeadroom, GetVirtualChannelSoundLengths / LoopStartTimes, StopTrack, GetTrackLengthMs, GetActiveTrackID, GetPlayingTrackID, Get{Effects,Music}MasterScalingFactor, GetEffectsFaderScalingFactor, IsStreamingFromDVD, GetDVDDriveLetter. No port mismatch: this TU is exact (1 min 18 s alone).

## Rows added to `wave3_audio_oracle_test` (wave3_oracle_audio6.inc) and port bugs fixed
Pre-fix counts = PC24 failing cases of the final rows on the unfixed code (the three reserve/settings rows from the first run, CanWePlayGlobal from a rebuilt pre-fix tree).
| function | asm | result | pre-fix | cause / fix |
|---|---|---|---|---|
| CAEPedSpeechAudioEntity::ReservePedConversationSpeechSlots | 0x4E37F0 | FIXED | 435/800 | port took two slots from the round-robin GetFreeSpeechSlot (and advanced s_NextSpeechSlot); exe: first two free slots of 0..4 in index order, s_NextSpeechSlot untouched |
| ReservePlayerConversationSpeechSlot | 0x4E3870 | FIXED | 409/800 | same: lowest free slot, no round robin |
| CAEVehicleAudioEntity::GetVehicleAudioSettings | 0x4F5C10 | FIXED | 424/424 | exe subtracts 0x190 from its argument (model id); the port added MODEL_VEHICLE_FIRST to its argument and its one caller subtracted it first: an exe-style argument indexed past the table (assert) |
| CAEPedSpeechAudioEntity::CanWePlayGlobalSpeechContext | 0x4E5F10 | FIXED | 596/3000 | conversation slot -1 falls through to the player check and then a free slot (port indexed the table with -1); free slot taken (s_NextSpeechSlot advanced) BEFORE the next-play-time test (port tested time first); no fault |
| CanWePlayScriptedSpeech 0x4E4950, CAEVehicleAudioEntity::StaticInitialise 0x5B99F0, UpdateGasPedalAudio 0x4F5EB0 (types 0 / 9: automobile / bike; the exe ignores other types, the port treats anything else as an automobile - no caller passes one) | | 0 mismatches | | |

## Skipped (tests/standalone/tools/w3_audio_skip.tsv, reasons per line)
- CAEPedSpeechAudioEntity::Constructor 0x4E4F10: diagnostic run on a zeroed object differs only in bytes the exe constructor does not write (base-class CAEAudioEntity fields, initialised by the caller in the exe) and in m_PedAudioType (exe 0, port -1 UNK; read behind m_IsInitialized): not a row, not changed.
- GetSpeechContextVolumeOffset (needs Stats.cpp linked), GetSexFromModel (model info table), LoadAndPlaySpeech (hardware PlaySound stubs), RequestPlayerConversation (two ped fixtures), ReleasePlayerConversation (padding only), vehicle Constructor / Initialise / JustGotInVehicleAsDriver / ProcessVehicleSkidding / ProcessPlayerTrainBrakes / ProcessSpecialVehicle (vehicle, wheel, train fixtures): budget (250k tokens), not written.

## Gates / runtime
- `wave3_audiohw_oracle_test`: 19 functions, PC24 0 (final tree). `wave3_audio_oracle_test`: see the numbers appended by the final run below.
- Changed port TUs also link into world_oracle_audio / audio_fade / other targets that include AEPedSpeechAudioEntity.cpp / AEVehicleAudioEntity.cpp (wave2 ped / tasks via AEPedSpeechAudioEntity.cpp): the orchestrator's run_all_tests covers them; I ran only the two audio targets.
- Runtime: the whole `wave3_audio_oracle_test` takes ~8 min under load (baseline 8m18 before this work, 164 functions); the new rows add < 1 min. The target < 5 min is met for the new target (1m18) and for the new rows; the old audio target was already over it (the 4000-case x 2 precision full-data-image compare per case) - splitting it is a follow-up.

## Final runs (tree = the pushed branch)
`wave3_audio_oracle_test`: 171 functions, PC24 0 / 0 mismatches, PC53 1400 (informational, same as before this work), 610 s wall clock (low priority, alongside the audiohw run and the orchestrator's activity; baseline before this work 498 s with 164 functions). `wave3_audiohw_oracle_test`: 19 functions, PC24 0 / 0, 75 s. The old audio target is over the 5-minute target on its own (not the new rows); splitting it per topic into targets is the follow-up.
