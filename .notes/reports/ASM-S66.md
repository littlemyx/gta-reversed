# ASM-S66 + ASM-S57d (CRunningScript locate/area checks, CReplay::RetrievePedAnimation) - 2026-10-30
S66: 15 compared in full (4 MATCH, 11 FIXED in 10 commits, 0 UNSURE as a verdict). S57d: 7 rows, 1 FIXED, 6 NOT_REACHED (token budget). Tables: S66.result.tsv, S57d.result.tsv.

## S66 fixes (exe address, one line each)
- 614caae17 LocateCarCommand (0x487A20-0x487D02), 49ec0d55a LocateCharCommand (0x486D80-0x4870A5), 8ea7c238c LocateObjectCommand (0x487D10-0x487F5D), 668c9f1a7 LocateCharCarCommand (0x487420-0x487700), ee403646b LocateCharCharCommand (0x4870F0-0x4873F5), 232ad23a9 LocateCharObjectCommand (0x487720-0x487A00): parameters, jump tables (any / on foot / in car, stopped variants), highlight and debug-square arguments all match; the box test `!(c - r > p || c + r < p)` passed a NaN operand while the exe (fcomp + jp / jne) fails it. Now `c - r <= p && r + c >= p` (IsInCentreRange).
- ac1894e1b CarInAreaCheckCommand (0x488EC0-0x489147), 2e0ccf46b CharInAreaCheckCommand (0x488B50-0x488EA0), ccdb5d9cf ObjectInAreaCheckCommand (0x489150-0x4893A3): z/x/y corner swap, is3D and stopped ranges, highlight z = (maxZ + minZ) * 0.5 (0x858B8C) match; same NaN arm (IsInRange).
- 2325ac8cb CharInAngledAreaCheckCommand (0x487F60-0x4883B5) and, through the shared IsPointInAngledArea2D, ObjectInAngledAreaCheckCommand (0x4883F0-0x48877D): the AB direction is normalised INLINE on a copy in the exe (zero length -> factor 1, direction (0,0)); the port used CVector2D::Normalise (zero length -> (1,0)). AD uses the real CVector2D::Normalise (a NaN length takes the normalising arm, the port's `mag > 0` sent it to x = 1: local ExeNormalise2D). AB/AD dot tests fail on a NaN.
- 75f02e0f7 FlameInAngledAreaCheckCommand (0x488780-0x488B49): same ExeNormalise2D for the in-place AB/AD re-normalisation per shot; NaN-failing dot and Z tests.
- MATCH: ShutdownThisScript (0x465AA0), ProcessOneCommand (0x469EB0), Process (0x469F00), ScriptTaskPickUpObject (0x46AF50, ctor argument order checked against the stack layout).
- Open / UNSURE: ObjectInAngledArea with a ZERO-length AB: the exe's inline code pops its x87 stack once too often (`fstp st0` x2 then `fld 1` then `fmulp st(1)` underflows) -> NaN -> false; the port gives a defined result. Degenerate script input, not replicated.
- Not in the slice, seen on the way: CTheScripts::IsVehicleStopped compares `speed <= 0.01 * min(ts, old)` with the unrounded x87 product in the exe (0x4861F0), the port rounds to float; CVector2D::Normalise (0x44E480) has the same NaN arm difference as above (shared, left alone).

## S57d
- 78f5b8f52 CReplay::RetrievePedAnimation (0x45B4D0-0x45B71E): time/speed/blend are MULTIPLIED by the exe constants 0x859C2C / 0x859C28 / 0x859C24 (1/63.75, 1/85, 1/127.5), the port divided; the NumAnims > 0 check of the second animation applies only to ids > 3 in the exe (ids 1..3 blend in the ped's own group unconditionally); the third animation does not store m_nCallbackType.
- NOT_REACHED (budget): ProcessPedUpdate, DealWithNewPedPacket, ProcessReplayCamera, ProcessLookAroundCam, RecordThisFrame, PlayBackThisFrameInterpolation. Disassembly of the first four is in C:/gta-scratch/asm-s66/r45CA70.txt, r45CEA0.txt, r45D060.txt, r45D760.txt.

## Tests
See the report line of the agent (wave2_oracle_test, gameplay_regress combat + missions + arrest0, PortableWin64 compile of RunningScript.cpp / Replay.cpp).
