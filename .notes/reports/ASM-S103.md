# ASM-S103 (phase 3 tier B: CPostEffects, CRegisteredMotionBlurStreak, CShinyTexts, CSpecialFX) - 2026-10-30
Result: 36 rows compared, 26 MATCH, 10 FIXED (15 commits), 0 UNSURE. Table in S103.result.tsv.
Fixes (exe range, one line each):
- b03e85e0a Initialise (0x704630): grain raster fill ran 16384 iterations instead of 0x10000.
- e389f020a NightVision (0x7011C0): switch-on counter min() instead of max(count-step,0): flash never ran.
- aeab18d17 UnderWaterRipple (0x7039C0): vertex colour ABGR instead of ARGB (r/b swapped).
- 006f67017/5a000786b InfraredVision (0x703F80): W+(xy+xy) association; DrawQuadSetUVs vertex mapping (3rd pair -> vertex 3, 4th -> 2) so DrawQuadSetDefaultUVs stops swapping vertices 2/3 (left stale UVs for later textured quads).
- d9d8d561c Grain (0x7037C0): UV tiling 1.5 stretched only for width > 640.
- ac82ae22e/ebed7f097/50c9ec3a8 Render (0x7046E0): colour passes clamp to [0,255] before ftol (was uint8 wrap in dark areas); rain strength never overshoots; script heat haze and underwater arms call HeatHazeFX(1,false); water depth factor 1.0 when switch off; xoffset association; photo wait flag reset to false.
- c447d354a ShinyTexts::Render (0x724890): vertex alpha 0xFF (was 12). bd85ebba8 RegisterOne (0x724B60): z,y,x distance sum, NaN arm.
- cce39ba19 AddWeaponStreak (0x7233F0): TransformPoint for both points; uninitialised RwMatrix* write removed.
- df4ae783a/bfa59677a Render2DFXs (0x721660): video cam text integer (W*8)/10; scanline band ftol before -70; lift-cam rects rand()%W, %H and rect order; snapshot flash fade *0.65.
Not exhaustive: Radiosity compared on the strip-copy default path (remainder loops / non-strip path skimmed); Fog re-read after SYSTEMIC-9; SetupBackBufferVertex error arm not compared.
Checks: PortableWin64 compile of the 3 touched TUs (rc 0, PCH disabled, main build's compile command re-pointed at the worktree); v6 pre-commit grep clean.
