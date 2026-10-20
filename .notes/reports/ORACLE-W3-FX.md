# ORACLE-W3-FX report

Agent ORACLE-W3-FX (rules v3), worktree C:/gta-wt/oracle-w3-fx (branch oracle-w3-fx, base 30dd2c12), 2026-10-20 12:00 - ~12:30 WEST. Scope cut for quota: game_sa/Fx root files only (FxSystem.cpp, FxManager.cpp first);
**game_sa/Fx/Blueprints was NOT started** (FxInfo*::GetValue, FxInfoManager::AddFxInfo ...: 26 A / 4 B left, needs FxInfo key-curve fixtures; next agent).
New target `wave3_fxs_oracle_test` (W2 harness, `W3_FXS_ONLY`), rows in `tests/standalone/wave3_oracle_fxs.inc`; **39 functions, PC24 0 mismatches**, run time **~110 s** (PC53 1102 informational: x87 extended precision; the game runs PC24).
Dev switches: `W3X_DIV=n` divides the case counts, `./wave3_fxs_oracle_test.exe <substring>...` runs matching rows, `W3X_DBG=1` prints the port's librw camera layout.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4389 | **4428** |
| game_sa/Fx root files covered (uncovered A / B) | 12 (40 / 4) | **51 (1 / 4)** |
| port bugs fixed (one commit each) | | **3** |

## Fixtures
One 16 KB fixture block (compared as a whole after every case, together with the whole exe data image): an FxSystem_c over a blueprint with a prim pointer array and 4 prim objects, RwMatrix blobs, a particle box, particle list nodes (8 doubly linked 0x40-byte nodes in random order, with m_System pointers), an FxManager_c with a real list, a memory pool over a data block, spheres / planes / a frustum (near-each-other and far cases), an emitter blueprint with a particle list, and **two cameras for CalcFrustumInfo: the port's librw layout (viewWindow +0x1C, farPlane +0x30, frustumPlanes +0x84, frame matrix +0x18) and the exe's (viewWindow +0x68, far +0x84, planes +0x94, frame matrix +0x10) in separate buffers with the same values**. `g_fxMan` (0xA9AE80) is prepared where the function reaches it by itself (pool 0xA9AF2C for FxSystemBP::SetBoundingSphere / operator new, particle list for FreePrtFromPrim -> ReturnParticle). vptr bytes of the fake objects are not compared. `FxSphere.cpp` is #included in the test TU (its `inline` members have no body in the header).

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first run on the unfixed code, log C:/gta-scratch/w3fx/r3.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| FxSphere_c::GetDistToPlane | 0x4AA010 | `DotProduct` x,y,z order; the exe sums n.y*c.y, n.x*c.x, then n.z*c.z, then subtracts the distance (each rounded to float at PC24) | 275/2000 | 268d80ee |
| FxSystem_c::FxSystem_c | 0x4AAF00 | exe clears [+0x100] = m_FireAE.m_FxSystem (inlined CAEFireAudioEntity ctor), the port left it uninitialised | 87/100 | f5282ee0 |
| FxManager_c::CalcFrustumInfo | 0x4A9140 | port did the maths in double ("extended") with a float-rounded second sine and reset m_Sphere.m_nNumPlanesPassed; the game runs the x87 at PC=24 (every add / mul / div / sqrt rounds to float), only FPATAN / FSIN are unrounded and the second sine is used unrounded in the final fmulp (`fst` stores a copy); the exe writes only centre and radius (+0x44 untouched) | 1353/1500 | 9df3da33 |

## Rows that pass without a change
FxSystem_c: Pause, GetPlayStatus, EnablePrim, SetMatrix (allocated and plain parent; 42/600 cases fault identically in both sides - allocated flag with a null parent - and are skipped by the harness), SetOffsetPos, AddOffsetPos (RwMatrixUpdate via standalone/rw/math.cpp), SetConstTime / SetRateMult / SetTimeMult (values up to 1e12: `notsa::detail::Ftol` already models _ftol2), SetVelAdd, SetLocalParticles, SetZTestEnable, SetMustCreatePrts, GetBoundingSphereLcl, UpdateBoundingBoxCB (box around the particle, NaN / equal / inf);
FxManager_c: Constructor (uninitialised frustum bytes masked, see below), Destructor, GetFrustumInfo, SetWindData, FxRwMatrixCreate / Destroy, ReturnParticle, GetParticle;
FxMemoryPool_c: Reset, GetMem (alignments 1..0x100, sizes up to 2^32); FxSystemBP_c: operator new, Constructor, Destructor, Unload, GetBoundingSphere, SetBoundingSphere (pool-allocated sphere, radius <= 0 / NaN); FxEmitterPrt_c::operator new[]; FxSphere_c::IsCollision; FxFrustumInfo_c::IsCollision (4000 cases, only 187 true: most random spheres are rejected by a plane); FxEmitterBP_c: Constructor, FreePrtFromPrim.

