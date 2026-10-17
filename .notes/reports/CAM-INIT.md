# CAM-INIT — CCamera::Init ported store-for-store from the exe (0x5BC520), 2026-10-17

Commit bc6b434f. (Report saved by the orchestrator from the agent's hand-back; the agent's Write was refused.)

* `CCamera::Init` now follows the exe order: `rep stosd` over this+0x18..0xD78, `m_pRwCamera = null`, InitialiseScriptableComponents,
  3x CCam::Init, 124 member stores (many `= 0` after the CCam::Init calls), ClearPlayerWeaponMode, `m_mCameraMatrix.SetScale(1.0f)`,
  SetMotionBlur(255,255,255,0,0), target entity via FindPlayerVehicle(-1,false) / Players[PlayerInFocus].m_pPed + RegisterReference,
  the m_bStartGameLoading branches, 6 global stores. D-C34's CCam::Init / member-initialiser fixes kept; notsa::IsFixBugs() mouse-accel
  variant kept (FIX_BUGS off in the run build -> exe constant 0x3AC49BA6).
* Behaviour change: fields Init never touched are now set (m_bJustInitialized=1, m_bLookingAtPlayer=1, m_nLongestTimeInMill=5000,
  m_nCarZoom/m_nPedZoom=2, two-player modes, m_fFovForTrain=70, m_fNearClipScript=0.9, m_fPlayerExhaustion=1, m_nModeToGoTo=4, identity
  camera matrix). Between e65fc32e (D-C34 removed member initialisers) and bc6b434f these were 0 after boot. Re-Init now clears all state.
* Oracle: `tests/standalone/gameplay_oracle_camera.inc` case `CCamera::Init 0x5BC520`: exe vs port on poisoned TheCamera, all 3448 bytes
  + 5 global ranges + recorded FindPlayerVehicle/RegisterReference calls; 4000 cases, 0 mismatches at PC24 and PC53; mutation caught.
  Masked: 0x8CC38A..B (upper half of the exe's dword store to the port's 16-bit gCameraMode, never read); 0xB6EC18 only with FIX_BUGS.
* source/CMakeLists.txt: game_sa/IdleCam.cpp added to gameplay_oracle_test (needed by CCam::Init -> gIdleCam.Reset).
* Tests (StandaloneRW): gameplay 312, world 116, review 42, x87 67 checks — 0 failures. Full suite not run.
  Release build OK (private worktree); soak 3 min RESULT OK, 21 markers, 9800 frames, 0 errors, ~25 fps.

