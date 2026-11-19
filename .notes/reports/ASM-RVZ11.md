# ASM-RVZ11: independent exe review of HC8e/HC11 fixes
Helpers verified: 0x404330 = a<b?a:b, 0x420800 = a>b?a:b (NaN -> second). All 5 CONFIRMED, no source change:
1. UpdateBlend (0x4D1490): CONFIRMED incl. other branches and constants.
2. CAutomobile::ProcessControl turn-mass cap (0x6B2626): CONFIRMED (mass<2500 ? mass : 2500, 2500/-0.008 consts).
3. CAutomobile::ProcessAI + ProcessCarWheelPair min(1,3000/mass): CONFIRMED (arg order 1.0 first, multiply order).
4. CBike::ProcessAI two max sites: CONFIRMED (first arg = q, second = pedal; gas/brake offsets 0x49c/0x4a0).
5. CPhysical::ProcessCollision tail 0x54E5DA-0x54E641: CONFIRMED (new-old, (x2+y2)+z2).
No build/tests needed (no source change).
