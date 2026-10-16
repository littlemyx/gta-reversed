# FIXADDR: global address / type bugs (aliases.json `retype` / `readdress`)

Branch reverse/interior_c. Reference: `gta_sa_compact.exe`. Every address below was swept with capstone over the whole .text
(1.5 M instructions, every instruction whose operand is in the range). Lists are complete for the ranges swept.

## Commits (one per bug, + the oracle case)
| # | commit | bug |
|---|---|---|
| 1 | 59803c65 | `s_pVehicleAudioSettingsForRadio` 0xB6B98C -> 0xB6B99C (+ 3 readers) |
| 2 | 6e9e56a6 | `CGameLogic::GameState` / `ActivePlayers` 1 byte each (+ `AddSayEvent` enum) |
| 3 | 37577efa | `col1` is one `CColModel`, `col1Data` = `CCollisionData` at 0xC17854 |
| 4 | ae300f56 | `FurnitureManager_c::g_currSubGroupId` / `g_currFurnitureId` uint16 |
| - | ed2f8769 | `logic_oracle_test`: `CGameLogic::InitAtStartOfGame` (0x441210) vs the exe (fails 300/300 on the old types, 0/300 now) |

## 1. s_pVehicleAudioSettingsForRadio (0xB6B98C -> 0xB6B99C)
Exe accesses (all of them):
* 0xB6B98C = the DRIVER (CPed*): writes 0x4F5B29 (TurnOnRadioForVehicle: `mov edx,[eax+0x460]` = `m_Entity->m_pDriver`; `mov [0xB6B98C],edx`),
  0x4F5B88 (TurnOff, 0), 0x4F5BD8 (PlayerAboutToExitVehicleAsDriver, 0), 0x4FBAEC (Terminate), 0x4FD0EF (JustWreckedVehicle), 0x5B9A40 (StaticInitialise);
  reads 0x4E5DD2 (PlayLoadedSound), 0x4E61D5 and 0x4E66F0 (the two instantiations of `I_AddSayEvent`), 0x4E769B (police scanner `Service`).
* 0xB6B99C = `&m_AuSettings` (`lea edx,[ecx+0x80]`): writes 0x4F5B35, 0x4F5B8E, 0x4F5BE2, 0x4FBAF2, 0x4FD0F5, 0x5B9A3A; the only read is 0x4F4ED0
  (`StaticGetPlayerVehicleAudioSettingsForRadio`).
Repo: both names were on 0xB6B98C, so `s_pVehicleAudioSettingsForRadio = &m_AuSettings` overwrote the driver.
Fix: the settings pointer lives at 0xB6B99C. The three non-radio readers (ped speech x2 sites, `PlayLoadedSound`, police scanner) read the DRIVER in the exe
(they only test it for null) and now use `s_pPlayerAttachedForRadio`. Constants checked in .rdata: 0x8C80F0 = 3.0f (non-null), 0x8C80F4 = 2.0f, 0x8C815C = -8.0f.
aliases.json: the `readdress` entry and the `0xB6B98C` conflict note are now fixed in source (entry obsolete, `new_globals[0xB6B99C]` is the real global now).

## 2. CGameLogic::GameState / ActivePlayers
Exe (byte accesses): 0x96A8B0 GameState (22 accesses: 0x43DB6C, 0x43DBD2, 0x43DC6F, 0x441223, 0x44126C, 0x442AAC, 0x442B61, 0x442B73, 0x442D11, 0x442D56, 0x442D97,
0x442E60, 0x44317A, 0x4434B7, 0x468979, 0x4E6139, 0x4E65AD, 0x56E842, 0x609590, 0x6A0820, 0x6A153E, 0x6AE9D3); 0x96A8B1 ActivePlayers: only `mov byte [0x96A8B1],1`
at 0x441212 (InitAtStartOfGame); 0x96A8B2 bPlayersCannotTargetEachOther (w 0x498881 `sete al`, r 0x60E0A3, 0x60E6D8); 0x96A8B3 bPlayersCanBeInSeparateCars
(w 0x498A3F, r 0x57088B); 0x96A8B4 nPrintFocusHelpTimer is a dword (0x441232, 0x442C03 ...). The neighbour flags already had the right names/types in the repo.
Repo: `GameState` was an int-sized enum, `ActivePlayers` int32. Effects: every GameState store zeroed ActivePlayers and both flags; `ActivePlayers = true` wrote
0x96A8B1..B4 (bytes B2, B3 and the low byte of nPrintFocusHelpTimer); every GameState read included the three bytes behind it.
Fix: `enum eGameLogicState : uint8`, `ActivePlayers` uint8. Also `AddSayEvent` compared GameState with `eGameState::GAME_STATE_PLAYING_LOGO` (= 2 numerically, the exe's
`cmp byte [0x96A8B0],2` = GAMELOGIC_STATE_BUSTED): now the right enum.
Oracle case (`logic_oracle_misc.inc`, 300 cases with random bytes in 0x96A880..0x96A8C0): old types 300/300 mismatches (`0x96A8B0 got 00000000 exe xxxx0100`), fixed 0/300.
Outside my zone (BUG2 agent, Automobile.cpp 3475, 3718, 5875): `CGameLogic::GameState != GAME_STATE_INITIAL` / `GameState` truthiness compare against the wrong enum;
numerically equal to GAMELOGIC_STATE_PLAYING (0), so correct now that the variable is a byte, only the name is wrong.

