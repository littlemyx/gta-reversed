# ASM-S27c (World.cpp, 5 functions) - 2026-10-28
Result: 3 compared and FIXED (3 commits), 2 NOT REACHED (token budget; table in S27c.result.tsv), 0 UNSURE.
Fixes:
- TestSphereAgainstSectorList (0x566140): scan code is set before the ignore/usesCollision/camera checks; the bound-sphere radius comes from the model-info colmodel (83a0ad0a).
- TriggerExplosionSectorList (0x567750): see the result table. Largest: static objects were damaged with 1.0 instead of x300, non-object statics reached AsObject()->ObjectDamage, the vehicle sphere centre used TransformVector instead of 0x59C810, normal.z threshold -0.2 (was -0.0), vehicle force dot lacked the mass factor and the vehicle got ApplyTurnForce instead of ApplyForce, plane damage normal was not negated, ped knock direction was not negated, crime type used the creator's ped type instead of the victim's, CEventDamage last arg bInVehicle (was bIsTalking), reciprocal multiplies and dot/length sum orders (28fbab83).
- Process (0x5684A0): inactive ped groups no longer receive global events; shift damping uses 0.707f and the raw ms_fTimeStep (was sqrt2/2 and 20x time step); the explosion ped z-force had the same 20x time step (7a88dc0f).
NOT REACHED: ProcessLineOfSightSector (0x56B5E0-end), ProcessLineOfSightSectorList (0x566EE0-end).
Gate: Release build OK (1186/1186, StandaloneRelease). No wave2 oracle row covers these functions; StandaloneRW was not built (budget). combat gameplay_regress: PASS=14 (NOVSYNC, StandaloneRelease build of this branch).
