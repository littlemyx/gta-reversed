# ASM-P4R19b
Review of P4R19: all three fixes CONFIRMED against the exe (WhizzedBy 0x4BBE5B, VehicleDied 0x4BA8D1, VehicleOnFire 0x4BB306 untouched-on-null; OnFire Die weapon arg 0 = UNARMED; Died weapon 0x33 = EXPLOSION correct).
Class scan: the `return nullptr` vs untouched pattern is inert elsewhere, because ComputeEventResponseTask clears m_EventResponseTask on entry and nested calls already return the field. No source change.
New compare: BuildingCollision MATCH, PedCollisionWithPlayer MATCH (tail after 0x4BF19F structural only). NOT_REACHED: ComputeDamage, VehiclePotentialCollision, PersonalityResponseToDamage, event ctors/Clone/priority getters.
Commits: none (no source change, no build needed).
