# ASM-P4R23: root-file functions with exe-address comments but no verdict

Regenerated noverdict list: only 19 root-zone rows (size >= 0x30) still carry exe comments and no verdict; all gameplay ones (CWeapon, CCarCtrl, CPopulation) already had verdicts from later slices. All 19 are Cam/PlantMgr/Shadows/WaterLevel/Streamed scripts.

Compared against exe 1.0: CCollision::ProcessColModels tail from 0x419A5E (A tris/boxes vs B spheres, MATCH), CWaterQuad::GetWaterLevel, CWaterTriangle::GetWaterLevel, CCam::RotCamIfInFrontCar: all MATCH, 0 source changes, no build needed.
NOT_REACHED: ProcessColModels disk block (dead in SA), CCam::Process_FollowCar_SA (6.3 KB), Process_Cam_TwoPlayer_*, Process_FlyBy, CCam::Init, CPlantMgr/CShadows/CPlantSurfPropMgr/CStreamedScripts rows (render/cosmetic).
