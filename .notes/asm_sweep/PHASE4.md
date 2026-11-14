# ASM sweep phase 4: finish the port (2026-11-14, branch reverse/interior_c, tip 33940b020)

Trigger: the user said the port was presented as done while it is not. "100%" was hook coverage only; it never meant verified.

## Honest numbers (latest verdict per address over 285 S*.result.tsv, scratchpad latest.py)

| what | count |
|---|---|
| hooked addresses | 9492 |
| asm-compared, latest verdict MATCH / FIXED / DEVIATION (fixed) / DEVIATION_FIXED | 3165 / 925 / 252 / 12 = **4354 (45.9%)** |
| REPLACED by the platform layer (SDL3/librw/portable), never compared | 94 |
| oracle-test only, never asm-read | ~5068 |
| open sweep rows (NOT_REACHED 25+6 to re-check, UNSURE 3, MATCHORACLE 3, NA 1) | 38 |

Corrections to the 14.11 read-only audit: it counted verdict ROWS, not functions. 2783 NOT_REACHED rows are budget carry-overs that later continuation slices closed (25 remain); all 252 latest DEVIATION rows carry a fix commit (246 found on the branch by message; hashes changed on rebase). plugin::Call* is a compile error in the detached run build (PluginBase.h NOTSA_PLUGIN_CALL_FORBIDDEN): the ~107 left are DLL/oracle-build only. The run build plays without the exe (gameplay_regress 70 scenarios incl. missions, soak); "boots to menu" was a 10.10 note.

**Fixed progress metric for the C++ port: asm-compared functions / hooked addresses (now 45.9%).** Move it only by landed result rows.

## Remaining work, in order

1. P4-LEFT: the 38 open rows (one agent).
2. P4-A1: random sample of 40 oracle-only gameplay functions, measure the deviation rate (one agent). The rate decides the pace of step 3.
3. Oracle-only sweep, ~5068 functions (~130 slices + continuations): first what missions and the TS port touch (scripts/commands, tasks, events, peds, vehicles, world, physics), first entries of web/docs/cpp-suspects.md on branch web-ts-port of littlemyx/gta-reversed-private; then the rest; tier C last.
4. Independent second pass on a sample of sonnet MATCH verdicts (error rate of the sweep itself).
5. README progress line = the metric above.
6. After every gate that moves interior-c-ref: give the TS session the list of changed functions (git diff of source/game_sa between snapshots).

Pace: 1-2 sonnet agents at once (quota shared with the TS session, ~12%/day there). Gate (orch_gate.sh regress) after each few landings.

## Process changes (2026-11-14, from the peer audit cpp-sweep-audit.md; phases 1-3 cost ~61M tokens, ~14k/function)

Order: (T1) shared annotator + exe-vs-our-build signature diff (calls, immediates, float literals, branch count, size) in tools/standalone; AUTO_MATCH is triage only, not counted in the metric; mismatches go to agents with the diff list. (T2) class scanners (ftol/int truncation, neighbour constants, bit fields, jcc inversion) over all oracle-only rows. (T3) hit-counter build over hooked addresses, run on the 70 scenarios + soak; intersect with web PORT_MAP partial/not-ported + sweep2-list.md. Then: script opcode handlers first, then high-defect classes (Scripts, Camera, CWaterLevel, CRadar 59-73%), low-defect classes (Loaders, HUD, Interior_c 0-17%) only via T1/T2. Second pass: 30-row sample of sonnet MATCH first.
Bookkeeping: one result commit per slice, metrics by script, rules v1-v6 merged into a single v7 (<=6 KB), full gate once a day. Agents: 1 at a time (shared quota; TS port has priority).