## Observed, not changed
* Constructors: the port zero-initialises members the exe leaves uninitialised (FxSystem_c::m_FireAE members other than +0x8C / +0x100, FxManager_c::m_Frustum centre / radius / planes): masked in the two constructor rows (`g_unc`); the objects are `new`ed / globals that are written before use.
* FxSystem_c::SetMatrix with the allocated-parent flag and a null parent faults identically in both (those cases are skipped).
* FxManager_c::CalcFrustumInfo differs at PC53 (extended precision), informational.

## Not covered
* game_sa/Fx/Blueprints (FxInfo* GetValue / Load, FxInfoManager::AddFxInfo, FxInfoAttractLine::FindClosestPtOnLine ...): not started (scope cut).
* Fx_c::RotateVecAboutVec 0x4A1780 (B: sine table at 0xBB3E00 + _ftol2; Fx.cpp is not linked in the target), CCarFXRenderer PreRenderUpdate 0x5D5B10 / IsCCPCPipelineAttached 0x5D5B80 (needs an atomic with the pipeline plugin) / SetFxEnvMapLightMult 0x5D5BA0 (one store into a CarFXRenderer global; TU not linked): next candidates, they need Fx.cpp / CarFXRenderer.cpp in the target.
* Class C/S/D remaining in game_sa/Fx (Update / Render / AddParticle: prim virtual calls, textures, file IO): see ORACLE_COVERAGE.md.

## Tests run (worktree)
wave3_fxs_oracle_test (39 functions, ~110 s, exit 0, log C:/gta-scratch/w3fx/r4.log). The changed TUs (FxSphere.cpp, FxSystem.cpp, FxManager.cpp) are linked by no other test target, so no other target needed a rerun; the orchestrator runs the full run_all_tests.

