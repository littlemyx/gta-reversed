# ASM-P4R7j: never-audited 100-250 byte functions (Vehicle/Automobile/Train/Ped/Physical)
Method: structure-first (calls, branches, constants, offsets, jump tables decoded from the exe), instruction-level only on differences.
Result: 35 functions, 35 MATCH, 0 fixes, 0 UNSURE; no source changed (no build needed). Table: .notes/asm_sweep/P4R7j.result.tsv.
Caveats: PedIsReadyForConversation task/event ids (0x4B4, 0x13) not re-verified against enums; CPlayerPed::GetWantedLevel null path differs (exe crashes).
Not reached: Task/Event list (w_out.txt), CarCtrl, Population, Pad, PathFind, ControllerConfigManager rows.
