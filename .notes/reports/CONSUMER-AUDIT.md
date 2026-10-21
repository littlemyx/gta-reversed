# CONSUMER-AUDIT - fix commits since 67087158 vs. the readers/callers of what they changed

Rules v4 (rule 2). Worktree consumer-audit from origin a4878e02. Method: `git log 67087158..origin/reverse/interior_c -- source/` = 311 commits; classified by subject + diff
(scripted: header-touching commits, commit messages with data addresses / "inverted|swap|opposite|negated", assignments to globals in the diffs of the "local" group), then
every (a)/(b) commit read and its readers/writers/callers grepped (exedis.py at the exe where a reader was in doubt).

Classes: a = layout / indexing / size / meaning of shared state (global, array, pool slot, struct field), b = function contract (return value, out parameter, arguments,
side effect on globals), c = local arithmetic / control flow inside one function, t = tests / harness / tooling / hook-address correction.

## Counts
311 commits: a 54, b 35 (89 non-local, one table row each), c 172, t 50. Non-local: 89, all checked; unexplained: 0.

## Findings
- **FIXED 671d3437** - bf204ed1 swapped the stores of CGarages::TriggerMessage (first number to 0x96C010, second to 0x96C00C, correct per 0x447C2A) but CGarages::PrintMessages
  still passed (0x96C00C, 0x96C010) to InsertNumberInString; the exe (0x447884..0x44789A) passes n1 = [0x96C010], n2 = [0x96C00C]. Effect: every two-number garage/pickup message
  showed the numbers swapped (HO_ONE "collected of total" printed total of collected, same OY_ONE). Fixed in PrintMessages.
- 729926c4 is the REGRESS-POLICE fix (ToBeStreamed readers); 1ef99baa's other potential readers: none (grep).
- No other reader/caller still on an old convention found.

