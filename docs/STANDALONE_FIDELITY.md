# Standalone build: deliberate deviations from the original exe

Collected from `.notes/PHASE2_STATUS.md` (row in brackets). "Unobservable" = no effect on game behaviour that we could find.

## Game logic
* Where the exe reads an uninitialised stack value, we return a defined one (0/false): `CCollision::ProcessSphereTriangle` with `numPassed==0`, `ProcessLineBox` with `t0>1` [P2E, S2 CCollision].
* `FIX_BUGS` is off in the run build; remaining `IsFixBugs()` guards (e.g. null guards when a pool is full, `CTaskSimpleBeHitWhileMoving::StartAnim` null anim) are inactive, so the exe's behaviour is kept [D7/D8, S1 phase 1e].
* Run-build guards where the exe relies on UB: `CAutomobile::ProcessEntityCollision` null colmodel, `CCover::Update` stale buildings, CRT "array subscript out of range" report tolerated (the exe reads the neighbour) [S5-librw update 3].
* `CWorld::FindPlayerSlotWithVehiclePointer` 0x564000 returns 0 in the exe, port returns -1; `GangLeader::ControlSubTask` keeps nullptr where the exe would `_purecall` [S1 D10-D12, S2 hooks].
* `NOTSA_DEBUG` not defined: no siren cheat Ctrl+J/K, no SoundManager stack-trace module, no Replay iterator asserts; UIRenderer (ImGui, F7, debug hotkeys, `RHManager::CheckAll`) is stubbed [D7/D8].
* `NDEBUG`: the original has no asserts (e.g. `CRect` flip in `DoFade`) [S5 2026-10-10].
* Debug-only extras kept as is, inert for valid data: `_DEBUG` bounds check in `Population.cpp`, `Pool.h` `NOTSA_DEBUG_BREAK`, memory logging [D7/D8].
* `Population.h` fields behind commands 2435/2462 look swapped (byte semantics kept) [S6-I]; open TODO: 2463 `-1` path allocates with plain `new` instead of the task pool [S6-I].
* Script commands: `GET_HASH_KEY` in the exe reads an uninitialised stack label (oracle zeroes the stack) [D-REL].

## RenderWare shim (librw-based)
* **MANAGED vertex buffers** instead of the exe's shared pools (behaviour-neutral); no morph-target interpolation, no LRU instance arena, no NATIVE geometry [P2B-07].
* **`renderFrame` is never incremented** (as in the exe: `RpWorldRender` has no callers); the car env-map once-per-frame guard therefore never fires [fidelity leftovers].
* **PAL8 textures are expanded at load** to 32 bit (only `outro.txd` has PAL8); `RwRasterLockPalette` returns NULL for them; `RwRasterCreate` with P8 on an open device returns NULL [P2B-03a/b].
* **Bump map not ported** (one material in `gta3.img`, `tikitorch01_lvs`); the D3D9 light-setup/pipeline table-sqrt sites at 0x814F0D.., 0x755193, 0x76500B are bump/tangent code, unused [MatFX pipeline, fix4].
* **Light slot order** differs from the exe's; invisible after 8-bit quantisation, not implemented [fix4].
* **Sub-hierarchies** and UV key-frame Blend/Add/MulRecip are not implemented (the game never calls them) [P2B-05b]. `RpLightGet/SetConeAngle` are ported but unused by the game [fix4].
* `D3DCREATE_PUREDEVICE` is not added (librw does not query caps first); `D3DCREATE_MULTITHREADED` is correctly not set (the exe never writes it) [librw fork patches, D7].
* `_rwD3D9EnableClippingIfNeeded` is the exe's verbatim loop (NaN counts as inside, negative radius differs from librw) [librw fork patches].
* `_rwStreamInitialize` ignores the caller-supplied storage (heap stream); `RwFree` is a thin wrapper; `RwStreamClose` on stale streams is an open issue [P2B-01, S5].
* `SetMaterial` cache invalidation differs from the exe (deliberate); `ALPHATESTENABLE` tie to blending is unverified [P2B-02c].
* librw fork patches change behaviour to match SA (anisotropic filtering, `sortLikeSA`, case-sensitive UV-anim lookup, matrix kernel order); a texture dictionary iterates newest first as in the exe [P2B-03b, fork].
* Textures: the exe-checked pixel/format decisions (BMP reader is our own because librw pads rows wrongly; camera-texture lock is own code) [P2B-03a].
* Skin pipeline: until the vs_1_1 / CPU path lands, librw's skinning is used instead of the exe's shader/CPU skinning [11x-4, S3 skin row].
* MatFX: stages >= 1 disabled explicitly, pixel shader cleared after the draw, PS constant written after `flushCache` (librw clobbers c0) [MatFX AllInOne].
* Im2D/Im3D: the exe's FF driver is ported, the textured stage-0 branch is untested [P2B-06].
* RW maths: remaining small `RwSqrt` table gaps in ConvertFromMatrix/SetupSlerpCache/Blend/Rotate and `BoneNode_c::BlendKeyframe` early-outs (NaN only) [11x-4].
* Wine only: restore callback via a device-vtable `Reset` proxy, real `D3DERR_DEVICELOST` path untested; DEC3N vertex path untested [P2B-09, P2B-07].

## Platform
* No DirectShow: videos are skipped (`NOTSA_STANDALONE_SKIP_VIDEOS`); no 800x600 mode on Wine, first mode with width >= 800 is used [S5-librw].
* `NOTSA_STANDALONE_MUTE=1` (env, off by default): DirectSound secondary buffers get a patched vtable whose `SetVolume` only ever sets `DSBVOLUME_MIN` on the real buffer while `GetVolume` returns the volume the game set (audio logic unchanged, output silent); no effect without the env var [MUTE].
* `TRACY_ENABLE` undefined in the run build (events were queued with no server -> OOM) [S5].
* The pad/code range is `PAGE_NOACCESS`; unported code pointers are trap stubs that log and terminate [P2A].
* Hooks not ported or unverifiable: `CTaskSimpleChat` dtor 0x5F7FD0 is the base dtor jump; residual float oracle diffs: NaN payload/sNaN quieting only, `acos(-1)` is environment dependent [P2E, S2].
* Open oracle mismatches: `CCurves::CalcCurvePoint` (97%), `CWeapon::FireM16_1stPerson` `_ftol` + double 0.0125; `cHandlingDataMgr`/`ConvertDataToGameUnits` unproven (runtime-initialised constants) [S2 reciprocal audit].
