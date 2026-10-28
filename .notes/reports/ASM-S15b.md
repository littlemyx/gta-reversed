# ASM-S15b (UseGun, ThrowProjectile, Tired, Turn180, WaitUntil*; Swim x2, LineUpPedWithCar x3 and UseGun::ProcessPed not reached)

Scope: .notes/asm_sweep/S15b.tsv, 20 rows. Compared 14 (6 rows fixed, 8 match), NOT_REACHED 6 (UseGun::ProcessPed, Swim x2, LineUp x3). Result table: S15b.result.tsv.

| Function | Verdict | Commit |
|---|---|---|
| UseGun::AimGun (0x61ED10) | FIXED: one-shot m_IsAimImmediate cleared; look-at position/bone read from the AIMING ped player data (BONE_UNKNOWN for non-ped target); m_TargetPos look-at leaves the look-in-use flag unset; no-targetpos branch looks at the ped itself (bone R_UPPER_ARM, only if not already looking) and the left twin-pistol arm uses R_UPPER_ARM too; non-player points in m_fLookDirection (+0x73C); blocked-left-arm abort targets LEFT | f54577da |
| UseGun::RemoveStanceAnims (0x61E8E0) | FIXED: end test total > current (not current/total < 1); walking player returns after the walk blend | f54577da |
| UseGun::StartAnim (0x624F30) | FIXED: pistol whip checks IsTaskInUseByOtherTasks (not StopFireGun); burst-shots signed compare | 79753613 |
| UseGun::SetMoveAnim (0x61E3F0) | FIXED: aim-with-arm still branch SetMoveState(STILL) + idle from the ped anim group; reset loop covers fwd/L/bwd/R (bwd twice before); strict x < -0.75; NaN y -> forward | 79753613 |
| ThrowProjectile::StartAnim (0x6259E0) | FIXED: anim choice inverted (THROW vs THROWU), z,y,x distance sum | 34466bf1 |
| ThrowProjectile::ProcessPed (0x62AF50) | FIXED (minor): x87 compare, signed clamp | 34466bf1 |
| UseGun FireGun, Clone, MakeAbortable, SetPedPosition; Tired::StartAnim; Turn180::ProcessPed; WaitUntilAreaCodesMatch; WaitUntilPedIsOutCar | MATCH | - |

Open (not compared): UseGun::ProcessPed (0x62A380, large), Swim::ProcessSwimmingResistance (0x68A1D0), Swim::ProcessControlInput (0x688A90), LineUpPedWithCar (3 rows).
Note: GetAimLookAtInfo now takes the aiming ped (header changed). The 0x8D2E88 gun flash lifetime is a .data global initialised to 250 (port keeps the literal).
