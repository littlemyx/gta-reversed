# SAVE-LOAD (W6) - Win32 save -> load round trip

Start 12:39, end ~13:45 (2026-10-24), base fid-mac 8a385f01, branch save-load.

## Root cause of the FID-MAC crash (`UNREACHABLE notsa::script::Read`, `command 10240`)
The saved state was invalid: `cmd:savemenu` ran with the INTRO mission script active (OnAMission = 1, `INTRO` in the save's script table, its m_BaseIP != 0).
- Save 0x5D4C40 writes the globals chunk of ScriptSpace only (`m_NextChunkOffset` = 43808 B here) + per script: u16 index, 0xE0 record, u32 IP offset, 8 x u32 stack offsets.
- Load 0x5D4FD0 restores exactly that (verified byte for byte: reads 0x5D4FE3..0x5D536A, record 0xE0 at 0x5D5335, IP at +0x14 = 0xA49960 + offset); the mission script's code (MissionBlock = ScriptSpace + 200000, 69000 B) is not in the save.
- After the load INTRO ran 3000+ zero NOPs at ip 215796.. (traced), i.e. through the zeroed MissionBlock and on into the data behind it -> garbage opcode (`LocateCharInCar3D` with a bogus param type, then command 0x2800 = 10240).
- Same in the exe by design: scripts call ACTIVATE_SAVE_MENU only outside missions. Not a loader bug; the exe was not run (the original exe cannot be redirected away from Documents, rules).
Evidence: before = save made during INTRO (/c/gta-scratch/sl/saveA.b) crashes 13 s after start; after `cmd:endmission` the same load of a mission-free save works on the UNMODIFIED loader (run B4: money 777, same position).

## Port deviation fixed (consumer audit)
Win32 `LoadDataFromWorkBuffer<T>` / `SaveDataToWorkBuffer<T>` used the raw sizeof(T) copy, so CRunningScript carried process (ASLR) addresses in m_BaseIP / m_IP / m_IPStack instead of the exe's fixed 0xA49960-based VAs; after a reload m_BaseIP (used by `CRunningScript::UpdatePC` for negative jump offsets, MissionCommands.cpp:36) was garbage (INTRO: base = ScriptSpace + 3140928). Fix: the FileRecord path (already used by NOTSA_PORTABLE, with x86 layout static_asserts) now serves every platform. Records: CRunningScript, CMatrix, CPedSaveStructure, CVehicleSaveStructure, CPickup, tRadarTrace, CWeapon, CPathNode, BreakInfo_t - all readers/writers of those go through the same two templates (grep: no other raw reader). File layout unchanged: BLOCK offsets identical to the raw writer's save, 202752 B; a save of the old raw writer loads with the new reader (run B7).

## Evidence (run build, windowed, silent, private NOTSA_STANDALONE_USERDIR)
- tools/standalone/saveload_roundtrip.sh: new game -> endmission -> car + cop spawned -> money 777 -> real save menu -> 202752 B slot 1; second start: Start game -> Load game -> slot 1 -> Yes -> CJ in control at the saved position, money 777, mission 0, 3.6 min of walking/sprinting/jumping, quit through cmd:quit, 0 UNREACHABLE/FATAL/exception lines. RESULT OK.
- soak.sh 3 min NOVSYNC on the final exe: RESULT OK, errors 0.
- Not done: a save written by the ORIGINAL exe (no private way to produce one: gta_sa.exe always writes to Documents); full run_all_tests (orchestrator's gate); the cheat-spawned parked car (and cop) is not in the loaded world (not analysed; CPools::Save keeps only part of the vehicle pool).
