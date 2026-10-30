# ASM sweep phase 3 plan (2026-10-30, branch reverse/interior_c)

Input: `COVERAGE.md` (audit), recomputed against the tree at `origin/reverse/interior_c` with S01..S80 result files present. Generator: scratchpad `p3_hooks.py` + `p3_an.py` (the audit scripts, root = a worktree of this branch) + `p3_cut.py`. Addresses normalised to integers (5-8 hex digits).

## Recomputed counts

| what | count |
|---|---|
| hooked addresses (incl. Cheat.cpp table) | 9492 |
| covered by sweep verdict MATCH/FIXED/DEVIATION | 2857 |
| covered by oracle only (address literal in `tests/**`) | 5068 |
| pending rows in S70c.tsv / S76b.tsv (no result file yet) | 36 |
| latest verdict NOT_REACHED with nothing later (carry-over, not sliced) | 10 |
| **uncovered** | **1521** |
| of which REPLACED by the port platform layer (not sliced) | 94 |
| of which sliced into phase 3 (S81..S117) | 1427 |

Tier split of the 1427 sliced rows: **A = 546, B = 599, C = 282**.

Carry-over NOT_REACHED rows (need a trigger/harness, not another asm read): 0x4D5AF0 CCutsceneMgr::RemoveEverythingBecauseCutsceneDoesntFitInMemory, 0x5EF710 CPedAttractorManager::RestoreStuffFromMem, 0x5F7FE0 PedGroupMembership::From, 0x698790 CCover::FindVectorFromFirstToMissingVertex, 0x6987F0 CCover::CalculateHorizontalSize, 0x698990 CCover::DoLineCheckWithinObject, 0x69DC50 CMessages::CutString, 0x6FDC00 CCustomCarPlateMgr::LoadPlatecharsetDat, 0x73A8D0 CWeapon::LaserScopeDot, 0x746A10 CPad::ProcessPad.

## Tiers

- **A** (player-visible logic): audio entities/managers/engine (what plays and when, radio choice, police scanner, ambience), data loaders (FileLoader IDE/IPL/DAT, IplStore, ColStore, CFileMgr, CDirectory, AnimManager/AssocGroup ifp loading, audio bank/track lookup loaders, IniFile), gameplay-coupled FX (Fx_c, FxManager, FxSystem, emitters, BP managers).
- **B** (visible values / render setup): HUD, menu, text, loading screen, model infos (vehicle colours/dummies), shadows, coronas, lights, post effects, plants/procobj, renderer/visibility, fx blueprint infos, anim internals, app loop, core containers.
- **C** (platform / draw call / plugin / backend): DirectSound channel+hardware backend (runs unchanged on the dsound_al OpenAL shim), Vorbis/Wave decoders, sprite/font/RW helpers, pipelines, plugins, debug, misc. Last slices. Every phase-3 TSV has a 7th column `tier` (A/B/C).

## Slices

TSV format = `S78.tsv` plus the 7th column `tier`; `oc` = U. Ordered A, then B, then C.

