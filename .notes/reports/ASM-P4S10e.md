# ASM-P4S10e: P4S10 queue continuation (unannotated rows)

Compared 33 rows of .notes/asm_sweep/P4S10.queue.tsv (starting with CTaskSimpleClimb::GetCameraTargetPos): 30 MATCH (several only read up to the half-way point, noted in the TSV), 1 NOISE (LookForCarAlarms sum order), 2 FIXED. About 320 rows  NOT_REACHED (token budget; rows with 100+ lines such as CPed::UpdatePosition, CEventDamage::ComputeDeathAnim, CBike::FixHandsToBars, CTaskSimpleFight::IsTargetInRange untouched).

Changes (one line each: function, exe range, before -> after):
- CTaskSimpleClimb::GetCameraTargetPos, 0x67A390-0x67A59F: pull/stand/jump-B offsets used lerp<CVector2D>({H0,H1},{V0,V1}) (components paired the wrong way: x = lerp(HangH, HangV)) -> per-component from*(1-t)+to*t in x87d (x = from.H*(1-t)+to.H*t, y = from.V*(1-t)+to.V*t), as the exe's three blend cases at 0x67A3F6/0x67A437/0x67A475 (jump table at 0x67A5A8 / 0x67A5C0 checked: ids 130/131/133 -> the three blends, 132 -> stand-up constants, 121/134 -> vault constants).
- CTaskComplexFollowNodeRoute::CreateFirstSubTask, 0x671800-0x6718D0: blend reset also cleared m_bSlowingDown/m_bSpeedingUp -> only the four floats at +0x50..+0x5C are zeroed (the exe never touches the bit flags in [+0x4C] here).

Build: StandaloneRelease full link in C:/gta-wt/p4s10e (2 .cpp changed), linked OK (1186/1186), no errors.
