# ASM-S57b (also S56c + the attractor pool stride todo) - 2026-10-29
S56c: 1 row, MATCH (DrawCrosshair trail loop: all render states, history shift incl. the harmless Y->time copy, pulse / ring / angle maths, 9 exe float constants). Table: S56c.result.tsv.
S57b: 25 rows: 14 MATCH, 4 DEVIATION (fixed), 7 NOT_REACHED (budget). Table: S57b.result.tsv.

Attractor pool stride (SYSTEMIC_TODO): exe pool ctor 0x550E40 allocates 0xC4 bytes per slot, CPedAttractor is 0x8C (ctor 0x5EDFB0 writes up to the 0x84 script name); all 10 derived ctors only call the base ctor, the port derived classes add no field. 0x38 bytes of every exe slot are unused; nothing addresses attractors by byte offset. No behaviour depends on it: documented, todo ticked.

Fixes:
- CReplay::Update (0x460500): F1/F2/F3 are GetIsKeyboardKeyJustDown (0x52E450), the port used the held-state GetIsKeyboardKeyDown (replay re-triggered / finished every frame while the key was down). GetIsKeyboardKeyJustDown made public (separate commit).
- CReplay::PlayReplayFromHD (0x460390): the file is closed (0x460456) before SetDir / TriggerPlayback / StreamAllNecessaryCarsAndPeds.
- CReplay::TriggerPlayback (0x4600F0): OldRadioStation is read before StopRadio (0x46015C / 0x460171).
- CTheScripts::DrawScriptSpritesAndRectangles (0x464980): in the angled-rect case sin(angle) is spilled to a float (fstp [esp+0x34]) before the products, cos stays unrounded.

MATCH: Display, SaveReplayToHD, EmptyPedsAndVehiclePools_NoDestructors, StreamAllNecessaryCarsAndPeds, CreatePlayerPed, RestoreStuffFromMem, FinishPlayback, StoreStuffInMem, FastForwardToTime, PlayBackThisFrame, the 4 pool New() rows (carried from S57).
Open (NOT_REACHED): RetrievePedAnimation 0x45B4D0, ProcessPedUpdate 0x45CA70, DealWithNewPedPacket 0x45CEA0, ProcessReplayCamera 0x45D060, ProcessLookAroundCam 0x45D760, RecordThisFrame 0x45E300, PlayBackThisFrameInterpolation 0x45F380 (the large packet decode / record functions).
Notes (no change): TriggerPlayback with no IN_USE buffer gives slot 1 in the exe (port 0), unreachable; PlayReplayFromHD does not bound the read loop at 8 buffers in the exe (corrupt files only); CreatePlayerPed port breaks where the exe continues on an empty first packet, equivalent for the contiguous buffer layout.
Tests: Release + StandaloneRW build clean; wave2_oracle_test (rebuilt after the last edit): 1142 functions, PC24 0/0, PC53 0/0; combat regress PASS=14 (first run had one flaky combat.foot.weapon_pickup FAIL, unrelated to replay/scripts code, passed on rerun); v6 grep clean; PortableWin64 compile of Replay.cpp / TheScripts.cpp / ControllerConfigManager.cpp clean.
