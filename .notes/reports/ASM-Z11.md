# ASM-Z11: script command handlers without verdict (continuation of Z8)

Compared 14 handlers (see .notes/asm_sweep/Z11.result.tsv): TASK_GO_STRAIGHT_TO_COORD, SET_CAR_HEADING, SET_CAR_COORDINATES,
SET_FIXED_CAMERA_POSITION, POINT_CAMERA_AT_POINT, DO_FADE, SET_CHAR_PROOFS, SET_CHAR_ACCURACY, IS_CHAR_DEAD, IS_CHAR_IN_CAR,
CREATE_OBJECT, SET_OBJECT_HEADING, SET_CHAR_AREA_VISIBLE, RESTORE_CAMERA_JUMPCUT/SET_CAMERA_BEHIND_PLAYER. All MATCH; no fixes, no commits to source.

Tooling notes:
- A mechanical audit (exe `push N; call CollectParameters` vs C++ handler arity, C:/gta-scratch/z11/audit.py -> audit.tsv) is too noisy to use:
  C:/gta-scratch/z6/ops.json addresses for several opcodes (0xFE, 0x5F6, 0x792, ...) point at shared/offset code, and dm.py hides `push 1`.
  Locate-char family is already FIXED by P4R1 (shared helper 0x486D80/0x4870F0), which the Z6 coverage heuristic does not see.
- Open risk (not changed): `Read<float>` of a static INT operand converts the value (exe keeps the raw dword = denormal float); `Read<int>` of a static
  FLOAT operand returns the raw bits (fixed earlier). Making the first exe-exact would break any handler whose port type differs from the exe's
  (fild vs fld), so it needs a per-handler audit first.
- Handlers still unverified: GIVE_WEAPON_TO_CHAR (0x47D307), GET_SCRIPT_TASK_STATUS (0x49220A), CREATE_CHAR_INSIDE_CAR, WARP_CHAR_FROM_CAR_TO_COORD (0x482EA3), REMOVE_CHAR_ELEGANTLY, CAMERA_SET_VECTOR_MOVE/TRACK, SET_ZONE_GANG_STRENGTH (0x46B4BE), TASK_PLAY_ANIM.
