# MAC-P1 report: split size/offset asserts, x86 in-memory layout vs file layout (slice P-1)

Branch portable/main (pushed to `fork`). Commits: f5fa55f1 + 137e1a30 (Base.h), 0b389c4b (PORTABLE_SYNC announcement), 18 per-directory commits (last 8627ff41).
FILE-struct evidence list (131 rows, with file:line): `.notes/reports/MAC-P1-file-structs.tsv` (built by one read-only sonnet agent; kinds direct/nested/doubt).

## Macro design (source/Base.h)
| macro | meaning | MSVC/x86 | NOTSA_PORTABLE |
|---|---|---|---|
| `VALIDATE_SIZE(T,n)` / `VALIDATE_OFFSET(T,m,o)` | x86 in-memory layout of the original exe | unchanged static_assert (same text/message) | `static_assert(true)` |
| `VALIDATE_LAYOUT(...)` | other constant expression over in-memory layout (`offsetof(A,a)+offsetof(B,b)==N`, `sizeof(std::array<..>)`, `sizeof(x) <= N`, asserts with a message) | `static_assert(__VA_ARGS__)` | `static_assert(true)` |
| `VALIDATE_SIZE_FILE` / `VALIDATE_OFFSET_FILE` / `VALIDATE_LAYOUT_FILE` | on-disk / raw-byte layout | enforced | **enforced** (no switch disables them) |
The six basic-type checks in Base.h (bool/char/short/int/float/long long) are `_FILE` (true on arm64).
Codemod rules (script kept out of the tree): (1) every `VALIDATE_*(T,..)` whose T is a FILE struct -> `_FILE` (113 renames); (2) every raw `static_assert(sizeof/offsetof ..)` that FAILED in the portable census (located incl. `#line` directives; 390 statements in 140 files) -> `VALIDATE_SIZE/OFFSET[_FILE]`, `&&` chains split into one statement each (line count preserved), anything else -> `VALIDATE_LAYOUT[_FILE]`; passing raw asserts untouched; (3) a clause is `_FILE` iff a type it names is in the FILE set. 161 game_sa files touched, line counts of every file unchanged (verified per file).

## Census (native arm64, brew LLVM 23, PortableMac compdb, 1,105 TUs)
| | before (HEAD 1db39c22) | after (this series) |
|---|---|---|
| P: sizeof/offsetof static_assert, unique | **927** (575 files) | **33** (25 files) |
| TUs clean | 404 | 858 |
| unique diagnostics, all | 3,643 | 2,739 |
New diagnostics in the after-run other than P: 0 (set difference by file+message; the P ones differ only by macro-name text). Note: a first after-run was polluted by MAC-C6 editing source/portable/crt.h mid-run (stale PCH in 440 TUs); the numbers above are from the clean re-run.

### What the 33 are
* FILE structs failing on arm64 (the P-2 work list, `_FILE` asserts deliberately left failing): 27 diagnostics, table below.
* Not mine / not converted (7): fakerw/fakerw.h x5 (`RwEngineOpenParams`, `sizeof(long)==4`, rw::LLLink, rw::LinkList, rw::Object), fakerw/rwextra.h `RwFileFunctions` (44), source/standalone/rw/hanim.cpp `rw::HAnimKeyFrame` (FILE, 36 vs 40; it is the only FILE failure outside game_sa), game_sa/Animation/AnimManager.h `sizeof(std::array<CAnimBlendHierarchy,2500>)` (skipped: Windows host has CAnimManager in flight; becomes `VALIDATE_LAYOUT` once it lands). fakerw/standalone are outside my zone (librw-compat layer: belongs with stream R/W).

## FILE structs that FAIL on arm64 (P-2 work list)
| struct | evidence (file:line, see tsv for all) | offending members / note |
|---|---|---|
| CRunningScript (+ 6 offsetof asserts in Scripts/Commands/Ported/Group05_08, 13_15, 16_17c, 20a, 24a, 24b) | TheScripts.cpp:1254,1375 whole-object save/load | m_pNext/m_pPrev, m_BaseIP/m_IP/m_IPStack (pointers); size 416 vs 224 |
| tScriptParam | ScriptParam.h, script param stack in saved scripts | `void* pParam`, `char* szParam` in the union (8 bytes) |
| CPickup (+Group16_17a, Group25_26a) | Pickups.cpp save/load | `CObject* m_pObject` |
| tRadarTrace | Radar.cpp save/load | union with `CEntryExit*` |
| CWeapon (+Group16_17a) | member of CPedSaveStructure (saved in GenericGameStorage) | `FxSystem_c* m_FxSystem` |
| CPedSaveStructure | Ped.cpp save | embeds CWeapon[13] |
| CVehicleSaveStructure | Vehicle.cpp:1146,1155 | embeds CMatrix (RwMatrix*) |
| CMatrix | nested in replay / vehicle save | `RwMatrix* m_pRwMatrix` (+ static uint8* is irrelevant) |
| CSimpleVariablesSaveStructure | SimpleVariablesSaveStructure.h:72 | census says sizeof == 1 vs 312: members vanish in the portable parse (not a layout issue; needs a look, not a pointer) |
| CPathNode | PathFind.cpp nodes*.dat | `CPathNode *m_next, *m_prev` (40 vs 36 resp. 28) |
| BreakInfo_t | BreakablePlugin.cpp DFF stream | 7 pointers |
| BreakablePlugin | BreakablePlugin.cpp | union uint32 header / `BreakInfo_t*` (only 4 bytes streamed) |
| ExtraVertColour (doubt) | CustomBuildingDNPipeline.cpp stream callback | `RwRGBA *NightColors, *DayColors`; the writer does `RwStreamWrite(&NightColors, sizeof(NightColors))` (pointer-sized) while the reader expects uint32 |
| tReplayCameraBlock | Replay.cpp:1599 (VALIDATE_BLOCK, left raw) | `uint8 matrix[sizeof(CMatrix)]` |
| rw::HAnimKeyFrame | standalone/rw/hanim.cpp | pointer member (36 vs 40), outside my zone |
Other save-game note (not an assert): the size prefixes for CPed/CPlayerPed/CPlayerInfo/CObject/GangWars are written with `SaveDataToWorkBuffer(sizeof(X))` = size_t (8 bytes on arm64, 4 in the file), the loaders read int32/uint32.

