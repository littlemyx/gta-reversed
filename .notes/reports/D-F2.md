# D-F2: hooks out of the detached run build (Windows, commits 5e50f10e, b72bb2ae, a52f70d8)

## 1. Mechanism
`NOTSA_NO_HOOK_REGISTRY` (new `source/reversiblehooks/NoHookRegistry.h`, derived, never set by hand) = `NOTSA_STANDALONE_RUN && NOTSA_DETACHED_GLOBALS && !NOTSA_ORACLE_ADDRESS_GLOBALS`
(= the StandaloneDetached preset; the oracle test TUs add `NOTSA_ORACLE_ADDRESS_GLOBALS` and keep the full machinery; DLL / dump-only / non-detached run builds are untouched).
| piece | detached run build | everything else |
|---|---|---|
| `RH_Scoped*Install` (9.5 k sites incl. 6,455 `RH_ScopedInstall`), `RH_ScopedClass/VirtualClass/Namespace*`, `Category*` (ReversibleHooks.h) | `((void)0)` / `using HS = ...` only: no exe address, no `&fn`, no string, no RHManager item, no VMTInfo/`GetProcAddress` | as before |
| `Fixups::RegisterFunction/RegisterVMTSlot/RegisterUnverified/RegisterVMTClass` | **do not exist** (Fixups.h): any caller is a compile error | as before |
| Fixups.cpp maps (FnMap, SlotMap, VtableClasses, Unverified, NoCopySlots), whole-vtable copy (523 classes / 6192 slots), `standalone_unverified_hooks.txt` | compiled out; `FindKnown` returns nullptr; `ApplyToDataImage` only traps every listed image code pointer (V=7838 C=1957, was V=1842 C=1915 + vtable copy) | as before |
| `RHManager::InstallStatic/InstallVirtual/InstallVirtualDestructor` | registry branches off (never called) | as before |
| `NOTSA_EXPORT_VTABLE` (541 classes) | empty: nothing reads exported vtables any more (only `VMTInfo::FindByClassName` did); exe exports 9,308 -> 0 | `__declspec(dllexport)` |
| `NOTSA_INLINE_IN_CPP` (new, Base.h) | empty | `inline` (token for token what it was) |
| `Cheat.cpp` `cheats[]` (65 exe addresses) + the InjectHooks loop that patched `m_aCheatFunctions` by address | `#ifndef`d out (the table is initialised with our functions by NOTSA_GLOBAL_DEF) | as before |
| `IplDefPool.h` raw `rh->InstallStatic` | `#ifndef`d out | as before |

`InjectHooksMain()` still runs: its non-hook content is essential (script command handlers `RegisterHandlers`, logger, `CommandLine::PostHooksInjected`). With the macros gone it takes **1 ms (was 22-23 ms)**; process start -> boot-dump exit **~305 ms (was ~800 ms)**, 3 runs each.

Side finding (why NOTSA_INLINE_IN_CPP): dropping the class exports and the hooks' `&fn` exposed 12 members declared/defined `inline` in a .cpp but called from other TUs (CRect::{Restrict,Resize,IsPointInside x2,GetCenter,StretchToPoint,SetFromCenter}, CMatrixLink::{Insert,Remove}, CMatrixLinkList::{MoveToList1,MoveToList2,MoveToFreeList}): an ODR violation that only linked because the exported class / the hook took their address (LNK2019 on the first link, 9 symbols). Fixed with the macro, so DLL objects are unchanged.
Line-preservation note: `HookInstallOptions::InstallSrcLoc` embeds `__LINE__`/column, so guards in files with hook code (Cheat.cpp, IplDefPool.h) replace existing blank/comment lines instead of adding lines (found by the option-OFF identity compare: the first version shifted two objects).

## 2. What still depends on the registry, and why
Nothing compiled into the detached run build reads it. Residual users of the (now empty) classes, all harmless and either F3 or non-run:
* `RHManager` is still created in WinMain.cpp and its root category stays empty: `CommandLine::DumpHooks` (`--dump-hooks-to` writes `[]`), `UIRenderer::PreRenderUpdate -> CheckAll` (not reached: `DrawLoop` returns under NOTSA_STANDALONE_RUN), `HooksDebugModule` + `RListBuilder` (compiled for the DLL; shows an empty tree; the debug UI is off in the run build). Not on the data-image path -> left alone; they cost nothing.
* `VMTInfo.cpp`, `HookCategory*`, `NullHook` etc. stay linked through RHManager (a few KB); `VMTInfo::FindIndexOf` (reads an exe vtable) has no caller.
* The C-phase vptr proof stands: `verify_vptrs.py` 15,012 vptr dwords, 0 wrong (every object points at our own vtable), 24 ms_groups pointers ok. No runtime table was built from the registry besides the Fixups vtable copy (gone) and Cheat's address match (gone, dead since A2).

