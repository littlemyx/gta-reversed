# ASM-S64c - 2026-10-30
S64c: 20 rows: 11 MATCH (ComputeAnimDoorOffsets, Init, ShutDown, IsPlaybackPausedForCar, SetPlaybackSpeed, RenderLineSegment, Render, RegisterRecordingFile, FindIndexWithFileNameNumber, InitAtStartOfGame, Extract), 8 DEVIATION (all fixed), 1 NOT_REACHED (LaserScopeDot, dead code). Table: S64c.result.tsv. Fixes are in source/game_sa/VehicleRecording.cpp and Weapon.cpp (one commit per function, "ASM-S64c: ..." on reverse/interior_c).

Fixes:
- SaveOrRetrieveDataForThisFrame 0x45A610 (mission car playback, the important one): (1) a PAUSED playback (Pause/UnpausePlaybackRecordedCar) kept advancing its clock, the exe skips the clock step while bPlaybackPaused; (2) the frame search bound `next` once (`auto& next = frames[cur+1]` in the for-init), so as soon as the first next frame was behind the clock the car ran to the LAST frame (and then stopped/looped); the exe re-reads the next frame each step; (3) the back-step ran only down to the old index, the exe runs down to frame 0; (4) clock step `(uint32)dt * speed * 0.25f + clock` and the interpolation factor `(clock - t0) / (uint32)(t1 - t0)` are unrounded (x87).
- SkipForwardInRecording 0x459D10 (script command): a positive distance walks forward until used up and does NOT walk back after the overshoot (port did); a remainder <= 0/NaN walks backward; PlaybackIndex is stored on every step (port never stored it); the remainder is kept unrounded.
- RemoveAllRecordingsThatArentUsed 0x45A160: no `number == slot` test (the port never freed files whose carrec number differs from the slot) and all 475 slots; RemoveRecordingFile 0x45A0A0 removes every slot with the number.
- SkipToEndAndStopPlaybackRecordedCar 0x45A4A0: clears bDisableCollisionForce (bit 2), port cleared bCollidable.
- CPath::RemoveRef (StopPlaybackWithIndex 0x459440): Free + RemoveModel without an m_pData test.
- Load 0x45A8F0: prune loop over byte offsets.
- TakePhotograph 0x73C1F0 (NOTSA-AUDIT resolved): range test !(sqrt((dz^2+dy^2)+dx^2) < 125) with unrounded differences for peds and objects (the exe skips out-of-range objects, like peds); screen border `fild(w)*0.1f < x < fild(w)*0.9f`, NaN rejected; the line of sight starts 2 m along TheCamera.m_mCameraMatrix's forward (exe 0xB6F9AC, VALIDATE_OFFSET 0x974), the port used the placeable matrix forward.

MATCH details: ComputeAnimDoorOffsets jump table decoded (0x6E3E08: 359, 361, 363, 373, 375, 378, 379); Extract offsets agree with Construct.
Not ported: LaserScopeDot 0x73A8D0 (no exe callers; stub kept).
Tests: Release (pre-rebase) + StandaloneRW (post-rebase, last edit) built; wave2_oracle_test 1142 functions: PC24 0/0, PC53 0/0; combat regress PASS=14 and missions PASS=11; v6 grep clean (the only hit is the integer-cast false positive in the clock step); PortableWin64 compile of Weapon.cpp and VehicleRecording.cpp: 0 errors.
