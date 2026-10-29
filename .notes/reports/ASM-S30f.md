# ASM-S30f (Camera.cpp, 6 rows)

Result table: .notes/asm_sweep/S30f.result.tsv. 2 fixes (task 0, 2 commits), CamControl tail MATCH, 4 NOT_REACHED + TryToStartNewCamMode NOT_REACHED.

- Task 0a (f24fded9): CCam::Process (Cam.cpp:1345/1350/1351, exe 0x52744B-0x5275EA) `pow` bases are the float constants 0x8CC39C/0x8CC394/0x8CC398 loaded with `fld dword`; the port used decimal double literals (0.899999976, 0.600000024, 0.800000012), 1e-10 off. Now `std::pow(X87W((x87d)0.9f/0.6f/0.8f), X87W((x87d)ts))`. Cam.cpp.obj diff: only the 3 constants. The Process_Cam_TwoPlayer_InCarAndShooting pow bases (exe 0x8CC544, 0x862F90, 0x862F78) were already `(double)0.98f/0.85f/0.96f` = exact.
- Task 0b (5b0e3f17): CamControl `aimHeading` is now an x87d: `X87R(atan2 - HALF_PI)` / `(x87d)GetHeading() - HALF_PI` (portable rounds the fsub). Camera.cpp.obj before/after `dumpbin /disasm`: identical.
- CamControl 0x52A9E0-0x52B725 MATCH (full): decision cascade on gNewCamMode / active mode (all lists compared entry by entry), FOLLOWPED aim block (PI wraps NaN arms, 30 deg * 0.0174532924 test, standing flag), bombshop MRWHOOP, garage dot (x,z,y), 17.5 wait-for-interpolation test, scripted cam branch, copy-fixed-mode block, player visibility, RestoreWithJumpCut, replay/sound conditions.
- CamControl 0x528351-0x528A64 (vehicle garage fixed cam): structure and constants MATCH (center *0.5, 15.0 LOS window, -10.0 / +2.0). Open: ped garage fixed cam 0x5291EB-0x529B9A.
- Note: StartTransition is called with `push edx` where the upper half of edx can hold junk (e.g. front.y bits after 0x52AD98); StartTransition reads the argument as a word (not verified further).
- Not reached (budget): SetRwCamera, Constructor, CopyCameraMatrixToRWCam, DealWithMirrorBeforeConstructRenderList, TryToStartNewCamMode tail.
