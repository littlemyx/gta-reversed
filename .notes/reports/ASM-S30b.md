# ASM-S30b (Camera.cpp, 21 rows)

Result table: .notes/asm_sweep/S30b.result.tsv. 21 rows (+ the shared StepCineyCamSeq decision): 7 MATCH (IsItTimeForNewCamera, Train, Heli, Boat tail, SetNearClipBasedOnPedCollision, UpdateSoundDistances, StepCineyCamSeq), 1 FIXED, 1 PARTIAL, 13 NOT_REACHED (budget).

- FIXED CameraGenericModeSpecialCases (b859a15a): the exe stores `[taskHold + 8]` (the held entity) in m_pExtraEntity[]; the port stored the target ped itself (IsExtraEntityToIgnore then ignored the wrong entity for camera collision).
- IsItTimeForNewCamera: prologue and all 30 jump-table targets (0x51E4E8; 9-14 -> 0x51DB9D false, 24 and 25 share 0x51E423) compared; constants, x87 compare polarity, dot-product term order (z-first / z-last), boat+Skimmer test and LOS flags all match.
- StepCineyCamSeq (S30 UNSURE) decided: the exe does `mov [idx], rem; jge ..; mov edx, n-1; jmp <store>`; the jump target is a second store of edx, so a negative remainder ends as n-1 in the global too. Port is exact; no change (same code in Car/Plane/Train/Heli/Boat).
- Train / Heli / Boat: failure tails (idx = n, ON_A_STRING 0x12 test, TryToStartNewCamMode(29), time stored only on that branch; Train/Car always store the time and use table[n]) match.
- TryToStartNewCamMode: PARTIAL. Cases 0-3 and 5 line by line; constants and branch skeleton of 7/8 and the later heading cases up to ~0x51F8xx. Open: the tails of the heading-rotation cases and the shared tail 0x51FFAD-0x520101.
- NOT_REACHED: SetRwCamera, Constructor, CopyCameraMatrixToRWCam, DealWithMirrorBeforeConstructRenderList, UpdateTargetEntity, CalculateGroundHeight, ImproveNearClip, CameraColDetAndReact, CamControl, Process, DeleteCutSceneCamDataMemory, LoadPathSplines, ProcessJiggle (long default-init block 0x516586-0x516970).
