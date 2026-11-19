# ASM-P4R21c: core math, continued
Review of 22c83b78b (cBuoyancy::AddSplashParticles): magnitude sum order z,y,x, speed clamp x*(1/s)*0.1, delta + t, NaN returns CONFIRMED. Velocity modifier WRONG: exe 0x6C3680 uses the clamped speed vector components, (sx*0, sy*0, sz*-1)*120 (not the shifted y/z * 0 and the scalar speed), plus a trailing entity->m_vecMoveSpeed*0*0 term. Fixed in 11c61ecb.
Of the 24 NOT_REACHED rows: 19 MATCH, 5 not reached (ConvertToEulerAngles, ConvertFromEulerAngles, Rotate, Quaternion Get/Set-Euler). Slerp core is the exe asm verbatim. Details: P4R21c.result.tsv.
