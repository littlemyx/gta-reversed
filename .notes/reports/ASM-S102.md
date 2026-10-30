# ASM-S102 (+ S94b) - 2026-10-30
S94b: 2 of 2 compared: FxEmitterBP_c::RenderHeatHaze MATCH, FxEmitterBP_c::Render FIXED. Table in S94b.result.tsv.
S102 (40 rows, tier B): 16 compared (12 MATCH, 4 FIXED), 24 UNSURE (not compared, budget). Table in S102.result.tsv.

Fixes:
- 4b26a45d9 FxEmitterBP_c::Render (0x4A2C40-0x4A3E9F): the colour brightness multiplier is stored by the exe back into the `brightness` argument slot (`[esp+0x204]` is arg2 once `push ebp` ran; the `doHeatHaze` flag is at +0x208). Particles with `m_Brightness > 100` therefore use the previous multiplier, or the `brightness` argument (`balance * 0.6 + 0.4` from FxManager) for the first one. The port started at 0.0 (black particles) and its comment blamed the doHeatHaze slot.
- 2c03aac37 CCoronas::Init (0x6FAA70): texture is read only for non-empty sprite names; `NumCoronas` (0xC3E038) is reset to 0.
- be19f803d CMotionBlurStreaks::RegisterStreak (0x721DC0): a freshly taken slot clears render history [1] and [2] (`mov word ptr [slot+0x51], 0`); the port cleared [1] only, leaving a stale [2].
- 809583e7c CMirrors::CreateBuffer (0x7230A0): quality >= 1 tries 1024x512 colour+Z; on either failure the exe falls through to the unchecked 512x256 pair (port left the buffer null). CMirrors::BuildCamMatrix (0x723150): the exe also normalises the right vector after the cross product.

Verified twice against the asm (no change needed): RenderHeatHaze (states 1/5/2, cross-product association, rotation loops, size, raster-select stale case, quad corners/tri uv order); the Render pieces listed in the table (smoke clamps with their jp NaN arms, trail mode 1/2 vector math incl. prevCam at 0xB6FA14, flat/cam matrix copies, RANGE/BRIGHT colour with `fimul`, ftol of r,g,b,a, texture select).
Not changed: CCoronas::Init clears the whole corona struct, the exe only the ids (harmless: inactive slots are reinitialised on registration).
Open (not compared): Coronas RenderOutGeometryBufferForReflections / Render / RenderReflections / RenderSunReflection; Clouds Render, RenderSkyPolys, RenderBottomFromHeight, MovingFog_Create/Update, MovingFogRender, VolumetricClouds_Create, VolumetricCloudsRender; Mirrors RenderMirrorBuffer, BuildCameraMatrixForScreens, BeforeConstructRenderList, BeforeMainRender; BrightLights RenderOutGeometryBuffer, Render, RegisterOne; Occlusion IsPositionOccluded, OccluderHidesBehind, ProcessBeforeRendering; PointLights ProcessVerticalLineUsingCache, RenderFogEffect.
Gate: Release + StandaloneRW build OK; wave2_oracle_test PC24 0/0, PC53 0/0 (1142 fns); combat PASS=14; PortableWin64 compile of FxEmitterBP/Coronas/Mirrors/MotionBlurStreaks 0 errors; pre-commit grep clean.
