/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include <optional>
#include <functional>
#include <extensions/utility.hpp>
#include <reversiblebugfixes/Bugs.hpp>

#include "Vehicle.h"
#include "Garages.h"
#include "ModelIndices.h"
#include "CustomCarPlateMgr.h"
#include "Buoyancy.h"
#include "CarCtrl.h"
#include "VehicleSaveStructure.h"
#include "Radar.h"
#include "RideAnims.h"
#include "Rope.h"
#include "Ropes.h"
#include "Ragdoll/IKChainManager.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "Shadows.h"
#include "PedClothesDesc.h"
#include "Skidmarks.h"

static CVector TransformPointExt(const CMatrix& m, const CVector& v); // defined below

auto& planeRotorDmgTimeMS = StaticRef<uint32>(0xC1CC1C);

auto& fBurstTyreMod = StaticRef<float>(0x8D34B4);                // 0.13f
auto& fBurstSpeedMax = StaticRef<float>(0x8D34B8);               // 0.3f
auto& CAR_NOS_EXTRA_SKID_LOSS = StaticRef<float>(0x8D34BC);      // 0.9f
auto& WS_TRAC_FRAC_LIMIT = StaticRef<float>(0x8D34C0);           // 0.3f
auto& WS_ALREADY_SPINNING_LOSS = StaticRef<float>(0x8D34C4);     // 0.2f
auto& fBurstBikeTyreMod = StaticRef<float>(0x8D34C8);            // 0.05f
auto& fBurstBikeSpeedMax = StaticRef<float>(0x8D34CC);           // 0.12f
auto& fTweakBikeWheelTurnForce = StaticRef<float>(0x8D34D0);     // 2.0f
auto& AUTOGYRO_ROTORSPIN_MULT = StaticRef<float>(0x8D34D4);      // 0.006f
auto& AUTOGYRO_ROTORSPIN_MULTLIMIT = StaticRef<float>(0x8D34D8); // 0.25f
auto& AUTOGYRO_ROTORSPIN_DAMP = StaticRef<float>(0x8D34DC);      // 0.997f
auto& AUTOGYRO_ROTORLIFT_MULT = StaticRef<float>(0x8D34E0);      // 4.5f
auto& AUTOGYRO_ROTORLIFT_FALLOFF = StaticRef<float>(0x8D34E4);   // 0.75f
auto& AUTOGYRO_ROTORTILT_ANGLE = StaticRef<float>(0x8D34E8);     // 0.25f
auto& ROTOR_SEMI_THICKNESS = StaticRef<float>(0x8D34EC);         // 0.05f
float* gfSpeedMult = (float*)0x8D34F8;                   // float fSpeedMult[5] = { 0.8f, 0.75f, 0.85f, 0.9f, 0.85f, 0.85f }
auto& fDamagePosSpeedShift = StaticRef<float>(0x8D3510);         // 0.4f
auto& DIFF_LIMIT = StaticRef<float>(0x8D35B4);                   // 0.8f
auto& DIFF_SPRING_MULT_X = StaticRef<float>(0x8D35B8);           // 0.05f
auto& DIFF_SPRING_MULT_Y = StaticRef<float>(0x8D35BC);           // 0.05f
auto& DIFF_SPRING_MULT_Z = StaticRef<float>(0x8D35C0);           // 0.1f
auto& DIFF_SPRING_COMPRESS_MULT = StaticRef<float>(0x8D35C4);    // 2.0f
auto& VehicleGunOffset = StaticRef<std::array<CVector, 14>>(0x8D35D4); // maybe [12]

void CVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CVehicle, 0x871e80, 66);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6D5F10);
    RH_ScopedInstall(Destructor, 0x6E2B40);

    RH_ScopedVMTInstall(SetModelIndex, 0x6D6A40);
    RH_ScopedVMTInstall(DeleteRwObject, 0x6D6410);
    RH_ScopedVMTInstall(SpecialEntityPreCollisionStuff, 0x6D6640);
    RH_ScopedVMTInstall(SpecialEntityCalcCollisionSteps, 0x6D0E90);
    RH_ScopedVMTInstall(SetupLighting, 0x553F20);
    RH_ScopedVMTInstall(RemoveLighting, 0x5533D0);
    RH_ScopedVMTInstall(PreRender, 0x6D6480);
    RH_ScopedVMTInstall(Render, 0x6D0E60);
    RH_ScopedVMTInstall(ProcessOpenDoor, 0x6D56C0);
    RH_ScopedVMTInstall(GetHeightAboveRoad, 0x6D63F0);
    RH_ScopedVMTInstall(CanPedStepOutCar, 0x6D1F30);
    RH_ScopedVMTInstall(CanPedJumpOutCar, 0x6D2030);
    RH_ScopedVMTInstall(GetTowHitchPos, 0x6DFB70);
    RH_ScopedVMTInstall(GetTowBarPos, 0x6DFBE0);
    RH_ScopedVMTInstall(Save, 0x5D4760, {.State = HS::RedirectToGTA });
    RH_ScopedVMTInstall(Load, 0x5D2900, {.State = HS::RedirectToGTA });

    // It can't be properly unhooked, original function assumes that CVehicle::GetVehicleAppearance doesn't spoil ECX register, and calls
    // it without making sure that the pointer in it still points to current instance. While it worked for original function, we can't
    // force the compiler to keep ECX unchanged through function execution
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6DF4A0, { .Locked = true });

    RH_ScopedOverloadedInstall(IsPassenger, "Ped", 0x6D1BD0, bool(CVehicle::*)(CPed*) const);
    RH_ScopedOverloadedInstall(IsPassenger, "ModelID", 0x6D1C00, bool(CVehicle::*)(int32) const);
    RH_ScopedOverloadedInstall(IsDriver, "Ped", 0x6D1C40, bool(CVehicle::*)(const CPed*) const);
    RH_ScopedOverloadedInstall(IsDriver, "ModelID", 0x6D1C60, bool(CVehicle::*)(int32) const);
    RH_ScopedInstall(Shutdown, 0x6D0B40);
    RH_ScopedInstall(GetRemapIndex, 0x6D0B70);
    RH_ScopedInstall(SetRemap, 0x6D0C00);
    RH_ScopedInstall(SetCollisionLighting, 0x6D0CA0);
    RH_ScopedInstall(UpdateLightingFromStoredPolys, 0x6D0CC0);
    RH_ScopedInstall(CalculateLightingFromCollision, 0x6D0CF0);
    RH_ScopedInstall(ResetAfterRender, 0x6D0E20);
    RH_ScopedInstall(ProcessWheel, 0x6D6C00);
    RH_ScopedInstall(ApplyBoatWaterResistance, 0x6D2740);
    RH_ScopedInstall(ProcessBoatControl, 0x6DBCE0);
    RH_ScopedInstall(ChangeLawEnforcerState, 0x6D2330);
    RH_ScopedInstall(GetVehicleAppearance, 0x6D1080);
    RH_ScopedInstall(DoHeadLightBeam, 0x6E0E20);
    RH_ScopedInstall(GetPlaneNumGuns, 0x6D3F30); // ??: register problem?

    RH_ScopedInstall(CustomCarPlate_TextureCreate, 0x6D10E0);
    RH_ScopedInstall(CustomCarPlate_TextureDestroy, 0x6D1150);
    RH_ScopedInstall(CanBeDeleted, 0x6D1180);
    RH_ScopedInstall(ProcessWheelRotation, 0x6D1230);
    RH_ScopedInstall(CanVehicleBeDamaged, 0x6D1280);
    RH_ScopedInstall(ProcessDelayedExplosion, 0x6D1340);
    RH_ScopedOverloadedInstall(AddPassenger, "Auto-Seat", 0x6D13A0, bool(CVehicle::*)(CPed*));
    RH_ScopedOverloadedInstall(AddPassenger, "Fixed-Seat", 0x6D14D0, bool(CVehicle::*)(CPed*, uint8));
    RH_ScopedInstall(RemovePassenger, 0x6D1610);
    RH_ScopedInstall(SetDriver, 0x6D16A0);
    RH_ScopedInstall(RemoveDriver, 0x6D1950);
    RH_ScopedInstall(SetUpDriver, 0x6D1A50);
    RH_ScopedInstall(SetupPassenger, 0x6D1AA0);
    RH_ScopedInstall(KillPedsInVehicle, 0x6D1C80);
    RH_ScopedInstall(IsUpsideDown, 0x6D1D90);
    RH_ScopedInstall(IsOnItsSide, 0x6D1DD0);
    RH_ScopedInstall(CanPedOpenLocks, 0x6D1E20);
    RH_ScopedInstall(CanDoorsBeDamaged, 0x6D1E60);
    RH_ScopedInstall(CanPedEnterCar, 0x6D1E80);
    RH_ScopedInstall(ProcessCarAlarm, 0x6D21F0);
    RH_ScopedInstall(IsVehicleNormal, 0x6D22F0);
    RH_ScopedInstall(IsLawEnforcementVehicle, 0x6D2370);
    RH_ScopedInstall(ExtinguishCarFire, 0x6D2460);
    RH_ScopedInstall(ActivateBomb, 0x6D24F0);
    RH_ScopedInstall(ActivateBombWhenEntered, 0x6D2570);
    RH_ScopedInstall(CarHasRoof, 0x6D25D0);
    RH_ScopedInstall(HeightAboveCeiling, 0x6D2600);
    RH_ScopedInstall(SetComponentVisibility, 0x6D2700);
    RH_ScopedInstall(SetComponentAtomicAlpha, 0x6D2960);
    RH_ScopedInstall(UpdateClumpAlpha, 0x6D2980);
    RH_ScopedInstall(UpdatePassengerList, 0x6D29E0);
    RH_ScopedInstall(PickRandomPassenger, 0x6D2A10);
    RH_ScopedInstall(AddDamagedVehicleParticles, 0x6D2A80);
    RH_ScopedInstall(MakeDirty, 0x6D2BF0);
    RH_ScopedInstall(AddWheelDirtAndWater, 0x6D2D50);
    RH_ScopedInstall(SetGettingInFlags, 0x6D3000);
    RH_ScopedInstall(SetGettingOutFlags, 0x6D3020);
    RH_ScopedInstall(ClearGettingInFlags, 0x6D3040);
    RH_ScopedInstall(ClearGettingOutFlags, 0x6D3060);
    RH_ScopedInstall(SetWindowOpenFlag, 0x6D3080);
    RH_ScopedInstall(ClearWindowOpenFlag, 0x6D30B0);
    RH_ScopedInstall(SetVehicleUpgradeFlags, 0x6D30E0);
    RH_ScopedInstall(ClearVehicleUpgradeFlags, 0x6D3210);
    RH_ScopedInstall(CreateUpgradeAtomic, 0x6D3510);
    RH_ScopedInstall(RemoveUpgrade, 0x6D3630);
    RH_ScopedInstall(GetUpgrade, 0x6D3650);
    RH_ScopedInstall(CreateReplacementAtomic, 0x6D3700);
    RH_ScopedInstall(AddReplacementUpgrade, 0x6D3830);
    RH_ScopedInstall(RemoveReplacementUpgrade, 0x6D39E0);
    RH_ScopedInstall(GetReplacementUpgrade, 0x6D3A50);
    RH_ScopedInstall(RemoveAllUpgrades, 0x6D3AB0);
    RH_ScopedInstall(GetSpareHasslePosId, 0x6D3AE0);
    RH_ScopedInstall(SetHasslePosId, 0x6D3B30);
    RH_ScopedInstall(InitWinch, 0x6D3B60);
    RH_ScopedInstall(UpdateWinch, 0x6D3B80);
    RH_ScopedInstall(RemoveWinch, 0x6D3C70);
    RH_ScopedInstall(RenderDriverAndPassengers, 0x6D3D60);
    RH_ScopedInstall(PreRenderDriverAndPassengers, 0x6D3DB0);
    RH_ScopedInstall(GetPlaneGunsAutoAimAngle, 0x6D3E00);
    RH_ScopedInstall(SetFiringRateMultiplier, 0x6D4010);
    RH_ScopedInstall(GetFiringRateMultiplier, 0x6D4090);
    RH_ScopedInstall(GetPlaneGunsRateOfFire, 0x6D40E0);
    RH_ScopedInstall(GetPlaneGunsPosition, 0x6D4290);
    RH_ScopedInstall(GetPlaneOrdnanceRateOfFire, 0x6D4590);
    RH_ScopedInstall(GetPlaneOrdnancePosition, 0x6D46E0);
    RH_ScopedInstall(SelectPlaneWeapon, 0x6D4900);
    RH_ScopedInstall(DoPlaneGunFireFX, 0x6D4AD0);
    RH_ScopedInstall(FirePlaneGuns, 0x6D4D30);
    RH_ScopedInstall(FireUnguidedMissile, 0x6D5110);
    RH_ScopedInstall(CanBeDriven, 0x6D5400);
    RH_ScopedInstall(ReactToVehicleDamage, 0x6D5490);
    RH_ScopedInstall(GetVehicleLightsStatus, 0x6D55C0);
    RH_ScopedInstall(CanPedLeanOut, 0x6D5CF0);
    RH_ScopedInstall(SetVehicleCreatedBy, 0x6D5D70);
    RH_ScopedInstall(SetupRender, 0x6D64F0);
    RH_ScopedInstall(ProcessBikeWheel, 0x6D73B0);
    RH_ScopedInstall(FindTyreNearestPoint, 0x6D7BC0);
    RH_ScopedInstall(InflictDamage, 0x6D7C90);
    RH_ScopedInstall(KillPedsGettingInVehicle, 0x6D82F0);
    RH_ScopedInstall(UsesSiren, 0x6D8470);
    RH_ScopedInstall(IsSphereTouchingVehicle, 0x6D84D0);
    // RH_ScopedInstall(FlyingControl, 0x6D85F0);
    RH_ScopedInstall(BladeColSectorList<CPtrListSingleLink<CEntity*>>, 0x6DAF00);
    RH_ScopedInstall(SetComponentRotation, 0x6DBA30);
    RH_ScopedInstall(SetTransmissionRotation, 0x6DBBB0);
    RH_ScopedInstall(DoBoatSplashes, 0x6DD130);
    RH_ScopedInstall(DoSunGlare, 0x6DD6F0);
    RH_ScopedInstall(AddWaterSplashParticles, 0x6DDF60);
    RH_ScopedInstall(AddExhaustParticles, 0x6DE240);
    RH_ScopedInstall(AddSingleWheelParticles, 0x6DE880);
    RH_ScopedInstall(GetSpecialColModel, 0x6DF3D0);
    // RH_ScopedInstall(RemoveVehicleUpgrade, 0x6DF930);
    // RH_ScopedInstall(AddUpgrade, 0x6DFA20);
    // RH_ScopedInstall(UpdateTrailerLink, 0x6DFC50);
    // RH_ScopedInstall(UpdateTractorLink, 0x6E0050);
    // RH_ScopedInstall(ScanAndMarkTargetForHeatSeekingMissile, 0x6E0400);
    // RH_ScopedInstall(FireHeatSeakingMissile, 0x6E05C0);
    // RH_ScopedInstall(PossiblyDropFreeFallBombForPlayer, 0x6E07E0);
    // RH_ScopedInstall(ProcessSirenAndHorn, 0x6E0950);
    RH_ScopedInstall(DoHeadLightEffect, 0x6E0A50);
    RH_ScopedInstall(DoHeadLightReflectionSingle, 0x6E1440);
    RH_ScopedInstall(DoHeadLightReflectionTwin, 0x6E1600);
    RH_ScopedInstall(DoHeadLightReflection, 0x6E1720);
    RH_ScopedInstall(DoTailLightEffect, 0x6E1780);
    RH_ScopedInstall(DoVehicleLights, 0x6E1A60);
    RH_ScopedInstall(FillVehicleWithPeds, 0x6E2900);
    RH_ScopedInstall(DoBladeCollision, 0x6E2E50);
    // RH_ScopedInstall(AddVehicleUpgrade, 0x6E3290);
    RH_ScopedInstall(SetupUpgradesAfterLoad, 0x6E3400);
    // RH_ScopedInstall(GetPlaneWeaponFiringStatus, 0x6E3440);
    // RH_ScopedInstall(ProcessWeapons, 0x6E3950);
    RH_ScopedInstall(DoFixedMachineGuns, 0x73F400);
    RH_ScopedInstall(FireFixedMachineGuns, 0x73DF00);
    RH_ScopedInstall(DoDriveByShootings, 0x741FD0);
    RH_ScopedInstall(ReleasePickedUpEntityWithWinch, 0x6D3CB0);
    RH_ScopedInstall(PickUpEntityWithWinch, 0x6D3CD0);
    RH_ScopedInstall(QueryPickedUpEntityWithWinch, 0x6D3CF0);
    RH_ScopedInstall(GetRopeHeightForHeli, 0x6D3D10);
    RH_ScopedInstall(SetRopeHeightForHeli, 0x6D3D30);

    RH_ScopedGlobalOverloadedInstall(SetVehicleAtomicVisibilityCB, "Object", 0x6D2690, RwObject*(*)(RwObject*, void*));
    RH_ScopedGlobalOverloadedInstall(SetVehicleAtomicVisibilityCB, "Frame", 0x6D26D0, RwFrame*(*)(RwFrame*, void*));
    RH_ScopedGlobalInstall(SetCompAlphaCB, 0x6D2950);
    RH_ScopedGlobalInstall(IsVehiclePointerValid, 0x6E38F0);
    RH_ScopedGlobalInstall(IsValidModForVehicle, 0x49B010);
    RH_ScopedGlobalInstall(RemoveUpgradeCB, 0x6D3300);
    RH_ScopedGlobalInstall(FindUpgradeCB, 0x6D3370);
    RH_ScopedGlobalOverloadedInstall(RemoveObjectsCB, "Object", 0x6D33B0, RwObject*(*)(RwObject*, void*));
    RH_ScopedGlobalOverloadedInstall(RemoveObjectsCB, "Frame", 0x6D3420, RwFrame*(*)(RwFrame*, void*));
    RH_ScopedGlobalInstall(CopyObjectsCB, 0x6D3450);
    RH_ScopedGlobalInstall(FindReplacementUpgradeCB, 0x6D3490);
    RH_ScopedGlobalInstall(RemoveAllUpgradesCB, 0x6D34D0);
    RH_ScopedNamedGlobalInstall(::DestroyVehicleAndDriverAndPassengers, "DestroyVehicleAndDriverAndPassengers", 0x6D2250); // `::` because the CVehicle member of the same name would win; cdecl (vehicle on the stack), NOT a thiscall member
}

// 0x6D5F10
CVehicle::CVehicle(eVehicleCreatedBy createdBy) : CPhysical(), m_vehicleAudio(), m_autoPilot() {
    m_bHasPreRenderEffects = true;
    SetTypeVehicle();

    m_fRawSteerAngle = 0.0f;
    m_f2ndSteerAngle = 0.0f;
    m_nCurrentGear = 1;
    m_fGearChangeCount = 0.0f;
    m_fWheelSpinForAudio = 0.0f;
    m_nCreatedBy = createdBy;
    m_nForcedRandomRouteSeed = 0;

    m_nVehicleUpperFlags = 0;
    m_nVehicleLowerFlags = 0;
    vehicleFlags.bFreebies = true;
    vehicleFlags.bIsHandbrakeOn = true;
    vehicleFlags.bEngineOn = true;
    vehicleFlags.bCanBeDamaged = true;
    vehicleFlags.bParking = false;
    vehicleFlags.bRestingOnPhysical = false;
    vehicleFlags.bCreatedAsPoliceVehicle = false;
    vehicleFlags.bVehicleCanBeTargettedByHS = true;
    vehicleFlags.bWinchCanPickMeUp = true;
    vehicleFlags.bPetrolTankIsWeakPoint = true;
    vehicleFlags.bConsideredByPlayer = true;
    vehicleFlags.bDoesProvideCover = true;
    vehicleFlags.bUsedForReplay = false;
    vehicleFlags.bDontSetColourWhenRemapping = false;
    vehicleFlags.bUseCarCheats = false;
    vehicleFlags.bHasBeenResprayed = false;
    vehicleFlags.bNeverUseSmallerRemovalRange = false;
    vehicleFlags.bDriverLastFrame = false;

    auto fRand = static_cast<float>(CGeneral::GetRandomNumber()) / static_cast<float>(RAND_MAX);
    vehicleFlags.bCanPark = fRand < 0.0F; //BUG: Seemingly never true, CGeneral::GetRandomNumber() strips the sign bit to always be 0

    CCarCtrl::UpdateCarCount(this, false);
    m_nExtendedRemovalRange = 0;
    m_fHealth = 1000.0f;
    m_pDriver = nullptr;
    m_nNumPassengers = 0;
    m_nMaxPassengers = 8;
    m_nNumGettingIn = 0;
    m_nGettingInFlags = 0;
    m_nGettingOutFlags = 0;

    m_nBombOnBoard = 0;
    m_nOverrideLights = eVehicleOverrideLightsState::NO_CAR_LIGHT_OVERRIDE;
    m_ropeType = 0;
    m_nGunsCycleIndex = 0;
    physicalFlags.bCanBeCollidedWith = true;

    m_nLastWeaponDamageType = -1;
    m_vehicleSpecialColIndex = -1;

    m_pWhoInstalledBombOnMe = nullptr;
    m_wBombTimer = 0;
    m_pWhoDetonatedMe = nullptr;
    m_nTimeWhenBlowedUp = 0;

    m_nPacMansCollected = 0;
    m_pFire = nullptr;
    m_nGunFiringTime = 0;
    m_nCopsInCarTimer = 0;
    m_nUsedForCover = 0;
    m_HornCounter = 0;
    m_HornPattern = 0;
    m_nCarHornTimer = 0;
    field_4EC = 0;
    m_pTowingVehicle = nullptr;
    m_pVehicleBeingTowed = nullptr;
    m_nTimeTillWeNeedThisCar = 0;
    m_nAlarmState = 0;
    m_nDoorLock = eCarLock::CARLOCK_UNLOCKED;
    m_nProjectileWeaponFiringTime = 0;
    m_nAdditionalProjectileWeaponFiringTime = 0;
    m_nTimeForMinigunFiring = 0;
    m_pLastDamageEntity = nullptr;
    m_pEntityWeAreOn = nullptr;
    m_fVehicleRearGroundZ = 0.0f;
    m_fVehicleFrontGroundZ = 0.0f;
    field_511 = 0;
    field_512 = 0;
    m_comedyControlState = eComedyControlState::INACTIVE;
    m_FrontCollPoly.valid = false;
    m_RearCollPoly.valid = false;
    m_pHandlingData = nullptr;
    m_nHandlingFlagsIntValue = static_cast<eVehicleHandlingFlags>(0);
    m_autoPilot.m_nTempAction = TEMPACT_NONE;
    m_autoPilot.SetCarMission(MISSION_NONE, 0);
    m_autoPilot.carCtrlFlags.bAvoidLevelTransitions = false;
    m_nRemapTxd = -1;
    m_nPreviousRemapTxd = -1;
    m_pRemapTexture = nullptr;
    m_pOverheatParticle = nullptr;
    m_pFireParticle = nullptr;
    m_pDustParticle = nullptr;
    m_pCustomCarPlate = nullptr;
    m_anUpgrades.fill(-1);
    m_fWheelScale = 1.0f;
    m_nWindowsOpenFlags = 0;
    m_nNitroBoosts = 0;
    m_nHasslePosId = 0;
    m_nVehicleWeaponInUse = CAR_WEAPON_NOT_USED;
    m_fDirtLevel = (float)((CGeneral::GetRandomNumber() % 15));
    m_nCreationTime = CTimer::GetTimeInMS();
    SetCollisionLighting(tColLighting(0x48));
}

// 0x6E2B40
CVehicle::~CVehicle() {
    CReplay::RecordVehicleDeleted(this);
    m_nAlarmState = 0;
    DeleteRwObject(); // V1053 Calling the 'DeleteRwObject' virtual function in the destructor may lead to unexpected result at runtime.
    CRadar::ClearBlipForEntity(eBlipType::BLIP_CAR, GetVehiclePool()->GetRef(this));

    if (m_pDriver) {
        m_pDriver->FlagToDestroyWhenNextProcessed();
    }


    for (const auto passenger : GetPassengers()) {
        if (passenger) {
            passenger->FlagToDestroyWhenNextProcessed();
        }
    }

    if (m_pFire) {
        m_pFire->Extinguish();
        m_pFire = nullptr;
    }

    CCarCtrl::UpdateCarCount(this, true);
    if (vehicleFlags.bIsAmbulanceOnDuty) {
        --CCarCtrl::NumAmbulancesOnDuty;
        vehicleFlags.bIsAmbulanceOnDuty = false;
    }

    if (vehicleFlags.bIsFireTruckOnDuty) {
        --CCarCtrl::NumFireTrucksOnDuty;
        vehicleFlags.bIsFireTruckOnDuty = false;
    }

    if (m_vehicleSpecialColIndex > -1) {
        m_aSpecialColVehicle[m_vehicleSpecialColIndex] = nullptr;
        m_vehicleSpecialColIndex = -1;
    }

    for (auto particle : { m_pOverheatParticle, m_pFireParticle, m_pDustParticle }) {
        if (particle) {
            g_fxMan.DestroyFxSystem(particle);
            particle = nullptr;
        }
    }

    if (m_pCustomCarPlate) {
        RwTextureDestroy(m_pCustomCarPlate);
        m_pCustomCarPlate = nullptr;
    }

    const auto iRopeInd = CRopes::FindRope(reinterpret_cast<uint32>(this) + 1);
    if (iRopeInd >= 0) {
        CRopes::GetRope(iRopeInd).Remove();
    }

    if (!physicalFlags.bRenderScorched && m_fHealth < 250.0F) {
        CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
    }
}

void* CVehicle::operator new(unsigned size) {
    return GetVehiclePool()->New();
}

void CVehicle::operator delete(void* data) {
    GetVehiclePool()->Delete(static_cast<CVehicle*>(data));
}

void* CVehicle::operator new(unsigned size, int32 poolRef) {
    return GetVehiclePool()->NewAt(poolRef);
}

void CVehicle::operator delete(void* data, int32 poolRef) {
    GetVehiclePool()->Delete(static_cast<CVehicle*>(data));
}

// 0x6D6A40
void CVehicle::SetModelIndex(uint32 index) {
    CEntity::SetModelIndex(index);
    auto mi = CModelInfo::GetModelInfo(index)->AsVehicleModelInfoPtr();
    CustomCarPlate_TextureCreate(mi);
    for (auto i = 0u; i < std::size(m_anExtras); i++) {
        m_anExtras[i] = CVehicleModelInfo::ms_compsUsed[i];
    }
    m_nMaxPassengers = CVehicleModelInfo::GetMaximumNumberOfPassengersFromNumberOfDoors(index);
    switch (m_nModelIndex) {
    case MODEL_RCBANDIT:
    case MODEL_RCBARON:
    case MODEL_RCRAIDER:
    case MODEL_RCGOBLIN:
    case MODEL_RCTIGER:
        vehicleFlags.bIsRCVehicle = true;
        break;
    default:
        vehicleFlags.bCreatedAsPoliceVehicle = false;
        vehicleFlags.bIsRCVehicle = false;
        break;
    }

    // Set up weapons
    switch (m_nModelIndex) {
    case MODEL_RUSTLER:
    case MODEL_STUNT:
        m_nVehicleWeaponInUse = CAR_WEAPON_HEAVY_GUN;
        break;
    case MODEL_BEAGLE:
        m_nVehicleWeaponInUse = CAR_WEAPON_FREEFALL_BOMB;
        break;
    case MODEL_HYDRA:
    case MODEL_TORNADO:
        m_nVehicleWeaponInUse = CAR_WEAPON_LOCK_ON_ROCKET;
        break;
    }
}

// 0x6D6410
void CVehicle::DeleteRwObject() {
    SetRemapTexDictionary(-1);
    RemoveAllUpgrades();
    CEntity::DeleteRwObject();
}

// 0x6D6640
void CVehicle::SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled,
    bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck) {
    if (colPhysical->GetIsTypePed()
        && colPhysical->AsPed()->bKnockedOffBike
        && colPhysical->AsPed()->m_pVehicle == this)
    {
        bCollisionDisabled = true;
        return;
    }

    if (physicalFlags.bSubmergedInWater
        && GetStatus() != STATUS_PLAYER
        && (GetStatus() != STATUS_REMOTE_CONTROLLED && colPhysical->DoesNotCollideWithFlyers())) // BUG:? Seems like it should check for it being heli
    {
        bCollisionDisabled = true;
        return;
    }

    if (m_pEntityIgnoredCollision == colPhysical || colPhysical->m_pEntityIgnoredCollision == this) {
        bCollidedEntityCollisionIgnored = true;
        physicalFlags.bSkipLineCol = true;
        return;
    }

    if (m_pAttachedTo == colPhysical) {
        bCollidedEntityCollisionIgnored = true;
        return;
    }

    if (colPhysical->m_pAttachedTo == this) {
        bCollisionDisabled = true;
        physicalFlags.bSkipLineCol = true;
        return;
    }

    if (physicalFlags.bDisableCollisionForce && colPhysical->physicalFlags.bDisableCollisionForce) {
        bCollisionDisabled = true;
        return;
    }

    if (GetIsStuck()
        && colPhysical->GetIsTypeVehicle()
        && (colPhysical->AsVehicle()->physicalFlags.bDisableCollisionForce && !colPhysical->AsVehicle()->physicalFlags.bCollidable)
    ) {
        bCollidedEntityCollisionIgnored = true;
        physicalFlags.bSkipLineCol = true;
        return;
    }

    if (colPhysical->IsImmovable()) {
        if (bIgnoreStuckCheck)
            bCollidedEntityCollisionIgnored = true;
        else if (GetIsStuck() || colPhysical->GetIsStuck())
            bThisOrCollidedEntityStuck = true;

        return;
    }

    if (colPhysical->GetIsTypeObject())
    {
        if (colPhysical->AsObject()->IsFallenLampPost())
        {
            bCollisionDisabled = true;
            colPhysical->AsObject()->m_pEntityIgnoredCollision = this;
        }
        else
        {
            if (colPhysical->IsModelTempCollision())
            {
                bCollisionDisabled = true;
                return;
            }

            if (colPhysical->AsObject()->IsTemporary()
                || colPhysical->AsObject()->IsExploded()
                || !colPhysical->GetIsStatic())
            {
                if (IsConstructionVehicle())
                {
                    if (GetIsStuck() || colPhysical->GetIsStuck())
                        bThisOrCollidedEntityStuck = true;
                }
                else if (!colPhysical->AsObject()->CanBeSmashed() && !IsBike())
                {
                    auto tempMat = CMatrix();
                    auto* cm = colPhysical->GetColModel();
                    auto& vecMax = cm->GetBoundingBox().m_vecMax;
                    if (vecMax.x < 1.0F && vecMax.y < 1.0F && vecMax.z < 1.0F)
                    {
                        const auto vecSize = cm->GetBoundingBox().GetSize();
                        const auto vecTransformed = colPhysical->m_matrix->TransformPoint(vecSize);

                        if (GetPosition().z > vecTransformed.z)
                            bCollidedEntityCollisionIgnored = true;
                        else
                        {
                            Invert(*m_matrix, tempMat);
                            if (tempMat.TransformPoint(vecTransformed).z < 0.0F) // `m_matrix->GetUp().Dot(vecTransformed)` should work too
                                bCollidedEntityCollisionIgnored = true;
                        }
                    }
                }
            }

            if (!bCollidedEntityCollisionIgnored
                && !bCollisionDisabled
                && !bThisOrCollidedEntityStuck
                && colPhysical->GetIsStuck())
            {
                bCollidedEntityUnableToMove = true;
            }
            return;
        }
    }

    if (colPhysical->IsRCCar()) {
        bCollidedEntityCollisionIgnored = true;
        physicalFlags.bSkipLineCol = true;
        return;
    }

    if (IsRCCar() && (colPhysical->GetIsTypeVehicle() || colPhysical->GetIsTypePed())) {
        bCollidedEntityCollisionIgnored = true;
        physicalFlags.bSkipLineCol = true;
        return;
    }

    if (colPhysical == m_pTowingVehicle || colPhysical == m_pVehicleBeingTowed) {
        bThisOrCollidedEntityStuck = true;
        physicalFlags.bSkipLineCol = true;
        return;
    }

    if (colPhysical->GetIsStuck()) {
        bCollidedEntityUnableToMove = true;
        return;
    }
}

