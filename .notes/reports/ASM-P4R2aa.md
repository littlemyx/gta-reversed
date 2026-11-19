# ASM-P4R2aa
Per-entity level-1 callees left after P4R2z (47 NOT_REACHED + 14 AUDITED-IN-SOURCE rows): 32 compared against the 1.0 exe (30 MATCH, 2 FIXED), 29 NOT_REACHED (budget).
- 8e4ff513b CPad::StartShake (0x53F920): `time > ShakeDur` test, uint32 NoShakeBeforeThis, unsigned NoShakeFreq.
- a26e65c5f CPhysical::AttachEntityToEntity (0x54D690): m_pEntityIgnoredCollision = new target; matrix allocated for a matrix-less target.
- Re-compared AUDITED rows: ProcessCarAlarm, ActivateBombWhenEntered MATCH; cTransmission::CalculateDriveAcceleration, CBike::ProcessBikeWheel still not compared (NOT_REACHED).
- All 13 CPad getters (jump tables read from the exe), CWanted::Update, CEventDamage::AffectsPed, UpdatePedCount: MATCH.
