# MAC-R12: slice R-12, lighting close to the D3D9 fixed-function model on librw GL3 (2026-10-24, portable/main)

Commits: librw fork `notsa` d546b26 + 663392f (pushed); gta-reversed 5d259d01 (pin), fdda05b3 (test + car pipeline + routes), 7a588af0 (pin), 1a118e65 (cmd:lighting + A/B marks), 27b93723 (log), notes commit. Final pipe test on 663392f: PIPE RESULT OK 182 checks; with NOTSA_GL3_OLD_LIGHTING=1: 10 FAIL. Win32: no MSVC TU touched (below).

## Reference: what the Win32 exe does (read from the shim ports, all "verified against the exe")
* `standalone/rw/pipeline.cpp` LightingCallback (exe 0x7574E0): lights = global lights with LIGHTATOMICS; ambient lights are SUMMED into `AmbientSaturated` (not clamped); directional lights only if the geometry has NORMALS; no LIGHT flag / no lights -> D3DRS_LIGHTING off; no local lights exist. Directional D3DLIGHT9 diffuse = light colour, direction = frame `at`.
* `RwD3D9SetSurfaceProperties` (0x7FC4D0, rwd3d_ff.cpp): modulate = MODULATE flag and colour != white. modulate: Diffuse = c*kd, not prelit -> Ambient mat = c*A*ka, D3DRS_AMBIENT white; prelit -> Emissive = c*A*ka, ambient source = vertex colour, D3DRS_AMBIENT = c. not modulate: Diffuse = kd, Ambient = A*ka, prelit -> Emissive = vertex colour. D3D sums Emissive + AmbSrc*(GlobalAmb + light ambient) + sum(Diffuse*Ld*max(N.-d,0)) and saturates ONCE at the end.
* Lighting off: prelit -> vertex colour (x TFACTOR = material colour when modulating); not prelit -> flat TFACTOR 0xFF000000 silhouette (black, alpha 1, vertex alpha off) (R9-3).
* D3DRS_NORMALIZENORMALS on for non-unit world matrices (0x757757). Skin VS (skin_vs.cpp, 0x75FAAC): `oD0 = (prelit + ambient + sum(light)) * matColor` saturated at the output = same order as the FF path.
* Car pipeline: spec material also enables D3D light 1 (g_GameLight: diffuse 0.25, ambient 0.75, specular 0.65); its ambient term multiplies the AMBIENT source: material ambient, or the VERTEX colour (not the material colour) for prelit + modulate.

## Findings (librw gl3 as of R-10) and fixes
| # | difference | fix |
|---|---|---|
| 1 | librw: `clamp(prelit + amb*ka + dyn*kd) * matColor`; D3D/RW VS: `clamp((prelit + amb*ka + dyn*kd) * matColor)`. Differs whenever the lit sum > 1 on a coloured material (bright day light: D3D saturates the product, librw returned the bare material colour) | default / skin / matfx_env vertex shaders (librw gl3, same order) |
| 2 | normals not renormalised (scaled frames, blended skin normals) | `NormalizeNormal` (header.vert), all three shaders + car shader |
| 3 | unlit + not prelit drew black with the MATERIAL alpha (blended); exe: opaque (R9-3) | `setMaterial(flags,..)` forces alpha 255 when `curLightBits == 0 && !PRELIT` |
| 4 | car pipeline: extra light 1 terms were added before the material multiply, also for prelit + modulate where D3D multiplies the vertex colour, not the material colour | carenv shader: `addAfter = prelit*0.75` for prelit+modulate (flag in `u_surfProps.w`), else ambient*0.75 inside the product |
| 5 | fog colour R/B (re3 f027c3af) | checked: shim swaps R/B both ways; new test with an asymmetric colour (0xFFC86432 -> (208,122,79) as computed) passes. Nothing to fix |
| 6 | model switch for A/B | `rw::gl3::lightingModel` (1 = D3D order default, 0 = librw original, env `NOTSA_GL3_OLD_LIGHTING=1`, `cmd:lighting:old|new` at run time); carried in `u_ambLight.a` (unused before) so no new uniform |