## Blueprints (ORACLE-W3-FXBP, rules v3, 2026-10-20 12:28 - ~12:55 WEST)
Rows in `tests/standalone/wave3_oracle_fxbp.inc` (included by wave3_oracle_fxs.inc, same target; CMake adds Blueprints/*.cpp, Interp/*.cpp, FxInfoManager.cpp, Fx.cpp, CarFXRenderer.cpp, Maths.cpp, PipelinePlugin.cpp). **73 functions in the target, PC24 0 mismatches, whole target ~155 s** (the new rows compare only the 16 KB fixture block, not the 4 MB image: ~45 s). PC53 4189 informational (x87 extended precision).

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4428 | **4462** |
| game_sa/Fx/Blueprints covered (uncovered A / B) | 5 (26 / 4) | **35 (0 / 0)** |
| game_sa/Fx covered (uncovered A / B) | 51 (1 / 4) | **55 (0 / 1)** |
| port bugs fixed (one commit each) | | **6** |

Fixture: an FxInfo object in the block (vptr, m_nType, m_bTimeModeParticle, embedded FxInterpInfo: looped / numKeys / count / times* / keys*), uint16 time array, per-attribute key arrays (float / int16 / uint16 storage), 1..6 keys (rarely 7..8, 1 key 1 in 8), ascending times (rarely unordered, zero-length last key), looped 1 in 3, key values game range + NaN / inf / 1000x; args ct / mult / totalTime / len: game range + NaN / inf / negative / 0; useConst and m_bTimeModeParticle both ways (incl. other non-zero bytes); output EmissionInfo / MovementInfo / RenderInfo pre-filled with random floats (untouched fields must stay). The exe's RenderWare maths (RwV3dLength / RwV3dNormalize / RwMatrixUpdate) needs the engine plugin data: `w3x::InitExeRw` (copy of w3k::InitExeRw). Wind reads g_fxMan.m_pfWindSpeed / m_pWindDir (pointed into the block). AddFxInfo: all 32 types of the exe's switch + gaps + garbage, g_fxMan pool over the block, vptrs and the exe-uninitialised bytes (+6/+7, high byte of m_nCount) cleared on both sides.

### Port bugs fixed (failing PC24 cases of the first full run on the unfixed code, log C:/gta-scratch/w3fxbp/r2.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| Fx_c RotateVecAboutVec | 0x4A1780 | exe association: t*x*z = (z*x)*t, t*y*z = (t*z)*y, t*a*a = (a*a)*t, sums of .y in order x, z, y terms (x87 PC24 rounds each op) | 1911/4000 | 19d02bae |
| FxInfoAttractPt_c::GetValue | 0x4A56E0 | exe calls RwV3dLength 0x7EDAC0 / RwV3dNormalize 0x7ED9B0 (table sqrt), not sqrtf; direction * (values[0] * totalTime) (values[0] is the exe's strength too) | 2664/3000 | 3bbb1098 |
| FxInfoAttractLine_c::GetValue | 0x4A5850 | same, strength values[6] * totalTime | 2656/3000 | af73001a |
| FxInfoEmRate_c::GetValue | 0x4A4B60 | `test ah,5 / jp`: NaN (currentTime - totalTime) takes the >= 0 branch (port took the split-integral branch) | 3/2000 | 9f79fd11 |
| FxInfoManager_c::AddFxInfo | 0x4A7B00 | the exe allocates every FxInfo from the Fx pool (GetMem(size, 4) on g_fxMan.m_Pool 0xA9AF2C), the port used the game heap: FxInfo_c::operator new added (+ no-op delete) | 791/1500 | 8d1cf0bb |
| FxInfoFloat_c 0x4A5DA0 / HeatHaze 0x4A61A0 / SelfLit 0x4A6760 / Underwater 0x4A5E20 ctors | | no Allocate(0): the exe stores only the count byte (m_Keys stays null); the port took a zero-sized aligned block from the pool | 94/1500 (AddFxInfo, after the previous fix) | 25617bc4 |

### Pass without a change
FxInfo*::GetValue of EmSize, EmSpeed, EmDir, EmAngle, EmLife, EmPos, EmWeather, EmRotation, Force, Wind, RotSpeed, Underwater, Float, Colour, Size (count 2 and 4), SpriteRect, HeatHaze, Trail, Flat (RwMatrixUpdate), Dir, AnimTexture, ColourRange, SelfLit, ColourBright, Smoke; FxInfoSelfLit_c::Load; FindClosestPtOnLine (4000 cases, degenerate / short / on-line); CCarFXRenderer::SetFxEnvMapLightMult, IsCCPCPipelineAttached (random plugin offset, id hit 1 in 3).

### Observed, not changed
* The port writes the full int16 m_nCount in Allocate, the exe one byte (readers do movsx byte; high byte garbage in the exe): masked in the AddFxInfo row. FxInfo_c's value-initialised base zeroes m_bTimeModeParticle / padding that the exe leaves uninitialised (Load sets it): masked.
* The eFxInfo "ROTATE / TEXCOORDS / ROTATEOFFSET" (MH-only) types return null in the port; the AddFxInfo row includes the gap values and they agree with the exe (null).

### Not covered
* CCarFXRenderer::PreRenderUpdate 0x5D5B10 (B): jumps to CCustomCarEnvMapPipeline::PreRenderUpdate 0x5D8870, which reads pDirect's RpLight -> frame -> matrix (RenderWare layouts differ between the exe and librw, as for the CalcFrustumInfo camera: needs a light / frame fixture in both layouts) and fills g_GameLight; the TU (CustomCarEnvMapPipeline.cpp) is not linked. Next candidate, small.
* Class C/S/D remaining in Blueprints (Load file IO 7, CRT 1) and the C ones (Noise / Friction / GroundCollide / Jitter GetValue, Process* with virtual calls): see ORACLE_COVERAGE.md.

### Tests run (worktree)
wave3_fxs_oracle_test (73 functions, ~155 s, exit 0, PC24 0; log C:/gta-scratch/w3fxbp/full1.log). The changed TUs (Fx.cpp, FxInfo*.cpp) are linked by no other test target (wave3_fx_oracle_test etc. link only the Interp TUs, unchanged); the orchestrator runs the full run_all_tests.
