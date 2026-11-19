# ASM-P4RVX

Part A: CHandShaker::Process (0x50D930) CONFIRMED in full (sign test, z,y,x draws, Range(1,N)==2, scale formula, clamp, SetRotate). CPedGroupMembership::Process (0x5FBA60) area-code fix CONFIRMED; one more deviation found and fixed (1aa7e5dc3): the exe sums dz^2+dy^2+dx^2 and removes a far member only when sep^2 < dist^2 (NaN never removes).

Part B: GetVectorsReadyForRW, Process_Fixed, CamControl ped garage/stairs fixed-cam block (0x5291EB-0x529B9A, last open region) all MATCH. FollowPed/FollowCar/AimWeapon/WheelCam/1stPerson/1rstPersonPedOnPC/CCamera::Process already had full-range verdicts. There is no separate Process_Cam_On_A_String in the port (routes to FollowPed_SA).