## 3. What F3 must delete (in addition to the plan's list)
* Fixups.cpp: `ApplyToDataImage` (+ `data_pointers.bin` reading, `standalone_unknown_pointers.txt` now 9,795 rows / ~215 KB, `.rdata` PAGE_READONLY, probes), `MakeStub/GetTrapStub/Resolve/FindKnown`, `NotsaStandaloneTrapHandler`, `RedirectVEH` (the AddVectoredExceptionHandler call in `InstallRedirectHandler`; keep its diagnostics half - atexit/purecall/invalid-param/terminate/abort/CRT report hooks/sampler - as `InstallDiagnostics`), `FixupStats`, `g_Stats`, the registry block (already behind `#ifndef NOTSA_NO_HOOK_REGISTRY`), `PluginBase.h` `NOTSA_PLUGIN_ADDR` and `Fixups.h` `ResolveCallTarget`.
* With the non-detached run config gone: the `NOTSA_STANDALONE_RUN` branches of `RH_ScopedVirtualClass` (ReversibleHooks.h), `RHManager.h` (2) / `RHManager.cpp::InstallVirtual`, `VMTInfo.cpp` (`GetVtableModule` exe handle, `FindByClassName` try/catch + `Fixups::Log`); then `NOTSA_NO_HOOK_REGISTRY` can become plain `NOTSA_STANDALONE_RUN`.
* tools: `soak.sh:149` copies `standalone_unknown_pointers.txt`; `extract_exe_data.py` outputs; `make_orig_pad.py`.
* `preprocessed_scan.py` on the result: remaining hits = flag/colour constants, verify-only `Image<>` blocks (PedModelInfo, VehicleModelInfo, Shopping), template definitions in PluginBase.h/Base.h, F3 machinery (DataImage.h ORIG_PAD_SIZE, Fixups.cpp probes/sampler ranges, Fixups.h ResolveCallTarget), camera_sync lifted constants. The 65 Cheat.cpp rows of the first scan are gone (a52f70d8).

## 4. Gates
| gate | result |
|---|---|
| Release + Detached build (build/StandaloneDetached) | OK (1121 TUs, 0 errors; the first link found the 9 ODR symbols, fixed) |
| option-OFF object identity (build/StandaloneRelease, 1119 objects vs `rel_b20b.json`, NOTSA_SIG_NORM_STRINGS=1) | 1117 identical; the 2 others are other agents' commit 0fa3ade8: `IplStore.cpp`, `Fixups.cpp` (poollog in LogGameStateTrace: dozens of new pool-iterator COMDATs). The first compare found 2 of mine (Cheat.cpp, InjectHooksMain.cpp via IplDefPool.h): `__LINE__`/column of `InstallSrcLoc`, fixed as described above |
| Debug DLL (build/Debug, full) | 1135 TUs compile, 0 errors; 3 environment failures independent of this change (WndProc.cpp, UIRenderer.cpp: `libs/imgui/bindings/imgui_impl_win32.h` missing; imgui_impl_sdl3.cpp: `SDL3/SDL.h` missing) |
| verify_globals.py (final build) | 2492 globals / 3,817,331 B, **0 unexplained**, table checks 4 of 4 |
| verify_vptrs.py | 15,012 vptr dwords, 27 classes, **0 wrong**, ms_groups 24 ok |
| codemod_globals.py --check-converted | 2719 addresses, **0 strays** |
| run_all_tests.sh (build/StandaloneRW, Debug, GPU lock) | **42 of 42 ok** (0 failures / 0 mismatches) |
| full-poison soak, 3 min (NOTSA_POISON_CONVERTED = all globals), final build | RESULT OK, errors 0, 22 markers, 10,300 frames, exit via atexit, 0 TRAP/FATAL lines (2 gate timeouts: the known `invehicle` route issue) |
| census 6 min, poison on | section 5 |
| exe size | 33,592,832 -> **25,925,120 B (-7.67 MB, -23 %)**: .text 29.19 -> 22.86 MB (incl. the 9.1 MB pad: real code ~20.1 -> ~13.7 MB), .rdata 4.01 -> 2.69 MB, exports 9,308 -> 0; map 435 -> 43 MB |
| startup | InjectHooksMain 22 -> 1 ms; process start -> boot dump ~800 -> ~305 ms |

## 5. Census / flakiness
| run | exe | result |
|---|---|---|
| census_p1 | first F2 build | soak half ended with an AV in `CEntity::~CEntity+0x17d` (walking `m_pReferences`, `*ref->ppEntity` dangling) during `CPools::ShutDown` after quit_ask -> no census file ("0 touches" there is NOT a pass) |
| census_base1 | pre-F2 baseline, same route | OK, 5 touches / 4 pages (the F1 UNCLAIMED sites) |
| census_p2 | first F2 build, again | OK, 1 touch UNCLAIMED (F1 site), STRAY 0, PENDING 0 |
| census_p3 | F2 + F1 | OK, **0 touches**, UNCLAIMED 0 / STRAY_CONVERTED 0 / PENDING 0 / VTABLE 0 |
| census_p4 | final build (a52f70d8) | OK, **0 touches**, UNCLAIMED 0 / STRAY_CONVERTED 0 / PENDING 0 / VTABLE 0, 9,700 frames, exit via atexit |

The one AV is a shutdown-order dangling-reference read in the game's own entity destructor (1 of 4 F2 census runs, 0 of 1 baseline; quit-time crashes are known flaky since D-B07V: 0xC0000374 / AV at quit). It does not involve vtables or the registry (a wrong vptr would trap with the exe address). Not reproduced in p2/p3/p4 or in the 3 soaks.
