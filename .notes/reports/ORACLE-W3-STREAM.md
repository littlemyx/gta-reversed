# ORACLE-W3-STREAM report

Agent ORACLE-W3-STREAM (rules v3), 2026-10-20 18:43 - 19:38 WEST, worktree C:/gta-wt/oracle-w3-stream (branch oracle-w3-stream, base 12e011a0). New target `wave3_stream_oracle_test` (W2 harness: deterministic arena, the whole exe data image + the 16 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_stream.inc` (+ `_a.inc` IplStore/TxdStore, `_b.inc` Streaming). `W3S_DIV=n` divides the case counts.
**34 functions (IplStore 12, TxdStore 12, Streaming 10), PC24 0 mismatches (PC53 0), run time ~3 min 20 s** (pre-fix run 3 min 25 s).

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4581 | **4615** |
| IplStore.cpp uncovered A / B | 1 / 11 | **0 / 0** (5 C, 5 S, 8 D left) |
| TxdStore.cpp uncovered A / B | 1 / 11 | **0 / 0** (4 C, 2 S, 8 D left) |
| Streaming.cpp uncovered A / B | 1 / 9 | **0 / 0** |
| port bugs fixed (one commit each) | | **2** (+ 2 users of the first) |

## Fixtures
* IPL pool: the CPool header at `ms_pPool` 0x8E3FB0 over 4..16 slots of 0x34 bytes (flag bytes random used/free incl. Ref counts at 127, first-free -1..cap-1), IplDefs with random bounding boxes (game range + NaN/inf/denormal/huge), bool bytes 0/1; names 0..17 characters (the exe copies unbounded). `AddIplSlot` is compared bit for bit except the 2 padding bytes and the name tail after the NUL (the port `strcpy_s` fills the tail with 0xFE, the exe leaves it; nothing reads it). `IncludeEntity`: building (type 1) / dummy (type 5) raw entities, the exe CBuilding / CDummy pool headers (0xB74498 / 0xB744A0) get a storage base such that the entity index is 0, 1..40, 0..14000 or 32767; the def first/last counters at the int16 extremes, plausible or garbage. The three quad-tree callbacks (`SetIfIplIsRequired` 0x404660, `SetIfInteriorIplIsRequired` 0x4045F0, `SetIfIplIsRequiredReducedBB` 0x404690) get a bounding box and a position placed on the +-140/160 tolerance edges, `ms_currentIPLAreaCode` 0 / 1..19.
* TXD pool: CPool header at 0xC8800C over 4..16 TxdDefs (dictionary pointer to one of 8 blobs or null, refs including int16 wrap, parent slot -1 or a slot, hash drawn from a small set so `FindTxdSlot(hash)` meets duplicates and free slots), the parent plugin of the dictionaries at a random `ms_txdPluginOffset` (0x10..0x38). RenderWare is not run: RwTexDictionaryGetCurrent 0x7F3A90, RwTexDictionarySetCurrent 0x7F3A70, RwTexDictionaryFindNamedTexture 0x7F39F0 are `oracle::Patch`ed to host stubs and the fakerw entry points of the same names (defined in the test TU) have the same bodies (current dictionary and per-dictionary result live in the fixture block, calls recorded).
* Streaming: request list over the real info array (`ms_aInfoForModel` 0x8E4CC0, head/tail sentinels 26312/26313 linked through the int16 next/prev indices, 0..12 members mixed from 20 DFF models with fake model infos (`m_nTxdIndex`), TXD models 20000+slot (used slots) and models >= 25000), both loading channels (16 model ids each), `ms_pedsLoaded`, load states 0..4; the RW object-instance link list (used list with sentinels in the fixture block, free list sentinel at the exe 0x965508, `ms_renderEntityLink`); `CLinkedUpgradeList::FindOtherUpgrade` (0x4C74D0) stubbed on both sides with a table; the population world for the three car functions: zone info, perc-type table (0xC0BC78, 8640 bytes), `m_nNumCarsInGroup` / `m_CarGroups` for 36 groups with model ids from one small set (400..423), the loaded car groups (boats, appropriate, inappropriate, 11 gang groups, 0x8E4C24), the six traffic cheats, 24 fake vehicle model infos (`m_nTimesUsed`).

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the full run on the unfixed code, log C:/gta-scratch/w3s_prefix2.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| CStreaming::IsCarModelNeededInCurrentZone | 0x407DD0 (gang loop 0x407EC6) | the exe tests `GangStrength[g]` against car group 0x12 + g (BALLAS + gang); the port passed the bare gang index (groups 0..9: workers, business, clubbers ...) | 220/2500; users AddToLoadedVehiclesList 0x408000 138/2000, ReclassifyLoadedCars 0x40AFA0 362/1000 (they call it) | 29f409c5 |
| CStreaming::AreTexturesUsedByRequestedModels | 0x409A90 (channel loop 0x409B63..0x409BC8) | the exe decides "TXD model" in the loading-channel loop by comparing the ADDRESS of the model-id slot (esi, 0x8E4Axx) with 0x8F8378..0x8FD198 instead of the model id: never true, so only DFF models count in the channels. The port also matched TXD models by their parent slot | 175/3000 | f06bb2d3 |

Rows that matched without a fix: all 12 IplStore functions, all 12 TxdStore functions, ClearFlagForAll, HasSpecialCharLoaded, HasVehicleUpgradeLoaded, FindMIPedSlotForInterior, RenderEntity, RemoveEntity (their request-list / link bookkeeping equals the exe).

## Skips / limits
* Cases where only a free pool slot makes the exe dereference null (EnableDynamicStreaming, AddRef, GetNumRefs ... on a free slot; AddIplSlot / AddTxdSlot on a full pool) are not generated: the exe crashes there and the game never calls with them. IncludeEntity with an entity type other than building / dummy: the port has NOTSA_UNREACHABLE, the exe does nothing; not generated.
* FindMIPedSlotForInterior with a negative `randFactor`: the exe indexes `ms_pedsLoaded` before the array (C remainder), the port std::array asserts; the game passes non-negative values, rows use 0..0x7FFFFFF0. HasVehicleUpgradeLoaded ids < 26316 (the port GetInfo asserts above).
* The final run has no case that faults on both sides (the earlier 13 % of TxdStoreFindCB were fixture garbage in the current dictionary).

## Tests run (worktree, final tree)
wave3_stream_oracle_test (the target): 34 functions PC24 0 / PC53 0, exit 0, 3 min 21 s (log C:/gta-scratch/w3s_post3.log). It is the only test whose rows reach the two changed functions: the exe callers of both (StreamOneNewCar, ConvertBufferToObject, RequestModelStream, RemoveLeastUsedModel) are class D and no other test references the addresses. Streaming.cpp is also compiled into the W2 base of every wave2/wave3 target and into logic_oracle_test; those were not rebuilt (same TU, no signature change). The orchestrator runs the full run_all_tests.
