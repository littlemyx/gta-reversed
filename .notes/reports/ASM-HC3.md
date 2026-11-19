# ASM-HC3: executed-but-unverified functions, root gameplay files in source/game_sa

Zone rows (hit-count tsv, exe_addr != "-", verdict NONE, top-level source/game_sa/*.cpp): 513; 448 after dropping rows already covered by .notes/asm_sweep/*result*.tsv.
Compared: 49 (all MATCH, 0 fixes, no source changed, no build needed). NOT_REACHED: about 330 (budget), including CCurves::CalcSpeedScaleFactor/CalcCurvePoint, CCamera::SetColVars*, CPedGeometryAnalyser, CDamageManager rest, IplStore rest, PathFind rest, Garages rest, Weather, World.
SKIP-RENDER: Occluder/ActiveOccluder/Occlusion, Coronas, PointLights, Radar, Font, Sprite, VisibilityPlugins, Hud, Messages, Renderer, Shadows, Clouds, Grass, Skidmarks, Water, TimeCycle, MenuManager Stretch, CStreaming::RenderEntity, CCamera::RenderMotionBlur.
Details: .notes/asm_sweep/HC3.result.tsv. Assumption: x87 PC24 (D3D) for float chains, as in earlier slices.
