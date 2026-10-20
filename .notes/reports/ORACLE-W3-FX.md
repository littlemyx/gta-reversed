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