// 0x6D0E90
uint8 CVehicle::SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2) {
    if (physicalFlags.bDisableCollisionForce)
        return 1;

    const auto fMoveSquared = m_vecMoveSpeed.SquaredMagnitude() * sq(CTimer::GetTimeStep());
    if (fMoveSquared < 0.16F)
        return 1;

    auto fMove = sqrt(fMoveSquared);
    if (!TreatAsPlayerForCollisions())
    {
        if (fMoveSquared <= 0.32F)
            fMove *= (10.0F / 4.0F);
        else
            fMove *= (10.0F / 3.0F);
    }
    else if (IsBike())
        fMove *= (10.0F / 1.5F);
    else
        fMove *= (10.0F / 2.0F);

    auto& bbox = CEntity::GetColModel()->GetBoundingBox();
    auto fLongestDir = std::fabs(DotProduct(m_vecMoveSpeed, GetForward()) * CTimer::GetTimeStep() / bbox.GetLength());
    fLongestDir = std::max(fLongestDir, std::fabs(DotProduct(m_vecMoveSpeed, GetRight()) * CTimer::GetTimeStep() / bbox.GetWidth()));
    fLongestDir = std::max(fLongestDir, std::fabs(DotProduct(m_vecMoveSpeed, GetUp()) * CTimer::GetTimeStep() / bbox.GetHeight()));

    if (IsBike())
        fLongestDir *= 1.5F;

    if (fLongestDir < 1.0F)
        bProcessCollisionBeforeSettingTimeStep = true;
    else if (fLongestDir < 2.0F)
        unk2 = true;

    return static_cast<uint8>(ceil(fMove));
}

// 0x6D6480
void CVehicle::PreRender() {
    if (!IsTrain())
        CalculateLightingFromCollision();

    PreRenderDriverAndPassengers();
    if (CModelInfo::GetModelInfo(m_nModelIndex)->m_n2dfxCount)
        CEntity::ProcessLightsForEntity();

    m_renderLights.m_bRightFront = false;
    m_renderLights.m_bLeftFront = false;
    m_renderLights.m_bRightRear = false;
    m_renderLights.m_bLeftRear = false;

    const auto fCoeff = CPhysical::GetLightingFromCol(false) * 0.4F;
    GetVehicleModelInfo()->SetEnvMapCoeff(fCoeff);
}

// 0x6D0E60
void CVehicle::Render() {
    auto* mi = GetVehicleModelInfo();
    const auto iDirtLevel = static_cast<int32>(m_fDirtLevel) & 0xF;
    CVehicleModelInfo::SetDirtTextures(mi, iDirtLevel);

    CEntity::Render();
}

// 0x553F20
bool CVehicle::SetupLighting() {
    ActivateDirectional();
    return CRenderer::SetupLightingForEntity(this);
}

// 0x5533D0
void CVehicle::RemoveLighting(bool bRemove) {
    if (!physicalFlags.bRenderScorched)
        CPointLights::RemoveLightsAffectingObject();

    SetAmbientColours();
    DeActivateDirectional();
}

// 0x6D56C0
void CVehicle::ProcessOpenDoor(CPed* ped, uint32 doorComponentId_, uint32 animGroup, uint32 animId, float fTime) {
    auto doorComponentId = (int32)doorComponentId_; // silence warns, todo: OpenDoor receives int32, why?
    eDoors iCheckedDoor = [&] {
        switch (doorComponentId) {
        case COMPONENT_DOOR_RF: return DOOR_RIGHT_FRONT;
        case COMPONENT_DOOR_LR: return DOOR_RIGHT_REAR;
        case COMPONENT_DOOR_RR: return DOOR_LEFT_FRONT;
        case COMPONENT_WING_LF: return DOOR_LEFT_REAR;
        default:
            assert(false); // Shouldn't get here
            return static_cast<eDoors>(fTime);
        }
    }();

    if (IsDoorMissing(iCheckedDoor)) {
        return;
    }

    const auto group = static_cast<AssocGroupId>(m_pHandlingData->m_nAnimGroup);
    float fAnimStart, fAnimEnd;
    switch (animId) {
    case ANIM_ID_CAR_OPEN_LHS:
    case ANIM_ID_CAR_OPEN_RHS:
    case ANIM_ID_CAR_OPEN_LHS_1:
    case ANIM_ID_CAR_OPEN_RHS_1: {
        CVehicleAnimGroupData::GetInOutTimings(group, eInOutTimingMode::OPEN_START, &fAnimStart, &fAnimEnd);
        if (fTime < fAnimStart) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, false);
        } else if (fTime > fAnimEnd) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        } else if (fTime > fAnimStart && fTime < fAnimEnd) {
            const auto fNewRatio = invLerp(fAnimStart, fAnimEnd, fTime);
            const auto fCurRatio = GetDooorAngleOpenRatio(iCheckedDoor);
            if (fCurRatio < fNewRatio) {
                OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
            }
        }

        return;
    }
    case ANIM_ID_CAR_CLOSE_LHS_0:
    case ANIM_ID_CAR_CLOSE_RHS_0:
    case ANIM_ID_CAR_CLOSE_LHS_1:
    case ANIM_ID_CAR_CLOSE_RHS_1: {
        CVehicleAnimGroupData::GetInOutTimings(group, eInOutTimingMode::CLOSE_STOP, &fAnimStart, &fAnimEnd);
        if (fTime < fAnimStart) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        } else if (fTime > fAnimEnd) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        } else if (fTime > fAnimStart && fTime < fAnimEnd) {
            const auto fNewRatio = 1.0F - invLerp(fAnimStart, fAnimEnd, fTime);
            const auto fCurRatio = GetDooorAngleOpenRatio(iCheckedDoor);
            if (fCurRatio > fNewRatio) {
                OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
            }
        }
        return;
    }
    case ANIM_ID_CAR_CLOSEDOOR_LHS_0:
    case ANIM_ID_CAR_CLOSEDOOR_RHS_0:
    case ANIM_ID_CAR_CLOSEDOOR_LHS_1:
    case ANIM_ID_CAR_CLOSEDOOR_RHS_1: {
        CVehicleAnimGroupData::GetInOutTimings(group, eInOutTimingMode::OPEN_STOP, &fAnimStart, &fAnimEnd);
        if (fTime < fAnimStart) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        } else if (fTime > fAnimEnd) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        } else if (fTime > fAnimStart && fTime < fAnimEnd) {
            const auto fNewRatio = 1.0F - invLerp(fAnimStart, fAnimEnd, fTime);
            OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
        }
        return;
    }
    case ANIM_ID_CAR_GETOUT_LHS_0:
    case ANIM_ID_CAR_GETOUT_RHS_0:
    case ANIM_ID_CAR_GETOUT_LHS_1:
    case ANIM_ID_CAR_GETOUT_RHS_1: {
        CVehicleAnimGroupData::GetInOutTimings(group, eInOutTimingMode::CLOST_START, &fAnimStart, &fAnimEnd);
        if (fTime < fAnimStart) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        } else if (fTime > fAnimEnd) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        } else if (fTime > fAnimStart && fTime < fAnimEnd) {
            const auto fNewRatio = invLerp(fAnimStart, fAnimEnd, fTime);
            const auto fCurRatio = GetDooorAngleOpenRatio(iCheckedDoor);
            if (fCurRatio < fNewRatio) {
                OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
            }
        }
        return;
    }
    case ANIM_ID_CAR_JACKEDLHS:
    case ANIM_ID_CAR_JACKEDRHS:
        if (fTime < 0.1F) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        } else if (fTime > 0.4F) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        } else if (fTime > 0.1F && fTime < 0.4F) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, (fTime - 0.1F) * (10.0F / 3.0F), true);
        }
        return;

    case ANIM_ID_CAR_FALLOUT_LHS:
    case ANIM_ID_CAR_FALLOUT_RHS:
        if (fTime < 0.1F) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        } else if (fTime > 0.4F) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        } else if (fTime > 0.1F && fTime < 0.4F) {
            OpenDoor(ped, doorComponentId, iCheckedDoor, (fTime - 0.1F) * (10.0F / 3.0F), true);
        }

        return;

    case ANIM_ID_CAR_PULLOUT_LHS:
    case ANIM_ID_CAR_PULLOUT_RHS:
    case ANIM_ID_UNKNOWN_15:
        switch (animGroup) {
        case ANIM_GROUP_STDCARAMIMS:
        case ANIM_GROUP_LOWCARAMIMS:
        case ANIM_GROUP_TRKCARANIMS:
        case ANIM_GROUP_COACHCARANIMS:
        case ANIM_GROUP_BUSCARANIMS:
        case ANIM_GROUP_CONVCARANIMS:
        case ANIM_GROUP_MTRKCARANIMS:
        case ANIM_GROUP_STDTALLCARAMIMS:
        case ANIM_GROUP_BFINJCARAMIMS:
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        }
        return;

    case ANIM_ID_CAR_ROLLOUT_LHS:
    case ANIM_ID_CAR_ROLLOUT_RHS: {
        switch (animGroup) {
        case ANIM_GROUP_STDCARAMIMS:
        case ANIM_GROUP_LOWCARAMIMS:
        case ANIM_GROUP_TRKCARANIMS:
        case ANIM_GROUP_RUSTPLANEANIMS:
        case ANIM_GROUP_COACHCARANIMS:
        case ANIM_GROUP_CONVCARANIMS:
        case ANIM_GROUP_MTRKCARANIMS:
        case ANIM_GROUP_STDTALLCARAMIMS:
        case ANIM_GROUP_BFINJCARAMIMS:
            fAnimStart = 0.01F;
            fAnimEnd = 0.1F;
            break;
        default:
            NOTSA_UNREACHABLE();
        }

        if (fTime < fAnimStart)
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        else if (fTime > fAnimEnd)
            OpenDoor(ped, doorComponentId, iCheckedDoor, 1.0F, true);
        else if (fTime > fAnimStart && fTime < fAnimEnd) {
            const auto fNewRatio = invLerp(fAnimStart, fAnimEnd, fTime);
            const auto fCurRatio = GetDooorAngleOpenRatio(iCheckedDoor);
            if (fCurRatio < fNewRatio) {
                OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
            }
        }

        return;
    }

    case ANIM_ID_CAR_ROLLDOOR: {
        float fAnimEnd2;
        switch (animGroup) {
        case ANIM_GROUP_STDCARAMIMS:
        case ANIM_GROUP_LOWCARAMIMS:
        case ANIM_GROUP_TRKCARANIMS:
        case ANIM_GROUP_RUSTPLANEANIMS:
        case ANIM_GROUP_COACHCARANIMS:
        case ANIM_GROUP_CONVCARANIMS:
        case ANIM_GROUP_MTRKCARANIMS:
        case ANIM_GROUP_STDTALLCARAMIMS:
        case ANIM_GROUP_BFINJCARAMIMS:
            fAnimStart = 0.05f;
            fAnimEnd = 0.3f;
            fAnimEnd2 = 0.475f;
            break;
        default:
            NOTSA_UNREACHABLE();
        }

        if (fTime > fAnimEnd2)
            OpenDoor(ped, doorComponentId, iCheckedDoor, 0.0F, true);
        else if (fTime > fAnimStart && fTime < fAnimEnd) {
            const auto fNewRatio = invLerp(fAnimStart, 0.2F, fTime);
            const auto fCurRatio = GetDooorAngleOpenRatio(iCheckedDoor);
            if (fCurRatio < fNewRatio) {
                OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
            }
        } else if (fTime > fAnimEnd && fTime < fAnimEnd2) {
            const auto fNewRatio = 1.0F - invLerp(fAnimEnd, fAnimEnd2, fTime);
            const auto fCurRatio = GetDooorAngleOpenRatio(iCheckedDoor);
            if (fCurRatio > fNewRatio)
                OpenDoor(ped, doorComponentId, iCheckedDoor, fNewRatio, true);
        }

        return;
    }
    }
}

// 0x6DF4A0
void CVehicle::ProcessDrivingAnims(CPed* driver, bool blend) {
    if (m_bOffscreen || !driver->IsPlayer())
        return;

    auto* radioTuneAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_CAR_TUNE_RADIO);
    if (blend) {
        radioTuneAnim = CAnimManager::BlendAnimation(driver->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CAR_TUNE_RADIO, 4.0F);
    }

    if (radioTuneAnim)
        return;

    CRideAnims* usedAnims;
    if (vehicleFlags.bLowVehicle)
        usedAnims = &aDriveAnimIdsLow;
    else if (IsBoat() && !m_pHandlingData->m_bSitInBoat)
        usedAnims = &aDriveAnimIdsBoat;
    else if (CVehicleAnimGroupData::UsesKartDrivingAnims(static_cast<AssocGroupId>(m_pHandlingData->m_nAnimGroup)))
        usedAnims = &aDriveAnimIdsKart;
    else if (CVehicleAnimGroupData::UsesTruckDrivingAnims(static_cast<AssocGroupId>(m_pHandlingData->m_nAnimGroup)))
        usedAnims = &aDriveAnimIdsTruck;
    else {
        const auto fDrivingSkill = CStats::GetStatValue(eStats::STAT_DRIVING_SKILL);
        const auto fSpeed = m_vecMoveSpeed.Magnitude();
        if (fDrivingSkill < 50.0F) {
            usedAnims = fSpeed <= 0.4F ? &aDriveAnimIdsBadSlow : &aDriveAnimIdsBad;
        } else if (fDrivingSkill < 100.0F) {
            usedAnims = fSpeed <= 0.4F ? &aDriveAnimIdsStdSlow : &aDriveAnimIdsStd;
        } else {
            usedAnims = fSpeed <= 0.4F ? &aDriveAnimIdsProSlow : &aDriveAnimIdsPro;
        }
    }

    // 0x6DF620
    auto* idleAnim      = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), usedAnims->idle);
    auto* lookLeftAnim  = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), usedAnims->left);
    auto* lookRightAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), usedAnims->right);
    auto* lookBackAnim  = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), usedAnims->back);

    if (!idleAnim) {
        if (RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_CAR_SIT)) {
            CAnimManager::BlendAnimation(driver->GetRpClump(), ANIM_GROUP_DEFAULT, usedAnims->idle, 4.0F);
        }
        return;
    }

    if (idleAnim->m_BlendAmount < 1.0F)
        return;

    if (lookBackAnim
        && CCamera::GetActiveCamera().m_nMode == eCamMode::MODE_1STPERSON
        && CCamera::GetActiveCamera().m_nDirectionWasLooking == eLookingDirection::LOOKING_DIRECTION_BEHIND
    ) {
        lookBackAnim->m_BlendDelta = -1000.0F;
    }

    // 0x6DF6DA
    auto* driveByAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_DRIVEBY_L);
    if (!driveByAnim) driveByAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_DRIVEBY_R);
    if (!driveByAnim) driveByAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_DRIVEBYL_L);
    if (!driveByAnim) driveByAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_DRIVEBYL_R);

    if (!vehicleFlags.bLowVehicle
        && m_GasPedal < 0.0F
        && !driveByAnim
        && GetVehicleAppearance() != VEHICLE_APPEARANCE_HELI
        && GetVehicleAppearance() != VEHICLE_APPEARANCE_PLANE
    ) {
        if ((CCamera::GetActiveCamera().m_nMode != eCamMode::MODE_1STPERSON
            || CCamera::GetActiveCamera().m_nDirectionWasLooking != eLookingDirection::LOOKING_DIRECTION_BEHIND)
            && (!lookBackAnim || lookBackAnim->m_BlendAmount < 1.0F && lookBackAnim->m_BlendDelta <= 0.0F)
        ) {
            CAnimManager::BlendAnimation(driver->GetRpClump(), ANIM_GROUP_DEFAULT, usedAnims->back, 4.0F);
        }
        return;
    }

    if (m_fSteerAngle == 0.0F || driveByAnim) {
        if (lookLeftAnim)  lookLeftAnim->m_BlendDelta  = -4.0F;
        if (lookRightAnim) lookRightAnim->m_BlendDelta = -4.0F;
        if (lookBackAnim)  lookBackAnim->m_BlendDelta  = -4.0F;
        return;
    }

    auto fUsedAngle = std::fabs(m_fSteerAngle / 0.61F);
    fUsedAngle = std::clamp(fUsedAngle, 0.0F, 1.0F);

    if (m_fSteerAngle < 0) {
        if (lookLeftAnim) {
            lookLeftAnim->m_BlendAmount = 0.0F;
            lookLeftAnim->m_BlendDelta  = 0.0F;
        }
        if (lookRightAnim) {
            lookRightAnim->m_BlendAmount = fUsedAngle;
            lookRightAnim->m_BlendDelta  = 0.0F;
        } else {
            CAnimManager::BlendAnimation(driver->GetRpClump(), ANIM_GROUP_DEFAULT, usedAnims->right, 4.0F);
        }
    } else {
        if (lookRightAnim) {
            lookRightAnim->m_BlendAmount = 0.0f;
            lookRightAnim->m_BlendDelta = 0.0F;
        }
        if (lookLeftAnim) {
            lookLeftAnim->m_BlendAmount = fUsedAngle;
            lookLeftAnim->m_BlendDelta = 0.0F;
        } else {
            CAnimManager::BlendAnimation(driver->GetRpClump(), ANIM_GROUP_DEFAULT, usedAnims->left, 4.0F);
        }
    }

    if (lookBackAnim) {
        lookBackAnim->m_BlendDelta = -4.0F;
    }
}

// 0x871F54// 0x6D63F0
float CVehicle::GetHeightAboveRoad() {
    return CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->GetBoundingBox().m_vecMin.z * -1.0F;
}

// 0x6D1F30
bool CVehicle::CanPedStepOutCar(bool bIgnoreSpeedUpright) const {
    auto const fUpZ = m_matrix->GetUp().z;
    if (std::fabs(fUpZ) <= 0.1F) {
        if (std::fabs(m_vecMoveSpeed.z) > 0.05F || m_vecMoveSpeed.Magnitude2D() > 0.01F || m_vecTurnSpeed.SquaredMagnitude() > 0.0004F) { // 0.02F / 50.0f
            return false;
        }
        return true;
    }

    if (IsBoat())
        return true;

    if (bIgnoreSpeedUpright)
        return m_vecTurnSpeed.SquaredMagnitude() > 0.0004F;

    return m_vecMoveSpeed.Magnitude2D() <= 0.01F &&
           std::fabs(m_vecMoveSpeed.z) <= 0.05F &&
           m_vecTurnSpeed.SquaredMagnitude() <= 0.0004F;
}

// 0x6D2030
bool CVehicle::CanPedJumpOutCar(CPed* ped) {
    if (IsBike())
    {
        if (!HasPassengerAtSeat(0) || ped == m_apPassengers[0])
            return m_vecMoveSpeed.SquaredMagnitude2D() >= 0.07F;

        return false;
    }

    const auto fHorSpeedSquared = m_vecMoveSpeed.SquaredMagnitude2D();
    if (!IsSubPlane()
        && !IsSubHeli()
        && (!IsAutomobile() || m_matrix->GetUp().z >= 0.3F || m_nLastCollisionTime <= CTimer::GetTimeInMS() - 1000))
    {
        return fHorSpeedSquared >= 0.1F && fHorSpeedSquared <= 0.5F;
    }

    if (fHorSpeedSquared >= 0.1F)
        return true;

    if (CanPedStepOutCar(false))
        return false;

    m_vecTurnSpeed *= 0.9F; // BUG(PRONE): We are modifying vehicle move and turn speeds in function that seems that it should be const
    auto const fMoveSpeedSquared = m_vecMoveSpeed.SquaredMagnitude();
    if (fMoveSpeedSquared / 100.0F > sq(CTimer::GetTimeStep()) * sq(0.008F))
    {
        m_vecMoveSpeed *= 0.9F;
        return false;
    }

    auto fMoveMult = CTimer::GetTimeStep() / std::sqrt(fMoveSpeedSquared) * 0.016F;
    fMoveMult = std::max(0.0F, 1.0F - fMoveMult);
    m_vecMoveSpeed *= fMoveMult;
    return false;
}

// 0x871F6C// 0x6DFB70
bool CVehicle::GetTowHitchPos(CVector& outPos, bool bCheckModelInfo, CVehicle* vehicle) {
    if (!bCheckModelInfo)
        return false;

    auto const fColFront = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->GetBoundingBox().m_vecMax.y;
    outPos.Set(0.0F, fColFront + 1.0F, 0.0F);
    outPos = m_matrix->TransformPoint(outPos);
    return true;
}

// 0x871F70// 0x6DFBE0
bool CVehicle::GetTowBarPos(CVector& outPos, bool bCheckModelInfo, CVehicle* vehicle) {
    if (!bCheckModelInfo)
        return false;

    auto const fColRear = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->GetBoundingBox().m_vecMin.y;
    outPos.Set(0.0F, fColRear - 1.0F, 0.0F);
    outPos = m_matrix->TransformPoint(outPos);
    return true;
}

// 0x871F80// 0x5D4760
bool CVehicle::Save() {
    uint32 size = sizeof(CVehicleSaveStructure);
    CVehicleSaveStructure data;
    data.Construct(this);
    CGenericGameStorage::SaveDataToWorkBuffer(&size, sizeof(uint32)); // Unused, game ignores it on load and uses const value
    CGenericGameStorage::SaveDataToWorkBuffer(&data, size);
    return true;
}

// 0x871F84// 0x5D2900
bool CVehicle::Load() {
    uint32 size;
    CVehicleSaveStructure data;
    CGenericGameStorage::LoadDataFromWorkBuffer(&size, sizeof(uint32));
    CGenericGameStorage::LoadDataFromWorkBuffer(&data, sizeof(CVehicleSaveStructure)); // BUG: Should use the value readen line above this, not constant
    data.Extract(this);
    return true;
}

// 0x6D0B40
void CVehicle::Shutdown() {
    for (auto& specialColModel : m_aSpecialColModel) {
        if (specialColModel.m_pColData) {
            specialColModel.RemoveCollisionVolumes();
        }
    }
}

// -1 if no remap index
// 0x6D0B70
int32 CVehicle::GetRemapIndex() {
    auto* mi = GetVehicleModelInfo();
    if (mi->GetNumRemaps() <= 0) {
        return -1;
    }

    for (auto i = 0; i < mi->GetNumRemaps(); ++i) {
        if (mi->m_anRemapTxds[i] == m_nPreviousRemapTxd) {
            return i;
        }
    }
    return -1;
}

// 0x6D0BC0
void CVehicle::SetRemapTexDictionary(int32 txdId) {
    if (txdId != m_nPreviousRemapTxd) {
        if (txdId == -1) {
            m_pRemapTexture = nullptr;
            CTxdStore::RemoveRef(m_nPreviousRemapTxd);
            m_nPreviousRemapTxd = -1;
        }
        m_nRemapTxd = txdId;
    }
}

// index for m_awRemapTxds[] array
// 0x6D0C00
void CVehicle::SetRemap(int32 remapIndex) {
    if (remapIndex == -1) {
        SetRemapTexDictionary(-1);
    } else {
        auto const infoRemapInd = GetVehicleModelInfo()->m_anRemapTxds[remapIndex];
        SetRemapTexDictionary(infoRemapInd);
    }
}

// 0x6D0CA0
void CVehicle::SetCollisionLighting(tColLighting lighting) {
    std::ranges::fill(m_anCollisionLighting, lighting);
}

// 0x6D0CC0
void CVehicle::UpdateLightingFromStoredPolys() {
    m_anCollisionLighting[0] = m_FrontCollPoly.ligthing;
    m_anCollisionLighting[1] = m_FrontCollPoly.ligthing;

    m_anCollisionLighting[2] = m_RearCollPoly.ligthing;
    m_anCollisionLighting[3] = m_RearCollPoly.ligthing;
}

// 0x6D0CF0
void CVehicle::CalculateLightingFromCollision() {
    float fAvgLight = 0.0F;
    for (auto& colLighting : m_anCollisionLighting) {
        fAvgLight += colLighting.GetCurrentLighting();
    }

    fAvgLight /= 4.0F;
    m_fContactSurfaceBrightness = fAvgLight;

}

// 0x6D0E20
void CVehicle::ResetAfterRender() {
    RwRenderStateSet(RwRenderState::rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLBACK));
    CVehicleModelInfo::ResetEditableMaterials((RpClump*)GetRwObject());

    if (IsAutomobile()) {
        auto* const mi = GetVehicleModelInfo();
        assert(mi != nullptr);
        AsAutomobile()->CustomCarPlate_AfterRenderingStop(mi);
    }
}

// 0x6D1080
eVehicleAppearance CVehicle::GetVehicleAppearance() const {
    uint32 flags = (
        m_pHandlingData->m_nModelFlags &
        (
            VEHICLE_HANDLING_MODEL_IS_BOAT |
            VEHICLE_HANDLING_MODEL_IS_PLANE |
            VEHICLE_HANDLING_MODEL_IS_HELI |
            VEHICLE_HANDLING_MODEL_IS_BIKE
        )
    );

    if (flags <= VEHICLE_HANDLING_MODEL_IS_HELI) {
        switch (flags) {
        case VEHICLE_HANDLING_MODEL_IS_HELI: return VEHICLE_APPEARANCE_HELI;
        case VEHICLE_HANDLING_MODEL_NONE:    return VEHICLE_APPEARANCE_AUTOMOBILE;
        case VEHICLE_HANDLING_MODEL_IS_BIKE: return VEHICLE_APPEARANCE_BIKE;
        default:                             return VEHICLE_APPEARANCE_NONE;
        }
    }

    if (flags == VEHICLE_HANDLING_MODEL_IS_PLANE)
        return VEHICLE_APPEARANCE_PLANE;

    if (flags != VEHICLE_HANDLING_MODEL_IS_BOAT)
        return VEHICLE_APPEARANCE_NONE;

    return VEHICLE_APPEARANCE_BOAT;
}

// returns false if vehicle model has no car plate material
// 0x6D10E0
bool CVehicle::CustomCarPlate_TextureCreate(CVehicleModelInfo* model) {
    m_pCustomCarPlate = nullptr;

    if (!model->m_pPlateMaterial) {
        return false;
    }

    if (const auto text = model->GetCustomCarPlateText()) {
        m_pCustomCarPlate = CCustomCarPlateMgr::CreatePlateTexture(text, model->m_nPlateType);
        model->SetCustomCarPlateText(text);
        model->m_nPlateType = -1;
    } else {
        m_pCustomCarPlate = RpMaterialGetTexture(model->m_pPlateMaterial);
        RwTextureAddRef(m_pCustomCarPlate);
    }

    return true;
}

// 0x6D1150
void CVehicle::CustomCarPlate_TextureDestroy() {
    if (m_pCustomCarPlate) {
        RwTextureDestroy(m_pCustomCarPlate);
        m_pCustomCarPlate = nullptr;
    }
}

// 0x6D1180
bool CVehicle::CanBeDeleted() {
    if (m_nNumGettingIn || m_nGettingOutFlags)
        return false;

    if (m_pDriver) {
        if (m_pDriver->IsCreatedByMission())
            return false;

        if (!m_pDriver->IsStateDriving() && !m_pDriver->IsStateDead())
            return false;
    }

    for (auto passenger : m_apPassengers) {
        if (passenger) {
            if (passenger->IsCreatedByMission())
                return false;

            if (!passenger->IsStateDriving() && !passenger->IsStateDead()) // OG: checked twice
                return false;
        }
    }

    switch (GetCreatedBy()) {
    case MISSION_VEHICLE:
    case PERMANENT_VEHICLE:
        return false;
    default:
        return true;
    }
}

// 0x6D1230
float CVehicle::ProcessWheelRotation(tWheelState wheelState, const CVector& arg1, const CVector& arg2, float arg3) {
    if (wheelState == WHEEL_STATE_SPINNING)
        return -1.1f;

    if (wheelState == WHEEL_STATE_FIXED)
        return 0.0f;

    const auto angle = DotProduct(arg1, arg2) / arg3;
    return -angle;
}

// 0x6D1280
bool CVehicle::CanVehicleBeDamaged(CEntity* damager, eWeaponType weapon, bool& bDamagedDueToFireOrExplosionOrBullet) {
    if (!vehicleFlags.bCanBeDamaged)
        return false;

    const auto player = FindPlayerPed();
    const auto vehicle = FindPlayerVehicle();
    if (   GetStatus() != STATUS_PLAYER
        && physicalFlags.bInvulnerable
        && damager != player
        && damager != vehicle
    ) {
        return false;
    }

    if (player && damager == player && player->m_pAttachedTo == this)
        return false;

    bDamagedDueToFireOrExplosionOrBullet = false;
    if (CanPhysicalBeDamaged(weapon, &bDamagedDueToFireOrExplosionOrBullet)) {
        return !bDamagedDueToFireOrExplosionOrBullet || GetStatus() != STATUS_PLAYER || m_fHealth >= 250.0f;
    } else {
        return false;
    }
}

// 0x6D1340
void CVehicle::ProcessDelayedExplosion() {
    if (!m_wBombTimer) {
        return;
    }

    const auto period = (int16)(CTimer::GetTimeStep() * (100.0f / 6.0f));
    m_wBombTimer = std::max(m_wBombTimer - period, 0);

    if (!m_wBombTimer) {
        BlowUpCar(m_pWhoDetonatedMe, false);
    }
}

// NOTSA
void CVehicle::ApplyTurnForceToOccupantOnEntry(CPed* passenger) {
    // Apply some turn force
    switch (m_nVehicleType) {
    case VEHICLE_TYPE_BIKE: {
        ApplyTurnForce(
            GetUp() * passenger->m_fMass / -50.f,
            GetForward() / -10.f // Behind the bike
        );
        break;
    }
    default: {
        ApplyTurnForce(
            CVector{ .0f, .0f, passenger->m_fMass / -5.f },
            CVector{ CVector2D{passenger->GetPosition() - GetPosition()}, 0.f }
        );
        break;
    }
    }
}

// 0x6D13A0
bool CVehicle::AddPassenger(CPed* passenger) {
    ApplyTurnForceToOccupantOnEntry(passenger);

    // Now, find a seat and place them into it
    const auto seats = GetMaxPassengerSeats();
    if (const auto emptySeat = rng::find(seats, nullptr); emptySeat != seats.end()) {
        *emptySeat = passenger;
        CEntity::RegisterReference(*emptySeat);
        m_nNumPassengers++;
        return true;
    }

    // No empty seats
    return false;
}

