> CORRECTION 2026-10-10 (from the implementation): the exe takes the vertex-shader path ONLY when `skin->numMeshes != 0` (split-skin data) AND a hierarchy exists; SA ped DFFs have numMeshes == 0, so all peds are CPU-skinned (SSE/x87 kernels 0x7CAAD0..0x7CAF50, bones 0x7CB390) and drawn with the stock FF render callback 0x756DF0. Section 1 below is superseded on that point.

# P2B skin pipeline: what the exe installs vs what the shim gets from librw (review 11x-4, date 2026-10-09)

Question: does the game render skinned peds through fixed-function indexed vertex blending (`D3DRS_VERTEXBLEND` + `D3DTS_WORLDMATRIX(i)`) or a shader, and does
librw's `d3d9::makeSkinPipeline` match? All addresses are from `gta_sa_compact.exe`.

## 1. What the exe does (answer: neither FF vertex blending; a run-time-assembled vertex shader, or CPU skinning as fallback)

* `RpSkinPluginAttach` 0x7C6820 -> `_rpSkinPipelinesCreate` 0x7C8980 -> 0x7CB2B0: queries `RwD3D9GetCaps` (0x7FAD20) and stores
  `0xC978E0` = DevCaps `HWTRANSFORMANDLIGHT`, `0xC978E4` = (VertexShaderVersion & 0xFFFF) >= 0x101 && (PixelShaderVersion & 0xFFFF) >= 0x101,
  `0xC978EC` = MaxVertexShaderConst, `0xC978E8` = (MaxVertexShaderConst - 12) / 3 (= max bones in constants). Then it creates ONE RxPipeline (pluginID 0x116 =
  skin, pluginData 1) and stores it in `0xC978C4`; `RpSkinAtomicSetType` 0x7C7830 -> 0x7C89B0 just writes that pipeline into `atomic+0x6C` for every type
  (types MatFX 2 / Toon 3 collapse to generic 1 when the plugin is missing).
* Per geometry the instance/render callbacks pick the path with the predicates 0x7C89C0 / 0x7C8A00 (HW T&L && VS/PS >= 1.1 && skin->numUsedBones <= 0xC978E8):
  * HW path (0x7C8060, bone matrices from 0x7C78A0): the bone matrices (3 float4 per bone, 12 floats, transposed) go to the vertex-shader constants through
    `SetVertexShaderConstantF` (device vtable +0x178) -- constants c0.. = transforms/material, light constants, then 3 per bone; meshes with a bone split
    (`rleCount`/`remapIndices`) upload only the bones of the mesh. The vertex shader is NOT precompiled: 0x7609xx..0x760Cxx concatenates vs_1_1 text
    (`dp3 ...`, `mad oD0.xyz, r1, c[5].z, c[5].z`, `mov oT%u.xy, v%u` strings at .rdata 0x8D641C..0x8D67EC), runs D3DXAssembleShader (0x7652AE) and
    `CreateVertexShader` (0x7FAC60, the only CreateVertexShader call in the exe). Lights are evaluated IN the shader from the RW light list; a budget loop
    (0x7C8221..0x7C8295) drops lights while `lightConsts(0x75EDD0) + 3 * numBones > MaxVertexShaderConst`.
  * Fallback (no HW T&L / old VS): CPU skinning (0x7CAAD0 / 0x7CAB80 / 0x7CAC60 / 0x7CAF50 SSE, 0x7CA330 / 0x7CA410 / 0x7C9DA0 x87; chosen by `[0xC980A4]`, SSE
    available) into a dynamic VB, then the normal fixed-function draw. Dispatch by `skin->numWeights` (>2: 4 weights, 2: 2 weights, 1 with 1 used bone: rigid copy
    with one matrix, 1 with several bones: one weight per vertex).
* Bone matrix convention (0x7C78A0 = 0x7CB390 SW twin): `LOCALSPACEMATRICES`: bone = skinToBone[i] x hierMatrix[i]; else bone = skinToBone[i] x (hierMatrix[i] x
  inverse(atomic LTM)). Same as librw `uploadSkinMatrices`.

