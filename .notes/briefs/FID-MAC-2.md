# FID-MAC-2 — Win32 fidelity items from the Mac stability work (PORTABLE_SYNC, MAC-STAB1..4, MAC-FPTRAP)

Rules: .notes/agent_rules/v6.md. Worktree via tools/standalone/windows/new_worktree.sh fid-mac-2 on origin/reverse/interior_c.
Read the [mac] 2026-10-25..27 entries of .notes/PORTABLE_SYNC.md on origin/portable/main (`git show origin/portable/main:.notes/PORTABLE_SYNC.md`)
and the reports they name (on portable/main: .notes/reports/MAC-STAB1..4.md, MAC-FPTRAP.md). Each item: check the exe (exedis), then fix
unguarded on Win32 where the exe agrees (one commit each, exe address), or record "faithful/exe-too, not applied".
1. CTheScripts::ClearSpaceForMissionEntity 0x486B00: exe pre-pass nulls colliding peds with bInVehicle before the loop; `continue` after
   `delete vehicle` (MAC-STAB3: use-after-free of freed ped slots, leaked list-1 matrix links). HIGH.
2. CAEWeaponAudioEntity::PlayGunSounds 0x503FE9..0x503FFC: t = ((12/r)-dist)/((12/r)-(5/r)); the port divides by (12/r) and subtracts 5/r
   (gunshot loudness 5..12 m, NaN front-end volume). HIGH.
3. CCarCtrl::CreateConvoy (CarCtrl.cpp:788) / CreatePoliceChase (:907): `*car->m_matrix = *vehicle->m_matrix` copies CMatrixLink list
   pointers; exe calls CMatrix::operator= 0x59BBC0 (matrix only) -> `*static_cast<CMatrix*>(car->m_matrix) = *vehicle->m_matrix;`.
4. CVehiclePool = CPool<CVehicle, CHeli>: confirm sizeof(CPlane) <= sizeof(CHeli) on x86 (VALIDATE_SIZE); if not, it is a heap overwrite.
5. CFormation::FindCoverPointsBehindBox (Formation.cpp:84) `(i - 2) % 4` indexes corners[-2/-1] for i=0,1 — check exe 0x69A325.
6. Optional, gameplay-neutral latent UAF fixes the Mac applied under NOTSA_PORTABLE: CEntity::DeleteRwObject CopyParentMatrix for fx
   systems (MAC-STAB4); CMatrixLinkList::Init zero m_pOwner; CPedModelInfo::CreateHitColModelSkinned bone-local centre (MAC-FPTRAP 2).
   Apply only where it does not change exe-visible behaviour; say which.
(GetWheelPosn memcpy is already fixed on Win32: f40dae01.)
Gate (v6): Release + touched oracle targets + soak + the PortableWin64 compile if the gate step exists by then. HARD budget: stop at 120k
by your estimate. Write a [win] verdict entry into .notes/PORTABLE_SYNC.md.
