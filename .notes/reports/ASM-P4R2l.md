# ASM-P4R2l
Part 1 of the brief (rest of the anim callbacks and helpers). No source changed, no fixes: ~38 rows MATCH, 1 INTENTIONAL (ExtractAssociations null guard), 1 UNSURE (GetCurrentTranslation lerp spill order), rest NOT_REACHED (see .notes/asm_sweep/P4R2l.result.tsv).
- All 6 FrameUpdateCallBack variants + dispatchers match the templated port (FrameUpdateCallBackT); compressed variants differ from their uncompressed twins only by the compressed calls.
- RpAnimBlend association/frame helpers, ClumpData members and key-frame accessors match.
- Part 2 (ped movement physics) not reached: budget. The CJ position divergence is not explained by anything in this slice.
