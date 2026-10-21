# LSPD-POP (Windows, rules v4, 13:18-~14:25) - who populates the LSPD interior

## Verdict
Two port bugs in the attractor population (CPopulation::AddPedAtAttractor / GeneratePedsAtAttractors), plus a wrong poller on the original. The original DOES have cop-type peds in the station
(9-10 of 15 peds); the "no PED_TYPE_COP" reading was the poller (ped-pool stride 0x7A4 instead of 0x7C4, type at +0x594 instead of +0x598 - the PT2-TRAILER 2b bug, which `orig_gr_poll.py` still had).
The "+13 dm transient" is NOT a port bug: the original keeps cops at +14 dm for the whole 15 s it was watched (they stand on an upper walkway of the station).

## 1. The poller (commit 0badf286)
`tools/standalone/gameplay_regress/orig_gr_poll.py`: stride 0x7C4, type +0x598, intelligence +0x47C, contact +0x584; the area-code filter of `cops40` is gone (the exe's interior peds carry area 0, like ours);
new `[ped-o]` per-ped lines (pos, d, area, type, model, hp, state, contact, task chain) and `copstk=`.

## 2. Ped lists (interior 6, CJ at 246.8,63.9,1003.6, 1 s after entering; types: 4 CIVMALE, 5 CIVFEMALE, 6 COP)
| | peds within 60 m | cops (model 280) | civilians | notes |
|---|---|---|---|---|
| original exe (run o1) | 15 | 9 (10 later) | 3 x type 4 (models 17, 22, 58), 3 x type 5 (13, 10, 56) | all area 0 (currArea 6); 7 peds with an attractor chain p2:233>235>1800>401, the rest p3:912>900 / p4:203; 2 in the next room (contact area 10). CJ: wanted 2 at ~+19 s, hp 100 -> 0 in ~10 s (the cops shoot) |
| original exe (run in p5) | - | max 7 within 40 m | | dz of the cops (dm): 13, 0, 0, 0, -3, 13, 13, ... |
| port BEFORE (13:20 build) | 16 | 4 (3 chasing + 2 far ones in 1307 wait-for-area) | 11-12, models 69,142,229,226,56 repeating in a 5-cycle | PopulateInterior n=15: attractors=12 (all ENT 14846, type 5, no name, one ped per pass: the same 5 models cycling), node peds=3 (all cops) |
| port AFTER | ~15 | 5-8 | rest | PopulateInterior then spreads the 15 peds over several attractors and nodes |

Cops +14 dm in the original, per second after the teleport (dm, cops within 40 m): s1 [-26,0x7,14], s5 [-12,0x6,14,14], s13 [0x7,14,14] ... two cops stay at +14 for the full 15 s.

## 3. Root cause (asm)
`CPopulation::AddPedAtAttractor` 0x614210: at 0x614250..0x6142B1 the exe scans the whole ped pool inline: min of `dx*dx+dy*dy` (XY only, squared, float start 0x863A3C = FLT_MAX) against
the float at 0x86D26C = 0.1225 (0.35 m squared); `<=` -> return false. The port called `FindDistanceToNearestPed` (3D, sqrt'd) `<= 0.015`, so a second ped could be created on the very spot of the first
(PlacePedAtEffect puts the ped on the effect). The LSPD interior has ONE ped attractor (model 14846) in range, so `GeneratePedsAtAttractors` filled it with 12 peds (one per pass) and only 3 node peds
followed. Attractor peds are never cops (the attractor has no script name, `PedMICanBeCreatedAtThisAttractor` rejects the cop model), node peds are cops with 70 % (`FindNewPedType`, bInPoliceStation): 4 vs 9-10 cops.
Fix a7b39d64: the exe's rule.
`GeneratePedsAtAttractors` 0x615970 (ff4685f3): the 12-pass limit counts consecutive passes without a ped (0x615C2E `mov [esp+0x2c],0` after each created ped), the remaining count is re-checked
at every entity (0x615A00), decisionMaker -1 returns after the first ped (0x615C1E), model <= 0 || == 7 is skipped (0x615BE3/E7). (The port could create up to 12 peds for a request of one.)

## 4. Consumer audit (rule 2)
No shared state changed: AddPedAtAttractor has one caller (GeneratePedsAtAttractors; the debug menu TwoDEffectsDebugModule calls it directly and now gets the exe's refusal rule); GeneratePedsAtAttractors is
called by the street population (dm -1, 1 ped) and PopulateInterior (dm 7); both only use the returned count. `FindDistanceToNearestPed` itself is unchanged (other readers).

## 5. The +13 dm "transient" (brief item 3)
Original: two cops at +14 dm (z 1005.0 vs CJ 1003.6) stay there 15 s; others at -12 / -26 dm. These are the exe's node heights (upper/lower walkways). The port's earlier "0 by 10 s" runs just had no cop
spawned on those nodes. The old check (|dz| <= 10 dm) was not a property of the exe; replaced by `copstk` (cops at CJ's XY, > 0.5 m above him = the U15 symptom, must be 0 in every in-interior snapshot).

## 6. Results
- interior_lspd, port + original leg in one run (`gameplay_regress.sh -o interior_lspd`, p5): PASS=8 (cops_present 5, cops_on_floor copstk [0,0,0,0], orig_reference: entered, 7 cops within 40 m, dz 13/0/-3 ...; cops_react engaged 1-4, hp 100 -> 50).
- Before the checker change but with the fixes (p4): cops 7-8, 12 settled dz values with +13 dm in 5 of them -> the old limit failed (hence the change of the check).
- Soak (NOVSYNC, 3 min route 393 s, new game): RESULT OK, 0 errors, 37000 frames, 24.9 fps. gameplay_regress combat + interior_lspd (p6): 21 PASS; interior_lspd alone p7: 7 PASS; three consecutive runs with the new check (p5, p6, p7) PASS (cops 5, 7, 5).

## Files
a7b39d64, ff4685f3 (Population.cpp), 0badf286 (harness: orig_gr_poll.py, gr_check.py, baseline.json, GameplayRegress.cpp `copstk`, gameplay_regress.sh PROBE for interior_lspd).
Scratch: C:/gta-scratch/lspd_pop/{o1 (orig), p1..p5 (port), soak1}.
