# ASM-HC2 (executed-but-unverified, Tasks + Events + PedIntelligence/TaskManager/scanners)
Zone rows (ryder1.tsv, exe_addr != -, verdict NONE, zone): 102. Compared: 100 (MATCH 96, FIXED 4); 2 already had verdicts (CPedGroupIntelligence::AddEvent, SetScriptCommandTask). SKIP-RENDER: none (AESoundManager / police scanner rows are logic, compared).
Oracle: gta_sa_compact.exe via exedis.py. In-source comments not trusted; each row compared structure-first (calls, branches, constants, offsets, arg order, compare strictness).

| Function | exe | Fix | Commit |
|---|---|---|---|
| CPedAttractor::GetTaskForPed | 0x5EC500 | erase from ms_tasks only if found (port erased end() when absent) | 382283b9a |
| CTask::IsTaskPtr | 0x61A4B0 | pointer equality over used slots (port IsObjectValid accepts interior pointers) | 9fde5d538 |
| CTaskSimpleCarGetIn::FinishAnimCarGetInCB | 0x6467F0 | anim->m_CurrentTime (+0x1C) = -1000 before clearing m_anim (missing) | 0e071d82c |
| CTaskSimpleCarSetTempAction::MakeAbortable | 0x63D740 | also m_autoPilot.m_nTempActionTime = CTimer ms (veh+0x3BC) | 848ae1296 |

Build: StandaloneRelease ninja -j4 linked. No game run, no run_all_tests / regress (budget; gate to run them).
Per-row verdicts: .notes/asm_sweep/HC2.result.tsv
