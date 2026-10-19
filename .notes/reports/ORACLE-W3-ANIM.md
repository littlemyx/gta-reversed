# ORACLE-W3-ANIM report

Agent ORACLE-W3-ANIM (rules v2), worktree C:/gta-wt/oracle-w3-anim, branch oracle-w3-anim. New target `wave3_anim_oracle_test` (= wave1_oracle_test.cpp + W2 harness, `W3_ANIM_ONLY`), rows in `tests/standalone/wave3_oracle_anim.inc`: **62 rows, PC24 0 mismatches** (PC53 12, CAnimBlendAssociation::UpdateTimeStep float vs 53-bit x87, informational).

| | before | after |
|---|---:|---:|
| game_sa/Animation covered (uncovered A / B) | 19 (36 / 3) | **58 (0 / 0)** |
| game_sa/Plugins/RpAnimBlendPlugin covered (A / B) | 1 (5 / 18) | **24 (0 / 0)** |
| exe-oracle coverage, whole game | 3386 | **3448** |
| port bugs fixed (one commit each, asm address in the subject) | | **6** |

## Fixtures (real, not garbage)
A fake clump whose plugin slot (exe `ClumpOffset` 0xB5F878) points at a CAnimBlendClumpData with a frame array and a doubly linked list of 0..8 blend associations (flags incl. partial / playing, blend amount, delta, hierarchy, anim id, time); hierarchies with 0..5 sequences of 0..6 un-/compressed key frames (random unit quaternions with sign flips, ascending time stamps, raw int16 compressed frames, translation or not, external frame memory); association groups with static associations (the exe's `ms_aAnimAssocGroups` / `ms_aAnimations` / `ms_AnimCache` globals point into the fixture); a fake RwFrame with the node-name plugin. The whole fixture (32 KB) and the exe data image are compared after every call. Hit rate: list queries returned a non-null association in 40-70 % of the cases.

## Port bugs (each row failed on the pre-fix code; logs C:/gta-scratch/w3anim/run1.log, prefix.log, postfix.log)
| function | asm | bug (pre-fix failing cases) |
|---|---|---|
| RpAnimBlendClumpGetNumAssociations 0x4D6B60 / NumNonPartial 0x4D6BB0 / NumPartial 0x4D6B80 / PauseAll 0x4D6B00 / UnPauseAll 0x4D6B30 | 0x4D6B60 `mov eax,[eax] / test eax,eax` loops start at m_AnimList.next | `CAnimBlendLink::begin()` returned the list head itself, so the CAnimBlendClumpData (4 bytes below its link, seen as an association) was counted: n+1 (300/300) and UnPauseAllAnimations wrote ANIMATION_IS_PLAYING into the bytes after the clump data (heap corruption) |
| RpAnimBlendClumpGetMainAssociation_N 0x4D6A30 / GetMainPartialAssociation_N 0x4D69F0 | 0x4D6A50 `shr dl,4 / test dl,1 / je`, 0x4D6A10 `jne` | predicates were swapped relative to the exe (238/400 each); no exe code calls either (parity only) |
| RpAnimBlendClumpExtractAssociations 0x4D6BE0 | 0x4D6BF5 `mov [ecx+4],0` | extracted list head kept prev -> clump list head (250/300) |
| ClumpAnimConstruct 0x4D5F40 | 0x4D5F48 `mov [eax+ecx],0` | cleared the slot at the ClumpOffset global instead of the offset RW passes (100/100) |
| CAnimBlendAssocGroup::IsCreated 0x4D37A0 | `mov edx,[ecx+4] / test / setne` | tested m_NumAnims != 0 instead of m_Anims != null (14/100) |
| RpAnimBlendFrameSetName 0x4D5F00 -> SetFrameNodeName 0x72FB00 | strncpy(dst, name, 23) + dst[23]=0 | strncpy_s did not zero-pad the old tail (134/200) |

## Verified equal (examples that carry real blend / keyframe maths)
CAnimBlendHierarchy::CalcTotalTime 0x4CF2F0 and CalcTotalTimeCompressed 0x4CF3E0 (in-place delta times, int16 fixed-point), RemoveQuaternionFlips (sequence 0x4D1190 and hierarchy 0x4CF4E0, 600/400 cases with sign flips), UpdateTimeStep 0x4D13A0, SetBlend/SetBlendTo, GetMainAssociation 0x4D6910 (second-best blend out parameters), GetKeyFrame 0x4CF1F0/0x4CF220, ReferenceAnimBlock 0x4CEA50, the AnimManager block-ref and hash-lookup statics, ms_AnimCache RemoveFromUncompressedCache 0x4D42A0.

## Observed, not changed
- CAnimBlendSequence::SetBoneTag 0x4D0C70: the port compares in the 16-bit eBoneTag (65535 == BONE_UNKNOWN), the exe only skips -1; no real bone tag is that large (rows use tags < 1024).
- Debug-CRT artefacts masked in the rows: strcpy_s fills the rest of the buffer with 0xFE (AddAnimToAssocDefinition tail), and the port's `std::span`/`views::take` subscripts assert on out-of-range ids in this debug test build (the rows keep ids in range and set ms_numAnimBlocks).
- Constructors: the port initialises more than the exe (m_hashKey, upper half of the sequence union): lenient compare (only bits the exe changed must agree).
- CAnimManager::AddAnimBlockRef/RemoveAnimBlockRef index -1: the run build's detached globals guard it (D-FIX1); not exercised.

## Not covered (class S/C/D, outside A/B) - next candidates
S: CAnimBlendAssociation SetCurrentTime 0x4CEA80 / SyncAnimation 0x4CEB40 / Start 0x4CEB70 / Init x3 / Constructors 1-3 / AllocateAnimBlendNodeArray, CAnimBlendStaticAssociation Init/Constructor2/AllocateSequenceArray, InitEmptyAssociations: runnable on the same fixture (allocator redirect exists). C: UpdateTime 0x4D13D0 / UpdateBlend 0x4D1490 (delete via vptr when auto-remove), RpAnimBlendClumpRemove*/GiveAssociations.