## 3. col1
Exe: FightSetUpCol 0x61D5F0..0x61D6DC: `[0xC17850] = 0xC17854` (CColModel::m_pColData is at +0x2C), `word [0xC17854] = 1` (CCollisionData::m_nNumSpheres), `[0xC1785C] = 0xC17884`
(m_pSpheres at +8), bounding box / sphere writes 0xC17824..0xC17848; ShutDownForRestart zeroes `[0xC1785C]` and `[0xC17850]` (0x53CAA8, 0x53CAAE); 0xC17824 is passed to
ProcessColModels at 0x624534 / 0x6640F6; operator-delete-thunks 0x851FF0 / 0x852010 are the ctors of the model (0x40FB60) and of the data (0xC17854, 0x40F030).
Repo: `std::array<CColModel,2>` at 0xC17824; `Game.cpp:271` wrote `col1[1].m_boundBox.m_vecMin.z = 0.0f` (= the zero store into m_pSpheres, same bytes).
Fix: `Game.h`: `col1` (CColModel) and `col1Data` (CCollisionData, 0xC17854); `ShutDownForRestart` uses `col1Data.m_pSpheres`; TaskSimpleFight / TaskGangHassleVehicle use `col1`;
TaskSimpleFight's private duplicate declarations now alias them. No behaviour change (identical bytes).

## 4. FurnitureManager_c ids
Exe (all `word ptr`): 0xBAB37C: 0x5910E0, 0x5910FB, 0x5C04A8, 0x5C055D; 0xBAB378: 0x5C00C6, 0x5C00E6, 0x5C04AE, 0x5C0564 (0x58FF2F / 0x590CCE are `cmp esi, 0xBAB378` pointer
bounds of the array in front, not accesses). Repo: uint32 (class statics + two dead, shadowed copies in FurnitureManager_c.cpp, deleted). The 4-byte stores of `Init` only
touched the padding in front of the manager (0xBAB37A, 0xBAB37E); nothing else in the exe touches those bytes.

