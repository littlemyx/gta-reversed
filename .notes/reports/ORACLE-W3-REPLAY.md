# ORACLE-W3-REPLAY report

Agent ORACLE-W3-REPLAY (rules v4), 2026-10-23 14:29 - ~15:40 WEST, worktree C:/gta-wt/oracle-w3-replay (branch oracle-w3-replay, base e3b8998d). New target `wave3_replay_oracle_test` (W2 harness, whole exe data image + fixture blocks compared per case), rows in `tests/standalone/wave3_oracle_replay.inc`, run with `W2_PORT_FAULT_IS_DIFF=1`; `W3R_DIV=n` divides the case counts. **24 of the 27 uncovered A/B functions run by default: PC24 0 / PC53 0, run time 2 min 46 s.** 3 more have rows that are off by default (see Skips).

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game (address-literal count) | 4724 | **4751** (+27, the 3 gated rows count) |
| Replay.cpp uncovered A / B | 7 / 20 | 0 / 0 |
| port bugs fixed (one commit each) | | **7 commits, 8 functions** |

## Fixtures
The 8 replay buffers (0x97FB88, 8 x 100000 bytes) hold valid packet streams: every packet type 1..19 with the exe size (verified by the FindSizeOfPacket row), small pool references so lookups hit, END terminator, garbage behind the END and in buffers whose status byte is 0; status bytes 0/1/2; Record at the buffer boundary (offset near 99984, slot 0..7); Mode; the CPool headers of the ped/vehicle pools (0xB74490/94) over host storage with the exe strides 0x7C4/0xA18, random slot flags and entity bytes; the conversion tables; a fake ped with a clump (RpAnimBlend plugin at ClumpOffset 0xB5F878, 0..6 blend associations, equal/negative/zero blend amounts), a matrix, a vehicle slot, the player in focus; clothes descriptions (fat/muscle stats incl. int16 sign cases) and 120-byte clothes packets.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the full run on the unfixed code)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| IsThisVehicleUsedInRecording / IsThisPedUsedInRecording (+ GetIndex) | 0x45DE40 / 0x45DDE0 | the exe skips buffers whose status is 0; the port scanned all 8. Root cause `tReplayBuffer::GetIndex()` = `(this - Buffers.data()) / sizeof(tReplayBuffer)` was always 0 so `IsAvailable()` tested BufferStatus[0] for every buffer (fix in Replay.h; users FindFirstFocusCoordinate, NumberFramesAvailableToPlay, StreamAllNecessaryCarsAndPeds) | 152/1500, 98/1500 | 259f58c1 |
| FindFirstFocusCoordinate | 0x45D6C0 | the exe clears the result vector first | 845/1500 | 3dca8a03 |
| NumberFramesAvailableToPlay | 0x45D670 | the port loop never advanced (`find_if` from the same iterator): infinite loop on any end-of-frame packet in an active buffer; exe counts one per packet | 1366/1500 (hang) | d16ccdf9 |
| RestoreClothesDesc | 0x45C7D0 | fat/muscle stats are int16 (movsx) in the exe, the port zero-extended the uint16 field | 884/1500 | cebe43cf |
| EmptyReplayBuffer | 0x45EC90 | `Record = CAddressInReplayBuffer(...)` copied 3 padding bytes of stack garbage into the image; exe writes the 3 fields | 191/300 | 25269898 |
| Init | 0x45E220 | `Playback = {}` zeroed the padding too; exe writes the fields | 150/150 | 85b79ef1 |
| StorePedUpdate | 0x45C940 | designated-initialiser temporary left 4 padding bytes of the 52-byte packet as 0xCC stack garbage in the replay buffer | every case (DIV=4 run) | 54935271 |

