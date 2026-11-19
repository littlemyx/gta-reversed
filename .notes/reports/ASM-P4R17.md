# ASM-P4R17
Review 7a04ec197 (CTaskSimpleCarFallOut::FinishAnimFallOutCB): CONFIRMED against exe 0x648EE0 (status 0->1, 2->3).
Level-2 list: C:/gta-scratch/p4r17/l2.tsv (29 functions >= 0x30 bytes without verdict; ~10 are CRT/RW, unnamed).
Compared 10 (all MATCH, 0 fixes); 8 gameplay functions NOT_REACHED (budget): CalcBuoyancyForce, CopyToRwMatrix, TransformFromObjectSpace, UpdateMatrix, CPathNode::GetPosition, AddToList1, FxSystem_c::Stop, SetTranslate.
No source changed, no build needed. level4.tsv of P4R16 was re-checked (no overlap).
