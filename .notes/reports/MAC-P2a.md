# MAC-P2a report: P-2 infrastructure (FileRecord trait, 64-bit scalar guard, layout_diff, golden generator)

Branch portable/main (pushed to `fork`). Date 2026-10-23.

## Commits
* d1a9d3e9 P-2a: `source/game_sa/FileRecord.hpp` (new), `GenericGameStorage.h`, `RenderWare/RenderWare.h`, `tests/portable/file_records_test.cpp`, `tools/portable/run_file_records_test.sh`
* (second commit) `tools/portable/layout_diff.py`, `tools/portable/gen_x86_golden.py`, notes (reports, `git add -f`)

## What landed
* `notsa::FileRecord<T>` / `HasFileRecord` / `FileSizeOf<T>`; typed `CGenericGameStorage::SaveDataToWorkBuffer<T>/LoadDataFromWorkBuffer<T>(T&)/<T>()` take the record path ONLY under `NOTSA_PORTABLE`
  (`if constexpr`); raw `(void*, int32)` overloads and the MSVC raw path are textually the old code.
* 64-bit scalar guard (`IsWidthUnsafeScalar<T>`: pointer, long, unsigned long, size_t, ptrdiff_t, intptr_t, uintptr_t, enums over them, arrays of them; vacuous when `sizeof(void*)==4`),
  two layers: (1) clang `diagnose_if` on the three templates (Save/Load/Load<T>() and `RwStreamRead<T>`) => one diagnostic **per call site**; warning by default (portable census keeps going), error with
  `-DNOTSA_FILE_SCALAR_STRICT`; (2) `FileScalarCheck<T>()` in the body ([[deprecated]] on non-clang, static_assert in strict). Policy: warning now, STRICT to be switched on globally after slices b..k.
  `File::Read<T>/Write<T>` do not exist in `extensions/File.hpp` (only raw `(void*, size)`), nothing to guard.
* Host test + runner (`tools/portable/run_file_records_test.sh`, flags taken from build/PortableMac/compile_commands.json): real GenericGameStorage.h with stubbed globals; golden 16-byte record round trip,
  truncated load, no-record raw copy, 4-byte LE prefix, guard truth table, warn mode (exactly 4 warnings), **6 negative TUs (size_t, pointer, long, ptrdiff_t, enum:size_t, pointer[]) each must FAIL to compile**
  under STRICT with the guard's message. Result: ALL OK (fails=0).
* `tools/portable/layout_diff.py run|guard`, `tools/portable/gen_x86_golden.py` (3 seed records CPickup_File 0x20, CWeapon_File 0x1C, CMatrix_File 0x48; `selfcheck` compares offsets with the i686 dump of the
  in-memory twins: OK).

## Guard offenders (10 call sites, `layout_diff.py guard`, 31 user TUs of the portable compdb)
| site | note |
|---|---|
| GangWars.cpp:75, PlayerInfo.cpp:1465, Entity/Ped/PlayerPed.cpp:162, Entity/Ped/Ped.cpp:545, Entity/Object/Object.cpp:610 | the 5 expected size_t prefixes (plan said Ped.cpp:547) |
| Scripts/TheScripts.cpp:1299, 1323, 1327, 1331 | the 4 expected `GetIndex()+1` (ptrdiff_t) writes |
| **IplStore.cpp:801** | **NEW**: `SaveDataToWorkBuffer(num)` with `num = ms_pPool->GetSize()` (size_t) writes 8 bytes; `CIplStore::Load` reads `<int32>` (line 813). Add to slice b: `<int32>(num)`. |
No other hit in any `RwStreamRead<T>` / typed Save/Load user. Not fixed here (slice b). With STRICT these 10 sites would fail to compile, as designed.

## layout_diff headline (.notes/reports/MAC-P2a-layout.md; i686-pc-windows-msvc clang-cl vs arm64-apple-macos with the portable flags incl. `-mms-bitfields`)
131 TSV rows: **111 identical**, 17 differ, 3 not comparable. Of the 17: 15 are the known MAC-P1 failing structs (the plan's 15; table in the layout report, all pointer-induced shifts, no surprises),
**2 NEW: `RpHAnimNodeInfo` / `rw::HAnimNodeInfo`** (16 -> 24: `RwFrame* pFrame`). It is the in-memory hierarchy node (TSV "doubt / no pointers" was wrong), not a raw file record (RW streams 3 int32 per node, librw);
no file fix needed, but nothing may read/dump `nodeInfo` raw. Not comparable: `CSimpleVariablesSaveStructure` (absent on arm64: SYSTEMTIME missing => class body lost, P-2b), `t2dEffectParticleStreamData` (not dumped by name:
local/unnamed in 2dEffect.cpp), `CStoredAnimationState` (a std::array alias). **Bit-field audit: all 30-odd identical rows with bit-fields have identical byte:bit offsets and widths** (arm64 build uses
`-mms-bitfields`, so this rests on that flag staying in the portable compile line - add to CMake review). No extra `*_File` slices needed (the plan's "1-3 extra slices" fear did not materialise);
std::array internals (`_Elems` vs `__elems_`) are normalised.

## No-op proof (sub-agent, cl.exe under wine, scratch /private/tmp/claude-501/p2a_ab; A = `git archive 8fcde553`, B = A + the 3 headers; NOTSA_SIG_NORM_STRINGS=1, no PCH, StdInc.cpp forced)
| TU | code bytes A/B | identical |
|---|---|---|
| GenericGameStorage.cpp | 143974 / 143974 | yes |
| Pickups.cpp | 124663 / 124663 | yes |
| GangWars.cpp | 55002 / 55002 | yes |
| Scripts/TheScripts.cpp | 153687 / 153687 | yes |
| Animation/AnimManager.cpp | 128576 / 128576 | yes |
`cmake --preset StandaloneRelease` A vs B: 1281/1281 compile commands identical, build.ninja identical after path normalisation, FileRecord.hpp not globbed. (Reloc names covered by obj_code_sig.)
Consumer audit: no state layout changed; the typed templates' callers are the 31 TUs listed by `guard`, behaviour on x86 unchanged (raw path).

## Open items
1. CMake wiring (MAC-W0 owns CMake): add `tools/portable/run_file_records_test.sh` as a ctest / custom target one-liner; keep `-mms-bitfields` in the portable flags.
2. Slice b additions: `IplStore.cpp:801` -> `<int32>`; after b..k are done define `NOTSA_FILE_SCALAR_STRICT` globally (e.g. in Base.h or the portable target) so new offenders fail.
3. `layout_diff.py run --fail-on-new` can be used as the P-2k gate (needs build/StandaloneRelease and build/PortableMac compile databases + ~/tools/msvc); `gen_x86_golden.py` gets its records from slices c..h.
4. layout_diff cache lives in /private/tmp (--work); runs take ~2.5 min cold.
5. The guard cannot see pointers/longs inside struct members of a record (only top-level scalars): those are P-2c..h's FileRecord rows + layout_diff.