// 0x6D14D0
bool CVehicle::AddPassenger(CPed* passenger, uint8 seatIdx) {
    if (vehicleFlags.bIsBus) {
        return AddPassenger(passenger);
    }

    // Check if seat is valid
    if (seatIdx >= m_nMaxPassengers) {
        return false;
    }

    // Check if anyone is already in that seat
    if (HasPassengerAtSeat(seatIdx)) {
        return false;
    }

    // Place passenger into seat, and add ref
    m_apPassengers[seatIdx] = passenger;
    CEntity::RegisterReference(m_apPassengers[seatIdx]);
    m_nNumPassengers++;

    return true;
}

// 0x6D1610
void CVehicle::RemovePassenger(CPed* passenger) {
    if (!passenger) {
        return;
    }

    const auto RemovePassengerFromArray = [&](auto array) { /* array by-value because it's either a span or a view */
        if (const auto seatOfPsgr = rng::find(array, passenger); seatOfPsgr != array.end()) {
            CEntity::SafeCleanUpRef(*seatOfPsgr);
            *seatOfPsgr = nullptr;

            assert(m_nNumPassengers > 0); // NOTSA: Sanity check
            m_nNumPassengers--;
        }
    };
    if (IsTrain()) {
        RemovePassengerFromArray(std::span{ m_apPassengers });
    } else {
        RemovePassengerFromArray(GetMaxPassengerSeats());
    }
}

// 0x6D16A0
void CVehicle::SetDriver(CPed* driver) {
    CEntity::ChangeEntityReference(m_pDriver, driver);

    if (vehicleFlags.bFreebies && driver == FindPlayerPed()) {
        vehicleFlags.bFreebies = false;

        switch (m_nModelIndex)
        {
        case MODEL_AMBULAN: {
            FindPlayerInfo(0).AddHealth(20);
            break;
        }
        case MODEL_TAXI:
        case MODEL_CABBIE: {
            FindPlayerInfo().m_nMoney += 12;
            break;
        }
        case MODEL_ENFORCER: {
            driver->m_fArmour = std::max((float)FindPlayerInfo(0).m_nMaxArmour, driver->m_fArmour);
            break;
        }
        case MODEL_CADDY: {
            // Pirulax:
            // `driver->IsPlayer()` check is useless here, because the precondition
            // to ever reach this code is `driver == FindPlayerPed()`
            if (!driver->IsPlayer() || driver->AsPlayer()->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_GOLFCLUB, true)) {
                CStreaming::RequestModel(MODEL_GOLFCLUB, STREAMING_GAME_REQUIRED);
            }
            break;
        }
        case MODEL_HOTDOG: {
            CStats::IncrementStat(STAT_CALORIES, 40.0f);
            break;
        }
        case MODEL_COPCARLA:
        case MODEL_COPCARSF:
        case MODEL_COPCARVG:
        case MODEL_COPCARRU: {
            CStreaming::RequestModel(MODEL_CHROMEGUN, STREAMING_GAME_REQUIRED);
            vehicleFlags.bFreebies = true;
            break;
        }
        default:
            break;
        }
    }

    ApplyTurnForceToOccupantOnEntry(driver);
}

// 0x6D1950
void CVehicle::RemoveDriver(bool dontTurnEngineOff) {
    SetStatus(STATUS_ABANDONED);

    if (!dontTurnEngineOff) {
        if (!m_pDriver || !m_pDriver->IsPlayer()) {
            vehicleFlags.bEngineOn = false;
        }
    }

    if (const auto playerPed = FindPlayerPed();  m_pDriver == playerPed) {
        switch (m_nModelIndex) {
        case MODEL_CADDY: {
            if (CStreaming::IsModelLoaded(MODEL_GOLFCLUB)) {
                if (playerPed->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_GOLFCLUB, true)) {
                    playerPed->GiveWeapon(WEAPON_GOLFCLUB, 1, true);
                }
                CStreaming::SetModelIsDeletable(MODEL_GOLFCLUB);
            }
            break;
        }
        case MODEL_COPCARLA:
        case MODEL_COPCARSF:
        case MODEL_COPCARVG:
        case MODEL_COPCARRU: {
            if (CStreaming::IsModelLoaded(MODEL_CHROMEGUN) && vehicleFlags.bFreebies) {
                if (playerPed->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_SHOTGUN, true)) {
                    playerPed->GiveWeapon(eWeaponType::WEAPON_SHOTGUN, 5, true);
                } else {
                    playerPed->GrantAmmo(eWeaponType::WEAPON_SHOTGUN, 5);
                }
                vehicleFlags.bFreebies = false;
                CStreaming::SetModelIsDeletable(MODEL_CHROMEGUN);
            }
            break;
        }
        }
    }

    CEntity::ClearReference(m_pDriver);
}

// 0x6D1A50
CPed* CVehicle::SetUpDriver(int32 gangPedType, bool createAsMale, bool createAsCriminal) {
    if (m_pDriver) {
        return m_pDriver;
    }

    if (IsCreatedBy(eVehicleCreatedBy::RANDOM_VEHICLE)) {
        CPopulation::AddPedInCar(this, true, gangPedType, 0, createAsMale, createAsCriminal);
        return m_pDriver;
    }

    return nullptr;
}

// 0x6D1AA0
CPed* CVehicle::SetupPassenger(int32 seatIdx, int32 gangPedType, bool createAsMale, bool createAsCriminal) {
    if (const auto passenger = m_apPassengers[seatIdx]) {
        return passenger;
    }

    switch (m_nModelIndex) {
    case MODEL_TAXI:
    case MODEL_CABBIE:
    case MODEL_STRETCH: {
        if (!seatIdx) {
            // RemovePassenger(m_apPassengers[0]); // Nice C*! => This does nothing, because above we've already ensured that nobody sits here!
            return nullptr;
        }
    }
    }

    const auto psgrAdded = CPopulation::AddPedInCar(this, false, gangPedType, seatIdx, createAsMale, createAsCriminal);

    const auto ShouldCheckModels = [&] {
        // unit test: https://godbolt.org/z/deqcso6WT
        switch (psgrAdded->m_nPedType) {
        case PED_TYPE_MEDIC:
        case PED_TYPE_FIREMAN:
        case PED_TYPE_COP: {
            return false;
        }
        case PED_TYPE_CRIMINAL: { // (ped_added_to_car_type != PED_TYPE_CRIMINAL || pedType < PED_TYPE_GANG8 || pedType > PED_TYPE_SPECIAL) )
            switch (gangPedType) { // pedType < PED_TYPE_GANG8 || pedType > PED_TYPE_SPECIAL)
            case PED_TYPE_GANG8:
            case PED_TYPE_GANG9:
            case PED_TYPE_GANG10:
            case PED_TYPE_DEALER:
            case PED_TYPE_MEDIC:
            case PED_TYPE_FIREMAN:
            case PED_TYPE_CRIMINAL:
            case PED_TYPE_BUM:
            case PED_TYPE_PROSTITUTE:
            case PED_TYPE_SPECIAL:
                return false;
            }
            break;
        }
        default: 
            return !IsPedTypeGang(psgrAdded->m_nPedType);
        }
        return true;
    };

    // In case of some specific ped types we make sure
    // that no occupant in the seats before the current (eg.: `seatIdx`)
    // has the same model id.
    // In case they do, the passenger that we've just added will be removed
    // and nullptr will be returned.
    if (ShouldCheckModels()) {
        const auto ProcessOccupant = [&](CPed* occupant) {
            if (occupant && occupant->m_nModelIndex == psgrAdded->m_nModelIndex) {
                RemovePassenger(psgrAdded);
                CPopulation::RemovePed(psgrAdded);
                return false;
            }
            return true;
        };

        // Not sure why this checks only up to the seat the passenger was added to, but okay.
        if (!ProcessOccupant(m_pDriver) || !rng::all_of(std::span{ m_apPassengers.data(), (size_t)seatIdx }, ProcessOccupant)) {
            return nullptr;
        }
    }

    return psgrAdded;
}

// 0x6D1BD0
bool CVehicle::IsPassenger(CPed* ped) const {
    if (!ped)
        return false;

    for (const auto& passenger : m_apPassengers) {
        if (passenger == ped) {
            return true;
        }
    }
    return false;
}

// 0x6D1C00
bool CVehicle::IsPassenger(int32 modelIndex) const {
    for (const auto& passenger : m_apPassengers) {
        if (passenger && passenger->m_nModelIndex == modelIndex) {
            return true;
        }
    }
    return false;
}

bool CVehicle::IsPedOfModelInside(eModelID model) const {
    return IsDriver(model) || IsPassenger(model);
}

bool CVehicle::IsDriver(const CPed* ped) const {
    return ped && ped == m_pDriver;
}

bool CVehicle::IsDriver(int32 modelIndex) const {
    return m_pDriver && m_pDriver->m_nModelIndex == modelIndex;
}

/*!
* @addr 0x6D1C80
* @brief Kill all peds in the vehicle, and dispatch an event as if they were killed by an explosion
*/
void CVehicle::KillPedsInVehicle() {
    const auto ProcessOccupant = [this](CPed* occupant) {
        if (occupant) {
            if (!CGameLogic::IsCoopGameGoingOn()) {
                CDarkel::RegisterKillByPlayer(*occupant, WEAPON_EXPLOSION, false, 0);
            }
            CEventVehicleDied event{ this };
            occupant->GetIntelligence()->m_eventGroup.Add(&event);
        }
    };

    ProcessOccupant(m_pDriver);
    rng::for_each(GetMaxPassengerSeats(), ProcessOccupant);
}

// 0x6D1D90
bool CVehicle::IsUpsideDown() const {
    return m_matrix->GetUp().z <= -0.9f;
}

// 0x6D1DD0
bool CVehicle::IsOnItsSide() const {
    return m_matrix->GetRight().z >= 0.8f || m_matrix->GetRight().z <= -0.8f;
}

// 0x6D1E20
bool CVehicle::CanPedOpenLocks(const CPed* ped) const {
    switch (m_nDoorLock) {
    case CARLOCK_LOCKED:
    case CARLOCK_COP_CAR:
    case CARLOCK_LOCKED_PLAYER_INSIDE:
    case CARLOCK_SKIP_SHUT_DOORS:
        return false;
    case CARLOCK_LOCKOUT_PLAYER_ONLY:
        return !ped->IsPlayer();
    default:
        return true;
    }
}

// 0x6D1E60
bool CVehicle::CanDoorsBeDamaged() const {
    // TODO: ranges::contains({...}, m_nDoorLock)
    switch (m_nDoorLock) {
    case CARLOCK_NOT_USED:
    case CARLOCK_UNLOCKED:
    case CARLOCK_SKIP_SHUT_DOORS:
        return true;
    default:
        return false;
    }
}

// 0x6D1E80
bool CVehicle::CanPedEnterCar() {
    const auto upZ = GetUp().z;
    if (IsBike() || upZ > 0.1f || upZ < -0.1f) {
        return true;
    }

    return m_vecTurnSpeed.SquaredMagnitude() <= sq(0.2f) &&
           m_vecMoveSpeed.SquaredMagnitude() <= sq(0.2f);
}

// 0x6D21F0
void CVehicle::ProcessCarAlarm() {
    switch (m_nAlarmState) {
    case 0:
    case std::numeric_limits<decltype(m_nAlarmState)>::max(): { // Doing this in case we ever the underlying type.
        return;
    }
    }

    const auto ts = (uint16)CTimer::GetTimeStepInMS();
    if (m_nAlarmState >= ts) {
        m_nAlarmState = ts;
    } else {
        m_nAlarmState = 0;
        m_HornCounter = 0;
    }
}

// 0x6D2250 - cdecl in the exe (the vehicle is a stack argument, there is no `this`)
void DestroyVehicleAndDriverAndPassengers(CVehicle* vehicle) {
    const auto ProcessOccupant = [](CPed* occupant) {
        if (occupant) {
            if (!CGameLogic::IsCoopGameGoingOn()) {
                CDarkel::RegisterKillByPlayer(*occupant, WEAPON_UNIDENTIFIED, false, 0);
            }
            occupant->FlagToDestroyWhenNextProcessed();
        }
    };

    ProcessOccupant(vehicle->m_pDriver);
    rng::for_each(vehicle->GetMaxPassengerSeats(), ProcessOccupant);

    CWorld::Remove(vehicle);
    delete vehicle;
}

// NOTSA: Member form kept for existing callers (e.g. `CGarage`), forwards to the free function above
void CVehicle::DestroyVehicleAndDriverAndPassengers(CVehicle* vehicle) {
    ::DestroyVehicleAndDriverAndPassengers(vehicle);
}

// 0x6D22F0
bool CVehicle::IsVehicleNormal() {
    if (m_pDriver
        && !m_nNumPassengers
        && GetStatus() != STATUS_WRECKED
        && GetVehicleModelInfo()->m_nVehicleClass != VEHICLE_CLASS_IGNORE
    ) {
        return true;
    }
    return false;
}

// 0x6D2330
void CVehicle::ChangeLawEnforcerState(bool bIsEnforcer) {
    if (bIsEnforcer) {
        if (!vehicleFlags.bIsLawEnforcer) {
            vehicleFlags.bIsLawEnforcer = true;
            ++CCarCtrl::NumLawEnforcerCars;
        }
    }
    else if (vehicleFlags.bIsLawEnforcer){
        vehicleFlags.bIsLawEnforcer = false;
        --CCarCtrl::NumLawEnforcerCars;
    }
}

// 0x6D2370
bool CVehicle::IsLawEnforcementVehicle() const {
    switch (m_nModelIndex) {
    case MODEL_ENFORCER:
    case MODEL_PREDATOR:
    case MODEL_RHINO:
    case MODEL_BARRACKS:
    case MODEL_FBIRANCH:
    case MODEL_COPBIKE:
    case MODEL_FBITRUCK:
    case MODEL_COPCARLA:
    case MODEL_COPCARSF:
    case MODEL_COPCARVG:
    case MODEL_COPCARRU:
    case MODEL_SWATVAN:
        return true;
    default:
        return false;
    }
}

// unused
// 0x6D2450
bool CVehicle::ShufflePassengersToMakeSpace() {
    return true;
}

// 0x6D2460
void CVehicle::ExtinguishCarFire() {
    if (GetStatus() != STATUS_WRECKED) {
        if (m_fHealth <= 300.0f)
            m_fHealth = 300.0f;
    }

    if (m_pFire) {
        m_pFire->SetIsScript(false);
        m_pFire->Extinguish();
        m_pFire = nullptr;
    }

    if (IsAutomobile()) {
        if (AsAutomobile()->m_damageManager.GetEngineStatus() >= 225) {
            AsAutomobile()->m_damageManager.SetEngineStatus(215);
        }
        AsAutomobile()->m_fBurnTimer = 0.0f;
    }
}

// 0x6D24F0
void CVehicle::ActivateBomb() {
    switch (m_nBombOnBoard) {
    case BOMB_TIMED_NOT_ACTIVATED: {
        m_nBombOnBoard = BOMB_TIMED_ACTIVATED;
        m_wBombTimer = 7000;
        m_pWhoDetonatedMe = FindPlayerPed();
        break;
    }
    case BOMB_IGNITION: {
        m_nBombOnBoard = BOMB_IGNITION_ACTIVATED;
        break;
    }
    default: {
        return;
    }
    }
    CGarages::TriggerMessage("GA_12", -1, 3000u); // "Bomb armed"
}

// 0x6D2570
void CVehicle::ActivateBombWhenEntered() {
    if (m_pDriver) {
        if (!vehicleFlags.bDriverLastFrame && m_nBombOnBoard == BOMB_IGNITION_ACTIVATED) { // If the driver just entered and there's an ignition bomb...
            m_wBombTimer = 1000;
            m_pWhoDetonatedMe = m_pWhoInstalledBombOnMe; // NOTE: `m_pWhoInstalledBombOnMe` isn't set in `ActivateBomb` weird...
            CEntity::RegisterReference(m_pWhoDetonatedMe);
        }
    }
    vehicleFlags.bDriverLastFrame = m_pDriver != nullptr;
}

// 0x6D25D0
bool CVehicle::CarHasRoof() {
    return !m_pHandlingData->m_bConvertible || !m_anExtras[0] || !m_anExtras[1];
}

// 0x6D2600
float CVehicle::HeightAboveCeiling(float height, eFlightModel flightModel) {
    switch (flightModel) {
    case eFlightModel::FLIGHT_MODEL_BARON: {
        if (height >= 500.f) {
            if (height < 950.f) {
                return height - 500.f;
            }

            if (height >= 1500.f) {
                return (height - 1000.f) + 500.f;
            }
        }
        return -1.f;
    }
    default: {
        // Originally this was the condition used, but it's ugly
        // Leaving here to make sure it all works as expectd
        assert(!(((int)flightModel - 1) <= 1));

        if (height < 800.f)
            return -1.f;
        return height - 800.f;
    }
    }
}

// 0x6D2690
RwObject* SetVehicleAtomicVisibilityCB(RwObject* object, void* data) {
    assert(RwObjectGetType(object) == rpATOMIC);
    const auto atomic      = reinterpret_cast<RpAtomic*>(object);
    const auto toSet = std::bit_cast<eAtomicComponentFlag>(data);
    if (const auto current = CVisibilityPlugins::GetUserValue(atomic) & ATOMIC_MASK; current != ATOMIC_NONE) {
        RpAtomicSetFlags(atomic, current != toSet ? 0 : rpATOMICRENDER);
    }
    return object;
}

// 0x6D26D0
RwFrame* SetVehicleAtomicVisibilityCB(RwFrame* frame, void* data) {
    RwFrameForAllObjects(frame, SetVehicleAtomicVisibilityCB, data);
    RwFrameForAllChildren(frame, SetVehicleAtomicVisibilityCB, data);
    return frame;
}

// 0x6D2700
void CVehicle::SetComponentVisibility(RwFrame* component, uint32 visibilityState) { // see eAtomicComponentFlag
    assert(visibilityState == ATOMIC_NONE || visibilityState == ATOMIC_OK || visibilityState == ATOMIC_DAMAGED);
    if (component) {
        if (visibilityState == eAtomicComponentFlag::ATOMIC_DAMAGED) {
            vehicleFlags.bIsDamaged = true;
        }
        RwFrameForAllObjects(component, SetVehicleAtomicVisibilityCB, (void*)visibilityState);
        RwFrameForAllChildren(component, SetVehicleAtomicVisibilityCB, (void*)visibilityState);
    }
}

// 0x6D2740
void CVehicle::ApplyBoatWaterResistance(tBoatHandlingData* boatHandling, float fImmersionDepth) {
    float fSpeedMult = sq(fImmersionDepth) * m_pHandlingData->m_fSuspensionForceLevel * m_fMass / 1000.0F;
    if (m_nModelIndex == MODEL_SKIMMER) {
        fSpeedMult *= 30.0F;
    }

    auto fMoveDotProduct = DotProduct(m_vecMoveSpeed, GetForward());
    fSpeedMult *= sq(fMoveDotProduct) + 0.05F;
    fSpeedMult += 1.0F;
    fSpeedMult = std::fabs(fSpeedMult);
    fSpeedMult = 1.0F / fSpeedMult;

    float fUsedTimeStep = CTimer::GetTimeStep() * 0.5F;
    auto vecSpeedMult = Pow(boatHandling->m_vecMoveRes * fSpeedMult, fUsedTimeStep);

    CVector vecMoveSpeedMatrixDotProduct = GetMatrix().InverseTransformVector(m_vecMoveSpeed);
    m_vecMoveSpeed = vecMoveSpeedMatrixDotProduct * vecSpeedMult;

    auto fMassMult = (vecSpeedMult.y - 1.0F) * m_vecMoveSpeed.y * m_fMass;
    CVector vecTransformedMoveSpeed = GetMatrix().TransformVector(m_vecMoveSpeed);
    m_vecMoveSpeed = vecTransformedMoveSpeed;

    auto vecDown = GetUp() * -1.0F;
    auto vecTurnForce = GetForward() * fMassMult;
    ApplyTurnForce(vecTurnForce, vecDown);

    if (m_vecMoveSpeed.z <= 0.0F)
        m_vecMoveSpeed.z *= ((1.0F - vecSpeedMult.z) * 0.5F + vecSpeedMult.z);
    else
        m_vecMoveSpeed.z *= vecSpeedMult.z;
}

// 0x6D2950
RpMaterial* SetCompAlphaCB(RpMaterial* material, void* data) {
    material->color.alpha = static_cast<uint8>(reinterpret_cast<uintptr_t>(data)); // 0x6D2950: byte store at +7 (RwRGBA::alpha)
    return material;
}

// 0x6D2960
void CVehicle::SetComponentAtomicAlpha(RpAtomic* atomic, int32 alpha) {
    auto geometry = atomic->geometry;
    geometry->flags |= rpGEOMETRYMODULATEMATERIALCOLOR;
    RpGeometryForAllMaterials(geometry, SetCompAlphaCB, reinterpret_cast<void*>(alpha));
}

CVehicleModelInfo* CVehicle::GetVehicleModelInfo() const {
    return CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
}

CVector CVehicle::GetDummyPositionObjSpace(eVehicleDummy dummy) const {
    return GetVehicleModelInfo()->GetModelDummyPosition(dummy);
}

// if bWorldSpace is true, returns the position in world-space
// otherwise in model-space
CVector CVehicle::GetDummyPosition(eVehicleDummy dummy, bool bWorldSpace) {
    CVector pos = GetDummyPositionObjSpace(dummy);
    if (bWorldSpace)
        pos = GetMatrix().TransformPoint(pos); // transform to world-space
    return pos;
}

// 0x6D2980
void CVehicle::UpdateClumpAlpha() {
    const auto GetAlphaToSet = [this] {
        const auto curr = CVisibilityPlugins::GetClumpAlpha(GetRpClump());
        if (vehicleFlags.bFadeOut) {
            return std::max(0, curr - 8);
        } else if (curr < 255) {
            return std::min(255, curr + 16);
        }
        return 255;
    };
    CVisibilityPlugins::SetClumpAlpha(GetRpClump(), GetAlphaToSet());
}

// 0x6D29E0
void CVehicle::UpdatePassengerList() {
    // No sure what's the point of this
    // It checks if there should be any passengers
    // If there's none only then it sets the number of them to 0.. weird.
    if (m_nNumPassengers) {
        if (rng::all_of(m_apPassengers, [](auto&& p) { return p == nullptr; })) {
            m_nNumPassengers = 0;
        }
    }
}

// 0x6D2A10
CPed* CVehicle::PickRandomPassenger() {
    // TODO: Add a function for this to random.hpp

    const auto rnd = CGeneral::GetRandomNumberInRange(0u, std::size(m_apPassengers));
    for (auto i = 0u; i < std::size(m_apPassengers); i++) {
        if (const auto psgr = m_apPassengers[(rnd + i) % std::size(m_apPassengers)]) {
            return psgr;
        }
    }

    return nullptr;
}

// 0x6D2A80
void CVehicle::AddDamagedVehicleParticles() {
    if (IsSubPlane())
        return;

    if (m_fHealth >= 650.0f || m_fHealth < 250.0f || physicalFlags.bSubmergedInWater) {
        FxSystem_c::SafeKillAndClear(m_pOverheatParticle);
        return;
    }

    auto* const matrix = GetRpClump()
        ? RwFrameGetMatrix(RpClumpGetFrame(GetRpClump()))
        : nullptr;

    if (!m_pOverheatParticle && matrix) {
        m_pOverheatParticle = g_fxMan.CreateFxSystem(
            m_pHandlingData->m_transmissionData.m_nEngineType == 'E'
                ? "overheat_car_electric"
                : "overheat_car",
            GetVehicleModelInfo()->GetModelDummyPosition(DUMMY_ENGINE),
            matrix,
            false
        );
        if (m_pOverheatParticle) {
            m_pOverheatParticle->Play();
        }
    }

    if (m_pOverheatParticle) {
        m_pOverheatParticle->SetConstTime(1u, 1.0f - (m_fHealth - 250.0f) / 400.0f);
        CVector velocity = m_vecMoveSpeed * 50.0f;
        m_pOverheatParticle->SetVelAdd(velocity);
    }
}

// 0x6D2BF0
void CVehicle::MakeDirty(CColPoint& colPoint) {
    if (g_surfaceInfos.IsWater(colPoint.m_nSurfaceTypeB) || CWeather::IsRainy()) {
        if (m_fDirtLevel <= 1.0f) {
            return;
        }
        m_fDirtLevel = m_fDirtLevel - CTimer::ms_fTimeStep * 0.01f;
        return;
    }

    if (g_surfaceInfos.MakesCarDirty(colPoint.m_nSurfaceTypeB)) {
        if (m_vecMoveSpeed.Magnitude2D() <= 0.06f) {
            return;
        }
        m_fDirtLevel = std::clamp(m_fDirtLevel + CTimer::GetTimeStep() * 0.003f, m_fDirtLevel, 15.0f); // todo: check
        return;
    }

    if (g_surfaceInfos.MakesCarClean(colPoint.m_nSurfaceTypeB)) {
        if (m_vecMoveSpeed.Magnitude2D() <= 0.04f || m_fDirtLevel <= 4.0f) {
            return;
        }
        m_fDirtLevel = std::clamp(m_fDirtLevel - CTimer::GetTimeStepInSeconds(), 4.0f, m_fDirtLevel); // todo: check
    }
}

// 0x6D2D50
bool CVehicle::AddWheelDirtAndWater(CColPoint& colPoint, bool isProduceWheelDrops, bool isWheelsSpinning, bool isWheelInWater) {
    if (!isProduceWheelDrops && !g_surfaceInfos.IsSand(colPoint.m_nPieceTypeB)) {
        return false;
    }

    if (isWheelInWater) {
        g_fx.AddWheelSpray(this, colPoint.m_vecPoint, isWheelsSpinning, true, m_fContactSurfaceBrightness);
        return false;
    }

    const auto CreateFxForSurface = [&](auto CheckSurface, auto AddFx) {
        if ((g_surfaceInfos.*CheckSurface)(colPoint.m_nSurfaceTypeB)) {
            (g_fx.*AddFx)(this, colPoint.m_vecPoint, isWheelsSpinning, m_fContactSurfaceBrightness);
            return true;
        }
        return false;
    };
    if (CreateFxForSurface(&SurfaceInfos_c::CreatesWheelGrass,  &Fx_c::AddWheelGrass)) {
        return false;
    }
    if (CreateFxForSurface(&SurfaceInfos_c::CreatesWheelGravel, &Fx_c::AddWheelGravel)) {
        return true;  // The only odd one, wonder why
    }
    if (CreateFxForSurface(&SurfaceInfos_c::CreatesWheelMud, &Fx_c::AddWheelMud)) {
        return false;
    }
    if (CWeather::WetRoads <= 0.0 || CGeneral::GetRandomNumberInRange(CWeather::WetRoads, 1.01f) <= 0.5f) {
        if (CreateFxForSurface(&SurfaceInfos_c::CreatesWheelDust, &Fx_c::AddWheelDust)) {
            return false;
        }
        if (CreateFxForSurface(&SurfaceInfos_c::CreatesWheelSand, &Fx_c::AddWheelSand)) {
            return false;
        }
    }
    if (CWeather::WetRoads > 0.4f && !CCullZones::CamNoRain()) {
        if (g_surfaceInfos.CreatesWheelSpray(colPoint.m_nPieceTypeB)) {
            g_fx.AddWheelSpray(this, colPoint.m_vecPoint, isWheelsSpinning, false, m_fContactSurfaceBrightness);
            return false;
        }
    }
    return true;
}

// 0x6D3000
void CVehicle::SetGettingInFlags(uint8 doorId) {
    m_nGettingInFlags |= doorId;
}

// 0x6D3020
void CVehicle::SetGettingOutFlags(uint8 doorId) {
    m_nGettingOutFlags |= doorId;
}

// 0x6D3040
void CVehicle::ClearGettingInFlags(uint8 doorId) {
    m_nGettingInFlags &= ~doorId;
}

// 0x6D3060
void CVehicle::ClearGettingOutFlags(uint8 doorId) {
    m_nGettingOutFlags &= ~doorId;
}

// 0x6D3080
void CVehicle::SetWindowOpenFlag(uint8 doorId) {
    auto frameFromId = CClumpModelInfo::GetFrameFromId(GetRpClump(), doorId);
    if (frameFromId) {
        RwFrameForAllObjects(frameFromId, CVehicleModelInfo::SetAtomicFlagCB, (void*)eAtomicComponentFlag::ATOMIC_DONT_RENDER_ALPHA);
    }
}

// 0x6D30B0
void CVehicle::ClearWindowOpenFlag(uint8 doorId) {
    auto frameFromId = CClumpModelInfo::GetFrameFromId(GetRpClump(), doorId);
    if (frameFromId) {
        RwFrameForAllObjects(frameFromId, CVehicleModelInfo::ClearAtomicFlagCB, (void*)eAtomicComponentFlag::ATOMIC_DONT_RENDER_ALPHA);
    }
}

// 0x6D30E0
bool CVehicle::SetVehicleUpgradeFlags(int32 upgradeModelIndex, int32 modId, int32& resultModelIndex) {
    // At the one and only place this function is called from
    // componentIndex == CModelInfo::GetModelInfo(upgradeModelIndex)->AsVehicleModelInfo().CarMod
    // Now, I'm not sure what value it has, so..

    switch (modId) {
    case 16: {
        if (handlingFlags.bHydraulicInst) {
            resultModelIndex = upgradeModelIndex;
        }

        handlingFlags.bHydraulicInst = true;
        m_nFakePhysics = false;
        m_vecMoveSpeed.z = 0.0f;

        return true;
    }
    case 15: {
        if (!IsAutomobile()) {
            return false;
        }

        const auto GetNitroValue = [&]() -> int8 {
            if (upgradeModelIndex == ModelIndices::MI_NITRO_BOTTLE_LARGE) {
                return 5;
            } else if (upgradeModelIndex == ModelIndices::MI_NITRO_BOTTLE_DOUBLE) {
                return 10;
            }
            return 2;
        };

        if (handlingFlags.bNosInst) {
            resultModelIndex = ModelIndices::MI_NITRO_BOTTLE_SMALL;
        }

        AsAutomobile()->NitrousControl(GetNitroValue());

        return GetModelInfo()->AsVehicleModelInfoPtr()->m_pVehicleStruct->m_aUpgrades[15].m_nParentComponentId < 0;
    }
    case 17: {
        if (m_vehicleAudio.m_AuSettings.RadioType != AE_RT_CIVILIAN || vehicleFlags.bUpgradedStereo) {
            resultModelIndex = upgradeModelIndex;
            return true;
        }

        auto& bs = m_vehicleAudio.m_AuSettings.BassSetting;
        switch (bs) {
        case eBassSetting::CUT:    bs = eBassSetting::NORMAL; break;
        case eBassSetting::NORMAL: bs = eBassSetting::BOOST;  break;
        case eBassSetting::BOOST:  return true;
        }
        AudioEngine.SetRadioBassSetting(bs);

        vehicleFlags.bUpgradedStereo = true;

        return true;
    }
    default: {
        return false;
    }
    }
}

