# ASM-P4R4 (ped AI per-frame + car AI, no-verdict functions)
- Review c0bdcae69 (SET_NO_RESPRAYS): CONFIRMED (exe 0x47B992-0x47B9A5 NoResprays=(p!=0), push !NoResprays).
- Compared 23 rows: 2 FIXED (IsInSeeingRange, FindRespectedFriendInInformRange: x87 sum order), rest MATCH. TaskManager helpers, EntityScanner/VehicleScanner, small CarCtrl/CarAI helpers all match.
- Not reached: CCarAI::UpdateCarAI, CCarCtrl::PickNextNodeToChaseCar (and other NOT_REACHED CarCtrl rows from S22).
- Commits: 0f95bfc94, d28cd677c.
