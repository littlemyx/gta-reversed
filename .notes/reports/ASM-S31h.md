# ASM-S31h - Cam.cpp ASM sweep (slice S31h): 5 functions fully compared, no deviation found, 6 not reached

Scope: .notes/asm_sweep/S31h.tsv (15 rows, 11 distinct functions). Result table: .notes/asm_sweep/S31h.result.tsv. Branch asm-s31h from origin/reverse/interior_c a199405a. No code change, so no code commits.

| fn | verdict |
|---|---|
| CCam::ProcessArrestCamOne (0x518500-0x51921C) | MATCH (full): mode lists, random compares (extended product vs 0.5 / 0.65f), free-cam pick, found/loop exit, nearby-ped search, mode 2 maxZ clamp, modes 4-8 move/test sphere/ground, apply paths. Only difference: exe `camPos` is uninitialised stack, port inits it (no observable effect) |
| CCam::GetArrestCamPosBehindTarget (0x516010) | MATCH (full) |
| CCam::LimitPrecision(float,int) (0x50A0A0) | MATCH (full) |
| CCamera::AvoidTheGeometry (0x514030) | MATCH (full): every compare arm, sum orders, near-clip logic, ped timer/dirn branches, WellBufferMe args |
| CCam::Process_AttachedCam (0x512B10) | MATCH (full); the inlined underwater blur equals ApplyUnderwaterMotionBlur |
| Process_Cam_TwoPlayer, DW_BirdyCam, DW_CamManCam, DW_HeliChaseCam rest, Process_WheelCam, Process_Rocket head, CCam::Process | NOT_REACHED (budget) |

Open ranges for a next slice: Process_Cam_TwoPlayer (0x525E50), DW_BirdyCam (0x51B850), DW_CamManCam (0x51B120), DW_HeliChaseCam 0x51A740-0x51B020, Process_WheelCam (0x512110), Process_Rocket 0x511B50-0x511EC2, CCam::Process (0x526FC0).

## Tests
- StandaloneRelease built clean from scratch on unmodified origin (1186/1186, /WX).
- wave2_oracle_test (StandaloneRW, RW_EXE_ORACLE = GTA_EXE = main-checkout gta_sa_compact.exe): EXIT 0, 1142 functions, PC24 0/0, PC53 0/0.
- No code changed, so combat regress, PortableWin64 compile and the v6 grep were not needed (diff is .notes only).
