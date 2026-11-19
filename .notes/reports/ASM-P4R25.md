# ASM-P4R25

14 functions compared (3 FIXED, 11 MATCH); the rest of the CarCtrl/PathFind steer/weave/park rows NOT_REACHED (budget).
- CWeapon::FireM16_1stPerson: the shot target was origin+front*3, exe uses front*3*WeaponRange.
- CWeapon::FindNearestTargetEntityWithScreenCoors: extended-precision sums.
- CShotInfo::Initialise: RandTable running value kept unrounded.
Build: StandaloneRelease links. No game run.
