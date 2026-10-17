# D-F1 report: slice F1 (raw casts / plugin::Call*), Windows reference host, 2026-10-17

Start: HEAD bca00b77 (+ D-F2 working tree), census UNCLAIMED 3 pages (4 touches), verify_globals 2485 / 0 unexplained.
Result: **census of the full 6-min route: 0 touches, UNCLAIMED 0, STRAY_CONVERTED 0, PENDING 0**; verify_globals 2492 globals / 0 unexplained; `plugin::Call*` and `StaticRef<T>` are compile errors in the detached run build.
Commits: 4e6623a5 (1/4 plugin::Call guard + MemoryMgr), 616d8c85 (2/4 raw casts), 22ebb679 (3/4 two reads the source grep could not see), ae059bad (4/4 StaticRef guard) + this report / tool.

## 1. Converted

| site | address | form |
|---|---|---|
| `AEWMADecoder.cpp:8` `CAEWMADecoder::WMCreateSyncReader` | 0xB6BAD8 | detached: `NOTSA_GLOBAL_SYNTH` (function-pointer typed, 0 in the image, set by `InitLibrary` with GetProcAddress) + the static member reference bound to it; address mode keeps the raw cast (`#else`) |
| `CustomBuildingDNPipeline.cpp:187/189` | 0xC02C20 | `NOTSA_GLOBAL_SYNTH(k_C02C20, int32, {})` + `NOTSA_GLOBAL_EXPR` (read only, 0) |
| `Automobile.cpp:4107` `renderLicensePlateTexture` | 0xC1BFD8 | detached: SYNTH `RwTexture*`; address mode keeps the raw cast |
| `Pickups.h:31` `CPickups::StaticCamCoors` | 0x97D660 | **found by the sweep**: a `static inline CVector* x = (CVector*)0x97D660` pointer variable initialised to the *address of the exe cell* (never read, written by Pickup.cpp). Detached: `NOTSA_GLOBAL(.., (CVector*), {})`, address mode untouched |
| `Shopping.h:136` `gpCurrentProperty` | 0xA97294 | same pattern (write only: `CShopping::SetCurrentProperty`); detached `NOTSA_GLOBAL_HDR` |
| `Group18.cpp:651` TASK_FLEE_CHAR_ANY_MEANS `StaticRef<int32>(0x86F678)` | 0x86F678 | **found by the preprocessed scan**: live `StaticRef` read of an .rdata constant (1000), never reached by the census route. `NOTSA_GLOBAL_SYNTH(.., {1000})` + `NOTSA_GLOBAL_EXPR` |
| `TaskSimpleGunControl.h:28` `DEFAULT_GUN_ATTACK_PERIOD` | 0x8D2E78 | **found by the scan**: address written as the DECIMAL `StaticRef<uint32>(9252472)` (invisible to every hex grep and to the codemod; it was an `aliases.json` `new_globals` row since A4 but never converted). `NOTSA_GLOBAL(.., (uint32), { 2000 })` |

5 new `aliases.json` `new_globals` rows (0xB6BAD8, 0xC02C20, 0xC1BFD8, 0x97D660, 0xA97294); the other two addresses already had rows. Every edit that changes the line count is followed by `#line N`.

## 2. plugin::Call* / StaticRef guards

