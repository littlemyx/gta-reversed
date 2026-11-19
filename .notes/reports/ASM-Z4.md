# ASM-Z4: camera functions installed without an asm verdict

Rebuilt the list from noverdict.tsv against all *result*.tsv: 59 camera/cam rows still without verdict (rest of the Z1 ~154 already landed elsewhere).
Compared: 17 (16 MATCH, 1 UNSURE), 0 fixes, no source changes. 13 rows NOT_REACHED (none of them hit in RYDER_1).
CCamera::StartTransition 0x515200 (Z1's UNSURE) is now MATCH in full. WellBufferMe (hottest, 26575 hits) MATCH.
Open: TakeControlAttachToEntity tail, CamShakeNoPos source side, section (3) script handlers from T3_SCM_USAGE not started.
Note: VectorMoveLinear/AddShakeSimple/LerpFOV use `const double now` (v6 says no bare double); values match the exe, left untouched.
