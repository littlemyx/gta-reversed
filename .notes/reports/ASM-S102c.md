# ASM-S102c (Clouds + Mirrors, tier B) - 2026-10-30
Result: 12 rows, 9 compared (6 FIXED, 3 MATCH), 3 UNSURE not compared (Clouds Render, RenderSkyPolys, RenderBottomFromHeight: budget). Table in S102c.result.tsv.

Fixes (exe range, one line each):
- b336bfaae / 7f0501780 CMirrors::RenderMirrorBuffer (0x726090-0x72669F): the proximity centre of the Z-only quad is (216,-42,1003) (the constants at 0x8729EC sit z,y,x); the port used (1003,-42,216). The four render states set before that quad (ZWRITE on, no vertex alpha, src ZERO, dst ONE) were missing.
- 501534e7a / 7f0501780 CMirrors::BeforeConstructRenderList (0x726DF0-0x727139): 8-track centroid summed z0+z1+z3+z2 / y0+y1+y3+y2 / x0+x1+x2+x3 unspilled.
- 8881d1be8 CClouds::VolumetricClouds_Create (0x715F40-0x716379): rand order is alpha, size, THEN position (port drew the position first); the 5-cloud offsets are an integer range [-N,N), N=ftol(r*3).
- 6a7dee0a1 CClouds::VolumetricCloudsRender (0x716380-0x716B0F): medium FX density 0.66 (port 1/3); removed `FindPlayerPed()->GetMoveSpeed() = 0` every frame (no such store in the exe); bJustCreated cleared on strict compares; fader NaN arms; z,y,x unspilled distance/dot/wind/vertex sums.
- 00a477c35 / 7f0501780 CClouds::MovingFog_Update (0x716B10-0x716C8F): z,y,x unrounded distance, NaN fades in.
- f618b7f61 / 55771cc62 CClouds::MovingFogRender (0x716C90-0x7170B4): early return is `currArea != 0 || playerArea != 0` (port &&); corner sums and alpha as unspilled chains.
- cf06ebf9c CRegisteredCorona::CalculateIntensity: exe fade `z < half ? 1 : 1 - (z-half)/half` instead of clamp(invLerp); no callers.

Verified twice against the asm (no change): MovingFog_Create (all ranges, rand order, array layout), BuildCameraMatrixForScreens (jump table 0-3/10-13/22-25, 19 constants decoded from the immediates, arithmetic), BeforeMainRender, RenderMirrorBuffer main/fudge quads (states, vertex order, uv tables, index table 0,1,2,0,2,3), CClouds::VolumetricCloudsRender quad colours / vcClr / flush args.
Open (not compared): CClouds::Render (only entry states checked), RenderSkyPolys, RenderBottomFromHeight.
