# ASM-P4R23g: ReturnInteriorNodeIndex review + ManageAllPopulation + last ped/world rows

Review of d66aa0979 (CPathFind::ReturnInteriorNodeIndex, exe 0x451300): CONFIRMED (hidden return pointer + intId + nodeId, ret 0xC).
Compared 5 functions: CPopulation::ManageAllPopulation (FIXED: dummy pool walked in reverse), CPad::StartShake_Train (FIXED: `<=` 4900 compare, unrounded x87 chain),
BuildRotatedDoorMatrix, FindPedRaceFromName (MATCH). ManageObject/ManageDummy already had verdicts (S23b).
All other zone rows of cand.tsv already have a verdict in earlier result files; only these two had none.