| slice | rows | tier | groups | files |
|---|---|---|---|---|
| S81 | 41 | A | A1 audio entities | AEPedAudioEntity(13), AECollisionAudioEntity(11), AEGlobalWeaponAudioEntity(6), AEDoorAudioEntity(4), AEFireAudioEntity(4), AEFrontendAudioEntity(3) |
| S82 | 44 | A | A1 audio entities | AEPedSpeechAudioEntity(29), AEPoliceScannerAudioEntity(8), AEPedlessSpeechAudioEntity(7) |
| S83 | 47 | A | A1 audio entities | AEVehicleAudioEntity(34), AEScriptAudioEntity(10), AETwinLoopSoundEntity(3) |
| S84 | 36 | A | A1 audio entities | AEVehicleAudioEntity(34), AEWaterCannonAudioEntity(2) |
| S85 | 33 | A | A1 audio entities, A2 radio/track managers | AEWeaponAudioEntity(16), AEAudioUtility(6), AESound(6), AEAudioEnvironment(3), AEWeatherAudioEntity(2) |
| S86 | 38 | A | A2 radio/track managers | AudioEngine(38) |
| S87 | 24 | A | A2 radio/track managers | AEAudioHardware(14), AEAmbienceTrackManager(5), AECutsceneTrackManager(5) |
| S88 | 43 | A | A2 radio/track managers | AERadioTrackManager(35), AESoundManager(8) |
| S89 | 46 | A | A2 radio/track managers, A3 data loaders | AnimManager(22), AEUserRadioTrackManager(15), AnimBlendAssocGroup(9) |
| S90 | 40 | A | A3 data loaders | ColStore(15), Directory(10), AEMP3TrackLoader(6), AEBankLoader(4), AEMP3BankLoader(4), AETrackLoader(1) |
| S91 | 24 | A | A3 data loaders | FileLoader(24) |
| S92 | 42 | A | A3 data loaders | FileLoader(24), FileMgr(17), IniFile(1) |
| S93 | 43 | A | A3 data loaders, A4 fx systems | Fx(22), IplStore(18), FxEmitter(3) |
| S94 | 45 | A | A4 fx systems | FxSystem(14), FxManager(13), FxEmitterBP(7), FxInfoManager(4), FxSystemBP(4), FxSphere(2), FxPrimBP(1) |
| S95 | 30 | B | B1 HUD | Hud(25), HudColours(5) |
| S96 | 45 | B | B2 menu | MenuManager(25), LoadingScreen(20) |
| S97 | 34 | B | B2 menu | MenuManager(25), MenuSystem(9) |
| S98 | 37 | B | B2 menu, B3 model info | Text(13), ClumpModelInfo(6), BaseModelInfo(5), ModelInfo(5), PedModelInfo(5), AtomicModelInfo(1), DamageAtomicModelInfo(1) ... |
| S99 | 39 | B | B3 model info | VehicleModelInfo(38), WeaponModelInfo(1) |
| S100 | 35 | B | B4 shadows | ShadowCamera(19), RealTimeShadow(9), RealTimeShadowManager(7) |
| S101 | 35 | B | B4 shadows | Shadows(19), StencilShadows(13), StencilShadowObject(3) |
| S102 | 40 | B | B5 coronas/lights/postfx | Clouds(10), Mirrors(8), Coronas(7), BrightLights(4), MotionBlurStreaks(4), Occlusion(4), PointLights(3) |
| S103 | 36 | B | B5 coronas/lights/postfx | PostEffects(23), SpecialFX(7), ShinyTexts(4), RegisteredMotionBlurStreak(2) |
| S104 | 43 | B | B6 plants/procobj | PlantMgr(14), ProcObjectMan(8), PPTriPlantBuffer(5), ProcSurfaceInfo(4), GrassRenderer(3), PlantLocTri(3), PlantColEntEntry(2) ... |
| S105 | 39 | B | B7 renderer/visibility | Renderer(18), TxdStore(14), StreamingInfo(7) |
| S106 | 41 | B | B7 renderer/visibility | VisibilityPlugins(41) |
| S107 | 46 | B | B8 fx blueprints | FxInfoFriction(3), FxInfoGroundCollide(3), FxInfoAnimTexture(2), FxInfoAttractLine(2), FxInfoAttractPt(2), FxInfoColour(2), FxInfoColourBright(2) ... |
| S108 | 45 | B | B8 fx blueprints, B8 anim internals | AnimBlendHierarchy(10), FxInfoJitter(3), FxInfoNoise(3), FxMemoryPool(3), FxInfoRotSpeed(2), FxInfoSize(2), FxInfoSmoke(2) ... |
| S109 | 42 | B | B8 anim internals, B9 app loop, B9 core containers | app_game(12), app(7), app_input(5), Matrix(4), AnimBlendSequence(3), app_light(3), AnimBlendStaticAssociation(2) ... |
| S110 | 12 | B | B9 core containers | QuadTreeNode(12) |
| S111 | 44 | C | C1 audio backend | AEAudioChannel(12), AEStreamThread(10), AEAudioHardware(8), AEStaticChannel(8), AESmoothFadeThread(6) |
| S112 | 37 | C | C1 audio backend | AEStreamingChannel(24), AEDataStream(11), AEStreamingDecoder(2) |
| S113 | 37 | C | C1 audio backend, C2 draw/RW helpers | Font(13), AEVorbisDecoder(11), AEWaveDecoder(6), RenderBuffer(5), Debug(1), Lines(1) |
| S114 | 36 | C | C2 draw/RW helpers | RwHelper(29), Sprite(7) |
| S115 | 42 | C | C2 draw/RW helpers, C3 pipelines/plugins | Sprite2d(21), CustomBuildingDNPipeline(13), CarFXRenderer(8) |
| S116 | 45 | C | C3 pipelines/plugins | RpAnimBlend(9), CustomBuildingPipeline(6), CustomCarEnvMapPipeline(6), BreakablePlugin(6), JPegCompress(5), CustomBuildingRenderer(4), CollisionPlugin(3) ... |
| S117 | 41 | C | C3 pipelines/plugins, C4 misc platform | platform(19), LoadMonitor(7), MemoryMgr(6), 2dEffect(5), Securom(2), common(2) |

