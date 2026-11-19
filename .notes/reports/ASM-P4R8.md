# ASM-P4R8: never-audited functions in the other game_sa subdirectories

Zone counts (noverdict, >= 0x30 bytes, game_sa subdirs minus the excluded ones): 368 rows total
(Models 84, Core 67, Collision 62, Ragdoll 45, Interior 42, Animation 39, DecisionMakers 16, Attractors 6, Plant 5, Text 2).
Most of the bodies already carry exe-address / x87 audit comments; the exe sizes in noverdict are inflated (many are 1-5 instructions).

Compared: 15 (13 MATCH, 2 partially, tails NOT_REACHED). Fixes: 0. No source changed, no build needed.
Everything else is NOT_REACHED (budget); see P4R8.result.tsv.
