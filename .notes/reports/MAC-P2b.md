# MAC-P2b report: P-2 scalar fixes (2026-10-23, branch portable/main, pushed to fork)

## Commits
* 1f1d7a53 P-2b: scalar fixes, systemtime.h, tests, runner
* d16f4fdf IplStore `<uint32>` (code-identical on x86) + MemoryHeap.cpp line numbers kept (found by the wine A/B)

## Changes
| item | change |
|---|---|
| 5 size_t prefixes | GangWars.cpp:75, PlayerInfo.cpp:1465, PlayerPed.cpp:162, Ped.cpp:545, Object.cpp:610: `SaveDataToWorkBuffer<uint32>(sizeof(..))` |
| 4 GetIndex writes | TheScripts.cpp:1299/1323/1327/1331: `<uint32>(Pool->GetIndex(x) + 1)` |
| IplStore.cpp:801 (new from P-2a) | `<uint32>(num)` (size_t count; Load reads int32, same 4 bytes). `<int32>` changed the callee template symbol on x86, so `<uint32>` = the type deduced there |
| tScriptParam | removed unused `void* pParam / char* szParam` (0 users): 4 bytes everywhere, `VALIDATE_SIZE_FILE(.,4)` now holds on arm64 |
| SYSTEMTIME / CSimpleVariablesSaveStructure | new `source/portable/systemtime.h`: `notsa::SystemTime` (16 B Win32 layout) + `notsa::GetLocalTime` (found by ADL, SimpleVariablesSaveStructure.cpp unchanged). Header: `using SaveSystemTime = notsa::SystemTime` under NOTSA_PORTABLE, `= SYSTEMTIME` otherwise. **Not named SYSTEMTIME on purpose**: MAC-W0 added a global `struct SYSTEMTIME` to winapi_compat.h meanwhile (my first version collided: "redefinition"). arm64 sizeof == 0x138 |
| ExtraVertColour | `VALIDATE_SIZE_FILE` -> `VALIDATE_SIZE` (in-memory, 12 x86 / 24 LP64); portable writer emits `uint32 magic = NightColors ? 1 : 0` instead of the raw pointer (4 vs 8 bytes). MSVC branch textually unchanged |
| HeapBlockDesc | `#error` in MemoryMgr.h if `MEMORY_MGR_USE_MEMORY_HEAP` && `NOTSA_PORTABLE`; VALIDATE_LAYOUT stays (compiled out) |

## Results
* Guard (`layout_diff.py guard`, warn mode): **before 10 hits (5+4+IplStore) -> after 0** (31 user TUs), run twice (before and after the IplStore fix).
* Sizes asserted (portable arm64, enforced): tScriptParam 4 (+ `tScriptParam[34]` 136), SYSTEMTIME record 16, CSimpleVariablesSaveStructure 0x138 (51 member offsets equal to clang `i686-pc-windows-msvc` dump, 0 mismatches), ExtraVertColour 24 (in-memory).
  Mutation checks: re-adding `void* pParam` and dropping systemtime.h each make `run_p2b_test.sh` fail.
* Host test: `tools/portable/run_p2b_test.sh` -> ALL OK (layout vs real headers + i686 offsets; prefix test: 4-byte LE for sizeof / ptrdiff_t / size_t writes via the real GenericGameStorage.h templates under `-DNOTSA_FILE_SCALAR_STRICT`, source scan "no unguarded write left, exactly 10 `<uint32|int32>` writes", GetLocalTime sanity).
* Native census on the 11 touched TUs + C_PcSave + Scripts/**: no new diagnostics from my lines (SimpleVariablesSaveStructure.cpp: 1 error -> 0). Remaining errors in those TUs are other work (operator new, Pool.h min, D3D constants, CPedSaveStructure size = P-2d). MSVC-mode census on the 11 TUs: 0 errors.
* Wine A/B (cl.exe x86, A = HEAD 6310e2ce tree, B = A + my files, NOTSA_SIG_NORM_STRINGS=1, no PCH): **15/15 TUs code-identical, data sections identical** (GangWars, PlayerInfo, PlayerPed, Ped, Object, TheScripts, IplStore, SimpleVariablesSaveStructure, CustomBuildingDNPipeline, CustomBuildingRenderer, MemoryMgr, MemoryHeap, GenericGameStorage, RunningScript, Group05_08). First round: IplStore (`<int32>` template symbol) and MemoryHeap (comment line shifted __LINE__) differed -> both fixed, re-run identical. Scratch /private/tmp/claude-501/p2b_ab.

## Consumer audit
* Prefix writes: loaders read `<uint32>`/`<int32>` at PlayerInfo:1455, PlayerPed:140, GangWars:65, Ped:528, Object:599, TheScripts:1149/1174 (poolRef `<uint32>`), IplStore:813 (int32): all already 4 bytes.
* tScriptParam: no user of pParam/szParam (grep source+tests); StoreArg.hpp `static_assert(sizeof(dst) <= sizeof(tScriptParam))` is arithmetic-only, unaffected.
* m_systemTime: C_PcSave.cpp:74 reads wMonth/wDay etc. (compiles, census clean).
* ExtraVertColour writer: only the portable branch changed; reader tests `!= 0` only.
* CMemoryHeap: macro defined nowhere.

## Open items
1. Wire `tools/portable/run_p2b_test.sh` (and run_file_records_test.sh) into CMake/ctest (W0 owns CMake). The layout part needs `tools/standalone/winstub` + a configured PortableMac compdb.
2. `portable/systemtime.h` is redundant with the global `SYSTEMTIME` in winapi_compat.h once W0 also adds `GetLocalTime`; then `SaveSystemTime` can alias `SYSTEMTIME` and the file can go (orchestrator/W0 decision).
3. The 5 prefixes still write `sizeof(<in-memory struct>)`; slices c/d/(CPed, CPlayerInfo...) must switch to `FileSizeOf<>` when their records exist (P-2d for CPedSaveStructure, which also still fails its 396 assert).
4. After b..k: define `NOTSA_FILE_SCALAR_STRICT` globally (P-2a open item 2) - nothing in the tree trips it now.