// 0x6D3210
bool CVehicle::ClearVehicleUpgradeFlags(int32 arg0, int32 modId) {
    // See `SetVehicleUpgradeFlags` for a comment on what `componentIndex` is

    switch (modId) {
    case 17: { // 0x6D3270
        if (m_vehicleAudio.m_AuSettings.RadioType != AE_RT_CIVILIAN && vehicleFlags.bUpgradedStereo) {
            auto& bs = m_vehicleAudio.m_AuSettings.BassSetting;
            switch (bs) {
            case eBassSetting::BOOST:  bs = eBassSetting::NORMAL; break;
            case eBassSetting::NORMAL: bs = eBassSetting::CUT;    break;
            }
            AudioEngine.SetRadioBassSetting(bs);
            vehicleFlags.bUpgradedStereo = false;
        }
        return true;
    }
    case 15: { // 0x6D32C6
        if (!IsAutomobile()) {
            return false;
        }

        AsAutomobile()->NitrousControl(-1);

        return GetModelInfo()->AsVehicleModelInfoPtr()->m_pVehicleStruct->m_aUpgrades[15].m_nParentComponentId < 0;
    }
    case 16: { // 0x6D321C
        if (handlingFlags.bHydraulicInst) {
            auto& specColIdx = m_vehicleSpecialColIndex;
            if (specColIdx > -1) {
                m_aSpecialColVehicle[specColIdx] = nullptr;
                specColIdx = -1;

                SetupSuspensionLines();
                m_nFakePhysics = false;
                m_vecMoveSpeed.z = .02f;
            }
        }
        handlingFlags.bHydraulicInst = false;
        return true;
    }
    default: {
        return false;
    }
    }
}

// 0x6D3300
RpAtomic* RemoveUpgradeCB(RpAtomic* atomic, void* data) {
    // NOTE: `data` is the upgrade id passed BY VALUE (not a pointer) - see `CVehicle::RemoveUpgrade`
    if (CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_UPGRADE) {
        const auto mi = CVisibilityPlugins::GetModelInfo(atomic);
        if (static_cast<int32>(reinterpret_cast<intptr_t>(data)) == mi->CarMod && !mi->bUsesVehDummy) {
            const auto frame = RpAtomicGetFrame(atomic);
            RpClumpRemoveAtomic(RpAtomicGetClump(atomic), atomic);
            RpAtomicDestroy(atomic);
            RwFrameDestroy(frame);
            if (mi) {
                mi->RemoveRef();
            }
        }
    }
    return atomic;
}

// 0x6D3370
RpAtomic* FindUpgradeCB(RpAtomic* atomic, void* data) {
    struct tFindUpgradeData { int32 upgradeId; RpAtomic* atomic; };
    const auto d = static_cast<tFindUpgradeData*>(data);
    if (CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_UPGRADE) {
        const auto mi = CVisibilityPlugins::GetModelInfo(atomic);
        if (d->upgradeId == mi->CarMod) {
            d->atomic = atomic;
            return nullptr; // stop iteration
        }
    }
    return atomic;
}

// 0x6D33B0
RwObject* RemoveObjectsCB(RwObject* object, void* data) {
    if (RwObjectGetType(object) != rpATOMIC) {
        return object;
    }

    const auto atomic = reinterpret_cast<RpAtomic*>(object);

    // NOTSA: The original overwrites (4 bytes of) whatever `data` points to with the atomic's flags
    // (The only caller passes a pointer to a `RwFrame*` => It's overwritten)
    const auto flags = CVisibilityPlugins::GetAtomicId(atomic);
    *static_cast<int32*>(data) = flags;

    if (flags & ATOMIC_UPGRADE) {
        return object;
    }

    const auto mi    = CVisibilityPlugins::GetModelInfo(atomic);
    const auto frame = RpAtomicGetFrame(atomic);
    RpClumpRemoveAtomic(RpAtomicGetClump(atomic), atomic);
    RpAtomicDestroy(atomic);
    if (!CVisibilityPlugins::GetFrameHierarchyId(frame)) {
        RwFrameDestroy(frame);
    }
    if (mi) {
        mi->RemoveRef();
    }

    return object;
}

// 0x6D3420
RwFrame* RemoveObjectsCB(RwFrame* frame, void* data) {
    RwFrameForAllObjects(frame, RemoveObjectsCB, data);
    RwFrameForAllChildren(frame, RemoveObjectsCB, data);
    return frame;
}

// 0x6D3450
static auto& CopyObjectsCB_TargetClump = StaticRef<RpClump*>(0xC1CB58);
RwObject* CopyObjectsCB(RwObject* object, void* data) {
    const auto frame = (RwFrame*)data;

    if (RwObjectGetType(object) == rpATOMIC) {
        const auto atomic = (RpAtomic*)object;
        const auto clone = RpAtomicClone(atomic);
        RpClumpAddAtomic(CopyObjectsCB_TargetClump, clone);
        RpAtomicSetFrame(clone, frame);
    }

    return object;
}

// 0x6D3490
RwObject* FindReplacementUpgradeCB(RwObject* object, void* data) {
    if (RwObjectGetType(object) == rpATOMIC) {
        const auto atomic = reinterpret_cast<RpAtomic*>(object);
        if (!(CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_UPGRADE)) {
            if (CVisibilityPlugins::GetModelInfoIndex(atomic) != -1) {
                static_cast<tCompSearchStructById*>(data)->m_pFrame = reinterpret_cast<RwFrame*>(object); // (sic) the original stores the atomic here
                return nullptr; // stop iteration
            }
        }
    }
    return object;
}

// 0x6D34D0
RpAtomic* RemoveAllUpgradesCB(RpAtomic* atomic, void* data) {
    auto* mi = CVisibilityPlugins::GetModelInfo(atomic);
    if (mi) {
        RpClumpRemoveAtomic(atomic->clump, atomic);
        RpAtomicDestroy(atomic);
        mi->RemoveRef();
    }
    return atomic;
}

// From [0x6D35BC - 0x6D3611]
static void SetupUpgradeAtomicRendering(RpAtomic* atomic, bool isDamaged) {
    RpMaterial* hasAlphaMaterial = nullptr;
    RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), CVehicleModelInfo::HasAlphaMaterialCB, &hasAlphaMaterial);
    if (hasAlphaMaterial) {
        CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB);
        CVisibilityPlugins::SetAtomicFlag(atomic, ATOMIC_ALPHA);
    } else {
        CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB);
    }

    CVehicleModelInfo::SetRenderPipelinesCB(atomic, nullptr);
}

// 0x6D3510
RpAtomic* CVehicle::CreateUpgradeAtomic(CBaseModelInfo* mi, const UpgradePosnDesc* upgradePosn, RwFrame* parentComponent, bool isDamaged) {
    if (isDamaged) {
        CDamageAtomicModelInfo::ms_bCreateDamagedVersion = true;
    }

    const auto atomic = reinterpret_cast<RpAtomic*>(mi->CreateInstance()); // Trust me, CreateInstace returns an atomic in this case
    const auto frame = RpAtomicGetFrame(atomic);
    const auto mat   = RwFrameGetMatrix(frame);

    // Update it's position
    upgradePosn->m_qRotation.Get(mat);
    RwV3dAssign(RwMatrixGetPos(mat), &upgradePosn->m_vPosition);
    RwMatrixUpdate(mat);

    // Update us and parent frame
    RpClumpAddAtomic(GetRpClump(), atomic);
    RwFrameAddChild(parentComponent, frame);

    mi->AddRef();

    CVisibilityPlugins::SetAtomicId(atomic, isDamaged ? eAtomicComponentFlag::ATOMIC_DAMAGED : eAtomicComponentFlag::ATOMIC_OK);
    CVisibilityPlugins::SetAtomicFlag(atomic, eAtomicComponentFlag::ATOMIC_UPGRADE);
    CVisibilityPlugins::SetAtomicFlag(atomic, eAtomicComponentFlag::ATOMIC_DONT_CULL);

    SetupUpgradeAtomicRendering(atomic, isDamaged);

    CDamageAtomicModelInfo::ms_bCreateDamagedVersion = false;
    return atomic;
}

// 0x6D3630
void CVehicle::RemoveUpgrade(int32 upgradeId) {
    // NOTE: the id is passed BY VALUE (RemoveUpgradeCB compares `data` itself), NOT as `&upgradeId`
    RpClumpForAllAtomics(GetRpClump(), RemoveUpgradeCB, reinterpret_cast<void*>(static_cast<intptr_t>(upgradeId)));
}

// 0x6D3650
int32 CVehicle::GetUpgrade(int32 upgradeId) {
    struct { int32 upgradeId; RpAtomic* atomic; } data = { upgradeId, nullptr };
    RpClumpForAllAtomics(GetRpClump(), FindUpgradeCB, &data);
    if (data.atomic) {
        return CVisibilityPlugins::GetModelInfoIndex(data.atomic);
    }

    switch (upgradeId) {
    case 15:
        if (handlingFlags.bNosInst) {
            return ModelIndices::MI_NITRO_BOTTLE_SMALL;
        }
        break;
    case 16:
        if (handlingFlags.bHydraulicInst) {
            return ModelIndices::MI_HYDRAULICS;
        }
        break;
    case 17:
        if (vehicleFlags.bUpgradedStereo) {
            return ModelIndices::MI_STEREO_UPGRADE;
        }
        break;
    }
    return -1;

}

// 0x6D3700
RpAtomic* CVehicle::CreateReplacementAtomic(CBaseModelInfo* mi, RwFrame* parentFrame, eAtomicComponentFlag flags, bool isDamaged, bool bIsWheel) {
    if (isDamaged) {
        CDamageAtomicModelInfo::ms_bCreateDamagedVersion = true;
    }

    const auto atomic = reinterpret_cast<RpAtomic*>(mi->CreateInstance());
    const auto frame = RpAtomicGetFrame(atomic);

    mi->AddRef();

    // Update us and parent frame
    RpClumpAddAtomic(GetRpClump(), atomic);

    if (bIsWheel) {
        const auto mat = RwFrameGetMatrix(frame);
        CMatrix::GetIdentity().UpdateRwMatrix(mat);
        mat->flags |= rwMATRIXINTERNALIDENTITY | rwMATRIXTYPEMASK;
        CVisibilityPlugins::SetFrameHierarchyId(frame, 0);
        RwFrameAddChild(parentFrame, frame);
    } else {
        RpAtomicSetFrame(atomic, parentFrame);
        RwFrameDestroy(frame);
    }

    CVisibilityPlugins::SetAtomicId(atomic, flags & ~eAtomicComponentFlag::ATOMIC_MASK);
    CVisibilityPlugins::SetAtomicFlag(atomic, isDamaged ? eAtomicComponentFlag::ATOMIC_DAMAGED : eAtomicComponentFlag::ATOMIC_OK);

    SetupUpgradeAtomicRendering(atomic, isDamaged);

    CDamageAtomicModelInfo::ms_bCreateDamagedVersion = false;

    return atomic;
}

// 0x6D3830
void CVehicle::AddReplacementUpgrade(int32 modelIndex, int32 nodeId) {
    const auto frame = CClumpModelInfo::GetFrameFromId(GetRpClump(), nodeId);

    // Remove everything that's currently attached to the frame, remember the (last) atomic's flags
    int32 oldAtomicFlags = 0;
    RwFrameForAllObjects(frame, RemoveObjectsCB, &oldAtomicFlags);
    RwFrameForAllChildren(frame, RemoveObjectsCB, &oldAtomicFlags);

    const auto mi = CModelInfo::GetModelInfo(modelIndex);
    const auto atomic = reinterpret_cast<RpAtomic*>(mi->CreateInstance());
    mi->AddRef();

    const auto atomicFrame = RpAtomicGetFrame(atomic);
    RpClumpAddAtomic(GetRpClump(), atomic);
    RpAtomicSetFrame(atomic, frame);
    RwFrameDestroy(atomicFrame);

    CVisibilityPlugins::SetAtomicId(atomic, oldAtomicFlags & ~ATOMIC_MASK);
    CVisibilityPlugins::SetAtomicFlag(atomic, ATOMIC_OK);

    SetupUpgradeAtomicRendering(atomic, false); // NOTE: 0x6D38D5 - 0x6D390B

    CDamageAtomicModelInfo::ms_bCreateDamagedVersion = false;

    if (nodeId == CAR_EXHAUST) {
        if (m_pHandlingData->m_bDoubleExhaust) {
            const auto second = CreateReplacementAtomic(mi, frame, (eAtomicComponentFlag)oldAtomicFlags, false, true);
            const auto mat = RwFrameGetMatrix(RpAtomicGetFrame(second));
            mat->pos.x = (float)((double)RwFrameGetMatrix(frame)->pos.x * -2.0f); // 0x858B18 = -2.0f
            RwMatrixUpdate(mat);
        }
    } else if (nodeId == CAR_BUMP_FRONT || nodeId == CAR_BUMP_REAR) {
        const auto vmi = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
        CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade(GetRpClump(), vmi->m_pPlateMaterial, vmi->m_nPlateType);
    }

    if (mi->AsDamageAtomicModelInfoPtr()) {
        CreateReplacementAtomic(mi, frame, (eAtomicComponentFlag)oldAtomicFlags, true, false);
        if (frame) {
            RwFrameForAllObjects(frame, SetVehicleAtomicVisibilityCB, (void*)ATOMIC_OK);
            RwFrameForAllChildren(frame, SetVehicleAtomicVisibilityCB, (void*)ATOMIC_OK);
        }
    }
}

// 0x6D39E0
void CVehicle::RemoveReplacementUpgrade(int32 frameId) {
    auto frameOfUpgrade = CClumpModelInfo::GetFrameFromId(GetRpClump(), frameId);
    RwFrameForAllObjects(frameOfUpgrade, RemoveObjectsCB, &frameOfUpgrade);
    RwFrameForAllChildren(frameOfUpgrade, RemoveObjectsCB, &frameOfUpgrade);

    CopyObjectsCB_TargetClump = GetRpClump();
    RwFrameForAllObjects(
        CClumpModelInfo::GetFrameFromId(GetModelInfo()->GetRpClump(), frameId),
        CopyObjectsCB,
        frameOfUpgrade
    );
}

// 0x6D3A50
int32 CVehicle::GetReplacementUpgrade(int32 nodeId) {
    auto frame = CClumpModelInfo::GetFrameFromId(GetRpClump(), nodeId);
    tCompSearchStructById data = { nodeId, nullptr };
    RwFrameForAllObjects(frame, FindReplacementUpgradeCB, &data);
    if (data.m_pFrame)
        return CVisibilityPlugins::GetModelInfoIndex((RpAtomic*)data.m_pFrame);
    else
        return -1;
}

// 0x6D3AB0
void CVehicle::RemoveAllUpgrades() {
    RpClumpForAllAtomics(GetRpClump(), RemoveAllUpgradesCB, nullptr);
    m_anUpgrades.fill(-1);
}

// 0x6D3AE0
int32 CVehicle::GetSpareHasslePosId() const {
    const auto numberOfPositions = [&] {
        switch (m_nVehicleSubType) {
        case eVehicleType::VEHICLE_TYPE_BIKE:
        case eVehicleType::VEHICLE_TYPE_BMX:
        case eVehicleType::VEHICLE_TYPE_QUAD:
            return 2;
        case eVehicleType::VEHICLE_TYPE_AUTOMOBILE:
            return 6;
        default:
            return 0;
        }
    }();

    for (auto i = 0; i < numberOfPositions; i++) {
        if ((m_nHasslePosId  & (1 << i)) == 0) {
            return i;
        }
    }

    return -1;
}

// 0x6D3B30
void CVehicle::SetHasslePosId(int32 hasslePos, bool enable) {
    if (enable) {
        m_nHasslePosId |= 1 << hasslePos;
    } else {
        m_nHasslePosId ^= 1 << hasslePos;
    }
}

// 0x6D3B60
void CVehicle::InitWinch(int32 winchType) {
    m_ropeType = winchType;
}

// 0x6D3B80
void CVehicle::UpdateWinch() {
    if (!m_ropeType) {
        return;
    }

    const auto ropeID = GetRopeID();

    const auto GetZAndSegmentCount = [&, this]() -> std::pair<float, uint32> {
        const auto baseLen = (eRopeType)m_ropeType == eRopeType::MAGNET ? -0.2f : -0.6f;
        if (const auto ropeIdx = CRopes::FindRope(ropeID); ropeIdx >= 0) { // Inverted condition
            const auto& rope = CRopes::GetRope(ropeIdx);
            const auto segCount = (uint32)(rope.m_fSegmentLength * 32.f);
            return { (rope.m_fMass * rope.m_fSegmentLength) - ((float)segCount * rope.m_fTotalLength) + baseLen, segCount };
        }
        return { baseLen, 0 };
    };

    const auto [pointZ, segCount] = GetZAndSegmentCount();
    CRopes::RegisterRope(
        ropeID,
        m_ropeType,
        m_matrix->TransformPoint(CVector{ 0.f, 0.f, pointZ }),
        false,
        segCount,
        1u,
        this,
        20'000u
    );
}

// 0x6D3C70
void CVehicle::RemoveWinch() {
    // NOTE: This function is not correct, as in
    //       the original code uses `&this[29]` as the rope index (and not `GetRopeID()`).
    //       BUT it's not used anywhere, so..

    NOTSA_UNREACHABLE("Unused function");

    if (const auto ropeIdx = GetRopeID(); ropeIdx >= 0) {
        CRopes::GetRope(ropeIdx).Remove();
    }

    m_ropeType = 0;
}

CVector CVehicle::GetDriverSeatDummyPositionOS() const {
    return GetDummyPositionObjSpace(
        IsBoat() ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_SEAT_FRONT
    );
}

CVector CVehicle::GetDriverSeatDummyPositionWS() {
    return GetMatrix().TransformPoint(GetDriverSeatDummyPositionOS());
}

CVehicleAnimGroup& CVehicle::GetAnimGroup() const {
    return CVehicleAnimGroupData::GetVehicleAnimGroup(m_pHandlingData->m_nAnimGroup);
}

AssocGroupId CVehicle::GetAnimGroupId() const {
    return m_pHandlingData->GetAnimGroupId();
}

// 0x6D3CB0
void CVehicle::ReleasePickedUpEntityWithWinch() const {
    return CRopes::GetRope(GetRopeID()).ReleasePickedUpObject();
}

// 0x6D3CD0
void CVehicle::PickUpEntityWithWinch(CEntity* entity) const {
    return CRopes::GetRope(GetRopeID()).PickUpObject(entity);
}

// 0x6D3CF0
CEntity* CVehicle::QueryPickedUpEntityWithWinch() const {
    return CRopes::GetRope(GetRopeID()).m_pRopeAttachObject;
}

// 0x6D3D10
float CVehicle::GetRopeHeightForHeli() const {
    return CRopes::GetRope(GetRopeID()).m_fSegmentLength;
}

// 0x6D3D30
void CVehicle::SetRopeHeightForHeli(float height) const {
    CRopes::GetRope(GetRopeID()).m_fSegmentLength = height;
}

// 0x6D3D60
void CVehicle::RenderDriverAndPassengers() {
    if (m_pDriver && m_pDriver->m_nPedState == PEDSTATE_DRIVING) {
        m_pDriver->Render();
    }

    for (auto& passenger : m_apPassengers) {
        if (passenger && passenger->m_nPedState == PEDSTATE_DRIVING) {
            passenger->Render();
        }
    }
}

// 0x6D3DB0
void CVehicle::PreRenderDriverAndPassengers() {
    if (m_pDriver && m_pDriver->m_nPedState == PEDSTATE_DRIVING) {
        m_pDriver->PreRenderAfterTest();
    }

    for (auto& passenger : m_apPassengers) {
        if (passenger && passenger->m_nPedState == PEDSTATE_DRIVING) {
            passenger->PreRenderAfterTest();
        }
    }
}

// 0x6D3E00
float CVehicle::GetPlaneGunsAutoAimAngle() {
    switch (m_nModelIndex) {
    case MODEL_SEASPAR:  return 10.0f;
    case MODEL_RUSTLER:
    case MODEL_MAVERICK:
    case MODEL_POLMAV:
    case MODEL_HYDRA:
    case MODEL_CARGOBOB:
    case MODEL_TORNADO:  return 15.0f;
    case MODEL_RCTIGER:  return 20.0f;
    case MODEL_HUNTER:   return 25.0f;
    case MODEL_RCBARON:  return 30.0f;
    default:
        return 0.0f;
    }
}

// 0x6D3F30
int32 CVehicle::GetPlaneNumGuns() {
    switch (m_nModelIndex) {
    case MODEL_HUNTER:
    case MODEL_SEASPAR:
    case MODEL_RCBARON:
    case MODEL_MAVERICK:
    case MODEL_POLMAV:
    case MODEL_CARGOBOB:
        return 1;
    case MODEL_RUSTLER:
        return 6;
    case MODEL_HYDRA:
    case MODEL_TORNADO:
        return 2;
    default:
        return 0;
    }
}

// 0x6D4010
void CVehicle::SetFiringRateMultiplier(float multiplier) {
    multiplier = std::clamp(multiplier, 0.0f, 15.9375f);
    switch (m_nVehicleSubType) {
    case VEHICLE_TYPE_PLANE:
        AsPlane()->m_nFiringMultiplier = uint8(multiplier * 16.0f);
        break;
    case VEHICLE_TYPE_HELI:
        AsHeli()->m_nFiringMultiplier = uint8(multiplier * 16.0f);
        break;
    }
}

// 0x6D4090
float CVehicle::GetFiringRateMultiplier() {
    switch (m_nVehicleSubType) {
    case VEHICLE_TYPE_PLANE:
        return float(AsPlane()->m_nFiringMultiplier) / 16.0f;
    case VEHICLE_TYPE_HELI:
        return float(AsHeli()->m_nFiringMultiplier) / 16.0f;
    default:
        return 1.0f;
    }
}

// 0x6D40E0
uint32 CVehicle::GetPlaneGunsRateOfFire() {
    const auto mult = GetFiringRateMultiplier();
    switch (m_nModelIndex) {
    case MODEL_SEASPAR:
    case MODEL_RCBARON:
        return uint32(40.0f / mult);
    case MODEL_RUSTLER:
        return uint32(80.0f / mult);
    case MODEL_HYDRA:
        return uint32(17.0f / mult);
    case MODEL_CARGOBOB:
        return uint32(100.0f / mult);
    case MODEL_TORNADO:
        return uint32(45.0f / mult);
    default:
        return uint32(60.0f / mult);
    }
}

// 0x6D4290
CVector CVehicle::GetPlaneGunsPosition(int32 gunId) {
    const auto pos = GetModelInfo()->AsVehicleModelInfoPtr()->GetModelDummyPosition(DUMMY_VEHICLE_GUN);
    if (!pos.IsZero()) {
        return pos;
    }

    switch (m_nModelIndex) {
    case MODEL_HUNTER:   return VehicleGunOffset[0];
    case MODEL_SEASPAR:  return { -0.5f, 2.4f, -0.785f }; // 0x8D35F8
    case MODEL_RCBARON:  return { 0.0f, 0.45f, 0.0f };    // 0x8D3634
    case MODEL_RUSTLER:  return CVector{ 2.19f, 1.5f, -0.58f } + CVector{ 0.2f, 0.0f, 0.0f } * (float)(gunId - 1);  // 0x8D3610 (posn), 0x8D361C (offset)
    case MODEL_MAVERICK: return { 0.0f, 2.85f, -0.5f };   // 0x8D35E0
    case MODEL_POLMAV:   return { 0.0f, 2.85f, -0.5f };   // 0x8D35EC [Values same as the maverick's]
    case MODEL_HYDRA:    return { 1.48f, 0.44f, -0.52f }; // 0x8D3628
    case MODEL_CARGOBOB: return { 0.0f, 6.87f, -1.65f };  // 0x8D3604
    case MODEL_TORNADO:  return {};                       // 0xC1CC2C
    default:
        return { 0.f, 0.f, 0.f };
    }
}

// 0x6D4590
uint32 CVehicle::GetPlaneOrdnanceRateOfFire(eOrdnanceType type) {
    const auto mult = GetFiringRateMultiplier();
    switch (m_nModelIndex) {
    case MODEL_HUNTER:
        return (uint32)((float)500 / mult);
    case MODEL_SEASPAR:
    case MODEL_RUSTLER:
        return (uint32)((float)1000 / mult);
    case MODEL_HYDRA:
        return (uint32)((float)(type != 1 ? 1000 : 500) / mult); // todo: heat trap / missile?
    default:
        return (uint32)((float)350 / mult);
    }
}

// 0x6D46E0
CVector CVehicle::GetPlaneOrdnancePosition(eOrdnanceType type) {
    // CVector result;
    // ((void(__thiscall*)(CVehicle*, CVector*, eOrdnanceType))0x6D46E0)(this, &result, type);
    // return result;

    const auto pos = GetVehicleModelInfo()->GetModelDummyPosition(DUMMY_VEHICLE_GUN);
    if (!pos.IsZero()) {
        return pos;
    }

    constexpr CVector HUNTER_ORDNANCE_POS  = { 2.17f, 1.00f, -0.80f }; // 0x8D3640
    constexpr CVector RUSTLER_ORDNANCE_POS = { 2.19f, 1.50f, -0.58f }; // 0x8D364C
    constexpr CVector HYDRA_1_ORDNANCE_POS = { 3.70f, 0.98f, -1.02f }; // 0x8D3658
    constexpr CVector HYDRA_2_ORDNANCE_POS = { 3.92f, 0.98f, -1.02f }; // 0x8D3664

    constexpr CVector SEASPAR_ORDNANCE_POS = { }; // 0xC1CC38;
    constexpr CVector RCBARON_ORDNANCE_POS = { }; // 0xC1CC50;
    constexpr CVector TORNADO_ORDNANCE_POS = { }; // 0xC1CC44;

    switch (m_nModelIndex) {
    case MODEL_HUNTER:  return HUNTER_ORDNANCE_POS;
    case MODEL_SEASPAR: return SEASPAR_ORDNANCE_POS;
    case MODEL_RCBARON: return RCBARON_ORDNANCE_POS;
    case MODEL_RUSTLER: return RUSTLER_ORDNANCE_POS;
    case MODEL_HYDRA:
        if (type == 1) {
            return HYDRA_1_ORDNANCE_POS;
        } else if (type == 2) {
            return HYDRA_2_ORDNANCE_POS;
        }
        return pos;
    case MODEL_TORNADO: return TORNADO_ORDNANCE_POS;
    default:
        return {};
    }
}

// 0x6D4900
void CVehicle::SelectPlaneWeapon(bool bChange, eOrdnanceType type) {
    const auto GetWeaponToUse = [&, this] {
        switch (m_nModelIndex) {
        case MODEL_TORNADO: // Originally a separate case at the bottom, but since they both do the same, I moved it here.
        case MODEL_HUNTER:
            if (type == 1) {
                return CAR_WEAPON_DOUBLE_ROCKET;
            } else {
                return bChange ? CAR_WEAPON_HEAVY_GUN : m_nVehicleWeaponInUse;
            }
        case MODEL_SEASPAR:
        case MODEL_RCBARON:
            return bChange ? CAR_WEAPON_HEAVY_GUN : m_nVehicleWeaponInUse;

        case MODEL_RUSTLER:
            if (type == 1) {
                return CAR_WEAPON_FREEFALL_BOMB;
            } else {
                return bChange ? CAR_WEAPON_HEAVY_GUN : m_nVehicleWeaponInUse;
            }
        case MODEL_HYDRA: {
            switch (type) {
            case 1:
                return CAR_WEAPON_DOUBLE_ROCKET;
            case 2:
                return CAR_WEAPON_LOCK_ON_ROCKET;
            default:
                return bChange ? CAR_WEAPON_HEAVY_GUN : m_nVehicleWeaponInUse;
            }
        }
        default:
            return m_nVehicleWeaponInUse;
        }
    };
    m_nVehicleWeaponInUse = GetWeaponToUse();
}

// 0x6D4AD0
void CVehicle::DoPlaneGunFireFX(CWeapon* weapon, CVector& particlePos, CVector& gunshellPos, int32 fxIdx) {
    const auto DoFx = [&](auto& parts) {
        if (!parts) {
            const auto nguns = GetPlaneNumGuns();
            parts = new FxSystem_c*[nguns];
            rng::fill(std::span{ parts, (size_t)nguns }, nullptr);
        }

        if (parts) {
            auto& part = parts[fxIdx];
            if (!part) {
                part = g_fxMan.CreateFxSystem(
                    "gunflash",
                    CVector{0.f, 0.f, 0.f},
                    RwFrameGetMatrix(RpClumpGetFrame(GetRpClump())),
                    false
                );
            }
            if (part) {
                part->SetOffsetPos(particlePos);
                part->Play();
            }
        }

        if (s_bPlaneGunsEjectShellCasings) {
            weapon->AddGunshell(this, gunshellPos, { 0.f, 0.1f }, 0.5f);
        }

        CPointLights::AddLight(0, gunshellPos, { 0.f, 0.f, 0.f }, 3.f, 0.5f, 0.4f, 0.f);
    };


    switch (m_nVehicleSubType) {
    case VEHICLE_TYPE_PLANE: {
        DoFx(AsPlane()->m_pGunParticles);
        break;
    }
    case VEHICLE_TYPE_HELI: {
        DoFx(AsHeli()->m_ppGunflashFx); // original reads [heli + 0xA0C]
        break;
    }
    default: {
        return;
    }
    }
}

// 0x6D4D30
void CVehicle::FirePlaneGuns() {
    if (!GetPlaneNumGuns()) {
        return;
    }

    if (CTimer::GetTimeInMS() <= m_nGunFiringTime + GetPlaneGunsRateOfFire()) {
        return;
    }

    struct ModelGunInfo {
        bool        isDoubleGun{};
        uint8       freq{};
        eWeaponType weap{};
    };
    const auto GetModelGunInfo = [this]() -> std::optional<ModelGunInfo> {
        switch (m_nModelIndex) {
        case MODEL_HUNTER:
        case MODEL_CARGOBOB:
            return ModelGunInfo{ false, 160u, WEAPON_MINIGUN };
        case MODEL_SEASPAR:
        case MODEL_RCBARON:
        case MODEL_MAVERICK:
        case MODEL_POLMAV:
            return ModelGunInfo{ false, 92u, WEAPON_M4 };
        case MODEL_RUSTLER: {
            if (m_nGunsCycleIndex + 1 == 3) {
                m_nGunsCycleIndex = 0;
            } else {
                m_nGunsCycleIndex++;
            }
            return ModelGunInfo{ true, 225u, WEAPON_M4 };
        }
        case MODEL_HYDRA:
        case MODEL_TORNADO:
            return ModelGunInfo{ true, 123u, WEAPON_MINIGUN };
        default:
            return std::nullopt;
        }
    };

    const auto ginfo = GetModelGunInfo();
    if (!ginfo.has_value()) {
        return;
    }

    const auto [giIsDoubleGun, giFreq, giWeaponType] = *ginfo;

    CWeapon weapon{ giWeaponType, 5000 };

          auto planeGunPosMS  = GetPlaneGunsPosition(m_nGunsCycleIndex); // MS = Model Space
    const auto velocityOffset = m_vecMoveSpeed * CTimer::GetTimeStep();
          auto gunShellPos    = velocityOffset +  m_matrix->TransformPoint(planeGunPosMS);

    const auto FireGun = [&](const auto gunId) {
        DoPlaneGunFireFX(
            &weapon,
            planeGunPosMS,
            gunShellPos,
            gunId
        );
        weapon.FireInstantHit(this, &gunShellPos, &gunShellPos);
    };

    if (giIsDoubleGun) {
        // NOTE: There might be a bug in the original code, being that
        //       the gun position (`planeGunPosMS`) is not recalculated for the second gun
        //       although, the gunId in `GetPlaneGunsPosition` is only used for the "rustler"
        for (auto i = 0u; i < 2; i++) {
            FireGun(m_nGunsCycleIndex * 2 + i);
        }
    } else {
        FireGun(m_nGunsCycleIndex);
    }

    // Shake pad (if necessary)
    const auto GetPadIdToShake = [this]() -> std::optional<int32> {
        if (GetStatus() == STATUS_REMOTE_CONTROLLED) {
            return 0;
        }

        if (m_pDriver && m_pDriver->IsPlayer()) {
            return m_pDriver->GetPadNumber();
        }

        return std::nullopt;
    };
    if (const auto padId = GetPadIdToShake()) {
        CPad::GetPad(*padId)->StartShake(240, giFreq, 0);
    }

    AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE_PLANE, giWeaponType == WEAPON_MINIGUN ? WEAPON_M4 : giWeaponType, this); // Unsure about why the change the M4 => minigun but okay

    m_nGunFiringTime = CTimer::GetTimeInMS();
}

