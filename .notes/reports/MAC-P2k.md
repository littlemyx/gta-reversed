# MAC-P2k report: cross-slice verification of P-2a..j (2026-10-24, portable/main)

## Commits
* f6a7e1a6 ctest wiring + STRICT: `CMakeLists.txt` (enable_testing() under GTASA_PORTABLE), `source/CMakeLists.txt` (portable-only include of `tools/portable/portable_tests.cmake`; `NOTSA_FILE_SCALAR_STRICT` on the game target next to NOTSA_PORTABLE=1), `tools/portable/portable_tests.cmake` (new).
* b2bc35cd clang fix for the 7 census parse errors: `Replay.h`, `WaterLevel.h` (`#ifdef __clang__` only).

## Numbers
| gate | result |
|---|---|
| host suites (file_records, b..j) standalone and via ctest | 10/10 green; `ctest --test-dir build/PortableMac -L portable` = 10 tests, ~14 s wall (-j3). `-L mutation` = f,g,h,i,j with `mutate` (all mutants must fail the test): 5/5 passed (g 69 s, i 57 s, f/h/j ~1 min). First ctest run of g/i hung 30+ min in ASan's `atos` symbolizer (no tty): fixed with `ASAN_OPTIONS=...symbolize=0` in the ctest environment and in run_p2i_test.sh. `portable.p2d` is RED against P4's uncommitted `Weapon.h` (see open items 1). A broken COMPDB makes the script (and the test) fail (checked). |
| layout_diff.py run | identical 123, known-diff 13, NEW 2 (the pre-existing in-memory RpHAnimNodeInfo/rw::HAnimNodeInfo pair, not file records), not found 2 (variable/typedef names). All 9 `*_File` rows identical (BreakInfo 52, CMatrix 72, CPathNode 28, CPedSave 396, CPickup 32, CRunningScript 224, CVehicleSave 104, CWeapon 28, tRadarTrace 40). |
| guard (`layout_diff.py guard`, 31 user TUs) | **0 offenders** (was 10 at P-2a). STRICT is on for PortableMac (1104 compile commands carry `-DNOTSA_FILE_SCALAR_STRICT`); the 31 TUs compile with 0 guard errors; control: a scratch copy of GangWars.cpp with `SaveDataToWorkBuffer(sizeof(..))` (size_t) fails with the guard message at the call line. |
| native census, full snapshot (HEAD + my 2 headers, 1100 TUs, build/snap_census/p2k_native.tsv) | 273 unique diagnostics, **0 `_FILE`/guard/FileRecord diagnostics** (the P `_FILE` list, 27 at the P-2 start -> 0), 0 new vs p4_base.tsv (the 24 gone = the 7 Replay/WaterLevel errors + their cascades + platform.cpp). Remaining P rows are P-4's (operator new size_t 20, casts 101, mixed-type 16, AnimManager.h:30 array sizeof 1). |
| msvc-mode census, full (1117 TUs of the StandaloneRelease-style compdb incl. NOTSA_DETACHED_GLOBALS) | PCH closure **0 errors**; 7 left, all environment (header-not-found imgui_stdlib.h x4 / imgui_impl_sdl3.h x1 = submodule paths not in the snapshot, 1 inline asm, 1 other) - none in game code touched by P-2. |
| StandaloneRelease neutrality of the CMake change | `cmake -S src -B cfgA/cfgB` (cache copy of build/StandaloneRelease, A = HEAD CMake files, B = mine, same sources): build.ninja, compile_commands.json (4671 commands) and CMakeFiles/rules.ninja **identical after path normalisation**; no CTestTestfile generated. PortableMac: only the new librw/R3b files in compile_commands.json changed besides the new -D. |
| wine cl.exe A/B (Replay/WaterLevel fix), 9 TUs Replay, WaterLevel, Population, Heli, Group24a, Group05_08, Group23, MenuManager_Process, WaterLevelDebugModule | code+relocs+data **identical** (NOTSA_SIG_NORM_STRINGS=1, no PCH, DETACHED compdb). Earlier variant without `#ifdef __clang__` (user ctor on both) was DIFFERENT in the 5 TUs that use the types (4 extra COMDAT functions, `__builtin_array_init_helper<PolyInfo>`, Playback/Record/m_BlockPolyInfo/m_PolyCombos turned into dynamic initialisers) -> that is why the fix is clang-only. |

## The 7 msvc-mode census parse errors (Replay.h:143/166/171, WaterLevel.h:196)
* Reproduced only with `-DNOTSA_DETACHED_GLOBALS` (the real run-build flag; the local build/StandaloneRelease compdb is from Oct 16 and lacks it, so a census with it as-is shows 0, that was the confusion). They are also in the native (PortableMac) census (same 7) and in its PCH closure.
* Cause: `NOTSA_GLOBAL(Playback/Record, ..., (CAddressInReplayBuffer), {})` and `NOTSA_GLOBAL(m_BlockPolyInfo, ...(mdarray<PolyInfo..>)...)` became real `static inline` variables of a nested type with default member initializers (+ `= default` ctor) INSIDE the enclosing class when the globals were detached (Windows-side commits 1044fd8c B15 Replay, 7568eb50 B19 WaterLevel, 2026-10-17; lines 166/171 `expected ']'` are cascades). cl.exe accepts it; clang does not (same class as 0bc515d1 of Oct 11). Not from P-2 (no P-2 slice touches Replay.h/WaterLevel.h; last touches: a2d46d7c P-1 assert rename, 259f58c1 Windows fix) and not from the Windows merge itself.
* Fix: `#ifdef __clang__` DMI-free members + `constexpr` ctor (Replay.h CAddressInReplayBuffer, WaterLevel.h PolyInfo), original text in `#else`, so cl.exe sees the same code.

## Real save / replay.rep from the Windows host
NOT delivered (`~/gta-fixtures/` does not exist), so the round-trip check on a real `GTASAsf1.b` / `replay.rep` was skipped. All records are covered by the independent-golden tests, the real-data tests (NODES*.DAT, gta3.img DFFs, main.scm) and layout_diff. Still open for the Windows host: one new-game + mission save and a replay.rep via PORTABLE_SYNC.

## Open items
1. **P4 conflict (tests/portable/p2d_ped_test.cpp:10)**: P4's uncommitted `Weapon.h` change `enum ePedPieceTypes;` -> `: int;` breaks `portable.p2d` ("enumeration previously declared with nonfixed underlying type"). The test's local `enum ePedPieceTypes { PED_PIECE_TORSO = 3 };` must become `enum ePedPieceTypes : int {...}` in the same commit as the Weapon.h change (the real definition is `Ped.h:69 enum ePedPieceTypes : int`). Not touched by me (P4's file zone).
2. tools/portable/run_p2*_test.sh real-data parts skip with a NOTE when the Steam copy is absent (ctest then passes without them).
3. The local build/StandaloneRelease compdb is stale (Oct 16): `cmake --preset StandaloneRelease` regenerates it with NOTSA_DETACHED_GLOBALS before the next msvc-mode census.
4. `source/CMakeLists.txt` is a Windows hot file: my edits are 1 define in the portable branch + an include block at the very end, both under `if (GTASA_PORTABLE)`.
5. `-L portable` also matches nothing under label `mutation` (distinct on purpose).
