# ASM-P4R6h
Reviewed P4R6g: HateBadlyLit::AffectsPed and GunShotWhizzedBy::AffectsPed both CONFIRMED (dot in z,y,x order; plain float is enough since the exe runs PC24). Tails of AreaCodes::AffectsPed and ProstituteSolicit::CreateFirstSubTask: MATCH.
Compared 24 functions (Events/Tasks): 22 MATCH, 2 FIXED.
- eb90291ab CEventAcquaintancePed::TakesPriorityOver: null ref-event ped => true (exe sete), port false.
- 36e25f96a CEventInWater::TakesPriorityOver: KNOCK_OFF_BIKE and DAMAGE win unconditionally; port required acceleration > 1 for them too.
Zone open (never-audited list, 89 before): ~70 (only the ones above plus P4R6e/f/g rows closed).
