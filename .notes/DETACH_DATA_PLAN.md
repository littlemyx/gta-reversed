# Detach the standalone run build from gta_sa_compact.exe (plan, 2026-10-10, branch reverse/interior_c, HEAD 6fafc75a)

Goal: `GTASA_STANDALONE=ON` + StandaloneRelease builds and runs from a clean clone WITHOUT any original exe (assets from the user's Steam copy are fine).
Method: all numbers below were measured on `gta_sa_compact.exe` + `build/StandaloneRelease/standalone_data/*` (post-_initterm image) + a regex census of `source/`
(ad-hoc scripts, not committed; slice A1 re-derives them exactly). "est" = the C++ type size could not be read from the declaration text (492 of 2755), extent capped at 8 KB.

## 0. Totals

| item | number |
|---|---|
| committed data image (VA 0x858000..0xCB0000) | 4,554,752 B (.rdata 311,296 / .data 4,169,728 = init 262,144 + BSS 3,907,584 / _TEXT_HA 69,632 / _rwdseg 4,096) |
| non-zero bytes of the image (post-_initterm) | 563,252 B (.rdata 204,708; .data-init 179,602; BSS 127,968 = all from the initterm delta; _TEXT_HA 50,974) |
| `StaticRef` sites / distinct addresses | 2,807 sites in 366 files (1,798 in headers) / 2,755 addresses; 2,735 sites (97 %) are `auto& X = StaticRef<T>(0xADDR);` declarations |
| (a) zero-init globals | 1,902 (1,888 BSS); declared extent >= 680 KB (est-capped), real BSS 3.9 MB |
| (b) plain constants / tables | 699 (609 scalars <= 8 B, 50 of 9..64 B, 40 tables > 64 B); 18.3 KB non-zero bytes in total |
| (c) pointers (fn ptr / string ptr / data ptr tables) | 20 globals (6 are RW-internal = dead under librw, 14 game: 91 code ptr dwords, 337 data ptr dwords, 3.7 KB) + 523 exported vtables (6,198 slots) handled by the compiler |
| (d) objects needing ctors (initterm delta differs) | 134 (133 BSS), 33.5 KB non-zero, 90 files; plus 1,489 float-constant initialisers (7 live) |
| nonzero image bytes NOT reachable by any `StaticRef` / registered vtable | .rdata 186,543; .data-init 142,561; BSS 94,501; _TEXT_HA 50,974 (= EH metadata 0x88F7xx..0x8A19xx, `_initterm` table 0x8A4000, CRT/D3DX/RW tables, ported constants) |
| bytes of exe-derived content that must be committed as C++ literals | ~22 KB (b: 18.3 KB, c: 3.7 KB; 16.1 KB of it are the 40 tables, see section 5) + structure of 134 objects (ctors, mostly vptr/-1 fills) |
| slices | 38 (28 if the 20 codemod batches are merged in pairs) |

## 1. Inventory: everything the build / the running exe takes from gta_sa_compact.exe

### 1.1 Build time
| # | where | what | detach action |
|---|---|---|---|
| 1 | `source/CMakeLists.txt:48-52` | cache var `GTASA_ORIGINAL_EXE`; WARNING if empty ("exe will refuse to start"), FATAL if the file is missing | delete (oracle tests get their own optional var, G1) |
| 2 | `source/CMakeLists.txt:215-237` | `add_custom_command` -> `extract_exe_data.py <exe>` -> `original_data.bin` (4,554,752 B), `.json` (2.7 KB), `data_pointers.bin` (78,360 B = 9,795 x 8), `.txt` (443 KB); DEPENDS the exe, `initterm_delta.py`, `.notes/symbols.txt`; target `gta_reversed_exe_data`; POST_BUILD copy of bin/json/pointers next to the exe | delete |
| 3 | `source/CMakeLists.txt:185-213` | link flags `/BASE:0x400000 /DYNAMICBASE:NO /FIXED /MERGE:_TEXT=.text /INCREMENTAL:NO`; `orig_image_pad.obj` (9.1 MB zero `.text`, `tools/standalone/make_orig_pad.py`, 38 lines) forced FIRST on the link line | delete in F3 |
| 4 | `source/CMakeLists.txt:195-198` | python required (`GTASA_PYTHON`), needs `unicorn` (+`capstone` for tests) | only for optional oracle tooling afterwards |
| 5 | `CMakePresets.json:22` | StandaloneRelease preset hard-sets `GTASA_ORIGINAL_EXE=${sourceDir}/gta_sa_compact.exe` | delete |
| 6 | `.gitignore:72-82` | ignores original_data.* and `/gta_sa_compact.exe` | keep the exe line |
| 7 | `tools/standalone/run_all_tests.sh:25,45` | `EXE=${GTA_EXE:-$REPO/gta_sa_compact.exe}`, exports `RW_EXE_ORACLE`, passes `-DGTASA_ORIGINAL_EXE` | make optional |
| 8 | `tools/standalone/soak.sh:28,63,64` | requires/copies gta_reversed.exe + data_pointers.bin + original_data.bin/json into the run dir | drop 3 files |
| 9 | `docs/STANDALONE.md:8,26,48,61-66` | documents the exe as build input | rewrite |

### 1.2 Run time (`NOTSA_STANDALONE_RUN`, 89 `#ifdef` hits in 34 files)
| # | where | what |
|---|---|---|
| 10 | `source/standalone/DataImage.cpp` (202 l) + `.h` (28 l) | `.CRT$XIB` early init reads `original_data.json` + `.bin` next to the exe, `VirtualProtect`s the in-image pad RW, fills 4,554,752 B; code range 0x401000..0x858000 -> PAGE_NOACCESS; `ORIG_PAD_SIZE=0x8B0000` |
| 11 | `source/standalone/Fixups.cpp` (627 l) + `.h` (80 l) | registry exeAddr->our fn (9,403 fns + 2,083 vtable slots); `ApplyToDataImage` reads `data_pointers.bin`: 523 classes copied whole (6,192 slots), 93 by function, trapped V 1,755 + C 1,909 (dead: `_initterm` array 1,665, C++ EH metadata 1,808, RW/D3D tables); `InstallRedirectHandler` (vectored AV handler for raw calls into the NOACCESS range); trap stubs |
| 12 | `source/app/platform/win/WinMain.cpp:575-591` | `DataImage::Load` -> `InstallRedirectHandler` -> `RHManager` + `InjectHooksMain()` (registers) -> `ApplyToDataImage` |
| 13 | `source/Base.h:178-186` | `StaticRef<T>(addr)` = `*(T*)addr` (+ assert image loaded); `ScopedStaticRef` (4 uses) |
| 14 | `source/PluginBase.h:13,36` | `plugin::Call*<addr>` -> `Fixups::ResolveCallTarget` (trap if unknown) |
| 15 | `source/reversiblehooks/{RHManager.h:143-237, RHManager.cpp:60-71, ReversibleHooks.h:49, VMTInfo.cpp:6}` | `RH_Scoped*` macros feed the registry in the run build (6,446 `RH_ScopedInstall`, 1,961 `RH_ScopedVMTInstall`, 530 `RH_ScopedVirtualClass`, ... ~10.7 k macro uses) |
| 16 | `tools/standalone/initterm_delta.py` (63 l) | Unicorn replay of the 1,667 `_initterm` initialisers: 18,250 ranges, 127,968 B BSS + 53 B .data, 19 `rand()` calls (CHandShaker x6 = 18, CMenuManager = 1) |
| 17 | `tools/standalone/extract_exe_data.py` (302 l) | PE parse; pointer classifier (`function_starts` = 9,014 symbols + 9,782 E8/E9 targets + 9,357 install addrs = 13,085; `is_code_pointer`; DENY_RANGES 12 entries; ALLOW 1) |
| 18 | `.notes/symbols.txt` (9,016 lines, addr+name, function symbols only, NO data symbols) | classifier input; repo-resident RE metadata, not an exe dependency |

### 1.3 Residual raw calls into exe code (run build)
| kind | count | where | status |
|---|---|---|---|
| `plugin::Call*` live in the run build | ~9 | `MemoryMgr.cpp:255-493` (CRT allocators, replaced under RUN by host CRT, to be verified), `preset_view.cpp:24` | verify, F1 |
| `plugin::Call*` dead in run build | ~45 | `Collision.cpp:4508-4749` (`TEST_COLLISION_FUNCS`), `WindowedMode.cpp` (excluded with librw), `RwCompat.h:45` (non-librw branch), `game_sa/RenderWare/rw/*.cpp` (excluded with librw; 18 files, 81 files in dir) | leave, compile-time guard in F1 |
| raw casts `(T*)0xADDR` / `*(T*)0xADDR` / `reinterpret_cast<T*>(0xADDR)` | 15 + 8 | `Shopping.h:136`, `Pickups.h:31`, `Pickups.cpp:395`, `Matrix.cpp:12-13`, `Vehicle.cpp:55`, `Automobile.cpp:4107`, `AEWMADecoder.cpp:8`, `Streaming.cpp:2661`, `TimeCycle.cpp:302-303`, `RenderWare.h:40-51`, `CustomBuildingDNPipeline.cpp:187`, `dllmain.cpp:33`, `LinkList.h:25-29` (guarded NOTSA_STANDALONE) | convert in C-slices / F1 |

### 1.4 Tests and tooling that legitimately keep needing the exe (dev-only, must become optional)
| item | count | note |
|---|---|---|
| `tests/standalone/*_test.cpp` | 40 total; 23 read `getenv("RW_EXE_ORACLE")` (21 `*oracle*`); exit 77 when the exe is absent | they map the exe's .text/.data at the original VA and run original machine code next to the port, sharing `StaticRef` globals through the VA -> need "address mode" (A2) |
| oracle tooling | `gen_oracle_fixups.py`, `lift_x87.py`, `strip_unicorn.py`, `quat_unicorn.py`, `gen_strip_cases.py`, `extract_img.py` (reads the game's gta3.img, an asset, fine) | no build dependency |
| already committed exe-derived artifacts | `source/standalone/rw/{camera_sync_lifted.inc 126,915 B, pipeline_skin_cpu_lifted.inc 89,672 B, pipeline_skin_lifted.inc 13,433 B}` (machine-lifted RW code), `rwmath_exact.h`/`rwmath_tables.cpp`/`rwtrig.h` constants, `tests/standalone/rw_{quat,strip}_cases.inc` | not an exe dependency; legal flag L3 |

## 2. Data image: structure and categories

### 2.1 Image, per section (post-_initterm)
| section | VA | size B | non-zero B | initterm delta B | code-ptr dwords (V) | data-ptr dwords | claimed by StaticRef/vtable: nz B / code ptrs | unclaimed nz B / code ptrs |
|---|---|---|---|---|---|---|---|---|
| .rdata | 0x858000 | 311,296 | 204,708 | 0 | 7,891 (6,006) | 2,773 | 18,165 / 5,973 | 186,543 / 1,918 |
| .data (init) | 0x8A4000 | 262,144 | 179,602 | 53 | 1,885 (1,830) | 3,810 | 37,041 / 134 | 142,561 / 1,751 |
| .data (BSS) | 0x8E4000 | 3,907,584 | 127,968 | 127,968 | 0 | 15,038 | 33,467 / 0 | 94,501 / 0 |
| _TEXT_HA | 0xC9E000 | 69,632 | 50,974 | 0 | 19 (2) | 65 | 0 / 0 | 50,974 / 19 |
| _rwdseg | 0xCAF000 | 4,096 | 0 | 0 | 0 | 0 | - | - |
| total | | 4,554,752 | 563,252 | 128,021 | 9,795 (7,838 V / 1,957 C) | 21,686 aligned (extractor says 22,549) | 88,673 / 6,107 | 474,579 / 3,688 |

Data-pointer edges (aligned dwords, 21,686): ->vtable 15,017 (all vptrs of ctor-built objects incl. 14,119 in big CStore/pool arrays), ->.rdata string/const 5,851, ->another global only ~190 (24 d->d, 17 c->c, 90 unclaimed->zero-global, ...). So rebasing pointers between globals is a non-problem; vptrs come from the compiler; string pointers become literals.

### 2.2 Referenced globals by category (2,755 addresses; sections: .rdata 13, .data-init 721, BSS 2,021)
| cat | globals | .rdata | .data-init | BSS | extent B | nz B | est | biggest |
|---|---|---|---|---|---|---|---|---|
| a zero | 1,902 | 1 | 13 | 1,888 | 680,252 (capped) | 0 | 329 | `uint8[0x8000]` @0xBC40D8, `char[16384]` @0xC8E0C8, `CStreamingInfo[26316]` @0x8E4CC0, CPathFind, CPickup[], CRunningScript[], tReplayBuffer[], CBaseModelInfo*[NUM_MODEL_INFOS] @0xA9B0C8, CAESoundManager (all est-capped, real sizes larger) |
| b const | 699 | 12 | 687 | 0 | 29,807 | 18,257 | 34 | CAEAmbienceTrackManager @0x8AC15C (5,396 B / 4,887 nz), tRadioSoundRange[12][31] x2 @0x8C9610/0x8CA1B0 (2,976 / 1,476 each), char[210][14] @0x8D3978 (2,940 / 1,380), int32[12][22] @0x8CB280, int32[12][31] @0x8C9040, float[28][6] @0x8CC8E0, int32[13][23] @0x8CADD0, sReverbEnvironment[] @0x8AD670, int32[TOTAL_CHEATS] @0x8A5CC8 (cheat hashes), BoneInfo_t[] @0x8D26D0, FollowCarCamSettings[7] @0x8CC600, float[80] @0x8D5658, uint8[256] x2 @0x8D2C00/0x8D2D00 |
| c ptr | 20 | 0 | 20 | 0 | 30,950 (est-inflated) | 18,846 | 19 | AnimAssocDefinition[118] @0x8AA5A8 (236 string ptrs), tStatLine @0x8CDBF8.. (stat names), Cheat fn table @0x8A5B58 (65 code ptrs), CommandHandlerTable @0x8A6168 (26 code ptrs), crosshair array @0x8D6144, tWaterCreatureInfo[] @0x8D3698, section names, cutscene vehicle names; RW-only (dead): RtDictSchema @0x8DED50, RwPluginRegistry @0x8D628C, RwSurfaceProperties @0x8A645C, RwMemoryFunctions @0x8D6228, 2 RwObjectNameIdAssocation tables |
| d ctor | 134 | 0 | 1 | 133 | 193,463 (est-capped) | 33,554 | 110 | `CNodeAddress[5000]` @0x972CD0 (10,000 nz: -1 fill), `CScriptedEffectPairs[]` @0xC3BB00 (6,200), `tUserList[]` @0xC3A200, `CAnimBlendHierarchy[2500]` @0xB4EA40, CAEAudioHardware @0xB5F8B8, `CStore<CAtomicModelInfo,...>` @0xAAE950 (+ 5 more CStores, 14 k elements with vptr), CControllerConfigManager @0xB70198, `CPedGroup[8]` @0xC09920, CPedScriptedTaskRecordData[] @0xC0B1E8, CScriptsForBrains @0xA90CF0 |
| total | 2,755 | 13 | 721 | 2,021 | | | 492 | |

Unreferenced-by-address but already ported (hex only in comments): 1,134 distinct addresses (481 .rdata float/int constants inlined in function bodies, 532 .data, 121 BSS).
Excluded from the run build: 34 addresses used only in `game_sa/RenderWare/rw/*.cpp` + `WindowedMode.cpp` (live: 2,721).

### 2.3 The `_initterm` side (from `.notes/INITTERM_AUDIT.md`)
1,667 initialisers: 1,489 float constants (7 live: 5 `StaticRef` reads 0xC1BFF0, 0xC1C81C, 0xC1C818, 0xC279CC, 0xC18D48 + 2 header defaults; 1,475 dormant), 178 ctor/fill (91 need a replica, 80 live; 66 no net effect; 20 atexit-only; 1 CRT cookie). Biggest effects: model-info CStores (vptr in ~14 k elements), CPedGroup[8], CColModel x ~35 (`0x40FB60`), -1 fills (`aNodesToBeCleared` 10,000 B, `RoadBlockNodes` 650 B, `aExteriorNodeLinkedTo`), CMenuManager (writes 52 B outside its object), CHandShaker x6 (18 `rand()`).

## 3. Fixed-address reliance in code and how to generate definitions

### 3.1 What still depends on original addresses
| mechanism | count | after detach |
|---|---|---|
| `StaticRef<T>(0xADDR)` decl sites | 2,735 (`auto&` decls) + 12 `T& X = StaticRef` + 13 accessor returns + 47 expression uses | become real variables (macro, A2) |
| duplicate declarations of one address | 52 (2,807 sites vs 2,755 addrs) | owner + alias refs (A4) |
| type conflicts on one address | 12 (e.g. 0x8A5B24 int32/uint32; 0xB6B98C CPed*/tVehicleAudioSettings*; 0xBB3E00 float[256] x3 spellings; 0xC17824 CColModel / array<CColModel,2>; 0xC278F8, 0xC4D958, 0xC8CF68 WinVer/OSStatus) | decide in A4 |
| extents overlapping another declared address | 93 candidates (many from my enum=4 B guess; real: ~25 member/aggregate aliases, e.g. 0xB613EC speech-context array vs its fields, 0xB6EC5C array<bool,29> vs floats at +4) | A4, resolved with real `sizeof` |
| `RH_Scoped*` with a DATA address | 519 (523 vtables incl. templates; 6,198 slots) | vtables: compiler; macro args unused in run end-state |
| `RH_Scoped*Install` code addresses | 9,357 distinct (13,085 function starts known) | registry deleted from run build (F2) |
| raw casts / reinterpret | 23 | C-slices / F1 |
| x87-lifted `MEMABS(0xC97B24/0xC979BC)` | 2 | already mapped by `camera_sync.h` switch (self-contained) |
| oracle tests | 21 + 2 | keep VA/"address mode" (A2) |

### 3.2 Reusable pieces of the classifier
`extract_exe_data.py`: `parse_pe`, `function_starts`, `is_code_pointer`, DENY/ALLOW (single source of truth), `text_like`; `initterm_delta.compute`; `data_pointers.txt` (class V/C/S/U per dword); `INITTERM_AUDIT.md` table (178 rows: ctor VA -> object VA -> `StaticRef` symbol -> action).

### 3.3 Generator `tools/standalone/gen_globals.py` (dev-time, needs the exe ONCE, output is reviewed and committed; the build never runs it)
1. Scan `source/` for every `StaticRef<T>(0xA)` / `ScopedStaticRef` / raw cast: row = {addr, owner file:line, C++ name (qualified), type text, all other sites}. Reuses the regex census (2,755 rows).
2. Type probe: emit `tools/standalone/sizeof_probe.cpp` (includes the headers, one `{sizeof,alignof,is_trivially_default_constructible,is_polymorphic,is_pointer}` row per distinct type text, 719 distinct), build in the Debug standalone config, run under Wine -> `probe.json`. Removes all 492 "est" rows and decides (d) by trait instead of by delta.
3. Read bytes `[addr, addr+sizeof)` from the post-initterm image; classify: zero -> (a); delta differs or polymorphic/non-trivial ctor -> (d); has pointer dwords -> (c); else (b).
4. Emit initialiser expression per category: (a) none; (b) literal by type (`float` as `std::bit_cast<float>(0x3F800000u)`/hex-float to keep bits, int/bool/enum, arrays via nested braces; zero runs compressed with a helper `notsa::fill<N>(v)`); (c) per pointer dword: code ptr -> `&Qualified::fn` found through `RH_Scoped*Install(fn, 0xADDR)` + `RH_ScopedClass(...)` context (9,357 addresses) else `nullptr /*TODO 0xADDR*/` listed in a report; string ptr (NUL-terminated printable ASCII in .rdata) -> literal; global ptr -> `&name`; vtable ptr -> omitted; (d) no value: report `objects.tsv` {addr, type, ctor VA from the audit, repo ctor exists?, delta nz bytes, vptr dwords}.
5. Codemod `tools/standalone/codemod_globals.py --files <list>`: rewrite `[static] [inline] auto& NAME = StaticRef<T>(0xA);` to `NOTSA_GLOBAL(...)` (macro in A2) with the literal from 4, idempotent, emits aliases from `aliases.json` (A4).
6. Verifier `tools/standalone/verify_globals.py` + `NOTSA_VERIFY_GLOBALS` (A2): the exe writes `{addr, size, bytes}` for every converted global at boot (+ after 60 s in game); the script compares against `original_data.bin`, masking vptr dwords (value must equal the address of OUR vtable, checked by class), pointer dwords (compared by target kind), and the documented intended deviations (`standalone_deviations.tsv`, e.g. rand()-derived bytes of gHandShaker / FrontEndMenuManager, the 9 known header-default fixes).

### 3.4 Target macro (A2), three modes so the DLL build and the oracle tests keep working
`NOTSA_GLOBAL(name, addr, (Type), init...)`:
- `NOTSA_DETACHED_GLOBALS` (new define, only on the real standalone exe target): `inline Type name init;` (constant-initialised where possible, so no static-init-order problem; objects (d) are normal globals with ctors).
- otherwise (ASI/DLL build, hook dump, oracle tests, anything with `NOTSA_ORACLE_ADDRESS_GLOBALS`): `inline Type& name = StaticRef<Type>(addr);` (today's behaviour, bit-for-bit).
`auto&`-declared class statics keep their qualified names, so no call site changes (2,735 decls, 0 use-site edits except the 47 expression uses and 13 accessors).

## 4. Slices (each = one sonnet agent + line-by-line review pass by another sonnet; <= 5 agents in parallel; every slice ends with the acceptance block AT below)

**AT (every slice):** (1) `cmake --preset StandaloneRelease` + `ninja -C build/StandaloneRelease gta_reversed.exe` 0 errors; (2) `tools/standalone/soak.sh build/StandaloneRelease 3` exit 0 (menu -> NEW GAME -> 3 min in game, zero trap/assert/AV in `standalone.log`); (3) `tools/standalone/run_all_tests.sh` same pass/fail table as before the slice (23 oracle tests still need the exe = address mode); (4) from A2 on: `verify_globals.py` 0 unexplained mismatches for all addresses converted so far; (5) from A3 on: the converted ranges are poisoned (0xDD) in the image under `NOTSA_POISON_CONVERTED=1` and (2) still passes; (6) the ASI/`DUMP_HOOKS_ONLY` configure+compile still passes (mode macro). Commit only own files; trailer as per repo rules.

### Phase A: mechanism (sequential)
| # | slice | content | depends | extra acceptance |
|---|---|---|---|---|
| A1 | gen_globals scanner + probe | steps 1-3 of 3.3 -> `.notes/DETACH_GLOBALS.tsv` (2,755 rows), `probe.json`, `objects.tsv` (134 rows), duplicates (52) / conflicts (12) / overlaps lists | - | row count = 2,755 +- 0; est = 0; category counts within 2 % of section 0 |
| A2 | `NOTSA_GLOBAL` macro + codemod + verifier | Base.h macro (3 modes), CMake `GTASA_DETACHED_GLOBALS` (OFF), codemod tool, `NOTSA_VERIFY_GLOBALS` dump, `verify_globals.py`; pilot = `Timer.h` (25), `Hud.h` (68), `Cheat.h` + `Cheat.cpp` (65 fn ptrs), `Pickups` colour table | A1 | pilot: option ON -> the 95 pilot globals match 0 mismatches; option OFF -> bit-identical object code of the pilot TUs (dumpbin /disasm diff = 0) |
| A3 | poison + census | `DataImage`: env `NOTSA_POISON_CONVERTED=<file>` fills ranges with 0xDD; env `NOTSA_IMAGE_CENSUS=1` protects the image page-wise, logs first touch per page + IP, then unprotects (1,112 pages); `census.tsv` of pages touched in a 10-min soak that no converted global / vtable explains | A2 | census run produces the list; the poisoned pilot passes AT(2) |
| A4 | alias / conflict decisions | resolve 52 duplicates (owner + `auto& alias = Owner;`), 12 conflicts, ~25 overlap clusters -> `aliases.json` consumed by the codemod; pick ONE type per address | A1 | codemod `--check` on all 366 files: 0 unresolved |

### Phase B: (a)+(b)+(c) codemod batches (mostly independent: run <= 4 in parallel after A4; (d) objects inside these files are left as address-mode `StaticRef` until Phase C, c-items needing function names are done by the agent from the generator report)
Weights: a=1, b=3, c=10, d=25 points per global, cap 450 points / 25 files per batch. 357 owner files -> 20 batches (a / b / c / d-deferred globals).
| # | files (first..last, sorted by path) | a | b | c | d (deferred) |
|---|---|---|---|---|---|
| B01 | 25 files: app.h .. AudioZones.h | 87 | 20 | 2 | 5 |
| B02 | 24: AEAudioEntity.cpp .. BouncingPanel.h | 95 | 34 | 1 | 9 |
| B03 | 9: BreakManager_c.cpp .. Cam.cpp | 74 | 56 | 0 | 6 |
| B04 | 19: Camera.cpp .. ColStore.h | 145 | 55 | 1 | 5 |
| B05 | 8: Collision.cpp .. PedToPlayerConversations.h | 34 | 6 | 0 | 15 |
| B06 | 25: Matrix.cpp .. Object.h | 120 | 8 | 1 | 4 |
| B07 | 22: Ped.cpp .. Train.h | 40 | 65 | 0 | 8 |
| B08 | 25: Vehicle.cpp .. Game.h | 86 | 45 | 0 | 9 |
| B09 | 22: GameLogic.cpp .. FurnitureManager_c.h | 182 | 14 | 0 | 9 |
| B10 | 19: FurnitureSubGroup_c.cpp .. MissionCleanup.cpp | 72 | 5 | 0 | 7 |
| B11 | 1: ModelIndices.cpp (164 `ModelIndex` consts, all b) | 0 | 164 | 0 | 0 |
| B12 | 13: DamageAtomicModelInfo.h .. Pad.h | 53 | 10 | 3 | 12 |
| B13 | 25: PathFind.cpp .. BreakablePlugin.h | 76 | 17 | 0 | 4 |
| B14 | 25: CollisionPlugin.cpp .. D3DResourceSystem.h | 167 | 74 | 0 | 2 |
| B15 | 20: RenderWare.h .. TheScripts.cpp | 123 | 14 | 3 | 8 |
| B16 | 15: TheScripts.h .. StencilShadows.h | 174 | 34 | 7 | 4 |
| B17 | 18: Streaming.cpp .. TaskSimpleCarGetIn.cpp | 66 | 24 | 0 | 11 |
| B18 | 21: TaskSimpleClimb.cpp .. VehicleRecording.h | 143 | 20 | 0 | 9 |
| B19 | 20: VisibilityPlugins.h .. cTransmission.cpp | 154 | 34 | 2 | 6 |
| B20 | 2: common.h (+ leftovers, 47 expression uses, 13 accessor returns, 4 `ScopedStaticRef`) | 11 | 0 | 0 | 1 |
(Table = alphabetical packing of owner files; B01 and B02 need the explicit file lists emitted by A1 (`batches.json`), the first..last range is only a label.) B05/B03/B12 contain the model-info/ColModel owners: order B11 first (pure b, proves the literal emitter on 164 `ModelIndex` values).

### Phase C: (d) objects, 134 globals in 6 families (after B of the owner files; <= 3 in parallel). Each object = real global with the repo's own ctor / default member initialisers; compare with the delta via the verifier (vptr dwords masked); where the repo ctor is missing or wrong the agent reverses the ctor from `.notes/decomp/<VA>_*.c` and `.notes/INITTERM_AUDIT.md` section 5.
| # | family | globals | notes |
|---|---|---|---|
| C1 | Tasks (PedGroupDefaultTaskAllocators.h x6, TaskSequences, TaskSimpleClimb x4 ...) + Attractors + PedGroups[8] + Gangs + InformEvent queues | 22 | ctor 0x4xxxxx per allocator, `CPedGroup[8]` 1,208 nz B |
| C2 | Collision (TempColModels.h x13 incl. `ms_colModelCutObj[20]`, Game.h, Weapon.h) + Matrix.cpp + OctTree | 17 | ctor `0x40FB60` is shared; `CColModel` bbox seeds 1.0f |
| C3 | Entity (Ped/Physical `CColPoint[32]`, Automobile/Bike float seeds) + Cam.cpp x3 + Camera.cpp/.h x4 (`TheCamera` @0xB6F028) | 22 | TheCamera ctor writes outside the object (see audit) |
| C4 | Models (6 `CStore<..>` ModelInfo.cpp, ~14 k pre-constructed elements) + Animation (`CAnimBlendHierarchy[2500]`) + Fx/FxManager + Interior (`Furniture_c[256]`) + Clothes | 21 | biggest vptr consumers; element default ctors must set vptr + `-1` ids |
| C5 | Audio x12 (+ Explosion/Weather audio entities) + ControllerConfigManager + MenuManager (rand) + HandShaker (18 rand) + Pad + HudColours | 24 | rand-derived bytes are documented deviations |
| C6 | Scripts x7 (`CScriptsForBrains`, `Scripted2dEffects` 8,192+2,304 B), PathFind (`CNodeAddress[5000]` and `[325]` -1 fills, 5000-entry loops), RoadBlocks, Garages, World/Replay `CPlayerInfo`, ~25 single globals | 28 | |
Acceptance extra: object size equals `sizeof` (static_assert), verifier: 0 mismatches outside masked vptr dwords; boot with `NOTSA_POISON_CONVERTED` on the whole family.

### Phase D/E: leftovers inside the data (independent of B/C)
| # | slice | content | depends |
|---|---|---|---|
| D1 | Cheat table + script handler table + pickups colours + CLodTimeModelInfo vtable | 65 + 26 + 3 + 5 = 99 live pointer dwords (TRAP_READERS.md). Cheat table: `&CCheat::XxxCheat` from the registry; script group table: delete it and the `s_OriginalCommandHandlerTable` fallback once S6 (977 commands, `.notes/SCRIPT_CMDS_PLAN.md`) is complete, otherwise keep a fatal naming the opcode; pickup colours: constexpr | A2; S6 done for the script table |
| D2 | strings and name tables | AnimAssocDefinition (236 string ptrs), tStatLine x8, section/vehicle/ped-name tables, crosshair/waterCreature: literals; dedupe against tables already in source (`AnimAssocAnimations.h`, 1,134 comment-only addresses) | A2 |
| E1 | initterm float constants | the 5 live float stores (`0xC1BFF0` = 0x3BB60B6A, `0xC1C81C` = 0x3BB60B6A, `0xC1C818` = 0x3B83126E, `0xC279CC` = 0x3C03126F, `0xC18D48` = 0x3F6C835E cos 22.5 deg) and 2 header defaults as `constexpr` (bit exact); the 1,475 dormant stay out | - |

### Phase F: remove the image machinery from the run build (after A-E are 100 %: census shows 0 unexplained touched pages)
| # | slice | content | acceptance extra |
|---|---|---|---|
| F1 | raw calls / casts | ~9 live `plugin::Call*` (MemoryMgr, preset_view), 23 raw casts -> named globals; in the run build `plugin::Call*` becomes a `static_assert`/compile error (the 45 dead ones are `#if`-guarded) | grep `plugin::Call` in compiled files = 0 |
| F2 | hooks out of the run build | `RH_Scoped*` -> no-ops under `NOTSA_DETACHED_GLOBALS` (RegisterFunction/RegisterVMTSlot/RegisterVMTClass/RegisterUnverified calls gone: 9,403 + 2,083 + 523 registrations, startup time drops); `VMTInfo` exe reads gone; DLL build untouched | soak (2) passes with `Fixups::ApplyToDataImage` still called but registry empty -> must find 0 non-vtable code ptr it needs |
| F3 | delete image loader | remove `DataImage.cpp/.h`, `ApplyToDataImage`, `InstallRedirectHandler`, trap stubs, `data_pointers.bin` code, `make_orig_pad.py`, pad obj, `/BASE:0x400000 /FIXED /DYNAMICBASE:NO /MERGE`, `ORIG_PAD_SIZE`; keep `Fixups::Log/Fatal/UnreachableHit/LogFpuState` (move to `standalone/Log.cpp`); `WinMain.cpp:575-591` shrinks to `NOTSA_WinMain` | exe file size -9.1 MB pad; starts without original_data.* in the run dir |

### Phase G: build and clean-clone proof
| # | slice | content |
|---|---|---|
| G1 | CMake / presets / scripts | delete `GTASA_ORIGINAL_EXE`, extract custom command, `gta_reversed_exe_data` target, POST_BUILD copy, preset line 22, soak.sh 3 files, docs/STANDALONE.md; oracle tests get optional `GTASA_ORACLE_EXE` (tests only; `RW_EXE_ORACLE` env stays; address mode); `unicorn`/`capstone` needed only for tools |
| G2 | clean-clone acceptance | fresh `git clone`, `gta_sa_compact.exe` absent from disk (not just from the tree), `cmake --preset StandaloneRelease` + build, run dir = exe + Steam assets only, `soak.sh 20` exit 0, non-oracle tests (17) pass, oracle tests exit 77 (skipped), then with the exe restored the 23 oracle tests pass again |

Order / critical path: A1 -> A2 -> (A3 | A4) -> {B01..B20 (4 parallel), D1, D2, E1} -> C1..C6 (after the B batch that owns their files) -> F1 || F2 -> F3 -> G1 -> G2. Slices: 4 + 20 + 6 + 3 + 3 + 2 = 38.

## 5. Legal angle (derived tables are still derived from the exe)
| class | what | size | assessment | recommendation |
|---|---|---|---|---|
| L0 | names, types, sizes, addresses, layout (zero-init globals, 1,902) | 0 B of content | comes from the repo's own source / RE metadata | free |
| L1 | scalar constants (609 <= 8 B + 50 of 9..64 B: floats like 1.0f, ints, bools, 2.1 KB total), 5 initterm floats, vtable pointers, -1 fills, ctor state | 2.1 KB | facts / engineering constants, 481 of the .rdata ones already appear inline in ported code | commit; cross-check against the ported function bodies before taking the exe value |
| L2 | game-content-like tables: 40 tables > 64 B (16,103 nz B) + game string tables (14 c-globals, 3.7 KB, ~470 string pointers): radio sound ranges (tRadioSoundRange x11, ~5.9 KB), AEAmbienceTrackManager (4,887 B), char[210][14] (1,380 B), int32[12][22]/[12][31]/[13][23] audio tables, float[28][6] cam tuning, FollowCarCam/FollowPedCam/AimWeaponCam settings, reverb environments, BoneInfo, cheat hashes, anim/stat/vehicle name tables | ~20 KB | compilation of values chosen by Rockstar; thin protection for factual tuning data, stronger for names/strings and track lists | USER DECISION. Default: commit (as re3/reVC do) with provenance comment; alternative for the ones the game also loads from files at runtime (check each against the Steam data files and read at runtime instead). Never regenerate at build time from the exe (that is the dependency we are removing) |
| L3 | machine-lifted code (`*_lifted.inc` 230,020 B, `rwmath_exact.h` constants) | 252 KB, already in the tree | derived from RenderWare 3.6 object code (Criterion/EA licence) | outside this plan; flag for the same decision |
| L4 | `.notes/symbols.txt` (9,016 names+addresses) | | RE metadata, same status as upstream gta-reversed | no change |
No exe bytes other than L1/L2 values enter the tree; the 4.5 MB image and 9,795 pointer records are never committed.

## 6. Risks and open decisions
| # | risk | mitigation |
|---|---|---|
| R1 | 23 oracle tests share `StaticRef` globals with the original code through the VA | address mode (A2); never convert in test targets |
| R2 | static-init order for object globals (d) (the exe's order was the `_initterm` slot order) | prefer constant initialisation; C-slices check cross-object use (audit lists ctor-written-outside-object cases: CMenuManager 52 B in 25 ranges) |
| R3 | reads beyond the declared extent / unclaimed 474,579 nz B | A3 census + poison, final proof F3/G2 |
| R4 | types spelled differently at one address (12) and aggregate/field aliases (~25) | A4, decided once, in `aliases.json` |
| R5 | script handler table needs the 977 missing commands | D1 waits for S6 or keeps a named fatal |
| R6 | upstream DLL build must still compile | three-mode macro, AT(6) |
| R7 | CStore elements: vptr of 14 k elements must come from real ctors (an element ctor that does not match the exe changes behaviour silently) | verifier masks vptr only, checks every other byte |
| R8 | rand()-derived bytes (gHandShaker, FrontEndMenuManager) differ | game reseeds `srand(RsTimer())`; listed in `standalone_deviations.tsv` |

## 7. Portability cross-reference (stream D of `.notes/PORTABILITY_GOAL.md`; counts for streams A/P/F/W/R/C in `.notes/PORTABILITY_INVENTORY.md`)
| point | consequence for this plan |
|---|---|
| `NOTSA_GLOBAL` detached mode has no VA dependency, no 4-byte-pointer assumption | it is also the 64-bit-safe mode: pointer-valued initialisers are `&symbol`/literals, never 32-bit VAs (c-class: 20 globals, 14 game) |
| (d) objects built by real ctors | no `original_data` vptr masking problem on 64-bit; verifier (3.3) compares only 32-bit-layout builds: run it on the Win32 reference, never on arm64 |
| F3 (delete image loader/pad/`/BASE`/`/FIXED`) + A-1 | the only Win32/x86-only link-time machinery; after F3 the exe is a normal relocatable image |
| oracle tests and address mode (R1) | stay Win32 x86 only (reference); portable builds never define `NOTSA_ORACLE_ADDRESS_GLOBALS` |
| L2 tables (~20 KB) | committed as plain C++ literals, endian/pointer-size neutral |

## 8. Decisions (orchestrator, 2026-10-10)
- Legal L2+ tables (~20 KB game-content-like): committed as generated source, like re3/reVC did; the repo stays private (user's standing rule). Revisit only if the repo is ever published.
- `*_lifted.inc` (230 KB machine-lifted RW code) is out of scope of stream D; it goes to stream R/F (rewrite as readable C++ or replace by librw) before any portable target.
- Oracle tests keep address mode (Win32 x86 only), per 3.4.
