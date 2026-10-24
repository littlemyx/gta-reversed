# MAC-BISECT1: R9-1 (SIGBUS in CRenderer::ScanSectorList) - root cause and fix

## Result
Not a commit regression and not RR3's edits: a latent LP64 layout bug. `CRenderer::GetLodRenderListBase()` returned `&PC_Scratch[24]`; in the exe (4-byte pointers) that is right behind the
5-pointer `tScanLists` (0x14 bytes) which lives at `PC_Scratch+0`. With 8-byte pointers `tScanLists` is 40 bytes, so the first `AddToLodRenderList` entry (16-byte `tRenderListEntry`)
overwrote `scanLists->pedsList` (+24) and half of `dummiesList` (+32) in the middle of `ScanSectorList`. The loop then walked a "list" that was a CBuilding (list head = the
CBuilding vptr, items = vtable entries = code addresses, SIGBUS on `entity->SetCurrentScanCode()`; seen as `[bisect1] BAD entity ... listType=CPtrListDoubleLink<CPed*> head=<CBuilding vtable>`).
Only a LOD parent with >1 children added mid-scan triggers it (SetupEntityVisibility -> AddToLodRenderList), which is why it is intermittent (~40-60 % of 60 s drive runs) and appears only while driving
through new areas (the route also needs `cmd:roadveh`, new in PLAY1 4f7c0550 - older builds ignore it, so the player never drives).

## Fix (source/game_sa/Renderer.cpp)
`LOD_RENDER_LIST_OFFSET = max(24, sizeof(tScanLists))` used by GetLodRenderListBase, static_asserts (no overlap, alignment, below the don't-render list at 0x2000). MSVC x86: sizeof = 0x14 -> 24, unchanged.
Wine cl.exe A/B of Renderer.cpp.obj (StandaloneReleaseOFF flags, no PCH): code + relocations of all 1111 code sections identical (obj_code_sig). Consumer audit: tScanLists is used only in Renderer.cpp
(SetupScanLists, 2 VisitLists loops); the LOD list is only read through GetLodRenderListBase/ms_pLodRenderList; EntryExitManager's PC_Scratch[256] user is not live at the same time (as in the exe).
Capacity side effect: LP64 entries are 16 bytes, 509 entries below the don't-render list (exe: 1021); a frame has a handful of LOD parents.

## Step 1: clean HEAD (615be0f0, committed tree, own worktree, route lod_drive.txt = r9-like: new game, tp, roadveh, drive 25 s)
Clean HEAD crashes (SIGBUS ScanSectorList +2360, same offset as R9-1): r9.txt 4 of 5 runs, lod_drive 1 of 3. So RR3's uncommitted edits (now ce366bcf) are not the cause; the PortableMacP1 exe crashes the same
way (3/3 on r9, 1/3 on lod_drive). R9-1's "P1 runs the route fine" was a lucky run.

## Step 2: bisect (oracle: detector `[bisect1] LODOVERLAP` = a LOD list entry written into PC_Scratch[0..40); each event was followed by the crash in 4 of 6 runs)
| commit | runs | LODOVERLAP / crash |
|---|---|---|
| e8d04fac (Windows merge), fc7de681 | 1 each | boot crash at PROLOG1 (NVCPipelineProcess; GL3 stock pipelines arrive in 6d26c5e5): not bootable |
| 6d26c5e5 (R-6) lod_drive | 8 | 0 (route cannot drive: no cmd:roadveh yet) |
| 1eb6b3cb (RR-1) lod_drive | 8 | 0 (same reason) |
| 6d26c5e5 / 1eb6b3cb with lod_tp.txt (tp through 12 spots, no car) | 1 each | event 3x in both |
| 4f7c0550 (PLAY1) lod_drive | 2 | event in run 2 |
| e78a49ec (RR-2) lod_drive | 1 | event + SIGBUS |
| 3d315dab HEAD + fix | r9 x3 + lod_drive x6 | no crash |
The first rows are a route artefact, not a culprit: the bug exists in the earliest bootable commit (6d26c5e5); lod_tp.txt (needs no car) shows it there. ce366bcf (RR3): clean 615be0f0 without it crashes, HEAD with it + fix does not.

## Acceptance
Fixed HEAD (3d315dab + fix): r9.txt x3 (47 s each, exit 0, mark r9_dawn_stopped reached), lod_drive.txt x6 (57-69 s) - no crash; one further lod_drive run was killed by the hang watchdog inside the Metal swap in FrontendIdle (GPU stall while wine builds ran; environment, not game logic). 9 clean runs of the fixed build vs 4/5 + 2/6 crashes unfixed. Same build unfixed crashes in about half of such runs.
Routes added: tools/portable/play1/lod_drive.txt, lod_tp.txt. Regression guard: the static_asserts (compile-time; fires on any layout where the LOD list would overlap the scan lists).
Other finding (not fixed): FxManager.cpp:215 "Couldn't find Fx system bp 'ects' / 'arette_smoke' / 't2'" - effect names arrive without their first characters (cigarette_smoke -> arette_smoke), in good and bad runs.
