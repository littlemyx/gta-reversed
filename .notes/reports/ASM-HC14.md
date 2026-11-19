# ASM-HC14: FX / camera / pad executed-but-unverified functions
Zone rows (exe_addr != "-", verdict NONE, FX/Cam/Pad/Controller, not render): 100; 47 still without a verdict after re-check (53 were done by HC3/HC5/HC7/HC9).
Compared: 38 rows (+ CreateParticle, which has no address row). SKIP-RENDER: CCamera::RenderMotionBlur (and CustomCarEnvMap/CarFXRenderer/PostEffects/Occluder/VisibilityPlugins/Grass rows). NOT_REACHED: FxSystemBP::operator new, FxEmitterPrt::operator new[], Fx_c::CreateMatFromVec, CCamera::StartTransition, CIdleCam::Reset, CCam::Init (1-82 calls, startup).
Result: MATCH 33, FIXED 3 (below). Build: StandaloneRelease links (no warnings). run_all_tests / PortableWin64 compile / regress not run (left to the gate).

## FIXED
1. FxEmitter_c::CreateParticle (0x4A2580, no hook row): the port used GetRandomNumberInRange (rand/32767 ranges) and `/5000`; the exe uses `rand()%10000 * 1e-4f` (0x858FC4) for [0,1) and `* 2e-4f` (0x85A7D0) for [-1,1) for life, RGB (x255 ftol), rotation, box position, sphere vector (+ 4th draw only when radius >= 0 or NaN, |v|^2 summed z,y,x, fsqrt, 1/len), speed. Draw COUNT/ORDER was already equal, the VALUES were not. Also direction normalise = RwV3dNormalize (0x7ED9B0 table), compare `!(x > 10)` (NaN takes the direction branch), `fabs(r) < 0.001f` strict/NaN box branch.
2. FxSystem_c::Play (0x4AA2F0): `/10000` -> `* 1e-4f`.
3. FxInterpInfo GetVal template: double -> x87d (identical in Win32; on PortableWin64 it was 53-bit per op vs exe PC24).
No change needed: all FxInfo*::GetValue, IsCollision (visibility gate: z,y,x sum, strict compares, NaN), AddSparks, matrix pool, camera setters.
