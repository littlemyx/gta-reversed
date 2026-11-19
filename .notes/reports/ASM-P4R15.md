# ASM-P4R15: close-out of never-audited lists

- Review fc3ccbca1 (CModelInfo::ReInit2dEffects 0x4C63B0): CONFIRMED. Exe loops 20000 slots, tests ecx before Init2dEffects (0x4C63C2); port now skips null.
- Tasks/Events small rows (29, list = C:/gta-scratch/p4r6d/w.py minus all result.tsv): 28 MATCH, 1 FIXED.
  - FIXED 391375dbb CTaskSimpleCarFallOut::FinishAnimFallOutCB (0x648EE0): exe does door status 0->1 / 2->3 (open), port called SetDoorClosed_Component (1->0 / 3->2).
  - Note: CTaskSimpleUseGun::FinishGunAnimCB compares m_BurstShots as signed word in the exe; port uint16 (differs only above 32767 shots, not reachable).
- CModelInfo Is*Model (11) + IsVehicleModelType + Add*Model (9) now disassembled: all MATCH (store bases, strides 0x20/0x24/0x28/0x308/0x44, virtual Init +0x18, vehicleType constants).
- Result rows: .notes/asm_sweep/P4R15.result.tsv. No game runs, no tests beyond the build link check.
