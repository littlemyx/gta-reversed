# ASM-S102b (Occlusion / PointLights / Coronas / BrightLights compared; Clouds, Mirrors left open) - 2026-10-30
Result: 24 rows, 12 compared (8 FIXED in 7 commits, 4 MATCH), 12 UNSURE (Clouds x8, Mirrors x4: not compared, budget). Table in S102b.result.tsv.
Fixes (exe range, one line each):
- 797c14757 COcclusion::IsPositionOccluded (0x7200B0-0x7201A9): depth test strict with signed int16 dist; long edge `scaleX > scaleY ? scaleX : scaleY`.
- 2322ce241 COcclusion::ProcessBeforeRendering (0x7201C0-0x7204BA): interior occluders are processed only when the outside can NOT be seen (the port did it always, mixing map and interior occluders); the far-away list is walked before the nearby list also when a walk is in progress (port reversed it); a full active array is skipped; occluded occluders are erased like the exe (shift down + re-examine the same index, signed int16 distance compare).
- 841646bf9 CPointLights::RenderFogEffect (0x7002D0-0x700BDC): NaN-skipping tests, (z+10)-20 line end, z/x/y dot and z/y/x length association, x*(1/12) / x*0.2, point branch uses the rounded 2D distance and (d*(1/size))^2, sprite angle from the exe double constants.
- f58ade5f8 CCoronas::Render: exe fade formula, integer half raster, strict NaN-skipping tests.
- e126aff1a CCoronas::RenderReflections: height-above-ground test used the bool flag (never skipped); skips empty / no-reflection coronas; integer colour multiplier; bikes use the plain matrix; width association; InitSpriteBuffer.
- 3a4186a6e CCoronas::RenderSunReflection: 3.333f scale (was 1/0.3f), 4th spot radius 350 and *0.01f ramp, NaN returns, exe vertex sum association.
- 27caea7d7 CBrightLights::RegisterOne/Render: z,y,x distance, NaN reject, exe rnd formula (ftol((rand&0xFF)*0.2)), alpha ramp *(1/15), vertex sum association.
MATCH: OccluderHidesBehind, ProcessVerticalLineUsingCache, Coronas::RenderOutGeometryBufferForReflections, BrightLights::RenderOutGeometryBuffer.
Open (not compared): Clouds Render, RenderSkyPolys, RenderBottomFromHeight, MovingFog_Create/Update, MovingFogRender, VolumetricClouds_Create, VolumetricCloudsRender; Mirrors RenderMirrorBuffer, BuildCameraMatrixForScreens, BeforeConstructRenderList, BeforeMainRender.
Notes: CRegisteredCorona::CalculateIntensity (RegisteredCorona.cpp, another slice) still uses invLerp/clamp; the sweep code no longer calls it. RenderReflections keeps the port's debug settings (s_DebugSettings). FogAngle takes a `double` on purpose (exe fmul qword constants); the portable branch uses fp::Round24Mul.
