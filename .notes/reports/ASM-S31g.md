# ASM-S31g - Cam.cpp ASM sweep (slice S31g)

Scope: .notes/asm_sweep/S31g.tsv (21 rows). Result table: .notes/asm_sweep/S31g.result.tsv. Branch asm-s31g from origin/reverse/interior_c 3c3ef074.

## Task 0 - m_nLastHSMissileLOSTime consumer audit
CPlayerPedData layout (size 0xAC): `m_nLastTimeFiring` is at +0x6C, the 31-bit `m_nLastHSMissileLOSTime` + 1-bit `m_bLastHSMissileLOS` dword at +0xA0.
The exe's CPickup::Update (0x457410) reads +0x6C at 0x457BC5 and 0x457CA3 (`now - [data+0x6C] < 0x5DC`), i.e. the time of the last shot, not the HS-missile LOS field.
Pickup.cpp:432/435 read `m_nLastHSMissileLOSTime` (a wrong member, +0xA0) -> fixed to `m_nLastTimeFiring` (commit 9bf3493e).

| site | exe | port | verdict |
|---|---|---|---|
| Pickup.cpp:432, :435 (CPickup::Update 0x457BC5, 0x457CA3) | reads +0x6C (m_nLastTimeFiring), `<1500` unsigned | read +0xA0 field as a time | FIXED |
| Cam.cpp ~7181/7182 (Process_Rocket 0x511F44-0x511F7E) | read `now - (field==0)`, write `now>1` (bool) | same (S31f) | MATCH |
| Cam.cpp ~7201/7205 (m_bLastHSMissileLOS bit 31) | `& 0x80000000` write via xor/and, read in 0x512021 | same | MATCH |
| PlayerPedData.cpp:30-31, :116-117 (ctor 0x56F810, SetInitialState 0x56F210) | stores 0x80000000 to +0xA0 (time 0, LOS true) | field = 0, LOS = true | MATCH |
No other readers/writers exist in source/ (grep over the whole tree).

## Functions
| fn | verdict | commit |
|---|---|---|
| Finalise_DW_CineyCams | DEVIATION_FIXED: shake rotation via 0x59C810 (InverseTransformVector); second-pass roll = shaker.m_ang.z * shake | d1bf6518 |
| GetLookFromLampPostPos | MATCH | - |
| Get_TwoPlayer_AimVector | DEVIATION_FIXED: 0x8631D4 float product; Y scales the up term; (right*X)*(float)tan | ec9bc939 |
| IsTimeToExitThisDWCineyCamMode | MATCH | - |
| ArrestCamLookAtCopHead | DEVIATION_FIXED: flip only for t>=1 (NaN arm) | bd13df64 |
| ProcessPedsDeadBaby | MATCH (reset branch front/right register trace spot-checked only) | - |
| ProcessArrestCamOne, Process_AttachedCam, Process_Cam_TwoPlayer, DW_BirdyCam, DW_CamManCam, DW_HeliChaseCam rest, Process_WheelCam, LimitPrecision, GetArrestCamPosBehindTarget, AvoidTheGeometry, Process_Rocket head, CCam::Process | NOT_REACHED (budget) | - |

Counts: 3 DEVIATION_FIXED (+1 Task 0), 3 MATCH, 13 NOT_REACHED rows (the 4 duplicate `Process` rows count once).

## Tests
See the end of this file (filled in after the runs).
- wave2_oracle_test (StandaloneRW, RW_EXE_ORACLE = GTA_EXE = main-checkout gta_sa_compact.exe): 1142 functions, mismatches PC24 0/0, PC53 0/0.
- Combat regress (`-b <wt>/build/StandaloneRelease combat`): PASS=14.
- StandaloneRelease built clean (/WX), v6 pre-commit grep clean on every commit.
- PortableWin64: Cam.cpp and Pickup.cpp syntax-only compile (/Zs, x64 subshell, same flags as the portable build): 0 errors.