## 2. What the shim gets (librw)

`rw::Skin::setPipeline` -> `skinGlobals.pipelines[D3D9]` = `d3d9::makeSkinPipeline()` (d3d9skin.cpp): `skinInstanceCB` builds a position/normal/prelit/uv VB + 4 weights
+ 4 indices per vertex; `skinRenderCB` calls `lightingCB_Shader` (librw's own lighting: ambient + directional [+ point/spot] from the RW world lights, 3 variants
skin_amb / skin_amb_dir / skin_all, precompiled `shaders/skin_*_VS.h`), uploads `skinMatrices[64*16]` (max 64 bones), sets one pixel shader (default / default_tex), no
fog / alpha-test / specular states beyond librw's defaults. All 4-weight skinning, no mesh bone split, no CPU path.

## 3. Differences that can show on screen (not verified pixel-wise, no GPU diff possible under Wine)

1. Lighting model: the exe shader reproduces RW's FF-like lighting for the lights of the clump's world (including the budget-driven light dropping and the
   `D3DRS_AMBIENT`/material-colour handling of the 07 facade's default lighting callback, exe 0x756DF0 port in `pipeline.cpp`); librw's `lightingCB_Shader` is a different
   evaluation (own ambient/diffuse combination, no light dropping, shader constants instead of FF lights). Peds/vehicles' non-skinned parts use the 07 facade (FF) so a
   skinned head next to a rigid hat can differ in brightness.
2. Render-state side effects: the exe's HW callback toggles vertex alpha through 0x7FE0A0(material alpha != 255 || instance has vertex alpha) -- librw's
   `skinRenderCB` does the same (`SetRenderState(VERTEXALPHA, ...)`) -- and binds the material texture through `RwD3D9SetTexture`/`RwD3D9SetSurfaceProperties`; librw uses
   `d3d::setTexture` + its default pixel shaders (anisotropy / addressing come from the librw texture and sampler cache, so equal; material ambient/diffuse/specular
   `surfaceProps` go through the shader constants instead of the FF material).
3. Fog: librw's skin VS writes the fog factor itself (`TexCoord0.z = clamp((w - fogEnd) * fogRange, fogDisable, 1)` from `fogData`, c14), the exe's shader leaves fog to the
   device (FF table/vertex fog states set by the game's `RwRenderStateSet(rwRENDERSTATEFOG*)`). UNVERIFIED: that the shim's renderstate.cpp feeds the same distances to librw's
   `fogData` (fog start/end/colour/enable) as it sets for the FF device state; if not, skinned actors would fog differently from the world.

## 4. Options (scope: not done, > 1 h with a visual check)

A. Keep librw's skin pipeline (current). Cheapest; the risk above is lighting/fog parity only.
B. CPU-skin variant on the 07 facade (recommended if A looks wrong): an `ObjPipeline` subclass in `pipeline.cpp` whose instance callback is the facade's default
   one, and whose render callback (1) computes the 12-float bone matrices as in 0x7C78A0 (above, `skinToBone x hier [x invLTM]`), (2) transforms position (and normal
   with the rotation part) of the 4 weighted bones per vertex into a dynamic VB (`D3DLOCK_DISCARD`) -- weights/indices come from `rw::Skin` (float[4]/uint8[4], already
   sorted like the exe by `Skin::sortLikeSA`), (3) calls the facade's default render callback (exe 0x756DF0 port) on the temporary VB. This is exactly the exe's
   fallback path, so lighting/fog/alpha parity with the rest of the actors is by construction; cost = CPU skinning of ~2-4k vertices per ped per frame (the original did
   the same on SW fallback; fine for a modern CPU). Hook: `RpSkinAtomicSetType` / `rw::Skin::setPipeline` -> own pipeline; keep `pluginID = ID_SKIN`, `pluginData = 1`
   so `RpSkinAtomicGetType` stays correct.
C. Port the exe's HW path (0x7C8060 + vs_1_1 text generator 0x7609xx) 1:1: large (~2 kLoC of asm, needs D3DX assembler or hand-written bytecode); only worth it for a
   pixel-exact goal.
