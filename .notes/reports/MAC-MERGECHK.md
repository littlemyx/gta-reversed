# MAC-MERGECHK (2026-10-25): portable build after the merge of reverse/interior_c (c9dcae43)
- Base HEAD c9dcae43 (58 interior_c commits merged, 26 source files changed).
- PortableMac ninja: FAILED on the merge as landed (4 errors in General.cpp), green after the fix below (717/717, link ok).
- ctest -L portable: 19/19 passed. ctest -L mutation: 12/12 passed.
## Breakage fixed (portable only)
- Windows REPLAY-DIFF harness (source/standalone/Lockstep.*, GameRand.h hooks g_RandHook/g_RandCaller) is Win32-exe-only: GameRand.h
  has no hooks under NOTSA_PORTABLE (msvc_rand.h) and Lockstep.cpp uses GetModuleHandleA/_ReturnAddress. General.cpp
  (GetRandomNumber, NOTSA_RD_OUTER) failed to compile.
  Fix: `#if defined(NOTSA_STANDALONE_RUN) && !defined(NOTSA_PORTABLE)` in General.cpp (2 sites) and Lockstep.cpp; Lockstep.h gets
  inline no-op stubs under NOTSA_PORTABLE (Timer.cpp / AudioEngine.cpp / app_game.cpp call FrameRead/Tick/NoAudio/Note/PinTimer/TimerFn).
  Win32: wine cl.exe code+relocs A/B (ftol_ab.py, base HEAD) IDENTICAL on General.cpp, Timer.cpp, AudioEngine.cpp, app_game.cpp
  (Lockstep.cpp is not in the stale build/StandaloneRelease compile db; its only change is an `&& !NOTSA_PORTABLE` on a Win32-undefined macro).
## Scans on the 16 merge-changed game_sa/app TUs
- pc24_scan (--cc build/PortableMac): no AUTO/CHECK site on a line added by the merge -> 0 conversions.
- ftol_scan: no class A site on added lines; one class B (EventHandler.cpp:1081 `(int32)(1000.f/(...*ExeRecip(40.f)))`, a block moved
  by the merge, same expression as before) -> left, class B is not converted by the codemods. 0 NOTSA_FTOL conversions.
- Windows-side semantic changes reviewed (FileRecord on all platforms, CarCtrl tail, EntryExit, EventHandler, Streaming): nothing
  portable-specific needed; GetUpperCase(uint8), DELETE_OBJECT handle, StareAtPed value compile clean.
## For the Windows host
- New code under NOTSA_STANDALONE_RUN that uses Win32 API or the GameRand.h hooks must also be `!NOTSA_PORTABLE` (or have a stub in the header).
No background tasks or wine processes left by this agent.
