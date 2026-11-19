# ASM-P4R7f
Review 4315bbf2f (CWeapon::DoBulletImpact): CONFIRMED against exe 0x73B550-0x73C1BB (self-shot exit, null firedBy, skill block keyed on firedBy ped). Its FireProjectile hunk is only a null guard with a mislabelled address comment.
Zone: 627 never-audited rows (Entity/** + gameplay .cpp, render-only excluded, P4R7* rows removed); compared 10 (incl. the review).
Fixes: 1208ec9ba CAutomobile::AddMovingCollisionSpeed (dumper without misc node -> multiplier 0); 604c15bb3 CPedGroup::GetClosestGroupPed (out dist stored always, 1e8 when none).
MATCH: SetPursuitCop, ComputePursuitCopToDisplace, FindNearestObjectOfType, AddMember, GetAllWheelsOffGround, DoTankDoomAiming, GetNumFiresInArea.
Rest NOT_REACHED (budget). Next in size order: DoDriveByAutoAiming, SteerAICarBlockingPlayerForwardAndBack, TestCollisionBetween2MovingRects, Garages AddOne.
