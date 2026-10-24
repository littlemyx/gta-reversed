# MAC-FTOL1: float -> int conversion class (slice F-5) - DONE (portable) - 2026-10-24
Rules v4 (Windows-only parts skipped). Branch portable/main, commits ba6af533 (tool+macro), 2e1301dc / batch 2 / eb9f876c (batch 3) / 914f5056 (World.h guard).

## What exists now
* `tools/portable/ftol_scan.py` (same clang JSON AST pipeline + stale-PCH guard as pc24_scan.py; header inline code is covered because the casts are found in headers reached from the TUs): lists every `FloatingToIntegral` cast of source/{game_sa,app,extensions} with class A/B, `--apply A --dir RE` rewrites class A sites (explicit C / static_cast / functional casts, nested ones too). Note: clang spells `(int)f` as an implicit f2i cast under a NoOp explicit cast; the tool rewrites the explicit node.
  Class A = result reaches a loop bound / array index / call or member named sector|grid|cell|pool|index|count|size... (directly or through a local, 4 levels) OR is a floor/ceil/+-3000/0.02f grid expression (heuristic "grid-expr": over-approximates, e.g. some timers in Tasks, harmless).
* `NOTSA_FTOL(T, expr)` (Base.h): Win32/other = `(T)(expr)` TOKEN-identical to the old spelling (an extra pair of parentheses changed MSVC's x87 scheduling in CWeather::AddRain: A/B caught it, the codemod strips a redundant outer pair); NOTSA_PORTABLE = `notsa::fp::FtolCast<T>(expr)` (Core/Fp.h): exe `_ftol2` low dword for int32/uint32 and the low 8/16 bits for narrow targets, `Ftol64` for 64 bit, `size_t` = 32 bit semantics (exe size_t), enums via underlying type. NaN/inf/|x|>=2^63 -> 0, 2^31..2^63 wrap (NOT "0x80000000": that is the low dword of cvttsd2si; fistp qword's indefinite is 0x8000000000000000 whose low dword is 0 - same as the existing fp::Ftol / MAC-F4a). Host test `portable.ftolcast` (1.8M checks vs an independent model, ASan/UBSan).
* `tools/portable/ftol_ab.py`: Win32 no-op proof (wine cl.exe, flags of build/StandaloneRelease, no PCH, A = git worktree of the base commit, B = tree; obj_code_sig code+relocs, NOTSA_SIG_NORM_STRINGS).

## Results
| Item | Result |
|---|---|
| Casts found (game_sa + app + extensions) | 1081 total: 155 class A (all rewritten), 926 class B (not touched) |
| Class A residual after the codemod | 0 (re-scan of the portable AST) |
| Win32 A/B (code + relocs) | batch 1: 35 TUs identical; batch 2: 19 identical; batch 3: 12 identical; World.h guard: World/Renderer/Streaming/Collision identical; Escalator (Base.h only) identical. First try: GangWars.cpp and Weather.cpp DIFFERENT (extra parens), fixed, then identical |
| PortableMac build | green (32 s full) |
| ctest -L portable | 19/19 pass (incl. new portable.ftolcast) |
| play1 s1 / s2 / lod_drive / lod_tp (caffeinate, load not high) | 4/4 exit 0 (32 / 65 / 57 / 65 s), "Loading the Game" present in all, no SIGSEGV/SIGBUS/abort |

## Span guards (NOTSA_PORTABLE only)
1. `CWorld::GetSectorX/Y/GetLodSectorX/Y` (World.h) clamp to [-1, MAX_SECTORS] ([-1, MAX_LOD_PTR_LISTS]): the FindObjects*/ray-cast sector loops of World.cpp (and every other user) have no clamp in the exe; with an exe-wrapped junk value they would run ~2^31 iterations or read `GetSector(x, y)` far out of bounds. -1 / MAX are the nearest out-of-map indices, so callers' `< 0` / `>= MAX` tests and max(0)/min(MAX-1) clamps behave as before (in-map values untouched).
2. Already there (MAC-ANIM1): PhysicalSectors.h `PhysicalLimitSectorSpan` (CPhysical::Add/RemoveAndAdd).
Not guarded (evidence-free): particle/count loops (AddWheelSpray, AddSplash..., ProjectileInfo, Weather, WaterLevel numX/numY, Plant AddObjects, PointLights fog): their inputs are finite world quantities; with the exe-wrap a huge value is as bad as in the exe. Candidates if a soak shows a hang: Fx.cpp:188/742, Vehicle.cpp:5661/5915, Object.cpp:867.
The two sites flagged by MAC-ANIM1: Shadows.cpp:1593 and EntityScanner.cpp:62 (both `floor(v*0.02f+60)` lambdas) are NOTSA_FTOL now; their results are clamped by the callers (max(0)/min(MAX-1)).

