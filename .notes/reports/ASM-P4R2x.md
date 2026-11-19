# ASM-P4R2x: review of P4R2v/w fixes + CGame::Process walk remainder

Part A: all 6 fixes CONFIRMED against the exe (6ca59be1d, 3ef399e37, 9de3e9a62, a1bfaf569, 8c7bc86e7, cc2c59f6d); no corrections needed.
Part B: open list recomputed from all result TSVs against walk.tsv: 35 rows left (not ~65; P4R2s..w closed more); 33 compared, all MATCH, 0 fixes; open: CCamera::Init (skipped per brief), CCheat::ApplyCheat (not reached).
No source changed, so no build. Result: .notes/asm_sweep/P4R2x.result.tsv.
