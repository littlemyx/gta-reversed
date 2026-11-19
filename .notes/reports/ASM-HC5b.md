# ASM-HC5b: continuation of HC5 (the 60 NOT_REACHED rows)

Zone: 60 rows. Compared 34, all MATCH, 0 fixes (no source change, no build needed). SKIP-REPLAY 9 (record buffer only), SKIP-TEXT 2 (CMessages Insert*), SKIP-RENDER 1. NOT_REACHED 14 (budget).

- IKChain/IKChainManager/BoneNodeManager/BoneNode (22 compared): MoveBonesToTarget walked in full (gates, dot order, cross, quat normalise, angle chain, gate bytes 0x8D2BD0..2 = 1). SetupBones: exe is a do-while without the BONE_UNKNOWN guard (port guard unreachable for valid tags). BlendKeyframe early-outs live in the port's RtQuatSlerp.
- CWaterLevel (4): FindNearestWaterAndItsFlow, CalculateWavesOnlyForCoordinate2 (6 constants checked), AddPolyToBlock, FillQuadsAndTrianglesList.
- CTxdStore FindCB / SetupTxdParent; CControllerConfigManager::AffectPadFromKeyBoard / AffectPadFromMouse (inlined ButtonDown dispatch; GTATranslateShiftKey skipped under SDL3 = INTENTIONAL); CReplay::MarkEverythingAsNew flag bits (ped dword +0x470 bit 13, vehicle flags bit 26).
- Left: CTimeCycle::CalcColoursForPoint (1352 asm lines), BoneNode QuatToEuler/Limit/CalcWldMat (port comments cite exe addresses, oracle-tested earlier, not re-compared), CControllerConfigManager GetIsKeyboardKeyJustDown (jump table) + 8 init/config rows.