## Class A sites (155; all converted) by file: count, sink kind
| file | n | sinks |
|---|---|---|
| game_sa/AttractorScanner.cpp | 1 | grid-expr |
| game_sa/Audio/AEAudioEnvironment.cpp | 1 | index |
| game_sa/Audio/AEAudioUtility.cpp | 2 | grid-exprx2 |
| game_sa/Audio/Entities/AECollisionAudioEntity.cpp | 1 | grid-expr |
| game_sa/Audio/Hardware/AEAudioHardware.cpp | 1 | grid-expr |
| game_sa/Buoyancy.cpp | 1 | loop(iNumParticles) |
| game_sa/Cam.cpp | 5 | indexx3, grid-expr, loop(time) |
| game_sa/CarCtrl.cpp | 10 | grid-exprx8, loop(remaining), index |
| game_sa/Collision/Collision.cpp | 2 | grid-exprx2 |
| game_sa/CreepingFire.cpp | 2 | indexx2 |
| game_sa/Entity/Entity.cpp | 1 | index |
| game_sa/Entity/Object/Object.cpp | 7 | grid-exprx6, loop(nUsedSize) |
| game_sa/Entity/Ped/Ped.cpp | 2 | grid-exprx2 |
| game_sa/Entity/Ped/PlayerPed.cpp | 2 | grid-exprx2 |
| game_sa/Entity/Vehicle/Automobile.cpp | 1 | loop(numSmokes) |
| game_sa/Entity/Vehicle/Bike.cpp | 1 | grid-expr |
| game_sa/Entity/Vehicle/Plane.cpp | 2 | grid-exprx2 |
| game_sa/Entity/Vehicle/Vehicle.cpp | 4 | grid-exprx2, loop, loop(num) |
| game_sa/EntityScanner.cpp | 1 | grid-expr |
| game_sa/Escalator.cpp | 3 | member:m_NumIntermedia, member:m_NumBottomPlan, member:m_NumTopPlanes |
| game_sa/FireManager.cpp | 1 | grid-expr |
| game_sa/Formation.cpp | 1 | grid-expr |
| game_sa/Fx/Fx.cpp | 2 | loop(numParticles)x2 |
| game_sa/Fx/FxEmitter.cpp | 1 | member:m_MultSize |
| game_sa/GangWars.cpp | 5 | grid-exprx3, loop(numPeds), loop(pedCount) |
| game_sa/General.cpp | 1 | return:GetNodeHeadingF |
| game_sa/GridRef.cpp | 2 | grid-exprx2 |
| game_sa/Hud.cpp | 3 | grid-exprx3 |
| game_sa/InterestingEvents.cpp | 1 | grid-expr |
| game_sa/Interior/Interior_c.cpp | 2 | member:m_tileX, member:m_tileY |
| game_sa/MemoryMgr.cpp | 1 | call:memcpy |
| game_sa/Models/VehicleModelInfo.cpp | 1 | grid-expr |
| game_sa/PedGeometryAnalyser.cpp | 2 | loop(maxIterations), loop(numSamples) |
| game_sa/Plant/ProcSurfaceInfo.cpp | 8 | loop(xCur)x2, loop(xEnd)x2, loop(yCur)x2, loop(yEnd)x2 |
| game_sa/PlayerInfo.cpp | 4 | grid-exprx3, return:DecayedTempBuff |
| game_sa/PointLights.cpp | 8 | loop(startX)x2, loop(startY)x2, loop(endX)x2, loop(endY)x2 |
| game_sa/PopCycle.cpp | 1 | loop(rnd) |
| game_sa/PostEffects.cpp | 5 | loopx2, return:ExeRasterSize, loop(lineHeight), loop(numLines) |
| game_sa/ProjectileInfo.cpp | 1 | loop(numParticles) |
| game_sa/Radar.cpp | 4 | grid-exprx4 |
| game_sa/RoadBlocks.cpp | 1 | loop(numBars) |
| game_sa/Ropes.cpp | 1 | index |
| game_sa/Scripts/Commands/Ported/Group13_15.cpp | 1 | index |
| game_sa/Shadows.cpp | 5 | call:IterateSectorsx4, grid-expr |
| game_sa/Stats.cpp | 2 | grid-exprx2 |
| game_sa/StencilShadows.cpp | 1 | grid-expr |
| game_sa/Streaming.cpp | 1 | grid-expr |
| game_sa/Tasks/TaskTypes/TaskComplexPartnerChat.cpp | 2 | grid-exprx2 |
| game_sa/Tasks/TaskTypes/TaskComplexProstituteSolicit.cpp | 1 | grid-expr |
| game_sa/Tasks/TaskTypes/TaskComplexWanderCriminal.cpp | 1 | grid-expr |
| game_sa/Tasks/TaskTypes/TaskSimpleCarDrive.cpp | 1 | grid-expr |
| game_sa/Tasks/TaskTypes/TaskSimpleChoking.cpp | 1 | grid-expr |
| game_sa/Tasks/TaskTypes/TaskSimpleClimb.cpp | 1 | member:m_GetToPosCount |
| game_sa/Tasks/TaskTypes/TaskSimpleDuck.cpp | 1 | member:m_ShotWhizzingC |
| game_sa/Tasks/TaskTypes/TaskSimpleFight.cpp | 1 | member:m_nIdleCounter |
| game_sa/Tasks/TaskTypes/TaskSimpleFightingControl.cpp | 1 | grid-expr |
| game_sa/Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp | 3 | call:GetDrivebyAnimForx2, grid-expr |
| game_sa/Tasks/TaskTypes/TaskSimpleInAir.cpp | 1 | member:m_nProcessCount |
| game_sa/Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp | 1 | grid-expr |
| game_sa/TheZones.cpp | 4 | indexx4 |
| game_sa/WaterLevel.cpp | 14 | grid-exprx6, indexx4, loop(numFogLayers), loop(numLayersToRender |
| game_sa/Weather.cpp | 2 | loop(numSplashSpots), loop(numSplashesPerSpo |
| game_sa/World.h | 4 | return:GetSectorX, return:GetSectorY, return:GetLodSectorX, return:GetLodSectorY |

## Class B (926, listed by count only; next slice)
game_sa top level 543, Entity 102, app/platform 67, Tasks 48 (after batch 3: some timers already class A), Fx 36, Collision 32, Audio 28, Frontend 23, Interior 20, Scripts 13, Events 6, Plant 4, Pipelines 2, Conversations 1. Mostly colours (uint8 of float*255), timers (GetTimeStep*0.02*1000), random counts (GetRandomNumber*(1/32768)*N - bounded), screen pixels. Reproduce: `python3 tools/portable/ftol_scan.py --out x.tsv` (needs a fresh PCH, `--rebuild-pch`). Candidates worth promoting by hand: Frontend/MenuManager_Input.cpp:557-564 (mouse -> int16), Clouds.cpp:948/949/1045/1046 (screen x/y), Replay.cpp int8/uint8 packers (junk velocities).

## Consumer audit
The macro changes no value in range; out of range / NaN it now yields the exe's value instead of arm64 saturation. Readers: only the converted expressions themselves. The sector clamp is the only semantic change (out-of-map indices beyond -1 / MAX): readers = all CWorld::GetSector* callers (grep GetSectorX/Y: World.cpp loops, Renderer, Streaming, Collision, EntityScanner, Shadows, PedGeometry...), all either clamp or test the out-of-map range.

## Win32-relevant findings
None where the Win32 build differs from the exe. One tooling lesson for the Windows side: MSVC /arch:IA32 x87 scheduling is sensitive to an added pair of parentheses around a float operand of a cast (CWeather::AddRain operand order of an unrelated vector add changed): macros wrapping float expressions must keep the tokens `(T)(expr)`.
