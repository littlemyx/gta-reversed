# ASM-P4R2v: review of P4R2u fixes + game loop functions without a verdict, continued

Walk list: P4R2n (281 functions; 69 still without a clean verdict after P4R2u, 137 already MATCH/FIXED/INTENTIONAL).
Part A: both P4R2u fixes re-checked in the exe: CFire::ExtinguishWithWater z offset and CWorld::RemoveVehicleAndItsOccupants (m_pDriver nulled) confirmed.
Part B: compared 8 functions: 5 MATCH (GenerateDamageEvent and SaveOrRetrieveDataForThisFrame, whose old DEVIATION rows are now confirmed whole-function), 3 FIXED:
- CPedGroups::Process 43c8cb50f: the end-of-frame test used the OLD mission flag (std::exchange) so the kill counter was not reset at mission end.
- CGangWars::AttackWaveOvercome 0d2ebb966: also cleared bDonePositionOutOfCollision (a different bit than bPartOfAttackWave).
- CGangWars::PickStreamedInPedForThisGang 414096cea: rotating index aliased the x of the first stored safehouse car (0x96ABD8) instead of the static at 0x96ABD4, so every call clobbered a saved car position; early-out now tests the ped count.
Not reached (budget): CCamera::UpdateTargetEntity, CStats::DisplayScriptStatUpdateMessage, CPointLights::AddLight, CGangWars::GetGangColor and the rest.
Build: StandaloneRelease ninja -j4 (full link) OK. No game runs, no tests.
