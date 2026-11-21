# ASM-Z20: hit-count ranking of failing story missions, then asm compare (oracle: gta_sa_compact.exe via exedis)

Hit pass: HITCOUNT RR build (build/PortableWin64RR_HC, from origin ef9a54fa1) run over 40 story missions that fail/stop in BOTH ports (compare_ts.tsv idx 2, 10-53 range, earliest first; dumps C:/gta-scratch/hc_z20/<idx>.hc.txt, driver output C:/gta-scratch/allmissions_hcz20). All 40 finished; no background work left.

Ranking: .notes/asm_sweep/Z20_HC_RANK.tsv = 256 hooked functions without a verdict hit in at least one of the 40 missions, weight = missions that hit it, after dropping render/audio/streaming/RW names. The top rows are boot or render-adjacent (CVisibilityPlugins, COccluder, EnvMap pipe, CReplay, PlantMgr) that every mission hits; the gameplay part is small and mostly Clone/Init helpers.

Compared 21 functions (Z20.result.tsv): all MATCH, 0 fixes (no source commits). Gameplay rows left in the ranking: BoneNode_c::CalcWldMat/QuatToEuler/Limit (30 missions), ProcSurfaceInfo_c::AddObjects/AddObject, CIdleCam::ProcessSlerp/FinaliseIdleCamera, CCover::FindCoverPointsForThisBuilding, CWeapon::FireInstantHit, CEventKnockOffBike::CalcForcesAndAnims, CEventDeath/CEventDamage Clone variants, the ~40 Task Clone rows (1-16 missions each).

Takeaway: the story-mission failures are not in the unverified hooked functions the missions execute; the remaining unverified set that they touch is 256 functions, mostly boot/render. Fixing missions needs a different lever (script/behaviour traces) rather than further static asm sweeps.
