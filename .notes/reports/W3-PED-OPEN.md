# W3-PED-OPEN report

Agents W3-PED-OPEN and W3-PED-OPEN-2 (rules v2), worktree C:/gta-wt/w3-ped-open, branch w3-ped-open. Finishes items 1, 3, 4 of ORACLE-W3-PED.

| | before | after |
|---|---:|---:|
| game_sa/Entity/Ped covered | 94 | **102** |
| uncovered A / B in Entity/Ped | 3 / 10 | **1 / 4** |
| exe-oracle coverage, whole game | 3029 | **3037** |
| wave3_ped_oracle_test rows | 64 | **72** (PC24 0, PC53 0) |
| port bugs fixed | | **3** (one commit each, asm address in the subject) |

## Rows added
- Un-skipped (speech + real CWanted fixture): CPed::EnablePedSpeech 0x5EFF70, EnablePedSpeechForScriptSpeech 0x5EFF90, CanPedHoldConversation 0x5EFFA0, CPlayerPed::SetWantedLevel 0x609F10, SetWantedLevelNoDrop 0x609F30, CheatWantedLevel 0x609F50.
- Hand rows: CPed::operator delete 0x5E4760 (pool ped), CPlayerPed::CanIKReachThisTarget 0x609F80 (13 weapon slots, 3000 cases, NaN / on-the-edge targets).
- Fixture: WireWanted (zeroed real CWanted: chaos / wanted levels, flags, queued crimes, exe statics MaximumWantedLevel / MaximumChaosLevel / cheat byte, stat help message switched off), WireSpeech (CPed+0x294: init flag, audio type 0..5, voice below the per-type table, disable flags).

## Port bugs (each row failed on the pre-fix code)
| function | asm | bug |
|---|---|---|
| CPlayerPed::CanIKReachThisTarget 0x609F80 | fcompp + `test ah,5 / jp` | returns false only when 2D distance < height difference; the port's `>=` was false for NaN (cases 5, 8) |
| CWanted::SetWantedLevel 0x562470 / SetWantedLevelNoDrop 0x562570 | no logging in the exe | port-only NOTSA_LOG_DEBUG allocated 2384 bytes / 44 allocs per call and faulted under the arena (all 3 wanted rows) |
| CAEPedSpeechAudioEntity::GetSpecificSpeechContext 0x4E4470 | `cmp si,6 / jge` | ped type SPC (5) asserted on the std::array; the exe reads the 6th int16 of the 16-byte entry (RepeatTime), callers then reject type > 4 (CanPedHoldConversation, 9 of 60 cases) |

## Item 2: fixtures with fewer agreed faults
| row | agreed faults of 120 before | after | cause |
|---|---:|---:|---|
| IsHidden 0x609620 | 88 | 0 | port called CTimeCycle getters from a unit not linked into the test (EIP 0x400000, counted as agreed fault); TimeCycle.cpp added + finite lighting floats |
| ControlButtonSprint 0x60A610 | 114 | 14 | m_nPedType was random 0..31; GetPad(0/1) only for PLAYER1/2 (both exe and port pass a null pad otherwise); player ped type now 0/1 in 7 of 8 cases, both pads random |
| ClearWeaponTarget 0x609C80 | 8 | 0 | WeaponEffects.cpp not linked (became visible once player types were common) |
| FlagToDestroyWhenNextProcessed 0x5E7B70 | 2 | 2 | rest is an unresolved symbol in the vehicle path, left |
| SetWantedLevel* / CheatWantedLevel | 66 / 52 / 90 | 0 | CStats::CheckForStatsMessage (via UpdateWantedLevel) needs CHud / CText: switched off with byte 0x8CDE56 |

## Item 4
- SpawnFlyingComponent 0x5F0190 is `ret 8` in the exe and a NOP in the port: no int range hazard.
- The `_exit` at the end of the W3_PED_ONLY target is removed; 5 consecutive full runs exit by themselves (rc 0 each).