* `PluginBase.h`: under `NOTSA_GLOBALS_DETACHED` (detached run build; not the DLL build, not the address-mode oracle tests, not option OFF) `Call`, `CallAndReturn`, `CallMethod`, `CallMethodAndReturn` carry `static_assert(kCallByAddressForbidden<address>)`: any *instantiation* is an error (virtual-slot calls are not by address and stay). The only 2 instances in the link (`MemoryMgr.cpp` Calloc 0x72F460 / MallocAlign 0x72F4C0, dead code behind an unconditional `return`) are `#ifndef NOTSA_GLOBALS_DETACHED`-ed out.
* `Base.h` (beyond the plan, same mechanism): `StaticRef<T>()` has `static_assert(sizeof(T)==0)` when `NOTSA_DETACHED_GLOBALS && !NOTSA_ORACLE_ADDRESS_GLOBALS`. The whole run build (1121 steps) compiles, i.e. **no StaticRef/ScopedStaticRef is instantiated any more**, and no new one can be added.
* Mutation tests: re-enabling MemoryMgr's `CallAndReturn<..0x72F460..>` -> `C2338 plugin::Call*<0xADDR>: call by address ... not allowed` + instantiation note; re-enabling `3dMarkers.cpp`'s `StaticRef<RxObjSpace3DVertex*>(0xC278F8)` -> `C2338 StaticRef<T>(0xADDR) reads ... not allowed`. Both restored.

## 3. Sweep of the run build (what a grep cannot decide)

`tools/standalone/preprocessed_scan.py` (new): runs `cl /E` with the TU's own flags over all 1119 PCH TUs of the Detached build, keeps what comes from `source/` or `vendor/` and is ACTIVE after `#if` / macro expansion: `StaticRef`, `plugin::Call*`, `CallMethod`, `CallAndReturn`, `ResolveCallTarget`, any hex literal in 0x401000..0xCB0000 (`MODE=dec`: decimal). First run (before 1/4..3/4 fixes): 108 distinct lines; after: all remaining are listed here.

| remaining active hit | verdict |
|---|---|
| flag masks / colours: Physical.h `0x800000`, eVehicleHandling*Flags.h, Automobile.cpp:5758, Group18.cpp:354, VehicleModelInfoDescs.inc (0x404052 ...), TwoDEffectsDebugModule.cpp `0x006400ff` | constants, not addresses |
| `Cheat.cpp` table (65 lines, `{ 0x4385b0, CCheat::WeaponCheat1, ... }`) | install addresses of the hook loop (D-F2: `RH_ScopedNamedGlobalInstall`), compared as integers, never dereferenced |
| `PluginBase.h` template bodies, `Base.h` `ScopedStaticRef` body, `Fixups.h:95` `ResolveCallTarget` | definitions only; no instance (proved by the static_asserts) |
| `camera_sync.h` / `camera_sync_lifted.inc` `MEMABS(0xC97B24/0xC979BC)` | emulated memory model of the lifted RwMatrixOptimize (answers with the address of `MatrixTolerances()`), reads no exe memory |
| `AnimAssocDefinitions.cpp`, `PedModelInfo.cpp`, `VehicleModelInfo.cpp`, `Shopping.cpp` `Image<T>(0x...)` | `NOTSA_TABLE_CHECKS` verifier blocks, compiled only with `NOTSA_VERIFY_GLOBALS`, read `original_data.bin` from disk (G1 decides) |
| `GlobalsVerify.cpp` `kBase 0x858000`, `DataImage.h` `ORIG_PAD_SIZE`, `Fixups.cpp` probes (0x860E2C / 0x8A2A18 / 0x85DA64 reads, 0x401000..0x3400000 sample filter) | the image machinery itself: F3 |
| MODE=dec | after the 0x8D2E78 fix only `Stats.cpp` (1000000 / 10000000 money limits) and `TaskInteriorUseInfo.cpp` (9999999 ms) |

Source-level only (not compiled, proved by the scan): `LinkList.h` raw calls (`#else` of NOTSA_STANDALONE), `AEStreamingDecoder.cpp` operator delete (`USE_DEFAULT_FUNCTIONS`, defined nowhere), `RenderWare.h` / `rwcore.h` / `rpmatfx.h` / `WindowedMode.cpp` StaticRef rows (alt-config RenderWare, `!NOTSA_RW_LIBRW`), Collision.cpp `plugin::Call` (TEST_COLLISION_FUNCS), Radar.cpp absolute-address asm (`#else` of NOTSA_GLOBALS_DETACHED), preset_view.cpp, TimeCycle/Streaming/PostEffects/Stats `#else` branches. Module-relative `GetModuleHandle(nullptr)` uses (app_debug.cpp stack dump, AESound.cpp `IsInExeImage`, ImageDiag.cpp, VMTInfo.cpp) read the loaded module, not a fixed address.

