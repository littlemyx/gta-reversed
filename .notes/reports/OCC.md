# OCC report: COccluder verbatim ports + GetATanOfXY caller audit

## Task 1: COccluder::ProcessLineSegment (0x71E130) / ProcessOneOccluder (0x71E5D0)
Commit 18ca5ede (Occluder.cpp, common.cpp, fixed_oracle_test.cpp). fixed_oracle_test, 4000 cases: PC24 mismatches 1721 -> 0 and 1918 -> 0;
the whole test: 0 PC24 mismatches over 16 functions (PC53 is informational, as before). The occluder fixtures are default-on now.

The "BUG fix" in ProcessLineSegment (index roles of the 2nd endpoint, `lerp`) is NOT gated: it computes the same point mathematically
(the point of the segment at z = 1.1 is unique); only the rounding differs, so there is nothing to gate. The exe uses
`from*t + to*(1-t)`, t = |zTo'| / (|zFrom'| + |zTo'|), z' = z - 1.1f, for BOTH endpoints, with the from-z spilled to float, the to-z on the stack.

Deviations found and fixed (asm VAs):
| Where | Exe | Port was |
|---|---|---|
| PLS flipped line origin | 0x71E481: origin = float(delta + from) (not exactly `to`), dir = -delta | origin = to |
| PLS dir/length | 0x71E4B6: len = float(sqrt(dy^2+dx^2)), dir = float(delta * (1.0/len)) | Normalized() |
| PLS min/max | 0x71E36A: FCOMP/FCOM + JP/JNP chains (ties/NaN take the later operand) | std::min/max |
| PLS return | 0x71E52C: (W/2-ox)*dirY - (H/2-oy)*dirX < 0 | IsPointInsideLine (>= on NaN differs) |
| DoesInfiniteLineTouchScreen 0x71DB80 | strict on-screen test (0<x<W, 0<y<H); two edges as inline products, right/bottom through 0x71DB30 | inclusive rect + 4 helper calls |
| CalcScreenCoors(in,out) 0x71DAB0 | z<=1 AND NaN rejected | NaN passed |
| POO distance 0x71E6D5 | sqrt((l^2+w^2)+h^2) on the x87 stack, fsubr, _ftol2, low word | Magnitude() float |
| POO rotation 0x71E743..0x71E7FF | (Y*X)*Z of SetRotateX/Y/Z (0x59B060/0x59B0A0/0x59B0E0, 0x59BE30) | SetRotate(x,y,z) (0x59B120, different formula) |
| POO face visibility 0x71EAFF..0x71EDDD | 6 inlined dot products: different term order per face (up/right x,y,z; bottom/left x,z,y; back/front z,y,x), dx/dz spilled to float on different faces | one Dot() |
| POO size checks 0x71F321/0x71F7EB | fail iff W*k > dx (ordered), NaN passes | `<=` forms |
| POO face offset 0x71F3AE..0x71F415 | (dir_unnormalised + centre) . dir_normalised, ((x+y)+z) | (centre + normalised dir) . normalised dir (a +1 error) |
| POO flat variant 0x71F844 | normal = Normalise(right x up), offset = (n.x*c.x + n.y*c.y) + n.z*c.z | un-normalised normal |

Unchanged and verified equal: the 12-edge chain order and indices, the corner sums (left-assoc float adds), the screen-depth range test (NaN-equal), the 4 flat-case branches.

## Task 2: GetATanOfXY callers (0x53CC70)
Background: fpatan is not affected by the x87 precision control, so the result stays 64-bit until the first arithmetic op / store.
All 111 `call 0x53cc70` sites in the exe were classified (helper agent, reviewed by me for the Camera.cpp behavioural claims at 0x5154CA and 0x5155BC).
Helper commit bbfba408 (8 files). Added oracle case: CIdleCam::VectorToAnglesRotXRotZ 0x50A360 (aa1c8fbb), 0 PC24 mismatches / 4000. The other
fixed sites call entity/camera state and are not pure enough to drive from this harness.

