# ASM-P4S8b

- CTaskSimplePlayerOnFoot::ProcessPlayerWeapon (exe 0x6859A0): FIXED 2, commit cace921f4. Build StandaloneRelease OK; no game run.
  1. exe 0x6862FE-0x686327: instant-hit/area case, active weapon state == 2: before -> always AIM command; after -> skip the whole case (no task, no look pitch reset) unless GetTarget() || m_pTargetedObject || m_bFreeAiming, then AIM. State != 2 -> FIRE unchanged.
  2. exe 0x686AEB-0x686BEA (gun pulled at a ped, instant-hit weapon): IsInSeeingRange, GetActivePrimaryTask and the CEventGunAimedAt event group Add now use the TARGET ped's CPedIntelligence (before: the player's).
  Rest compared MATCH (melee, throw, camera fire, reload anims, lock-on/shift target, free-aim, stop-aim, look-at).
- CVehicle::FlyingControl (exe 0x6D85F0): NOT_REACHED. Prologue, wind, 3x3 centre of mass, switch and FlyingControlModel0 (0x6D8715-0x6D8B1D) MATCH; Model1345 / Model268 / Drag not compared.