## 4. Gates

| gate | result | numbers |
|---|---|---|
| `codemod_globals.py --check-converted --boot-dump` | PASS | 2719 converted addresses (was 2712; +7), **0 strays** |
| `verify_globals.py` | PASS | **2492 globals** / 3,817,331 B: 2456 identical, 65 code-ptr, 15,041 masked rdata-ptr dwords, 2156 deviation bytes, **0 unexplained**; table checks 4/4. (An intermediate run while D-F2's hook registry was half-removed showed `m_aCheatFunctions` 191 B differing: the verifier explains the 65 code-pointer dwords through the registry; gone again with D-F2's final commits) |
| `verify_vptrs.py` | PASS | 15,012 vptr dwords, 27 classes, 0 wrong; 24 ms_groups pointers ok (needs `gta_reversed.map` next to the exe) |
| option-OFF object identity (StandaloneRelease, 1120 objects vs D-B20's `rel_b20b.json`, `NOTSA_SIG_NORM_STRINGS=1`) | PASS | 1118 identical; the 2 others are not mine: `IplStore.cpp` (0fa3ade8, FLAKES: CIplStore::RemoveIpl/IncludeEntity fix) and `Fixups.cpp` (D-F2 5e50f10e) |
| Debug DLL (build/Debug, address mode) | PASS | PCH + 10 TUs (MemoryMgr, AEWMADecoder, CustomBuildingDNPipeline, Automobile, Group18, Pickups, Pickup, Shopping, TaskSimpleGunControl, TaskComplexFleeAnyMeans), 0 errors |
| oracle tests (Detached config, `NOTSA_ORACLE_ADDRESS_GLOBALS`) | PASS | `world_oracle_test`, `audio_fade_oracle_test`, `logic_oracle_test` compile + link (the guards are off for them) |
| full-poison soak, final exe (ae059bad + 06cbb533), 3 min | PASS | `RESULT: OK`, 9,800 frames, errors 0, 0 gate timeouts, peak committed 524 MB (the earlier exe: 10,500 frames, 1 known `invehicle` timeout) |
| census 6 min, `NOTSA_POISON_CONVERTED` = all 2492 globals | PASS | **0 touches, 0/1112 pages: UNCLAIMED 0 (was 3), STRAY_CONVERTED 0, PENDING 0, VTABLE 0** |

## 5. Notes
* The soak/census scripts of D-B20 hard-code `b20` output dirs: copies in `C:/gta-scratch/f1/{dump,soak,census,chain}.sh`, results in `C:/gta-soak-out/f1` (dumps, verify/vptr/stray text, soak_a/b consoles, census_a/b). `census_a` ran on the intermediate exe.
* Lessons: (1) a hex-only grep misses decimal addresses (`StaticRef<uint32>(9252472)`): scan the *preprocessed* build with both radixes. (2) `ninja -t commands` + `cl /E` is ~1 s per TU, the whole run build in ~8 min at 2 jobs. (3) `static_assert` on an uninstantiated template parameter is the cheapest "proved dead" gate: it needs no reachability analysis.
* Left for F3: the image machinery hits of section 3 (Fixups probes, ORIG_PAD_SIZE, GlobalsVerify kBase) and `Fixups::Resolve` / `ResolveCallTarget` (no caller can instantiate it any more).
* Accident to know about: I killed all `cl.exe` once (to stop my own scan) - if another agent's compile died around 17:20 that was me; `gta_reversed.pdb` of the Detached build was removed at 17:14 (relinked on the next build, LNK1201 workaround).
