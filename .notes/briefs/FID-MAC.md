# FID-MAC — fidelity items the Mac session found that exist on Win32 too

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh fid-mac based on origin/merge-portable (= reverse/interior_c
+ portable/main, being gated by the orchestrator; it is pushed to reverse/interior_c when green — then rebase onto it).
## Evidence
.notes/PORTABLE_SYNC.md (sections MAC-PLAY1, MAC-C3, RR-1, MAC-P4) and the reports it names (.notes/reports/MAC-PLAY1.md, MAC-C3.md, MAC-P4.md,
.notes/RR_PLAN.md 9.2). Items, in priority order:
- W1 CGenericGameStorage::GenericSave end-of-file padding (exe 0x5D15C2..0x5D16A0, asm quoted in the sync log): saves > ~151.5 KB are not
  padded to 202752 bytes and fail to load ("Load Failed"). The `#ifdef NOTSA_PORTABLE` branch in GenericGameStorage.cpp is the proposed
  replacement: verify against the asm, make it the shared code, test a > 150 KB save -> load round trip on Win32 (run build, private USERDIR).
- F7 CFireManager::Update corona position reads `point` in its own initialiser (FireManager.cpp:480-482).
- F8 CText::GetUpperCase signed char (0x69F750 movsx vs movzx).
- F9 CEventPotentialWalkIntoBuilding default ctor passes its own uninitialised members to the base ctor.
- F5 CBike::m_vecAveGroundNormal not initialised in the ctor (exe ctor 0x6BF430) and read in ProcessControl (Bike.cpp:2239).
- W2 CTaskSimplePlayHandSignalAnim dtor CWorld::Remove on hand objects already deleted by CWorld::ShutDown (what does the exe do?).
- W3 CEntryExitManager::Load NOTSA_UNREACHABLE (EntryExitManager.cpp:449) where the exe skips (`if (enex)`).
- W4 ObjectData::Load stale effectName buffer for fxType>0 lines without a name (object.dat, 5 lines).
- P4(1) TaskComplexStareAtPed passes &gDefaultTaskTime as a duration; P4(2) ObjectCommands.cpp:53 passes a deleted object pointer as a handle.
## Task
For each: read the exe asm (gta_sa_compact.exe via the repo's disassembly tooling / .notes/symbols.txt), decide port bug vs faithful,
fix port bugs as the exe does (one commit each, asm address, oracle row where feasible, consumer audit). Faithful exe bugs: leave as is,
note in the report. Write the per-item verdict back into .notes/PORTABLE_SYNC.md as one [win] entry.
Gate (v5): Release + touched oracle targets + soak. HARD budget 250k tokens / 100 min — at 200k stop and land.
## Do not touch
tools/standalone/replay_diff/, source/standalone/Lockstep.*, statehash/RR files.
