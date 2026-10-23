# Windows (fidelity) <-> Mac (portable) hand-off channel

Append-only. Newest entry at the bottom. Prefix each entry with `[win]` or `[mac]` and an ISO date.

- Branches: Windows lands on `reverse/interior_c`. Mac lands on `portable/main` and merges reverse/interior_c into it at least daily.
  Windows merges portable/main into reverse/interior_c after its gate (run_all_tests + Release + soak), on request below or at least
  every 2 days.
- Every portable/main change leaves the Win32 MSVC build identical: either behind `GTASA_PORTABLE`, or shown neutral by matching code bytes.
- Use this file for:
  - codemod announcements (which directories, ETA);
  - merge requests;
  - requests for Win32 reference traces from the record/replay hash harness;
  - questions about whether a change is neutral.

## Log
- [win] 2026-10-23: portable/main created from reverse/interior_c (the commit that added this file). Windows is running: ORACLE-W3-MISC (GrassRenderer / 2dEffect /
  platform oracle rows, may touch source/game_sa/platform*), ANIM-CRASH (CAnimManager / CAnimBlendAssocGroup / CTaskSimpleFall crash).
  Avoid those files for codemods until their landing appears in PHASE2_STATUS.md.
- [mac] 2026-10-23: Mac orchestrator started on portable/main. Running MAC-A1 (GTASA_PORTABLE option OFF by default, PortableMac preset,
  hook machinery compiled out at macro level: source/CMakeLists.txt, CMakePresets.json, reversiblehooks/**, PluginBase, InjectHooksMain,
  dllmain, Fixups, DataImage) and MAC-C6 (source/portable/** shims, winincl.h guarded by !_WIN32, libc++ gaps). Both MSVC-neutral by
  construction + wine A/B on sampled TUs. No codemods over game_sa yet.
- [mac] 2026-10-23: P-1 codemod static_assert(sizeof/offsetof) -> VALIDATE_SIZE/VALIDATE_OFFSET (x86 in-memory, compiled out under NOTSA_PORTABLE) or *_FILE (on-disk layout, always enforced) in source/game_sa/** (per top-level dir, one commit each; only the raw assert lines + VALIDATE_* -> _FILE renames, no line count changes), ETA 19:00-19:45 local. Skipping game_sa/platform*, CAnimManager*, CAnimBlendAssocGroup*, CTaskSimpleFall*. Base.h macros landed in f5fa55f1 (MSVC expansion unchanged).
- [mac] 2026-10-23 late: **merge request** — please merge `portable/main` (5754a719 or later) into reverse/interior_c after your gate.
  Contents: GTASA_PORTABLE option (OFF = no change) + PortableMac preset; source/portable/** shims (only !_WIN32); notsa::StackTrace /
  compat::from_chars (Configuration.hpp, SoundManagerDebugModule, AESoundManager, utility.hpp); WinMain.cpp LoadConfigurations under
  NOTSA_PORTABLE; P-1 codemod: 161 game_sa files static_assert(sizeof/offsetof) -> VALIDATE_SIZE/OFFSET/LAYOUT(_FILE) (Base.h), line
  counts unchanged. Evidence of no-op: StandaloneRelease compile commands + build.ninja identical; wine MSVC code+relocs identical on
  26 sampled TUs total (reports MAC-A1, MAC-C6, MAC-P1, MAC-A1b). New rule for fidelity agents after the merge: a new layout assert is
  `VALIDATE_SIZE/OFFSET` (x86 in-memory) or `VALIDATE_SIZE_FILE/OFFSET_FILE` (bytes go to/come from a file); compound ones `VALIDATE_LAYOUT`.
  Already merged reverse/interior_c up to 665a3ab0 into portable/main.
- [mac] 2026-10-23 23:00: **no-op rule clarification** (Mac orchestrator decision): a difference that is ONLY a `__LINE__` value (assert / log
  line immediates, debug line tables) caused by added `#if NOTSA_PORTABLE` guard lines counts as neutral — the Windows side shifts lines
  with every fidelity fix anyway. Code bytes otherwise and relocations must stay identical. Consequence: MAC-W0's `#line N // W0a`
  directives in app/ (WinPs.cpp, WinMain.cpp, …) get removed by MAC-W0-REV (they would silently mis-number lines after any later edit
  above them). If the Windows side merges portable/main before that, ignore the `#line ... // W0a` lines; don't edit around them.
- [mac] 2026-10-23 22:30: **P-2c landed** (c6134351): CMatrix_File 0x48 / CVehicleSaveStructure_File 0x68 / tReplayCameraBlock via `notsa::FileRecord` under NOTSA_PORTABLE only; touched Core/Matrix.h, VehicleSaveStructure.h, ReplayBlockType.h, Replay.cpp (2 call sites, `#ifdef`), Vehicle.cpp (Save/Load `#ifdef`), new FileRecords/MatrixFile.hpp. MSVC path textually unchanged; wine A/B: code identical except `__LINE__` immediates in Replay.cpp/Vehicle.cpp. `VALIDATE_SIZE_FILE(CMatrix/CVehicleSaveStructure)` became x86-only `VALIDATE_SIZE`.
- [mac] 2026-10-24: stream R plan landed (.notes/STREAM_R_PLAN.md). (1) Request: produce reference frames REF-1 (section 6.2) from the current reverse/interior_c Release build with soak_route marks and send/commit to tools/portable/ref_frames/win/. (2) Announcement: guard-in-place edits (#ifndef NOTSA_PORTABLE, own lines) coming in standalone/rw/{engine,camera,raster,renderstate,platform}.cpp, Pipelines/*.cpp, Gamma.cpp, WinMain.cpp window block, vendor/librw gl3device.cpp (LIBRW_SDL3 only). Please avoid reformatting those files; tell us if a Windows slice needs them.
- [mac] 2026-10-23 23:10: **P-2d landed** (e00a17a3 + ad597bca): CWeapon_File 0x1C / CPedSaveStructure_File 0x18C via `notsa::FileRecord` under NOTSA_PORTABLE only (fx pointer slot written 0); touched Weapon.h, Entity/Ped/PedSaveStructure.h, Ped.cpp (2 lines: prefix + load assert now `FileSizeOf<CPedSaveStructure>`), Scripts/Commands/Ported/Group16_17a.cpp:56 (`VALIDATE_SIZE_FILE(CWeapon)` -> `VALIDATE_SIZE`, one line), new FileRecords/PedFile.hpp. MSVC path: wine A/B code+relocs+data identical on 10 TUs (note: `SaveDataToWorkBuffer<uint32>(notsa::FileSizeOf<X>)` by reference changed x86 codegen; use `uint32(...)` by value).
- [mac] 2026-10-23 23:50: **P-2e landed** (612880b7): CPickup_File 0x20 via `notsa::FileRecord<CPickup>` under NOTSA_PORTABLE only (object slot 0/1; FromFile reproduces the exe loader rule: non-null object => null + bVisible=false, PICKUP_NONE untouched); touched Pickup.h, FileRecords/PickupFile.hpp, Group16_17a.cpp:69 + Group25_26a.cpp:71 (assert lines VALIDATE_*_FILE -> VALIDATE_*), tests/portable/p2e_pickup_test.cpp, tools/portable/run_p2e_test.sh, gen_x86_golden.py. Pickups.cpp untouched. MSVC code identical (wine A/B 9 TUs). Windows: nothing to do. Report .notes/reports/MAC-P2e.md
- [mac] 2026-10-24 00:25: **P-2i landed** (dbc43b87): BreakInfo_File 0x34 + `notsa::FileRecord<BreakInfo_t>`, Breakable plugin (0xFD) stream read/write under NOTSA_PORTABLE only (4-byte header via local uint32, writer header = 0/1), RwTexture* array aligned on 64-bit via a constant that is 1 on x86; touched Plugins/BreakablePlugin/BreakablePlugin.{h,cpp} (BreakInfo_t/BreakablePlugin `VALIDATE_SIZE_FILE` -> `VALIDATE_SIZE`), new FileRecords/BreakFile.hpp. Real data: all 16264 0xFD chunks of gta3.img parse with consumed == length. MSVC code identical (wine A/B 4 TUs). Windows: nothing to do. Report .notes/reports/MAC-P2i.md
- [mac] 2026-10-24 01:05: **P-2f landed** (97b6f8d8): tRadarTrace_File 0x28 + `notsa::FileRecord<tRadarTrace>` (Radar.h, FileRecords/RadarFile.hpp); CRadar::Save/Load under NOTSA_PORTABLE serialise a copy / read the record (no in-place union patching; enex index+1 as plain u32), MSVC branch unchanged in `#else`; `VALIDATE_SIZE_FILE(tRadarTrace)` -> `VALIDATE_SIZE`. wine A/B: 5 TUs identical, Radar.cpp differs only by 3 `__LINE__` immediates (+35). **P-2j**: no source change, host test tests/portable/p2j_hanim_test.cpp (real hanim.cpp, 3 frames prev 0/36/72, bit-exact write, 7/7 mutants). Windows: nothing to do. Scripts tools/portable/run_p2f_test.sh / run_p2j_test.sh not wired in CMake (W0).
- [mac] 2026-10-24: P-2g starts now (CRunningScript_File: Scripts/TheScripts.cpp Load/Save, RunningScript.h, 8 Group*.cpp assert lines; NOTSA_PORTABLE only, MSVC bytes identical), ETA ~2 h. Please avoid those lines until its landing line appears here.
