# ASM-P4R13b
Review of P4R13 fixes (KillCriminal 0x68DC02, WalkAlongsidePed 0x685598, FollowNodeRoute 0x6695A1, FollowPointRoute 0x66E454): all four CONFIRMED against the 1.0 exe (fcomp + test ah,5 / jp skip = strict <; test ah,0x41 / jne = <=).
Sweep: ~14 further sites, 1 fix: 1aabc0b9f CPopulation::ManagePed (thresholds scaled by the creation multiplier, strict compare, no dist division).
Not reached: RoadBlocks, rest of SlowCarDownForPedsSectorList. See P4R13b.result.tsv.
