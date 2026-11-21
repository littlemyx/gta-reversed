# ASM-Z14: independent review of Z13 fixes + callee spot-compare

## A. Review of the five Z13 fixes (exe oracle, exedis)
All five CONFIRMED (lines appended to C:/gta-scratch/review_results.tsv): 31ca9b75a (0x480148), f294480d6 (0x47D78B/0x47D813), 2eac021b7 (0x47379B), c821191fe (0x47ADB9), ec0e527ec (0x47FEEC).

## B. Callees without a verdict
Method: executed functions in hitcount/ryder1.tsv with no verdict in any .notes/asm_sweep/*.tsv, class-filtered (render/audio/occlusion skipped). Few remain; the ones compared are in Z14.result.tsv:
14 rows, all MATCH, 0 FIXED. Partially re-read rows (IsWanderPathClear water loop, TransitionFinished tail) are marked in the note column.
Left uncompared: CCover::Update, CPedGroupMembership::RemoveMember, CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle, CEntryExit::TransitionFinished tail, CEntryExitManager::LinkEntryExit, CStreaming::LoadScene/RequestSpecialModel.
No source change, so no build; docs-only commit.