## Table (non-local rows)
| commit | class | state / function | callers / readers checked | verdict |
|---|---|---|---|---|
| 729926c4 CPathFind::UpdateStreaming 0x450A60 / LoadSceneForPathNodes  | a | ToBeStreamed [x*8+y] vs area y*8+x (UpdateStreaming/LoadSceneForPathNodes) | UpdateStreaming flag loop + rect marks, LoadSceneForPathNodes, CarCtrl FindNodesThisCarIsNearestTo (regions only) | FIXED (the REGRESS-POLICE fix itself) |
| 505a4aef CTheScripts::Process 0x46A000 - the script loop stops at the | b | CTheScripts::Process script loop / ScriptsUpdated | other pActiveScripts walkers: TerminateAllScriptsWithThisName (saves next, exe-identical), counters | OK (NEWGAME-CRASH fix) |
| b7036680 PostEffects: raster size = exe's 2^(ftol(ln(w)/ln2)+1), not  | a | PostEffects front-buffer raster size = ExeRasterSize | both creation sites + UV (349-350); other readers use RwRasterGetWidth of the raster | OK |
| 6cbe5dd1 CEntity::IsInCurrentArea compares the zero-extended area cod | b | CEntity::IsInCurrentArea zero-extended | 22 callers, signature unchanged | OK |
| 5b7fd834 audio: CAEVehicleAudioEntity::GetVehicleAudioSettings takes  | b | CAEVehicleAudioEntity::GetVehicleAudioSettings takes model id | caller Initialise (changed in the same commit); no other callers | OK |
| b5dd75d2 audio: CAEPedSpeechAudioEntity::ReservePlayerConversationSpe | a | speech slot reservation (first free / 5 regular slots) | consumers use s_pConversationPedSlot1/2 + Reserved flags; GetNumSlotsPlayingContext callers | OK |
| 52651985 audio: CAEPedSpeechAudioEntity::ReservePedConversationSpeech | a | speech slot reservation (first free / 5 regular slots) | consumers use s_pConversationPedSlot1/2 + Reserved flags; GetNumSlotsPlayingContext callers | OK |
| d0af3b8f Zone script commands 0x46B4DC/0x46B535/0x46ECC0/0x46ED23/0x4 | a | ZoneInfoArray[idx] unchecked / index 0 | ZONEINFO.md audit: all GetZoneInfo(CZone*) / ZoneInfoArray users | OK (ZONEINFO.md) |
| 09b34f44 CGangWars::AddKillToProvocation 0x443950, UpdateTerritoryUnd | a | ZoneInfoArray[idx] unchecked / index 0 | ZONEINFO.md audit: all GetZoneInfo(CZone*) / ZoneInfoArray users | OK (ZONEINFO.md) |
| bbe7c686 CGangWars::CanPlayerStartAGangWarHere 0x443F80: compare with | a | ZoneInfoArray[idx] unchecked / index 0 | ZONEINFO.md audit: all GetZoneInfo(CZone*) / ZoneInfoArray users | OK (ZONEINFO.md) |
| 2efd59d5 CMessages::InsertPlayerControlKeysInString 0x69E160: port th | b | message string helpers | callers via CMessages only | OK |
| 02da4754 CShopping::FindItem 0x49AD20 / GetItemIndex 0x49AB10: an abs | b | CShopping::FindItem/GetItemIndex absent key = -1 | callers Shopping.cpp 90-102,164,187,280,331,369,386-391,522 index with the result unchecked, same as the exe's inlined code | OK (exe parity) |
| 68aded90 CPedAttractor::IsAtHeadOfQueue 0x5EB530 (212/3000) + GetQueu | b | CPedAttractor IsAtHeadOfQueue / GetQueueSlot -1 | PedAttractor.cpp:278-279 | OK |
| 29f409c5 CStreaming::IsCarModelNeededInCurrentZone 0x407DD0: gang che | b | IsCarModelNeededInCurrentZone gang group = BALLAS+gang | DoesCarGroupHaveModelId callers (Streaming 2946-2976), AddToLoadedVehiclesList/ReclassifyLoadedCars already BALLAS+gang | OK |
| 1a208459 CCustomCarEnvMapPipeline::pluginSpecMatDestructorCB 0x5D97D0 | a | CustomCarEnvMapPipeline plugin slot via RW offset arg / pool alloc without defaults | Env/Spec plugin accessors read the same slot; one plugin offset == arg | OK |
| 8b0fa60e CCustomCarEnvMapPipeline::pluginSpecMatCopyConstructorCB 0x5 | a | CustomCarEnvMapPipeline plugin slot via RW offset arg / pool alloc without defaults | Env/Spec plugin accessors read the same slot; one plugin offset == arg | OK |
| 40532208 CCustomCarEnvMapPipeline::SetFxEnvTexture 0x5DA230: a full p | a | CustomCarEnvMapPipeline plugin slot via RW offset arg / pool alloc without defaults | Env/Spec plugin accessors read the same slot; one plugin offset == arg | OK |
| 49600477 CCustomCarEnvMapPipeline::pluginEnvAtmCopyConstructorCB 0x5D | a | CustomCarEnvMapPipeline plugin slot via RW offset arg / pool alloc without defaults | Env/Spec plugin accessors read the same slot; one plugin offset == arg | OK |
| a16161b7 CCustomCarEnvMapPipeline::pluginEnvAtmDestructorCB 0x5D9730: | a | CustomCarEnvMapPipeline plugin slot via RW offset arg / pool alloc without defaults | Env/Spec plugin accessors read the same slot; one plugin offset == arg | OK |
| 8dc466d8 CCustomCarEnvMapPipeline::pluginEnvAtmConstructorCB 0x5D8D30 | a | CustomCarEnvMapPipeline plugin slot via RW offset arg / pool alloc without defaults | Env/Spec plugin accessors read the same slot; one plugin offset == arg | OK |
| bd3b3f91 FxInfoFloat_c 0x4A5DA0 / FxInfoHeatHaze_c 0x4A61A0 / FxInfoS | a | FxInfo* ctors no Allocate(0) | Fx blueprint readers | OK |
| b29df374 FxInfoManager_c::AddFxInfo 0x4A7B00: FxInfo objects are allo | b | FxInfo operator new -> Fx mem pool | operator delete is a no-op; no other allocation/free path of FxInfo | OK |
| 9df3da33 FxManager_c::CalcFrustumInfo 0x4A9140: float arithmetic at P | a | FxSphere m_nNumPlanesPassed kept by CalcFrustumInfo | FxFrustumInfo.cpp:26-34 only | OK |
| f01d66b4 rwmath_exact.h: build the RW sqrt / inverse sqrt tables at 5 | a | rwmath_exact sqrt tables 53 bit | consumers are the RwV3d*/RtQuat* exact helpers only | OK |
| f526d3c0 CCover::FindCoordinatesCoverPoint 0x699570: translated line  | b | CCover::Init / Find2HighestPoints outputs / FindCoordinatesCoverPoint / FindAndReserveCoverPoint | callers inside Cover*.cpp and the cover tasks use the return as the exe | OK |
| 15445027 CCover::FindAndReserveCoverPoint 0x6992B0: translated line b | b | CCover::Init / Find2HighestPoints outputs / FindCoordinatesCoverPoint / FindAndReserveCoverPoint | callers inside Cover*.cpp and the cover tasks use the return as the exe | OK |
| 89d27865 CObject::GrabObjectToCarryWithRope 0x5A1AB0: the grabbed ent | a | Rope m_pRopeAttachObject (+0x318) vs m_pAttachedEntity (+0x314) | all Rope.cpp / Ropes.cpp / Vehicle.cpp winch readers use m_pRopeAttachObject for the picked-up entity | OK |
| d1d80b46 CCover::Init 0x698710: only the type byte of every cover poi | b | CCover::Init / Find2HighestPoints outputs / FindCoordinatesCoverPoint / FindAndReserveCoverPoint | callers inside Cover*.cpp and the cover tasks use the return as the exe | OK |
| 2b83e181 CCover::Find2HighestPoints 0x6988E0: every branch writes bot | b | CCover::Init / Find2HighestPoints outputs / FindCoordinatesCoverPoint / FindAndReserveCoverPoint | callers inside Cover*.cpp and the cover tasks use the return as the exe | OK |
| 5e67b6ac CCarCtrl::ReInit 0x4213B0 / CPopulation::m_LoadedGangCars: t | a | CPopulation::m_LoadedGangCars 10 -> 11 groups | CarCtrl 337/362 (range-for), Population:1538, Streaming 2173/2691/3642 (index by gang < 10) | OK |
| f7d34ed6 CPad::StartShake_Train 0x53FA70: take the point by value (tw | b | CPad::StartShake_Train(CVector2D by value) | no other callers in the port | OK |
| 58cd4c0a CPad::Update 0x541C40: shift the steering buffer right by on | a | CPad::SteeringLeftRightBuffer shift direction | readers Pad.cpp:644 (GetSteeringLeftRight), init fill | OK |
| 83507b8a CPad::ReconcileTwoControllersInput 0x53F530: keep the functi | a | CPad::ReconcileTwoControllersInput static -> 0xB736C0 | no other writer; returns a copy | OK |
| 734c45f2 CControllerConfigManager::DeleteMatchingVehicle_3rdPersonCon | b | DeleteMatchingVehicle_3rdPersonControls extra arg | 4 call sites ControllerConfigManager.cpp:1213-1239 already pass 3 args | OK |
| 799b61c0 CGlass::Init 0x71A8D0 / FindFreePane 0x71ACA0: five pane pol | a | CGlass pane polygons 4->5, panes 44->45 | Glass.cpp loops use std::size, FallingGlassPane:78/106, PanePolyCenterPositions[5] | OK |
| b348c32f CWaterCannon 0x728B40 / 0x728C20 / 0x728C80 / 0x728CB0: m_nS | a | CWaterCannon m_nSectionsCount int16 | WaterCannon.cpp, AEWaterCannonAudioEntity:117-123 | OK |
| 54f550e0 CMessages::InsertStringInString 0x69E040 (pre-fix 452/1500 P | b | message string helpers | callers via CMessages only | OK |
| 052732bf CMessages::Process 0x69EE60 (pre-fix 1484/1500 PC24 oracle c | a | CMessages queue direction / previous-brief array | BriefMessages readers (Group19:170), ClearThisPrint_Impl, AddToPreviousBriefArray, all in Messages.cpp | OK |
| 6a50a355 CMessages::ClearThisPrint 0x69EA30 (pre-fix 47/1500 PC24 ora | a | CMessages queue direction / previous-brief array | BriefMessages readers (Group19:170), ClearThisPrint_Impl, AddToPreviousBriefArray, all in Messages.cpp | OK |
| c9b7ed7f CMessages::AddToPreviousBriefArray 0x69DD50 (pre-fix 1241/15 | a | CMessages queue direction / previous-brief array | BriefMessages readers (Group19:170), ClearThisPrint_Impl, AddToPreviousBriefArray, all in Messages.cpp | OK |
| f65f2b74 CMessages::Init 0x69EE00 (pre-fix 98/100 PC24 oracle cases): | a | CMessages queue direction / previous-brief array | BriefMessages readers (Group19:170), ClearThisPrint_Impl, AddToPreviousBriefArray, all in Messages.cpp | OK |
| 86f90f44 CTheZones::Init 0x572670 (follow-up) - the clear covers byte | a | CTheZones::Init clear range | zone arrays are bss-zero | OK |
| 66c2e732 CTheZones::Init 0x572670 - clears only the first 0x17 bytes  | a | CTheZones::Init clear range | zone arrays are bss-zero | OK |
| 326a5e80 CPathFind::AddDynamicLinkBetween2Nodes_For1Node 0x44E000 - t | a | dynamic-link free blocks (area 0xFFFF), m_aDynamicLinksBaseIds free = <0 | Init fill -1, RemoveInterior (-1 writes/reads), AddDynamicLink | OK |
| 1ef99baa CPathFind::MarkRegionsForCoors 0x44DB60 - flags are stored a | a | ToBeStreamed layout (MarkRegionsForCoors) | all readers: PathFind.cpp UpdateStreaming/LoadScene (fixed by 729926c4); no other TU touches it | OK after 729926c4 |
| 80f1ea56 CPathFind::ReturnInteriorNodeIndex 0x451300 - not found retu | b | ReturnInteriorNodeIndex not-found value | no port callers (hook only) | OK |
| e987cf5c CPathFind::Init 0x44D080 - also stores 0 into the exe global | a | NumTempExternalNodes: local static -> exe global 0x96F02C | only Init writes it, no readers | OK |
| 16d641a5 CGridRef::GetArtistBugstarID 0x71D650 - in range the exe ret | b | GetArtistBugstarID / MakeUpperCase / AddExistingPedInCar return value | no caller uses the changed value (Conversations/Group19 ignore MakeUpperCase result) | OK |
| b4d5429a MakeUpperCase 0x7186E0 - returns the start of the destinatio | b | GetArtistBugstarID / MakeUpperCase / AddExistingPedInCar return value | no caller uses the changed value (Conversations/Group19 ignore MakeUpperCase result) | OK |
| 478e97cb FindPlayerHeading 0x56E450 - the remote vehicle is used when | b | FindPlayerHeading/CentreOfWorld remote vehicle first | callers use the public API; no workaround | OK |
| 8aaf11af FindPlayerCentreOfWorld 0x56E250 - the remote vehicle comes  | b | FindPlayerHeading/CentreOfWorld remote vehicle first | callers use the public API; no workaround | OK |
| 662d7a1c CScripted2dEffects::GetIndex 0x6F9F60 / GetEffectPairs 0x6FA | b | Scripted2dEffects IndexOfEffect exact-address match | GetIndex/GetEffectPairs callers (AttractorScanner, PedAttractorManager, TaskComplexUse*) | OK |
| 1920b531 CTheScripts::AddToWaitingForScriptBrainArray 0x46AB60 - the  | a | script brain / connect-LOD arrays: LAST free slot | every reader iterates the whole array | OK |
| 06c62fa0 CTheScripts::AddToListOfConnectedLodObjects 0x470980 - the e | a | script brain / connect-LOD arrays: LAST free slot | every reader iterates the whole array | OK |
| 0727bca0 Camera: CCamera::ProcessMusicFade 0x50B6D0 - the fade-in bra | a | CCamera music fade flag selection | m_nFadeInOutFlag/m_nMusicFadingDirection readers Camera.cpp/Cam.cpp | OK |
| bfba9497 PathFind: CPathFind::AddInteriorLink 0x44DED0 - first negati | a | ConnectsToGiven row overflow (entry 6 = next row) | ConnectsToGiven readers PathFind.cpp 1456-1600 (all stride 6) | OK |
| f5819d91 Interior: FurnitureSubGroup_c::AddFurniture 0x5C00C0 - width | b | FurnitureGroup_c::AddFurniture always true / SubGroup width | single caller FurnitureManager_c:91 (sub-group variant) | OK |
| d75b6f91 Interior: InteriorManager_c::IsInteriorEffectVisible 0x59869 | b | InteriorManager_c Get*Interior / SetEntryExitPtr / PruneVisibleEffects | callers: TaskInteriorGoToInfo:76, TaskComplexFollowNodeRoute:278-286, EntryExit:440 | OK |
| 9ef35d9e Interior: InteriorManager_c::PruneVisibleEffects 0x598A60 -  | b | InteriorManager_c Get*Interior / SetEntryExitPtr / PruneVisibleEffects | callers: TaskInteriorGoToInfo:76, TaskComplexFollowNodeRoute:278-286, EntryExit:440 | OK |
| 513bf802 Interior: InteriorManager_c::GetPedsInterior 0x598620 - sear | b | InteriorManager_c Get*Interior / SetEntryExitPtr / PruneVisibleEffects | callers: TaskInteriorGoToInfo:76, TaskComplexFollowNodeRoute:278-286, EntryExit:440 | OK |
| d3deecd4 Interior: InteriorManager_c::GetVectorsInterior 0x5983D0 - t | b | InteriorManager_c Get*Interior / SetEntryExitPtr / PruneVisibleEffects | callers: TaskInteriorGoToInfo:76, TaskComplexFollowNodeRoute:278-286, EntryExit:440 | OK |
| 4770d7e8 Interior: InteriorManager_c::SetEntryExitPtr 0x598180 - stor | b | InteriorManager_c Get*Interior / SetEntryExitPtr / PruneVisibleEffects | callers: TaskInteriorGoToInfo:76, TaskComplexFollowNodeRoute:278-286, EntryExit:440 | OK |
| d383a6c1 Interior: FurnitureGroup_c::AddFurniture 0x5C0230 - add to e | b | FurnitureGroup_c::AddFurniture always true / SubGroup width | single caller FurnitureManager_c:91 (sub-group variant) | OK |
| 2835c0db CEntity::SetupBigBuilding 0x533150: set m_bDontCastShadowsOn | a | CEntity::SetupBigBuilding flag bit | callers FileLoader:2188, ObjectCommands:33, Group05_08:364, TheScripts:2056 | OK |
| 70d2e657 CAEPedSpeechAudioEntity::GetNumSlotsPlayingContext 0x4E49B0: | a | speech slot reservation (first free / 5 regular slots) | consumers use s_pConversationPedSlot1/2 + Reserved flags; GetNumSlotsPlayingContext callers | OK |
| 5581527e CAEVehicleAudioEntity RequestBankSlot 0x4F4D10 / DemandBankS | a | dummy engine bank slot RefCnt / victim selection | all users inside AEVehicleAudioEntity.cpp (Request/Demand/Release) | OK |
| 599d2d8a CAECollisionAudioEntity::GetCollisionSoundStatus 0x4DA830: o | b | GetCollisionSoundStatus outIndex = list size when none looping | only caller (ReportCollision) uses outIndex under LOOPING | OK |
| d2ab2148 CAEScriptAudioEntity::Reset 0x4EC150: clear m_Physical (+0x1 | a | CAEScriptAudioEntity::Reset clears m_Physical | readers of m_Physical/m_Entity in ScriptAudio | OK |
| ab60914a CAEFireAudioEntity::StaticInitialise 0x5B9A90: load the expl | a | CAEFireAudioEntity::StaticInitialise bank/slot | rain bank loaded by AEWeatherAudioEntity.cpp:32; explosions by AEExplosion/AEFire | OK |
| 9abce993 CAnimBlendClumpData::SetNumberOfBones 0x4CF140: round the by | a | anim blend array byte count rounded up to 64 | allocation sites only; freed with matching Free | OK |
| 725bf5cf CAnimBlendAssociation::AllocateAnimBlendNodeArray 0x4CE9F0:  | a | anim blend array byte count rounded up to 64 | allocation sites only; freed with matching Free | OK |
| 203b5d84 CAnimBlendAssocGroup::IsCreated 0x4D37A0: test the associati | b | CAnimBlendAssocGroup::IsCreated | no callers | OK |
| 17342786 ClumpAnimConstruct 0x4D5F40: clear the plugin slot at the of | a | anim clump plugin slot at RW offset arg | single plugin; offset arg == registered offset | OK |
| 72368293 RpAnimBlendClumpExtractAssociations 0x4D6BE0: clear the extr | a | extracted association list head prev link | RpAnimBlendClumpExtractAssociations / restore RpAnimBlend.cpp 287-303 | OK |
| e283a4ff RpAnimBlendClumpGetMainAssociation_N 0x4D6A30 / GetMainParti | b | RpAnimBlendClumpGetMain(Partial)Association_N predicate | no callers (decl only) | OK |
| ea549221 RpAnimBlendClumpGetNumAssociations 0x4D6B60 / GetNumNonParti | a | CAnimBlendLink::begin() skips the list head | all range-for/count_if users of RpAnimBlendClumpGetAssociations (3 sites, fixed in the commit); manual loops already start at .next | OK |
| c3f69060 CColPoint 0x40FC80 call sites: ProcessColModels 0x419236/0x4 | a | CColPoint::operator= vs AssignLikeExe (padding) | all 17 AssignLikeExe sites listed; operator= is the full copy elsewhere | OK |
| de1c6bf5 CBike::ProcessEntityCollision 0x6BDEA0: driver colpoints cop | a | CColPoint::operator= vs AssignLikeExe (padding) | all 17 AssignLikeExe sites listed; operator= is the full copy elsewhere | OK |
| a66064f4 CAutomobile::ProcessEntityCollision 0x6ACE70: forklift filte | a | CColPoint::operator= vs AssignLikeExe (padding) | all 17 AssignLikeExe sites listed; operator= is the full copy elsewhere | OK |
| 40f01968 CColPoint: operator= is the full copy again, AssignLikeExe ( | a | CColPoint::operator= vs AssignLikeExe (padding) | all 17 AssignLikeExe sites listed; operator= is the full copy elsewhere | OK |
| 5b5d28df CColPoint::operator= 0x40FC80: do not copy the padding float | a | CColPoint::operator= vs AssignLikeExe (padding) | all 17 AssignLikeExe sites listed; operator= is the full copy elsewhere | OK |
| bf204ed1 CGarages::TriggerMessage 0x447B80: first number goes to 0x96 | a | CGarages MessageNumberInString/2 (0x96C00C/0x96C010) store order | reader CGarages::PrintMessages passed (00C,010) = reversed vs exe 0x447884-0x44789A (HO_ONE/OY_ONE showed total, collected) | FIXED 671d3437 |
| 0f080c86 CPopulation::AddExistingPedInCar 0x611560: return the ped in | b | GetArtistBugstarID / MakeUpperCase / AddExistingPedInCar return value | no caller uses the changed value (Conversations/Group19 ignore MakeUpperCase result) | OK |
| afe57023 CPedGroup::FindDistanceToNearestMember 0x5FB0A0: out paramet | b | CPedGroup::FindDistanceToNearestMember out = last follower | callers PlayerPed:2842 (nullptr out), GangWars:168 (only checks non-null) | OK |
| 54fce88b CEventScriptedAttractor::Constructor 0x5FEF40: two stack arg | b | CEventScriptedAttractor::Constructor 2 args | hook only | OK |
| 032d3a91 CPed::SetAimFlag(float) 0x5E8830: stores the heading | a | CPed flag bits ClearLookFlag/SetLookFlag/SetAimFlag | bShakeFist/bIsDrowning readers: no compensating write in the port | OK |
| 3b9453dc CPed::ClearLookFlag 0x5E1950 (+ClearLook 0x5E3FF0): clears b | a | CPed flag bits ClearLookFlag/SetLookFlag/SetAimFlag | bShakeFist/bIsDrowning readers: no compensating write in the port | OK |
| 3f623669 CPed::SetLookFlag(float) 0x5DEDC0: clears bIsRestoringLook | a | CPed flag bits ClearLookFlag/SetLookFlag/SetAimFlag | bShakeFist/bIsDrowning readers: no compensating write in the port | OK |
| 19189c63 CPlayerPed::IsHidden 0x609620: hidden when NOT in a vehicle  | b | CPlayerPed::IsHidden inverted | one caller Radar.cpp:2499 (blip colour) | OK |

Class c (local arithmetic / control flow, 172 commits) and t (tests / tooling / hook address, 50 commits), one per line:

0badf286 t gameplay_regress: original-exe poller reads the ped pool right (stride 0x7C4, ty
ff4685f3 c CPopulation::GeneratePedsAtAttractors (0x615970): exe loop shape - pass counter 
a7b39d64 c CPopulation::AddPedAtAttractor (0x614210): refuse by the exe's 2D squared distan
053b1245 t gameplay_regress: interior cops_on_floor judged inside the interior (>= 10 s), m
2bd9f6bf c PostEffects: InfraredVisionStoreAndSetLightsForHeatObjects scales the death time
cd6ec7ec t tests: wave3_render_oracle_test - exe-oracle rows for 38 functions of Renderer, 
d7f34d0e c audio: CAEPedSpeechAudioEntity::CanWePlayGlobalSpeechContext follows the exe (0x
3239d0cc t tests: wave3_audiohw_oracle_test - exe-oracle rows for 19 CAEAudioHardware funct
1ab54ccf c CHud::SetHelpMessageStatUpdate 0x588D40: any non-zero state byte gives '+'
f6a76f9d t oracle: link/forward the callee TUs that cut 26 wave2/wave3 rows short (LINK-CAL
435fc23e t oracle harness: W2_PORT_FAULT_IS_DIFF=1 switch for every wave2/wave3 row (port-o
122d0b50 c CWorld::FindObjectsKindaCollidingSectorList 0x565000: stamp the scan code of eve
1605e16b c CVehicle::UpdateClumpAlpha 0x6D2980: an alpha already >= 255 is stored unchanged
7fd4793f c CRestart::FindClosestHospitalRestartPoint 0x460850: NaN radius / distance / stat
b4af1637 c CTrafficLights::FindTrafficLightTypeFromOrientation 0x49D4D0: NaN heading is wes
5da249d7 c CTrafficLights::LightForPeds 0x49D400: green until masked time 15383 inclusive (
e36d53ed t ORACLE-W3-GP2: wave3_gp2_oracle_test, rows for TrafficLights / Restart / Shoppin
f06bb2d3 c CStreaming::AreTexturesUsedByRequestedModels 0x409A90: loading-channel TXD model
1cdfdf58 t ORACLE-W3-STREAM: exe-oracle target wave3_stream_oracle_test (IplStore 12, TxdSt
327f1197 c CTagManager::SetAlphaInArea 0x49CFE0: set the tag's alpha also when its entity h
68ed92a0 c CTagManager::FindTagDesc 0x49CCB0 (+ GetAlpha 0x49CF90 / SetAlpha 0x49CEC0 users
13d50c4e c CTagManager::AddTag 0x49CC90: write Entity and Alpha only, not the padding bytes
fde4b475 c CTagManager::ShutdownForRestart 0x49CC60: clear only the first ms_numTags alphas
92586c20 t ORACLE-W3-MISC: exe-oracle target wave3_misc_oracle_test (Localisation + TagMana
c5b700ad t ORACLE-W3-CARENV: wave3_carenv_oracle_test, 24 rows for CustomCarEnvMapPipeline 
2a9bb2a1 t CConversations: drop EnableConversation hook 0x43A7F0 (mid-instruction inside Cl
14289ca4 t CTaskSimpleGiveCPR: FinishGiveCPRAnimCB hook 0x658910 -> 0x658990 (0x658910 is m
4b1a1adc t CVehicleModelInfo: SetRenderPipelinesCB hook 0x4C88F4 -> 0x4C88F0 (0x4C88F4 is t
d0abeab7 t LoadingScreen: IsActive hook 0x744DB5 -> 0x744DB0 (0x744DB5 is the bare ret of '
e16464c0 t Game: ValidateVersion hook 0x5BA0BC -> 0x5BA060 (0x5BA0BC is a jl target inside 
249b02a5 t VisibilityPlugins: hook SetClumpForAllAtomicsFlag at its real address 0x7322F0 (
8bcf0ed5 c app_light: GetDayNightBalance multiplies by the float 1/60 instead of dividing b
ea90a464 c app_light: SetLightColoursForPedsCarsAndObjects adds the green / blue brightness
a0fae8ce c app_light: SetAmbientColoursToIndicateRoadGroup indexes the colour tables with i
a64a12a4 c app_light: SetFlashyColours / SetFlashyColours_Mild copy only the rgb of the dir
3e9c033b c VisibilityPlugins: CalculateFadingAtomicAlpha takes min(drawDist, radius) with t
dc00cac8 c VisibilityPlugins: InsertEntityIntoSortedList compares the sign-extended model i
c62bc2bc c VisibilityPlugins: GetDotProductWithCameraVector sums its dot products in y,x,z 
f84048fc c VisibilityPlugins: GetDistanceSquaredFromCamera sums z*z+y*y+x*x like the exe (0
de6c430f t ORACLE-W3-VIS: wave3_vis_oracle_test target + rows for VisibilityPlugins.cpp (A/
0fad3442 c FxInfoEmRate_c::GetValue 0x4A4B60: NaN (currentTime - totalTime) takes the >= 0 
e2a2a67b c FxInfoAttractLine_c::GetValue 0x4A5850: RwV3dLength / RwV3dNormalize (table sqrt
2979bd8f c FxInfoAttractPt_c::GetValue 0x4A56E0: RwV3dLength / RwV3dNormalize (table sqrt, 
57d5f8cb c Fx_c RotateVecAboutVec 0x4A1780: the exe's association of the products ((z*x)*t,
c9202502 t ORACLE-W3-FXBP: wave3_fxs_oracle_test rows for game_sa/Fx/Blueprints (FxInfo*::G
f5282ee0 c FxSystem_c::FxSystem_c 0x4AAF00: clear m_FireAE.m_FxSystem ([+0x100] = 0, the in
268d80ee c FxSphere_c::GetDistToPlane 0x4AA010: sum the plane products in the exe's y, x, z
27235c0f t tests: wave3_fxs_oracle_test - game_sa/Fx rows (FxSystem setters, FxManager, FxM
a195d7e9 t tests: wave3_ik_oracle_test - IKChainManager_c GetLookAtEntity / GetLookAtOffset
f36778c0 c IKChain_c::Init 0x618370: register the ped reference only after the checks passe
241f3f61 c IKChain_c::MoveBonesToTarget 0x6178B0: x87 evaluation order of the solver (RwV3d
ebd00d83 c BoneNode_c::CalcWldMat 0x616CD0: build the rotation inline like the exe (s = 2/|
e6595642 c IKChain_c::IsAtTarget 0x617F30: distance via RwV3dLength (0x7EDAC0, approximate 
a9ab7e4a c BoneNode_c::Limit 0x617650: head Z-range factor is 1 - |x| * (1/45f) with a 0 fl
d0641030 c IKChain_c::GetBoneNodeFromTag 0x617C60: compare the full 32-bit tag (cmp [ebx+8]
964fe432 c BoneNode_c::GetIdFromBoneTag 0x617050: compare the full 32-bit tag (cmp edx, mov
4e24f8b5 t tests: wave3_ik_oracle_test - exe-oracle rows for game_sa/Ragdoll BoneNode / Bon
a85bceb2 t wave3_input_oracle_test: 18 min -> 1 min (it never hung: run_all_tests kills ora
68ab4360 t ORACLE-W3-AI: report, regenerated ORACLE_COVERAGE.md (4275 -> 4335), PHASE2_STAT
612447da t ORACLE-W3-AI: wave3_ai_oracle_test (PedIntelligence, Object, CarCtrl, Cover rows
231ff3a1 c CCarCtrl::SlowCarDownForObject 0x426220: the factor is min(*arg3, (1 - (heading 
47b849dc c CCarCtrl::RemoveFromInterestingVehicleList 0x423ED0: clear every slot holding th
addd6804 c CObject::SetMatrixForTrainCrossing 0x59F200: keep fcos / fsin unrounded in the p
57e45fd5 c CPedIntelligence::CanSeeEntityWithLights 0x605550: x87 order of the exe ((dist /
95596322 c CPedIntelligence::GetActivePrimaryTask 0x4B85B0: test all three primary slots (p
5bf7621c c CControllerConfigManager::AffectPadFromKeyBoard 0x531140: release handlers outsi
6dc3198e c CControllerConfigManager::AffectPadFromMouse 0x5314A0: run the release handlers 
f957dc6c c CControllerConfigManager Driving 0x52F7B0 / FirstAndThirdPersonOnly 0x52FAB0: th
f38a5546 c CControllerConfigManager::UpdateJoyInConfigMenus_ButtonDown/Up 0x52DAB0 / 0x52DC
30a537e4 c CControllerConfigManager::GetIsMouseButtonJustUp 0x52F110: test 'just released' 
288c4b70 c CControllerConfigManager::SetMouseButtonAssociatedWithAction 0x52F590: do not ca
147cc0ad c CPad::StartShake_Train 0x53FA70: measure the distance to the camera in the plane
aa504f22 c CPad::StartShake_Train 0x53FA70: return when the player IS in a train, not when 
370682be c CKeyboardState::Clear 0x53F090: leave the numpad ENTER word (+0x23C) untouched l
cedc8737 t ORACLE-W3-INPUT: wave3_input_oracle_test, exe-oracle rows for ControllerConfigMa
6bf7ac53 t ORACLE-W3-EFFECTS: wave3_fx_oracle_test, 71 exe-oracle rows for the world-effect
f4916541 c CFireManager::GetNextFreeFire 0x539E50: with no free slot take the first fire th
ae6c28f2 c CClouds::VolumetricCloudsGetMaxDistance 0x713630: a NaN far plane gives 600 (fco
58fafcc1 c CGlass::CalcAlphaWithNormal 0x71ACF0: the exe's float operation order and specul
b29ee373 c CPlantMgr::UpdateAmbientColor 0x5DB310: 64 - _ftol2(amb * 2.5 * -255) clamped to
dfbaf1e5 c CClouds::MovingFogInit 0x713660: clear only the 350 slot flags like the exe (rep
e68d59c8 t ORACLE-W3-UI: link MenuManager_Input / Pad; direction-check, CheckHover, BinaryS
4a80ff41 c CFont::FilterOutTokensFromString 0x719240: copy into a 252 byte stack buffer lik
aed3c58e c CMenuManager::CheckHover 0x57C4F0 (pre-fix 98/2000 PC24 oracle cases): integer b
9ec251d0 c CFont::SetDropColor 0x719510 (pre-fix 235/500 PC24 oracle cases): the drop-shado
654d6cd8 t ORACLE-W3-UI: Font / Text / Sprite2d / MenuManager rows; Font.h: GetScriptLetter
df108529 t ORACLE-W3-UI: Hud and Radar rows; link Radar / Font / Sprite / MenuManager / Tex
30941673 c CRadar::SetCoordBlip 0x583820 (pre-fix 642/1000 PC24 oracle cases; SetShortRange
db24086b c CRadar::ChangeBlipBrightness 0x583C70 (pre-fix 9/700 PC24 oracle cases): brightn
360fac38 t ORACLE-W3-UI: wave3_ui_oracle_test target, harness and Messages.cpp rows (rows f
717b0393 c CWorld::FindNearestObjectOfTypeSectorList 0x565450 (pre-fix 453/1500 PC24 oracle
8ac26b78 c CWorld::FindObjectsOfTypeInRangeSectorList 0x5635C0 (pre-fix 6/1500 PC24 oracle 
56ad0008 c CWorld::FindObjectsInRangeSectorList 0x563500 (pre-fix 12/1500 PC24 oracle cases
1038eeae t ORACLE-W3-PATHS: exe-oracle rows for CPathFind route finding, CWaterLevel tables
d92485af c CShadows::RemoveOilInArea 0x7074F0 (pre-fix 555/1000 PC24 oracle cases) - the ar
9eccb9d3 c CStaticShadow::Free 0x707670 (pre-fix 121/600 PC24 oracle cases) - m_pPolyBunch 
521a52a7 c CWaterLevel::FindNearestWaterAndItsFlow 0x6E9D70 (pre-fix 2/1200 PC24 oracle cas
0eeda179 c CWaterLevel::AddWaterLevelVertex 0x6E5A40 (pre-fix 490/1500 PC24 oracle cases) -
843cda95 c CCullZones::Update 0x72DEC0 - CurrentFlags_Camera is sign-extended from the 16-b
9612b363 c CCullZones::Update 0x72DEC0 - CurrentFlags_Player is the SIGN-extended 16-bit at
a92493b2 c CPathFind::SetLinksBridgeLights 0x44D960 - the bounds are strict (xMin < x < xMa
30a219b7 c CPathFind::SwitchRoadsOffInAreaForOneRegion 0x452820 / SwitchRoadsOffInArea 0x45
64084ae6 c CPathFind::SwitchOffNodeAndNeighbours 0x452160 - a linked node is skipped when i
9bc4f649 c CPathFind::ThisNodeWillLeadIntoADeadEnd 0x44D310 - a candidate link has behaviou
820823bf c CalcScreenCoors(in, out, scaleX, scaleY) 0x71DA00 - scale = (width * (1/z)) / fo
9eadfd54 t ORACLE-W3-WORLD: wave3_world_oracle_test - exe-oracle rows for CWorld sector acc
d9e46007 c CTheScripts::ProcessAllSearchLights 0x4939F0 - each state heads for its own dest
5ce2b57a c CTheScripts::IsVehicleStopped 0x4861F0 - the step used is 'ts < old ? ts : old' 
8512cec0 c CTheScripts::AddToInvisibilitySwapArray 0x481200 - an entity that is not in the 
637ca9ad c CTheScripts::InitialiseSpecialAnimGroupsAttachedToCharModels 0x474730 / Initiali
4355f106 c CRunningScript::ReadParametersForNewlyStartedScript 0x464500 - float arguments a
6e2c5e86 c CRunningScript::Init 0x4648E0 - the exe copies the 7 bytes "noname\0" into m_szN
c6d544dd t ORACLE-W3-SCRIPTS: wave3_scripts_oracle_test - exe-oracle rows for 48 uncovered 
c130d582 t ORACLE-W3-CAMERA: link ColSphere.cpp into wave3_camera_oracle_test (CColSphere::
061153ec c Camera: Process_Cam_TwoPlayer_Separate_Cars(_TopDown) 0x513510 / 0x513BE0 - unor
d2f52ff7 c Camera: CCam::Process_Cam_TwoPlayer_Separate_Cars_TopDown 0x513BE0 - raw m_pVehi
6aa8b8d5 c Camera: CCam::Process_Cam_TwoPlayer_Separate_Cars 0x513510 - exe summation order
cc84ac40 c Camera: CCamera::SetZoomValueCamStringScript 0x50C1B0 - branch on the entity TYP
97be17b6 c Camera: CCamera::TakeControl 0x50C7C0 - MODE_NONE picks the mode from the type o
fa7009db c Camera: CCamera::RestoreWithJumpCut 0x50BAB0 - also sets m_bCameraJustRestored, 
d68fefd2 c Camera: CCamera::SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString 0x50BD
c95a417a c Camera: CCamera::InitialiseCameraForDebugMode 0x50AF90 - empty like the exe (a s
4f110079 t ORACLE-W3-CAMERA: wave3_camera_oracle_test harness (TheCamera / CCam / gIdleCam 
d6b98664 t ORACLE-W3-INTERIOR: ArePathsLoaded row (PathFind.cpp linked, ThePaths area table
ff01277d t ORACLE-W3-INTERIOR: link Collision/Box.cpp (IsInteriorEffectVisible) and build t
cd7397b1 c Interior: Interior_c::IsPtInside 0x5913E0 - reject only when limit < |distance| 
bee3830b c Interior: InteriorGroup_c::FindClosestInteriorInfo 0x594A50 - squared distance s
78b45cf3 t ORACLE-W3-INTERIOR: wave3_interior_oracle_test - exe-oracle rows for the game_sa
a9ce94b5 t ORACLE-W3-ENTITY: wave3_entity_oracle_test - exe-oracle rows for the game_sa/Ent
0570887d c CPhysical::ApplyFriction(other) 0x545980: speed magnitudes as the exe's per-bran
9dde0ab4 c CEntity::IsEntityOccluded 0x71FAE0: corner points go through GetMatrix() one by 
7aa67f2a c IsPointInsideLine 0x71E050: 'not less than' (equal and NaN are inside); exe-orac
d3d9d22a c IsDummyPointerValid 0x532730: no slot-state check, index may equal the pool size
dfcf35f9 c CPhysical::GetLightingFromCol 0x5447B0: lighting * scale + RGB2 term (the port m
de45bf86 c CEntity::CalculateBBProjection 0x535340: axis lengths are XY-only (no z), exe ab
99507b73 c CEntity::GetIsTouching(centre, radius) 0x5344B0: x87 sequence verbatim ((dz^2 + 
c0da05e4 c CEntity::ModifyMatrixForBannerInWind 0x535040: scale by the selected wind consta
4b7c4651 c CEntity::BuildWindSockMatrix 0x532DB0: the wind direction is negated (x and y); 
55eb5775 c CPhysical::RemoveRefsToEntity 0x544280: walk from slot 0 (the port's empty-body 
1a4581ee c CAEVehicleAudioEntity::GetFreqForPlayerEngineSound 0x4F8070: caddy idle 1.0, lef
01c5533b c CAEVehicleAudioEntity::GetVolForPlayerEngineSound 0x4F5D00: ID uses SpeedRatio, 
35420a0b c CAEPedAudioEntity::UpdateJetPack 0x4E0EE0: fcom clamps (NaN -> limit), 0.2f / pi
5981b467 c CAEPedSpeechAudioEntity::GetSpecificSpeechContext 0x4E4470 / GetRepeatTime 0x4E4
a406da18 c CAEPedSpeechAudioEntity::CanPedSayGlobalContext 0x4E5730: no pain table, ped typ
c11b8e33 t ORACLE-W3-AUDIO: wave3_audio_oracle_test - rows for game_sa/Audio/Entities + Aud
ab13b33d c CAETwinLoopSoundEntity::Initialise 0x4F28A0: write the twin loop members only
4d62eb41 c CAEWeaponAudioEntity::Reset 0x503490: clear only the members the exe clears
22b93013 c CAESound::Initialise 0x4EFE50: write the fields one by one, keep m_IsAudioHardwa
8c4b9d55 c CAESound::SetPosition 0x4EF880: camera distance sums the squares as (dz*dz + dy*
2c379d40 c CAESound::UpdatePlayTime 0x4EF2E0: add the play time before the float to int tru
232b88a9 c CAEAudioEnvironment::GetReverbEnvironmentAndDepth 0x4D8010: an active zone with 
9e714c35 c CAEAudioEnvironment::GetDirectionalMikeAttenuation 0x4D7F60: normalise a copy of
b1ffa81f c CVehicleModelInfo::SetEnvMapCoeffCB 0x4C88B0: coefficient is the unsigned data t
b21021cf t ORACLE-W3-MODELS: rows for the txd refcount helpers, SetEditableMaterialsCB (rem
676d45c7 c CAtomicModelInfo::SetupVehicleUpgradeFlags 0x4C4570: the right-wing prefix is "w
b4b4d510 c CAtomicModelInfo::SetupVehicleUpgradeFlags 0x4C4570: clear the upper flag byte b
34cf815c c CAtomicModelInfo::GetAtomicFromDistance 0x4C44B0: a NaN distance gives no atomic
0a6d489b c SetAtomicModelInfoFlags 0x5B3B20: special-type assignment order is tag, breakabl
f618b472 c CVehicleModelInfo::GetNumDoors 0x4C73C0: the door count is a signed byte (movsx 
36066ab7 c CBaseModelInfo::SetHasBeenPreRendered 0x4C42F0: any non-zero int sets the bit
aacd9a49 c SetBaseModelInfoFlags 0x5B3AD0: draw-last is set by the draw-last OR the additiv
d6c81b8f t ORACLE-W3-MODELS: wave3_models_oracle_test, first 80 rows for game_sa/Models cla
e3443253 c CAnimBlendAssocGroup::InitEmptyAssociations 0x4CDFB0: iterate the associations a
b62b8c67 c CAnimBlendAssociation::UpdateBlend 0x4D1490: re-arm the callback as the same kin
20ca2ff0 c RpAnimBlendFrameSetName 0x4D5F00 / SetFrameNodeName 0x72FB00: zero-pad the node 
3d50d1ec t ORACLE-W3-ANIM: wave3_anim_oracle_test, 62 rows for game_sa/Animation + RpAnimBl
d934571e c CTaskComplexPolicePursuit::CreateSubTask 0x68D910: TASK_COMPLEX_ARREST_PED retur
f821363c c CTaskComplexDrivePointRoute::CreateNextSubTask 0x63CF90: compare the incremented
61b87bf1 c CTaskComplexCarSlowBeDraggedOut::CreateSubTask 0x64C6A0: CTaskSimpleCarSetPedOut
646d84d9 c CTaskComplexStealCar::CreateSubTask 0x640F30: TASK_FINISHED tests the ped for nu
10eb64f5 c CTaskComplexFleeEntity::CreateSubTask 0x65BA30: do not restart the position time
cf715561 c CTaskComplexFollowPointRoute::GetNextWaypoint 0x669420: LOOP mode wraps to entry
f9f63d10 c CTaskComplexWalkRoundBuildingAttempt::ComputeMoveDir 0x654B80: measure from the 
b6e03077 c CTaskSimpleCarShuffle::Clone 0x649C40: copy constructor passes the source's vehi
3e31a189 c CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::Clone 0x646EC0: copy construct
45d01d60 t ORACLE-W3-COLL: wave3_coll_oracle_test, 100 exe-oracle rows for the uncovered ga
2a5e544e c CMatrix Invert(in,out) 0x59B920: pos built with a float store per step
6424574b c CMatrix operator*-Mat 0x59BE30: per-component add order of the exe
67ca5b3c c CRect::IsPointInside 0x404290 / 0x4042D0: NaN passes every bound test
dc66f353 c CVector2D::Magnitude 0x420860: square, add and root in extended precision
4634f727 c CColTrianglePlane::Set 0x411660: plane offset and dominant axis from the quantiz
d8306c67 t ORACLE-W3-TASKS: wave3_tasks_oracle_test, 40 exe-oracle rows for game_sa/Tasks/T
319f75ff c CTaskSimpleWaitForBus::ProcessPed 0x632AA0: return false and keep m_bus when no 
07fbc086 c CTaskComplexGoToCarDoorAndStandStill::IsVehicleInRange 0x6458A0: range^2 > sum i
47beedbc c CGarages::ChangeGarageType 0x4476D0: modify the garage, not a copy of it
a237e542 c CPopulation::FindPedRaceFromName 0x5B6D40: a NUL first character does not end th
7aae104e c CGarage::BuildRotatedDoorMatrix 0x4479F0: sin/cos stored as float, angle kept un
907ee4e5 c CPedGroupMembership::RemoveAllFollowers 0x5FB190: skip the leader slot (index 7)
74ff4e46 c CWeaponEffects::ClearCrossHairsImmediately 0x742CC0: clear with the immediately 
f22181e5 t ORACLE-W3-GAMEPLAY: wave3_gameplay_oracle_test (gen_w3_gp.py, 121 rows for the g
80c53590 t ORACLE-W3-EVENTS: wave3_events_oracle_test, 44 exe-oracle rows for the uncovered
717b7d83 c CTaskComplexKillCriminal ctor 0x68BE70: drop the criminal for every mission ped 
b9b3d0c9 c CEventHandler::ComputeInteriorUseInfoResponse 0x4BAFE0: build a new CTaskInterio
0d0c9cd2 c CEventHandler::ComputeGotKnockedOverByCarResponse 0x4C3430: flee arguments, case
2f3d98cf c CEventHandler::ComputeVehicleToStealResponse 0x4B9F80: steal only when the ped i
e193451b c CEventHandler::ComputeCopCarBeingStolenResponse 0x4BB740: wanted level of the hi
e1782f2a c CEventKnockOffBike::From 0x4AFDD0: do not copy m_forceKnockOff
db88ccba c CEventPlayerCommandToGroup::AffectsPedGroup 0x4B24D0: a leaderless / non-player-
276117e9 c CEventGroup::Reorganise 0x4AB700: clear the moved slots like the exe
35a2ee6d c CAEPedSpeechAudioEntity::GetSpecificSpeechContext 0x4E4470: accept ped type SPC 
5e9b92a0 c CWanted::SetWantedLevel 0x562470 / SetWantedLevelNoDrop 0x562570: drop the port-
f76106c4 c CPlayerPed::CanIKReachThisTarget 0x609F80: unordered (NaN) compare is true
c13d122a t W3-PED-OPEN: speech + CWanted fixture, 6 un-skipped rows, hand rows 0x5E4760 / 0
6a871322 c CPed::SetAmmo 0x5DF290: clip is reduced to the total (min), signed compares
df154b98 c CPed::GrantAmmo 0x5DF220: signed sum compare
0eb54c40 c CPlayerPed::FindPedToAttack 0x60C5F0: loop over every ped pool slot, last to fir
3c6bcb8c c CPlayerPed::AnnoyPlayerPed 0x60A040: temper compared as a signed char
dad2a8c0 t ORACLE-W3-PED: wave3_ped_oracle_test, 64 rows for the A/B functions of game_sa/E
5136ffed c CBmx::LaunchBunnyHopCB 0x6C0390: keep the force scalars in registers like the ex
c239b335 c CAutomobile::DoSoftGroundResistance 0x6A4AF0: mirror the exe's arithmetic and un
dc2b3566 c CBike::DoSoftGroundResistance 0x6B6D40: keep upSpeed / vx / vy / fwdSpeed in reg
2f3338d1 c CVehicle::CanVehicleBeDamaged 0x6D1280: a NaN health counts as damageable
f4e8b125 c CPhysical::CanPhysicalBeDamaged 0x5448B0: molotov also checks explosion proof; w
d565702b c CVehicle::AddPassenger(ped, seat) 0x6D14D0: apply the entry turn force before th
7cf90e2d c CVehicle::GetPlaneGunsPosition 0x6D4290: the Rustler gun offset also applies to 
f6440ee1 t ORACLE-W3-VEH: wave3_vehicle_oracle_test, exe-oracle rows for game_sa/Entity/Veh
a2fcc0de c CAutomobile::FindWheelWidth 0x6A6090: rear wheels use the rear flag table, kart 
6b70b13c c CVehicle::ActivateBombWhenEntered 0x6D2570: register the bomber reference only w
c591995f c CVehicle::ProcessCarAlarm 0x6D21F0: subtract the time step from the alarm timer
f93c9ef3 c CVehicle winch rope accessors 0x6D3CB0/0x6D3CD0/0x6D3CF0/0x6D3D10/0x6D3D30: look
8f2ff1ff c CTaskSimpleDuck::ControlDuckMove 0x6923F0: step logic with the exe's unordered/e
a1bb0c1f c CTaskComplexFacial::CreateNextSubTask 0x690DC0: consume m_RequestB before the al