// 0x6D5110
void CVehicle::FireUnguidedMissile(eOrdnanceType type, bool bCheckTime) {
    auto& firingTimeForOrdnanceType = type == 1 ? m_nProjectileWeaponFiringTime : m_nAdditionalProjectileWeaponFiringTime;

    if (bCheckTime) {
        if (CTimer::GetTimeInMS() <= firingTimeForOrdnanceType + GetPlaneOrdnanceRateOfFire(type)) {
            return;
        }
    }

    switch (m_nModelIndex) {
    case MODEL_HUNTER:
    case MODEL_HYDRA:
    case MODEL_TORNADO:
        break;
    default:
        return;
    }

    CWeapon weapon{ WEAPON_RLAUNCHER, 5000 };

    for (auto i = 0; i < 2; i++) {
        const auto ordnancePos = m_matrix->TransformPoint(GetPlaneOrdnancePosition(type));
        // This places a point somewhere in front of us, depending on our velocity's direction
        auto origin = ordnancePos + m_matrix->GetForward() * (std::max(0.f, DotProduct(m_matrix->GetForward(), m_vecMoveSpeed)) * CTimer::GetTimeStep());
        weapon.FireProjectile(this, origin);
    }

    if (m_pDriver && m_pDriver->IsPlayer()) {
        CPad::GetPad(m_pDriver->GetPadNumber())->StartShake(240, 160u, 0);
    }

    firingTimeForOrdnanceType = CTimer::GetTimeInMS();
}

// 0x6D5400
bool CVehicle::CanBeDriven() const {
    if (IsSubTrailer() || IsSubTrain() && AsTrain()->m_nTrackId || vehicleFlags.bIsRCVehicle) {
        return false;
    }
    return GetDriverSeatDummyPositionOS().SquaredMagnitude() > 0.0f;
}

// 0x6D5490
void CVehicle::ReactToVehicleDamage(CPed* dmgCauser) {
    const auto DoReact = [=](CPed* pedThatLooks, CEntity* pedToLookAt) {
        g_ikChainMan.LookAt("ReactToVhclDam", pedThatLooks, pedToLookAt, CGeneral::GetRandomNumberInRange(2000, 5000), BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
    };

    const auto& passenger = m_apPassengers[0];

    if (m_pDriver) {
        if (passenger) {
            DoReact(m_pDriver, CGeneral::DoCoinFlip() ? dmgCauser : passenger);
        } else {
            DoReact(m_pDriver, dmgCauser);
        }
    }

    if (passenger) {
        DoReact(passenger, CGeneral::DoCoinFlip() ? dmgCauser : m_pDriver);
    }
}

// 0x6D55C0
bool CVehicle::GetVehicleLightsStatus() {
    // I've changed the logic flow a little bit so avoid the usage of variables
    // 0x6D566A - This branch overwrites everything, so test it first
    if (   m_pDriver
        && IsPedTypeGang(m_pDriver->m_nPedType)
        && m_pDriver->m_nRandomSeed % 2
        && CPopCycle::m_bCurrentZoneIsGangArea
    ) {
        return false; // Real OG' don't use lights! Vo�l�!
    }

    // The rest pretty much follows the original code

    if (CClock::GetIsTimeInRange(21, 6)) {
        return true;
    }

    if (CClock::GetGameClockHours() == 20 && CClock::GetGameClockMinutes() > (m_nRandomSeed % 64)) {
        return true;
    }
    if (CClock::GetGameClockHours() == 6 && CClock::GetGameClockMinutes() < (m_nRandomSeed % 64)) {
        return true;
    }

    if (const auto treshold = (float)m_nRandomSeed / 50'000.f; CWeather::Foggyness > treshold || CWeather::WetRoads > treshold) {
        return true;
    }

    return m_fContactSurfaceBrightness < 0.05f && CCullZones::CamNoRain();
}

// 0x6D5CF0
bool CVehicle::CanPedLeanOut(CPed* ped) {
    switch (m_pHandlingData->m_nAnimGroup) {
    case ANIM_GROUP_COLT45:
        return notsa::contains(std::array{ m_pDriver, m_apPassengers[0] }, ped);
    case ANIM_GROUP_COLT_COP:
    case ANIM_GROUP_COLT45PRO:
    case ANIM_GROUP_SAWNOFF:
    case ANIM_GROUP_SAWNOFFPRO:
    case ANIM_GROUP_SILENCED:
        return false;
    default: {
        switch (m_nVehicleSubType) {
        case VEHICLE_TYPE_HELI:
        case VEHICLE_TYPE_PLANE:
        case VEHICLE_TYPE_TRAIN:
        case VEHICLE_TYPE_BOAT:
            return false;
        default:
            return true;
        }
    }
    }
}

// 0x6D5D70
void CVehicle::SetVehicleCreatedBy(eVehicleCreatedBy createdBy) {
    if (GetCreatedBy() != createdBy) {
        CCarCtrl::UpdateCarCount(this, true);
        m_nCreatedBy = createdBy;
        CCarCtrl::UpdateCarCount(this, false);
    }
}

// unknown
float CVehicle::GetNewSteeringAmt() {
    return 0.0f;
}

// 0x6D64F0
void CVehicle::SetupRender() {
    const auto mi = GetModelInfo()->AsVehicleModelInfoPtr();

    RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(TRUE));

    if (IsAutomobile()) {
        AsAutomobile()->CustomCarPlate_BeforeRenderingStart(*mi);
    }

    // Handle remap TXD
    if (m_nRemapTxd < 0) {
        vehicleFlags.bDontSetColourWhenRemapping = false;
    } else {
        // Make sure it's loaded, if not, request it to be loaded
        if (CStreaming::IsModelLoaded(TXDToModelId(m_nRemapTxd))) {
            // If there was a remap texture set, remove it
            if (m_pRemapTexture) {
                m_pRemapTexture = nullptr;
                CTxdStore::RemoveRef(m_nPreviousRemapTxd);
            }

            // Add ref to current txd
            CTxdStore::AddRef(m_nRemapTxd);

            m_nPreviousRemapTxd = m_nRemapTxd;

            // Set this to -1. From the looks of it, this
            // should be set during the frame or something
            // and then it's loaded here
            // instead of having to load the txd and all that mid-frame
            m_nRemapTxd = -1;

            // Store texture of current txd
            m_pRemapTexture = GetFirstTexture(CTxdStore::GetTxd(m_nPreviousRemapTxd));

            if (!vehicleFlags.bDontSetColourWhenRemapping) {
                m_nPrimaryColor = 1;
            }
        } else {
            CStreaming::RequestModel(TXDToModelId(m_nRemapTxd), STREAMING_KEEP_IN_MEMORY);
        }
    }

    mi->SetVehicleColour(
        m_nPrimaryColor,
        m_nSecondaryColor,
        m_nTertiaryColor,
        m_nQuaternaryColor
    );

    CVehicleModelInfo::SetupLightFlags(this);
    CVehicleModelInfo::ms_pRemapTexture = m_pRemapTexture;
    CVehicleModelInfo::SetEditableMaterials(GetRpClump());
}

// 0x6D6C00
void CVehicle::ProcessWheel(CVector& wheelFwd, CVector& wheelRight,
                            CVector& wheelContactSpeed, CVector& wheelContactPoint,
                            int32 wheelsOnGround,
                            float thrust, float brake, float adhesion,
                            int8 wheelId, float* wheelSpeed,
                            tWheelState* wheelState, uint16 wheelStatus
) {
    static auto& bBraking = StaticRef<bool>(0xC1CDAE); // false
    static auto& bDriving = StaticRef<bool>(0xC1CDAD); // false
    static auto& bAlreadySkidding = StaticRef<bool>(0xC1CDAC); // false

    float right = 0.0f;
    float fwd = 0.0f;
    float contactSpeedFwd = DotProduct(wheelFwd, wheelContactSpeed);
    float contactSpeedRight = DotProduct(wheelRight, wheelContactSpeed);

    bBraking = brake != 0.0f;
    bDriving = !bBraking;
    if (bDriving && thrust == 0.0f)
        bDriving = false;

    adhesion *= CTimer::GetTimeStep();
    if (*wheelState != WHEEL_STATE_NORMAL) {
        bAlreadySkidding = true;
        adhesion *= m_pHandlingData->m_fTractionLoss;
        if (*wheelState == WHEEL_STATE_SPINNING) {
            if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED)
                adhesion *= (1.0f - fabs(m_GasPedal) * WS_ALREADY_SPINNING_LOSS);
        }
    }

    *wheelState = WHEEL_STATE_NORMAL;

    if (contactSpeedRight != 0.0f) {
        right = -(contactSpeedRight / wheelsOnGround);
        if (wheelStatus == WHEEL_STATUS_BURST) {
            float fwdspeed = std::min(contactSpeedFwd, fBurstSpeedMax);
            right += fwdspeed * CGeneral::GetRandomNumberInRange(-fBurstTyreMod, fBurstTyreMod) ;
        }
    }

    if (bDriving) {
        fwd = thrust;
        right = std::clamp(right, -adhesion, adhesion);
    }
    else if (contactSpeedFwd != 0.0f) {
        fwd = -contactSpeedFwd / wheelsOnGround;
        if (!bBraking && std::fabs(m_GasPedal) < 0.01f) {
            if (IsBike())
                brake = gHandlingDataMgr.fWheelFriction * 0.6f / (m_pHandlingData->m_fMass + 200.0f);
            else if (IsSubPlane())
                brake = 0.0f;
            else {
                brake = gHandlingDataMgr.fWheelFriction / m_pHandlingData->m_fMass;

                if (brake > 500.0f)
                    brake *= 0.1f;
                else if (m_nModelIndex == MODEL_RCBANDIT)
                    brake *= 0.2f;
            }
        }
        if (brake > adhesion) {
            if (std::fabs(contactSpeedFwd) > 0.005f) {
                *wheelState = WHEEL_STATE_FIXED;
            }
        } else {
            fwd = std::clamp(fwd, -brake, brake);
        }
    }

    float speedSq = right * right + fwd * fwd;
    if (speedSq > adhesion * adhesion) {
        if (*wheelState != WHEEL_STATE_FIXED) {
            float tractionLimit = WS_TRAC_FRAC_LIMIT;
            if (contactSpeedFwd > 0.15f && (!wheelId || wheelId == CAR_WHEEL_FRONT_RIGHT)) {
                tractionLimit += tractionLimit;
            }
            if (bDriving && tractionLimit * adhesion < std::fabs(fwd))
                *wheelState = WHEEL_STATE_SPINNING;
            else
                *wheelState = WHEEL_STATE_SKIDDING;
        }
        float tractionLoss = m_pHandlingData->m_fTractionLoss;
        if (bAlreadySkidding) {
            tractionLoss = 1.0f;
        } else if (*wheelState == WHEEL_STATE_SPINNING) {
            if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED) {
                tractionLoss = tractionLoss * (1.0f - std::fabs(m_GasPedal) * WS_ALREADY_SPINNING_LOSS);
            }
        }
        float l = sqrt(speedSq);
        fwd *= adhesion * tractionLoss / l;
        right *= adhesion * tractionLoss / l;
    }

    if (fwd != 0.0f || right != 0.0f) {
        bool separateTurnForce = false;
        CVector totalSpeed = fwd * wheelFwd + right * wheelRight;
        CVector turnDirection  = totalSpeed;

        if (m_pHandlingData->m_fSuspensionAntiDiveMultiplier > 0.0f) {
            if (bBraking) {
                separateTurnForce = true;
                turnDirection -= (m_pHandlingData->m_fSuspensionAntiDiveMultiplier * wheelFwd * fwd);
            }
            else if (bDriving) {
                separateTurnForce = true;
                turnDirection -= (0.5f * m_pHandlingData->m_fSuspensionAntiDiveMultiplier * wheelFwd * fwd);
            }
        }

        CVector direction = totalSpeed;
        float speed = totalSpeed.Magnitude();
        float turnSpeed = speed;
        if (separateTurnForce)
            turnSpeed = turnDirection.Magnitude();
        direction.Normalise();
        if (separateTurnForce)
            turnDirection.Normalise();
        else
            turnDirection = direction;

        float force = speed * m_fMass;
        float turnForce = turnSpeed * GetMass(wheelContactPoint, turnDirection);
        ApplyMoveForce(force * direction);
        ApplyTurnForce(turnForce * turnDirection, wheelContactPoint);
    }
}

// 0x6D73B0
void CVehicle::ProcessBikeWheel(CVector& wheelFwd, CVector& wheelRight, CVector& wheelContactSpeed, CVector& wheelContactPoint, int32 wheelsOnGround, float thrust, float brake,
                                float adhesion, float destabTraction, int8 wheelId, float* wheelSpeed, tWheelState* wheelState, eBikeWheelSpecial special, uint16 wheelStatus) {
    // NOTE: The original keeps float intermediates on the x87 stack (53-bit precision), hence the `double`s below.
    static auto& bBraking         = StaticRef<bool>(0xC1CDB2);
    static auto& bDriving         = StaticRef<bool>(0xC1CDB1);
    static auto& bReversing       = StaticRef<bool>(0xC1CDB0); // thrust < 0 (only set while not braking)
    static auto& bAlreadySkidding = StaticRef<bool>(0xC1CDAF); // BUG: never reset to false by the original

    const float contactSpeedFwd = (float)((double)wheelFwd.y * wheelContactSpeed.y + (double)wheelFwd.z * wheelContactSpeed.z + (double)wheelFwd.x * wheelContactSpeed.x);

    float  fwdOut = 0.f;
    double right  = 0.0;

    if (brake == 0.f) {
        bBraking   = false;
        bDriving   = thrust != 0.f;
        bReversing = thrust < 0.f;
    } else {
        bBraking   = true;
        bDriving   = false;
        bReversing = false;
    }

    if (*wheelState != WHEEL_STATE_NORMAL) {
        bAlreadySkidding = true;
    }
    const bool alreadySkidding = bAlreadySkidding;
    *wheelState = WHEEL_STATE_NORMAL;

    adhesion = CTimer::GetTimeStep() * adhesion;
    if (alreadySkidding) {
        adhesion = adhesion * m_pHandlingData->m_fTractionLoss;
    }

    if (special != 2 && special != 3) {
        const float contactSpeedRight = (float)((double)wheelRight.z * wheelContactSpeed.z + (double)wheelRight.y * wheelContactSpeed.y + (double)wheelRight.x * wheelContactSpeed.x);
        if (contactSpeedRight != 0.f) {
            right = -((double)contactSpeedRight / wheelsOnGround);
            if (wheelStatus == WHEEL_STATUS_BURST) {
                const float  rightF  = (float)right;
                const float  speed   = contactSpeedFwd < fBurstBikeSpeedMax ? contactSpeedFwd : fBurstBikeSpeedMax;
                const double lo      = -(double)fBurstBikeTyreMod;
                const double rnd     = (double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL;
                right = (rnd * ((double)fBurstBikeTyreMod - lo) + lo) * speed + rightF;
            }
        }
    }

    if (bDriving) {
        fwdOut = thrust;
        if (!(right > 0.0)) {
            const float negAdh = -adhesion;
            if (right < negAdh) {
                right = negAdh;
            }
        } else if (right > adhesion) {
            right = adhesion;
        }
    } else if (contactSpeedFwd != 0.f) {
        fwdOut = (float)-((double)contactSpeedFwd / wheelsOnGround);

        double brakeVal = brake;
        if (!bBraking && std::fabs(m_GasPedal) < 0.01f) {
            const auto fric = gHandlingDataMgr.fWheelFriction;
            const auto mass = m_pHandlingData->m_fMass;
            if (m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                if (fwdOut > -0.05f && fwdOut < 0.05f) {
                    brakeVal = ((double)fric * 0.5f) / ((double)mass + 200.0f);
                }
            } else if (m_nVehicleType == VEHICLE_TYPE_BIKE) {
                brakeVal = ((double)fric * 0.6f) / ((double)mass + 200.0f);
            } else {
                brakeVal = (double)fric / mass;
                if (mass < 500.f || m_nModelIndex == MODEL_RCBANDIT) {
                    brakeVal *= 0.2f;
                }
            }
        }

        if (!(brakeVal <= adhesion)) {
            if (std::fabs(contactSpeedFwd) > 0.005f) {
                *wheelState = WHEEL_STATE_FIXED;
            }
        } else if (fwdOut > 0.f) {
            if (fwdOut > brakeVal) {
                fwdOut = (float)brakeVal;
            }
        } else {
            const double negBrake = -brakeVal;
            if (fwdOut < negBrake) {
                fwdOut = (float)negBrake;
            }
        }
    }

    const float adhesionSq = adhesion * adhesion;
    const double sumSq     = right * right + (double)fwdOut * fwdOut;
    const float  sumSqF    = (float)sumSq;
    if (!(sumSq <= adhesionSq)) {
        if (*wheelState != WHEEL_STATE_FIXED) {
            *wheelState = (bDriving && contactSpeedFwd < 0.1f) ? WHEEL_STATE_SPINNING : WHEEL_STATE_SKIDDING;
        }
        const float tractionLoss = alreadySkidding ? 1.f : m_pHandlingData->m_fTractionLoss;
        const double scale = ((double)tractionLoss / std::sqrt((double)sumSqF)) * adhesion;
        fwdOut = (float)(fwdOut * scale);
        right  = right * scale;
        if (destabTraction < 1.f) {
            right *= destabTraction;
        }
    } else if (destabTraction < 1.f) {
        if (!alreadySkidding) {
            destabTraction = destabTraction * m_pHandlingData->m_fTractionLoss;
        }
        if ((double)adhesionSq * destabTraction * destabTraction < sumSqF) {
            right = (((double)adhesion * destabTraction) / std::sqrt((double)sumSqF)) * right;
        }
    }

    if (fwdOut == 0.f && right == 0.0) {
        return;
    }

    // Total force vector
    const float fx = fwdOut * wheelFwd.x;
    const float fy = fwdOut * wheelFwd.y;
    const float fz = fwdOut * wheelFwd.z;
    const float rx = (float)(right * wheelRight.x);
    const float ry = (float)(right * wheelRight.y);
    CVector force{
        rx + fx,
        ry + fy,
        (float)(right * wheelRight.z + fz) // z product stays unrounded in the original
    };
    const CVector forceBeforeNorm = force;
    { // 0x59C910 - the sum of squares and the reciprocal stay in the FPU registers (extended precision); the shared `CVector::Normalise` rounds them to float
        const double sumSq = (double)force.x * force.x + (double)force.y * force.y + (double)force.z * force.z;
        if (sumSq <= 0.0) {
            force.x = 1.f;
        } else {
            const double recip = 1.0 / std::sqrt(sumSq);
            force.x = (float)(force.x * recip);
            force.y = (float)(force.y * recip);
            force.z = (float)(force.z * recip);
        }
    }

    const float mag = (float)std::sqrt((double)forceBeforeNorm.z * forceBeforeNorm.z + (double)forceBeforeNorm.y * forceBeforeNorm.y + (double)forceBeforeNorm.x * forceBeforeNorm.x);
    // 0x59C730 - the products stay in the FPU registers (extended precision); the shared `CrossProduct` rounds them to float
    const CVector cp{
        (float)((double)wheelContactPoint.y * force.z - (double)wheelContactPoint.z * force.y),
        (float)((double)wheelContactPoint.z * force.x - (double)wheelContactPoint.x * force.z),
        (float)((double)wheelContactPoint.x * force.y - (double)wheelContactPoint.y * force.x)
    };
    const double cpSq = ((double)cp.x * cp.x + (double)cp.y * cp.y) + (double)cp.z * cp.z;
    const double moveScale = (double)m_fMass * mag;
    const double turnScale = (1.0 / (cpSq / m_fTurnMass + 1.0 / m_fMass)) * mag;

    ApplyMoveForce(CVector{ (float)(force.x * moveScale), (float)(force.y * moveScale), (float)(force.z * moveScale) }); // 0x5429F0
    const CVector turnForce{ (float)(force.x * turnScale), (float)(force.y * turnScale), (float)(force.z * turnScale) };

    const auto& R = m_matrix->GetRight();
    const auto& F = m_matrix->GetForward();
    const float turnRight = (float)(((double)turnForce.z * R.z + (double)turnForce.y * R.y) + (double)turnForce.x * R.x);
    const float cpRight   = (float)(((double)R.y * wheelContactPoint.y + (double)R.z * wheelContactPoint.z) + (double)wheelContactPoint.x * R.x);
    const float cpFwd     = (float)(((double)F.y * wheelContactPoint.y + (double)F.z * wheelContactPoint.z) + (double)wheelContactPoint.x * F.x);

    if (wheelId != 1 || (!bBraking && !bReversing)) {
        const double ax = (double)cpRight * R.x;
        const double ay = (double)cpRight * R.y;
        const float  az = cpRight * R.z;
        const CVector point{
            (float)(wheelContactPoint.x - ax),
            (float)(wheelContactPoint.y - ay),
            (float)(wheelContactPoint.z - az)
        };
        const double bx = (double)turnRight * R.x;
        const double by = (double)turnRight * R.y;
        const float  bz = turnRight * R.z;
        const float  tx = (float)(turnForce.x - bx);
        const CVector f{
            tx * fTweakBikeWheelTurnForce,
            (float)((turnForce.y - by) * fTweakBikeWheelTurnForce),
            (float)((turnForce.z - (double)bz) * fTweakBikeWheelTurnForce)
        };
        ApplyTurnForce(f, point); // 0x542A50
    }

    ApplyTurnForce(
        CVector{ turnRight * R.x, turnRight * R.y, turnRight * R.z },
        CVector{ cpFwd * F.x, cpFwd * F.y, cpFwd * F.z }
    );
}

// 0x6D7BC0
auto CVehicle::FindTyreNearestPoint(CVector2D point) -> eNearestCarWheel {
    const auto relativePt = point - GetPosition2D();
    const bool isFront = relativePt.Dot(GetForward()) > 0.f;
    if (IsBike()) { // only distinguishes front vs rear
        return isFront ? eNearestCarWheel::FRONT_LEFT : eNearestCarWheel::REAR_LEFT;
    }
    const bool isRight = relativePt.Dot(GetRight()) > 0.f;
    return isFront
        ? isRight ? eNearestCarWheel::FRONT_RIGHT : eNearestCarWheel::FRONT_LEFT
        : isRight ? eNearestCarWheel::REAR_RIGHT : eNearestCarWheel::REAR_LEFT;
}

// 0x6D7C90
void CVehicle::InflictDamage(CEntity* damager, eWeaponType weapon, float intensity, CVector coords) {
    bool dueToFireExplosionOrBullet = false;
    if (!CanVehicleBeDamaged(damager, weapon, dueToFireExplosionOrBullet)) {
        return;
    }

    if (GetStatus() == STATUS_PLAYER && CStats::GetPercentageProgress() >= 100.0f) {
        intensity *= 0.5f;
    }

    // Player caused property damage
    if (intensity > 10.0f
        && (damager == FindPlayerPed(-1) || damager == FindPlayerVehicle(-1, false))
        && GetStatus() != STATUS_WRECKED
    ) {
        auto& playerInfo = CWorld::Players[CWorld::PlayerInFocus];
        playerInfo.m_nHavocCaused += 2;
        playerInfo.m_fCurrentChaseValue += 1.0f;
        CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)(rand() % 20 + 5));
    }

    // Ped shot at the tyres (automobiles / bikes only)
    if (damager && damager->GetType() == ENTITY_TYPE_PED && (m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE || m_nVehicleType == VEHICLE_TYPE_BIKE)) {
        const auto damagerPed = damager->AsPed();

        int32 chance = 0;
        switch (weapon) { // 0x6D82C0 (jump table, indexed through the byte table at 0x6D82D0)
        case WEAPON_PISTOL:
        case WEAPON_PISTOL_SILENCED:
        case WEAPON_SHOTGUN:
        case WEAPON_MICRO_UZI:
        case WEAPON_MP5:
        case WEAPON_TEC9:
        case WEAPON_UZI_DRIVEBY:
            chance = 5;
            break;
        case WEAPON_DESERT_EAGLE:
            chance = 0x40;
            break;
        case WEAPON_AK47:
        case WEAPON_M4:
            chance = 10;
            break;
        default:
            break;
        }

        if (damagerPed->IsPlayer()) {
            chance = 0;
        }

        bool mayBurst = true;
        if (damagerPed->m_pVehicle && damagerPed->m_pVehicle->m_nVehicleSubType == VEHICLE_TYPE_BIKE) {
            if (chance > 1) {
                chance = 1;
            }
        } else if (m_nModelIndex == MODEL_COPBIKE && m_pDriver && m_pDriver->IsCop()) {
            mayBurst = false;
        }

        if (mayBurst && chance != 0 && !vehicleFlags.bTyresDontBurst && (rand() & 0x7F) < chance) {
            std::optional<bool> physicalEffect;
            if (m_nVehicleType == VEHICLE_TYPE_BIKE) {
                physicalEffect = false;
            } else if (GetVehicleAppearance() == VEHICLE_APPEARANCE_AUTOMOBILE) {
                physicalEffect = true;
            }
            if (physicalEffect) {
                BurstTyre((uint8)((uint8)FindTyreNearestPoint(CVector2D{coords.x, coords.y}) + 13), *physicalEffect);
            }
        }
    }

    // Shot the petrol tank (gas cap) of a vehicle => kill it
    if (vehicleFlags.bPetrolTankIsWeakPoint && dueToFireExplosionOrBullet && damager && damager->GetType() == ENTITY_TYPE_PED && damager->AsPed()->IsPlayer()) {
        const CVector gasCapPos = GetVehicleModelInfo()->m_pVehicleStruct->m_avDummyPos[DUMMY_GAS_CAP];
        if (gasCapPos != CVector{}) { // 0x509760 is `operator!=`
            const auto worldPos = TransformPointExt(*m_matrix, gasCapPos);
            coords = CVector{ coords.x - worldPos.x, coords.y - worldPos.y, coords.z - worldPos.z };
            if (coords.Magnitude() < 0.25f) {
                intensity = m_fHealth < 1100.0f ? m_fHealth : 1100.0f; // 0x404330 (min, with the original's NaN behaviour)
            }
        }
    }

    // Helis / planes take less damage from anything but explosions / rockets
    if ((m_nVehicleSubType == VEHICLE_TYPE_HELI || m_nVehicleSubType == VEHICLE_TYPE_PLANE)
        && !vehicleFlags.bIsRCVehicle
        && weapon != WEAPON_EXPLOSION && weapon != WEAPON_GRENADE && weapon != WEAPON_ROCKET && weapon != WEAPON_ROCKET_HS
    ) {
        intensity *= 0.4f;
    }

    if (m_fHealth > 0.0f) {
        m_nLastWeaponDamageType = (uint8)weapon;
        if (damager) {
            m_pLastDamageEntity = damager;
            damager->RegisterReference(&m_pLastDamageEntity);
        }

        if (m_fHealth > intensity) {
            const float prevHealth = m_fHealth;
            m_fHealth = m_fHealth - intensity;

            if ((GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_SIMPLE || GetStatus() == STATUS_PHYSICS)
                && damager && damager->GetType() == ENTITY_TYPE_PED
            ) {
                if (m_pDriver) {
                    m_pDriver->GetEventGroup().Add(CEventVehicleDamageWeapon{ this, damager, weapon }, false);
                }
                for (auto i = 0; i < (int32)m_nMaxPassengers; i++) { // NOTE: the original iterates up to `m_nMaxPassengers` (+0x488), not `m_nNumPassengers` (+0x484)
                    if (const auto passenger = m_apPassengers[i]) {
                        passenger->GetEventGroup().Add(CEventVehicleDamageWeapon{ this, damager, weapon }, false);
                    }
                }
            }

            if (prevHealth >= 250.0f && m_fHealth < 250.0f && m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
                AsAutomobile()->m_damageManager.SetEngineStatus(225);
                // NOTE: the original stores `damager` (any `CEntity*`) into CAutomobile +0x928, which is `m_pExplosionVictim` (a `CPed*`)
                const auto engineBreaker = reinterpret_cast<CEntity**>(&AsAutomobile()->m_pExplosionVictim);
                *engineBreaker = damager;
                if (damager) {
                    damager->RegisterReference(engineBreaker);
                }
            }
        } else {
            m_fHealth = 0.0f;

            if (damager == FindPlayerPed(-1)) {
                eCrimeType crime;
                if (m_nVehicleSubType == VEHICLE_TYPE_HELI && !vehicleFlags.bIsRCVehicle) {
                    crime = CRIME_DESTROY_HELI;
                } else if (m_nVehicleSubType == VEHICLE_TYPE_PLANE && !vehicleFlags.bIsRCVehicle) {
                    crime = CRIME_DESTROY_PLANE;
                } else {
                    crime = CRIME_DESTROY_VEHICLE;
                }
                CCrime::ReportCrime(crime, this, damager->AsPed());
            }

            if (weapon == WEAPON_EXPLOSION) {
                m_wBombTimer = (int16)((rand() & 0x7FF) + 1000);
                m_pWhoDetonatedMe = damager ? damager->AsPed() : nullptr;
                if (damager) {
                    damager->RegisterReference(reinterpret_cast<CEntity**>(&m_pWhoDetonatedMe));
                }
            } else {
                BlowUpCar(damager, false);
            }
        }
    }

    if (vehicleFlags.bIsLawEnforcer && damager == FindPlayerPed(-1)) {
        FindPlayerPed(-1)->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1);
    }
}

// 0x6D82F0
void CVehicle::KillPedsGettingInVehicle() {
    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (ped.bInVehicle || ped.bIsStanding) {
            continue;
        }

        if (const auto task = static_cast<CTaskComplexEnterCar*>(ped.GetTaskManager().Find<CTaskComplexEnterCarAsPassenger, CTaskComplexEnterCarAsDriver>());
            !task || task->GetTargetCar() != this
        ) {
            continue;
        }

        CPedDamageResponseCalculator dmgRespCalc{ &ped, 1000.f, WEAPON_EXPLOSION, PED_PIECE_TORSO, false };
        CEventDamage dmgEvent{ &ped, CTimer::GetTimeInMS(), WEAPON_EXPLOSION, PED_PIECE_TORSO, 0, false, (bool)ped.bInVehicle };
        if (dmgEvent.AffectsPed(&ped)) {
            dmgRespCalc.ComputeDamageResponse(&ped, dmgEvent.m_damageResponse, true);
        } else {
            dmgEvent.m_damageResponse.m_bDamageCalculated = true;
        }
        ped.GetEventGroup().Add(&dmgEvent);
    }
}

// 0x6D8470
bool CVehicle::UsesSiren() {
    switch (m_nModelIndex) {
    case MODEL_FIRETRUK:
    case MODEL_AMBULAN:
    case MODEL_MRWHOOP:
        return true;
    case MODEL_RHINO:
        return false;
    default:
        return IsLawEnforcementVehicle() != false;
    }
}

