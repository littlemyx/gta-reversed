# SAVE-LOAD (W6) — loading a Win32 save crashes in the script reader

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh save-load based on origin/fid-mac (8a385f01: W1 save
padding fix + `cmd:savemenu` injector command; base = merge-portable, not yet on reverse/interior_c).
## Evidence
FID-MAC (.notes/reports/FID-MAC.md if present, PORTABLE_SYNC [win] entry): with W1 a new-game save is 202752 bytes (money 777, slot
GTASAsf4.b). Loading it crashes ~0.1 s after the confirm: `UNREACHABLE notsa::script::Read (ReadArg.hpp:215)` then
`FATAL: script command 10240 (Unknown) has no ported handler`. Save + logs: C:/gta-scratch/fidmac-save/. Before W1 saves failed earlier
(alignment check), so the load path after the work buffer was probably never exercised on the standalone. Suspects, in order:
CTheScripts::Load (TheScripts.cpp ~1124-1262: raw CRunningScript reads, IP/stack stored as offsets vs pointers, ScriptSpace / mission
space restore), the order of the save blocks (CGenericGameStorage::GenericLoad block list vs the exe 0x5D1EA0), any block reader that
consumes a different number of bytes than the exe (shifts everything after it — check each block's byte count vs the exe writer/reader).
## Task
Root-cause with the exe as the reference (asm addresses), fix in shared code (the Mac branch has `notsa::FileRecord<CRunningScript>` under
NOTSA_PORTABLE — the Win32 path must stay byte-compatible with exe saves). Acceptance: save -> load round trip on the run build works
(new game save, load, CJ in control, money 777 restored, 3 min play after load without errors); ideally also a mid-game save; soak OK.
Note whether a save written by the ORIGINAL exe loads in the port (if one exists in a private test dir — never read or write the user's
Documents saves). HARD budget 250k tokens / 100 min — at 200k stop and land/branch.
