# ASM-P4R18g (Tasks, last rows)
Compared 8 (review of 8b997070e + 5 P4R18f NOT_REACHED rows + 2 x87 compares): 5 MATCH, 3 FIXED. No other no-verdict Tasks rows remain (all sel.tsv rows have a verdict in P4R18*.result.tsv).
- 8b997070e CONFIRMED against exe tables (0x64C89C / 0x64C88C).
- ComputeMoveDir: exe normalises when !(sumSq < 1e-8) (test ah,5; jp), port used > (equality / NaN differed). ec6eae9e7
- KillPedOnFootMelee::CreateFirstSubTask: UpdateTargetDistSq now x87d (z,x,y of unrounded fsub results) and range*range unrounded; CreateFirstSubTask now uses the zxy path (exe 0x62BE8A). ed181dbf1
- MedicTreatInjuredPed::CreateNextSubTask: dx,dy unrounded, fsqrt(dy*dy+dx*dx), only dz via float. 8f6b77d53
Build: StandaloneRelease ninja link, see result in commit message of the landing.