// 0x6D84D0
bool CVehicle::IsSphereTouchingVehicle(CVector posn, float radius) {
    const auto cm = GetColModel();
    const auto dist = posn - GetPosition();

    const auto dotRight = DotProduct(dist, GetRight());
    if (dotRight < cm->m_boundBox.m_vecMin.x - radius ||
        dotRight > cm->m_boundBox.m_vecMax.x + radius
    ) {
        return false;
    }

    const auto dotFwd = DotProduct(dist, GetForward());
    if (dotFwd < cm->m_boundBox.m_vecMin.y - radius ||
        dotFwd > cm->m_boundBox.m_vecMax.y + radius
    ) {
        return false;
    }

    const auto dotUp = DotProduct(dist, GetUp());
    if (dotUp < cm->m_boundBox.m_vecMin.z - radius ||
        dotUp > cm->m_boundBox.m_vecMax.z + radius
    ) {
        return false;
    }

    return true;
}

// 0x6D85F0
void CVehicle::FlyingControl(eFlightModel flightModel, float leftRightSkid, float steeringUpDown, float steeringLeftRight, float accelerationBreakStatus) {
    return ((void(__thiscall*)(CVehicle*, eFlightModel, float, float, float, float))0x6D85F0)(this, flightModel, leftRightSkid, steeringUpDown, steeringLeftRight, accelerationBreakStatus);

    /*
    * Code below should be correct. I just didn't finish it, because I can't be bothered to figure out what
    * 0x6D8BFD is.. maybe later.
    */
    if (!m_pFlyingHandlingData || CTimer::GetTimeStep() <= 0.f) {
        return;
    }

    const auto padOfPlayerDriver = (GetStatus() == STATUS_PLAYER && m_pDriver && m_pDriver->IsPlayer()) ? m_pDriver->AsPlayer()->GetPadFromPlayer() : nullptr;
    const auto windDragForce     = IsMissionVehicle() ? CVector{} : -CWeather::WindDir * m_pFlyingHandlingData->m_fWindMult;
    const auto velocityWithDrag = m_vecMoveSpeed + windDragForce; // Plus because `windDragForce` is opposing to our velocity already

    switch (flightModel) {
    case FLIGHT_MODEL_UNK:
    case FLIGHT_MODEL_UNK2: {
        NOTSA_UNREACHABLE("The function doesn't seem to be called with this model from anywhere, but it is present in the code.. So I wont bother");
        return;
    }
    case FLIGHT_MODEL_BARON:
    case FLIGHT_MODEL_PLANE:
    case FLIGHT_MODEL_BOAT: {
        if (leftRightSkid == -9999.9902f) { // What a weird fucking value lol
            leftRightSkid = 0.f;
            if (padOfPlayerDriver) {
                leftRightSkid = (float)padOfPlayerDriver->GetSteeringLeftRight() / 128.f;
            }
        }
        if (steeringUpDown == -9999.9902f) {
            steeringUpDown = 0.f;
            if (padOfPlayerDriver) {
                steeringUpDown = (float)padOfPlayerDriver->GetSteeringUpDown() / 128.f;
                const auto gunUpDown = padOfPlayerDriver->GetCarGunUpDown();
                if (std::abs((float)gunUpDown) > 1.f) {
                    steeringUpDown = -gunUpDown / 128.f;
                }
            }
        }
        break;
    }
    }
}

// 0x6DAF00
// always returns `false`, and `rotorType` is always `-3`
template<typename PtrListType>
bool CVehicle::BladeColSectorList(PtrListType& ptrList, CColModel& colModel, CMatrix& matrix, int16 rotorType, float damageMult) {
    if (ptrList.IsEmpty()) {
        return false;
    }

    // Returns UP vector and thickness vector (in which only 1 component is set and that is the thickness)
    const auto GetRotorDirUpAndThickness = [this, rotorType, &matrix]() -> std::pair<CVector, CVector> {
        // Seems like `rotorType` is just one of the 6 possible directions:
        // down, backwards, left, right, foward, up => -3, -2, -1, 1, 2, 3
        // Not sure how this works in the real world, as the code only uses -3
        assert(rotorType == -3); // NOTSA: Testing my theory (Pirulax)
        switch (rotorType) {
        case -3: return { -matrix.GetUp(),      {  0.0f,  0.0f, -0.2f }, }; // down
        case -2: return { -matrix.GetForward(), {  0.0f, -0.2f,  0.0f }, }; // backwards
        case -1: return { -matrix.GetRight(),   { -0.2f,  0.0f,  0.0f }, }; // left
        case  1: return {  matrix.GetRight(),   {  0.2f,  0.0f,  0.0f }, }; // right
        case  2: return {  matrix.GetForward(), {  0.0f,  0.2f,  0.0f }, }; // forward
        case  3: return {  matrix.GetUp(),      {  0.0f,  0.0f,  0.2f }, }; // up
        default: NOTSA_UNREACHABLE("Unknown rotorType");
        }
    };

    const auto [rotorUp, rotorSizeOS] = GetRotorDirUpAndThickness();
    const auto rotorSize              = matrix.TransformVector(rotorSizeOS);
    const auto colModelCenter         = matrix.TransformPoint(colModel.GetBoundCenter());
    const auto& thisPosn              = GetPosition();

    for (auto* entity : ptrList) {
        if (static_cast<CEntity*>(entity) == static_cast<CEntity*>(this) || !entity->m_bUsesCollision) {
            continue;
        }

        if (entity->IsScanCodeCurrent()) {
            continue;
        }
        entity->SetCurrentScanCode();

        auto entityCM = entity->GetIsTypePed()
            ? entity->GetModelInfo()->AsPedModelInfoPtr()->AnimatePedColModelSkinned(entity->GetRpClump())
            : entity->GetColModel();

        if (!entityCM) {
            continue;
        }

        if (entity->GetIsTypeObject() && entity->AsObject()->m_nObjectType == eObjectType::OBJECT_TEMPORARY) {
            continue;
        }

        const auto numColls = CCollision::ProcessColModels(
            matrix, colModel,
            entity->GetMatrix(), *entityCM,
            CWorld::m_aTempColPts,
            nullptr,
            nullptr,
            false
        );
        if (numColls <= 0) {
            continue;
        }

        if (entity->GetIsTypePed()) { // 0x6DB207
            auto& ped = *entity->AsPed();

            const auto dirToPed = Normalized(GetPosition() - ped.GetPosition());

            if (!ped.m_pAttachedTo) { // 0x6DB24C
                ped.ApplyMoveForce(CVector{ CVector2D{dirToPed} * -5.f, 5.f });
            }

            CEventDamage dmgEvent{ // 0x6DB2CE
                this,
                CTimer::GetTimeInMS(),
                WEAPON_RUNOVERBYCAR,
                PED_PIECE_TORSO,
                static_cast<uint8>(ped.GetLocalDirection(dirToPed)),
                false,
                false
            };
            dmgEvent.ComputeDamageResponseIfAffectsPed( // 0x6DB326
                &ped,
                { this, 1000.f, WEAPON_RUNOVERBYCAR, PED_PIECE_TORSO, false },
                true
            );
            ped.GetEventGroup().Add(dmgEvent);

            if (CLocalisation::Blood()) { // 0x6DB34D
                if (ped.GetIsOnScreen()) {
                    g_fx.AddBlood(
                        GetPosition() + CVector{ CVector2D{ dirToPed } * 0.35f, 0.6f}, // TODO: Magic 0.6f
                        dirToPed / 100.f,
                        16,
                        ped.m_fContactSurfaceBrightness
                    );
                }
            }
        } else if (entity->m_nModelIndex != eModelID::MODEL_MISSILE) { // 0x6DB44F
            bool  wasAnyCPValid{};
            float automobileCollisionDmgIntensity{};
            CVector cpOnRotor{}; //!< Col point on the rotor

            const auto prevElasticity = std::exchange(m_fElasticity, 1.f);

            for (const auto& cp : CWorld::m_aTempColPts | rng::views::take(numColls)) { // 0x6DB474
                const auto colDir = cp.m_vecPoint - colModelCenter;
                const auto colDirOnRotorUp = DotProduct(colDir, rotorUp);

                if ( std::abs(colDirOnRotorUp) > ROTOR_SEMI_THICKNESS * 2.f
                  && std::abs(colDirOnRotorUp) > std::abs(DotProduct(colDir, cp.m_vecNormal)) * 0.3f
                ) {
                    continue;
                }

                wasAnyCPValid = true;

                     cpOnRotor   = cp.m_vecPoint - rotorUp * colDirOnRotorUp;
                auto colForceDir = rotorSize.Cross(cpOnRotor - colModelCenter) + m_vecMoveSpeed;

                g_fx.AddSparks(
                    cpOnRotor,
                    colForceDir,
                    colForceDir.NormaliseAndMag() * 15.f,
                    16,
                    CVector{},
                    eSparkType::SPARK_PARTICLE_SPARK,
                    0.2f,
                    1.f
                );

                if (IsAutomobile()) {
                    const auto au = AsAutomobile();
                    if (au->m_fHeliRotorSpeed <= 0.15f) {
                        if (au->m_fHeliRotorSpeed < 0.15f / 2.f && au->m_fHeliRotorSpeed > 0.f) {
                            au->m_fHeliRotorSpeed *= -1.f;
                        }
                    } else {
                        ApplySoftCollision(entity, cp, automobileCollisionDmgIntensity);
                        ApplyTurnForce(colForceDir * (m_fTurnMass * -0.0005f), cpOnRotor - colModelCenter);
                        au->m_fHeliRotorSpeed = 0.15f;
                    }
                }

                SetDamagedPieceRecord(
                    std::max(automobileCollisionDmgIntensity, 100.f * m_fMass / 3000.f),
                    entity,
                    cp,
                    1.f
                );
            }

            if (wasAnyCPValid) {
                if (entity->GetIsTypePed() && !CTimer::IsTimeInRange(planeRotorDmgTimeMS - 2000, planeRotorDmgTimeMS)) {
                    const auto ReportCollision = [&](CVector pos) {
                        AudioEngine.ReportCollision(
                            this,
                            entity,
                            SURFACE_CAR_PANEL,
                            SURFACE_CAR,
                            pos,
                            nullptr,
                            0.15f,
                            1.f,
                            false,
                            false
                        );
                    };
                    if (GetStatus() == STATUS_REMOTE_CONTROLLED) {
                        ReportCollision(cpOnRotor);
                    } else if (GetStatus() == STATUS_PLAYER) {
                        const auto& gameCamPos = *TheCamera.GetGameCamPosition();
                        ReportCollision(gameCamPos + Normalized(cpOnRotor - gameCamPos) * 4.f);
                    }
                    planeRotorDmgTimeMS = CTimer::GetTimeInMS() + CGeneral::GetRandomNumberInRange(150, 250);
                }
            }

            m_fElasticity = prevElasticity;
        }
    }

    return false;
}

// 0x6DBA30
void CVehicle::SetComponentRotation(RwFrame* component, eRotationAxis axis, float angle, bool bSetRotate) {
    if (!component) {
        return;
    }

    CMatrix mat{ RwFrameGetMatrix(component) };

    switch (axis) {
    case AXIS_X: bSetRotate ? mat.SetRotateXOnly(angle) : mat.RotateX(angle, true); break;
    case AXIS_Y: bSetRotate ? mat.SetRotateYOnly(angle) : mat.RotateY(angle, true); break;
    case AXIS_Z: bSetRotate ? mat.SetRotateZOnly(angle) : mat.RotateZ(angle, true); break;
    default:     NOTSA_UNREACHABLE();
    }

    mat.UpdateRW();
}

// 0x6DBBB0
void CVehicle::SetTransmissionRotation(RwFrame* component, float angleL, float angleR, CVector wheelPos, bool isFront) {
    if (component) {
        CMatrix mat(&component->modelling);
        CVector savedPos = mat.GetPosition();
        float angleX = -std::atan2(
            (angleL + angleR) / 2.0f - wheelPos.z,
            mat.GetPosition().y - wheelPos.y
        );
        if (isFront) {
            angleX += PI;
        }
        mat.SetRotateX(angleX);
        mat.RotateY(std::atan2(angleL - angleR, std::fabs(wheelPos.x) + std::fabs(wheelPos.x)));
        mat.GetPosition() += savedPos;
        mat.UpdateRW();
    }
}

