# ASM-HC5: leftover non-render zone (SurfaceInfos_c, DamageManager, Messages, TimeCycle, TxdStore)

Zone: 125 NONE rows in ryder1.tsv; 116 still without a verdict in the sweep tsvs. Compared 56, all MATCH, 0 fixes. 60 NOT_REACHED (budget).
SKIP-RENDER: CWaterLevel::RenderDetailedSeaBedSegment.

- SurfaceInfos_c (20 rows, 1.5M calls): every getter's shift/mask checked against the SurfaceInfo_c bitfield; GetSurfaceIdFromName: 178 name->id pairs and the compare order extracted from the exe and diffed against the port table by script (C:/gta-scratch/hc5_gsid.py): identical.
- CDamageManager (Get/Set status, door/panel/light/aeroplane, node-from-panel): jump tables decoded, all offsets/ids match.
- CMessages (12 of 14): store layout, shift loop of ClearThisPrint, big-message stores identical; exe-only difference is the clears touch Text/String only (unobservable).
- CTimeCycle: FindTimeCycleBox (full x87 compare chain), StartExtraColour/StopExtraColour/AddOne/FindFarClipForCoors, 9 ambient getters.
- CTxdStore getters: valid-slot path identical; free slot: exe crashes on null deref, port returns null (not reachable).
- Not reached: CalcColoursForPoint, CMessages::InsertNumberInString / InsertPlayerControlKeysInString (text only), Replay (10), ControllerConfigManager (11), IKChain/BoneNode (~28), WaterLevel (4), TxdStoreFindCB, SetupTxdParent. Listed as NOT_REACHED in HC5.result.tsv.
