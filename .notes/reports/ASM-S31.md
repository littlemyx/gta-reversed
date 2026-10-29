# ASM-S31 - Cam.cpp per-mode camera processors (34 functions, 3 reached)

- Reached: CCam::Process (all, FIXED), CCam::Process_Rocket (FIXED, heat-seeking tail 0x511EC2-0x5120E7 PARTIAL), CCam::Process_SpecialFixedForSyphon (MATCH). 31 rows NOT_REACHED (budget; FollowPed_SA, FollowCar_SA, AimWeapon, M16_1stPerson, 1stPerson, LookBehind/Right, helpers, DW cams untouched).
- 3dce1dfe Process (exe 0x5272DB-0x52730C): vehicle target sets TheCamera.m_bCamDirectlyBehind when `!LookBehindForCar || Left || Right` (port: `LookBehindForCar && (Left||Right)`, which is practically never true; the exe sets the flag almost every frame in a car); unordered `vy+vx` takes the negative-speed branch.
- 3557dfb4 Process_Rocket: missing `CEntity::UpdateRpHAnim` (0x511C1E); source only z += 0.1; mouse deltas `(-3x)*(fov*accelH)`, `(fov*accelV)*(4y)` and pad deltas `(hv^2*1e-4)*(fov/17.5)*ts*sign` (fov factor multiplied first); vertical clamp +60 / -89.5 deg (0x8630F8 / 0x8630F4) instead of the inlined ClipAlpha's -85.5.
- Verified MATCH in Process: mode jump table 3..65 -> handler (all 31 groups), look-direction state machine, ped target smoothing (pow 0.9 / 0.6 / 0.8, ExeDivide), true alpha/beta, KeepTrackOfTheSpeed, look-behind/right dispatch, tail.
- Note: CCam::ClipAlpha() (inlined in the exe, -85.5/+60 deg) had Rocket as its only caller and is now unused; other modes clamp alpha inline with their own constants (check per function in the next slice).
- Tests: StandaloneRelease + StandaloneRW build OK; wave2_oracle_test and combat regress: see below.
  wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe): 1142 functions, PC24 0/0, PC53 0/0 mismatches. combat regress (-b worktree StandaloneRelease): PASS=14, TOTAL PASS. v6 pre-commit grep clean (both code commits).
