# ASM-S05e (CTaskComplexKillPedOnFootArmed::ControlSubTask 0x62CCE0) - 2026-10-28
Result: 1 of 1 compared in full (DEVIATION fixed in 9d89c4db). Table in S05e.result.tsv.
Fixes (exe 0x62CCE0-0x62D25E):
- Duck coin flips: `GetRandomNumberInRange_int32(0,100) < m_duckChancePerc` (unsigned) in both blocks, not the float `RandomBool` (<=). The "in range to duck" test is dist^2 < 400 strictly.
- Gun-control end-leisure conditions use the float distance (sqrt of the extended (x^2+y^2)+z^2 sum), strictly: `dist > range`; `range/2 < dist && !stay && now > m_lastAttackTime + 2500` (port: squared, `>=`, 2000); `dist > 4 && !stay && now > m_shootTimer`.
- Strafe: `now >= m_lastStrafeTime || stayFlag SET` goes to the flip branch (port had `!stay`); forward step only when `dist < 4 && !stay` (port: `<= && stay`).
- Seek-entity case: `range/2 > dist` strictly; dot test evaluated first, `now - m_lastAttackTime > 2000` strictly.
MATCH: m_newTarget/health prologue, duck-time gate, GetTaskDuck/SetDuckTimer branch, m_bIsInControl, ogSubTask return, strafe-flip and direction jump tables, roll timing (3000 ms, store after ControlDuckMove), Say contexts.
Tests: StandaloneRelease + StandaloneRW built; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / 0, PC53 0 / 0; combat gameplay_regress PASS=14. The ControlSubTask/CreateNext bodies are not oracle rows.
