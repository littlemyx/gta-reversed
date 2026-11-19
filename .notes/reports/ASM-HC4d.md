# ASM-HC4d (continuation of HC4, HC4b, HC4c)
Zone rows still without verdict: ~55 (hitcount ryder1.tsv, the rest were landed by other agents). Done: 30 compared (28 MATCH, 2 FIXED). Skipped-render: CStreaming::RenderEntity, CAE* audio rows (CAECollisionAudioEntity, CAEScriptAudioEntity).
Fixes: CStreamedScripts::GetProperIndexFromIndexUsedByScript scanned only the registered count, the exe scans all 82 slots (7188202e9); CScriptsForBrains::AddNewScriptBrain kept 5.0 for a NaN radius, the exe keeps the NaN (4ac788774).
Closed HC4c NOT_REACHED: TestLineTriangle 2D cases 1-5 (all instruction-level, MATCH), CDecisionMakerTypes ctor arrays, CWeaponModelInfo/CDamageAtomicModelInfo::Init, GetPedsInteriorGroup, SetEntryExitPtr, IncrementVoice, Get2dEffectStore, SetupVehicleUpgradeFlags, CAnimBlendStaticAssociation::Init.
Not reached: CCollision::SphereCastVersusVsPoly (1.7M calls), CWorld::GetSector (root), CStreaming small helpers, CTheScripts::IsPedStopped / AddToListOf* / StartTestScript, CPPTriPlantBuffer::SetPlantModelsTab, CPlantMgr rest.
Build: StandaloneRelease link OK (ninja -j4). Tests / regress left to the gate (two one-line behaviour changes in rarely hit paths).
