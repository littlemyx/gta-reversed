# ASM-S15 (SlideToCoord, StandStill, StandUp, StealthKill, Swim; rest of the slice not reached)

Scope: .notes/asm_sweep/S15.tsv, 33 rows. Compared 13 (5 rows fixed in 3 functions' worth of code, 8 match), NOT_REACHED 20 (budget). Result table: S15.result.tsv.

| Function | Verdict | Commit |
|---|---|---|
| StealthKill::ManageAnim (0x6296D0) | FIXED: CPedDamageResponseCalculator damager is m_target (the attacker), port passed the ped | 646805fd |
| StealthKill::ProcessPed (0x62E540) | FIXED: |v|^2 summed z, x, y before sqrt | 646805fd |
| Swim::ProcessPed (0x68B1C0) | FIXED: gasp test reads the byte at task+8 (m_bFinishedBlending), port tested m_pPed (+0x20) | 923559dd |
| Swim::ProcessEffects (0x68AA70) | FIXED: bubble oxygen is a signed int (JGE), port uint32 | 923559dd |
| Swim::ProcessSwimAnims (0x6899F0) | FIXED: climb-jump branch: no water level or z+0.5 >= level keeps the state, only z+0.5 < level resets to TREAD (port returned early / reset on no water); 80 deg literal 0x3FB2B8C3 | 38e4c36d |
| SlideToCoord::ProcessPed, StandStill::ProcessPed, StandUp (2), Swim MakeAbortable / ApplyRollAndPitch / ProcessControlAI / CreateFxSystem | MATCH | - |

Open (not compared): Swim::ProcessSwimmingResistance (0x68A1D0), Swim::ProcessControlInput (0x688A90), ThrowProjectile (2), Tired, Turn180, UseGun (9), WaitUntilAreaCodesMatch, WaitUntilPedIsOutCar, LineUpPedWithCar (3).
Swim jump table of ProcessSwimAnims decoded from the exe (0x68A1B8; state order TREAD..BACK_TO_SURFACE as the enum).
