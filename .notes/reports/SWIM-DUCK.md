# SWIM-DUCK - four open items of HEADING-AUDIT (+2 found in the review pass)

All four claims were CONFIRMED against the exe asm (disassembly: tools/standalone/exedis.py). One commit per function, branch swim-duck (worktree C:/gta-wt/swim-duck).

| # | function / exe | claim | verdict | fix commit |
|---|---|---|---|---|
| 1 | PlayerControlDucked 0x687F30 | direction (-sin a, cos a, 0) | CONFIRMED. exe 0x6880B8..0x6880EC (2-player) and 0x68820D..0x68823D (no-target): [edx]=-sin (fsin;fchs), [edx+4]=cos, [edx+8]=0 -> IsPlayerAllowedToGoInThisDirection; projections 0x688107..0x68814E use y*r.y + x*r.x, y*f.y + z*0 + x*f.x. Port had (0,-sin,cos) for the projection and (0,-sin,0) for both checks. | 22513e21 |
| 2a | ProcessControlInput 0x688A90 | same, 2-player | CONFIRMED (0x688FBB..0x688FD4: [esp+0x20]=-sin, [esp+0x24]=cos, [esp+0x28]=0) | f9b3708b |
| 2b | ProcessControlInput | mouse wrap `+= 2pi` | CONFIRMED. 0x6891CC..0x6891FC: fRot=-(aim-cur); >pi: -=2pi (0x6891E7); <-pi: fadd 2pi (0x6891FC). Port had 2pi - aim - cur. | f9b3708b |
| 3 | ProcessControlAI 0x689640 | 2D vs 3D | CONFIRMED. 0x689882..0x6898A5: fld [eax+4], fld [eax], x*x+y*y, fsqrt, fcomp 5.0 (0x858C80). | 3414992b |
| 4 | SeekPosCalculatorRadiusAngleOffset 0x6946F0 | null matrix | CONFIRMED. 0x6946F8..0x694731: matrix null -> (-sin h, cos h) of [target+0x10] stored as floats. Port called GetForward() on the null matrix (crash). | 3079a93a |

## Extra deviations found by the line-by-line review (sonnet helper, each re-checked against the asm by me)
- ProcessControlAI 0x689640, 0x6897F8..0x689830: m_pPed null / no group / leader != m_pPed all jump to 0x6898AD (anim-speed test); port took that branch only when m_pPed was null. Fixed in 3414992b.
- ProcessControlAI 0x689915: `test dh,0x20` on [ped+0x478] = byte 0x479 bit 5 = bCanClimbOntoBoat; the port tested bIsDyingStuck (byte 0x478 bit 5; cf. 0x442CF6 test byte [ecx+0x478],0x20 in CGameLogic 0x442AD0). Fixed in 67087158.
- ProcessControlInput 0x689463: SWIM_SPRINT `fabs(st0)` = the turn value (pedWalkX, but the clamped mouse turn in mouse-3rd-person mode); port used fabs(pedWalkX). 80-degree clamp constants 0x870944/48 = +-1.3962634801864624 (1 ulp above DegreesToRadians(80.0f)). Fixed in 513672ea.
- PlayerControlDucked: the rest of the function verified equal (conditions, constants, field offsets, branches, call arguments).

## Not changed (precision only, <= 1 ulp, existing project-wide pattern)
- LimitRadianAngle / GetRadianAngleBetweenPoints results are rounded to float in the port while the exe keeps them unrounded in st0 for fsin/fcos and the subtraction of the camera orientation (the `...Ext` variants exist); std::pow results rounded to float before the multiply in ProcessControlInput's decay (0x6894F7, 0x68953F, 0x6895BD). Neither changes behaviour measurably.
- PlayerControlDucked reads the weapon slot as signed byte in the exe, uint8 in the port (slot never negative).

## Repro
Items 1, 2a and the ducked 2-player branch only run under two-player controls; 2b only with the mouse-3rd-person option; 3/4 need a swimming group follower / a matrix-less target. No cheap scripted repro exists (the fixed functions are not oracle-able, class C/D in ORACLE_COVERAGE); verification = asm comparison (above) + regression gates.

## Gates
See the final status line in PHASE2_STATUS.md (Release build, run_all_tests, soak).
