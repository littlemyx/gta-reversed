# ASM-S101 (CShadows, CStencilShadowObject, CStencilShadows) - 2026-10-30
Result: 35 rows: 12 FIXED (9 commits), 20 MATCH, 2 UNSURE (RenderForVehicle / RenderForObject: float sums of the side quads not compared line by line), 1 NOT_REACHED. Table in S101.result.tsv.

Fixes (exe address, one line each):
- StoreRealTimeShadow 0x707CA0: CalcPedShadowValues got its refs as (displacement, front, side) instead of the exe's (front, side, displacement), so the real-time shadow orientation/offset used swapped values; the light direction was not normalised; CRealTimeShadow::DrawBorderAroundTexture(0,0,0,255) was never called; the distance test also rejects == 225 and NaN. Light angle arg 2 is unused by SetLightProperties (only changed to the exe's expression).
- RenderStoredShadows 0x70A960: the shadow rect was posn..posn+span (exe: posn +- span); sector range now x*0.02f+60 clamped to 0..119 (y outer); the render-state block runs without an early-out; the type/texture states are only set for non-batched shadows (the water pass of a batched shadow inherits the previous states); water pass rewritten: cell range floor/ceil((pos+-span)/2)*2 (the old call was an empty/flipped loop), x outer / y inner, 4 distinct vertices (the old code wrote all into one slot), U=(dot*recip+1)*0.5, V=0.5-dot*recip*0.5 (was +1 and divide), signed intensity.
- StoreShadowForPole 0x70C750: static shadow id is entity + localId + 0x33 (port: LOD + 3, so poles of one LOD shared an id); position sum order (offset + position) + front.
- CastShadowEntityXY 0x7086B0: plane distance is z*nz + y*ny + x*nx (port summed x,y,z); NaN clip side counts as outside.
- CastPlayerShadowSectorList 0x70A470: strict rect edges (CRect::OverlapsWith was non-strict), colmodel via CModelInfo; NaN tilt / z-extent skip the entity (also in CastShadowSectorList 0x70A630 and CastRealTimeShadowSectorList 0x70A7E0).
- StoreCarLightShadow 0x70C500: facing test returns for dot >= cosine and NaN, NaN distance returns, fade scale is x * float(1/9) instead of x / 9.
- StoreShadowForVehicle 0x70BDA0, StoreShadowForPedObject 0x707B40, RenderIndicatorShadow 0x70CCB0, RenderStaticShadows 0x708300 (movsx intensity): NaN / signedness only.

Open: RenderForVehicle 0x70FAE0 and RenderForObject 0x710310 (UNSURE, see table). SunSetPositionFromEntity 0x710AF0 has no caller in the exe (port body differs, unreachable).