Consumer audit (v4 rule 2): the only readers of `tReplayBuffer::IsAvailable/GetIndex/GetAllActiveBuffers` are those three functions in Replay.cpp (all now the exe's convention); Record/Playback keep their layout (field-wise writes only); no other reader of the changed packets/stats.
Rows that matched without a fix: FindSizeOfPacket, StoreClothesDesc, StorePedAnimation, Enable/DisableReplays, ShouldStandardCameraBeProcessed, FindPoolIndexFor{Ped,Vehicle}, CanWeFindPoolIndexFor{Ped,Vehicle}, MarkEverythingAsNew, GoToNextBlock, RecordVehicleDeleted/RecordPedDeleted, Initialise{Vehicle,Ped,}PoolConversionTable(s).

## Skips / fixture limits
* StorePlayerInfoVariables 0x45F020 / RestorePlayerInfoVariables 0x45E1F0 (rows exist, run with `W3R_PLAYERINFO=1`): both call CPlayerInfo::operator= 0x45DEF0 -> CPlayerPedData::operator= 0x45C4B0, a deep copy (CWanted, clothes, arresting cop); random bytes are not a CPlayerInfo the exe can copy, the first attempt differed in many fields. Needs a constructed CPlayerInfo/CWanted fixture and its own row for operator= (follow-up brief).
* StorePedUpdate (row runs with `W3R_PEDUPDATE=1`): after the padding fix 107/1200 cases still differ, all in the CCompressedMatrixNotAligned words (CompressFromFullMatrix 0x59BAD0 has no row of its own; port and exe disagree on some compressed rotation/position words). Follow-up: row for 0x59BAD0 / 0x59B9F0.
* Packet padding: RecordVehicleDeleted/RecordPedDeleted - the port writes a zero pad byte, the exe leaves it; the row zeroes the destination (never read). StorePedAnimation - state[0].m_nGroupId2 is left alone by the exe, zeroed by the port; the game passes a zeroed state.
* Initialise{Vehicle,Ped}PoolConversionTable: the exe loop runs over the fixed 110/140 slots and ends on `esi == 110` exactly, so a recorded reference >= ~100 makes it spin (latent exe bug, the game never records such); rows use full-size pools and references < 100. Float-to-int8/int16 conversions of NaN/huge values (heading, matrix) are x87-vs-SSE UB and not generated.
* Not in scope (class C/D): Update, RetrievePedAnimation, Display, ProcessReplayCamera, ... 

## Soak (v4 rule 3)
Release build of this tree (build/StandaloneRelease, ninja -j4), `soak.sh build/StandaloneRelease 3` with NOTSA_STANDALONE_NOVSYNC=1: RESULT OK, errors 0, 48300 frames; 1 gate timeout in the route (a step never saw its condition), no crash/assert.

## REPLAY-2 (2026-10-23 15:15 - 15:42 WEST; rules v4, worktree C:/gta-wt/replay-2, branch replay-2)
New rows in `wave3_replay_oracle_test`: **CCompressedMatrixNotAligned::CompressFromFullMatrix 0x59BAD0** (3000 cases: real rotations, k/127 truncation boundaries +-2 ulp, values beyond +-1, NaN / inf / 1e30, positions) and **CPlayerInfo::operator= 0x45DEF0** (1500 cases, two CPlayerInfo-sized blocks of random bytes, half with a terminated skin name). The two gated rows (`W3R_PEDUPDATE`, `W3R_PLAYERINFO`) are now on by default. Full run: **29 rows, PC24 0 / PC53 0, 3 min 35 s**.

| function | asm | finding | pre-fix (PC24) | commit |
|---|---|---|---|---|
| CompressFromFullMatrix | 0x59BAD0 | **matches** (x87 `fmul [127.0f]` + `_ftol2` = the port's float multiply + truncation, including NaN / inf / wrap); the 107 StorePedUpdate differences were never in the compressed bytes | 0/3000 | none needed |
| StorePedUpdate | 0x45C940 | the exe stores into the replay buffer in place and leaves padding (2 bytes before the matrix, the 2 pad bytes of the 20-byte compressed matrix, 2 trailing) and the unused bits of the flags byte alone; the port memcpy'd a zeroed local packet over all 52 bytes (the earlier "differs in the compressed words" was the matrix padding bytes 0x2A/0x2B) | 107/1200 -> 55 | 8b9d5d8c |
| StorePedAnimation | 0x45B170 | the exe leaves state[0].m_nGroupId2 (sixth byte) alone, the port assigned a whole AnimationState | 55/1200 -> 0 | 90e02608 |
| CPlayerInfo::operator= -> CPlayerPedData::operator= | 0x45DEF0 / 0x45C4B0 | (1) PlayerData flags word +0x34: bit 9 from the destination, bits 10..12 from the source, **all other bits from the caller's stale `esi`** (= the destination address; the exe's compiled bitfield assignments never load it); (2) 4 padding bytes of PlayerData (+0x22/+0x23/+0x87/+0x8F) not copied; (3) skin name copied as 32 bytes, the port's `strcpy_s` stopped at the terminator / asserted | operator= 1500/1500; Store/RestorePlayerInfoVariables 150/150 each | f11bc7d9 |

* The copy is **shallow**: CWanted / clothes / arresting-cop pointers are copied as pointers (0x45C4B0 is a mov list), so no CWanted deep-copy fixture is needed; the earlier "random bytes are not a CPlayerInfo" mismatch was the flags word and the unterminated skin name (assert). The Store/Restore fixture still puts live-looking pointers and a terminated skin name into half of the cases.
* The stale-esi bits are mapped to the exe addresses of the game's instances (`Players[i]` 0xB7CD98 + i*0x190, `CReplay::PlayerInfo` 0xA430B0) so builds whose globals live elsewhere get the exe's flag values (bit 4 `m_bCanBeDamaged`, bit 3 `m_bFreeAiming`, bit 7, bit 8 ... of the address), instead of the random bits of a heap address.
* Consumer audit (v4 rule 2): CompressFromFullMatrix is unchanged; its callers are StorePedUpdate (Replay.cpp, now calls it on the buffer), CReplay vehicle blocks (ReplayBlockType.cpp:12 `Compress`), CObjectSaveStructure::Construct (ObjectSaveStructure.cpp:16) = the exe's three callers (0x45B747 RetrievePedAnimation-area, 0x45C979, 0x5D216C). StorePedUpdate's only caller is RecordThisFrame (Replay.cpp:993), same offset advance (52). The only reader of ped-update packets (PlayBack / RetrievePedAnimation) reads state[0].m_nGroupId1, state[1/2].m_nGroupId1/2, never state[0].m_nGroupId2 or padding. CPlayerInfo::operator= is used only by CReplay::Store/RestorePlayerInfoVariables (Replay.cpp:700/711); the readers of PlayerData flags after a replay (Players[0]) now see the exe's bits.
* Soak (v4 rule 3): Release build of this tree (build/StandaloneRelease, ninja -j4), `soak.sh build/StandaloneRelease 3` with NOTSA_STANDALONE_NOVSYNC=1: RESULT OK, errors 0, 48200 frames; 1 gate timeout in the route (`until invehicle`, same as the previous soak), no crash / assert. The replay code is not exercised by the soak (no recording playback); it guards the build and the shared PlayerInfo / World TU.