| Port site | Exe VA of call | Verdict |
|---|---|---|
| PlayerInfo.cpp EvaluateCarPosition | 0x56DAE2 (spill), 0x56DB35 | fixed (Ext, inlined limit loops, fmul 1/2pi const) |
| TrafficLights.cpp:139 | 0x49D565 | fixed (Ext * 57.29578f) |
| Camera.cpp StartTransition | 0x5152A9, 0x515463, 0x5154CA, 0x5155BC, inline fpatan 0x515417 | fixed; ALSO 3 behaviour fixes: FOLLOWPED->CAM_ON_A_STRING always adds 235 deg (inlined loops/fabs run on a freshly loaded 0.0); CAM_ON_A_STRING stores the atan as is; aim modes use inline atan2(fwd.x, fwd.y) |
| Camera.cpp CamControl | 0x52ACC9, 0x52ADA6 | fixed |
| Camera.cpp Process (KeepAwayFromTarget) | 0x52BDD6, 0x52C125 | fixed (double angle for fcos/fsin) |
| Camera.cpp RotateHeading | 0x51F5DD, 0x51F82C, 0x51FB14, 0x51FD8F | fixed |
| Cam.cpp AvoidTheGeometry | 0x514100 (spill), 0x514133 | fixed (pitch double) |
| Cam.cpp Process_Rocket | 0x511E94 | fixed (- HALF_PI float const) |
| IdleCam.h VectorToAnglesRotXRotZ (exe 0x50A360) | 0x50A36C, 0x50A39D | fixed, oracle-tested |
| Object.cpp ProcessSamSiteBehaviour | 0x5A081F, 0x5A093C | fixed |
| PlayerPed.cpp EvaluateTarget | 0x60D0E9 (via GetATanOf wrapper) | fixed |
| CarAI.cpp UpdateCarAI | 0x42019F, 0x4201B3 (via GetATanOf wrapper) | fixed |
| Already correct | Camera.cpp 813/822/857/1038/1041/2373/2616/2617/4391/4795/4996, Cam.cpp 1265/1549/1550/1090/5624, Plane.cpp:622 (0x6CD1FD), Group09.cpp:123 (0x484522), Group13_15.cpp:798 (0x491392), Renderer.cpp:126 (0x72CCA3), IdleCam.cpp x4 (0x517A8E/ABD/AD0/AFF), PlayerPed lock-on sites | no change |

### Forbidden file (CarCtrl.cpp, agent BUG2 zone): changes still needed
| Line | Function | Exe VA | Needed |
|---|---|---|---|
| 2193 | FlyAIPlaneInCertainDirection | 0x423045 | keep `curHeadingExt = GetATanOfXYExt(fwd)`; targetHeading = (float)curHeadingExt / (float)(curHeadingExt - 2.0f) / (float)(2.0f + curHeadingExt); headingDiff = curHeadingExt - (double)targetHeading (0x42325A at 2257 is a float spill: OK) |
| 6966 | SteerAIHeliFlyingAwayFromPlayer | 0x42ACF9 | (float)(Ext(...) + (double)pi) |
| 7076 | SteerAIHeliToKeepEntityInView | 0x42AF08 | keep headingExt; wantedHeading = (float)(headingExt + (double)(PI/2)) |
| 6410 | SteerAICarWithPhysicsHeadingForTarget | 0x433479 | FindSpeedMultiplier((float)(Ext(...) - (double)heading), ...) |
| 5953 | SteerAICarWithPhysicsFollowPath | 0x435452 | same pattern |
| 5956 | SteerAICarWithPhysicsFollowPath | 0x435499 | (float)((double)curDirAngle - Ext(nextDirX, nextDirY)) |
Automobile.cpp (forbidden) sites match the exe (TellHeli fixed in 09a75367).

Unverified: the call at 0x4546A5 is in dead code (function 0x454340 has no references). For CarCtrl I only read the immediate consumers of the results.

## Verification
* fixed_oracle_test (StandaloneRW, 16 functions incl. the new IdleCam case): PC24 0 mismatches (run_all_tests: 16 checks, 0 failures). PC53 is informational.
* run_all_tests on the targets that contain touched files (fixed, gameplay, review, world, x87), after all commits:
  fixed ok, gameplay 311 checks ok, review 42 ok, world 114 ok; x87_oracle_test is reported NORESULT only because its summary line
  is "67 rows, ..." (parser expects "functions,"); the log shows PC24 2 / 2, both on CQuaternion::Get_Euler 0x59C160 (spec inputs), a function
  nobody in this task touched. No baseline run existed for these targets before the changes (the orchestrator's earlier full runs never got past
  physics/game oracle), so "no new failures" is argued from the failing function being outside every touched file.
* Occluder.cpp / common.cpp only link into fixed_oracle_test (+ the lib), so no other test can be affected by Task 1.
* FIXADDR reported a failure of CIdleCam::VectorToAnglesRotXRotZ: that run used a stale test exe built from my first (uncommitted)
  draft of the case, which passed the oracle out-args swapped (aa1c8fbb already contains the fixed call). Re-run 68874 (15:51): 0 / 0.
* StandaloneRelease: objects of every touched .cpp (Occluder, common, Camera, Cam, PlayerInfo, TrafficLights, CarAI, Object, PlayerPed, IdleCam users) compiled
  without errors in build/StandaloneRelease (a concurrent build from another agent did the final link at 17:02; I could not complete my own full link
  because of lock contention and other agents' ongoing header changes).