## 5. Other `retype` / `readdress` entries
| entry | live bug today? | disposition |
|---|---|---|
| readdress 0xB6B98C | yes | fixed (#1) |
| 0x96A8B0, 0x96A8B1 | yes | fixed (#2) |
| 0xC17824 col1 | typing only | fixed (#3) |
| 0xBAB378, 0xBAB37C | harmless (padding) | fixed (#4) |
| 0x8D35D4 VehicleGunOffset 14 -> 13 | no: only `[0]` is read | not changed (Vehicle.cpp is dirty with others' WIP; the extent matters only for the detached image) |
| 0xC4D958 TempVertexBuffer arena | no: C arrays, no bounds checks, the overlays are separate fixed-address statics | not changed (detach concern) |
| 0x8CC388 gCameraMode uint16 vs exe dword | latent: every reader/writer (CCam::Init, Process, Process_FollowCar_SA) is ported and 16-bit consistent; bites only if the Process / FollowCar hooks are toggled individually (stale 0xFFFF upper half vs exe `cmp dword`) | not changed: Camera.h `extern` is included everywhere; detach concern |
| 0x8D5094 m_grainStrength char[2] vs dword | no: nothing in the exe writes it (the single reader 0x705089 is ported), always 0 | not changed |
| 0x8D390C ms_WakePointAlphaMults 4 -> 5 | no: the global is never read (WaterLevel.cpp uses a local constexpr[5]) | not changed |
| 0x8CC548 gFollowPedCamTuning 2 -> 3 | no: Process_Cam_TwoPlayer (the 3rd row's reader) is ported with literals | not changed |
| 0x8D3808 WaterLayerAlpha 2 -> 3 | no: the 3rd dword is read as `StaticRef<int32>(0x8D3810)` (WaterLevel.cpp:942) | not changed |
| 8x tStatLine tables | no: pointer heads only | not changed |
| `views` gbExitCam (29 bools at 0xB6EC5C) | no: only indices 20..28 are used (0xB6EC70..78) | not changed |
Obsolete entries now: `readdress[0xB6B98C]`, `retype[0x96A8B0/B1]`, `retype[0xC17824]`, `retype[0xBAB378/37C]` (A2's codemod must not re-apply them; D-A2: the declarations are now byte / uint16 / uint16 /
`CColModel`+`col1Data` / `0xB6B99C` in source). `.notes/aliases.json` and gen_globals.py untouched.

## Builds and tests
* StandaloneRelease `gta_reversed.exe` (1119 steps): OK. Debug (DLL) `gta_reversed.asi` (1136 steps): OK. PCH deleted before each (Game.h and GameLogic.h are PCH headers).
  First attempt failed: `CCollisionData` undeclared in Game.h -> forward declaration added.
* `run_all_tests.sh build/StandaloneRW` (SCRATCH = scratchpad):
  * Baseline (pre-change objects, HEAD 1dd5e846 + dirty tree of the others, recorded before the first edit): game_oracle_test 44/0 ok; physics_oracle_test NORESULT (deterministic exit 3: fault at EIP 0x400000 in
    `CCarCtrl::SteerAICarWithPhysicsFollowPath 0x434900`, CarCtrl.cpp is somebody's dirty WIP); fixed_oracle_test FAIL (COccluder::ProcessLineSegment/ProcessOneOccluder, since fixed by 18ca5ede);
    pool_oracle_test, rw_renderstate_test, script_oracle_g16_test, x87_oracle_test NORESULT; rw_skin_vs_test NORESULT (hang > 900 s); 9 rw_* tests NOBUILD (standalone/rw/camera.cpp does not
    compile: `FindPlayerPed` / `CWorld` not declared); logic/gameplay/world_oracle_test not built (ninja stopped on the camera.cpp failures); everything else ok.
  * After (my 4 commits): identical except: rw_renderstate_test now ok (was NORESULT, flaky exit 1/5), gameplay_oracle_test ok (311 functions, PC24 0), world_oracle_test ok (114), game_oracle_test ok,
    fixed_oracle_test FAIL on a NEW case another agent added meanwhile (`CIdleCam::VectorToAnglesRotXRotZ`, aa1c8fbb; COccluder now clean), logic_oracle_test NORESULT in the runner because the whole suite
    needs > 900 s (also with the old types): run by hand, see below. No PC24 failure attributable to these changes.
  * logic_oracle_test by hand (900 s is too short): see the last paragraph.
  * logic_oracle_test by hand (`wine`, RW_EXE_ORACLE, ~20 min each): exe built with the OLD GameLogic.h (header swapped under the build lock, restored right after) vs the fixed one:
    224 functions, PC24 533 / 533 -> 233 / 233 (PC53 9840 -> 9540). Row-by-row diff: the ONLY differing row is `CGameLogic::InitAtStartOfGame` (300 -> 0). The remaining 233 PC24 mismatches are
    `CCarCtrl::ChooseCarModelToLoad 0x421900` (CarCtrl.cpp is dirty with other agents' work), identical before and after.
  * Note for the runner: logic_oracle_test needs more than the 900 s oracle timeout (set `TIMEOUT_logic_oracle_test=2400`); ninja also treated a stale logic_oracle_test as up to date after my header swap
    (mtime within the same second), I forced `ninja -t clean logic_oracle_test` before the final build.

## Do the bugs explain a runtime symptom?
* Radio / vehicle audio (#1): practically no. All readers and writers in the repo were consistent with each other (both names aliased one cell), and the four exe-side readers of the
  driver pointer only test it for non-null, which holds whenever the player drives. The observable difference is the case "radio turned on while `m_pDriver` is null" (the repo made the
  flag non-null anyway) and any exe-side reader with a hook disabled. Does not explain missing radio.
* Multiplayer flags (#2): yes, but only 2-player / coop: every GameState store (wasted, busted, mission failed/passed, load game, InitAtStartOfGame) cleared
  `bPlayersCannotTargetEachOther` / `bPlayersCanBeInSeparateCars`, and with a flag set every GameState read (CanPlayerStartMission, MissionCleanup, hydraulics / tank control in Automobile.cpp)
  saw a value != 0. Single player: no symptom (the neighbour bytes were 0 so the int read equalled the byte).
* Collision (#3): no (same bytes written). Interiors furniture (#4): no (padding bytes only).