## FILE structs that PASS on arm64 (enforced in every build now)
All other rows of the tsv (about 115 of 131), among them: tScriptParam siblings tSCM* chunk headers, CStuntJump, CGangInfo, CStoredCar, CSaveGarage, CSetPiece, tUser3dMarker, CGangWarsSaveStructure, CPlayerInfoSaveStructure, CPlayerPedDataSaveStructure, CObjectSaveStructure, CDamageManager, CCarGenerator, tCarGenPlateText, CZone, CZoneInfo, PriceModifier, CNodesSwitchedOnOrOff, COL (FileHeader, TSurface/TBox/TFaceGroup/TSphere/TFace/TVertex/Header, CColSphere/Box/Line/Triangle, CColAccel*), IplDef, tBinaryIplFile, CFileObjectInstance, CFileCarGenerator, CDirectory::DirectoryInfo, KeyFrame*, CCarPathLink, CNodeAddress, CPathIntersectionInfo, t2dEffect*StreamData, AEBankLookup/CAEBankSlot/AEPakLookup/StreamPack/tTrackLookup/tTrackInfo/tUserTracksInfo, ChunkHeader, tReplay* blocks (except Camera), CVehicleStateEachFrame, CControllerConfig, CVector/CRect/CBox/CSphere/CBoundingBox/CQuaternion. Doubt rows of RW types (RwMatrix, RpTriangle, RpHAnimNodeInfo, RtQuat, rw::Quat, RwChunkHeaderInfo, StdFrame): their asserts live in fakerw / standalone/rw (not converted), so unchanged.

## Skipped / left
* Windows in-flight (skip list): game_sa/platform*, CAnimManager*, CAnimBlendAssocGroup*, CTaskSimpleFall*: only one assert hit, game_sa/Animation/AnimManager.h `sizeof(std::array<CAnimBlendHierarchy, 2500>) == 60000` (not converted).
* game_sa/Audio/Managers/AESoundManager.cpp:16 (`NOTSA_GLOBAL(AESoundManager..); VALIDATE_SIZE(AESoundManager, 36028)`) was converted by my script while MAC-C6 had the file open; it landed inside C6's commit fd1ac44f (accepted by the orchestrator), not a separate commit of mine.
* Raw asserts that PASS on arm64 were left raw by design. Replay.cpp `VALIDATE_BLOCK` left raw (FILE).
* Not converted (outside zone): source/fakerw/**, source/standalone/**.

## No-op proof (MSVC x86, `cl.exe` under wine, flags of StandaloneRelease without PCH, StdInc.cpp force-included; A = HEAD with my 161 files restored from 1db39c22, B = HEAD; `obj_code_sig.py` with NOTSA_SIG_NORM_STRINGS=1: code bytes + relocation names)
| TU | directory | why | code identical |
|---|---|---|---|
| game_sa/Camera.cpp | root | NOTSA_GLOBAL assert + compound `&&` split | yes |
| game_sa/Entity/Vehicle/Automobile.cpp | Entity | 6 compound offsetof asserts | yes |
| game_sa/Scripts/Commands/Ported/Group24b.cpp | Scripts | CRunningScript `_FILE`, `&&` split | yes |
| game_sa/Collision/ColAccel.cpp | Collision | header `_FILE` renames | yes |
| game_sa/Plugins/BreakablePlugin/BreakablePlugin.cpp | Plugins | header `_FILE` renames | yes |
| game_sa/Text/Text.cpp | Text | NOTSA_GLOBAL assert + ChunkHeader `_FILE` | yes |
6 / 6 identical (Base.h, a PCH header, is part of every TU). Without NORM_STRINGS 5 of 6 differ only by the worktree path in strings/anon-namespace names (expected, as in earlier reports). Worktrees removed.

## Caveats
* The portable `VALIDATE_SIZE` ignores its arguments entirely (the type is not even named), so a typo in an in-memory assert is only caught by the MSVC build.
* PORTABLE_SYNC announcement said ETA 19:00-19:45 (wall clock mix-up), real finish about 21:00 local.
* Census front end: brew LLVM 23 (-fsyntax-only); numbers include other agents' concurrent edits.