## Asm-vs-source table
| 5bc53e | `[esi + 0x954], ebx` | 0x954 m_pRwCamera | 229 | `m_pRwCamera = nullptr;` |
| 5bc54f | `[esi + 0x27], bl` | 0x27 m_bPlayerWasOnBike | 231 | `m_bPlayerWasOnBike = false;` |
| 5bc552 | `[esi + 0x3e], bl` | 0x3e m_b1rstPersonRunCloseToAWall | 232 | `m_b1rstPersonRunCloseToAWall = false;` |
| 5bc555 | `[esi + 0x140], ebx` | 0x140 m_fPositionAlongSpline | 233 | `m_fPositionAlongSpline = 0.0f;` |
| 5bc55b | `[esi + 0x1c], bl` | 0x1c m_bCameraJustRestored | 234 | `m_bCameraJustRestored = false;` |
| 5bc58a | `[esi + 0x180], ax` | m_aCams[0]+0xc eCamMode m_nMode | 243 | `cam0.m_nMode = MODE_FOLLOWPED;` |
| 5bc591 | `[esi + 0x3b8], ax` | m_aCams[1]+0xc eCamMode m_nMode | 244 | `cam1.m_nMode = MODE_FOLLOWPED;` |
| 5bc59d | `fstp [esi + 0x274]` | m_aCams[0]+0x100 m_fTargetCloseInDist | 245 | `cam0.m_fTargetCloseInDist = 2.0837801f - 1.85f; // x87: fld [0x86A46C] ; fsub [0x86A468]` |
| 5bc5a5 | `[esi + 0x41], bl` | 0x41 m_bEnable1rstPersonCamCntrlsScript | 246 | `m_bEnable1rstPersonCamCntrlsScript = false;` |
| 5bc5a8 | `[esi + 0x42], bl` | 0x42 m_bAllow1rstPersonWeaponsCamera | 247 | `m_bAllow1rstPersonWeaponsCamera = false;` |
| 5bc5ab | `[esi + 0x43], bl` | 0x43 m_bCooperativeCamMode | 248 | `m_bCooperativeCamMode = false;` |
| 5bc5ae | `[esi + 0x44], 1` | 0x44 m_bAllowShootingWith2PlayersInCar | 249 | `m_bAllowShootingWith2PlayersInCar = true;` |
| 5bc5b2 | `[esi + 0x40], bl` | 0x40 m_bVehicleSuspenHigh | 250 | `m_bVehicleSuspenHigh = false;` |
| 5bc5b5 | `[esi + 0x270], eax` | m_aCams[0]+0xfc m_fMinRealGroundDist | 251 | `cam0.m_fMinRealGroundDist = 1.85f;` |
| 5bc5bb | `[esi + 0x250], 0xbf0ccccd` | m_aCams[0]+0xdc m_fTargetZoomGroundOne | 252 | `cam0.m_fTargetZoomGroundOne = -0.55f;` |
| 5bc5c5 | `[esi + 0x254], 0x3fc00000` | m_aCams[0]+0xe0 m_fTargetZoomGroundTwo | 253 | `cam0.m_fTargetZoomGroundTwo = 1.5f;` |
| 5bc5cf | `[esi + 0x258], 0x40666666` | m_aCams[0]+0xe4 m_fTargetZoomGroundThree | 254 | `cam0.m_fTargetZoomGroundThree = 3.6f;` |
| 5bc5d9 | `[esi + 0x25c], 0x3d75c28f` | m_aCams[0]+0xe8 m_fTargetZoomOneZExtra | 255 | `cam0.m_fTargetZoomOneZExtra = 0.06f;` |
| 5bc5e3 | `[esi + 0x260], 0xbdcccccd` | m_aCams[0]+0xec m_fTargetZoomTwoZExtra | 256 | `cam0.m_fTargetZoomTwoZExtra = -0.1f;` |
| 5bc5ed | `[esi + 0x264], ebx` | m_aCams[0]+0xf0 m_fTargetZoomTwoInteriorZExtra | 257 | `cam0.m_fTargetZoomTwoInteriorZExtra = 0.0f;` |
| 5bc5f3 | `[esi + 0x268], 0xbd8f5c29` | m_aCams[0]+0xf4 m_fTargetZoomThreeZExtra | 258 | `cam0.m_fTargetZoomThreeZExtra = -0.07f;` |
| 5bc5fd | `[esi + 0x26c], 0x3f668113` | m_aCams[0]+0xf8 m_fTargetZoomZCloseIn | 259 | `cam0.m_fTargetZoomZCloseIn = 0.90040702f;` |
| 5bc607 | `[esi + 0x2d], bl` | 0x2d m_bMoveCamToAvoidGeom | 260 | `m_bMoveCamToAvoidGeom = false;` |
| 5bc60f | `[esi + 0x22], bl` | 0x22 m_bInATunnelAndABigVehicle | 269 | `m_bInATunnelAndABigVehicle = false;` |
| 5bc612 | `[esi + 0x390], ebx` | m_aCams[0]+0x21c m_pCamTargetEntity | 270 | `cam0.m_pCamTargetEntity = nullptr;` |
| 5bc618 | `[esi + 0x5c8], ebx` | m_aCams[1]+0x21c m_pCamTargetEntity | 271 | `cam1.m_pCamTargetEntity = nullptr;` |
| 5bc61e | `[esi + 0x800], ebx` | m_aCams[2]+0x21c m_pCamTargetEntity | 272 | `cam2.m_pCamTargetEntity = nullptr;` |
| 5bc624 | `[esi + 0x1c8], ebx` | m_aCams[0]+0x54 m_fCamBufferedHeight | 273 | `cam0.m_fCamBufferedHeight = 0.0f;` |
| 5bc62a | `[esi + 0x1cc], ebx` | m_aCams[0]+0x58 m_fCamBufferedHeightSpeed | 274 | `cam0.m_fCamBufferedHeightSpeed = 0.0f;` |
| 5bc630 | `[esi + 0x400], ebx` | m_aCams[1]+0x54 m_fCamBufferedHeight | 275 | `cam1.m_fCamBufferedHeight = 0.0f;` |
| 5bc636 | `[esi + 0x404], ebx` | m_aCams[1]+0x58 m_fCamBufferedHeightSpeed | 276 | `cam1.m_fCamBufferedHeightSpeed = 0.0f;` |
| 5bc63c | `[esi + 0x176], bl` | m_aCams[0]+0x2 m_bCamLookingAtVector | 277 | `cam0.m_bCamLookingAtVector = false;` |
| 5bc642 | `[esi + 0x3ae], bl` | m_aCams[1]+0x2 m_bCamLookingAtVector | 278 | `cam1.m_bCamLookingAtVector = false;` |
| 5bc648 | `[esi + 0x5e6], bl` | m_aCams[2]+0x2 m_bCamLookingAtVector | 279 | `cam2.m_bCamLookingAtVector = false;` |
| 5bc64e | `[esi + 0x39c], ebx` | m_aCams[0]+0x228 m_fPlayerVelocity | 280 | `cam0.m_fPlayerVelocity = 0.0f;` |
| 5bc654 | `[esi + 0x5d4], ebx` | m_aCams[1]+0x228 m_fPlayerVelocity | 281 | `cam1.m_fPlayerVelocity = 0.0f;` |
| 5bc65a | `[esi + 0x80c], ebx` | m_aCams[2]+0x228 m_fPlayerVelocity | 282 | `cam2.m_fPlayerVelocity = 0.0f;` |
| 5bc660 | `[esi + 0x3f], bl` | 0x3f m_bHeadBob | 285 | `m_bHeadBob = false;` |
| 5bc663 | `[esi + 0xc10], 0x3e800000` | 0xc10 m_fFractionInterToStopMoving | 286 | `m_fFractionInterToStopMoving = 0.25f;` |
| 5bc66d | `[esi + 0xc14], 0x3f400000` | 0xc14 m_fFractionInterToStopCatchUp | 287 | `m_fFractionInterToStopCatchUp = 0.75f;` |
| 5bc677 | `[esi + 0xc20], 0x3f59999a` | 0xc20 m_fGaitSwayBuffer | 288 | `m_fGaitSwayBuffer = 0.85f;` |
| 5bc681 | `[esi + 0x50], bl` | 0x50 m_bScriptParametersSetForInterp | 289 | `m_bScriptParametersSetForInterp = false;` |
| 5bc684 | `[esi + 0x5c], ebx` | 0x5c m_nCamShakeStart | 290 | `m_nCamShakeStart = 0;` |
| 5bc687 | `[esi + 0x12c], ebx` | 0x12c m_fCamShakeForce | 291 | `m_fCamShakeForce = 0.0f;` |
| 5bc68d | `[esi + 0xc34], 0x1e` | 0xc34 m_nModeObbeCamIsInForCar | 292 | `m_nModeObbeCamIsInForCar = 30;` |
| 5bc697 | `[esi + 0x25], bl` | 0x25 m_bIgnoreFadingStuffForMusic | 293 | `m_bIgnoreFadingStuffForMusic = false;` |
| 5bc69a | `[esi + 0x3a], bl` | 0x3a m_bWaitForInterpolToFinish | 294 | `m_bWaitForInterpolToFinish = false;` |
| 5bc69d | `[esi + 0x81c], ebx` | 0x81c m_pToGarageWeAreIn | 295 | `m_pToGarageWeAreIn = nullptr;` |
| 5bc6a3 | `[esi + 0x820], ebx` | 0x820 m_pToGarageWeAreInForHackAvoidFirstPerson | 296 | `m_pToGarageWeAreInForHackAvoidFirstPerson = nullptr;` |
| 5bc6a9 | `[esi + 0x26], bl` | 0x26 m_bPlayerIsInGarage | 297 | `m_bPlayerIsInGarage = false;` |
| 5bc6ac | `[esi + 0x28], bl` | 0x28 m_bJustCameOutOfGarage | 298 | `m_bJustCameOutOfGarage = false;` |
| 5bc6af | `[esi + 0x138], 0x3f666666` | 0x138 m_fNearClipScript | 299 | `m_fNearClipScript = 0.9f;` |
| 5bc6b9 | `[esi + 0x31], bl` | 0x31 m_bUseNearClipScript | 300 | `m_bUseNearClipScript = false;` |
| 5bc6bc | `[esi + 0x4f], bl` | 0x4f m_bDoingSpecialInterp | 301 | `m_bDoingSpecialInterp = false;` |
| 5bc6bf | `[ebp], bl` | 0x18 m_bAboveGroundTrainNodesLoaded | 302 | `m_bAboveGroundTrainNodesLoaded = false;` |
| 5bc6d0 | `[esi + 0x19], bl` | 0x19 m_bBelowGroundTrainNodesLoaded | 303 | `m_bBelowGroundTrainNodesLoaded = false;` |
| 5bc6d3 | `[esi + 0x46], 0x32` | 0x46 eCamMode m_nModeForTwoPlayersSeparateCars | 304 | `m_nModeForTwoPlayersSeparateCars = MODE_TWOPLAYER_SEPARATE_CARS;` |
| 5bc6d9 | `[esi + 0x48], 0x31` | 0x48 eCamMode m_nModeForTwoPlayersSameCarShootingAllowed | 305 | `m_nModeForTwoPlayersSameCarShootingAllowed = MODE_TWOPLAYER_IN_CAR_AND_SHOOTING;` |
| 5bc6df | `[esi + 0x4a], 3` | 0x4a eCamMode m_nModeForTwoPlayersSameCarShootingNotAllowed | 306 | `m_nModeForTwoPlayersSameCarShootingNotAllowed = MODE_BEHINDCAR;` |
| 5bc6e5 | `[esi + 0x4c], 0x30` | 0x4c eCamMode m_nModeForTwoPlayersNotBothInCar | 307 | `m_nModeForTwoPlayersNotBothInCar = MODE_TWOPLAYER;` |
| 5bc6eb | `[esi + 0x3d], bl` | 0x3d m_bWideScreenOn | 308 | `m_bWideScreenOn = false;` |
| 5bc6ee | `[esi + 0x134], ebx` | 0x134 m_fFOV_Wide_Screen | 309 | `m_fFOV_Wide_Screen = 0.0f;` |
| 5bc6f4 | `[esi + 0x30], bl` | 0x30 m_bRestoreByJumpCut | 310 | `m_bRestoreByJumpCut = false;` |
| 5bc6f7 | `[esi + 0xb4], eax` | 0xb4 m_nCarZoom | 311 | `m_nCarZoom = 2;` |
| 5bc6fd | `[esi + 0xc8], eax` | 0xc8 m_nPedZoom | 312 | `m_nPedZoom = 2;` |
| 5bc703 | `[esi + 0xc0], ebx` | 0xc0 m_fCarZoomSmoothed | 313 | `m_fCarZoomSmoothed = 0.0f;` |
| 5bc709 | `[esi + 0xbc], ebx` | 0xbc m_fCarZoomTotal | 314 | `m_fCarZoomTotal = 0.0f;` |
| 5bc70f | `[esi + 0xb8], ebx` | 0xb8 m_fCarZoomBase | 315 | `m_fCarZoomBase = 0.0f;` |
| 5bc715 | `[esi + 0xd4], ebx` | 0xd4 m_fPedZoomSmoothed | 316 | `m_fPedZoomSmoothed = 0.0f;` |
| 5bc71b | `[esi + 0xd0], ebx` | 0xd0 m_fPedZoomTotal | 317 | `m_fPedZoomTotal = 0.0f;` |
| 5bc721 | `[esi + 0xcc], ebx` | 0xcc m_fPedZoomBase | 318 | `m_fPedZoomBase = 0.0f;` |
| 5bc76a | `[esi + 0x23], bl` | 0x23 m_bInitialNodeFound | 329 | `m_bInitialNodeFound = false;` |
| 5bc76d | `[esi + 0x144], ebx` | 0x144 m_fScreenReductionPercentage | 330 | `m_fScreenReductionPercentage = 0.0f;` |
| 5bc773 | `[esi + 0x148], ebx` | 0x148 m_fScreenReductionSpeed | 331 | `m_fScreenReductionSpeed = 0.0f;` |
| 5bc779 | `[esi + 0x3d], bl` | 0x3d m_bWideScreenOn | 332 | `m_bWideScreenOn = false;` |
| 5bc77c | `[esi + 0x3c], bl` | 0x3c m_bWantsToSwitchWidescreenOff | 333 | `m_bWantsToSwitchWidescreenOff = false;` |
| 5bc77f | `[esi + 0x56], bl` | 0x56 m_bWorldViewerBeingUsed | 334 | `m_bWorldViewerBeingUsed = false;` |
| 5bc782 | `[esi + 0x154], ebp` | 0x154 m_fPlayerExhaustion | 335 | `m_fPlayerExhaustion = 1.0f;` |
| 5bc788 | `[esi + 0x120], ebx` | 0x120 m_fPedOrientForBehindOrInFront | 336 | `m_fPedOrientForBehindOrInFront = 0.0f;` |
| 5bc796 | `[esi + 0x51], bl` | 0x51 m_bFading | 339 | `m_bFading = false;` |
| 5bc79f | `[esi + 0xbfc], ebx` | 0xbfc m_fFadeAlpha | 341 | `m_fFadeAlpha = 0.0f;` |
| 5bc7a5 | `[esi + 0x52], bl` | 0x52 m_bMusicFading | 342 | `m_bMusicFading = false;` |
| 5bc7a8 | `[esi + 0xc08], ebx` | 0xc08 m_fTimeToFadeMusic | 343 | `m_fTimeToFadeMusic = 0.0f;` |
| 5bc7ae | `[esi + 0xc00], ebx` | 0xc00 m_fEffectsFaderScalingFactor | 344 | `m_fEffectsFaderScalingFactor = 0.0f;` |
| 5bc7c7 | `[esi + 0xc08], ebx` | 0xc08 m_fTimeToFadeMusic | 348 | `m_fTimeToFadeMusic = 0.0f;` |
| 5bc7d9 | `[esi + 0x33], bl` | 0x33 m_bStartingSpline | 351 | `m_bStartingSpline = false;` |
| 5bc7dc | `[esi + 0xc3c], 1` | 0xc3c eSwitchType m_nTypeOfSwitch | 352 | `m_nTypeOfSwitch = eSwitchType::INTERPOLATION;` |
| 5bc7e5 | `[esi + 0x38], bl` | 0x38 m_bUseScriptZoomValuePed | 353 | `m_bUseScriptZoomValuePed = false;` |
| 5bc7e8 | `[esi + 0x39], bl` | 0x39 m_bUseScriptZoomValueCar | 354 | `m_bUseScriptZoomValueCar = false;` |
| 5bc7eb | `[esi + 0xd8], ebx` | 0xd8 m_fPedZoomValueScript | 355 | `m_fPedZoomValueScript = 0.0f;` |
| 5bc7f1 | `[esi + 0xc4], ebx` | 0xc4 m_fCarZoomValueScript | 356 | `m_fCarZoomValueScript = 0.0f;` |
| 5bc7f7 | `[esi + 0x36], bl` | 0x36 m_bUseSpecialFovTrain | 357 | `m_bUseSpecialFovTrain = false;` |
| 5bc7fa | `[esi + 0x130], 0x428c0000` | 0x130 m_fFovForTrain | 358 | `m_fFovForTrain = 70.0f;` |
| 5bc804 | `[esi + 0xc38], di` | 0xc38 eCamMode m_nModeToGoTo | 359 | `m_nModeToGoTo = MODE_FOLLOWPED;` |
| 5bc80b | `[esi + 0x2a], bl` | 0x2a m_bJust_Switched | 360 | `m_bJust_Switched = false;` |
| 5bc80e | `[esi + 0x37], bl` | 0x37 m_bUseTransitionBeta | 361 | `m_bUseTransitionBeta = false;` |
| 5bc816 | `[esi + 0x34], bl` | 0x34 m_bTargetJustBeenOnTrain | 363 | `m_bTargetJustBeenOnTrain = false;` |
| 5bc819 | `[esi + 0x24], bl` | 0x24 m_bInitialNoNodeStaticsSet | 364 | `m_bInitialNoNodeStaticsSet = false;` |
| 5bc81c | `[esi + 0x64], 0x1388` | 0x64 m_nLongestTimeInMill | 365 | `m_nLongestTimeInMill = 5000;` |
| 5bc823 | `[esi + 0x6c], ebx` | 0x6c m_nTimeLastChange | 366 | `m_nTimeLastChange = 0;` |
| 5bc826 | `[esi + 0x21], bl` | 0x21 m_bIdleOn | 367 | `m_bIdleOn = false;` |
| 5bc82a | `[esi + 0x70], ebx` | 0x70 m_nTimeWeLeftIdle_StillNoInput | 368 | `m_nTimeWeLeftIdle_StillNoInput = 0;` |
| 5bc82d | `[esi + 0x74], ebx` | 0x74 m_nTimeWeEnteredIdle | 369 | `m_nTimeWeEnteredIdle = 0;` |
| 5bc831 | `[esi + 0xf0], ebp` | 0xf0 m_fLODDistMultiplier | 370 | `m_fLODDistMultiplier = 1.0f;` |
| 5bc83c | `[esi + 0x1a], bl` | 0x1a m_bCamDirectlyBehind | 371 | `m_bCamDirectlyBehind = false;` |
| 5bc83f | `[esi + 0x1b], bl` | 0x1b m_bCamDirectlyInFront | 372 | `m_bCamDirectlyInFront = false;` |
| 5bc847 | `[esi + 0xa0], ebx` | 0xa0 m_nMotionBlur | 373 | `m_nMotionBlur = 0;` |
| 5bc854 | `[esi + 0x4e], bl` | 0x4e m_bGarageFixedCamPositionSet | 374 | `m_bGarageFixedCamPositionSet = false;` |
| 5bc85c | `[esi + 0x1e], bl` | 0x1e m_bCullZoneChecksOn | 376 | `m_bCullZoneChecksOn = false;` |
| 5bc85f | `[esi + 0x54], bl` | 0x54 m_bFailedCullZoneTestPreviously | 377 | `m_bFailedCullZoneTestPreviously = false;` |
| 5bc862 | `[esi + 0xa8], 6` | 0xa8 m_nCheckCullZoneThisNumFrames | 378 | `m_nCheckCullZoneThisNumFrames = 6;` |
| 5bc86c | `[esi + 0xac], ebx` | 0xac m_nZoneCullFrameNumWereAt | 379 | `m_nZoneCullFrameNumWereAt = 0;` |
| 5bc872 | `[esi + 0x124], ebx` | 0x124 m_fCameraAverageSpeed | 380 | `m_fCameraAverageSpeed = 0.0f;` |
| 5bc878 | `[esi + 0x128], ebx` | 0x128 m_fCameraSpeedSoFar | 381 | `m_fCameraSpeedSoFar = 0.0f;` |
| 5bc8b0 | `[esi + 0x94], edi` | 0x94 m_nWorkOutSpeedThisNumFrames | 383 | `m_nWorkOutSpeedThisNumFrames = 4;` |
| 5bc8b6 | `[esi + 0x98], ebx` | 0x98 m_nNumFramesSoFar | 384 | `m_nNumFramesSoFar = 0;` |
| 5bc8bc | `[esi + 0x29], 1` | 0x29 m_bJustInitialized | 385 | `m_bJustInitialized = true;` |
| 5bc8c0 | `[esi + 0x58], bl` | 0x58 m_bTransitionState | 386 | `m_bTransitionState = false;` |
| 5bc8c3 | `[esi + 0x78], ebx` | 0x78 m_nTimeTransitionStart | 387 | `m_nTimeTransitionStart = 0;` |
| 5bc8c6 | `[esi + 0x2b], 1` | 0x2b m_bLookingAtPlayer | 388 | `m_bLookingAtPlayer = true;` |
| 5bc8ca | `[esi + 0x2c], bl` | 0x2c m_bLookingAtVector | 389 | `m_bLookingAtVector = false;` |
| 5bc8e1 | `[esi + 0x164], ebx` | 0x164 m_fAvoidTheGeometryProbsTimer | 392 | `m_fAvoidTheGeometryProbsTimer = 0.0f;` |
| 5bc8e7 | `[esi + 0x168], bx` | 0x168 m_nAvoidTheGeometryProbsDirn | 393 | `m_nAvoidTheGeometryProbsDirn = 0;` |
| 5bc8f6 | `[esi + 0x971], 1` | 0x971 m_bResetOldMatrix | 395 | `m_bResetOldMatrix = true;` |