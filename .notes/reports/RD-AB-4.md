# RD-AB-4 - orig-vs-port divergences at frames 10 and 15 (population)

Agent RD-AB-4, rules v5, 2026-10-25 04:40 - ~06:20 WEST. Worktree C:/gta-wt/rd-ab-4 (branch rd-ab-4 on b6ef2518). Data C:/gta-scratch/rdd/ab4/r1..r10, own proxy RD_PX=C:/gta-scratch/rdd/px_ab4.

## Divergences (0-based traced frames, pin 1023, intro scenario)
| frame | layer | cause | fix / exe address |
|---|---|---|---|
| 10 | rng 306 vs 184 calls, peds 4 vs 3 | `CPathFind::GeneratePedCreationCoors` (exe 0x44E790): the exe keeps distSq and dist in ONE stack slot [esp+0x20]. 0x44E8AD stores distSq there, 0x44E97E (`fld [esp+0x20]; fsqrt; fstp [esp+0x24]`, i.e. the same slot) is executed for EVERY link that passes the link-level checks, so the 2nd and later links of a node see sqrt(previous dist) (65 -> 8.06 -> 2.84) and always pass `dist < maxDist1`. The port recomputed sqrt(distSq) per link, so it rejected the 2nd link of far nodes (node 15/699, dist 65 > 50.5) that the exe accepts (+5 failed tries = 5 rand draws; the whole call took 201 vs 47 rand calls). Found with an int3/VEH breakpoint at 0x44EA06 in the original (rd_orig.c RD_BP): the same node was hit twice at one rand count, the 2nd hit showed [esp+0x20] = 8.0598. Path-node data (DUMPPN, 6 areas, 28 B/node), camera gen. multiplier, ped counts, args were all equal. | PathFind.cpp: `distSlot` written back by each link |
| 15 | rng 145 vs 141 calls | `CTaskSimpleCarDrive::TriggerIK` (0x63C500): the exe returns BEFORE any rand unless `m_autoPilot.m_nCarMission` is 2..6 or 0xF/0x10 (0x63C50E..0x63C543); the port ("early-out refactor") drew `GetRandomNumberInRange(0,100)` for every mission = 1 extra draw per driving ped per frame. Seen by the orig stack probe RD_RSTACK=407100-407200: orig draws only from 0x642906 (ProcessHeadBopping), 0x642BDA (ArmBopping), 0x644B29. | TaskSimpleCarDrive.cpp: mission gate before IsLooking/rand |
| 19 | rng 241 vs 303 calls | not analysed (out of time). Port: ped group placement (PedGroupPlacer) is followed by long GenerateCarCreationCoors2 walks (a6c28c/a6c29e pairs) that the orig does not have; ped-group part has equal rand counts per group (9) | - |

## Equal prefixes (orig-port, pin 1023, intro; 3 reps for the last row)
before (RD-AB-3 tree b6ef2518): 10 / 10 / 10 -> after the GeneratePedCreationCoors fix 15 (1 rep, r8) -> after the TriggerIK fix 19 / 19 / 19 (3 reps x 1500 frames, r10). Self-identity in the same r10 run (own proxy, pin 1023): orig-orig 1500 / 1500 / 1500, port-port 1500 / 1500 / 1500 (all 6 pairs equal over 1500 frames).
The earlier open item "CStreamingInfo TXD ids 20014.. in different pairs" was not needed for frames 10-19 (stream state is not the cause of any divergence found here); not investigated.

## New diagnostics (inert without the env vars; rd_diff.py ignores D/Z/K/ST lines)
* D line: `gd=` CCamera::m_fGenerationDistMultiplier, `np=` NumOther/Cops/Gangs/Dealers_Peds (4 floats), `pcnt=` the 20 CPopulation counters 0xC0EC24..0xC0EC70 (both sides).
* `NOTSA_LOCKSTEP_DUMPPN` / `RD_DUMPPN=<prefix>`: raw path nodes (area, nveh, nped, 28 B per node) of every loaded area at traced frame 10 (compare with a few lines of Python, see scratch pncmp.py).
* rd_orig.c: R lines now carry the rand() VALUE (`R fc retaddr value`); `RD_BP=<hex addr> RD_BPFC=<fc>`: int3 + vectored handler (re-armed by single step) logging `Z fc ncalls regs`, `K` 72 stack dwords, `ST` the FPU registers (ST0..ST7 in context order) at each hit during traced frames fc, fc+1 - a poor man's debugger for the original exe without leaving the replay-diff determinism pins.
* tool tip: `rcmp.py <repo> <exe> o.trace p.trace K K` uses K = number of F lines before the R lines (rcmp frame K+1 = report frame K); `resolve_stack.py ... --frame <fc>` needs the fc of the R lines (1048576 + k).

## Gate (rules v5)
Release build (StandaloneRelease, this tree) ok. Soak (gpu_locked, NOVSYNC+MUTE, 3 min request = 393 s route): first soak after the PathFind fix RESULT OK (errors 0); second soak after the TriggerIK fix RESULT OK (errors 0). Oracle targets: none touched/built (no oracle row for GeneratePedCreationCoors or TriggerIK exists; both fixes asm-proven + A-B-proven). Not run: run_all_tests (orchestrator).
Consumer audit: distSlot is local to the function; the TriggerIK gate only removes a draw/IK start for missions the exe also ignores.
