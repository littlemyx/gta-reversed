# ASM-P4R4b
- Review: 89bc567c4 IsInSeeingRange CONFIRMED, 36a744502 FindRespectedFriendInInformRange CONFIRMED (x87 stack walked).
- Walked/compared (full): PickNextNodeToChaseCar, PickNextNodeToFollowPath, IsThisAnAppropriateNode: all MATCH, no fixes.
- UpdateCarAI: already fully covered by S42b/S42c (968e04e14, c08957acf); not re-walked.
- ReconsiderRoute head only (UNSURE). Remaining S22 NOT_REACHED CarCtrl rows (~28, e.g. DealWithBend_Racing, CreateConvoy, CreatePoliceChase, GenerateCarCreationCoors2, SteerAI*, heli/plane) still open; the ~25 unwalked zone functions not reached (budget).
- Commits: result+report only. No source changes, no build.
