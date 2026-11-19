# ASM-P4R6: global "no verdict" sweep, part 1 (Tasks + Events)

Generator: tools/standalone/asm/noverdict.py (stdlib + oracle_coverage inventory; covered = any MATCH/FIXED/INTENTIONAL/SUPERSEDED/DEVIATION_FIXED/CONFIRMED/RESOLVED/TRIVIAL row in .notes/asm_sweep/*.tsv; AUTO_MATCH is triage only, not a verdict).

## noverdict totals (9248 hooked sites, 4514 without a verdict at tip 221becb0d + P4R6 fix)
| top-level dir | no verdict |
|---|---|
| source/game_sa/Tasks | 1248 (of 2188) |
| source/game_sa/Entity | 284 |
| source/game_sa/Audio | 236 |
| source/app | 85 |
| source/game_sa/Pipelines | 38 |
| source/game_sa/Fx | 34 |
| source/game_sa/Plugins | 17 |
| source/game_sa/RenderWare | 5 |
| source/game_sa root files (Camera, Pad, Radar, ...) + Events (incl. EventHandler/Group/Scanner) + Models/Collision/Animation/Scripts/... | 2567 (Events alone ~337) |

Zone (Tasks + Events + EventHandler/Group/Scanner): 1645 rows, 1109 of them >= 0x30 bytes (Tasks/ and Events/ directories alone: 1622 in the final rerun; the extra rows are the EventHandler/Group/Scanner files). Size is an estimate (next hook / first int3 after ret); ctor/dtor rows with shared code are over-estimated.

## Compared: 22 functions, 1 fix
Verdict file: .notes/asm_sweep/P4R6.result.tsv. Focus: MakeAbortable family (cheap, bug-prone), plus a few Event/Task helpers.
- FIXED CTaskSimpleInAir::MakeAbortable: exe clears ped flag bit 9 (bIsInTheAir) on every abort, port left it set. Commit 63e82cf7e.
- 21 MATCH. NOT_REACHED: remaining ~1085 zone rows (budget).

Build: StandaloneRelease full rebuild + link OK (ninja -j4). No game run, no tests (per brief).
