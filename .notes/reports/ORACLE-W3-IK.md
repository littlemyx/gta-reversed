# ORACLE-W3-IK report

Agent ORACLE-W3-IK (rules v3), worktree C:/gta-wt/oracle-w3-ik (branch oracle-w3-ik), 2026-10-20 10:58 - ~12:15 WEST. New target `wave3_ik_oracle_test` (= wave1_oracle_test.cpp + W2 harness, `W3_IK_ONLY`, + oracle_fast helpers),
rows in `tests/standalone/wave3_oracle_ik{,_a,_b,_c,_d}.inc`: **57 rows, PC24 0 mismatches**, run time **~30 s** (PC53 3116 informational: extended precision vs the port's floats; 126 + 102 agreed-fault cases skipped by design).

| | before | after |
|---|---:|---:|
| game_sa/Ragdoll covered (uncovered A / B / C) | 7 (29 / 9 / 9) | **57 (0 / 0 / 5)** |
| exe-oracle coverage, whole game | 4336 | **4388** |
| port bugs fixed (one commit each, asm address and pre-fix failures in the subject) | | **8** |

## Fixtures (real, not garbage)
* Bone-node pool: 8 BoneNode_c with distinct tags from the exe's BoneInfo table (0x8D26D0), interp frames, unit / non-unit orientation, rigid world matrices, limits from the table or ordered random, child lists (random trees, chains), speeds.
* IK chains: bone array over the nodes (effector -> pivot parent links, world matrices computed by CalcWldMat), random target / effector offsets; the exe's `g_boneNodeMan` (0xC10820) and `g_ikChainMan` (0xC15448) with consistent free / active lists.
* Ped world: fake clump (RpAnimBlend plugin slot at the exe's ClumpOffset 0xB5F878), 32 frame datas + interp frames, an RpHAnimHierarchy in RenderWare's layout (32 node ids = the BoneInfo tags, matrix array), a ped of the rf fake pool (clump pointer, bHasBeenRendered), real CTaskSimpleIKManager / IKLookAt / IKPointArm objects in the ped's secondary task slot.
  RwHelper.cpp is not part of the target: GetAnimHierarchyFromSkinClump / FromClump are test-local (port) and Patch()ed (exe 0x734A40 / 0x734B10) stubs that return the fixture hierarchy.
* The exe's RenderWare maths needs the engine's plugin data: `InitExeRw()` runs the exe's own sqrt-table constructor (0x7EDE90) and sets the matrix plugin data (as rw_math_stream_test does).
* Hit rate (share of cases returning true / non-null): Init 86 %, AddIKChain 86 %, RemoveIKChain 89 %, GetBoneNode 88 %, GetBoneNodeFromTag 63 %, GetIdFromBoneTag 98 %, IsArmPointing 52 %, GetLookAtEntity 34 %, IsAtTarget 14 % (needs blend > 0.98 and a near target).

## Port bugs fixed (each row failed on the pre-fix code; logs C:/gta-scratch/w3ik/run*.log)
| function | asm | bug (pre-fix failing cases) | commit |
|---|---|---|---|
| BoneNode_c::GetIdFromBoneTag 0x617050 | 0x617063 `movsx esi, word [ecx]; cmp edx, esi` | tag compared through the 16-bit enum: bits above 15 ignored (37/400) | 964fe432 |
| IKChain_c::GetBoneNodeFromTag 0x617C60 | 0x617C77 `cmp [ebx+8], edi` | same truncation (66/400) | d0641030 |
| BoneNode_c::Limit 0x617650 | 0x617707: factor = 1 - abs(x) * (1/45f, 0x86D2B8) with an `fcomp 0` floor; clamps are fcomp + conditional store | `abs(x) / -45 + 1` rounds differently; std::clamp / std::max NaN and min > max rules (22/1500) | a9ab7e4a |
| IKChain_c::IsAtTarget 0x617F30 | 0x617F78 `call 0x7EDAC0` (RwV3dLength, table sqrt) | exact sqrt (CVector::Magnitude) instead of RW's approximate length (766/1000) | e6595642 |
| BoneNode_c::CalcWldMat 0x616CD0 | rotation built inline: s = 2/(((x*x+y*y)+z*z)+w*w), 9 float products in the exe's order, RwMatrixMultiply 0x7F18B0 | port went through CMatrix::SetRotate(quat) (unit form, other rounding, no normalisation) (786/800) | ebd00d83 |
| rwmath_exact.h Tables() (RW sqrt / inverse sqrt tables) | engine init 0x7EDE90 builds the tables before D3D sets PC=24 | the shim builds them on first use (`_rwSqrtInit` is never called) = at PC=24 in the game: 1066 of 4096 inverse sqrt entries one ulp off the exe's table, so ~25 % of all RwV3dNormalize / RtQuatRotate / RtQuatConvertFromMatrix results differed in the last bit (RwV3dNormalize 201/800, RtQuatRotate 142/1000). Tables are now built at 53 bits. **Affects the whole game** | f01d66b4 |
| IKChain_c::MoveBonesToTarget 0x6178B0 | dir = (t+root)-bone; RwV3dLength + `fcomp` 1e-5 (NaN skips); dot = (Oy*Ry+Ox*Rx)+Oz*Rz, only dot < 0.997 continues; cross products stored as floats; acos left unrounded on the x87 stack (FPATAN ignores PC) then * speed * m_Speed * (180/pi f) | port: t+(root-bone), exact Magnitude, DotProduct's z,y,x order, `>=` test (NaN passed), acos cast to float first (1373/1500; 590 left after the first four fixes, found with a composed-from-exe-calls probe, `W3K_PROBE=1`) | 241f3f61 |
| IKChain_c::Init 0x618370 | 0x618379 plain store of the ped, 0x618404 `RegisterReference` only after the checks, 0x61844F `m_Blend = 0` | EntityRef assigned first (a failing Init left a dangling reference: 81/600) and m_Blend not reset on a re-used chain (~365/600) (446/600) | f36778c0 |

## Verified equal
BoneNode_c ctor / dtor / Init / InitLimits / ClampLimitsCurrent / ClampLimitsDefault (flags 0x8D2BD0/1 toggled: the clamps are dead code in the game, both flags are 1 in the exe image) / Get-SetSpeed / Get-SetLimits / AddChild;
BoneNodeManager_c ctor / dtor / Init / Exit / Reset / GetBoneNode / ReturnBoneNode / GetNumBoneNodesLeft; IKChain_c setters, UpdateOffset / UpdateTarget / UpdateEntity (null old target), Get/SetLimits, Get/SetBoneSpeed, ClampLimits, SetupBones, Exit (after SetupBones), Update;
IKChainManager_c Init / AddIKChain / RemoveIKChain / Exit / Reset / GetLookAtEntity / GetLookAtOffset / IsArmPointing / AbortPointArm. RW helpers the solver calls (RwV3dTransformVector, RwV3dLength, RwV3dNormalize, RtQuatConvertFromMatrix / TransformVectors / Rotate, CRT acos 0x822380) are exact at PC24 on finite game-range inputs.

## Observed, not changed
* IKChain_c::UpdateEntity: the port also cleans up the OLD target reference (comment in the source, deliberate); only a null old target is compared. Init: a re-used chain's stale `m_Ped` reference is dropped by the port, the exe never did.
* IKChain_c::Update reads the pivot matrix through CEntity::GetBoneMatrix (GetAnimHierarchyFromClump, frame based); the exe uses GetAnimHierarchyFromSkinClump (atomic based) - the fixture stubs both, equal for skinned peds.
* MoveBonesToTarget: only the null-target path (offsetPosWS = offsetPos) is run; the entity-target path (CPed::GetBonePosition / RwFrame modelling matrix) needs RwFrame and ped bone fixtures in both RW layouts. The exe uses a stale stack buffer as the quaternion when |q|^2 <= 0 (not reproducible).
* Unknown bone tags: the exe reads BoneInfo[-1] / loops on BONE_UNKNOWN; the port has BUGFIXes (null checks, loop condition). Rows keep valid tags (GetBoneNodeFromTag / ClampLimits with unknown tags: both return null / no-op).
* The RW helper rows use finite values; NaN / inf / denormal inputs to RwV3dNormalize & co were seen to differ before the table fix and were not re-measured after it.
* IKChainManager_c::AbortPointArm / GetLookAt* : the exe has no type check of the secondary task (the port uses dyn_cast): same results for the real IK manager task, a different task type in that slot would differ.

## Not covered (class C: indirect calls) - next candidates
IKChainManager_c Update 0x6186D0 (CWorld scan code + ped->UpdateRpHAnim vtable), CanAccept 0x618800 / CanAcceptLookAt 0x6188B0 (camera, ped virtuals), IsFacingTarget 0x618330 (task->GetIKChain virtual; the chain's IsFacingTarget is covered by gameplay_oracle_peds), PointArm 0x618B60 (task creation). Same fixture (rf ped + ped IK manager task) with fake vtables would do.

## Tests run (worktree)
wave3_ik_oracle_test (57 rows, ~30 s), rw_math_stream_test (127 checks), rw_rtanim_rtquat_test (4816), rw_atomic_clump_test (104), rw_skin_hanim_test (141), rw_frame_camera_test (PASSED): all green. The orchestrator runs the full run_all_tests.
