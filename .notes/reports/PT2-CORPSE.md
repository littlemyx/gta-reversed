# PT2-CORPSE - playtest U4 (corpses rotated) + ghosting

## Summary
| # | finding | verdict |
|---|---|---|
| U4 | dead pedestrians lie rotated 90 degrees (legs up along walls, bodies sunk) | **port bug, fixed**: `CTaskSimpleDead::ProcessPed` (0x630600) used `acos` where the exe calls `asin` (0x821E70) |
| ghosting | double images on moving peds in the photo | **not a port artefact** (photo of the screen, see below) |
| (extra) | `CPedIK::PitchForSlope` took the bone-rotation branch for `bFallenDown` peds | port deviation from 0x5FE0E0, fixed in the same commit |

## U4 root cause
* The tilt block of `CTaskSimpleDead::ProcessPed` computes `roll = f(n . right)` ([ped+0x520]) and `pitch = f(n . fwd)` ([ped+0x518]) from the ground normal `field_578`.
  Both call sites (0x6308DA, 0x630927) call **0x821E70**. That function is the CRT `_CIasin`: executed in unicorn (`PT2-CORPSE_tools/trig_unicorn.py`) it returns
  f(0)=0, f(0.5)=0.5236, f(1)=pi/2, f(-0.5)=-0.5236. `_CIacos` is 0x822380 (`X87Intrinsics.h` documents both). The port (upstream reversal, kept by the x87 conversion 58b0d67c) used acos.
* On flat ground `n . fwd = 0`: exe -> 0 rad, port -> pi/2. `CPed::ProcessControl` skips `CalculateNewVelocity` (the only decay of the slope values) for `PEDSTATE_DEAD`, so the value stays,
  and `CPedIK::PitchForSlope` (dying branch: `RotateX(-pitch)`, `RotateY(roll)` about the ground point, every PreRender) rotated the finished death pose by 90/90 degrees for good.
  Visible as: body flat while the die anim plays (state DIE uses asin in CalculateNewVelocity), turning on its head ~1-2 s after the death, when the DEAD task makes its first tick.
* Numbers (injector `cmd:dumpdead`, run build, 6 civilians shot with AK, ground flat; route_time.txt):

| | DIE (54) | first DEAD (55) tick | later |
|---|---|---|---|
| before, flat ground | pitch 0.000 / roll 0.000 | **1.571 / 1.573** | 1.57 / 1.57 (no decay) |
| before, 30 degree ground n=(0,.5,.86) | - | **1.046** (acos .5) / 1.571 | same |
| after | -0.006..0.001 / -0.004..0 | -0.006..0.001 / -0.004..0.004 | same |
| ORIGINAL exe (gta_sa_compact, native, same scenario) | - | 0.000..0.001 / -0.002..0.004; slope n=(0,.5,.86): **0.525** (asin .5) | same |

* Screenshots (back buffer, same scene; `C:/gta-scratch/ptk_out/`): `c1/mark_g1_1s.png` flat at 1 s, `c1/mark_g1_4s.png` + `mark_g1_8s.png` legs up (before),
  `c1/mark_g4_shotgun_head.png` four corpses standing on their heads; `c3/mark_g1_8s.png`, `c3/mark_g4_shotgun_head.png`, `c3/mark_g5_fall.png` (after, lying flat);
  original: `orig_o4/dead/g2_fist.png` corpse lying flat. The user's photo (playtest1/4.webp) is the same defect (the "stretched leg" is the leg of a body rotated 90 degrees against the wall).
* Scenarios: shot (AK, torso), fist, baseball bat, shotgun headshot, 35 m fall, run over (weapon 49 damage event): all end at slope ~0 on flat ground after the fix, original identical.
  Original and port numbers match for every group (`ptk_out/orig_o4/probe.txt`, `ptk_out/c3/standalone.log`).

## PitchForSlope (0x5FE0E0)
The exe's first block (0x5FE120..0x5FE13A) leaves `bl = (state == 0x36 || state == 0x37 || (flags dword 0x474 & 0x100000))`, i.e. dying OR `bFallenDown` (flags dword 0x474 bit 20, verified from the
port's bitfield layout and `0x470 |= 0x4000` = bUpdateMatricesRequired). `bl` selects the matrix-tilt branch (0x5FE296); the pitch / roll clamps use the state alone (0x5FE15C, 0x5FE201).
The port used `IsStateDying()` for both: a knocked-down (not dying) ped took the bone-rotation branch. Fixed; the byte 0xC0AFC0 read by the thigh code is never written (always 0), so the port's non-fabs path is right.

## Ghosting
Photo 4: moving peds (red cap, the ped in front) show a second, offset, tinted copy, while CJ and the ped on the right (standing) are sharp. That is the signature of a phone exposure
spanning two displayed frames of a ~30-60 fps game (moving things double, static things not), not of the renderer: in the port's own back-buffer captures (`NOTSA_STANDALONE_SCREENSHOT`, burst of 4
consecutive Presents with CJ running, `ptk_out/g1/burst_walk_0[0-3].png`) moving peds are single and sharp, there is no accumulation / motion blur path in the run build (CPostEffects motion blur is only
enabled by script / cheat), and nothing draws a ped twice. Not reproducible as a port artefact; no change.

## Commits
* ac0e698a (main; a4adb688 in the worktree) -> main: CTaskSimpleDead asin + PitchForSlope bFallenDown.  * cf388272: run-build injector commands `spawnmany`, `killall`, `lift`, `dumpdead` (corpse scenarios).
Tools: `.notes/reports/PT2-CORPSE_tools/` (orig_dead.py = native original-exe harness: main-thread call stub via Wow64 SuspendThread/SetThreadContext calling CStreaming::RequestModel / LoadAllRequestedModels /
CPopulation::AddPed / CWeapon::GenerateDamageEvent, d3d9px screenshots; routes; run scripts).

## Not done / notes
* No exe-oracle row: `CTaskSimpleDead::ProcessPed` needs a fake player ped, intelligence, shadows and world state on both sides; the proof is the call-target identity (unicorn) plus the live numbers from the original.
* The upstream `acos`/`asin` swap class: the other `x87::acos` sites (Cam 2808, Quaternion::Get, CObject train crossing, BreakObject_c::Update 0x822394) call the exe's acos entry points; not every one of the 689 converted sites was re-proved.
* Gates (worktree C:/gta-scratch/ptk, base 26f9546b + the two commits): StandaloneRelease build OK; run_all_tests 44/44 (23 CPU oracle tests + 21 rw_ device tests under the GPU lock, 0 failures); soak 3 min RESULT OK (21 markers, 0 errors, 0 game [error] lines, 24.9 fps, ptk_out/soak3).
