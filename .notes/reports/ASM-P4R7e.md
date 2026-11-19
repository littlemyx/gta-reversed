# ASM-P4R7e

Compared 9 functions (zone rows reached: 13 listed), 1 fix, 4 NOT_REACHED (budget).

- CWeapon::DoBulletImpact: all five P4R7d fixes confirmed against 0x73B550-0x73C1BB. New fix ce00fa891: victim==shooter ped jumps to the epilogue (no pad shake, 0x73B8A1); null firedBy tolerated; skill-stat block keyed on firedBy ped (exe 0x73B740), not on player (port would deref null for a non-player shooter with STATUS_PLAYER).
- CWeapon::DoDoomAiming: MATCH (float-vs-x87 noise only).
- CWanted: WorkOutPolicePresence, UpdateWantedLevel, Initialise, SetWantedLevel MATCH; IsClosestCop/AddCrimeToQ/UpdateCrimesQ were already exe-audited (never.tsv false positives).
- NOT_REACHED: CWanted::SetPursuitCop, ComputePursuitCopToDisplace, CAutomobile::AddMovingCollisionSpeed, CWorld::FindNearestObjectOfType.
