# ASM-HC7b
Step 1: fa566305a (CAutomobile::PlayCarHorn) CONFIRMED against exe 0x6A3770 (m_nCarHornTimer is `char`, exe `and eax,7` on the byte).
Compared at instruction level: CPhysical::RemoveAndAdd (RECONFIRMED), CPed::UpdatePosition (FIXED f587dac51: bike threshold constant 0x863244 = 0x3D23D70B, one ulp above 0.04f).
Screen: exe float constants (>=0x850000) of the 162 top-by-count MATCH functions in the zones vs the float literals in the port bodies; no further 1-3 ulp near-misses.
Not reached: CPhysical::ApplyCollision (both; constants 0.7/0.3 only checked), SphereCastVsCaches, CObject::*, S48 scanners.
Wrong earlier verdict: S16/HC? MATCH on CPed::UpdatePosition (earlier slice) missed the 0.04f constant; S16 PlayCarHorn already fixed in HC7.
