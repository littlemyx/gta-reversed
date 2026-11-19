# ASM-RVZ
Reviewed a71789b11 (GetIsTouching) and dac6bef9f (GetBikeRidingSkill) against exe: both CONFIRMED. No other source-changing ASM-HC commits on origin (HC1/3/4 others were result/report only). All 5 callers of GetBikeRidingSkill (Bike.cpp) assume 0..1. No source changes, no build needed.
