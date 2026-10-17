# BUG-QUIT (Windows host, 2026-10-17)

## Result
| bug | status |
|---|---|
| 1 quit heap corruption (0xC0000374, rc 127) | FIXED: 4 independent shutdown bugs (all use-after-free / bad free, all hidden by Wine's tolerant heap) |
| 2 gta_sa.set crash in ResetDeviceNow -> destroyVertexBuffer | NOT reproducible any more on the tip (2+2 runs with a game-written set, DisplayVideoMode=4 = 800x600 exclusive); hardened the one defect that matches the signature (librw uninitialised out-pointer), root cause of the trigger UNPROVEN (see below) |
| AV in CCollisionData::GetShadTrianglePoint | not seen in ~15 runs, not investigated |

## Tooling used
WinDbg (winget `Microsoft.WinDbg`, x86 cdb copied to `C:\gta-scratch\dbg`), debugger heap (cdb's default debug heap: 0xFEEEFEEE fill makes a UAF fault at the first read of freed memory, instead of the late fast fail). gflags/page heap was NOT needed and never enabled (nothing to disable). Repro harness in `C:\gta-scratch\bugquit` (not in the repo): `run.sh <builddir> <name> plain|cdb` (env `ROUTE`, `SETFILE`, `CDBSCRIPT`, `LIMIT`; takes the GPU lock), `route_q3.txt` (soak route up to control + quit: ~200 s), `route_menu.txt` (main menu only), `cdb_default.txt` (`sxd -c2 "kb 60;q" av`, `sxe -c "k 40" c0000374`). Own data dir `C:\gta-quit` (hard links to the Steam install).

## Root causes (bug 1) - found one after the other, each run under cdb stopped at the next one
Reproduce: `ROUTE=route_q3.txt ./run.sh build/StandaloneRelease x cdb`, window: quit via ESC > QUIT GAME > YES. Before the fixes: rc 127, no `atexit handler ran`.
1. `C3dMarkers::Shutdown` (a67ee66f): AV in `RpClumpForAllAtomics` (eax=feeefeee) <- `C3dMarkers::Shutdown+0x4a` <- `CSpecialFX::Shutdown` <- `CGame::Shutdown+0x8c`. `m_pRpClumpArray[ARROW/CONE_NO_COLLISION/CONE]` share ONE clump (Init); the port cleared only the current slot (`std::exchange(v, nullptr)`), so the clump was destroyed 3 times. Exe 0x722763..0x7227A4: destroys each clump, then nulls EVERY slot holding it. Fixed verbatim.
2. `FxMemoryPool_c::Exit` (017e64cc): fast fail 0xC0000374 in `RtlFreeHeap` <- `_free_base` <- `FxMemoryPool_c::Exit+6` <- `CGame::Shutdown+0x101` (`FxManager_c::Exit`). Port: `CMemoryMgr::Free(this)` - `this` is the `m_Pool` member of the g_fx global, not a heap block. Exe 0x4A9C80: `mov eax,[ecx]; test; je; push eax; call Free` = free(m_pData).
3. `CCarFXRenderer::Shutdown` (f250b599): AV in `rw::Texture::destroy` (LLLink::remove, ecx=feeefeee) <- `RwTextureDestroy` <- `CCarFXRenderer::Shutdown+0x17` <- `CGame::Shutdown+0x1ec`. CGame::Shutdown calls it twice (exe 0x53CA61 and 0x53CA75); exe 0x5D5AD0 skips null entries and clears each destroyed one; the port destroyed the textures twice.
4. `CameraDestroy` in Game.cpp (1fdd82d6; the static copy that CGame::ShutdownRenderWare uses; app_camera.cpp's copy was already right): AV in `RwRasterDestroy(0xFEEEFEEE)` <- `CameraDestroy` lambda <- `CGame::ShutdownRenderWare+0x94` <- `RwTerminate` <- `NOTSA_WinMain+0x3f7`. The port read `RwRasterGetParent(buffer)` AFTER `RwRasterDestroy(buffer)`; exe 0x72FDBA reads the parent (`mov edi,[esi]`) before the destroy call.
No try/catch, no skipped frees: every fix is the exe's logic (asm addresses above).

## Test runs (build/StandaloneRelease, tip 1fdd82d6, route_q3 = full route to gameplay + menu quit)
| run | build | setfile | result |
|---|---|---|---|
| q1 | before fixes | none | rc 127, no atexit (repro) |
| n1,n2,n3 | 1fdd82d6 | none | rc 0, atexit line, no Exception, gta_sa.set written (2900 B) |
| a1,a2 | 1fdd82d6 | game-written set (DisplayVideoMode=4) | boots to gameplay (`[until] control ok`), quit rc 0 + atexit |
| b1,b2 | + librw f358ffd | same set | same, rc 0 + atexit |
| s1..s8 (cdb, menu route) | | game-written set | no crash; HIT log proves `LoadSettings` then `CMenuManager::Initialise` -> `SetVideoMode(4)` (from CheckForMenuClosing) really ran |
Note: the in-game quit needs ~200 s (startup 95 s on a cold host: the injector's first `wait` must exceed the data load, the old 20 s menu wait misfires then).

## Bug 2 analysis
Signature: `releaseVideoMemory -> releaseDynamicVBs -> destroyVertexBuffer(*dvb->buf)` with `*buf == 1`. librw `createVertexBuffer`/`createIndexBuffer` declare the out pointer uninitialised and ignore the HRESULT: when `CreateVertexBuffer` fails (device lost / not reset - e.g. Reset to the exclusive 800x600 mode failing while another process held the GPU/foreground, which D-B07V's concurrent device tests and soaks could do) the native d3d9.dll leaves it untouched, so `recreateDynamicVBs` stored stack garbage into im2dvertbuf/im3dvertbuf and the NEXT release called Release() on it. Wine's d3d9 writes NULL. Fix (librw f358ffd on branch notsa, pointer 5db3f021): init to nil + nil on FAILED(hr). This is robustness for the failure path, the trigger itself (failing Reset) could not be reproduced in isolation (not forced); with the GPU lock (no concurrent device users) the original crash does not occur. If it reappears: look at the HRESULT of `Device::Reset` in `showRaster` (librw ignores it) and at `TestCooperativeLevel`.
