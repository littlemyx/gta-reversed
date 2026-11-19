# ASM-P4RV2: independent re-review of old S-series FIXED rows, vehicle/world half

Candidates: 147 FIXED rows in old S*/P4S1-6 TSVs (vehicle/camera/object/garage/pickup/CarAI classes) not re-confirmed by a later TSV.
Reviewed (largest and most-executed first): 14 fix commits, checking the load-bearing hunks against gta_sa_compact.exe.
Result: 10 CONFIRMED, 4 PARTIAL (not all hunks re-read), 0 WRONG; no source changed, no build needed.
Not reached: the other ~133 rows (CCam::Process_* family, CCarAI small funcs, CPickups, CVehicle lights, CTrailer, CPlayerInfo, CTrain generation, CObject remaining).
Per-row verdicts: .notes/asm_sweep/P4RV2.result.tsv
