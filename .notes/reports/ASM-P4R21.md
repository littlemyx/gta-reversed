# ASM-P4R21: core math (CMatrix / CVector / CQuaternion / CGeneral / cBuoyancy)

Compared line by line against gta_sa_compact.exe: 41 functions MATCH (rows with an asm range in P4R21.result.tsv), 0 deviations, 0 commits, no source changed (no build needed).
Ports of Convert*EulerAngles / Rotate / Slerp-Core are verbatim x87 blocks (listed MATCH, not re-compared). 28 rows NOT_REACHED (budget): operator*, Reorthogonalise, most CQuaternion, cBuoyancy PreCalc/SimpleSum/SimpleCalc/Boat/Splash, CPlaceable static matrix.