## REPLACED (not sliced), 94 rows

Full list with addresses: `PHASE3_REPLACED.tsv`. These TUs are compiled out or swapped for `source/portable/*` / SDL3 / librw in the standalone and portable builds (see `source/CMakeLists.txt`), so an asm comparison of the hooked body would compare code the port does not run.

| file | rows | replaced by |
|---|---|---|
| app/platform/win/VideoMode.cpp | 4 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| app/platform/win/VideoPlayer/VideoPlayer.cpp | 6 | portable/app_video_player.cpp (VideoPlayer.cpp excluded from the portable build) |
| app/platform/win/WinInput.cpp | 5 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| app/platform/win/WinMain.cpp | 6 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| app/platform/win/WinPlatform.cpp | 1 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| app/platform/win/WinPs.cpp | 19 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| app/platform/win/WindowedMode.cpp | 3 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| app/platform/win/WndProc.cpp | 1 | SDL3 app layer (WinMain/WinPs/WndProc/WinInput/VideoMode/WinPlatform compile to SDL3 under NOTSA_USE_SDL3; WindowedMode.cpp removed with librw) |
| game_sa/Audio/Loaders/AEMFDecoder.cpp | 7 | portable/audio_decoders_portable.cpp (Media Foundation / WMA user-track decoders) |
| game_sa/Audio/Loaders/AEWMADecoder.cpp | 7 | portable/audio_decoders_portable.cpp (Media Foundation / WMA user-track decoders) |
| game_sa/CdStreamInfo.cpp | 9 | portable/cdstream_portable.cpp |
| game_sa/RenderWare/D3DResourceSystem.cpp | 21 | librw / GL3 render (D3D9 resource system excluded) |
| game_sa/RenderWare/rw/rtanim.cpp | 1 | RW wrappers forward to the exe RenderWare by address; replaced by fakerw/librw |
| game_sa/RenderWare/rw/rwcore.cpp | 4 | RW wrappers forward to the exe RenderWare by address; replaced by fakerw/librw |

Not treated as REPLACED (they still run, over the DirectSound API that `portable/dsound_al.cpp` implements on OpenAL): `Audio/Hardware/AE{Audio,Static,Streaming}Channel`, `AEStreamThread`, `AESmoothFadeThread`, `AEDataStream`, Vorbis/Wave decoders; tier C. `app/platform/platform.cpp` (Rs* wrappers), `Securom.cpp` and the AEAudioHardware init/DS rows are also tier C.

## Status 2026-10-30 22:50
All phase-3 slices done (S81-S117 incl. b/c/d/e/f continuations); INDEX has 0 pending. Remaining UNSURE rows only where noted in the per-slice reports (NaN-only arms, 1-ulp association, export-only paths).