// 0x6DBCE0
void CVehicle::ProcessBoatControl(tBoatHandlingData* boatHandling, float* fLastWaterImmersionDepth, bool bCollidedWithWorld, bool bPostCollision) {
    CVector vecBuoyancyTurnPoint{};
    CVector vecBuoyancyForce{};
    if (!mod_Buoyancy.ProcessBuoyancyBoat(this, m_fBuoyancyConstant, &vecBuoyancyTurnPoint, &vecBuoyancyForce, bCollidedWithWorld)) {
        physicalFlags.bSubmergedInWater = false;
        if (IsSubBoat()) {
            AsBoat()->m_nBoatFlags.bBoatInWater = false;
        }
        return;
    }

    bool bOnWater = false;
    // FIX_BUGS ? vehicleFlags.bIsDrowning = false;
    if (CTimer::GetTimeStep() * m_fMass * 0.0008F >= vecBuoyancyForce.z) {
        physicalFlags.bSubmergedInWater = false;
    } else {
        physicalFlags.bSubmergedInWater = true;
        bOnWater = true;

        if (GetUp().z < -0.6F
            && std::fabs(m_vecMoveSpeed.x) < 0.05F
            && std::fabs(m_vecMoveSpeed.y) < 0.05F
        ) {
            vehicleFlags.bIsDrowning = true;
            if (m_pDriver) {
                m_pDriver->physicalFlags.bTouchingWater = true;
                if (m_pDriver->IsPlayer()) {
                    m_pDriver->AsPlayer()->HandlePlayerBreath(true, 1.0F);
                } else {
                    auto damageEvent = CEventDamage(this, CTimer::GetTimeInMS(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, 0, false, true);
                    if (damageEvent.AffectsPed(m_pDriver)) {
                        auto pedDamageResponseCalc = CPedDamageResponseCalculator(this, CTimer::GetTimeStep(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, false);
                        pedDamageResponseCalc.ComputeDamageResponse(m_pDriver, damageEvent.m_damageResponse, true);
                    } else {
                        damageEvent.m_damageResponse.m_bDamageCalculated = true;
                    }

                    m_pDriver->GetEventGroup().Add(&damageEvent, false);
                }
            }
        }
    }
    vehicleFlags.bIsDrowning = false; // see above

    // 0x6DBF0A
    auto vecUsedBuoyancyForce = vecBuoyancyForce;
    auto fImmersionDepth = mod_Buoyancy.m_fEntityWaterImmersion;
    if (m_nModelIndex == MODEL_SKIMMER
        && GetUp().z < -0.5F
        && std::fabs(m_vecMoveSpeed.x) < 0.2F
        && std::fabs(m_vecMoveSpeed.y) < 0.2F
    ) {
        vecUsedBuoyancyForce *= 0.03F;
    }
    CPhysical::ApplyMoveForce(vecUsedBuoyancyForce);

    if (bCollidedWithWorld) {
        CPhysical::ApplyTurnForce(vecBuoyancyForce * 0.4F, vecBuoyancyTurnPoint);
    }

    // 0x6DC00B
    if (m_nModelIndex == MODEL_SKIMMER) {
        auto fCheckedMass = CTimer::GetTimeStep() * m_fMass;
        if (m_f2ndSteerAngle != 0.0F
            || (GetForward().z < -0.5F
            && GetUp().z > -0.5F
            && m_vecMoveSpeed.z < -0.15F
            && fCheckedMass * 0.01F / 125.0F < vecBuoyancyForce.z
            && vecBuoyancyForce.z < fCheckedMass * 0.4F / 125.0F)
        ) {
            bOnWater = false;

            auto fTurnForceMult = GetForward().z * m_fTurnMass * -0.00017F * vecBuoyancyForce.z;
            CPhysical::ApplyTurnForce(GetForward() * fTurnForceMult, GetUp());

            auto fMoveForceMult = DotProduct(m_vecMoveSpeed, GetForward()) / -2.0f * m_fMass;
            CPhysical::ApplyMoveForce(GetForward() * fMoveForceMult);

            if (m_f2ndSteerAngle == 0.0F) { // todo: missing checks for CTimer 0x6DC195 ?
                m_f2ndSteerAngle = (float)CTimer::GetTimeInMS() + 300.0F;
            }
            else if (m_f2ndSteerAngle <= (float)CTimer::GetTimeInMS()) {
                m_f2ndSteerAngle = 0.0F;
            }
        }
    }

    // 0x6DC1E2
    if (!bPostCollision && bOnWater && GetUp().z > 0.0F) {
        auto fMoveForce = m_vecMoveSpeed.SquaredMagnitude() * boatHandling->m_fAqPlaneForce * CTimer::GetTimeStep() * vecBuoyancyForce.z * 0.5F;
        if (m_nModelIndex == MODEL_SKIMMER)
            fMoveForce *= (m_GasPedal + 1.0F);
        else if (m_GasPedal <= 0.05F)
            fMoveForce = 0.0F;
        else
            fMoveForce *= m_GasPedal;

        auto fMaxMoveForce = CTimer::GetTimeStep() * boatHandling->m_fAqPlaneLimit * m_fMass / 125.0F;
        fMoveForce = std::min(fMoveForce, fMaxMoveForce);

        auto vecUsedMoveForce = GetUp() * fMoveForce;
        CPhysical::ApplyMoveForce(vecUsedMoveForce);

        auto vecOffset = GetForward() * boatHandling->m_fAqPlaneOffset;
        auto vecTurnPoint = vecBuoyancyTurnPoint - vecOffset;
        CPhysical::ApplyTurnForce(vecUsedMoveForce, vecTurnPoint);
    }

    CPad* pad = nullptr;
    if (GetStatus() == STATUS_PLAYER && m_pDriver && m_pDriver->IsPlayer()) {
        pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
    }

    // 0x6DC3AF
    if (GetUp().z > -0.6F) {
        float fMoveSpeed = 1.0F;
        if (std::fabs(m_GasPedal) <= 0.05F) {
            fMoveSpeed = m_vecMoveSpeed.Magnitude2D();
        }

        if (std::fabs(m_GasPedal) > 0.05F || fMoveSpeed > 0.01F) {
            if (IsSubBoat() && bOnWater && fMoveSpeed > 0.05F) {
                //GetColModel(); Unused call
                AsBoat()->AddWakePoint(GetPosition());
            }

            auto fTraction = 1.0F;
            if (GetStatus() == STATUS_PLAYER) {
                auto fTractionLoss = DotProduct(m_vecMoveSpeed, GetForward()) * m_pHandlingData->m_fTractionBias;
                if (pad->GetHandBrake())
                    fTractionLoss *= 0.5F;

                fTraction = 1.0F - fTractionLoss;
                fTraction = std::clamp(fTraction, 0.0F, 1.0F);
            }

            auto fSteerAngleChange = -(fTraction * m_fSteerAngle);
            auto fSteerAngleSin = std::sin(fSteerAngleChange);
            auto fSteerAngleCos = std::cos(fSteerAngleChange);

            const auto& vecBoundingMin = CEntity::GetColModel()->m_boundBox.m_vecMin;
            CVector vecThrustPoint(0.0F, vecBoundingMin.y * boatHandling->m_fThrustY, vecBoundingMin.z * boatHandling->m_fThrustZ);
            auto vecTransformedThrustPoint = GetMatrix().TransformVector(vecThrustPoint);

            auto vecWorldThrustPos = GetPosition() + vecTransformedThrustPoint;
            float fWaterLevel;
            CWaterLevel::GetWaterLevel(vecWorldThrustPos, fWaterLevel, true); // warn: result not checked
            if (vecWorldThrustPos.z - 0.5F >= fWaterLevel) {
                if (IsSubBoat())
                    AsBoat()->m_nBoatFlags.bBoatEngineInWater = false;
            }
            else {
                auto fThrustDepth = fWaterLevel - vecWorldThrustPos.z + 0.5F;
                fThrustDepth = std::min(sq(fThrustDepth), 1.0F);

                if (IsSubBoat())
                    AsBoat()->m_nBoatFlags.bBoatEngineInWater = true;

                bool bIsSlowingDown = false;
                auto fGasState = std::fabs(m_GasPedal);
                if (fGasState < 0.01F || m_nModelIndex == MODEL_SKIMMER) {
                    bIsSlowingDown = true;
                }
                else {
                    if (fGasState < 0.5F)
                        bIsSlowingDown = true;

                    auto fSteerAngle = std::fabs(m_fSteerAngle);
                    CVector vecSteer(-fSteerAngleSin, fSteerAngleCos, -fSteerAngle);
                    CVector vecSteerMoveForce = GetMatrix().TransformVector(vecSteer);
                    vecSteerMoveForce *= fThrustDepth * m_GasPedal * 40.0F * m_pHandlingData->m_transmissionData.m_EngineAcceleration * m_fMass;

                    if (vecSteerMoveForce.z > 0.2F)
                        vecSteerMoveForce.z = sq(1.2F - vecSteerMoveForce.z) + 0.2F;

                    if (bPostCollision) {
                        if (m_GasPedal < 0.0F)
                            vecSteerMoveForce *= CVector(5.0F, 5.0F, 1.0F);

                        vecSteerMoveForce.z = std::max(0.0F, vecSteerMoveForce.z);
                        CPhysical::ApplyMoveForce(vecSteerMoveForce * CTimer::GetTimeStep());
                    }
                    else {
                        CPhysical::ApplyMoveForce(vecSteerMoveForce * CTimer::GetTimeStep());

                        auto vecTurnForcePoint = vecTransformedThrustPoint - (GetUp() * boatHandling->m_fThrustAppZ);
                        CPhysical::ApplyTurnForce(vecSteerMoveForce * CTimer::GetTimeStep(), vecTurnForcePoint);

                        auto fTractionSide = -DotProduct(vecSteerMoveForce, GetRight()) * m_pHandlingData->m_fTractionMultiplier;
                        auto vecTurnForceSide = GetRight() * fTractionSide * CTimer::GetTimeStep();
                        CPhysical::ApplyTurnForce(vecTurnForceSide, GetUp());
                    }

                    //This code does nothing
                    /*if (m_fGasPedal > 0.0F && GetStatus() == eEntityStatus::STATUS_PLAYER) {
                        const auto& vecBoundMin = GetColModel()->m_boundBox.m_vecMin;
                        CVector vecUnkn = CVector(0.0F, vecBoundingMin.y, 0.0F);
                        CVector vecUnknTransformed;
                        Multiply3x3(&vecUnknTransformed, GetMatrix(), &vecUnkn);
                    }*/
                }

                if (!bPostCollision && bIsSlowingDown) {
                    auto fTractionLoss = DotProduct(m_vecMoveSpeed, GetForward()) * m_pHandlingData->m_fTractionLoss;
                    fTractionLoss = std::min(fTractionLoss, m_fTurnMass * 0.01F);

                    if (fGasState > 0.01F) {
                        fTractionLoss *= (0.55F - fGasState);
                        if (GetStatus() == STATUS_PLAYER)
                            fTractionLoss *= 2.6F;
                        else
                            fTractionLoss *= 5.0F;
                    }

                    if (m_GasPedal < 0.0f && fTractionLoss > 0.0f ||
                        m_GasPedal > 0.0f && fTractionLoss < 0.0f
                    ) {
                        fTractionLoss *= -1.0F;
                    }

                    CVector vecTractionLoss(-fSteerAngleSin, 0.0F, 0.0F);
                    vecTractionLoss *= fTractionLoss;
                    CVector vecTractionLossTransformed = GetMatrix().TransformVector(vecTractionLoss);
                    vecTractionLossTransformed *= fThrustDepth * CTimer::GetTimeStep();

                    CPhysical::ApplyMoveForce(vecTractionLossTransformed);
                    CPhysical::ApplyTurnForce(vecTractionLossTransformed, vecTransformedThrustPoint);

                    auto fUsedTimeStep = std::max(CTimer::GetTimeStep(), 0.01F);
                    auto vecTurn = GetRight() * fUsedTimeStep / fTraction * fTractionLoss * fSteerAngleSin * -0.75F;
                    CPhysical::ApplyTurnForce(vecTurn, GetUp());
                }
            }
        }
    }

    if (m_pHandlingData->m_fSuspensionBiasBetweenFrontAndRear != 0.0F) {
        auto right = GetForward().Cross(CVector::ZAxisVector());

        const auto mult =
              DotProduct(right, m_vecMoveSpeed)
            * m_pHandlingData->m_fSuspensionBiasBetweenFrontAndRear
            * CTimer::GetTimeStep()
            * fImmersionDepth
            * m_fMass
            * -0.1F;

        const auto x = right.x * 0.3F;
        right.x -= right.y * 0.3F;
        right.y += x;

        CPhysical::ApplyMoveForce(right * mult);
    }

    if (GetStatus() == STATUS_PLAYER && pad->GetHandBrake()) {
        auto fDirDotProd = DotProduct(m_vecMoveSpeed, GetForward());
        if (fDirDotProd > 0.0F) {
            auto fMoveForceMult = fDirDotProd * m_pHandlingData->m_fSuspensionLowerLimit * CTimer::GetTimeStep() * fImmersionDepth * m_fMass * -0.1F;
            auto vecMoveForce = GetForward() * fMoveForceMult;
            CPhysical::ApplyMoveForce(vecMoveForce);
        }
    }

    if (bOnWater && !bPostCollision && !bCollidedWithWorld) {
        ApplyBoatWaterResistance(boatHandling, fImmersionDepth);
    }

    // 0x6DCD63
    if ((m_nModelIndex != MODEL_SKIMMER || m_f2ndSteerAngle == 0.0F) && !bCollidedWithWorld) {
        auto vecTurnRes = Pow(boatHandling->m_vecTurnRes, CTimer::GetTimeStep());
        m_vecTurnSpeed = GetMatrix().InverseTransformVector(m_vecTurnSpeed);
        m_vecTurnSpeed.y *= vecTurnRes.y;
        m_vecTurnSpeed.z *= vecTurnRes.z;

        float fMult = vecTurnRes.x / (sq(m_vecTurnSpeed.x) * 1000.0F + 1.0F) * m_vecTurnSpeed.x - m_vecTurnSpeed.x;
        fMult *= m_fTurnMass;
        auto vecTurnForce = GetUp() * fMult;

        m_vecTurnSpeed = GetMatrix().TransformVector(m_vecTurnSpeed);
        auto vecCentreOfMass = GetMatrix().TransformVector(m_vecCentreOfMass);
        auto vecTurnPoint = GetForward() + vecCentreOfMass;
        CPhysical::ApplyTurnForce(vecTurnForce, vecTurnPoint);
    }

    // Handle wave collision
    if (!bPostCollision && bOnWater && GetUp().z > 0.0F) {
        auto fWaveMult = floorf((fImmersionDepth - *fLastWaterImmersionDepth) * 10000.0F);
        auto fAudioVolume = fWaveMult * boatHandling->m_fWaveAudioMult;
        if (fAudioVolume > 200.0F)
            m_vehicleAudio.AddAudioEvent(eAudioEvents::AE_BOAT_HIT_WAVE, fAudioVolume);

        if (fWaveMult > 200.0F) {
            auto fZComp = m_vecMoveSpeed.SquaredMagnitude() * fWaveMult / 1000.0F;
            CVector vecWaveMoveForce(0.0F, 0.0F, fZComp);

            vecWaveMoveForce.z = std::min(vecWaveMoveForce.z, m_pHandlingData->m_fBrakeDeceleration - m_vecMoveSpeed.z);
            vecWaveMoveForce.z = std::max(vecWaveMoveForce.z, 0.0F);

            auto fMoveDotProd = DotProduct(m_vecMoveSpeed, GetForward());
            auto fTurnForceMult = fWaveMult * m_pHandlingData->m_fBrakeBias * -0.01F;

            auto vecMoveForce = GetForward() * fTurnForceMult * fMoveDotProd;
            vecWaveMoveForce += vecMoveForce;
            vecWaveMoveForce *= m_fMass;

            CPhysical::ApplyMoveForce(vecWaveMoveForce);
            CPhysical::ApplyTurnForce(vecWaveMoveForce, vecBuoyancyTurnPoint);
        }
    }

    *fLastWaterImmersionDepth = fImmersionDepth;
    if (IsSubBoat()) {
        AsBoat()->m_fxBuoyancyForce = vecBuoyancyForce;
        AsBoat()->m_nBoatFlags.bBoatInWater = bOnWater;
    }
    else if (IsAutomobile()) {
        this->AsAutomobile()->m_fDoomHorizontalRotation = vecBuoyancyForce.Magnitude();
    }
}

// 0x59C890 - the original evaluation order; the sum stays in the FPU registers (extended precision)
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// 0x6DD130
void CVehicle::DoBoatSplashes(float fWaterDamping) {
    // NOTE: The original keeps intermediates on the x87 stack (53-bit precision), hence the `double`s below.
    const double speedSqExt = (double)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (double)m_vecMoveSpeed.y * m_vecMoveSpeed.y + (double)m_vecMoveSpeed.z * m_vecMoveSpeed.z;
    const float  speedSq    = (float)speedSqExt; // The original stores it as a float, but compares the unrounded value
    if (!(speedSqExt > 0.0025f) || !(GetUp().z > 0.0f) || TheCamera.GetLookingForwardFirstPerson() || !IsVisible()) {
        return;
    }

    const bool isCruising = m_autoPilot.m_nCarMission == MISSION_CRUISE;
    if (isCruising && (CTimer::m_FrameCounter & 2) != 0) {
        return;
    }

    auto camToVeh = GetPosition() - TheCamera.GetPosition(); // 0x40FE60
    camToVeh.z = 0.0f;
    const float dist = camToVeh.Magnitude();
    if (!(dist < 80.0f)) {
        return;
    }

    const bool isSkimmer = m_nModelIndex == MODEL_SKIMMER;

    double intensity = std::sqrt((double)speedSq) * 0.075f * fWaterDamping;
    if (isSkimmer) {
        intensity *= 3.0f;
        if (!(intensity <= 0.5f)) {
            intensity = 0.5f;
        }
    } else if (!(intensity <= 1.0f)) {
        intensity = 1.0f;
    }
    if (!(intensity > 0.15f)) {
        return;
    }

    float scaled = (float)(intensity * 0.75f);
    if (isCruising) {
        const double doubled = (double)scaled + scaled;
        scaled = doubled >= 1.0 ? 1.0f : (float)doubled;
    }

    uint8 alphaByte = (uint8)(int32)(scaled * 128.0f);
    if (alphaByte >= 0x40) {
        alphaByte = 0x40;
    }

    const double velScaleD = (double)scaled * 10.0f;
    const float  velScale  = (float)velScaleD;
    const float  sizeBase  = (float)(velScaleD + 0.75f);
    const float  lifeRand  = CGeneral::GetRandomNumberInRange(0.8f, 1.2f);
    const float  lifeBase  = (float)((((double)scaled + scaled) + 0.3f) * lifeRand);

    if (dist > 50.0f) {
        alphaByte = (uint8)(int32)(((80.0 - dist) * 0.033333335f) * alphaByte); // 0x858F10
    }

    FxPrtMult_c particleData(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    {
        const float alpha = (float)((double)alphaByte * 0.003921569f); // 1 / 255
        particleData.m_Color.alpha = alpha >= 1.0f ? 1.0f : alpha;

        const float size = (float)((double)sizeBase * 0.1f);
        particleData.m_fSize = size >= 1.0f ? 1.0f : size;

        const float life = (float)((double)lifeBase * 0.2f);
        particleData.m_fLife = life >= 1.0f ? 1.0f : life;
    }

    const CBoundingBox& bb = GetColModel()->GetBoundingBox();
    const CVector colMin = bb.m_vecMin;
    const CVector colMax = bb.m_vecMax;

    float xMult = 0.7f;
    float zMult = 0.0f;
    if (isSkimmer) {
        xMult = StaticRef<float>(0x8D3678); // 0.25f
        zMult = StaticRef<float>(0x8D3674); // 0.85f
    }

    const float posY = colMax.y * 0.5f;
    const float posZ = zMult * colMin.z;

    // 0x59C890 - the original evaluation order
    const auto TransformPointOriginal = [&](const CVector& v) { return TransformPointExt(*m_matrix, v); };

    const CVector backward{ -m_matrix->GetForward().x, -m_matrix->GetForward().y, -m_matrix->GetForward().z };

    // Left splash
    {
        const CVector pos = TransformPointOriginal({ colMin.x * xMult, posY, posZ });
        CVector       vel = backward * CGeneral::GetRandomNumberInRange(0.8f, 1.2f);
        vel -= m_matrix->GetRight() * CGeneral::GetRandomNumberInRange(0.3f, 0.7f);
        vel += m_matrix->GetUp() * CGeneral::GetRandomNumberInRange(0.8f, 1.2f);
        vel *= velScale;
        g_fx.m_BoatSplash->AddParticle(pos, vel, 0.0f, particleData);
    }

    // Right splash
    {
        const CVector pos = TransformPointOriginal({ colMax.x * xMult, posY, posZ });
        CVector       vel = backward * CGeneral::GetRandomNumberInRange(0.8f, 1.2f);
        vel += m_matrix->GetRight() * CGeneral::GetRandomNumberInRange(0.3f, 0.7f);
        vel += m_matrix->GetUp() * CGeneral::GetRandomNumberInRange(0.8f, 1.2f);
        vel *= velScale;
        g_fx.m_BoatSplash->AddParticle(pos, vel, 0.0f, particleData);
    }
}

// 0x59C910 - `CVector::Normalise` as the original evaluates it: the sum of squares and the reciprocal root stay in the FPU registers
// (extended precision), every component is stored as float. A length of 0 (or less) only writes `x = 1`; NaN takes the sqrt path.
static void NormaliseExt(CVector& v) {
    const double sq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sq <= 0.0) {
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
}

// 0x44E480 - `CVector2D::Normalise` as the original evaluates it (see `NormaliseExt`)
static void Normalise2DExt(CVector2D& v) {
    const double sq = (double)v.x * v.x + (double)v.y * v.y;
    if (sq <= 0.0) {
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
}

// 0x6DD6F0
void CVehicle::DoSunGlare() {
    // NOTE: The original keeps most intermediates on the x87 stack (extended precision), hence the `double`s below.
    if (physicalFlags.bRenderScorched || m_matrix->GetUp().z < 0.0f || GetVehicleAppearance() != VEHICLE_APPEARANCE_AUTOMOBILE || !(CWeather::SunGlare > 0.0f)) {
        return;
    }

    // Vehicle -> camera, scaled to a length of 2 (`vToCam`)
    const CVector toCam = TheCamera.GetPosition() - GetPosition();
    const float   dist  = (float)std::sqrt(((double)toCam.x * toCam.x + (double)toCam.z * toCam.z) + (double)toCam.y * toCam.y);
    const double  scale = 2.0f / (double)dist;
    const CVector vToCam{ (float)(toCam.x * scale), (float)(toCam.y * scale), (float)(toCam.z * scale) };

    // Sun direction (+ vToCam) in the vehicle's (right, forward) basis
    const auto&  vecToSun = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
    const double sx = (double)vToCam.x + vecToSun.x, sy = (double)vToCam.y + vecToSun.y, sz = (double)vToCam.z + vecToSun.z;
    const auto&  right = m_matrix->GetRight();
    const auto&  fwd   = m_matrix->GetForward();
    CVector glareDir{
        (float)((sz * right.z + sy * right.y) + sx * right.x),
        (float)((sz * fwd.z + sy * fwd.y) + sx * fwd.x),
        0.0f
    };
    NormaliseExt(glareDir);

    CVector2D fwd2D{ m_matrix->GetForward().x, m_matrix->GetForward().y };
    CVector2D toCam2D{ vToCam.x, vToCam.y };
    Normalise2DExt(fwd2D);
    Normalise2DExt(toCam2D);

    double facing = (double)toCam2D.y * fwd2D.y + (double)toCam2D.x * fwd2D.x;
    if (facing < 0.0) {
        facing = -facing;
    }

    double intensity;
    if (facing > 0.995f) {
        intensity = 1.0f;
    } else if (facing > 0.99f) {
        intensity = (facing - 0.99f) * 200.0f;
    } else {
        return;
    }

    if (!(dist > 30.0f)) {
        if (!(dist > 13.0f)) {
            return;
        }
        intensity = (intensity * ((double)dist - 13.0f)) * 0.05882353f; // 0x871FC0
    }

    const double colorScale = intensity * 0.8f;
    const auto   ToColor    = [&](uint16 core) { // 0x821B40 (_ftol) truncates
        return (uint8)(int32)((((double)(core + 0x1FE) * colorScale) * CWeather::SunGlare) * 0.33333334f);
    };
    const auto& colours = CTimeCycle::m_CurrentColours;
    const uint8 red     = ToColor(colours.m_nSunCoreRed);
    const uint8 green   = ToColor(colours.m_nSunCoreGreen);
    const uint8 blue    = ToColor(colours.m_nSunCoreBlue);

    const auto colData = GetColModel()->m_pColData;
    CCollision::CalculateTrianglePlanes(colData);

    const auto GetVertex = [&](uint16 idx) { return CVector{ colData->m_pVertices[idx] }; };

    // Walks over pairs of triangles that form a quad (they share an edge, so 1 of the second triangle's vertices is new)
    for (int32 i = 0; i <= (int32)(int16)colData->m_nNumTriangles - 2; i += 2) {
        const auto& tri0 = colData->m_pTriangles[i];
        const auto& tri1 = colData->m_pTriangles[i + 1];

        const uint16  a = tri0.vA, b = tri0.vB, c = tri0.vC;
        const CVector v0 = GetVertex(a);
        if (!(v0.z > 0.0f)) {
            continue;
        }
        const CVector v1 = GetVertex(b);
        const CVector v2 = GetVertex(c);

        int32   numNew = 0;
        CVector v3{};
        for (const auto idx : { tri1.vA, tri1.vB, tri1.vC }) {
            if (idx != a && idx != b && idx != c) {
                v3 = GetVertex(idx);
                numNew++;
            }
        }
        if (numNew != 1) {
            continue;
        }

        // Quad center
        const double sumX01 = (double)v1.x + v0.x;
        const double sumY01 = (double)v1.y + v0.y;
        const float  sumZ01 = (float)((double)v1.z + v0.z);
        const float  sumX   = (float)(sumX01 + v2.x);
        const double sumY   = sumY01 + v2.y;
        const double sumZ   = (double)sumZ01 + v2.z;
        const float  centerX = (float)((double)(float)((double)v3.x + sumX) * 0.25f);
        const float  centerY = (float)((double)(float)((double)v3.y + sumY) * 0.25f);
        const double centerZ = ((double)v3.z + sumZ) * 0.25f;

        // Smaller of the |center - v0| deltas
        const double dx    = (double)centerX - v0.x;
        const double dy    = (double)centerY - v0.y;
        const float  dyF   = (float)dy; // stored to a float temporary in the original
        const double adx   = dx < 0.0 ? -dx : dx;
        const double ady   = dy < 0.0 ? -dy : dy;
        double       delta = adx < ady ? dx : (double)dyF;
        if (delta < 0.0) {
            delta = -delta;
        }
        const double glare = delta * 1.4f;
        if (!(glare > 0.6f)) {
            continue;
        }

        const double offX = (double)glareDir.x * glare;
        const double offY = glare * glareDir.y;
        const float  offZ = (float)(glare * glareDir.z);
        const CVector localPos{ (float)(offX + centerX), (float)(offY + centerY), (float)((double)offZ + centerZ) };
        const CVector worldPos = TransformPointExt(*m_matrix, localPos); // 0x59C890
        const CVector coronaPos{
            (float)((double)worldPos.x + vToCam.x),
            (float)((double)worldPos.y + vToCam.y),
            (float)((double)worldPos.z + vToCam.z)
        };

        CCoronas::RegisterCorona( // 0x6FC580
            (uint32)(uintptr_t)this + 0x1B + i, // (sic) the id is derived from the vehicle's address
            nullptr,
            red, green, blue, 255,
            coronaPos,
            CWeather::SunGlare * 0.9f,
            90.0f,
            CORONATYPE_SHINYSTAR,
            FLARETYPE_NONE,
            CORREFL_NONE,
            LOSCHECK_OFF,
            TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            false,
            15.0f,
            false,
            false
        );
    }
}

// 0x6DDF60
void CVehicle::AddWaterSplashParticles() {
    if (!IsPointInSphere(GetPosition(), TheCamera.GetPosition(), 10.f)) {
        return;
    }

    auto fxPrtMult = FxPrtMult_c{ 1.0f, 1.0f, 1.0f, 0.35f, 0.02f, 0.0f, 0.03f };
    const auto& cd = *GetColModel()->m_pColData;
    for (const auto& tri : cd.GetTris()) {
        // Get and transform triangle vertices to world space
        auto vertices = cd.GetTriVertices(tri);
        for (auto& v : vertices) {
            v = m_matrix->TransformPoint(v);
        }

        const auto v0v1 = vertices[1] - vertices[0];
        const auto v1v2 = vertices[2] - vertices[1];

        for (auto i = 1 - (size_t)(CWeather::Rain * -2.f); i > 0; i--) {
            g_fx.m_Splash->AddParticle(
                vertices[0]
                    + v0v1 * CGeneral::GetRandomNumberInRange(0.f, 1.f)
                    + v1v2 * CGeneral::GetRandomNumberInRange(0.f, 1.f),
                {},
                0.f, fxPrtMult
            );
        }
    }
}

// 0x6DE240
void CVehicle::AddExhaustParticles() {
    if (m_bOffscreen)
        return;

    float dist = DistanceBetweenPointsSquared(GetPosition(), TheCamera.GetPosition());
    if (dist > 256.0f || dist > 64.0f && !((CTimer::GetFrameCounter() + m_nModelIndex) & 1)
    ) {
        return;
    }
    auto mi = GetVehicleModelInfo();
    CVector firstExhaustPos = mi->GetModelDummyPosition(DUMMY_EXHAUST);
    CVector secondExhaustPos = firstExhaustPos;
    secondExhaustPos.x *= -1.0f;
    CMatrix entityMatrix (*m_matrix);
    bool bHasDoubleExhaust = m_pHandlingData->m_bDoubleExhaust;
    if (IsSubBike()) {
        auto* bike = AsBike();
        bike->CalculateLeanMatrix();
        entityMatrix = bike->m_mLeanMatrix;
        switch (m_nModelIndex) {
        case MODEL_FCR900:
            if (m_anExtras[0] == 1 || m_anExtras[0] == 2)
                bHasDoubleExhaust = true;
            break;
        case MODEL_NRG500:
            if (!m_anExtras[0] || m_anExtras[0] == 1)
                secondExhaustPos = mi->GetModelDummyPosition(DUMMY_EXHAUST_SECONDARY);
            break;
        case MODEL_BF400:
            if (m_anExtras[0] == 2)
                bHasDoubleExhaust = true;
            break;
        }
    }

    if (firstExhaustPos == 0.0f) {
        return;
    }

    CVector vecParticleVelocity;
    if (DotProduct(GetForward(), m_vecMoveSpeed) >= 0.05f) {
        vecParticleVelocity = m_vecMoveSpeed * 30.0f;
    } else {
        static float randomFactor = CGeneral::GetRandomNumberInRange(-1.8f, -0.9f);
        vecParticleVelocity = randomFactor * GetForward();
    }

    firstExhaustPos = entityMatrix.TransformPoint(firstExhaustPos);
    bool bFirstExhaustSubmergedInWater = false;
    bool bSecondExhaustSubmergedInWater = false;
    float pLevel = 0.0f;
    if (physicalFlags.bTouchingWater && CWaterLevel::GetWaterLevel(firstExhaustPos, pLevel, true) &&
        pLevel >= firstExhaustPos.z) {
        bFirstExhaustSubmergedInWater = true;
    }

    if (bHasDoubleExhaust) {
        secondExhaustPos = entityMatrix.TransformPoint(secondExhaustPos);
        if (physicalFlags.bTouchingWater && CWaterLevel::GetWaterLevel(secondExhaustPos, pLevel, true) &&
            pLevel >= secondExhaustPos.z) {
            bSecondExhaustSubmergedInWater = true;
        }
    }

    if (CGeneral::GetRandomNumberInRange(1.0f, 3.0f) * (m_GasPedal + 1.1f) <= 2.5f)
        return;

    float fMoveSpeed = m_vecMoveSpeed.Magnitude() * 0.5f;
    float particleAlpha = 0.0f;
    if (0.25f - fMoveSpeed >= 0.0f) {
        particleAlpha = 0.25f - fMoveSpeed;
    }
    float fLife = std::max(0.2f - fMoveSpeed, 0.0f);
    FxPrtMult_c fxPrt(0.9f, 0.9f, 1.0f, particleAlpha, 0.2f, 1.0f, fLife);

    for (auto i = 0; i < 2; i++) {
        FxSystem_c* firstExhaustFxSystem = g_fx.m_SmokeII3expand;
        if (bFirstExhaustSubmergedInWater) {
            fxPrt.m_Color.alpha = particleAlpha * 0.5f;
            fxPrt.m_fSize = 0.6f;
            firstExhaustFxSystem = g_fx.m_Bubble;
        }
        firstExhaustFxSystem->AddParticle(firstExhaustPos, vecParticleVelocity, 0.0f, fxPrt, -1.0f, m_fContactSurfaceBrightness);
        if (bHasDoubleExhaust) {
            FxSystem_c* secondExhaustFxSystem = g_fx.m_SmokeII3expand;
            if (bSecondExhaustSubmergedInWater) {
                fxPrt.m_Color.alpha = particleAlpha * 0.5f;
                fxPrt.m_fSize = 0.6f;
                secondExhaustFxSystem = g_fx.m_Bubble;
            }
            secondExhaustFxSystem->AddParticle(secondExhaustPos, vecParticleVelocity, 0.0f, fxPrt, -1.0f, m_fContactSurfaceBrightness);
        }

        if (m_GasPedal > 0.5f && m_nCurrentGear < 3) {
            if (CGeneral::GetRandomNumber() % 2) {
                FxSystem_c* secondaryExhaustFxSystem = g_fx.m_SmokeII3expand;
                if (bFirstExhaustSubmergedInWater) {
                    fxPrt.m_Color.alpha = particleAlpha * 0.5f;
                    fxPrt.m_fSize = 0.6f;
                    secondaryExhaustFxSystem = g_fx.m_Bubble;
                }
                secondaryExhaustFxSystem->AddParticle(firstExhaustPos, vecParticleVelocity, 0.0f, fxPrt, -1.0f, m_fContactSurfaceBrightness);
            } else if (bHasDoubleExhaust) {
                FxSystem_c* secondaryExhaustFxSystem = g_fx.m_SmokeII3expand;
                if (bSecondExhaustSubmergedInWater) {
                    fxPrt.m_Color.alpha = particleAlpha * 0.5f;
                    fxPrt.m_fSize = 0.6f;
                    secondaryExhaustFxSystem = g_fx.m_Bubble;
                }
                secondaryExhaustFxSystem->AddParticle(secondExhaustPos, vecParticleVelocity, 0.0f, fxPrt, -1.0f, m_fContactSurfaceBrightness);
            }
        }
    }
}

// 0x41BD90 - `CGeneral::GetRandomNumberInRange(min, max)` as the original evaluates it (CRT `rand()`; the result is left in the FPU)
static double RandomInRangeExt(float min, float max) {
    return ((double)max - (double)min) * ((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL) + (double)min;
}

// 0x4082C0 - `CVector::Magnitude` (the result is left in the FPU)
static double MagnitudeExt(const CVector& v) {
    return std::sqrt(((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z);
}

// 0x59C970 - `CVector::NormaliseAndMag` as the original evaluates it. A length of 0 (or less) only writes `x = 1` and returns 1.
static float NormaliseAndMagExt(CVector& v) {
    const double sq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sq <= 0.0) {
        v.x = 1.0f;
        return 1.0f;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
    return (float)(1.0 / inv);
}

// always return false?
// 0x6DE880
bool CVehicle::AddSingleWheelParticles(tWheelState wheelState, uint32 arg1, float arg2, float arg3, CColPoint* colPoint, CVector* wheelPos, float arg6, int32 wheelId, uint32 skidmarkType, bool* bloodState, uint32 flags) {
    // NOTE: The original keeps most intermediates on the x87 stack (extended precision), hence the `double`s below.
    const auto playerVeh = FindPlayerVehicle(-1, false);
    if (m_bOffscreen) {
        return false;
    }

    // Distance to the camera
    const auto& vehPos = GetPosition();
    const auto& camPos = TheCamera.GetPosition();
    const double dx = (double)camPos.x - vehPos.x, dy = (double)camPos.y - vehPos.y, dz = (double)camPos.z - vehPos.z;
    const double distSq = (dz * dz + dy * dy) + dx * dx;
    if (distSq > 625.0f && !vehicleFlags.bAlwaysSkidMarks) {
        return false;
    }

    // Only every n-th frame for far away vehicles
    bool canAddParticles = true;
    if (distSq > 400.0f) {
        if (((uint8)m_nModelIndex + CTimer::m_FrameCounter) & 3) { // (sic) only the low byte of the model index
            canAddParticles = false;
        }
    } else if (distSq > 64.0f || !playerVeh) {
        if (((uint8)m_nModelIndex + CTimer::m_FrameCounter) & 1) {
            canAddParticles = false;
        }
    }

    if (!(arg2 < 1.0f)) {
        return false;
    }

    bool inWater = false;
    if (g_surfaceInfos.IsWater(colPoint->m_nSurfaceTypeB)) {
        inWater = true;
    } else if (physicalFlags.bTouchingWater) {
        float waterLevel;
        if (CWaterLevel::GetWaterLevel(wheelPos->x, wheelPos->y, wheelPos->z, waterLevel, true, nullptr) && wheelPos->z <= waterLevel) {
            inWater = true;
        }
    }

    const bool isLowArg3 = arg3 < 1.0f;

    // Sparks (metal on tarmac...)
    if (arg1 == 1 && (arg3 > 0.1f || wheelState == WHEEL_STATE_SPINNING) && g_surfaceInfos.GetFrictionEffect(colPoint->m_nSurfaceTypeB) == 1) {
        CVector sparkDir{
            (float)((double)m_vecMoveSpeed.x * -50.0f),
            (float)((double)m_vecMoveSpeed.y * -50.0f),
            (float)((double)m_vecMoveSpeed.z * -50.0f + 2.5f)
        };
        float sparkForce = (float)((double)arg3 * 32.0f);
        if (wheelState == WHEEL_STATE_SPINNING && arg3 < 0.2f) {
            const CVector back = (m_matrix->GetForward() * m_GasPedal) * -12.0f;
            sparkDir   = CVector{ back.x, back.y, (float)((double)back.z + 2.5f) };
            sparkForce = 10.0f;
        }
        const float sparkMag = NormaliseAndMagExt(sparkDir);
        g_fx.AddSparks(*wheelPos, sparkDir, sparkMag, (int32)sparkForce, m_vecMoveSpeed, SPARK_PARTICLE_SPARK, 0.1f, 0.3f); // 0x49F040
        AudioEngine.ReportCollision(
            this,
            FindPlayerPed(0),
            colPoint->m_nSurfaceTypeA,
            colPoint->m_nSurfaceTypeB,
            *wheelPos,
            nullptr,
            1.0f,
            1.0f,
            false,
            true
        ); // 0x506EB0
    }

    MakeDirty(*colPoint);

    // Smoke / dust particles of a (skidding / fixed) wheel => the particle's size / life depend on the vehicle's type
    const auto AdjustBikeParticleMults = [&](FxPrtMult_c& mults) {
        if (m_nVehicleSubType == VEHICLE_TYPE_BIKE || m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
            mults.m_fSize *= 0.5f;
        } else if (m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            mults.m_fSize *= 0.2f;
            mults.m_fLife *= 0.3f;
        }
    };

    // Used by the skidding & fixed wheel cases (0x6DED1A... & 0x6DEF50...)
    const auto AddTrailOfSmoke = [&] {
        FxPrtMult_c mults{ 0.9f, 0.9f, 1.0f, 0.5f, 0.7f, 1.0f, 0.3f };
        const CVector vel{ 0.0f, 0.0f, 0.5f };
        float countScale = 2.0f;
        if (m_nVehicleSubType == VEHICLE_TYPE_BIKE || m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
            mults.m_fSize *= 0.5f;
            countScale = 3.0f;
        } else if (m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            mults.m_fSize *= 0.2f;
            mults.m_fLife *= 0.3f;
            countScale = 3.0f;
        }

        const CVector step  = m_vecMoveSpeed * CTimer::ms_fTimeStep;
        const int32   num   = (int32)(MagnitudeExt(step) * countScale); // _ftol
        const int32   count = num >= 1 ? num : 1;
        const float   countF = (float)count;
        for (int32 i = 0; i < count; i++) {
            const double rnd = RandomInRangeExt(0.5f, 1.0f);
            mults.m_Color.blue  = (float)rnd;
            mults.m_Color.green = (float)(rnd * 0.9f);
            mults.m_Color.red   = (float)(rnd * 0.9f);

            const float   frac = (float)(1.0 - (double)i / (double)countF);
            const CVector pos  = *wheelPos - step * frac;
            g_fx.m_SmokeII3expand->AddParticle(pos, vel, 0.3f, mults, -1.0f, m_fContactSurfaceBrightness, 0.6f, false); // 0x4AA440
        }
    };

    // Skidmark (+ the quad bike's wheels are offset to the sides)
    const auto AddSkidmark = [&] {
        if (arg1 == 1) {
            return;
        }

        CVector skidPos = *wheelPos;
        if (m_nModelIndex == MODEL_QUAD) {
            const CVector offset = m_matrix->GetRight() * 0.15f;
            if (wheelId < 2) {
                skidPos += offset;
            } else {
                skidPos -= offset;
            }
        }

        if (inWater) {
            return;
        }

        const auto& mat   = m_matrix;
        const float width = FindWheelWidth((uint8)wheelId & 1); // 0xFC
        CSkidmarks::RegisterOne((uint32)wheelId + (uint32)(uintptr_t)this, skidPos, mat->GetForward().x, mat->GetForward().y, (eSkidmarkType)skidmarkType, bloodState, width); // 0x720930
    };

    switch (wheelState) {
    case WHEEL_STATE_SPINNING: { // 0x6DF197
        if (AddWheelDirtAndWater(*colPoint, isLowArg3, true, inWater) && canAddParticles) {
            FxPrtMult_c mults{ 0.9f, 0.9f, 1.0f, 0.5f, 1.0f, 1.0f, 0.5f };
            if (MagnitudeExt(m_vecMoveSpeed) > 0.15f) {
                mults.m_Color.alpha = 0.3f;
                mults.m_fSize       = 0.5f;
            }
            AdjustBikeParticleMults(mults);

            const float gas = fabsf(m_GasPedal);
            CVector     vel;
            vel.x = (float)RandomInRangeExt(0.0f, (float)(((double)gas * m_vecMoveSpeed.x) * -30.0f));
            vel.y = (float)RandomInRangeExt(0.0f, (float)(((double)gas * m_vecMoveSpeed.y) * -30.0f));
            vel.z = (float)((double)(rand() % 10000) * 0.0001f * 0.8f);

            const double rnd = RandomInRangeExt(0.5f, 1.0f);
            mults.m_Color.blue  = (float)rnd;
            mults.m_Color.green = (float)(rnd * 0.9f);
            mults.m_Color.red   = (float)(rnd * 0.9f);
            g_fx.m_SmokeII3expand->AddParticle(*wheelPos, vel, 0.0f, mults, -1.0f, m_fContactSurfaceBrightness, 0.6f, false); // 0x4AA440
        }
        AddSkidmark();
        return false;
    }
    case WHEEL_STATE_SKIDDING: { // 0x6DEF03
        if (flags & 4) {
            return false;
        }
        if (arg3 > 0.03f && AddWheelDirtAndWater(*colPoint, isLowArg3, false, inWater) && canAddParticles) {
            AddTrailOfSmoke();
        }
        AddSkidmark();
        return false;
    }
    case WHEEL_STATE_FIXED: { // 0x6DECEA
        if (arg3 > 0.03f && AddWheelDirtAndWater(*colPoint, isLowArg3, false, inWater) && canAddParticles) {
            AddTrailOfSmoke();
        }
        AddSkidmark();
        return false;
    }
    default: { // 0x6DEBD1
        if (arg3 > 0.03f) {
            AddWheelDirtAndWater(*colPoint, isLowArg3, false, inWater);
        }

        const bool alwaysSkid = vehicleFlags.bAlwaysSkidMarks && ((uint8)wheelId & 1) && std::sqrt(((double)m_vecMoveSpeed.x * m_vecMoveSpeed.x) + (double)m_vecMoveSpeed.y * m_vecMoveSpeed.y) > 0.04f;
        if (!alwaysSkid && !*bloodState && !(flags & 2)) {
            return false;
        }
        AddSkidmark();
        return false;
    }
    }
}

// 0x6DF3D0
bool CVehicle::GetSpecialColModel() {
    if (m_vehicleSpecialColIndex > -1 && m_aSpecialColVehicle[m_vehicleSpecialColIndex] == this) {
        return true;
    }

    const auto specialCMSlot = rng::find(m_aSpecialColVehicle, nullptr);
    if (specialCMSlot == m_aSpecialColVehicle.end()) {
        return false;
    }

    const auto specialCMIdx = rng::distance(m_aSpecialColVehicle.begin(), specialCMSlot);
    m_vehicleSpecialColIndex = specialCMIdx;
    physicalFlags.bAddMovingCollisionSpeed = true;
    *specialCMSlot = this;
    CEntity::RegisterReference(*specialCMSlot);

    auto& cm = m_aSpecialColModel[m_vehicleSpecialColIndex];
    cm.RemoveTrianglePlanes();
    if (!cm.m_pColData) {
        cm.AllocateData();
    }
    cm = *GetModelInfo()->m_pColModel;

    m_aSpecialHydraulicData[specialCMIdx].m_fSuspensionExtendedUpperLimit = 100.f;
    rng::fill(m_aSpecialHydraulicData[specialCMIdx].m_aWheelSuspension, 0.0f);
    return true;
}

// 0x6DF930
void CVehicle::RemoveVehicleUpgrade(int32 upgradeModelIndex) {
    ((void(__thiscall*)(CVehicle*, int32))0x6DF930)(this, upgradeModelIndex);
}

// 0x6DFA20
void CVehicle::AddUpgrade(int32 modelIndex, int32 upgradeIndex) {
    ((void(__thiscall*)(CVehicle*, int32, int32))0x6DFA20)(this, modelIndex, upgradeIndex);
}

// 0x6DFC50
void CVehicle::UpdateTrailerLink(bool arg0, bool arg1) {
    ((void(__thiscall*)(CVehicle*, bool, bool))0x6DFC50)(this, arg0, arg1);
}

// 0x6E0050
void CVehicle::UpdateTractorLink(bool arg0, bool arg1) {
    ((void(__thiscall*)(CVehicle*, bool, bool))0x6E0050)(this, arg0, arg1);
}

// 0x6E0400
CEntity* CVehicle::ScanAndMarkTargetForHeatSeekingMissile(CEntity* entity) {
    return ((CEntity * (__thiscall*)(CVehicle*, CEntity*))0x6E0400)(this, entity);
}

// 0x6E05C0
void CVehicle::FireHeatSeakingMissile(CEntity* targetEntity, eOrdnanceType type, bool arg2) {
    ((void(__thiscall*)(CVehicle*, CEntity*, eOrdnanceType, bool))0x6E05C0)(this, targetEntity, type, arg2);
}

// 0x6E07E0
void CVehicle::PossiblyDropFreeFallBombForPlayer(eOrdnanceType type, bool arg1) {
    ((void(__thiscall*)(CVehicle*, eOrdnanceType, bool))0x6E07E0)(this, type, arg1);
}

// 0x6E0950
void CVehicle::ProcessSirenAndHorn(bool arg0) {
    ((void(__thiscall*)(CVehicle*, bool))0x6E0950)(this, arg0);
}

// NOTSA
auto GetDummyFromLightId(eVehicleLightId lightId, bool isFront) -> eVehicleDummy {
    switch (lightId) {
    case eVehicleLightId::MAIN:      return isFront ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_LIGHT_REAR_MAIN;
    case eVehicleLightId::SECONDARY: return isFront ? DUMMY_LIGHT_FRONT_SECONDARY : DUMMY_LIGHT_REAR_SECONDARY;
    default:                         NOTSA_UNREACHABLE_CASE(lightId);
    }
}

// 0x6E0A50
// lightId here refers to an ordinal of entry of one kind dummy subgroup e.g. headlights here
bool CVehicle::DoHeadLightEffect(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm) {
    return DoLightEffectImpl(true, lightId, vehicleMatrix, isRight, disabledOrAlarm, false);
}

// 0x6E0E20
void CVehicle::DoHeadLightBeam(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight) {
    CVector pointModelSpace = GetDummyPositionObjSpace(GetDummyFromLightId(lightId, true));

    if (lightId == eVehicleLightId::SECONDARY && pointModelSpace.IsZero()) {
        return;
    }

    CVector point = vehicleMatrix.GetPosition() + vehicleMatrix.TransformVector(pointModelSpace);
    if (!isRight) {
        point -= 2 * pointModelSpace.x * vehicleMatrix.GetRight();
    }
    const CVector pointToCamDir = Normalized(TheCamera.GetPosition() - point);
    const auto    alpha         = (uint8)((1.0f - std::fabs(DotProduct(pointToCamDir, vehicleMatrix.GetForward()))) * 32.0f);

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,            RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATECULLMODE,             RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(FALSE));

    const float   angleMult   = ModelIndices::IsForklift(GetModelIndex()) ? 0.5f : 0.15f;
    const CVector lightNormal = Normalized(vehicleMatrix.GetForward() - vehicleMatrix.GetUp() * angleMult);
    const CVector lightRight  = Normalized(CrossProduct(lightNormal, pointToCamDir));
    const CVector lightPos    = point - vehicleMatrix.GetForward() * 0.1f;

    const CVector posn[]      = {
        lightPos - lightRight * 0.05f,
        lightPos + lightRight * 0.05f,
        lightPos + lightNormal * 3.0f - lightRight * 0.5f,
        lightPos + lightNormal * 3.0f + lightRight * 0.5f,
        lightPos + lightNormal * 0.2f
    };
    const uint8 alphas[] = { alpha, alpha, 0, 0, alpha };

    RxObjSpace3DVertex vertices[5];
    for (auto i = 0u; i < std::size(vertices); i++) {
        const RwRGBA color = { 255, 255, 255, alphas[i] };
        RxObjSpace3DVertexSetPreLitColor(&vertices[i], &color);
        RxObjSpace3DVertexSetPos(&vertices[i], &posn[i]);
    }

    if (RwIm3DTransform(vertices, std::size(vertices), nullptr, rwIM3D_VERTEXRGBA | rwIM3D_VERTEXXYZ)) {
        RxVertexIndex indices[] = { 0, 1, 4, 1, 3, 4, 2, 3, 4, 0, 2, 4 };
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indices, std::size(indices));
        RwIm3DEnd();
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,           RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,              RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,             RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,     RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATECULLMODE,              RWRSTATE(rwCULLMODECULLBACK));
}

// 0x6E1440
void CVehicle::DoHeadLightReflectionSingle(CMatrix& vehicleMatrix, bool isRight) {
    DoHeadLightReflectionImpl(vehicleMatrix, (eVehicleLightsFlags)0, !isRight, isRight);
}

// 0x6E1600
void CVehicle::DoHeadLightReflectionTwin(CMatrix& vehicleMatrix) {
    DoHeadLightReflectionImpl(vehicleMatrix, eVehicleLightsFlags::VEHICLE_LIGHTS_TWIN, true, true);
}

// NOTSA
void CVehicle::DoHeadLightReflectionImpl(CMatrix& vehicleMatrix, eVehicleLightsFlags flags, bool includeLeft, bool includeRight) {
    const bool doTwin   = (flags & VEHICLE_LIGHTS_TWIN) ? (includeLeft && includeRight) : ModelIndices::IsCombineHarvester(GetModelIndex());
    const bool doSingle = !doTwin && (includeLeft || includeRight);

    if (doTwin || doSingle) {
        auto vehOffset = GetDummyPositionObjSpace(DUMMY_LIGHT_FRONT_MAIN);
        if (doSingle && includeLeft) {
            vehOffset.x *= -1.f;
        }

        const auto  lightFwd2D     = CVector2D(vehicleMatrix.GetForward()).Normalized();
        const float lightSize      = (doSingle && (IsBike() || GetModelId() == MODEL_QUAD))
            ? 1.25f
            : (doTwin ? vehOffset.x : std::fabs(vehOffset.x)) * 4.0f;

        const float offsetDistance = lightSize * 2.0f + 1.0f + vehOffset.y;
        const auto  shdwFront      = lightFwd2D * (lightSize * 2.0f);
        const auto  shdwSide       = (lightFwd2D * lightSize).GetPerpRight();
        auto        lightPos2D     = lightFwd2D * offsetDistance;
        if (doSingle) {
            const auto lightRight2D = CVector2D(vehicleMatrix.GetRight()).Normalized();
            lightPos2D += lightRight2D * vehOffset.x;
        }

        CShadows::StoreCarLightShadow(
            this,
            reinterpret_cast<int32>(&m_matrix) + 2,
            doTwin ? gpShadowHeadLightsTex : gpShadowHeadLightsTex2,
            GetPosition() + CVector(lightPos2D, 2.0f),
            shdwFront.x, shdwFront.y,
            shdwSide.x, shdwSide.y,
            45, 45, 45,
            7.0f
        );
    }
}

// 0x6E1720
void CVehicle::DoHeadLightReflection(CMatrix& vehicleMatrix, eVehicleLightsFlags flags, bool includeLeft, bool includeRight) {
    DoHeadLightReflectionImpl(vehicleMatrix, flags, includeLeft, includeRight);
}

// 0x6E1780
// lightId here refers to an ordinal of entry of one kind dummy subgroup e.g. taillights here
bool CVehicle::DoTailLightEffect(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm, eVehicleLightsFlags flags_unused, bool staticEmission) {
    return DoLightEffectImpl(false, lightId, vehicleMatrix, isRight, disabledOrAlarm, staticEmission);
}

// NOTSA
// headlight/taillight
bool CVehicle::DoLightEffectImpl(bool isFront, eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm, bool staticEmission) {
    constexpr auto FRONT_SIZE_ROT_MULT = 0.4f; // 0x8D3684
    constexpr auto REAR_SIZE_ROT_MULT  = 0.2f; // 0x8D3688

    if (disabledOrAlarm) {
        return false;
    }

    if (!isFront) {
        if (IsAutomobile()
            && (GetModelIndex() == MODEL_STALLION || GetModelIndex() == MODEL_SABRE)
            && AsAutomobile()->m_damageManager.GetPanelStatus(ePanels::REAR_BUMPER)) {
            // they have light textures mapped on a bumper
            return false;
        }
    }

    CVector dummyPosObjSpace = GetDummyPositionObjSpace(GetDummyFromLightId(lightId, isFront));

    if (lightId == eVehicleLightId::SECONDARY && dummyPosObjSpace.IsZero()) {
        return false;
    }

    CVector tweakedDummy = dummyPosObjSpace;
    if (isFront) {
        const CVector fwd = GetForwardVector();
        tweakedDummy      = 0.05f * fwd + dummyPosObjSpace;
    }

    if (!isRight) {
        tweakedDummy.x -= 2.0f * dummyPosObjSpace.x;
    }

    const CVector dummyPosWorldSpace = vehicleMatrix * tweakedDummy;

    CVector     dirToCam             = TheCamera.GetPosition() - dummyPosWorldSpace;
    const float distToCam            = dirToCam.NormaliseAndMag();

    const CVector forward            = isFront ? vehicleMatrix.GetForward() : -vehicleMatrix.GetForward();

    const float angle                = DotProduct(dirToCam, forward);
    const float normAngle            = std::sqrt(angle);

    const bool isFullsizeTrain       = IsSubTrain() && GetModelIndex() != MODEL_TRAM;
    const auto RegisterCorona        = [&](uintptr id, uint8 r, uint8 g, uint8 b, uint8 intensity, CVector& pos, float size, eCoronaType type, eCoronaReflType reflType, float camDistLimit) {
        CCoronas::RegisterCorona(id, this, r, g, b, intensity, pos, size, 150.0f * TheCamera.m_fLODDistMultiplier, type, eCoronaFlareType::FLARETYPE_NONE, reflType, eCoronaLOSCheck::LOSCHECK_OFF, eCoronaTrail::TRAIL_OFF, normAngle, false, camDistLimit, false, 15.0f, false, false);
    };

    const float baseCorIntensity       = isFront ? 0.3f : 0.2f;
    const float baseCorRotMult         = isFront ? FRONT_SIZE_ROT_MULT : REAR_SIZE_ROT_MULT;
    const auto [intensity, coronaSize] = [&]() {
        const auto ang       = isFront ? normAngle : angle;

        const auto intensity = ang * 0.5f + baseCorIntensity;
        const auto size      = (1.0f - distToCam * (1.0f / 150.0f)) * ang * baseCorRotMult;

        if (!isFullsizeTrain) {
            return std::pair{ intensity, size };
        }

        const auto trainMult = isFront ? 2.0f : 3.0f;
        return std::pair{ std::min(trainMult * intensity, 1.0f), 4.0f * size };
    }();

    bool isBraking = false;

    if (angle > 0.0f
        && (TheCamera.GetActiveCamera().m_nMode != eCamMode::MODE_1STPERSON || this != FindPlayerVehicle())) {
        if (isFront) {
            const auto fieldAngle = isFullsizeTrain ? 0.85f : 0.9f;

            uint8 lightColorR     = 160u;
            uint8 lightColorG     = 160u;
            uint8 lightColorB     = 140u;

            if (normAngle > fieldAngle && distToCam < 40.0f) {
                const auto coronaSize = isFullsizeTrain ? 0.3f : 0.075f;

                if (m_pHandlingData->m_bHalogenLights) {
                    lightColorR = 150u;
                    lightColorG = 150u;
                    lightColorB = 195u;
                }

                RegisterCorona(reinterpret_cast<uintptr>(&m_placement) + 2 * static_cast<uint32>(lightId) + isRight, lightColorR, lightColorG, lightColorB, 255u, tweakedDummy, coronaSize, eCoronaType::CORONATYPE_HEADLIGHTLINE, eCoronaReflType::CORREFL_NONE, 0.3f);
            }

            if (m_pHandlingData->m_bHalogenLights) {
                lightColorR = static_cast<uint8>(190.0f * intensity);
                lightColorG = static_cast<uint8>(intensity * 255.0f);
            } else {
                lightColorR = static_cast<uint8>(210.0f * intensity);
                lightColorG = static_cast<uint8>(intensity * 195.0f);
            }

            RegisterCorona(reinterpret_cast<uintptr>(this) + 2 * static_cast<uint32>(lightId) + isRight, lightColorR, lightColorR, lightColorG, 128u, tweakedDummy, coronaSize, eCoronaType::CORONATYPE_HEADLIGHT, eCoronaReflType::CORREFL_SIMPLE, 0.5f);
        } else {
            uint8 redIntensity = 0u;

            if (m_pDriver && m_BrakePedal > 0.0f && !vehicleFlags.bIsHandbrakeOn) {
                redIntensity = static_cast<uint8>(128.0f * intensity);
                isBraking    = true;
            } else if (staticEmission) {
                redIntensity = static_cast<uint8>(96.0f * intensity);
                isBraking    = true;
            }

            RegisterCorona(reinterpret_cast<uintptr>(&m_placement.m_vPosn.y) + 2 * static_cast<uint32>(lightId) + isRight, redIntensity, 0, 0, 128u, tweakedDummy, coronaSize, eCoronaType::CORONATYPE_HEADLIGHT, eCoronaReflType::CORREFL_SIMPLE, 0.5f);
        }
    }

    return isFront || isBraking;
}

// 0x6E1A60
void CVehicle::DoVehicleLights(CMatrix& vehicleMatrix, eVehicleLightsFlags flags) {
    auto* asAuto = AsAutomobile();

    if (CVehicle::ms_forceVehicleLightsOff) {
        return;
    }

    const bool lightsStatus = CVehicle::GetVehicleLightsStatus();
    if (lightsStatus != vehicleFlags.bLightsOn && GetStatus() != STATUS_WRECKED) {
        if (GetStatus() != STATUS_ABANDONED || IsSubTrain()) {
            vehicleFlags.bLightsOn = lightsStatus;
        } else if (vehicleFlags.bLightsOn) {
            CVector vecCamPos  = TheCamera.GetPosition();
            CVector vecThisPos = GetPosition();
            if (std::abs(vecCamPos.x - vecThisPos.x) + std::abs(vecCamPos.y - vecThisPos.y) > 100.0f) {
                vehicleFlags.bLightsOn = false;
            }
        }
    }

    bool forceOn  = false;
    bool forceOff = false;

    if (m_nOverrideLights) {
        if (m_nOverrideLights == eVehicleOverrideLightsState::FORCE_CAR_LIGHTS_OFF) {
            vehicleFlags.bLightsOn = false;
        } else if (m_nOverrideLights == eVehicleOverrideLightsState::FORCE_CAR_LIGHTS_ON) {
            forceOn = true;
        }
    }

    if (!CanUpdateHornCounter()) {
        // alarm
        if (CTimer::GetTimeInMS() & 0x100) {
            forceOn = true;
        } else {
            forceOff = true;
        }
    }

    if (!vehicleFlags.bEngineOn) {
        return; // can return earlier; moved from 0x6E1DBE
    }

    if (GetModelIndex() == MODEL_ZR350 && GetStatus() != STATUS_WRECKED) {
        // calculate zr350's pop-up lights rotation
        constexpr auto popUpTarget = 0.69813174f; // aka (2.f / 9.f) * PI rad = 40 deg
        if (vehicleFlags.bLightsOn || forceOn || !CanUpdateHornCounter()) {
            asAuto->m_fPropRotate = notsa::step_up_to(asAuto->m_fPropRotate, popUpTarget, CTimer::GetTimeStep() * 0.01f);
            if (asAuto->m_fPropRotate < popUpTarget) {
                return;
            }
        } else {
            asAuto->m_fPropRotate = notsa::step_down_to(asAuto->m_fPropRotate, 0.0f, CTimer::GetTimeStep() * 0.01f);
        }
    }

    const auto IsLightOk = [&](eVehicleLightsFlags disabledFlag, eLights light) {
        return !(flags & disabledFlag)
            && (flags & VEHICLE_LIGHTS_IGNORE_DAMAGE)
            || IsAutomobile() && asAuto->m_damageManager.GetLightStatus(light) == VEHICLE_LIGHT_OK;
    };

    const bool lightOkFR    = IsLightOk(VEHICLE_LIGHTS_DISABLE_FRONT, LIGHT_FRONT_RIGHT);
    const bool lightOkFL    = IsLightOk(VEHICLE_LIGHTS_DISABLE_FRONT, LIGHT_FRONT_LEFT);
    const bool lightOkRR    = IsLightOk(VEHICLE_LIGHTS_DISABLE_REAR, notsa::bugfixes::CDamageManager_GetLightStatus_IncorrectStatusCheckForLightRR ? LIGHT_REAR_RIGHT : LIGHT_REAR_LEFT);
    const bool lightOkRL    = IsLightOk(VEHICLE_LIGHTS_DISABLE_REAR, LIGHT_REAR_LEFT);

    const auto RenderLights = [&](bool isFrontLight, bool disabledOrAlarmR, bool disabledOrAlarmL, bool staticEmission) {
        bool active = CVehicle::DoLightEffectImpl(isFrontLight, eVehicleLightId::MAIN, vehicleMatrix, true, disabledOrAlarmR, staticEmission);
        if (isFrontLight) {
            m_renderLights.m_bRightFront = active;
        } else {
            m_renderLights.m_bRightRear = active;
        }
        if (active) {
            CVehicle::DoLightEffectImpl(isFrontLight, eVehicleLightId::SECONDARY, vehicleMatrix, true, disabledOrAlarmR, staticEmission);
        }
        if (flags & VEHICLE_LIGHTS_TWIN) {
            active = CVehicle::DoLightEffectImpl(isFrontLight, eVehicleLightId::MAIN, vehicleMatrix, false, disabledOrAlarmL, staticEmission);
            if (isFrontLight) {
                m_renderLights.m_bLeftFront = active;
            } else {
                m_renderLights.m_bLeftRear = active;
            }
            if (active) {
                CVehicle::DoLightEffectImpl(isFrontLight, eVehicleLightId::SECONDARY, vehicleMatrix, false, disabledOrAlarmL, staticEmission);
            }
        }
    };

    // can return earlier; moved from 0x6E2739/0x6E271C
    if (forceOff) {
        return;
    }

    if (!vehicleFlags.bLightsOn && !forceOn) {
        // lights are off - process only dynamic part of taillight effect
        RenderLights(false, !lightOkRR, !lightOkRL, false);
    } else {
        // lights are on - process front lights
        const bool alarmOrDisabledFR = forceOff || !lightOkFR;
        const bool alarmOrDisabledFL = forceOff || !lightOkFL;
        RenderLights(true, alarmOrDisabledFR, alarmOrDisabledFL, false);
        // process static part of taillight effect
        const bool alarmOrDisabledRR = forceOff || !lightOkRR;
        const bool alarmOrDisabledRL = forceOff || !lightOkRL;
        RenderLights(false, alarmOrDisabledRR, alarmOrDisabledRL, true);

        if (!IsSubTrain()) {
            // draw light shadows
            CVehicle::DoHeadLightReflectionImpl(vehicleMatrix, flags, lightOkFL, lightOkFR);
        }

        // add directionals
        if (lightOkFR || lightOkFL) {
            CPointLights::AddLight(
                ePointLightType::PLTYPE_DIRECTIONAL,
                vehicleMatrix.GetPosition(),
                vehicleMatrix.GetForward(),
                20.0f,
                1.0f, 1.0f, 1.0f,
                m_vecMoveSpeed.SquaredMagnitude2D() < 0.2025f ? 0u : 1u
            );
        }

        if ((lightOkRR || lightOkRL)
            && m_BrakePedal > 0.0f
            && !vehicleFlags.bIsHandbrakeOn
            && m_pDriver) {
            CPointLights::AddLight(
                ePointLightType::PLTYPE_DIRECTIONAL,
                GetPosition() + -4.0f * GetForward(),
                -vehicleMatrix.GetForward(),
                10.0f,
                0.1f,  // 0x8D368C, StaticRefs
                0.02f, // 0x8D3690
                0.02f, // 0x8D3694
                0u,
                false,
                this
            );
        }
    }
}

// unused
// 0x6E2900
void CVehicle::FillVehicleWithPeds(bool setClothesToAfro) {
    if (setClothesToAfro) {
        const auto playerPed = FindPlayerPed(PED_TYPE_PLAYER1);
        CStats::SetStatValue(STAT_FAT, 1000.0f);
        playerPed->GetPlayerData()->m_pPedClothesDesc->SetModel("afro", CLOTHES_MODEL_HEAD);
        CClothes::RebuildPlayer(playerPed, false);
    }
    const eModelID modelId = setClothesToAfro ? MODEL_PLAYER : MODEL_WMOST;
    if (!CStreaming::IsModelLoaded(modelId)) {
        CStreaming::RequestModel(modelId, STREAMING_KEEP_IN_MEMORY);
        return;
    }
    const auto AddPedToSeat = [modelId, this](int32 seat) {
        CCarEnterExit::SetPedInCarDirect(
            CPopulation::AddPed(PED_TYPE_CIVFEMALE, modelId, GetPosition(), false),
            this,
            seat,
            true
        );
    };
    if (!m_pDriver || !m_pDriver->IsPlayer()) { // BUGFIX: Added `!m_pDriver`
        m_pDriver = nullptr;
        AddPedToSeat(0);
    }
    for (int32 i = 0; i < m_nMaxPassengers; i++) {
        m_apPassengers[i] = nullptr;
        AddPedToSeat(CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(this, i));
    }
}

// 0x6E2E50
bool CVehicle::DoBladeCollision(CVector pos, CMatrix& matrix, int16 rotorType, float radius, float damageMult) {
    static auto& s_TestBladeCol       = StaticRef<CColModel>(0xC1CD38);
    static auto& s_TestBladeColData   = StaticRef<CCollisionData>(0xC1CD68);
    static auto& s_TestBladeColSphere = StaticRef<CColSphere>(0xC1CD98);

    // Set-up collision model for the blade
    {
        CVector bbMin(pos - CVector(radius, radius, radius));
        CVector bbMax(pos + CVector(radius, radius, radius));

        const auto axis = abs(rotorType) - 1;
        assert(axis >= 0 && axis < 3);
        bbMin[axis] = pos[axis] - ROTOR_SEMI_THICKNESS;
        bbMax[axis] = pos[axis] + ROTOR_SEMI_THICKNESS;

        s_TestBladeCol.m_boundBox.Set(bbMin, bbMax);
        s_TestBladeCol.m_boundSphere.Set(radius, pos);
        s_TestBladeCol.m_pColData = &s_TestBladeColData;

        s_TestBladeColSphere.Set(radius, pos, SURFACE_DEFAULT);

        s_TestBladeColData.m_pSpheres = &s_TestBladeColSphere;
        s_TestBladeColData.m_nNumSpheres = 1;
    }

    bool collided = false;

    CWorld::AdvanceCurrentScanCode();
    CWorld::IterateSectorsOverlappedByRect(CRect{ m_matrix->TransformPoint(pos), radius }, [&](int32 x, int32 y) {
        const auto ProcessSector = [&]<typename PtrListType>(PtrListType& list, float damage) {
            return BladeColSectorList(list, s_TestBladeCol, matrix, rotorType, damage);
        };
        auto& s = CWorld::GetSector(x, y);
        auto& rs = CWorld::GetRepeatSector(x, y);
        collided |= ProcessSector(s.Buildings, damageMult);
        collided |= ProcessSector(rs.Vehicles, damageMult);
        collided |= ProcessSector(rs.Peds, 0.0f);
        collided |= ProcessSector(rs.Objects, damageMult);
        return 1;
    });

    s_TestBladeColData.m_nNumSpheres = 0;
    s_TestBladeCol.m_pColData = nullptr;

    return collided;
}

// 0x6E3290
void CVehicle::AddVehicleUpgrade(int32 modelId) {
    ((void(__thiscall*)(CVehicle*, int32))0x6E3290)(this, modelId);
}

// 0x6E3400
void CVehicle::SetupUpgradesAfterLoad() {
    for (auto& upgrade : m_anUpgrades) {
        if (upgrade != -1) {
            AddVehicleUpgrade(std::exchange(upgrade, -1));
        }
    }
}

// 0x6E3440
void CVehicle::GetPlaneWeaponFiringStatus(bool& status, eOrdnanceType& ordnanceType) {
    ((void(__thiscall*)(CVehicle*, bool&, eOrdnanceType&))0x6E3440)(this, status, ordnanceType);
}

// 0x49B010 - Checks if the upgrade model `modelId` can be installed on `vehicle`
bool IsValidModForVehicle(uint32 modelId, CVehicle* vehicle) {
    // NOTE: `CarMod` isn't an `eVehicleMod` here: for upgrade models it holds the component id from the tables
    //       in `CAtomicModelInfo::SetupVehicleUpgradeFlags` (dummy components if `bUsesVehDummy`, chassis components otherwise)
    constexpr uint8 DUMMY_COMP_WHEEL    = 0x2;  // "wheel_"
    constexpr uint8 CHASSIS_COMP_STEREO = 0x11; // "stereo"

    const auto* const vmi = CModelInfo::GetVehicleModelInfo(vehicle->m_nModelIndex);
    const auto* const mi  = CModelInfo::GetModelInfo(modelId);

    if (mi->bUsesVehDummy) {
        if (mi->CarMod == DUMMY_COMP_WHEEL) { // Wheels
            const auto wheelSet = (int32)(int8)vmi->m_nWheelUpgradeClass;
            for (int32 i = 0; i < CVehicleModelInfo::GetNumWheelUpgrades(wheelSet); i++) {
                if ((uint32)CVehicleModelInfo::GetWheelUpgrade(wheelSet, i) == modelId) {
                    return true;
                }
            }
            return false;
        }
    } else if (mi->CarMod == CHASSIS_COMP_STEREO) { // Stereo
        const auto& audio = vehicle->m_vehicleAudio.m_AuSettings;
        if ((uint8)audio.RadioType != 0) { // Not a civilian radio (+0x1D3)
            return false;
        }
        if ((uint8)audio.BassSetting == 1) { // +0x1BE
            return vehicle->vehicleFlags.bUpgradedStereo; // +0x42E & 0x10
        }
        return true;
    }

    // Is it in the list of upgrades of this model?
    return rng::any_of(vmi->m_anUpgrades, [&](int16 upgrade) { return (uint32)(int32)upgrade == modelId; });
}

// 0x6E38F0
bool IsVehiclePointerValid(CVehicle* vehicle) {
    const auto* const pool = GetVehiclePool();
    assert(pool);
    return pool->IsObjectValid(vehicle) && (vehicle->m_nVehicleType == VEHICLE_TYPE_FPLANE || !vehicle->m_pCollisionList.IsEmpty());
}

// 0x6E3950
void CVehicle::ProcessWeapons() {
    ((void(__thiscall*)(CVehicle*))0x6E3950)(this);
}

// 0x73F400
void CVehicle::DoFixedMachineGuns() {
    if (CCamera::GetActiveCamera().m_nDirectionWasLooking != eLookingDirection::LOOKING_DIRECTION_FORWARD)
        return;

    const auto* const driverPad = CPad::GetPad(m_pDriver && m_pDriver->m_nPedType == PED_TYPE_PLAYER2 ? 1 : 0);
    if (driverPad->GetCarGunFired() && !vehicleFlags.bGunSwitchedOff) {
        FireFixedMachineGuns();
    } else if (CTimer::GetTimeInMS() > m_nGunFiringTime + 1400) {
        m_nAmmoInClip = 20;
    }
}

// 0x73DF00
void CVehicle::FireFixedMachineGuns() {
    const auto now = CTimer::GetTimeInMS();
    if (now <= m_nGunFiringTime + 150u) {
        return;
    }

    const auto& fwd = m_matrix->GetForward();
    double len = std::sqrt((double)fwd.y * fwd.y + (double)fwd.x * fwd.x);
    if (0.1f > len) {
        len = 0.1f;
    }
    const double invLen = 1.0 / len;

    m_nGunFiringTime = now;

    // Offset of the end point along the (2D-normalised) forward direction
    const float offX = (float)(invLen * fwd.x);
    const float offY = (float)(invLen * fwd.y);

    // Fires from one gun at `localPos`
    // NOTE: The offsets are `double`s because the first gun uses the unrounded products (in the FPU registers) while the second one uses their float copies
    const auto FireFromGun = [&](CVector localPos, double scaledOffX, double scaledOffY) {
        CVector start = TransformPointExt(*m_matrix, localPos); // 0x59C890
        CVector end{ (float)(scaledOffX + start.x), (float)(start.y + scaledOffY), start.z };

        const auto r1 = rand();
        const auto r2 = rand();
        const float jitterX = (float)(int32)((r2 & 0xFF) - 0x80) * 0.015f;
        const float jitterY = (float)(int32)((r1 & 0xFF) - 0x80) * 0.015f;
        const auto  r3 = rand();
        const double jitterZ = (double)(int32)((r3 & 0xFF) - 0x80) * 0.02f;

        end.x = jitterX + end.x;
        end.y = end.y + jitterY;
        end.z = (float)(end.z + jitterZ);

        CWeapon::DoTankDoomAiming(this, m_pDriver, &start, &end); // 0x73D1E0
        FireOneInstantHitRound(start, end, 15); // 0x73AF00
    };

    const float scaledOffX = (float)(offX * 60.f);
    const float scaledOffY = (float)(offY * 60.f);
    FireFromGun({  2.f, 2.5f, 1.f }, (double)offX * 60.f, (double)offY * 60.f);
    FireFromGun({ -2.f, 2.5f, 1.f }, scaledOffX, scaledOffY);

    AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE_PLANE, WEAPON_M4, this); // 0x506F40

    if (--m_nAmmoInClip == 0) {
        m_nAmmoInClip    = 20;
        m_nGunFiringTime = CTimer::GetTimeInMS() + 1400;
    }
}

// 0x741FD0
void CVehicle::DoDriveByShootings() {
    CPed* const driver = m_pDriver;
    if (!driver) {
        return;
    }

    // NOTE: The original calls the CPlayerPed methods without checking that the driver is actually a player ped
    const auto playerInfo = driver->AsPlayer()->GetPlayerInfoForThisPlayerPed(); // 0x609FF0
    if (!playerInfo || !playerInfo->m_bCanDoDriveBy) {
        return;
    }

    const auto pad = driver->AsPlayer()->GetPadFromPlayer(); // 0x609560
    if (!pad) {
        return;
    }

    CWeapon& weapon = driver->GetActiveWeapon();
    if (CWeaponInfo::GetWeaponInfo(weapon.m_Type, eWeaponSkill::STD)->m_nSlot != +eWeaponSlot::SMG) {
        return;
    }

    bool isBike    = false;
    bool gunFired  = false;
    bool lookLeft  = false;
    bool lookRight = false;

    if (m_nVehicleSubType == VEHICLE_TYPE_BMX) {
        if (pad->GetCarGunFired() == 1) {
            gunFired = true;
        }
    } else if (pad->GetCarGunFired() != 0) {
        gunFired = true;
    }

    AssocGroupId animGroup{};
    int32        animLeft, animRight, animFront;
    if (GetRideAnimData()) {
        isBike    = true;
        animGroup = GetRideAnimData()->AnimGroup;
        animLeft  = ANIM_ID_BIKE_DRIVEBYLHS;
        animRight = ANIM_ID_BIKE_DRIVEBYRHS;
        animFront = ANIM_ID_BIKE_DRIVEBYFT;
    } else {
        animFront = ANIM_ID_NO_ANIMATION_SET;
        if (vehicleFlags.bLowVehicle) {
            animLeft  = ANIM_ID_DRIVEBYL_L;
            animRight = ANIM_ID_DRIVEBYL_R;
        } else {
            animLeft  = ANIM_ID_DRIVEBY_L;
            animRight = ANIM_ID_DRIVEBY_R;
        }
    }

    weapon.Update(nullptr); // 0x73DB40

    {
        auto& cam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        if (cam.m_nMode == MODE_TOPDOWN || TheCamera.m_bObbeCinematicCarCamOn || cam.m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
            lookLeft  = pad->GetLookLeft();
            lookRight = pad->GetLookRight();
        } else {
            lookLeft  = cam.m_bLookingLeft;
            lookRight = cam.m_bLookingRight;
        }
    }

    const auto SetAnimFadeOut = [](CAnimBlendAssociation* assoc) {
        if (assoc) {
            assoc->m_BlendDelta = -8.f;
        }
    };

    if (!(isBike ? gunFired : (lookLeft || lookRight)) || (int32)weapon.m_TotalAmmo <= 0) {
        if (weapon.m_TotalAmmo != 0) {
            const auto clipSize = (int32)CWeaponInfo::GetWeaponInfo(weapon.m_Type, eWeaponSkill::STD)->m_nAmmoClip;
            if (weapon.m_TotalAmmo < (uint32)clipSize) { // NOTE: the original sign-extends the 16-bit clip size (movsx)
                weapon.m_AmmoInClip = weapon.m_TotalAmmo;
            } else {
                weapon.m_AmmoInClip = clipSize;
            }
        }
        SetAnimFadeOut(RpAnimBlendClumpGetAssociation(driver->GetRpClump(), animLeft));
        auto* const assocRight = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), animRight);
        SetAnimFadeOut(assocRight);
        // In the original `assoc` is left as the right-side association for non-bikes
        SetAnimFadeOut(isBike ? RpAnimBlendClumpGetAssociation(driver->GetRpClump(), animFront) : assocRight);
        return;
    }

    CAnimBlendAssociation* assoc;
    int32                  animToUse;
    const auto             BlendIn = [&] {
        assoc = CAnimManager::BlendAnimation(driver->GetRpClump(), animGroup, (AnimationId)animToUse, 16.f);
    };
    bool doAnim = true;
    if (lookLeft) {
        animToUse = animLeft;
    } else if (lookRight) {
        animToUse = animRight;
    } else if (isBike) {
        animToUse = animFront;
    } else {
        doAnim = false;
    }
    if (doAnim) {
        assoc = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), animToUse);
        if (!assoc || assoc->m_BlendDelta < 0.f) {
            BlendIn();
        }
        if (assoc) {
            if ((assoc->m_Flags & 1) != 0) {
                return;
            }
            if (!(assoc->m_BlendAmount > 0.99f)) { // (not `<=`: NaN returns as well)
                return;
            }
        }
    }

    if (gunFired && CTimer::GetTimeInMS() > weapon.m_TimeForNextShotMs) {
        weapon.FireFromCar(this, lookLeft, lookRight);
        weapon.m_TimeForNextShotMs = CTimer::GetTimeInMS() + 70;
        driver->DoGunFlash(250, false);
    }
}

// NOTSA
bool CVehicle::AreAnyOfPassengersFollowerOfGroup(const CPedGroup& group) {
    return rng::any_of(GetMaxPassengerSeats(), [&](CPed* passenger) {
        return group.GetMembership().IsFollower(passenger);
    });
}

/*!
* @notsa
* @return The index of a passenger, or `std::nullopt` if the given ped isn't a passenger.
*/
auto CVehicle::GetPassengerIndex(const CPed* passenger) const -> std::optional<size_t> {
    const auto passengers = GetPassengers();
    const auto it = rng::find(passengers, passenger);
    if (it == passengers.end()) {
        return std::nullopt;
    }
    return (size_t)rng::distance(passengers.begin(), it);
}

bool CVehicle::IsDriverAPlayer() const {
    return m_pDriver && m_pDriver->IsPlayer();
}
