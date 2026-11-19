# ASM-P4RVY: review of latest fixes
- 6821ec61f CPopulation::ManageAllPopulation: CONFIRMED (reverse dummy walk over capacity, held-object skip).
- 89525a073 CPad::StartShake_Train: PARTIAL. Unrounded x87 chain and float store of sum correct; `<= 4900` WRONG: fcomp + test ah,5 + jp returns when dist >= 4900 (equal -> PF=1), so continue only on `<`. Fixed.
- P4R18h / P4R25e: no source-changing commits found.