## Tests (tools/portable/run_pipe_test.sh, tests/portable/rw_pipeline_gl_test.cpp part D `RunLighting`)
CPU reference `RefD3D` written the D3D way (Emissive / AmbientSource*(GlobalAmbient+LightAmbient) / Diffuse material sources, one saturate), 23 cases (day / night / interior ambient-only / two directionals / prelit building + coloured material / ped skin / ka 0 / kd 0 / back-facing normal / lighting off prelit modulated / not modulated / not prelit = black / 5 car-spec cases through the SA env pipeline incl. the extra light 1), tolerance 3/255 on the quad centre, plus: unlit alpha 128 opaque black, scaled frame x2.5 normalised, fog colour R/B, `RwRenderStateGet(FOGCOLOR)`. **PIPE RESULT OK, 182 checks** (was 153). Mutation: `NOTSA_GL3_OLD_LIGHTING=1` (old order) -> 10 checks FAIL (saturating light on coloured material, prelit + modulate x3, interior prelit, car x4, unlit alpha, scaled normals). Skinned == static (R-10 checks, same shader) still passes, so ped lighting equals the default pipeline numerically. All earlier checks unchanged.

## In-game frames (.notes/w0/r12/, native, caffeinate, own NOTSA_LOCK_FILE; route tools/portable/play1/r12{d,n,i}.txt)
r12_day_street_{before,after,oldmodel_samebuild}.png, r12_night_street_{before,after}.png, r12_day_car_*.png, r12_night_car_*.png (before = HEAD 1b5e788b exe, after = this slice, oldmodel_samebuild = this exe with NOTSA_GL3_OLD_LIGHTING=1). The camera phase differs between runs (the same route, +-1 s), so frames are visual evidence, not pixel-diffable; the numeric proof is the pipe test. Day street / car: nearly identical (day light sums rarely exceed 1 on grey materials); night: warm lights on the car / sidewalk, car body tint changes from pale green to warm white (the saturate-after-material order).
LIGHTLOG (`NOTSA_R12_LIGHTLOG=1`, night 23:xx, Idlewood): car meshes `flags 0xf3` (LIGHT + NORMALS + MODULATE, not prelit), ambient (0.10,0.10,0.10), directional (0,0,0), ka 0.3-0.4, kd 1. So the diffuse/ambient terms of the night car are tiny (~0.04); its bright body in the frames comes from the SA env + specular stages (factor from `gSpecIntensity` = CRenderer::SetupLightingForEntity multiplier, specular colour of g_GameLight 0.65 constant), identical to the exe's pipeline code.

## R-n items / open
| id | what |
|---|---|
| R12-1 | R9-2 (night car too bright): the lighting equations are now D3D-exact and tested; the remaining brightness is the env / spec stage. Whether `gSpecIntensity` (lighting multiplier from `GetLightingFromCol` x `CPointLights::GenerateLightsAffectingObject`) is as small as in the exe at night is NOT verified (needs a frame from the Windows build or a log of the multiplier; `NOTSA_R12_LIGHTLOG` logs it) |
| R12-2 | alpha test: shim maps RW GREATER ref r to librw GEQUAL r; fully transparent texels (alpha 0, ref 0) pass in GL, D3D drops them (renderstate.cpp is MSVC-compiled: not touched here) |
| R12-3 | lit geometry with vertex alpha: D3D takes alpha from the material only (DIFFUSEMATERIALSOURCE = MATERIAL), GL multiplies prelit alpha x material alpha |
| R12-4 | point / spot lights: SA never creates them in the RW world; GL keeps librw's linear (1 - d/r) model, = the RW skin VS model, unchanged and untested against D3D |
| R12-5 | NOT landed: in-game interior frames and same-scene old/new A/B frames (`cmd:interior` + `cmd:lighting:old|new` + routes r12i/r12d/r12n exist and compile). After the first 4 successful runs (14:33-15:01) ~10 consecutive runs hung in the GL swap / glDrawArrays (main thread blocked in WindowServer `CGSWindowIsOrderedIn` / Metal blit; watchdog abort at 238 s), with and without background priority, also on the previous exe; environment (macOS compositor), not the change. Interior is covered by the synthetic CPU-reference cases only; gSpecIntensity at night is not logged yet (R12-1) |
| R12-6 | `NOTSA_R12_LIGHTLOG` diagnostics stay in (env-gated) |

## No-op proof (Win32)
gta-reversed diff 1b5e788b..HEAD: source/portable/** (portable-only glob), tests/, tools/, vendor/librw pin: no MSVC-compiled TU. librw diff 4f8dd81..663392f: src/gl/ only (gl3 backend, compiled only for LIBRW_PLATFORM=GL3; rwgl3.h content is inside `#ifdef RW_GL3`), d3d9 / null library builds byte-identical by construction. No wine A/B needed (no TU touched).
