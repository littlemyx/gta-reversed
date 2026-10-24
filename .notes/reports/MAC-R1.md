# MAC-R1: slice R-1, fakerw under GL3 (2026-10-23, branch portable/main)

Commit: f3ff9de8 (fakerw.h, rwextra.h, game_sa/RenderWare/rw/skeleton.h, standalone/rw/hanim.cpp). No librw change (no submodule bump).

## Census (PortableMac native, snapshot of HEAD, unique diagnostics)
| item | before | after |
|---|---|---|
| total | 1992 | 1752 |
| group A (fakerw.h, rwextra.h, skeleton.h) | 15 | 0 |
| group B cascade in game render code (Clouds 44, Boat 49, WaterLevel, Shadows, ... 26 files) | 223 | 0 (what remains there is P: pointer casts, operator new, min, PolyInfo bitfield) |
| R-owned zones (standalone/rw, Pipelines, RenderWare/D3D*, fakerw/skeleton, Gamma, Radar) | 1286 | 1269 |
| R incl. cascade | 1509 | 1269 |
PCH closure: 0 fakerw/skeleton errors. R is 4 above the plan's "<= 1,265": +2 in D3DResourceSystem.cpp (GetD3D9Device() is now void*, same lines as before, file leaves in R-2) and the plan's baseline of 1498 used a slightly different filter. Tool: scratchpad r1/snap_census.sh (snapshot = HEAD + own files, because the shared checkout changes under a running census; the PCH of an in-tree run was invalidated once by P-2e editing Pickup.h). TSV: .notes/w0/r1_after.tsv.

## Changes (all `#ifndef NOTSA_PORTABLE` on own lines)
| where | change |
|---|---|
| fakerw.h | portable `RxObjSpace3DVertex`: game-owned struct, same 36-byte layout + setters/getters as rw::d3d::Im3DVertex, ARGB uint32 colour (R-4 converts to rw::gl3 RGBA bytes); `RwFixed` = int (long is 8 B); ILP32 assert split (scalars kept, void*==4 dropped); RwEngineOpenParams==rw::EngineOpenParams assert dropped (R-3a builds the params); RwLLLink/RwLinkList/RwObject asserts restated with sizeof(void*) |
| rwextra.h | `GetD3D9Device()` returns nullptr (void*); RwFileFunctions assert 11 pointers |
| skeleton.h | psGlobalType DirectInput slots are void* (WinPs.cpp still zeroes them; WinInput.cpp is !SDL3) |
| hanim.cpp | portable assert; `kFileKeyFrameSize = 0x24` for the .anm stream (read divisor, GetSize; guarded Write) instead of sizeof(HAnimKeyFrame) which is 40 on LP64 (real LP64 bug found in passing) |
Plan correction: `rw::d3d9::InstanceDataHeader/InstanceData/VertexStream` DO exist under RW_GL3 (rwd3d9.h is unguarded plain data), so RxD3D9ResEntryHeader/InstanceData/VertexStream stay aliases (no portable structs needed). A mirror version was tried and reverted (it added 6 diagnostics in files R-2 excludes).

## Win32 no-op proof (sub-agent, details /private/tmp/claude-501/r1_ab/REPORT.md)
A = f3ff9de8~1 vs B = f3ff9de8, StandaloneRelease: 1284/1284 TUs, 4670/4670 compile commands identical, build.ninja identical (paths normalised). wine cl.exe, 10 TUs (hanim, engine, pipeline, camera, Clouds, Boat, WaterLevel, WinPs, WinMain, RpAnimBlend): code + relocs + all non-debug sections + symbols + section order identical, 0 `__LINE__` immediates, 0 OTHER. Residue: .debug$T/.debug$S line numbers/checksums; hanim +12 B .debug$T (const unsigned int for the constexpr). Preprocessed text identical in 9/10; hanim differs by exactly the intended sizeof -> constant edits.

## Stubs / open items
* No stubs added. GetD3D9Device() returns nullptr: callers are WindowedMode (DLL), D3DResourceSystem (excluded R-2), UIRenderer ImGui DX9 (R §7).
* LP64 animation layout (P/anim, not R): HAnimKeyFrame 40 B vs HAnimInterpFrame 48 B (q/t offsets differ); RpHAnimKeyFrameBlend/Add/MulRecip treat inputs as StdFrame=InterpFrame. Verify at the first portable run that the game passes only interp frames to them; RpAnimBlend.cpp:84 passes sizeof(RpHAnimKeyFrame) as animKeyFrameSize (fine, in-memory).
* Remaining A-group-like items for R-2/R-3: none in fakerw.
