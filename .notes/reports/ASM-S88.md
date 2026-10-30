# ASM-S88 (CAERadioTrackManager + CAESoundManager, phase 3 tier A) - 2026-10-30
Result: 43 compared in full: 31 MATCH, 12 FIXED (7 fix commits), 0 UNSURE, 0 NOT REACHED. Table in S88.result.tsv.

## Fixes
| function | exe | what the port did wrong |
|---|---|---|
| CAESoundManager::Service | 0x4F0000-0x4F0B0E | (1) the "finished sound" loop tested HwAware instead of FrameDelay==0; (2) the last-used-slot scan stopped one index early (`>` instead of the exe's do-while down to AND including the prioritised count), after which the insert scan read `m_VirtuallyPlayingSoundList[-1]` (garbage) when the slot was empty; (3) the insert point break test was inverted (`bl <= cl`: new sound NOT more physical than B); (4) frame-delay decrement is a signed byte test |
| CAESoundManager::GetVirtualChannelForPhysicalChannel | 0x4EF630 | 16-bit subtract + sign test (unused function) |
| CAERadioTrackManager::AddIdent/Advert/DJBanterIndexToHistory + Service callers | 0x4E9720/60/B0, 0x4EBCE7.. | the track id is a dword (idents 1100.., banter up to 1900, adverts up to 134) but the port took/cast int8: the history held truncated values and never matched, so idents/adverts/DJ banter could repeat immediately |
| CAERadioTrackManager::Initialise | 0x5B9390 | `*this = {}`: m_HwClientHandle became 0 instead of the channel passed in, the four static history tables stayed at the image's 0 instead of -1 (index 0 / id 0 counted as recently played), user track play mode not read; now stores handle + STOPPED mode, runs the Reset body, field_368=0, play mode |
| CAERadioTrackManager::Reset | 0x4E7F80 | missed m_nTracksInARow=0 and m_nSavedTimeMs=0; BassGain is not written by the exe (kept) |
| CAERadioTrackManager::StartRadio(id,..) | 0x4EB3C0 | RADIO_OFF cleared the whole requested struct (`Reset()`: also the BassSetting/BassGain just stored, PlayTime, flags); the exe clears only the 3 queue arrays |
| CAERadioTrackManager::DisplayRadioStationName | 0x4E9E50 | x position is the INTEGER maximumWidth / 2 (cdq/sar), the port used width / 2.0f |
| CAERadioTrackManager::GetRadioStationName | 0x4E9E10 | id <= 0 returns null; the port hit NOTSA_UNREACHABLE |
| CAERadioTrackManager::UpdateRadioVolumes | 0x4EA010 | mission-audio ducking used std::log10 on a float quotient; now the inline x87 chain (fdivr PC24, fyl2x unrounded, x20, -9) via x87::log10 / X87R |

## Checked and equal
Choose* (ident/advert/music/DJ banter/FromList/talk show: all 31 show conditions and stat ids decoded, history sizes, rand order), QueueUp (6 cases), ChooseTracksForStation, TrackRadioStation (jump table 0..7, time windows, rand order), CheckForTrackConcatenation, CheckForStationRetune (+ DuringPause), CheckForMissionStatsChanges (all 22 stat/slot/index triples), StopRadio (state save via the CurrTrackType jump table), StartRadio(settings), CheckForPause, Bass/Retune setters, Save/Load order and element sizes, sound manager Initialise/Terminate/Reset/RequestNewSound and the two `...AndPhysical` helpers. All 17 data tables of AERadioTrackManager.cpp (DJ banter lists x9, ident/advert ranges, track sound ids, intro/outro ranges, talk-show lengths, blocked adverts, special banter 2) were dumped from the exe by script and are equal (the two "mismatches" were the omitted zero tails).

## Not changed on purpose
Saved-clock bytes (m_nStatsLastHitGameClockDays/Hours, m_nStatsCitiesPassed) are read unsigned where the exe uses movsx: only differs for values >= 128, which the game never stores. The `hours >= 8 && hours < 6` sunny-weather banter arm is an original bug (never true) and is kept.

## Tests
wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe, rebuilt after the last edit): 1142 functions, 0 mismatches (PC24 0 / 0, PC53 0 / 0). PortableWin64: AERadioTrackManager.cpp and AESoundManager.cpp compile (x64 cl, flags from the main checkout's compile_commands without the PCH, objects in scratch); the full PortableWin64 tree is not configured in the worktree (needs the conan deps). Pre-commit grep clean. Combat regress: see the result line in the agent report.
