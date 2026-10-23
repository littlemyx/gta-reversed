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
