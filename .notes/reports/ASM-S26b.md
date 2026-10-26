# ASM-S26b - Physical.cpp asm sweep, the 9 functions S26 did not reach (partial)
Method: exedis vs source, rules v5. 7 of 9 compared (ProcessCollisionSectorList head only), ApplyCollision-2 (0x548680) and ApplySoftCollision-2 (0x54A2C0) NOT_REACHED (token budget). Details: S26b.result.tsv.

| function | exe addr | fix |
|---|---|---|
| ProcessCollision | 0x54E760 | keep-processing test is bRestingOnPhysical (byte 0x42C & 2), port used bIsCarParkVehicle |
| ProcessShiftSectorList | 0x54674E.. | lists walked objects,peds,vehicles,buildings; per-entity "process entity collision" flag; IsSubTrain of this; m_pAttachedTo branch not exclusive; magnitude (x2+z2)+y2 |
| PositionAttachedEntity | 0x547677, 0x5476E1, 0x5479D9, 0x547A67 | detach force mass*0.01 (port 0.02); lift force up*(mass*0.02); heading difference unrounded FPATAN; turn *1e-5f instead of /1e5 |
| ApplyFriction(float,CColPoint&) | 0x5454DC..0x545699 | (z2+y2)+x2 magnitude, direction = diff*(1/mag), general-branch dot order y,z,x |
| ApplyCollisionAlt | 0x544EF9, 0x5452FB, 0x545421 | dot (x,z,y); move/turn speed multiplied by 1/mass (exe operator/ 0x4119D0 = reciprocal multiply) |
| ProcessCollisionSectorList_SimpleCar | 0x54D050 | objects list before vehicles list |
| ProcessCollisionSectorList | 0x54BB50 | repeat lists buildings,vehicles,OBJECTS,PEDS (head 54ba60-54bd5d only; body not compared) |

Systemic finding for the orchestrator: CVector operator/(vec, float) (Vector.h:255) divides, the exe's operator/ (0x4119D0) multiplies by 1/x (float-rounded). Other callers with `vec / scalar` (grep in Physical.cpp: ~15 `/ m_fMass`-style sites, also elsewhere) are candidates for the same 1-ulp deviation; not changed globally because inlined exe code elsewhere does divide.
Soak-at-exit "Segmentation fault": NOT from physics. It appears after "atexit handler ran" (exit path, route reached quit_ask, RESULT unaffected) and is intermittent: gate_soak logs 1023-1625, 1025-0416, 1025-1600, 1025-2244, 1026-0701 have it, 1026-1847 and ~15 others do not; it predates ASM-S23b/S26. Not investigated further (out of slice).
Consumer audit: no signatures/layouts changed. Gate: Release build OK; soak.sh NOVSYNC RESULT: OK (0 errors, no Segmentation fault line this time); physics_oracle does not cover the touched functions (ApplyFriction-other 0x545980 untouched); full suite = orchestrator gate.
