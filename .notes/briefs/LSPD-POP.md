# LSPD-POP — who populates the LSPD interior: the original shows no PED_TYPE_COP peds, our port 2-4 cops

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh lspd-pop.
## Evidence (.notes/reports/REGRESS-FLAKES.md, "open findings"; .notes/reports/PT2-INTERIOR / PT2-TRAILER reports for U15/U16 history)
gameplay_regress interior_lspd with the new original-exe leg (-o): in 2 original runs CJ enters interior 6 (LSPD) with a wanted level and is
shot to death within ~25 s, but the poller sees no PED_TYPE_COP ped within 60 m (and no unreadable positions). Our port has 2-4 cops there.
Also in 1 of 3 port runs two interior cops are +13 dm above CJ at 4 s, 0 by 10 s (U15-like transient).
## Task
1. First verify the poller on the original: read the ped pool there (stride 0x7C4, ped type at 0x598 per PT2-TRAILER) and list every ped
   near CJ in interior 6 with type, model, area code, task. Who shoots CJ in the original — cops of another ped type, or the interior's
   own script/attractor peds?
2. Compare with our port's list. If the populations differ, find why (PopulateInterior / CInteriorGroup / script spawns / ped type
   assignment) against the asm; fix as the exe (one commit per fix, asm address). If the poller was wrong, fix the poller.
3. The +13 dm transient: does the original show it too (sample its cop z at 1 s intervals)? If not, find it.
## Acceptance
Ped lists for both exes in the report; root cause or "same as exe" with evidence; interior_lspd original and port legs agree; soak OK
before landing (v4 rule 3). HARD budget under 250k tokens.
