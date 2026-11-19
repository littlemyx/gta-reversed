# ASM-P4R2m: CJ / ped movement functions without a verdict

Checked 18 functions against the exe (see .notes/asm_sweep/P4R2m.result.tsv): 0 deviations, 0 fixes, no source changes (no build needed).
ProcessPlayerMovement is CTaskSimplePlayerOnFoot::PlayerControlZelda (already MATCH). Other CPedIK members already had verdicts.
The P4R2l UNSURE (CAnimBlendNode::GetCurrentTranslation lerp order) is resolved MATCH, as are GetEndTranslation and both Compressed variants.
Not reached: CalcTotalTime, UpdateTime, NextKeyFrameCompressed, UpdateCompressed, Uncompress, RpAnimBlendClumpInit.
The RYDER_1 CJ drift is therefore not caused by any function checked here.
