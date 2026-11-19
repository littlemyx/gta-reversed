# ASM-P4R7g
Review: e09878341 (AddMovingCollisionSpeed) CONFIRMED; 64d21d8cb (GetClosestGroupPed) CONFIRMED.
never.tsv open rows 637 (of 657, 20 done by P4R7*): compared 14 groups (about 20 functions); 5 are AUDITED-IN-SOURCE/MATCH
(DoDriveByAutoAiming, Steer..ForwardAndBack, TestCollisionBetween2MovingRects, AddOne, ChooseGangCarModel).
Fixes: 0. No source changed, no build needed. Heuristic scan: 96 rows (size>=250) have no exe/x87 comments in source:
list in C:/gta-scratch/p4r7d/scan.tsv (col7==0) - candidates for the next slice (UpdateWeightInRope, Buy, FindObjectsOfTypeInRange, CTrain::FindCoorsFromPositionOnTrack, SetPanelDamage ...).
