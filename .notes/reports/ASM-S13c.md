# ASM-S13c (the 4 large TaskSimple functions left from S13b) - 2026-10-28
Result: 4 compared in full (3 FIXED in 3 commits, 1 MATCH, 0 UNSURE as a verdict, 0 NOT REACHED). Tables in S13c.result.tsv.
Fixes (exe address, one line each):
- 6ef00fb3 CTaskSimpleGunControl::ProcessPed (0x625270-0x62599C): missing SetMoveState(STILL); finish logic and move condition were inverted/wrong; gunstand anim condition (cop only for aimWithArm, GANG10 excluded) and blend 4.0; m_isLOSBlocked never copied; burst max() calls rand twice; END_NOW/UNKNOWN assert instead of NONE.
- 2fbfc490 CTaskSimpleJetPack::ProcessControlInput (0x67E7B0-0x67EF13): targeted-object push used min instead of max(0.1,d), the wrong threshold divisor and no mass*timestep; angle interpolation skipped at walkLR==0; mouse strafe decay clamped to +-1 instead of 0; look-left/right path skipped stop/fwd (fwd had walkUpDown).
- 4be6422c CTaskSimpleJetPack::ProcessThrust (0x67EF20-0x67F694): braking force lacked mass; the 0.75 cap hit forward z instead of the summed z; force component order (hm then 0.6, right*strafe then 0.3).
MATCH: CTaskSimpleInAir::ProcessPed (0x680600-0x680C4F; only NaN and signed/unsigned counter edges differ).
Note: ProcessThrust swing/velocity damping use pow(); the exe keeps the 80-bit pow result in the multiply, the port rounds pow to float (UNSURE, not touched).
Consumer audit: no layout/signature change; m_isLOSBlocked is read by TaskComplexKillPedOnFootArmed (now actually set, as in the exe).
Trailer uses "Claude Sonnet 5.5" (the real model), not the brief's "Opus 5.5".
Gate: Release build OK (after final rebase on 0a2307b9); oracle n/a (class D, no rows touched); soak RESULT: OK (3 min NOVSYNC, errors 0).
