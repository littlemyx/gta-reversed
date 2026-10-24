# MAC-FLOOP3 report (2026-10-24): record/replay fidelity loop 3 - the 900-frame and 2480-frame routes are now bit-identical to the Win32 build
Commits (portable/main, pushed): c5c8c75a (tool v3), 396e3e57 (AnimBlendNode.h + player probe), 75085658 (standalone RW layer), b93128bd (General.h / PathFind.h / scripts / codemod leftovers), efa8a0a7 (screen-size pin + drive route). Details RR_PLAN 11.9.

## Divergence table (first divergent frame, exact / quantised; native vs Win32 RR variant under Wine)
| route (frames) | step | rng | pop | pl | cam | pd | vh | ob |
|---|---|---|---|---|---|---|---|---|
| walk900 | FLOOP2 report | 254/254 | 256/256 | 56/181 | 181/182 | 27/181 | 27/200 | 163/286 |
| walk900 | start of FLOOP3 (rebuilt with the FLOOP2-REV fixes) | none | none | 56/none | none | 56/none | 231/none | 163/none |
| walk900 | + AnimBlendNode.h on x87d | none | none | none | none | none | none | 163/none |
| walk900 | + standalone RW layer on x87d | none | none | none | none | none | none | none = IDENTICAL, 900 frames |
| walk900 | + General.h / PathFind.h / scripts, screen pin | IDENTICAL (re-recorded Win32 side with the pinned exe) | | | | | | |
| walk3k (new, 2480 frames: sprint, jumps, 2 more ped batches, police car) | after the RW layer fix | IDENTICAL, 2480 frames (screen pin run: Win32 re-run see below) | | | | | | |
| drive (new, 1444 frames: car without driver, get in, throttle / steer / handbrake / reverse, get out) | before the screen pin | 436/436 | 435/435 | 436/926 | 925/944 | 435/435 | 435/435 | 505/505 |
| drive | after the screen pin | 942/942 | 957/957 | 436/926 | 925/944 | 436/926 | 436/926 | 966/966 |
The old open item (CAutomobile::ProcessControl wheel-pair step, frame 27) is gone: it was the same class as everything else (animation / RW-layer rounding feeding the vehicle drivers' skeletons), no wheel code was touched.

## Root causes (all class F: PC=24 rounding spelled as 53 bit double in code the codemod could not see)
| # | where | why the tool missed it | fix | proof |
|---|---|---|---|---|
| 1 | `game_sa/Animation/AnimBlendNode.h`: BlendAmountExt, CompressedDeltaTime, LerpTranslation, GetTimeRemainingProgress, I_GetCurrentTranslation / I_Update ... all `double` "extended" temporaries | header-only inline code lives in the PCH, the AST dump never contains it (FLOOP2 note) | x87d / (x87d) by hand | player mv 4 ulp at frame 56 (the keyframe boundary t = 0.1 was crossed with another interpolation weight), pl/pd/cam/vh exact closed |
| 2 | `standalone/rw/{rwmath_exact.h, rwtrig.h, rtquat.cpp, hanim.cpp}`: the exe-exact RenderWare shims keep FPU-stack temporaries in double ("rounded to the PC=24 the game runs with" - true on Win32 only): RpHAnimKeyFrameBlend slerp, RtQuat*, RwV3dTransform*, matrix kernels | not in game_sa/ (the tool only scanned game_sa/*.cpp) | tool run with `--headers` over standalone/rw, helpers D()/F()/rwtrig on x87d by hand, sqrt-table builder stays 53 bit (`// pc53`) | held props (obj 63/89, CJ_CIGGY etc.) orientation ulp errors from frame 163: ob closed, 900 frames IDENTICAL |
| 3 | `General.h` GetRandomNumberInRange (float + integer): `rand() * (1/32767) * range` as ONE double chain; `PathFind.h` OneWayLaneOffsetExtended; script helpers Magnitude87 / Group09_12 | PCH headers / .hpp | x87d | not visible in the 2480-frame route (no flip); consistency |
| 4 | codemod leftovers in clean files: Cam, CutsceneMgr, FxEmitter, Garage, Group21b, Group23 (the "not touched" list of FLOOP2) | other agent's edits then | tool v3 --apply | |
| 5 | harness: CCamera::CalculateFrustumPlanes uses RsGlobal.maximumWidth/Height (aspect of the vertical planes); native window 2560x1440 vs Wine desktop 1512x982 | not a float bug | `NOTSA_RR_SCREEN=WxH` (default 1280x720) pinned in game on both RR variants (statehash.cpp AdvanceClock) | drive route: car removal frame 435 (RemoveDistantCars: CAutomobile::Render sets m_nTimeTillWeNeedThisCar when visible) closed |

## Tool pc24_scan.py v3 (step 0), self-test tools/portable/pc24_selftest/run.sh (OK)
| blind spot | now |
|---|---|
| stale PCH looked like "0 rows, idempotent" | `ninja -n <pch>` preflight: refuses to run (exit 3) unless `--rebuild-pch`; any TU with clang errors / no AST is a failure (exit 4 when > 2 %, else exit 5 + message); verified with a fake ninja and a broken compile database |
| locals whose address is taken (AddrOf(x), `qword ptr [x]`, non-const reference parameter, std::ref) | never retyped (LOCAL-CHECK-D, note says why); const-ref use is fine |
| math overloads changed by the rewrite (pow/log10/sin/.../atan2/hypot ...) | MATH-OVERLOAD rows, `--apply` wraps the changed argument(s) in X87W(); no lines left in game_sa; rtquat.cpp std::sin/cos(h) wrapped |
| macro-expanded operands | operands spelled in macro ARGUMENTS are edited at their spelling location, function-like macro invocation extents are balanced (NOTSA_GLOBAL_EXPR, I(x) ...); only tokens spelled in a macro BODY stay SKIP-MACRO |
| int -> x87d of values that can exceed 2^24 | INT-WIDE rows (report only): 14 in game_sa |
| also: fabs/floor/ceil/trunc no longer wrap a 53 bit operand in X87R (AcosCrt `ad`), DONE verdict for already wrapped chains (the second --apply is 0 edits, asserted by the self-test), `--headers REGEX` (headers outside the PCH, edited once per file), `--census-headers` (textual list of `double` in PCH-hidden game_sa headers: 30 lines in 12 files, General.h / PathFind.h / AnimBlendNode.h done, FxInterpInfoGetVal.h / AEAudioTap.h / CullZones.h compare only / World.h Ext decls left), `// pc53` marker | |

## MSVC neutrality (wine cl.exe, code bytes + relocation targets, tools/standalone/obj_code_sig.py, A = HEAD tree, B = HEAD + my files, flags of build/StandaloneReleaseRR minus PCH)
* Run 1, 48 TUs (all 35 standalone/rw TUs, the 6 edited game_sa TUs, the 6 AnimBlendNode.h users, statehash): 43 identical; statehash.cpp (probe code, RR-only); hanim / light / rtquat / rwmath_tables differed ONLY by one unused COMDAT copy of rwx::D (my first version of the table builder stopped calling it) - builder restored to D()/F(), re-verified in run 2.
* Run 2, 32 TUs (those 4, General.h users CarCtrl / AutoPilot / Population / Ped + 12 random GetRandomNumberInRange users, all Group09..25_26 TUs that include the edited .hpp): 31 identical + statehash (RR-only).
* statehash.cpp, rr_route_*.txt, pc24_scan*: not part of the default Win32 build (GTASA_STATEHASH option / tools).

## Verification of the Win32 side
Win32 reference runs with probe exes (relinked by /tmp scratch tool w32probe.py = wine cl.exe of the changed TUs + original objects, link without PDB): the same code as the committed sources. walk900 pinned: native vs Win32 IDENTICAL 900 frames. Traces in .notes/rr_traces: {portable,win32-wine}-walk3k, -walk900-pin, -drive-pin, portable-walk900-floop3.
Smoke (tools/portable/play1 s1 / s2): see the end of this file.

## Open
* drive route, recorded frame 436 (exact): the player's car (veh slot 10, model 411): `steer` 3e725ab1 vs 3e725ab0 (1 ulp), matrix ry/fx ~1e-5 components 55 ulp apart, mv/tv: CAutomobile::ProcessControlInputs (raw steer smoothing, powf(raw, 2) == raw*raw verified on 2M random floats, DegreesToRadians) or the pad input path; next: `NOTSA_RR_LOG_PLAYER`-style probe on the car (raw steer, gas, brake) - the quantised divergence follows at 926, population / RNG at 942 / 957 (consequence).
* Rosetta x87 fidelity (transcendentals): x87::cos/sin in CalculateFrustumPlanes etc. rely on the soft x87 being equal to Rosetta's fsin/fcos; unproven, Windows-host confirmation still open (PORTABLE_SYNC.md).
* Load: wine start-up flakes grow with the host load (up to 6 retries observed), 43 load average while other agents build.
Consumer audit: arithmetic only (typedef-level on Win32), no layout / shared-state change. The only behavioural harness change is the RR-only screen pin.

## Smoke (tools/portable/play1, EXE=build/PortableMacRR, own NOTSA_LOCK_FILE)
s1 (walk / sprint / jump, 30.7 s): OK, player moved (2321,-1256) -> (2346,-1255), 0 crash markers. s2 (spawn car, enter, drive, exit, 65 s): OK, invehicle reached twice, exit 0, clean `atexit` (no IKChainManager abort any more: MAC-C3), 0 crash markers.
