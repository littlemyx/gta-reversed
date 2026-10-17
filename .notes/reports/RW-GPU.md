# RW-GPU - the 5 rw_* failures on the real D3D9 driver (RTX 4080, native Windows d3d9.dll), 2026-10-17

Branch reverse/interior_c, commits be2b9eb4 6e1e3520 9ea80268 e3bbd13e bc5c5cef. Zone: skin_vs.cpp + 4 tests; no BUG-QUIT claim existed in ZONES.md, platform.cpp / video-mode code not touched.

## Result per test
| test | class | cause (evidence) | fix |
|---|---|---|---|
| rw_skin_vs_test, rw_skin_pipeline_test | (a) port bug Wine masked + (c) for some keys | see 1 | be2b9eb4 (port), 6e1e3520 (test) |
| rw_platform_test | (b) test encodes Wine behaviour | see 2a | 9ea80268 |
| rw_im2d_im3d_test | (b) brittle expectation | see 2b | e3bbd13e |
| rw_custom_pipelines_test | (b) test artifact, native FF semantics the exe would see too | see 2c | bc5c5cef |

## 1. skin_vs: D3DAssemble flags 4
* Bit meaning: the exe's 0x760C4D call is `D3DXAssembleShader(buf, len, 0, 0, Flags=4, &shader, 0)`; 4 = D3DXSHADER_SKIPOPTIMIZATION (D3DCOMPILE_SKIP_OPTIMIZATION in d3dcompiler; 1 = DEBUG, 2 = SKIP_VALIDATION). The modern assembler (d3dcompiler_47, and d3dx9_43) accepts only DEBUG / SKIP_VALIDATION for assembling and fails any other bit with D3DERR_INVALIDCALL 0x8876086C and a NULL error blob. Wine (vkd3d) ignores the bit. An assembler has no optimiser, so the bit is meaningless there.
* Exe: static D3DX (not precompiled tokens); the text is composed by 0x75F0B0 and assembled at run time; the same bytes go to CreateVertexShader (0x7FAC60). The rw_skin_vs_test assembler section runs the exe's own D3DXAssembleShader in the oracle and compares bytecode.
* Flags 0 is NOT enough: with validation on, the current assembler rejects shaders the old D3DX accepted (1096 rejections in the 400 x 3 + 2.2 M differential: X5702 "input register v4/v5/v6 not declared", X5326 "read of uninitialised component", X5349 ">128 slots" in vs_1_1). The exe's composer emits such shaders for some keys. **Fix: flags = 2 (SKIP_VALIDATION)** -> assembler differential 8987 texts: 8987 byte-identical to the exe's D3DX, 0 different, 0 accepted by only one.
* Silent null removed: every failed assemble, a failed CreateVertexShader (HRESULT) and a missing device now log a `skinvs:` line to stderr (even when there is no error blob); AssembleToBytes logs too (the test's smoke/differential then show the reason).
* Native CreateVertexShader still rejects (0x8876086C) 45/400 realistic keys for vs_2_x / vs_2_0 and 55/400 for vs_1_1: exactly the keys whose text the validating assembler rejects (45 = 45, 45 = 45, 55 = 55; probe on a HAL device: `vs_2_x` / `vs_1_1` reading an undeclared v5 is rejected, declaring it fixes it; a vs_1_1 reading r5.w never written is rejected). Class (c): the exe produces the same bytes (assembler differential), so the same keys fail in the exe on this runtime (the XP-era runtime was lenient). rw_skin_vs_test device section now fails only on a rejection of a key whose text the validating assembler accepts ("unexplained"), prints the (c) count; it still catches port bugs (a text difference shows in the 2.2 M-key text differential, an assembler/flags regression in the device accept count).
* Game path: `GetShader` -> `SetVS(vertexShader)` with no null check, exactly the exe's 0x761030; there is no CPU fallback when the VS is null (the exe has none either: the CPU route is chosen only by the capability predicate IsHardwareSkin = HW T&L, VS/PS >= 1.1, bones <= (MaxVSConst-12)/3, hierarchy present). On this GPU (VS/PS 3.0, 256 constants) the predicate chooses the VS route for peds; with the fix the VS exists (rw_skin_pipeline_test: male01, HW route 52/52, HW vs CPU 0 differing pixels in all poses; before: 3 FAIL, 92 of 933 px differ). A game soak after the fix was NOT run (the Release build dir is ~1100 steps behind HEAD; not rebuilt on the shared PC). Residual risk: a ped/vehicle key in the (c) set draws with a null VS exactly as in the exe; grep `skinvs: CreateVertexShader rejected` in standalone.log of the next soak to see whether the game hits any.

## 2. The other three
### 2a rw_platform_test "two resets in one showRaster" (b)
Cause (proved with printf probes in the fakes): the native d3d9.dll's `Reset` calls `TestCooperativeLevel` through the object's vtable itself (Wine's does not); the test's armed fake NOTRESET answer was consumed inside the interval-change Reset, so the lost-device path of showRaster found a healthy device and only one Reset happened (log: FakeTestCoop, ProxyReset, FakePresent(lost), FakeTestCoop(real), ...). Fix in the test: the NOTRESET answer only becomes available after a faked Present returned DEVICELOST (the D3D9 contract: lost -> NOTRESET), so internal calls cannot consume it. Same assertions kept (2 resets, 2 callbacks, self-clearing callback not called again). Port unchanged (the ProxyReset/in-place vtable patch already handled the native class vtable).

### 2b rw_im2d_im3d_test "tri LTM scale 5" (b)
The check compared the 5x-scaled bbox with 5 x the MEASURED bbox of the small base triangle: base 28 px (true 30.4: the rasteriser's fill rule quantises a 30 px bbox), scaled 150 px (true 152): 140 vs 150 > 4. Fix: compare with the analytic size W/2 x H/2 (tolerance 4; a wrong LTM scale is off by tens of pixels, the base-size check at line 278 and the pixel-count check are unchanged).

### 2c rw_custom_pipelines_test infernus env map (b, native FF semantics)
The test loads infernus.dff without vehicle.txd: the geometry is flagged textured but `material->texture` is NULL, so the car pipeline (exe 0x5D9900 -> 0x5DA6A0) binds a NULL texture to stage 0 with COLOROP=MODULATE(TEXTURE, DIFFUSE). On native d3d9 a stage that references a missing texture ends the whole texture-blend chain, so the env stage 1 (MULTIPLYADD) never ran: env-only frame identical to plain (0 px; confirmed with a hooked DrawIndexedPrimitive: stage states correct, ValidateDevice OK, VS/PS null, even replacing stage 1 by ADD TFACTOR changed nothing; the same states with a white base texture give 12080 differing px). wined3d continues the chain. The exe would behave the same with an untextured car; in the game the textures exist. Fix: the test gives every material of the loaded clump a white 16x16 base texture (modulating by white leaves plain/spec frames identical: plain brightness unchanged 14449758). Port unchanged.

## 3. Final run (GPU lock for the device set only; the oracle set ran unlocked)
See the table appended below.

| test | checks / failures / mismatches |
|---|---|
| rw_custom_pipelines | 137 / 0 / 0 |
| rw_engine | 97 / 0 / 0 (101 in WIN-TEST: per-video-mode checks, the mode list is 49 modes now, desktop 2560x1600) |
| rw_frame_camera | 118 / 0 / 0 |
| rw_im2d_im3d | 117 / 0 / 0 |
| rw_matfx_pipeline | 273 / 0 / 0 |
| rw_pipeline | 541 / 0 / 0 |
| rw_platform | 55 / 0 / 0 |
| rw_raster_image | 325 / 0 / 0 |
| rw_renderstate | 373 / 0 / 0 |
| rw_skin_hw_oracle | 27800 / 0 / 0 |
| rw_skin_pipeline | 52 / 0 / 0 |
| rw_skin_vs (device part: 45/45/55 native rejections, all explained) | 8987 / 0 / 0 (221 s) |
| rw_texture | 103 / 0 / 0 |
| audio_fade_oracle 6000, fixed_oracle 16, game_oracle 62, gameplay_oracle 311, logic_oracle 224, physics_oracle 107, pool_oracle 1900009, review_oracle 42, world_oracle 116, x87_oracle 67 | all 0 / 0 (identical to WIN-TEST) |
| rw_atomic_clump 274, rw_matfx_uvanim 151, rw_math_stream 139, rw_rtanim_rtquat 4835, rw_skin_cpu_oracle 240000, rw_skin_hanim 166, rw_strip 46, rw_world_geometry 166 | all 0 / 0 |
| script_oracle base/g13_15/g16..g25_26 | 80/47/47/17/51/56/31/75/58/70/58, all 0 / 0 |

**42 of 42 test targets green** (`ninja -t targets` lists 42 `*_test` targets at HEAD; WIN-TEST said 43, an off-by-one in its count: 38 + 5 = 43 rows were counted). Run as two `run_all_tests.sh` invocations (SKIP_BUILD=1 after one `-j4` BelowNormal ninja of all tests): the 13 device tests under the GPU lock (~4 min hold, skin_vs 221 s), the 29 oracle/script/non-device tests unlocked; the oracle rows did not move.

## Notes / leftovers
* Not done: a game soak with the fix (see 1, game path); `grep "skinvs:" standalone.log` in the next soak shows whether any game key is in the (c) set.
* tests that opened a hooked vtable (platform) restore the slots; scratch probes (vsprobe, dumps) live in C:/gta-scratch/rwgpu, nothing committed there.
* WIN-TEST section 3 said the VS route was dead on native Windows and suggested flags 0 or 2: flags 2 is the right one (0 re-introduces 1096 validation rejections that the exe's D3DX does not make).
