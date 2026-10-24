# MAC-BUILD1: first full native build + link of PortableMac

Branch portable/main, commit 07b1800c (pushed to fork). Target `gta_reversed` (the exe), brew LLVM 23.1.2, `ninja -k 0 -j 12`. Scratch/logs: /private/tmp/claude-501/b1/.

## Result: linked arm64 executable build/PortableMac/bin/gta_reversed (42 MB), 1105 TUs + librw, no compile errors outside the winshim gap, 0 undefined/duplicate after fixes.

## Error tables
| stage | compile fails | root cause | count |
|---|---|---|---|
| build 1 | PCH (all TUs blocked) | `<intrin.h>` (clang's include_next on arm64) via Pool.h | 1 |
| build 2 (PCH ok) | 10 TUs | `mfapi.h` (AEUserRadioTrackManager.h) 6; `mmreg.h` (AEWaveDecoder.h) 2; `d3d9.h` (imgui_impl_dx9.cpp) 1; `ShObjIdl.h` 1 | 10 |
| build 3 | 0 (P4b's 9 rows already fixed in the working tree: all compiled) | | 0 |
Census-vs-build: the census used tools/standalone/winstub (empty stub headers incl. intrin/io/malloc/mfapi/mmreg/ShObjIdl); the real build only has source/portable/winshim, hence the gap. Template instantiation/codegen produced no new compile errors (warnings are -w'd by the preset).

| link (lld used for the full list; ld64 prints 20 max) | count | root cause |
|---|---|---|
| undefined `CFileLoader::LoadLine<FILE*>` | 1 (68+ refs) | `auto` template defined in FileLoader.cpp, instantiated only by RH_ScopedOverloadedInstall (empty in portable) |
| undefined C1 ctors of abstract classes | 7 | CEvent, CEventAcquaintancePed, CEventEditableResponse, CEventVehicleDamage, CTaskComplexPartner, CTaskComplexWander, CAEStreamingDecoder: hook helpers `this->T::T()` reference the complete-object ctor, clang emits only C2 for abstract classes (Itanium) |
| duplicate `CameraDestroy(RwCamera*)` | 1 | Game.cpp `static` after app.h's extern decl: MSVC internal, clang external |
| duplicate `ms_pQuadTree` | 1 | NOTSA_GLOBAL detached = external var; Itanium has no variable type in the mangling (MSVC does), ColStore.cpp vs IplStore.cpp |
| undefined from missing libs/frameworks/rw | 0 | SDL3, OpenAL, OpenGL, librw already linked (rw_stubs_gl3.cpp covers every rw symbol) |

## Changes (commit 07b1800c)
* winshim: intrin.h, io.h, malloc.h (placeholders), mmreg.h, mfapi.h, ShObjIdl.h (forward to the shim's mmsystem/mfidl/objidl).
* source/CMakeLists.txt: libs/imgui/bindings/imgui_impl_dx9.cpp in PORTABLE_EXCLUDED_FILES (needs d3d9.h; UIRenderer.cpp still names ImGui_ImplDX9_* but links, so P4b guarded it).
* source/portable/link_aliases.cpp (NEW): 7 `b C2` trampolines for the C1 symbols above. Not stubs: same code as the constructor. Needs `cmake --preset PortableMac` once (glob).
* FileLoader.cpp (explicit instantiation at EOF), Game.cpp (`#define CameraDestroy Game_CameraDestroy_static` before the static definition), IplStore.cpp (anonymous namespace around the global): all under `#ifdef NOTSA_PORTABLE`. Windows view proof: tools/portable/w0a_guardlines.py check ... TEXT OK x3 (only added guard-block lines; no wine cl run, nothing changes under MSVC).
* Honest stubs added: NONE (no new logged stub; existing rw_stubs_gl3.cpp / winshim ones suffice for the link).
* Excluded-TU game functions found undefined: none.

## Run (cwd = Steam GTA SA dir, hard 20 s kill, nothing written to the repo)
Gets through: InjectHooksMain, CdStream thread, WinPs init, SDL3 window + GL 4.1 Metal context (Apple M5 Max, S3TC, aniso 16), RwEngineStart (stub log: `RwShimPipelineEnsure (R-6) not implemented`), CGame::InitialiseRenderWare.
Crash: SIGSEGV fault addr 0x68 in fseek <- CFileMgr::Seek <- ValidateVersion (Game.cpp:127 UNREACHABLE "models\\coll\\peds.col file not found", NOTSA_UNREACHABLE does not stop, then Seek(nullptr)). The file exists as models/coll/peds.col: CFileMgr::OpenFile does not translate '\\' to '/' (only CdStreamOpen does, W-B9) => next slice W-7 (data root, path separators, case). lldb batch run hung at `run` (no backtrace from it; the in-process CrashHandler stack above is symbolicated).
Full stdout: /private/tmp/claude-501/b1/run1.out.

## Next
1. W-7: path normalisation in CFileMgr::OpenFile/FileMgr (backslash, case), then rerun to the next failure.
2. R-6 RwShimPipelineEnsure stub is hit before the first frame.
3. ld warning: duplicate libSDL3.a (harmless).
