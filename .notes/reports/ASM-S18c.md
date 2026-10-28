# ASM-S18c (Heli.cpp + Plane.cpp leftovers of S18b, 11 functions) - 2026-10-28
Result: 8 compared (1 FIXED, 7 MATCH), 3 PARTIAL/not reached (token budget). Table in S18c.result.tsv.
Fix (commit 25feddf7): CPlane::VehicleDamage (exe 0x6CC4B0-0x6CCCF0): the collision-branch threshold maths keeps mass/10000 unrounded on the x87 stack (exe 0x6CC57B..); the port rounded the quotient to float first, so damageThreshold/destroyThreshold could differ by 1 ULP.
MATCH: CHeli::GenerateHeli, UpdateHelis, ProcessControl; CPlane::ProcessControl, ProcessControlInputs, BlowUpCar, DoPlaneGenerationAndRemoval.
Notes: CPlane::ProcessControlInputs computes forwardSpeed/steering in float while the exe keeps the intermediate unrounded on the x87 stack (ULP-level, left as is, same class as above but with no threshold effect). CHeli::ProcessControl switch default (wanted level >6) reads leftover stack in the exe; port NOTSA_UNREACHABLE, unreachable in practice.
Open ranges for a follow-up slice (S18d): CHeli::SearchLightCone 0x6C58E0-0x6C6520; CPlane::PreRender 0x6C94A0-0x6CAB70; CPlane::ProcessFlyingCarStuff 0x6CB7C0-0x6CC4B0.
Consumer audit: no signature/layout change.
