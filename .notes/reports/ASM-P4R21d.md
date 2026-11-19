# ASM-P4R21d: core math last rows

Result: 9 functions compared against the 1.0 exe, 9 MATCH, 0 fixes, 0 source changes (no build needed).

- cBuoyancy::AddSplashParticles (011c61ecb): CONFIRMED correct against 0x6C34E0-0x6C3AF9 (velocity modifier (sx*0, sy*0, sz*-1)*120 from the clamped speed vector, plus the entity move speed *0*0 term).
- CMatrix::ConvertToEulerAngles / ConvertFromEulerAngles: the port embeds the exe x87 body; compared mechanically (instruction text, jump targets, constant tables): identical.
- CMatrix::Rotate: 184 instructions mapped from the exe stack frame to the port locals: identical.
- CQuaternion Get-Euler, Set-Euler, Set(matrix) (all 4 branches): equal, including +2pi condition and NaN behaviour.
- CMatrix::Reorthogonalise third cross product and CPlaceable::AllocateStaticMatrix null branch: equal.

Scripts: C:/gta-scratch/p4r21d/*.py. Rows: .notes/asm_sweep/P4R21d.result.tsv.
