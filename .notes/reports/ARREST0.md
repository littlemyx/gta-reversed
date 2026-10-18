# ARREST0 - CGameLogic::Update (0x442AD0): arrest at 0 stars hit NOTSA_UNREACHABLE, plus an asm review of the whole function

Rules v2. Worktree C:/gta-wt/arrest0 (branch arrest0). Evidence: PROBE-SUITE.md (`GameLogic.cpp:733`).

## Root cause
Exe 0x44319B: `eax = wantedLevel - 2 ; cmp eax, 4 ; ja 0x4431CA` - the jump table (0x4438D4, read from the exe: 0x4431A7/AE/B5/BC/C3 = 200/400/600/900/1500) covers levels 2..6 only; **everything else, i.e. levels 0 and 1, lands on `mov ecx, 100` (0x4431CA)**. The port's `default:` was `NOTSA_UNREACHABLE()`; level 0 (arrested with no stars, e.g. a script/`arrest:0`) logged and continued in the run build, but is a hard stop otherwise. Fix: `default: return 100`.

## Line-by-line review against the asm (my own reading of the busted/wasted copies + one sonnet helper pass over the whole function; every item below re-checked in the asm by me)
The exe inlines three copies of the resurrection sequence (wasted 0x442E59, busted 0x44316B, mission passed/failed 0x4434AA); the port shares one lambda, which hid copy-specific differences. Deviations found and fixed (all in GameLogic.cpp):

| # | asm | port before | fix |
|---|---|---|---|
| 1 | fee: `cmp [money],0 ; jle skip ; money = max(money-fee,0)` (busted 0x4431EA, wasted 0x442E31) | `money = max(money-fee,0)` always: money <= 0 became 0 (negative money lost) | only when money > 0 |
| 2 | fee default (levels 0/1/>6) = 100 (0x4431CA) | NOTSA_UNREACHABLE | `return 100` |
| 3 | `if (CRestart::bFadeInAfterNextDeath/Arrest) fade out else the GLOBAL is set to 1` (0x4430B7 / 0x44344E) | flag passed by value, global never set | pointer to the global; mission copy (always fades) passes nullptr |
| 4 | `PassTime(720)` + `m_nNumHoursDidntEat = 0`: wasted both (0x442FCA-ED), busted PassTime only (0x443375), mission copy neither (0x44361B -> 0x443624) | both in all three | per-copy flags |
| 5 | mission copy does not call `CHud::SetHelpMessage(nullptr,..)` (0x443534 -> 0x443542) | called | per-copy flag |
| 6 | `CPed::Say(ARRESTED, 2300, 1.0, 1, 1, 1)` six args (0x442D34), isFrontEnd=1 | isFrontEnd defaulted to 0 | pass 1 |
| 7 | restart-point search uses the ped's own position (0x442F0D `[ped+0x14] ? +0x30 : ped+4`) | `CPlayerInfo::GetPos` (vehicle position when driving) | `player1Ped->GetPosition()` (ClearExcitingStuffFromArea keeps GetPos_Hook, as the exe) |
| 8 | coop distance and the final StopPlayerMoving block use `Players[0/1].m_pPed` positions (0x442B18, 0x443725) | `FindPlayerCoors` (vehicle) | `FindPlayerPed(PLAYERn)->GetPosition()` |
| 9 | coop help messages (0x442BEA): timer/counter updated only when `now > timer` shows a message; WRN1_2P re-arms +15000, WRN2_2P +60000 | timer = now+60000 and counter++ every frame, +60000 always | as asm |

Checked OK (no change): head of the function, UpdateSkip only without coop, player loop (states 0x37/0x36/dying-stuck, 0x3F, BUSTED big message), state jump table 0x4438C4, fee table, free health care / get-out-of-jail flags, 3000/4000 ms thresholds and fades, bust audio status / `%28+1`, vehicle exit, ClearMessages, restart point selection, coop restart angle `* 57.29578` + RestorePedsWeapons, Remove2ndPlayer, SortOutStreaming args, camera/pad/reference resets, countdowns, DisablePlayerControls, `dist > 100`/`> 85` message logic. Equivalent: busted sets `m_nPlayerState`/`GameState` = PLAYING before the fee instead of after (nothing in between reads them).

## Test
New gameplay_regress scenario `arrest0` (`routes/arrest0.txt`, `gr_check.py chk_arrest0`, doc row in docs/STANDALONE.md; injector `cmd:money:N` and `money=` in the snapshots, GameplayRegress.cpp): arrest at 0 stars with 1000 money -> 900; arrest with -50 -> -50; kill with 1000 -> 900; kill with -50 -> -50; generic `no_crash` greps UNREACHABLE. CGameLogic::Update is not exe-oracle-able (whole-game state), hence the regress scenario.
* old code (HEAD d48e0973 + injector): FAIL no_crash (UNREACHABLE GameLogic.cpp:733), FAIL busted_negative_money_kept (-50 -> 0), FAIL wasted_negative_money_kept (-50 -> 0); PASS=4.
* fixes 1-2 only: PASS=7/7.
* final code: see the tail of this file.
* final code (all 9 fixes, Release build): `gameplay_regress.sh arrest0 missions` -> arrest0 7/7 PASS, missions 11/11 PASS (NOTSA_STANDALONE_NOVSYNC=1, silent, windowed). Items 3-9 (flag global, per-copy PassTime/help, Say flag, positions, coop messages) are asm-verified but not exercised by a scenario (coop is not reachable in the run build).
