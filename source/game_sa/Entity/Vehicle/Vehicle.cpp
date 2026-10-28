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
static double MagnitudeExt(const CVector& v);  // defined below
static void NormaliseExt(CVector& v);          // defined below

static NOTSA_GLOBAL(fFlyingLiftLimit, 0x8D3670, (float), { 0.5f }); // (BARON) lift limit tweak, 0.5f

NOTSA_GLOBAL(planeRotorDmgTimeMS, 0xC1CC1C, (uint32), {});

NOTSA_GLOBAL(fBurstTyreMod, 0x8D34B4, (float), { 0.13f });                // 0.13f
NOTSA_GLOBAL(fBurstSpeedMax, 0x8D34B8, (float), { 0.3f });               // 0.3f
NOTSA_GLOBAL(CAR_NOS_EXTRA_SKID_LOSS, 0x8D34BC, (float), { 0.9f });      // 0.9f
NOTSA_GLOBAL(WS_TRAC_FRAC_LIMIT, 0x8D34C0, (float), { 0.3f });           // 0.3f
NOTSA_GLOBAL(WS_ALREADY_SPINNING_LOSS, 0x8D34C4, (float), { 0.2f });     // 0.2f
NOTSA_GLOBAL(fBurstBikeTyreMod, 0x8D34C8, (float), { 0.05f });            // 0.05f
NOTSA_GLOBAL(fBurstBikeSpeedMax, 0x8D34CC, (float), { 0.12f });           // 0.12f
NOTSA_GLOBAL(fTweakBikeWheelTurnForce, 0x8D34D0, (float), { 2.0f });     // 2.0f
NOTSA_GLOBAL(AUTOGYRO_ROTORSPIN_MULT, 0x8D34D4, (float), { 0.006f });      // 0.006f
NOTSA_GLOBAL(AUTOGYRO_ROTORSPIN_MULTLIMIT, 0x8D34D8, (float), { 0.25f }); // 0.25f
NOTSA_GLOBAL(AUTOGYRO_ROTORSPIN_DAMP, 0x8D34DC, (float), { 0.997f });      // 0.997f
NOTSA_GLOBAL(AUTOGYRO_ROTORLIFT_MULT, 0x8D34E0, (float), { 4.5f });      // 4.5f
NOTSA_GLOBAL(AUTOGYRO_ROTORLIFT_FALLOFF, 0x8D34E4, (float), { 0.75f });   // 0.75f
NOTSA_GLOBAL(AUTOGYRO_ROTORTILT_ANGLE, 0x8D34E8, (float), { 0.25f });     // 0.25f
NOTSA_GLOBAL(ROTOR_SEMI_THICKNESS, 0x8D34EC, (float), { 0.05f });         // 0.05f
#ifdef NOTSA_GLOBALS_DETACHED
static NOTSA_GLOBAL(s_SpeedMult, 0x8D34F8, (float[6]), { 0.8f, 0.75f, 0.85f, 0.9f, 0.85f, 0.85f });
float* gfSpeedMult = s_SpeedMult;
#else
float* gfSpeedMult = (float*)0x8D34F8;                   // float fSpeedMult[5] = { 0.8f, 0.75f, 0.85f, 0.9f, 0.85f, 0.85f }
#endif
NOTSA_GLOBAL(fDamagePosSpeedShift, 0x8D3510, (float), { 0.4f });         // 0.4f
NOTSA_GLOBAL(DIFF_LIMIT, 0x8D35B4, (float), { 0.8f });                   // 0.8f
NOTSA_GLOBAL(DIFF_SPRING_MULT_X, 0x8D35B8, (float), { 0.05f });           // 0.05f
NOTSA_GLOBAL(DIFF_SPRING_MULT_Y, 0x8D35BC, (float), { 0.05f });           // 0.05f
NOTSA_GLOBAL(DIFF_SPRING_MULT_Z, 0x8D35C0, (float), { 0.1f });           // 0.1f
NOTSA_GLOBAL(DIFF_SPRING_COMPRESS_MULT, 0x8D35C4, (float), { 2.0f });    // 2.0f
NOTSA_GLOBAL(VehicleGunOffset, 0x8D35D4, (std::array<CVector, 13>), { {
    { 0.0f, 4.41f, -1.27f },
    { 0.0f, 2.85f, -0.5f },
    { 0.0f, 2.85f, -0.5f },
    { -0.5f, 2.4f, -0.785f },
    { 0.0f, 6.87f, -1.65f },
    { 2.19f, 1.5f, -0.58f },
    { 0.2f, 0.0f, 0.0f },
    { 1.48f, 0.44f, -0.52f },
    { 0.0f, 0.45f, 0.0f },
    { 2.17f, 1.0f, -0.8f },
    { 2.19f, 1.5f, -0.58f },
    { 3.7f, 0.98f, -1.02f },
    { 3.92f, 0.98f, -1.02f }
} }); // maybe [12]; 13 elements: a 14th would overlay fFlyingLiftLimit and two more floats (aliases.json retype)
namespace { NOTSA_GLOBAL_SYNTH(k_8D3674, 0x8D3674, (float), { 0.85f }); NOTSA_GLOBAL_SYNTH(k_8D3678, 0x8D3678, (float), { 0.25f }); }
#line 63

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
    RH_ScopedVMTInstall(Save, 0x5D4760);
    RH_ScopedVMTInstall(Load, 0x5D2900);

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
    RH_ScopedInstall(FlyingControl, 0x6D85F0);
    RH_ScopedInstall(BladeColSectorList<CPtrListSingleLink<CEntity*>>, 0x6DAF00);
    RH_ScopedInstall(SetComponentRotation, 0x6DBA30);
    RH_ScopedInstall(SetTransmissionRotation, 0x6DBBB0);
    RH_ScopedInstall(DoBoatSplashes, 0x6DD130);
    RH_ScopedInstall(DoSunGlare, 0x6DD6F0);
    RH_ScopedInstall(AddWaterSplashParticles, 0x6DDF60);
    RH_ScopedInstall(AddExhaustParticles, 0x6DE240);
    RH_ScopedInstall(AddSingleWheelParticles, 0x6DE880);
    RH_ScopedInstall(GetSpecialColModel, 0x6DF3D0);
    RH_ScopedInstall(RemoveVehicleUpgrade, 0x6DF930);
    RH_ScopedInstall(AddUpgrade, 0x6DFA20);
    RH_ScopedInstall(UpdateTrailerLink, 0x6DFC50);
    RH_ScopedInstall(UpdateTractorLink, 0x6E0050);
    RH_ScopedInstall(ScanAndMarkTargetForHeatSeekingMissile, 0x6E0400);
    RH_ScopedInstall(FireHeatSeakingMissile, 0x6E05C0);
    RH_ScopedInstall(PossiblyDropFreeFallBombForPlayer, 0x6E07E0);
    RH_ScopedInstall(ProcessSirenAndHorn, 0x6E0950);
    RH_ScopedInstall(DoHeadLightEffect, 0x6E0A50);
    RH_ScopedInstall(DoHeadLightReflectionSingle, 0x6E1440);
    RH_ScopedInstall(DoHeadLightReflectionTwin, 0x6E1600);
    RH_ScopedInstall(DoHeadLightReflection, 0x6E1720);
    RH_ScopedInstall(DoTailLightEffect, 0x6E1780);
    RH_ScopedInstall(DoVehicleLights, 0x6E1A60);
    RH_ScopedInstall(FillVehicleWithPeds, 0x6E2900);
    RH_ScopedInstall(DoBladeCollision, 0x6E2E50);
    RH_ScopedInstall(AddVehicleUpgrade, 0x6E3290);
    RH_ScopedInstall(SetupUpgradesAfterLoad, 0x6E3400);
    RH_ScopedInstall(GetPlaneWeaponFiringStatus, 0x6E3440);
    RH_ScopedInstall(ProcessWeapons, 0x6E3950);
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
    m_nOrdnanceCycleIndex = 0; // NOTSA: asm sweep S19i (exe 0x6D6083: `and byte [esi+0x4A9], 0xF0` clears both cycle indices)
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

    for (auto* particle : { &m_pOverheatParticle, &m_pFireParticle, &m_pDustParticle }) { // 0x6E2C69: the members are cleared
        if (*particle) {
            g_fxMan.DestroyFxSystem(*particle);
            *particle = nullptr;
        }
    }

    if (m_pCustomCarPlate) {
        RwTextureDestroy(m_pCustomCarPlate);
        m_pCustomCarPlate = nullptr;
    }

    const auto iRopeInd = CRopes::FindRope(GetRopeID()); // 0x6E2CD3: this + 0x1D (&m_nFlags + 1), the winch key
    if (iRopeInd >= 0) {
        CRopes::GetRope(iRopeInd).Remove();
    }

    if (!physicalFlags.bRenderScorched && m_fHealth < 250.0F) {
        CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
    }
}

void* CVehicle::operator new(size_t size) {
#ifdef NOTSA_PORTABLE
    GetVehiclePool()->NotsaCheckAllocSize(size, "vehicle");
#endif
    return GetVehiclePool()->New();
}

void CVehicle::operator delete(void* data) {
    GetVehiclePool()->Delete(static_cast<CVehicle*>(data));
}

void* CVehicle::operator new(size_t size, int32 poolRef) {
#ifdef NOTSA_PORTABLE
    GetVehiclePool()->NotsaCheckAllocSize(size, "vehicle");
#endif
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
        vehicleFlags.bIsRCVehicle = false; // 0x6D6AAB: `and byte [this+0x42C], 0x7F` - only this flag; bCreatedAsPoliceVehicle is left alone
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

    if (physicalFlags.bDontCollideWithFlyers // 0x6D66... `test byte [esi+0x43], 1` = bit 24 of physicalFlags (the port tested bSubmergedInWater, bit 8)
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

    const auto fMoveSquared = m_vecMoveSpeed.SquaredMagnitude() * CTimer::GetTimeStep() * CTimer::GetTimeStep(); // 0x6D0EBE/0x6D0EC4: (|v|^2 * dt) * dt, not |v|^2 * (dt * dt)
    if (!(fMoveSquared >= 0.16F)) // 0x6D0ED8: `fcomp; test ah,1; jne` returns for < and for NaN
        return 1;

    auto fMove = sqrt(fMoveSquared);
    if (!TreatAsPlayerForCollisions())
    {
        if (!(fMoveSquared > 0.32F)) // 0x6D0F16: `fcomp; test ah,0x41; jne`
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
    // 0x6D0FA1 / 0x6D0FFB: `fcompp; test ah,0x41; je` keeps the left operand only if it is strictly greater (NaN -> the right operand)
    const auto fRightDir = std::fabs(DotProduct(m_vecMoveSpeed, GetRight()) * CTimer::GetTimeStep() / bbox.GetWidth());
    fLongestDir = fLongestDir > fRightDir ? fLongestDir : fRightDir;
    const auto fUpDir = std::fabs(DotProduct(m_vecMoveSpeed, GetUp()) * CTimer::GetTimeStep() / bbox.GetHeight());
    fLongestDir = fLongestDir > fUpDir ? fLongestDir : fUpDir;

    if (IsBike())
        fLongestDir *= 1.5F;

    if (fLongestDir < 1.0F)
        bProcessCollisionBeforeSettingTimeStep = true;
    else if (fLongestDir < 2.0F)
        unk2 = true;

    return NOTSA_FTOL(uint8, ceil(fMove));
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
    const auto iDirtLevel = NOTSA_FTOL(int32, m_fDirtLevel) & 0xF;
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
            return NOTSA_FTOL(eDoors, fTime);
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
        const bool slow = !(fSpeed > 0.4F); // 0x6DF5BD/0x6DF5F1: `fcomp; test ah,0x41; jne` - <= 0.4 and NaN
        // 0x6DF5A9: `skill >= 100` is the only way to the pro set, then `skill >= 50` the only way to the std set (a NaN skill takes the bad set)
        if (fDrivingSkill >= 100.0F) {
            usedAnims = slow ? &aDriveAnimIdsProSlow : &aDriveAnimIdsPro;
        } else if (fDrivingSkill >= 50.0F) {
            usedAnims = slow ? &aDriveAnimIdsStdSlow : &aDriveAnimIdsStd;
        } else {
            usedAnims = slow ? &aDriveAnimIdsBadSlow : &aDriveAnimIdsBad;
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

    // 0x6DF803: `fcomp 0; test ah,0x41; jne` - a steer angle that is not > 0 (NaN too) takes the right-look branch with the factor [0x871FD0] = -1/0.61,
    // otherwise [0x871FD4] = 1/0.61 (not / 0.61F)
    const bool steerNotPositive = !(m_fSteerAngle > 0.0F);
    auto fUsedAngle = m_fSteerAngle * std::bit_cast<float>(steerNotPositive ? 0xBFD1D608u : 0x3FD1D608u);
    fUsedAngle = std::clamp(fUsedAngle, 0.0F, 1.0F);

    if (steerNotPositive) {
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
    // 0x6D1F30: FCOMP forms on extended precision values (NaN -> 'upright' and 'slow enough')
    const float fUpZ = m_matrix->GetUp().z;
    const auto  Mag2D = [&] { return std::sqrt((x87d)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (x87d)m_vecMoveSpeed.y * m_vecMoveSpeed.y); };
    const auto  TurnSlowEnough = [&] { // 0x6D1FA4
        const x87d sq = ((x87d)m_vecTurnSpeed.x * m_vecTurnSpeed.x + (x87d)m_vecTurnSpeed.y * m_vecTurnSpeed.y) + (x87d)m_vecTurnSpeed.z * m_vecTurnSpeed.z;
        return !(sq > (double)0.0004F);
    };
    const bool tilted = !(fUpZ <= 0.1F) || fUpZ < -0.1F; // |up.z| > 0.1 (or NaN): the car is not on its side

    if (!tilted) {
        if (std::fabs((x87d)m_vecMoveSpeed.z) > 0.05F || Mag2D() > 0.01F) {
            return false;
        }
        return TurnSlowEnough();
    }

    if (IsBoat())
        return true;

    if (bIgnoreSpeedUpright)
        return TurnSlowEnough();

    if (Mag2D() > 0.01F)
        return false;
    if (std::fabs((x87d)m_vecMoveSpeed.z) > 0.05F)
        return false;
    return TurnSlowEnough();
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
    if (fMoveSpeedSquared * 0.01F > sq(CTimer::GetTimeStep()) * sq(0.008F)) // 0x6D2153: fld [0x858C58] (0.01f), fmul st(1) (not / 100.0F)
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

// 0x5D4760
bool CVehicle::Save() {
#ifdef NOTSA_PORTABLE
    uint32 size = (uint32)notsa::FileSizeOf<CVehicleSaveStructure>; // 0x68 on file (the in-memory object is larger on LP64)
#else
    uint32 size = sizeof(CVehicleSaveStructure);
#endif
    CVehicleSaveStructure data;
    data.Construct(this);
    CGenericGameStorage::SaveDataToWorkBuffer(&size, sizeof(uint32)); // Unused, game ignores it on load and uses const value
#ifdef NOTSA_PORTABLE
    CGenericGameStorage::SaveDataToWorkBuffer(data);
#else
    CGenericGameStorage::SaveDataToWorkBuffer(&data, size);
#endif
    return true;
}

// 0x5D2900
bool CVehicle::Load() {
    uint32 size;
    CVehicleSaveStructure data;
    CGenericGameStorage::LoadDataFromWorkBuffer(&size, sizeof(uint32));
#ifdef NOTSA_PORTABLE
    CGenericGameStorage::LoadDataFromWorkBuffer(data); // BUG (kept): the size read above is ignored, the constant record size is used
#else
    CGenericGameStorage::LoadDataFromWorkBuffer(&data, sizeof(CVehicleSaveStructure)); // BUG: Should use the value readen line above this, not constant
#endif
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
        model->SetCustomCarPlateText(nullptr); // 0x6D1119: `push 0` - the text is consumed (the port re-set it from itself)
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

    // 0x6D123C: the dot product accumulates (z + y) + x, the quotient stays on the x87 stack until the negation
    const x87d dot = ((x87d)arg1.z * arg2.z + (x87d)arg1.y * arg2.y) + (x87d)arg1.x * arg2.x;
    return (float)-(dot / arg3);
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
        // 0x6D131E: `health < 250` is `fcomp / test ah,5 / jnp` -> false only for an ordered "less": a NaN health counts as damageable
        return !bDamagedDueToFireOrExplosionOrBullet || GetStatus() != STATUS_PLAYER || !(m_fHealth < 250.0f);
    } else {
        return false;
    }
}

// 0x6D1340
void CVehicle::ProcessDelayedExplosion() {
    if (!m_wBombTimer) {
        return;
    }

    // 0x6D1350..0x6D1375: period = _ftol(step * 100/6) as a 32-bit value, timer = timer > period (unsigned) ? timer - period : 0
    const auto period = (uint32)NOTSA_FTOL(int32, CTimer::GetTimeStep() * (100.0f / 6.0f));
    const auto timer = (uint32)(uint16)m_wBombTimer; // movzx
    m_wBombTimer = (int16)(timer > period ? timer - period : 0u);

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

    // 0x6D14F2: the exe nudges the vehicle for the entering ped before any seat test (even when the seat is invalid / taken)
    ApplyTurnForceToOccupantOnEntry(passenger);

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
            {
                const auto maxArmour = (float)FindPlayerInfo(0).m_nMaxArmour;
                driver->m_fArmour = maxArmour > driver->m_fArmour ? maxArmour : driver->m_fArmour; // ASM-S19j 0x6D177E: `fcom; test ah,0x41; je`: NaN keeps the armour
            }
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

    // ASM-S19j 0x6D17D5-0x6D1870: SetDriver has its OWN turn force (not AddPassenger's): factor mass * -0.02 for both vehicle types;
    // a bike only gets a move force of up * (mass * -0.02) (no turn force, no offset); everything else a turn force (0, 0, mass * -0.02)
    // applied at (driver.x - this.x, driver.y - this.y, 0) read from the raw matrix positions
    const x87d forceZ = (x87d)driver->m_fMass * -0.02f;
    if (m_nVehicleType == VEHICLE_TYPE_BIKE) {
        const auto& up = m_matrix->GetUp();
        ApplyMoveForce(CVector{ (float)(forceZ * up.x), (float)(forceZ * up.y), (float)(forceZ * up.z) });
    } else {
        const CVector point{ driver->m_matrix->GetPosition().x - m_matrix->GetPosition().x, driver->m_matrix->GetPosition().y - m_matrix->GetPosition().y, 0.0f };
        ApplyTurnForce(CVector{ 0.0f, 0.0f, (float)forceZ }, point);
    }
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
            if (CStreaming::IsModelLoaded(MODEL_CHROMEGUN)) {
                if (vehicleFlags.bFreebies) {
                    if (playerPed->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_SHOTGUN, true)) {
                        playerPed->GiveWeapon(eWeaponType::WEAPON_SHOTGUN, 5, true);
                    } else {
                        playerPed->GrantAmmo(eWeaponType::WEAPON_SHOTGUN, 5);
                    }
                    vehicleFlags.bFreebies = false;
                }
                // exe 0x6D19FD: the model is released even when the freebies were already taken
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
    // 0x6D1E80: only a bike or a car that is not lying on its side (|up.z| > 0.1, a NaN counts) can be entered, and only while it is (almost) at rest;
    // the squared speeds are summed x, y, z in extended precision and compared with 0.04 (FCOMP + JE: a NaN passes)
    const auto upZ = GetUp().z;
    if (!(IsBike() || !(upZ <= 0.1f) || upZ < -0.1f)) {
        return false;
    }
    const auto SqMag = [](const CVector& v) { return ((x87d)v.x * v.x + (x87d)v.y * v.y) + (x87d)v.z * v.z; };
    return !(SqMag(m_vecMoveSpeed) > (double)sq(0.2f)) && !(SqMag(m_vecTurnSpeed) > (double)sq(0.2f));
}

// 0x6D21F0
void CVehicle::ProcessCarAlarm() {
    switch (m_nAlarmState) {
    case 0:
    case std::numeric_limits<decltype(m_nAlarmState)>::max(): { // Doing this in case we ever the underlying type.
        return;
    }
    }

    // 0x6D2207: the exe converts the step with _ftol (signed 32 bit), compares the 16 bit alarm timer against it unsigned and SUBTRACTS it (the port stored the step itself)
    const auto ts = (uint32)NOTSA_FTOL(int32, CTimer::GetTimeStepInMS());
    if (m_nAlarmState >= ts) {
        m_nAlarmState = (uint16)(m_nAlarmState - ts);
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
            if (m_pWhoDetonatedMe) { // 0x6D25AC: the exe registers the reference only for a non-null bomber (the port dereferenced null)
                CEntity::RegisterReference(m_pWhoDetonatedMe);
            }
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
    case eFlightModel::FLIGHT_MODEL_BARON:
    case eFlightModel::FLIGHT_MODEL_RC: { // 0x6D2604 & 0x6D2609: models 1 AND 2 take this path
        // 0x6D2630: FCOMP + JP / JNP, a NaN height takes the last branch
        if (!(height < 500.f)) {
            if (height < 950.f) {
                return height - 500.f;
            }

            if (!(height < 1500.f)) {
                return (height - 1000.f) + 500.f;
            }
        }
        return -1.f;
    }
    default: {
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
    const auto toSet = static_cast<eAtomicComponentFlag>(reinterpret_cast<uintptr_t>(data));
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
        RwFrameForAllObjects(component, SetVehicleAtomicVisibilityCB, (void*)(uintptr_t)visibilityState);
        RwFrameForAllChildren(component, SetVehicleAtomicVisibilityCB, (void*)(uintptr_t)visibilityState);
    }
}

// 0x6D2740
void CVehicle::ApplyBoatWaterResistance(tBoatHandlingData* boatHandling, float fImmersionDepth) {
    // 0x6D2752..0x6D2769: ((depth * suspensionForceLevel) * depth) * mass * 0.001 (the depth is multiplied in twice, NOT squared first: a different float rounding)
    float fSpeedMult = fImmersionDepth * m_pHandlingData->m_fSuspensionForceLevel * fImmersionDepth * m_fMass * 0.001F; // 0x6D2769 fmul [0x858CDC] (not / 1000.0F)
    if (m_nModelIndex == MODEL_SKIMMER) {
        fSpeedMult *= 30.0F;
    }

    auto fMoveDotProduct = DotProduct(m_vecMoveSpeed, GetForward());
    fSpeedMult *= sq(fMoveDotProduct) + 0.05F;
    fSpeedMult += 1.0F;
    if (fSpeedMult < 0.0F) { // 0x6D27A6: `fcom 0; test ah,5; jp` -> fchs only for an ordered negative (NaN / -0 keep their sign)
        fSpeedMult = -fSpeedMult;
    }
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

    if (!(m_vecMoveSpeed.z > 0.0F)) // 0x6D2907: `fcomp; test ah,0x41; jne` - <= 0 and NaN take the blended factor
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
        return curr; // 0x6D29B0: `cmp eax, 0xFF; jge` stores the alpha unchanged when it is already >= 255 (the port stored 255)
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

    const auto rnd = CGeneral::GetRandomNumberInRange<size_t>(0u, std::size(m_apPassengers));
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

    if (!(m_fHealth < 650.0f) || /* ASM-S19j 0x6D2A99: jp = kill for >= and NaN */ m_fHealth < 250.0f || physicalFlags.bSubmergedInWater) {
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
        m_pOverheatParticle->SetConstTime(1u, 1.0f - (m_fHealth - 250.0f) * ExeRecip(400.0f));
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

static x87d RandomInRangeExt(float min, float max); // defined below (0x41BD90)

// 0x6D2D50
bool CVehicle::AddWheelDirtAndWater(CColPoint& colPoint, bool isProduceWheelDrops, bool isWheelsSpinning, bool isWheelInWater) {
    // NOTE: the exe reads the surface id from CColPoint+0x23 (m_nSurfaceTypeB) for every check below (also IsSand / CreatesWheelSpray)
    const auto surfaceB = colPoint.m_nSurfaceTypeB;
    if (!isProduceWheelDrops && !g_surfaceInfos.IsSand(surfaceB)) {
        return false;
    }

    if (isWheelInWater) {
        g_fx.AddWheelSpray(this, colPoint.m_vecPoint, isWheelsSpinning, true, m_fContactSurfaceBrightness);
        return false;
    }

    const auto CreateFxForSurface = [&](auto CheckSurface, auto AddFx) {
        if ((g_surfaceInfos.*CheckSurface)(surfaceB)) {
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

    // Dust and sand: when the roads are wet (WetRoads > 0; NaN does not roll) a random roll in [WetRoads, 1.01] > 0.5 skips the
    // effect AND returns false right away (the roll is only made for dust / sand surfaces; the exe returns, it does not fall through to the spray)
    const auto DustOrSand = [&](auto CheckSurface, auto AddFx) -> std::optional<bool> {
        if (!(g_surfaceInfos.*CheckSurface)(surfaceB)) {
            return std::nullopt;
        }
        if (CWeather::WetRoads > 0.0f) {
            const x87d roll = RandomInRangeExt(CWeather::WetRoads, 1.01f); // 0x41BD90
            if (roll > 0.5f) {
                return false;
            }
        }
        (g_fx.*AddFx)(this, colPoint.m_vecPoint, isWheelsSpinning, m_fContactSurfaceBrightness);
        return false;
    };
    if (const auto r = DustOrSand(&SurfaceInfos_c::CreatesWheelDust, &Fx_c::AddWheelDust)) {
        return *r;
    }
    if (const auto r = DustOrSand(&SurfaceInfos_c::CreatesWheelSand, &Fx_c::AddWheelSand)) {
        return *r;
    }

    if (g_surfaceInfos.CreatesWheelSpray(surfaceB) && CWeather::WetRoads > 0.4f && !CCullZones::CamNoRain()) {
        g_fx.AddWheelSpray(this, colPoint.m_vecPoint, isWheelsSpinning, false, m_fContactSurfaceBrightness);
        return false;
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
        if (m_vehicleAudio.m_AuSettings.RadioType == AE_RT_CIVILIAN && vehicleFlags.bUpgradedStereo) { // 0x6D3272: `test al, al; jne` skips non-civilian radios (the port had `!=`)
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
static NOTSA_GLOBAL(CopyObjectsCB_TargetClump, 0xC1CB58, (RpClump*), {});
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
// NOTE: Only `CreateUpgradeAtomic` (0x6D35EA) sets `ATOMIC_ALPHA`; `CreateReplacementAtomic` (0x6D37F6) and `AddReplacementUpgrade` (0x6D38F3) do not.
static void SetupUpgradeAtomicRendering(RpAtomic* atomic, bool isDamaged, bool setAlphaFlag) {
    RpMaterial* hasAlphaMaterial = nullptr;
    RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), CVehicleModelInfo::HasAlphaMaterialCB, &hasAlphaMaterial);
    if (hasAlphaMaterial) {
        CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB);
        if (setAlphaFlag) {
            CVisibilityPlugins::SetAtomicFlag(atomic, ATOMIC_ALPHA);
        }
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

    SetupUpgradeAtomicRendering(atomic, isDamaged, true);

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
        // 0x6D3743-0x6D377B: the exe ORs 0x20003 into the flags BEFORE `RwMatrixUpdate`, which clears them again (`and flags, 0xFFFDFFFC`);
        // the port ORed them in after the update and left the matrix flagged as identity
        CMatrix::GetIdentity().UpdateRwMatrix(mat);
        CVisibilityPlugins::SetFrameHierarchyId(frame, 0);
        RwFrameAddChild(parentFrame, frame);
    } else {
        RpAtomicSetFrame(atomic, parentFrame);
        RwFrameDestroy(frame);
    }

    CVisibilityPlugins::SetAtomicId(atomic, flags & ~eAtomicComponentFlag::ATOMIC_MASK);
    CVisibilityPlugins::SetAtomicFlag(atomic, isDamaged ? eAtomicComponentFlag::ATOMIC_DAMAGED : eAtomicComponentFlag::ATOMIC_OK);

    SetupUpgradeAtomicRendering(atomic, isDamaged, false);

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

    SetupUpgradeAtomicRendering(atomic, false, false); // NOTE: 0x6D38D5 - 0x6D390B

    CDamageAtomicModelInfo::ms_bCreateDamagedVersion = false;

    if (nodeId == CAR_EXHAUST) {
        if (m_pHandlingData->m_bDoubleExhaust) {
            const auto second = CreateReplacementAtomic(mi, frame, (eAtomicComponentFlag)oldAtomicFlags, false, true);
            const auto mat = RwFrameGetMatrix(RpAtomicGetFrame(second));
            mat->pos.x = (float)((x87d)RwFrameGetMatrix(frame)->pos.x * -2.0f); // 0x858B18 = -2.0f
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

    const auto GetZAndSegmentCount = [&, this]() -> std::pair<float, int32> {
        const auto baseLen = (eRopeType)m_ropeType == eRopeType::MAGNET ? -0.2f : -0.6f;
        if (const auto ropeIdx = CRopes::FindRope(ropeID); ropeIdx >= 0) { // Inverted condition
            const auto& rope = CRopes::GetRope(ropeIdx);
            const auto segCount = NOTSA_FTOL(int32, rope.m_fSegmentLength * 32.f); // signed `_ftol` + `fild` in the exe
            // The exe keeps the products and the difference unrounded on the x87 stack; only the final sum is stored as a float
            return { (float)(((x87d)rope.m_fMass * rope.m_fSegmentLength) - ((x87d)segCount * rope.m_fTotalLength) + baseLen), segCount };
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

// The exe looks the rope up with CRopes::FindRope(&this->m_nFlags + 1) (0x556000, key = this + 0x1D) and indexes aRopes with the result (imul 0x328): 0x6D3CB0 / 0x6D3CD0 / 0x6D3CF0 / 0x6D3D10 / 0x6D3D30.
// (the port indexed aRopes with the key itself, i.e. with a heap address). A missing rope yields -1 and reads the memory before aRopes, as the exe does.
static int32 FindWinchRope(const CVehicle& veh) { return CRopes::FindRope(veh.GetRopeID()); }

// 0x6D3CB0
void CVehicle::ReleasePickedUpEntityWithWinch() const {
    return CRopes::GetRope(FindWinchRope(*this)).ReleasePickedUpObject();
}

// 0x6D3CD0
void CVehicle::PickUpEntityWithWinch(CEntity* entity) const {
    return CRopes::GetRope(FindWinchRope(*this)).PickUpObject(entity);
}

// 0x6D3CF0
CEntity* CVehicle::QueryPickedUpEntityWithWinch() const {
    return CRopes::GetRope(FindWinchRope(*this)).m_pRopeAttachObject;
}

// 0x6D3D10
float CVehicle::GetRopeHeightForHeli() const {
    return CRopes::GetRope(FindWinchRope(*this)).m_fSegmentLength;
}

// 0x6D3D30
void CVehicle::SetRopeHeightForHeli(float height) const {
    CRopes::GetRope(FindWinchRope(*this)).m_fSegmentLength = height;
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
        AsPlane()->m_nFiringMultiplier = NOTSA_FTOL(uint8, multiplier * 16.0f);
        break;
    case VEHICLE_TYPE_HELI:
        AsHeli()->m_nFiringMultiplier = NOTSA_FTOL(uint8, multiplier * 16.0f);
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
        return NOTSA_FTOL(uint32, 40.0f / mult);
    case MODEL_RUSTLER:
        return NOTSA_FTOL(uint32, 80.0f / mult);
    case MODEL_HYDRA:
        return NOTSA_FTOL(uint32, 17.0f / mult);
    case MODEL_CARGOBOB:
        return NOTSA_FTOL(uint32, 100.0f / mult);
    case MODEL_TORNADO:
        return NOTSA_FTOL(uint32, 45.0f / mult);
    default:
        return NOTSA_FTOL(uint32, 60.0f / mult);
    }
}

// 0x6D4290
CVector CVehicle::GetPlaneGunsPosition(int32 gunId) {
    CVector pos = GetModelInfo()->AsVehicleModelInfoPtr()->GetModelDummyPosition(DUMMY_VEHICLE_GUN);
    if (pos.IsZero()) {
        switch (m_nModelIndex) {
        case MODEL_HUNTER:   pos = VehicleGunOffset[0]; break;
        case MODEL_SEASPAR:  pos = CVector(-0.5f, 2.4f, -0.785f); break; // 0x8D35F8
        case MODEL_RCBARON:  pos = CVector(0.0f, 0.45f, 0.0f); break;    // 0x8D3634
        case MODEL_RUSTLER:  pos = CVector(2.19f, 1.5f, -0.58f); break;  // 0x8D3610
        case MODEL_MAVERICK: pos = CVector(0.0f, 2.85f, -0.5f); break;   // 0x8D35E0
        case MODEL_POLMAV:   pos = CVector(0.0f, 2.85f, -0.5f); break;   // 0x8D35EC [Values same as the maverick's]
        case MODEL_HYDRA:    pos = CVector(1.48f, 0.44f, -0.52f); break; // 0x8D3628
        case MODEL_CARGOBOB: pos = CVector(0.0f, 6.87f, -1.65f); break;  // 0x8D3604
        case MODEL_TORNADO:  pos = CVector(); break;                       // 0xC1CC2C
        default:             pos = CVector(0.f, 0.f, 0.f); break;
        }
    }
    // 0x6D444E: the exe adds the per-gun offset of the Rustler (0x8D361C) for the dummy position as well as for the table one
    if (m_nModelIndex == MODEL_RUSTLER) {
        pos += CVector{ 0.2f, 0.0f, 0.0f } * (float)(gunId - 1);
    }
    return pos;
}

// 0x6D4590
uint32 CVehicle::GetPlaneOrdnanceRateOfFire(eOrdnanceType type) {
    const auto mult = GetFiringRateMultiplier();
    switch (m_nModelIndex) {
    case MODEL_HUNTER:
        return NOTSA_FTOL(uint32, (float)500 / mult);
    case MODEL_SEASPAR:
    case MODEL_RUSTLER:
        return NOTSA_FTOL(uint32, (float)1000 / mult);
    case MODEL_HYDRA:
        return NOTSA_FTOL(uint32, (float)(type != 1 ? 1000 : 500) / mult); // todo: heat trap / missile?
    default:
        return NOTSA_FTOL(uint32, (float)350 / mult);
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
            return ModelGunInfo{ true, 224u, WEAPON_M4 }; // ASM-S19f: 0x6D4DBE stores 0xE0 (the port had 225)
        }
        case MODEL_HYDRA:
        case MODEL_TORNADO:
            return ModelGunInfo{ true, 128u, WEAPON_MINIGUN }; // ASM-S19f: 0x6D4DDA stores 0x80 (the port had 123)
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

    // ASM-S19f: 0x6D4E85-0x6D4FBA. The exe fires the instant hit first and only then plays the effect (the port did it the other way
    // round, which swaps the rand() draws of the gunshell and the shot), and the second gun of the twin-gun planes is the first one
    // mirrored in x and transformed again (the port reused the first position for both shots)
    const auto FireGun = [&](const auto gunId) {
        weapon.FireInstantHit(this, &gunShellPos, &gunShellPos);
        DoPlaneGunFireFX(
            &weapon,
            planeGunPosMS,
            gunShellPos,
            gunId
        );
    };

    if (giIsDoubleGun) {
        FireGun(m_nGunsCycleIndex * 2);

        planeGunPosMS   = GetPlaneGunsPosition(m_nGunsCycleIndex); // 0x6D4EC9
        planeGunPosMS.x = -planeGunPosMS.x;
        gunShellPos     = velocityOffset + m_matrix->TransformPoint(planeGunPosMS);
        FireGun(m_nGunsCycleIndex * 2 + 1);
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

namespace {
// Helpers that mimic the original's x87 (extended precision) vector math, term order included.
// (The shared `CVector` / `CMatrix` helpers are float-based, which gives slightly different results.)

// 0x59C730 - a x b, each component computed in extended precision, then stored as float
CVector Cross_x87(const CVector& a, const CVector& b) {
    return {
        (float)((x87d)a.y * b.z - (x87d)a.z * b.y),
        (float)((x87d)a.z * b.x - (x87d)a.x * b.z),
        (float)((x87d)a.x * b.y - (x87d)a.y * b.x)
    };
}

// 0x59C790 - Rotates `v` by the 3x3 part of `m` (no translation)
CVector Multiply3x3_x87(const CMatrix& m, const CVector& v) {
    return {
        (float)(((x87d)m.GetUp().x * v.z + (x87d)m.GetForward().x * v.y) + (x87d)m.GetRight().x * v.x),
        (float)(((x87d)m.GetUp().y * v.z + (x87d)m.GetRight().y * v.x) + (x87d)m.GetForward().y * v.y),
        (float)(((x87d)m.GetUp().z * v.z + (x87d)m.GetRight().z * v.x) + (x87d)m.GetForward().z * v.y)
    };
}

// 0x59C890 - Transforms `v` by `m` (rotation + translation). The translation is added before the result is rounded to float
CVector TransformPoint_x87(const CMatrix& m, const CVector& v) {
    const auto& p = m.GetPosition();
    return {
        (float)((((x87d)m.GetUp().x * v.z + (x87d)m.GetForward().x * v.y) + (x87d)m.GetRight().x * v.x) + (x87d)p.x),
        (float)((((x87d)m.GetUp().y * v.z + (x87d)m.GetRight().y * v.x) + (x87d)m.GetForward().y * v.y) + (x87d)p.y),
        (float)((((x87d)m.GetUp().z * v.z + (x87d)m.GetRight().z * v.x) + (x87d)m.GetForward().z * v.y) + (x87d)p.z)
    };
}

// 0x406DA0 - x^2 + y^2 + z^2 (the result stays in extended precision in the original)
double SquaredMagnitude_x87(const CVector& v) {
    return ((x87d)v.x * v.x + (x87d)v.y * v.y) + (x87d)v.z * v.z;
}

// 0x59C910 - In place normalisation
void Normalise_x87(CVector& v) {
    const double sq = SquaredMagnitude_x87(v);
    if (sq > 0.0 || std::isnan(sq)) {
        const x87d inv = (x87d)1.0 / X87R(std::sqrt(sq));
        v.x = (float)(inv * v.x);
        v.y = (float)(inv * v.y);
        v.z = (float)(inv * v.z);
    } else {
        v.x = 1.f; // Note: y/z are left as-is
    }
}
}; // namespace

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
        // Fire one missile from each hard-point (the second one is mirrored along X)
        CVector ordnancePos = GetPlaneOrdnancePosition(type);
        if (i == 1) {
            ordnancePos.x = -ordnancePos.x;
        }

        // This places a point somewhere in front of us, depending on our velocity's direction (x87 evaluation order, see asm)
        const auto& fwd      = m_matrix->GetForward();
        const x87d speedFwd = ((x87d)m_vecMoveSpeed.z * fwd.z + (x87d)m_vecMoveSpeed.y * fwd.y) + (x87d)m_vecMoveSpeed.x * fwd.x;
        const x87d s        = speedFwd < (x87d)0.0 ? (x87d)0.0 : speedFwd; // Note: NaN is passed through
        const float  fy       = (float)(s * fwd.y);
        const float  fz       = (float)(s * fwd.z);
        const CVector offset{
            (float)((s * fwd.x) * CTimer::ms_fTimeStep),
            (float)((x87d)fy * CTimer::ms_fTimeStep),
            (float)((x87d)fz * CTimer::ms_fTimeStep)
        };

        const CVector posn = TransformPoint_x87(*m_matrix, ordnancePos);
        const CVector origin{ offset.x + posn.x, offset.y + posn.y, offset.z + posn.z };
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
    // ASM-S19e: the exe draws the look duration BEFORE the coin flip (0x6D5496 / 0x6D5539) and draws both durations even when
    // nobody is in the car => rand() sequence is: dur, [driver&&passenger: flip], dur, [passenger: flip]
    const auto DoReact = [=](CPed* pedThatLooks, CEntity* pedToLookAt, int32 duration) {
        g_ikChainMan.LookAt("ReactToVhclDam", pedThatLooks, pedToLookAt, duration, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
    };

    const auto& passenger = m_apPassengers[0];

    const auto driverDuration = CGeneral::GetRandomNumberInRange(2000, 5000);
    if (m_pDriver) {
        if (passenger) {
            DoReact(m_pDriver, CGeneral::DoCoinFlip() ? dmgCauser : passenger, driverDuration);
        } else {
            DoReact(m_pDriver, dmgCauser, driverDuration);
        }
    }

    const auto passengerDuration = CGeneral::GetRandomNumberInRange(2000, 5000);
    if (passenger) {
        DoReact(passenger, CGeneral::DoCoinFlip() ? dmgCauser : m_pDriver, passengerDuration);
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
// Rewritten from the asm (oracle bit-exact): the x87 stack keeps `right`, the clamp bounds and the force magnitudes unrounded (double here), the spill points are
// the float temporaries. The old port differed in: dot product order, the coast-brake test (`mass < 500` not `brake > 500`), the burst-tyre random term,
// the traction-limit flags, the force vector assembly (spilled products) and the move/turn force magnitudes (inlined GetMass in extended precision).
void CVehicle::ProcessWheel(CVector& wheelFwd, CVector& wheelRight,
                            CVector& wheelContactSpeed, CVector& wheelContactPoint,
                            int32 wheelsOnGround,
                            float thrust, float brake, float adhesion,
                            int8 wheelId, float* wheelSpeed,
                            tWheelState* wheelState, uint16 wheelStatus
) {
    NOTSA_GLOBAL_LOCAL(bBraking, 0xC1CDAE, (bool), {}); // false
    NOTSA_GLOBAL_LOCAL(bDriving, 0xC1CDAD, (bool), {}); // false
    NOTSA_GLOBAL_LOCAL(bAlreadySkidding, 0xC1CDAC, (bool), {}); // false

    const auto  statusIsPlayerLike = [&] { const auto st = (int)GetStatus(); return st == 0 || st == 8; }; // `shr al, 3; test al, al; je / cmp al, 8`
    const auto& cs = wheelContactSpeed;

    // 0x6D6C10 - ((y + x) + z), spilled to float
    const float contactSpeedFwd = (float)(((x87d)wheelFwd.y * cs.y + (x87d)wheelFwd.x * cs.x) + (x87d)cs.z * wheelFwd.z);

    bBraking = !(brake == 0.0f); // `fcom; test ah, 0x44; jnp`: NaN counts as braking
    bDriving = !bBraking && !(thrust == 0.0f);

    adhesion = (float)((x87d)CTimer::GetTimeStep() * adhesion); // spilled
    if (*wheelState != WHEEL_STATE_NORMAL) {
        adhesion = (float)((x87d)adhesion * m_pHandlingData->m_fTractionLoss);
        bAlreadySkidding = true;
        if (*wheelState == WHEEL_STATE_SPINNING && statusIsPlayerLike()) {
            adhesion = (float)(((x87d)1.0 - std::fabs((x87d)m_GasPedal) * WS_ALREADY_SPINNING_LOSS) * adhesion);
        }
    }
    *wheelState = WHEEL_STATE_NORMAL;

    // 0x6D6CEC - ((y + z) + x), spilled to float
    const float contactSpeedRight = (float)(((x87d)wheelRight.y * cs.y + (x87d)wheelRight.z * cs.z) + (x87d)wheelRight.x * cs.x);

    x87d right = (x87d)0.0;   // the x87 stack value
    float  fwd   = 0.0f;  // [esp + 0x10]
    if (!(contactSpeedRight == 0.0f)) {
        const float rightF = (float)(-((x87d)contactSpeedRight / wheelsOnGround)); // fild; fdivr; fchs; fst
        right = rightF;
        if (wheelStatus == WHEEL_STATUS_BURST) {
            const float speed = (contactSpeedFwd < fBurstSpeedMax) ? contactSpeedFwd : fBurstSpeedMax;
            const float hi    = fBurstTyreMod;
            const float lo    = (float)((x87d)fBurstTyreMod * -1.0f);
            const x87d rnd  = (x87d)rand() * (x87d)RAND_MAX_FLOAT_RECIPROCAL; // 0x6D6D76: rnd * (hi - lo) + lo
            right = ((rnd * ((x87d)hi - lo) + lo) * speed) + rightF;
        }
    }

    if (bDriving) {
        fwd = thrust;
        if (right > 0.0) {
            if (!(right <= adhesion)) {
                right = adhesion;
            }
        } else {
            const float negAdh = -adhesion;
            if (right < negAdh) {
                right = negAdh;
            }
        }
    } else if (!(contactSpeedFwd == 0.0f)) {
        fwd = (float)(-((x87d)contactSpeedFwd / wheelsOnGround));
        x87d brakeV = brake;
        if (!bBraking && std::fabs(m_GasPedal) < 0.01f) { // `fcomp 0.01; test ah, 5; jp` => NaN keeps the passed brake
            const x87d fric = gHandlingDataMgr.fWheelFriction;
            if (m_nVehicleType == VEHICLE_TYPE_BIKE) {
                brakeV = (fric * 0.6f) / ((x87d)m_pHandlingData->m_fMass + 200.0f);
            } else if (IsSubPlane()) {
                brakeV = (x87d)0.0;
            } else {
                brakeV = fric / m_pHandlingData->m_fMass;
                if (m_pHandlingData->m_fMass < 500.0f) {
                    brakeV *= 0.1f;
                } else if (m_nModelIndex == MODEL_RCBANDIT) {
                    brakeV *= 0.2f;
                }
            }
        }
        if (!(brakeV <= adhesion)) {
            if (std::fabs(contactSpeedFwd) > 0.005f) {
                *wheelState = WHEEL_STATE_FIXED;
            }
        } else if (fwd > 0.0f) {
            if (fwd > brakeV) {
                fwd = (float)brakeV;
            }
        } else {
            const x87d negBrake = -brakeV;
            if (fwd < negBrake) {
                fwd = (float)negBrake;
            }
        }
    }

    // 0x6D6F32
    const x87d sumSq  = right * right + (x87d)fwd * fwd;
    const float  sumSqF = (float)sumSq;
    if (sumSq > (x87d)adhesion * adhesion) {
        if (*wheelState != WHEEL_STATE_FIXED) {
            x87d tractionLimit = WS_TRAC_FRAC_LIMIT;
            if (contactSpeedFwd > 0.15f && (wheelId == 0 || wheelId == 2)) { // `test al, al; je / cmp al, 2`
                tractionLimit += tractionLimit;
            }
            *wheelState = (bDriving && tractionLimit * adhesion < std::fabs((x87d)fwd)) ? WHEEL_STATE_SPINNING : WHEEL_STATE_SKIDDING;
        }
        x87d tractionLoss = m_pHandlingData->m_fTractionLoss;
        if (bAlreadySkidding) {
            tractionLoss = (x87d)1.0;
        } else if (*wheelState == WHEEL_STATE_SPINNING && statusIsPlayerLike()) {
            tractionLoss = ((x87d)1.0 - std::fabs((x87d)m_GasPedal) * WS_ALREADY_SPINNING_LOSS) * tractionLoss;
        }
        const x87d scale = (tractionLoss / std::sqrt((x87d)sumSqF)) * adhesion;
        fwd   = (float)((x87d)fwd * scale);
        right = right * scale;
    }

    if (!(fwd != 0.0f) && right == 0.0) { // 0x6D701A: `jp` on !=, `jnp` on == 0
        return;
    }

    // 0x6D703C - total force direction: spilled products, y/z sums rounded to float, z of the right product unrounded
    const float fx = (float)((x87d)fwd * wheelFwd.x), fy = (float)((x87d)fwd * wheelFwd.y), fz = (float)((x87d)fwd * wheelFwd.z);
    const float rx = (float)(right * wheelRight.x), ry = (float)(right * wheelRight.y);
    const x87d rzExt = right * wheelRight.z;
    const CVector total{ (float)((x87d)rx + fx), (float)((x87d)ry + fy), (float)(rzExt + fz) };

    CVector turnDir = total;
    bool    separateTurnForce = false;
    const float antiDive = m_pHandlingData->m_fSuspensionAntiDiveMultiplier;
    if (antiDive > 0.0f && (bBraking || bDriving)) { // 0x6D70AC
        const x87d m = bBraking ? (x87d)antiDive : (x87d)antiDive * 0.5f;
        separateTurnForce = true;
        const x87d mfx = m * wheelFwd.x;
        const float  mfy = (float)(m * wheelFwd.y), mfz = (float)(m * wheelFwd.z);
        const x87d ax  = mfx * fwd;
        const x87d ay  = (x87d)mfy * fwd;
        const float  az  = (float)((x87d)mfz * fwd);
        turnDir.x = (float)((x87d)total.x - ax);
        turnDir.y = (float)((x87d)total.y - ay);
        turnDir.z = (float)((x87d)total.z - az);
    }

    // 0x6D7141 - magnitudes ((z^2 + y^2) + x^2), spilled to float
    const float speed = (float)std::sqrt(((x87d)total.z * total.z + (x87d)total.y * total.y) + (x87d)total.x * total.x);
    float turnSpeed   = speed;
    if (separateTurnForce) {
        turnSpeed = (float)std::sqrt(((x87d)turnDir.z * turnDir.z + (x87d)turnDir.y * turnDir.y) + (x87d)turnDir.x * turnDir.x);
    }

    CVector direction = total; // 0x6D71BF: normalise (reciprocal in extended precision, x = 1 for a zero length)
    if (!(speed > 0.0f) && !std::isnan(speed)) {
        direction.x = 1.0f;
    } else {
        const x87d recip = (x87d)1.0 / (x87d)speed;
        direction.x = (float)(total.x * recip);
        direction.y = (float)(total.y * recip);
        direction.z = (float)(total.z * recip);
    }
    if (!separateTurnForce) {
        turnDir = direction;
    } else if (!(turnSpeed > 0.0f) && !std::isnan(turnSpeed)) {
        turnDir.x = 1.0f;
    } else {
        const x87d recip = (x87d)1.0 / (x87d)turnSpeed;
        turnDir.x = (float)(turnDir.x * recip);
        turnDir.y = (float)(turnDir.y * recip);
        turnDir.z = (float)(turnDir.z * recip);
    }

    // 0x6D7291 - 0x59C730 cross product (extended), then 1 / (|cross|^2 / turnMass + 1 / mass) * turnSpeed
    const CVector cross{
        (float)((x87d)turnDir.z * wheelContactPoint.y - (x87d)wheelContactPoint.z * turnDir.y),
        (float)((x87d)turnDir.x * wheelContactPoint.z - (x87d)wheelContactPoint.x * turnDir.z),
        (float)((x87d)turnDir.y * wheelContactPoint.x - (x87d)wheelContactPoint.y * turnDir.x)
    };
    const x87d force = (x87d)m_fMass * speed;
    const x87d cpSq  = ((x87d)cross.x * cross.x + (x87d)cross.y * cross.y) + (x87d)cross.z * cross.z;
    const x87d turnForce = ((x87d)1.0 / (cpSq / m_fTurnMass + (x87d)1.0 / m_fMass)) * turnSpeed;

    ApplyMoveForce(CVector{ (float)(force * direction.x), (float)(force * direction.y), (float)(force * direction.z) }); // 0x5429F0
    ApplyTurnForce(CVector{ (float)(turnForce * turnDir.x), (float)(turnForce * turnDir.y), (float)(turnForce * turnDir.z) }, wheelContactPoint); // 0x542A50
}

// 0x6D73B0
void CVehicle::ProcessBikeWheel(CVector& wheelFwd, CVector& wheelRight, CVector& wheelContactSpeed, CVector& wheelContactPoint, int32 wheelsOnGround, float thrust, float brake,
                                float adhesion, float destabTraction, int8 wheelId, float* wheelSpeed, tWheelState* wheelState, eBikeWheelSpecial special, uint16 wheelStatus) {
    // NOTE: The original keeps float intermediates on the x87 stack (53-bit precision), hence the `double`s below.
    NOTSA_GLOBAL_LOCAL(bBraking, 0xC1CDB2, (bool), {});
    NOTSA_GLOBAL_LOCAL(bDriving, 0xC1CDB1, (bool), {});
    NOTSA_GLOBAL_LOCAL(bReversing, 0xC1CDB0, (bool), {}); // thrust < 0 (only set while not braking)
    NOTSA_GLOBAL_LOCAL(bAlreadySkidding, 0xC1CDAF, (bool), {}); // BUG: never reset to false by the original

    const float contactSpeedFwd = (float)((x87d)wheelFwd.y * wheelContactSpeed.y + (x87d)wheelFwd.z * wheelContactSpeed.z + (x87d)wheelFwd.x * wheelContactSpeed.x);

    float  fwdOut = 0.f;
    x87d right  = (x87d)0.0;

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
        const float contactSpeedRight = (float)((x87d)wheelRight.z * wheelContactSpeed.z + (x87d)wheelRight.y * wheelContactSpeed.y + (x87d)wheelRight.x * wheelContactSpeed.x);
        if (contactSpeedRight != 0.f) {
            right = -((x87d)contactSpeedRight / wheelsOnGround);
            if (wheelStatus == WHEEL_STATUS_BURST) {
                const float  rightF  = (float)right;
                const float  speed   = contactSpeedFwd < fBurstBikeSpeedMax ? contactSpeedFwd : fBurstBikeSpeedMax;
                const x87d lo      = -(x87d)fBurstBikeTyreMod;
                const x87d rnd     = (x87d)rand() * (x87d)RAND_MAX_FLOAT_RECIPROCAL;
                right = (rnd * ((x87d)fBurstBikeTyreMod - lo) + lo) * speed + rightF;
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
        fwdOut = (float)-((x87d)contactSpeedFwd / wheelsOnGround);

        x87d brakeVal = brake;
        if (!bBraking && std::fabs(m_GasPedal) < 0.01f) {
            const auto fric = gHandlingDataMgr.fWheelFriction;
            const auto mass = m_pHandlingData->m_fMass;
            if (m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                if (fwdOut > -0.05f && fwdOut < 0.05f) {
                    brakeVal = ((x87d)fric * 0.5f) / ((x87d)mass + 200.0f);
                }
            } else if (m_nVehicleType == VEHICLE_TYPE_BIKE) {
                brakeVal = ((x87d)fric * 0.6f) / ((x87d)mass + 200.0f);
            } else {
                brakeVal = (x87d)fric / mass;
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
            const x87d negBrake = -brakeVal;
            if (fwdOut < negBrake) {
                fwdOut = (float)negBrake;
            }
        }
    }

    const float adhesionSq = adhesion * adhesion;
    const x87d sumSq     = right * right + (x87d)fwdOut * fwdOut;
    const float  sumSqF    = (float)sumSq;
    if (!(sumSq <= adhesionSq)) {
        if (*wheelState != WHEEL_STATE_FIXED) {
            *wheelState = (bDriving && contactSpeedFwd < 0.1f) ? WHEEL_STATE_SPINNING : WHEEL_STATE_SKIDDING;
        }
        const float tractionLoss = alreadySkidding ? 1.f : m_pHandlingData->m_fTractionLoss;
        const x87d scale = ((x87d)tractionLoss / std::sqrt((x87d)sumSqF)) * adhesion;
        fwdOut = (float)(fwdOut * scale);
        right  = right * scale;
        if (destabTraction < 1.f) {
            right *= destabTraction;
        }
    } else if (destabTraction < 1.f) {
        if (!alreadySkidding) {
            destabTraction = destabTraction * m_pHandlingData->m_fTractionLoss;
        }
        if ((x87d)adhesionSq * destabTraction * destabTraction < sumSqF) {
            right = (((x87d)adhesion * destabTraction) / std::sqrt((x87d)sumSqF)) * right;
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
        const x87d sumSq = (x87d)force.x * force.x + (x87d)force.y * force.y + (x87d)force.z * force.z;
        if (sumSq <= 0.0) {
            force.x = 1.f;
        } else {
            const x87d recip = (x87d)1.0 / std::sqrt(sumSq);
            force.x = (float)(force.x * recip);
            force.y = (float)(force.y * recip);
            force.z = (float)(force.z * recip);
        }
    }

    const float mag = (float)std::sqrt((x87d)forceBeforeNorm.z * forceBeforeNorm.z + (x87d)forceBeforeNorm.y * forceBeforeNorm.y + (x87d)forceBeforeNorm.x * forceBeforeNorm.x);
    // 0x59C730 - the products stay in the FPU registers (extended precision); the shared `CrossProduct` rounds them to float
    const CVector cp{
        (float)((x87d)wheelContactPoint.y * force.z - (x87d)wheelContactPoint.z * force.y),
        (float)((x87d)wheelContactPoint.z * force.x - (x87d)wheelContactPoint.x * force.z),
        (float)((x87d)wheelContactPoint.x * force.y - (x87d)wheelContactPoint.y * force.x)
    };
    const x87d cpSq = ((x87d)cp.x * cp.x + (x87d)cp.y * cp.y) + (x87d)cp.z * cp.z;
    const x87d moveScale = (x87d)m_fMass * mag;
    const x87d turnScale = ((x87d)1.0 / (cpSq / m_fTurnMass + (x87d)1.0 / m_fMass)) * mag;

    ApplyMoveForce(CVector{ (float)(force.x * moveScale), (float)(force.y * moveScale), (float)(force.z * moveScale) }); // 0x5429F0
    const CVector turnForce{ (float)(force.x * turnScale), (float)(force.y * turnScale), (float)(force.z * turnScale) };

    const auto& R = m_matrix->GetRight();
    const auto& F = m_matrix->GetForward();
    const float turnRight = (float)(((x87d)turnForce.z * R.z + (x87d)turnForce.y * R.y) + (x87d)turnForce.x * R.x);
    const float cpRight   = (float)(((x87d)R.y * wheelContactPoint.y + (x87d)R.z * wheelContactPoint.z) + (x87d)wheelContactPoint.x * R.x);
    const float cpFwd     = (float)(((x87d)F.y * wheelContactPoint.y + (x87d)F.z * wheelContactPoint.z) + (x87d)wheelContactPoint.x * F.x);

    if (wheelId != 1 || (!bBraking && !bReversing)) {
        const x87d ax = (x87d)cpRight * R.x;
        const x87d ay = (x87d)cpRight * R.y;
        const float  az = cpRight * R.z;
        const CVector point{
            (float)(wheelContactPoint.x - ax),
            (float)(wheelContactPoint.y - ay),
            (float)(wheelContactPoint.z - az)
        };
        const x87d bx = (x87d)turnRight * R.x;
        const x87d by = (x87d)turnRight * R.y;
        const float  bz = turnRight * R.z;
        const float  tx = (float)(turnForce.x - bx);
        const CVector f{
            tx * fTweakBikeWheelTurnForce,
            (float)((turnForce.y - by) * fTweakBikeWheelTurnForce),
            (float)((turnForce.z - (x87d)bz) * fTweakBikeWheelTurnForce)
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
    // 0x6D7BC0: the relative position is spilled to floats; both dot products are 3D ones with a `* 0.0` z term (so a non-finite z of the axis poisons them)
    // and are summed y, x, z in extended precision; the right one is stored as a float, the forward one is compared straight from the FPU
    const auto& mat = *m_matrix; // the original dereferences the matrix unconditionally
    const auto  pos = GetPosition();
    const float relX = point.x - pos.x, relY = point.y - pos.y;
    const auto  fwd = mat.GetForward(), right = mat.GetRight();
    const x87d dotFwd = ((x87d)relY * fwd.y + (x87d)relX * fwd.x) + (x87d)fwd.z * 0.0f;
    const float  dotRight = (float)(((x87d)relY * right.y + (x87d)relX * right.x) + (x87d)right.z * 0.0f);
    const bool isFront = dotFwd > 0.0;
    if (IsBike()) { // only distinguishes front vs rear
        return isFront ? eNearestCarWheel::FRONT_LEFT : eNearestCarWheel::REAR_LEFT;
    }
    const bool isRight = dotRight > 0.0f;
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
            if (MagnitudeExt(coords) < 0.25f) { // 0x4082C0: the result stays in the FPU (not rounded to float)
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
    for (auto& ped : NOTSA_EXE_OR(GetPedPool()->GetAllValidReverse(), GetPedPool()->GetAllValid())) {
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

//
// CVehicle::FlyingControl (0x6D85F0) - split into file-local helpers, one per block of the original
//
// NOTE: The original keeps most intermediates on the x87 stack (extended precision), but stores some to float temporaries, hence the mix of `double`s and `float`s.
//

// 0x4082C0 / 0x406DA0 - (squared) magnitude, left in the FPU
static double SquaredMagnitudeExt(const CVector& v) {
    return ((x87d)v.x * v.x + (x87d)v.y * v.y) + (x87d)v.z * v.z;
}

// 0x40FDB0 - `DotProduct`, left in the FPU
static double DotProductExt(const CVector& a, const CVector& b) {
    return ((x87d)a.z * b.z + (x87d)a.y * b.y) + (x87d)a.x * b.x;
}

// 0x59C730 - `CrossProduct`, the products stay in the FPU registers
static CVector CrossProductExt(const CVector& a, const CVector& b) {
    return {
        (float)((x87d)a.y * b.z - (x87d)a.z * b.y),
        (float)((x87d)a.z * b.x - (x87d)a.x * b.z),
        (float)((x87d)a.x * b.y - (x87d)b.x * a.y)
    };
}

// 0x59C790 - 3x3 matrix * vector (no translation), the sums stay in the FPU registers
static CVector Multiply3x3Ext(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return {
        (float)(((x87d)u.x * v.z + (x87d)f.x * v.y) + (x87d)r.x * v.x),
        (float)(((x87d)u.y * v.z + (x87d)r.y * v.x) + (x87d)f.y * v.y),
        (float)(((x87d)u.z * v.z + (x87d)r.z * v.x) + (x87d)f.z * v.y)
    };
}

// Hydra: the thrust direction rotates with the nozzles (0x6D8FFF & 0x6D9C10)
static CVector FlyingControlHydraDirection(const CVehicle& v) {
    const auto&  mat   = *v.m_matrix;
    const x87d angle = ((x87d)v.AsAutomobile()->m_wMiscComponentAngle * 1.5707964f) / (x87d)(int16)CPlane::HARRIER_NOZZLE_ROTATE_LIMIT;
    const float  cosA  = (float)x87::cos(angle);
    const float  sinA  = (float)x87::sin(angle);
    return (mat.GetForward() * cosA) + (mat.GetUp() * sinA);
}


//! State shared by the blocks of `CVehicle::FlyingControl`
struct FlyingControlCtx {
    CVehicle*    veh;
    eFlightModel model;
    CPad*        pad;           // The driver's pad if the vehicle is driven by a player, otherwise null
    CVector      speed;         // Move speed relative to the wind
    CVector      com;           // Center of mass (rotated into the world)
    float        leftRightSkid;
    float        steeringUpDown;
    float        steeringLeftRight;
    float        accelerationBreakStatus;
};

constexpr float FLYING_CONTROL_NOT_SET = -9999.99f; // 0x871F8C - "use the pad's value"

// 0x6D8715 - flight model 0 (FLIGHT_MODEL_UNK)
static void FlyingControlModel0(const FlyingControlCtx& c) {
    CVehicle&     v   = *c.veh;
    const auto&   mat = *v.m_matrix;
    const auto&   V   = c.speed;
    const float   ts  = CTimer::ms_fTimeStep;
    const CVector right = mat.GetRight(), fwd = mat.GetForward(), up = mat.GetUp();

    const float rightDot = (float)((((x87d)V.z * right.z + (x87d)V.y * right.y) + (x87d)V.x * right.x) * -1.0f);
    const float rightFac = (float)(X87R((double)rightDot / MagnitudeExt(V)));

    const float dotFwd = (float)(((x87d)fwd.y * V.y + (x87d)fwd.x * V.x) + (x87d)fwd.z * V.z);
    const float k      = (float)((X87R(SquaredMagnitudeExt(V) * dotFwd)) * dotFwd);

    const CVector fwdScaled = fwd * -4.0f;

    const float steerTurn = (float)((((X87R((double)v.m_fSteerAngle * 0.001) + X87R((double)rightFac * 0.003)) * ts) * k) * v.m_fTurnMass);
    v.ApplyTurnForce(right * steerTurn, fwdScaled);

    const float moveScale = (float)(((((x87d)ts * v.m_fMass) * k) * rightFac) * 0.2f);
    const CVector sideMove = right * moveScale;
    v.ApplyMoveForce(sideMove);
    v.ApplyTurnForce(sideMove, up * 2.0f);

    const float upDot = (float)((((x87d)V.z * up.z + (x87d)V.y * up.y) + (x87d)V.x * up.x) * -1.0f);
    const float upFac = (float)(X87R((double)upDot / MagnitudeExt(V)));

    x87d pitchIn = (x87d)0.0;
    if (c.pad) {
        pitchIn = (x87d)(int32)c.pad->GetSteeringUpDown() * -0.0078125f;
    }
    pitchIn *= 0.001f;
    const float pitchTurn = (float)(((((pitchIn + (x87d)upFac * 0.002f) * ts) * k) * v.m_fTurnMass));
    v.ApplyTurnForce(up * pitchTurn, fwdScaled);

    float lift = (float)(X87R((((((x87d)upFac * (x87d)3.5 + (x87d)0.5) * v.m_fMass) * ts) * k) * 0.05));
    if (v.GetStatus() == STATUS_PLAYER || v.GetStatus() == STATUS_REMOTE_CONTROLLED) {
        if (((x87d)ts * v.m_fMass) * 0.008f < lift) {
            if (v.HeightAboveCeiling(v.GetPosition().z, FLIGHT_MODEL_UNK) > 0.0f) {
                lift = (float)(((x87d)v.m_fMass * ts) * (x87d)0.0072f);
            }
        }
    }
    const CVector liftForce = up * lift;
    v.ApplyMoveForce(liftForce);
    v.ApplyTurnForce(liftForce, (up * 2.0f) + c.com);

    v.m_vecTurnSpeed.y = (float)(X87R(std::pow((double)0.9f, (double)ts) * v.m_vecTurnSpeed.y));
}

// 0x6D8B1D - flight models 1 (baron), 3 (plane), 4, 5 (boat)
static void FlyingControlModel1345(FlyingControlCtx& c) {
    CVehicle&     v     = *c.veh;
    const auto&   mat   = *v.m_matrix;
    const auto    fh    = v.m_pFlyingHandlingData;
    const auto&   V     = c.speed;
    const float   ts    = CTimer::ms_fTimeStep;
    const auto    model = c.model;
    const auto    pad   = c.pad;
    float&        skid  = c.leftRightSkid;
    float&        upDown = c.steeringUpDown;
    float&        leftRight = c.steeringLeftRight;
    float&        accel = c.accelerationBreakStatus;

    // Inputs
    if (skid == FLYING_CONTROL_NOT_SET) {
        skid = 0.0f;
        if (pad) {
            skid = (float)((x87d)(int32)pad->GetSteeringLeftRight() * 0.0078125f);
        }
    }
    if (upDown == FLYING_CONTROL_NOT_SET) {
        upDown = 0.0f;
        if (pad) {
            upDown = (float)((x87d)(int32)pad->GetSteeringUpDown() * 0.0078125f);
            if (std::fabs((x87d)(int32)pad->GetCarGunUpDown()) > 1.0) {
                upDown = (float)((x87d)(-(int32)pad->GetCarGunUpDown()) * 0.0078125f);
            }
        }
    }

    // Scale the inputs by 1 / cos(angle to the nearest axis)
    const float inputAngle = (float)x87::atan2((double)upDown, (double)skid);
    constexpr x87d PI = std::numbers::pi_v<float>, HALF_PI = 1.5707964f;
    x87d angleFac = (x87d)1.0;
    {
        const x87d a = inputAngle;
        if (inputAngle > -0.7853981852531433f && inputAngle <= 0.7853981852531433f) {
            angleFac = X87R(1.0 / x87::cos(a));
        } else if (inputAngle > 0.7853981852531433f && inputAngle <= 2.356194496154785f) {
            angleFac = X87R(1.0 / x87::cos(a - HALF_PI));
        } else if (inputAngle > 2.356194496154785f) {
            angleFac = X87R(1.0 / x87::cos(a - PI));
        } else if (inputAngle <= -2.356194496154785f) {
            angleFac = X87R(1.0 / x87::cos(a + PI));
        } else if (inputAngle < -0.7853981852531433f) {
            angleFac = X87R(1.0 / x87::cos(a + HALF_PI));
        }
    }
    skid   = (float)((x87d)skid * angleFac);
    upDown = (float)(-(angleFac * upDown));

    const CVector fwd  = mat.GetForward();
    const CVector rear = fwd * v.GetColModel()->GetBoundingBox().m_vecMin.y; // (+4 = min.y)

    const x87d dotFwdExt = ((x87d)fwd.y * V.y + (x87d)fwd.x * V.x) + (x87d)fwd.z * V.z;
    const float  dotFwd    = (float)dotFwdExt;
    const float  dotFwdSq  = (float)(dotFwdExt * dotFwd);

    if (model == FLIGHT_MODEL_BARON) {
        v.ApplyMoveForce(((CVector{ 0.0f, 0.0f, 0.004f } * v.m_fMass) * ts));
    }

    // Throttle
    if (accel == FLYING_CONTROL_NOT_SET) {
        accel = 0.0f;
        if (pad) {
            const int32 acc = pad->GetAccelerate();
            const int32 brk = pad->GetBrake();
            accel = (float)((x87d)(acc - brk) * (x87d)0.003921568859368563);
        }
    }

    const auto modelIdx = v.m_nModelIndex;
    {
        bool gentle = true; // true => 0x6D8EFE, false => 0x6D8E83
        if (v.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
            const auto& comp = v.AsAutomobile()->m_fWheelsSuspensionCompression;
            bool        brakeCheck; // 0x6D8E6F
            if (modelIdx == 0x21B || comp[0] != 1.0f || comp[1] != 1.0f || comp[2] != 1.0f || comp[3] != 1.0f) {
                brakeCheck = true;
            } else if (modelIdx != 0x1CC) {
                brakeCheck = false;
            } else if (!v.physicalFlags.bSubmergedInWater || dotFwd > 0.2f) { // +0x40 & 0x100
                brakeCheck = false;
            } else {
                brakeCheck = true;
            }
            if (brakeCheck) {
                gentle = accel > 0.0f;
            }
        }

        if (!gentle) {
            if (!(accel < 0.0f) || !(DotProductExt(fwd, v.m_vecMoveSpeed) < 0.02f)) {
                accel = 0.0f;
            } else {
                x87d val = (x87d)accel - (x87d)dotFwd * 7.76f;
                if (0.0 < val) {
                    val = (x87d)0.0;
                }
                accel = (float)(val * fh->m_fThrust);
            }
        } else {
            x87d val = (x87d)accel;
            if (modelIdx == 0x21B) {
                val = val * fh->m_fThrust;
            } else {
                val = ((val + 1.0f) * fh->m_fThrust) * 0.5f;
            }
            accel = (float)val;

            if (dotFwd > 0.0f && fh->m_fThrustFallOff < 1.0f) {
                x87d g;
                if (!(fh->m_fThrustFallOff < 0.0f)) {
                    const x87d d = (x87d)dotFwd - fh->m_fThrustFallOff;
                    g = (d * d) * 0.65f;
                } else {
                    const x87d d = (x87d)dotFwd + fh->m_fThrustFallOff;
                    g = (d * d) * 3.0f;
                }
                x87d h = (x87d)1.0 - g;
                if (0.0 > h) {
                    h = (x87d)0.0;
                }
                accel = (float)(h * accel);
            }
        }
    }
    if (model == FLIGHT_MODEL_UNK2) {
        accel = (float)((x87d)accel * 0.3f);
    } else if (model == FLIGHT_MODEL_BOAT) {
        accel = (float)((x87d)accel * 0.1f);
    }

    // Direction of the thrust
    CVector thrustDir = mat.GetForward();
    if (modelIdx == 0x208) { // Hydra: the nozzles rotate
        thrustDir = FlyingControlHydraDirection(v);
    }

    {
        const x87d thrustScale = 0.008f;
        const float  tx = (float)(((x87d)thrustDir.x * thrustScale) * accel);
        const x87d ty = ((x87d)thrustDir.y * thrustScale) * accel;
        const float  tz = (float)((x87d)thrustDir.z * thrustScale);
        const float  mx = (float)((x87d)tx * v.m_fMass);
        const float  my = (float)(ty * v.m_fMass);
        const x87d mz = ((x87d)tz * accel) * v.m_fMass;
        v.ApplyMoveForce({ (float)((x87d)mx * ts), (float)((x87d)my * ts), (float)(mz * ts) });
    }

    const CVector right = mat.GetRight(), up = mat.GetUp();

    // Side drag
    {
        const x87d d = (((x87d)V.z * right.z + (x87d)V.y * right.y) + (x87d)V.x * right.x) * -1.0f;
        const x87d g = d * (std::fabs(d) * fh->m_fSideSlip);
        const x87d gx = (x87d)right.x * g;
        const float  gy = (float)((x87d)right.y * g);
        const float  gz = (float)((x87d)right.z * g);
        const x87d xm = gx * v.m_fMass;
        const x87d ym = (x87d)gy * v.m_fMass;
        const float  zm = (float)((x87d)gz * v.m_fMass);
        v.ApplyMoveForce({ (float)(xm * ts), (float)(ym * ts), (float)((x87d)zm * ts) });
    }

    // Yaw (rudder / skid)
    {
        const CVector spd = v.GetSpeed(rear);
        const x87d  h   = (((x87d)spd.z * right.z + (x87d)spd.y * right.y) + (x87d)spd.x * right.x) * -1.0f;

        x87d m;
        if (modelIdx == 0x21B) {
            if (dotFwd > 0.0f || std::isnan(dotFwd)) { // (0x6D9280: JP)
                m = dotFwd > accel ? (x87d)dotFwd : (x87d)accel;
            } else {
                m = dotFwd < accel ? (x87d)dotFwd : (x87d)accel;
            }
        } else {
            m = dotFwd;
        }
        const x87d hh  = (std::fabs(h) * fh->m_fYawStab) * h;
        const float  s   = (float)((((hh + ((m * fh->m_fYaw) * skid)) * ts) * v.m_fTurnMass));
        v.ApplyTurnForce(right * s, c.com + rear);
    }

    // Roll
    {
        const x87d in = leftRight == FLYING_CONTROL_NOT_SET ? (x87d)skid : (x87d)leftRight;
        const float  s  = (float)(((((x87d)ts * fh->m_fRoll) * dotFwd) * in) * v.m_fTurnMass);
        v.ApplyTurnForce(right * s, c.com + up);
    }

    // Roll stabilization
    {
        CVector cross = CrossProductExt(mat.GetForward(), { 0.0f, 0.0f, 1.0f });
        if (!(up.z > 0.0f)) {
            cross = cross * -1.0f;
        }
        const x87d sign = right.z > 0.0f ? -1.0f : 1.0f;
        const x87d t1   = (x87d)1.0 - std::fabs((x87d)mat.GetForward().z);
        const x87d t2   = (x87d)1.0 - (((x87d)cross.z * right.z + (x87d)cross.y * right.y) + (x87d)cross.x * right.x);
        const x87d w    = t1 * (t2 * sign);
        const float  s    = (float)(((((w * fh->m_fRollStab) * ts) * v.m_fTurnMass) * 0.5f));
        v.ApplyTurnForce(right * s, c.com + up);
    }

    // Pitch
    {
        const CVector spd = v.GetSpeed(rear);
        const x87d  d   = (((x87d)spd.z * up.z + (x87d)spd.y * up.y) + (x87d)spd.x * up.x) * -1.0f;
        const x87d  g   = d * (std::fabs(d) * fh->m_fPitchStab);
        const x87d  s   = ((g + (((x87d)dotFwd * fh->m_fPitch) * upDown)) * ts) * v.m_fTurnMass;
        const CVector f{ (float)(up.x * s), (float)(up.y * s), (float)(up.z * s) };
        v.ApplyTurnForce(f, c.com + rear);
    }

    // Lift
    {
        const x87d upDot  = (((x87d)V.z * up.z + (x87d)V.y * up.y) + (x87d)V.x * up.x);
        const float  upDotF = (float)upDot;
        const float  speedF = (float)MagnitudeExt(V);
        const float  den    = (0.01f > speedF) ? 0.01f : speedF;     // 0x420800
        const float  r1     = (float)((x87d)upDotF / den);
        const float  r2     = (1.0f < r1) ? 1.0f : r1;               // 0x404330
        const float  r3     = (-1.0f > r2) ? -1.0f : r2;             // 0x420800
        const x87d pitch  = X87R(x87::asin((double)r3) * -1.0f);         // 0x821E70
        if (v.m_nVehicleSubType == VEHICLE_TYPE_PLANE && pitch > 0.3490658700466156f) {
            v.AsPlane()->field_9A0 += NOTSA_FTOL(int32, ((x87d)ts * 0.02f) * 1000.0f);
        }

        x87d formLift = fh->m_fFormLift;
        if (model == FLIGHT_MODEL_BARON) {
            const float limit = (float)((x87d)fFlyingLiftLimit * 0.008f);
            if ((x87d)dotFwdSq * formLift > limit) {
                formLift = (x87d)limit / dotFwdSq;
            }
        } else if (v.m_nVehicleSubType == VEHICLE_TYPE_PLANE && v.AsPlane()->m_fLandingGearStatus < 1.0f) {
            formLift = formLift * fh->m_fGearDownL;
        }

        const x87d lift  = (((pitch * fh->m_fAttackLift + formLift) * v.m_fMass) * ts) * dotFwdSq;
        float        liftF = (float)lift;
        if (lift > ((x87d)ts * v.m_fMass) * 0.008f) {
            const float height = v.HeightAboveCeiling(v.GetPosition().z, model);
            if (height > 0.0f) {
                const x87d factor = ((x87d)1.0 - (x87d)height * 0.02f) < (x87d)0.0 ? (x87d)0.0 : ((x87d)1.0 - (x87d)height * 0.02f); // NOTE: the original calls `HeightAboveCeiling` 3 times
                liftF = (float)(((factor * v.m_fMass) * ts) * 0.008f);
            }
        }
        v.ApplyMoveForce(up * liftF);
    }
}

// `in * dir * coeff * turnMass * timeStep`, with the original's float roundings (0x6DA4DA, 0x6DA5BF, 0x6DA84B)
static CVector FlyingControlTorque(float in, const CVector& dir, float coeff, float turnMass, float ts) {
    const float  xa = (float)(((x87d)in * dir.x) * coeff);
    const float  xb = (float)((x87d)xa * turnMass);
    const x87d ya = ((x87d)in * dir.y) * coeff;
    const float  yb = (float)(ya * turnMass);
    const float  za = (float)((x87d)in * dir.z);
    const x87d zb = ((x87d)za * coeff) * turnMass;
    return { (float)((x87d)xb * ts), (float)((x87d)yb * ts), (float)(zb * ts) };
}

// 0x6D986E - flight models 2 (RC), 6 (heli), 8
static void FlyingControlModel268(FlyingControlCtx& c) {
    CVehicle&   v     = *c.veh;
    const auto& mat   = *v.m_matrix;
    const auto  fh    = v.m_pFlyingHandlingData;
    const auto& V     = c.speed;
    const float ts    = CTimer::ms_fTimeStep;
    const auto  model = c.model;
    const auto  pad   = c.pad;
    auto&       ms    = v.m_vecMoveSpeed;
    float&      skid  = c.leftRightSkid;
    float&      upDown = c.steeringUpDown;
    float&      leftRight = c.steeringLeftRight;
    float&      accel = c.accelerationBreakStatus;

    // Move speed drag
    {
        const double r = std::pow((double)fh->m_fMoveRes, (double)ts);
        ms.x = (float)(X87R(r * ms.x));
        ms.y = (float)(X87R(r * ms.y));
        ms.z = (float)(X87R(r * ms.z));
    }

    if ((int32)model == 8) {
        const CVector fwd = mat.GetForward();
        const float   dotF = (float)(((x87d)V.z * fwd.z + (x87d)V.y * fwd.y) + (x87d)V.x * fwd.x);

        x87d throttle = (x87d)accel;
        if (accel == FLYING_CONTROL_NOT_SET) {
            throttle = (x87d)0.0;
            if (pad) {
                throttle = (x87d)((int32)pad->GetAccelerate() - (int32)pad->GetBrake()) * (x87d)0.003921568859368563;
            }
        }

        float thrust;
        if (dotF > 0.0f || throttle > 0.0) {
            thrust = (float)((throttle - (x87d)dotF * fh->m_fThrustFallOff) * fh->m_fThrust);
        } else {
            x87d x = throttle - (((x87d)dotF * fh->m_fThrustFallOff) * 8.0f);
            if (0.0 < x) {
                x = (x87d)0.0;
            }
            thrust = (float)(x * fh->m_fThrust);
        }
        v.ApplyMoveForce(((fwd * thrust) * v.m_fMass) * ts);

        CVector tilt = mat.GetUp() - (mat.GetForward() * AUTOGYRO_ROTORTILT_ANGLE);
        NormaliseExt(tilt); // 0x59C910
        float tiltDot = (float)(((x87d)tilt.x * V.x + (x87d)tilt.z * V.z) + (x87d)tilt.y * V.y);

        float rotorSpin = 0.22f; // 0x8717AC
        if (v.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
            auto&        spin = v.AsAutomobile()->m_fHeliRotorSpeed;
            const x87d lo   = -AUTOGYRO_ROTORSPIN_MULTLIMIT;
            if (0.0 < ((lo > tiltDot) ? lo : (double)tiltDot)) {
                tiltDot = 0.0f;
            } else if (lo > tiltDot) {
                tiltDot = (float)lo;
            }

            const float spinNew = (float)((x87d)spin - (((x87d)tiltDot * ts) * AUTOGYRO_ROTORSPIN_MULT));
            spin = spinNew;
            const x87d damped = X87R(std::pow((double)AUTOGYRO_ROTORSPIN_DAMP, (double)ts) * spinNew);
            spin = (float)damped;
            if (damped > 0.4f) {
                spin = 0.4f;
            } else if (damped < 0.08f) {
                spin = 0.08f;
            }
            rotorSpin = spin;
        }

        const float lift = (float)(((x87d)rotorSpin * AUTOGYRO_ROTORLIFT_MULT) - ((x87d)tiltDot * AUTOGYRO_ROTORLIFT_FALLOFF));
        v.ApplyMoveForce((((tilt * 0.008f) * lift) * v.m_fMass) * ts);
    } else {
        CVector dir = mat.GetUp();
        if (v.vehicleFlags.bHeliMinimumTilt) { // +0x42B & 0x10
            dir.x = (float)x87::sin(X87R(x87::asin((double)dir.x) * 4.0f));
            dir.y = (float)x87::sin(X87R(x87::asin((double)dir.y) * 4.0f));
            dir.z = (float)x87::cos(X87R(x87::acos((double)dir.z) * 4.0f));
        } else if (v.m_nModelIndex == 0x208) {
            dir = FlyingControlHydraDirection(v);
        }

        float dirDot = (float)(((x87d)V.z * dir.z + (x87d)V.y * dir.y) + (x87d)V.x * dir.x);
        if (dirDot > 0.0f) {
            dirDot = (float)((x87d)dirDot + dirDot);
        }

        if (accel == FLYING_CONTROL_NOT_SET) {
            accel = 0.0f;
            if (pad) {
                const int32 acc = pad->GetAccelerate();
                const int32 brk = pad->GetBrake();
                accel = (float)((x87d)(acc - brk) * (x87d)0.003921568859368563);
                if (std::fabs((x87d)(int32)pad->GetCarGunUpDown()) > 1.0) {
                    accel = (float)((x87d)(int32)pad->GetCarGunUpDown() * 0.0078125f);
                }
            }
        }

        float thrust = (float)((x87d)accel * fh->m_fThrust + 0.45f);
        const float hover = (float)(0.5f - (x87d)ms.z);
        v.ApplyMoveForce(((((CVector{ 0.0f, 0.0f, 1.0f } * hover) * 0.008f) * v.m_fMass) * ts));

        thrust = (float)((x87d)thrust - (x87d)dirDot * fh->m_fThrustFallOff);
        if (v.HeightAboveCeiling(v.GetPosition().z, model) > 0.0f) {
            thrust = (float)(((x87d)thrust / ((x87d)v.HeightAboveCeiling(v.GetPosition().z, model) + 10.0f)) * 10.0f);
        }
        v.ApplyMoveForce((((dir * 0.008f) * thrust) * v.m_fMass) * ts);
    }

    // Stabilization
    const CVector right = mat.GetRight(), fwd = mat.GetForward(), up = mat.GetUp();
    if (up.z > 0.0f) {
        CVector wind{ 0.0f, 0.0f, 1.0f };
        wind += (CWeather::WindDir * fh->m_fWindMult);
        NormaliseExt(wind);

        const x87d lo = -fh->m_fFormLift;
        const auto Clamp = [&](const CVector& axis) -> float {
            const auto dot = [&] { return ((x87d)wind.z * axis.z + (x87d)wind.y * axis.y) + (x87d)wind.x * axis.x; };
            const x87d m = (lo > dot()) ? lo : dot();
            if (m > fh->m_fFormLift) {
                return fh->m_fFormLift;
            }
            return (float)m;
        };
        {
            const float val = Clamp(right);
            const float s   = (float)(((((x87d)-val * fh->m_fAttackLift) * ts) * v.m_fTurnMass));
            v.ApplyTurnForce(up * s, right + c.com);
        }
        {
            const float val = Clamp(fwd);
            const float s   = (float)(((((x87d)-val * fh->m_fAttackLift) * ts) * v.m_fTurnMass));
            v.ApplyTurnForce(up * s, fwd + c.com);
        }
    } else {
        {
            const float val = right.z < 0.0f ? fh->m_fFormLift : -fh->m_fFormLift;
            const float s   = (float)((((x87d)val * fh->m_fAttackLift) * ts) * v.m_fTurnMass);
            v.ApplyTurnForce(up * s, right + c.com);
        }
        {
            const float val = fwd.z < 0.0f ? fh->m_fFormLift : -fh->m_fFormLift;
            const float s   = (float)((((x87d)val * fh->m_fAttackLift) * ts) * v.m_fTurnMass);
            v.ApplyTurnForce(up * s, fwd + c.com);
        }
    }

    // Inputs
    if (!CHeli::bHeliControlsCheat) { // 0xC1C970
        if (upDown == FLYING_CONTROL_NOT_SET) {
            upDown = 0.0f;
            if (pad) {
                upDown = (float)((x87d)(int32)pad->GetSteeringUpDown() * 0.0078125f);
            }
        }
        if (leftRight == FLYING_CONTROL_NOT_SET) {
            leftRight = 0.0f;
            if (pad) {
                leftRight = (float)((x87d)(-(int32)pad->GetSteeringLeftRight()) * 0.0078125f);
            }
        }
        if (skid == FLYING_CONTROL_NOT_SET) {
            skid = 0.0f;
            if (pad) {
                skid = (float)pad->GetLookRight();
                if (pad->GetLookLeft()) {
                    skid = -1.0f;
                }
                if (std::fabs((x87d)(int32)pad->GetCarGunLeftRight()) > 1.0) {
                    skid = (float)((x87d)(int32)pad->GetCarGunLeftRight() * 0.0078125f);
                }
            }
        }
    } else {
        if (upDown == FLYING_CONTROL_NOT_SET) {
            upDown = 0.0f;
            if (pad) {
                upDown = (float)((x87d)(int32)pad->GetSteeringUpDown() * 0.0078125f);
            }
        }
        if (leftRight == FLYING_CONTROL_NOT_SET) {
            leftRight = 0.0f;
            if (pad) {
                leftRight = (float)pad->GetLookLeft();
            }
        }
        if (skid == FLYING_CONTROL_NOT_SET) {
            skid = 0.0f;
            if (pad) {
                if (pad->GetLookRight()) {
                    leftRight = -1.0f;
                }
                skid = (float)((x87d)(int32)pad->GetSteeringLeftRight() * 0.0078125f);
            }
        }
    }

    if (v.vehicleFlags.bHeliMinimumTilt) {
        const x87d s = (X87R(x87::sin(X87R(x87::asin((x87d)fh->m_fPitch / (x87d)fh->m_fAttackLift) * 0.25f)) / fh->m_fPitch)) * fh->m_fAttackLift;
        upDown    = (float)(upDown * s);
        leftRight = (float)(s * leftRight);
    }

    // Pitch / roll
    v.ApplyTurnForce(FlyingControlTorque(upDown, up, fh->m_fPitch, v.m_fTurnMass, ts), fwd + c.com);
    v.ApplyTurnForce(FlyingControlTorque(leftRight, up, fh->m_fRoll, v.m_fTurnMass, ts), right + c.com);

    // Side drag
    {
        const float  d    = (float)((((x87d)V.z * right.z + (x87d)V.y * right.y) + (x87d)V.x * right.x) * -1.0f);
        const float  dAbs = std::fabsf(d);
        const x87d g    = ((x87d)dAbs * fh->m_fSideSlip) * d;
        const x87d xg   = (x87d)right.x * g;
        const float  yg   = (float)((x87d)right.y * g);
        const float  zg   = (float)((x87d)right.z * g);
        const x87d xm   = xg * v.m_fMass;
        const x87d ym   = (x87d)yg * v.m_fMass;
        const float  zm   = (float)((x87d)zg * v.m_fMass);
        v.ApplyMoveForce({ (float)(xm * ts), (float)(ym * ts), (float)((x87d)zm * ts) });

        // Yaw
        const x87d hh = ((x87d)dAbs * fh->m_fYawStab) * d;
        const x87d s  = ((hh + (x87d)skid * fh->m_fYaw) * ts) * v.m_fTurnMass;
        const CVector yawForce{ (float)(right.x * s), (float)(right.y * s), (float)(right.z * s) };
        const CVector yawPoint{ (float)((x87d)-fwd.x + c.com.x), (float)((x87d)-fwd.y + c.com.y), (float)((x87d)-fwd.z + c.com.z) };
        v.ApplyTurnForce(yawForce, yawPoint);
    }

    // Roll
    v.ApplyTurnForce(FlyingControlTorque(skid, fwd, fh->m_fYaw, v.m_fTurnMass, ts), right + c.com);
}

// 0x59C810 - the transpose of `Multiply3x3`, the sums stay in the FPU registers
static CVector InverseMultiply3x3Ext(const CVector& v, const CMatrix& m) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return {
        (float)(((x87d)r.y * v.y + (x87d)r.z * v.z) + (x87d)v.x * r.x),
        (float)(((x87d)f.y * v.y + (x87d)f.x * v.x) + (x87d)f.z * v.z),
        (float)(((x87d)u.y * v.y + (x87d)u.x * v.x) + (x87d)u.z * v.z)
    };
}

// Reduces the length of `v` to 1.5 * the original (0x6DADBA & 0x6DAE4A)
static void FlyingControlClampSpeed(CVector& v, double threshold, float factor) {
    const double sq  = SquaredMagnitudeExt(v);
    const float  sqF = (float)sq;
    if (sq > threshold) {
        const x87d x   = (x87d)v.x * factor;
        const x87d y   = (x87d)v.y * factor;
        const float  z   = (float)((x87d)v.z * factor);
        const x87d inv = (x87d)1.0 / std::sqrt((x87d)sqF);
        const float  invF = (float)inv;
        v.x = (float)(inv * x);
        v.y = (float)(y * invF);
        v.z = (float)((x87d)z * invF);
    }
}

// 0x6DA900 - air resistance of the turning / moving (all flight models)
static void FlyingControlDrag(FlyingControlCtx& c) {
    CVehicle&   v   = *c.veh;
    const auto& mat = *v.m_matrix;
    const auto  fh  = v.m_pFlyingHandlingData;
    const float ts  = CTimer::ms_fTimeStep;
    auto&       ms  = v.m_vecMoveSpeed;
    const float tm  = v.m_fTurnMass;

    CVector acc{ 0.0f, 0.0f, 0.0f }; // How much to change the (local) turn speed by (when `speedRes > 0`)
    const float baseX = (float)std::pow((double)fh->m_vecTurnRes.x, (double)ts);
    const float baseY = (float)std::pow((double)fh->m_vecTurnRes.y, (double)ts);
    const float baseZ = (float)std::pow((double)fh->m_vecTurnRes.z, (double)ts);

    CVector local = InverseMultiply3x3Ext(v.m_vecTurnSpeed, mat); // 0x59C810

    float exponent = 1.0f; // [L10]
    switch ((int32)c.model) {
    case FLIGHT_MODEL_PLANE:
    case FLIGHT_MODEL_UNK2:
    case FLIGHT_MODEL_BOAT:
        exponent = (float)(X87R(std::sqrt(SquaredMagnitudeExt(ms))) * (x87d)2.0);
        break;
    case FLIGHT_MODEL_BARON:
        exponent = (float)(X87R(MagnitudeExt(ms) * 6.0f));
        break;
    case FLIGHT_MODEL_HELI:
    case 8:
        exponent = (float)(X87R(MagnitudeExt(ms) + 1.0f));
        break;
    default:
        break;
    }

    const auto Axis = [&](float speedRes, float base, float& localAxis, float& accAxis) {
        if (speedRes > 0.0f) {
            const x87d q = (x87d)base / (((x87d)localAxis * localAxis) * speedRes + (x87d)1.0);
            accAxis = (float)(X87R(std::pow(q, (double)ts) * localAxis) - localAxis);
        } else {
            localAxis = (float)(X87R(std::pow((double)base, (double)exponent) * localAxis));
        }
    };
    Axis(fh->m_vecSpeedRes.x, baseX, local.x, acc.x);
    Axis(fh->m_vecSpeedRes.y, baseY, local.y, acc.y);
    Axis(fh->m_vecSpeedRes.z, baseZ, local.z, acc.z);

    v.m_vecTurnSpeed = Multiply3x3Ext(mat, local); // 0x59C790
    acc.x = (float)(acc.x * exponent);
    acc.y = (float)(acc.y * exponent);
    acc.z = (float)(acc.z * exponent);

    const auto Apply = [&](float amount, const CVector& dir, const CVector& point) {
        const float  lx = (float)(amount * (dir.x * -1.0f));
        const x87d ly = (x87d)amount * (dir.y * -1.0f);
        const x87d lz = (x87d)amount * (dir.z * -1.0f);
        v.ApplyTurnForce(
            { (float)((x87d)lx * tm), (float)(ly * tm), (float)(lz * tm) },
            point
        );
    };
    if (acc.x != 0.0f) {
        Apply(acc.x, mat.GetForward(), c.com + mat.GetUp());
    }
    if (acc.y != 0.0f) {
        Apply(acc.y, mat.GetUp(), c.com + mat.GetRight());
    }
    if (acc.z != 0.0f) {
        Apply(acc.z, mat.GetRight(), c.com + mat.GetForward());
    }

    FlyingControlClampSpeed(ms, 2.25f, 1.5f);
    FlyingControlClampSpeed(v.m_vecTurnSpeed, 0.04f, 0.2f);
}

// 0x6D85F0
void CVehicle::FlyingControl(eFlightModel flightModel, float leftRightSkid, float steeringUpDown, float steeringLeftRight, float accelerationBreakStatus) {
    if (!m_pFlyingHandlingData) {
        return;
    }
    if (CTimer::ms_fTimeStep <= 0.0f) {
        return;
    }

    FlyingControlCtx c{};
    c.veh                     = this;
    c.model                   = flightModel;
    c.pad                     = nullptr;
    c.leftRightSkid           = leftRightSkid;
    c.steeringUpDown          = steeringUpDown;
    c.steeringLeftRight       = steeringLeftRight;
    c.accelerationBreakStatus = accelerationBreakStatus;

    if (GetStatus() == STATUS_PLAYER && m_pDriver && m_pDriver->IsPlayer()) {
        c.pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
    }

    // Speed relative to the wind
    {
        CVector wind{ 0.0f, 0.0f, 0.0f };
        if (m_nCreatedBy != MISSION_VEHICLE) {
            const float s = m_pFlyingHandlingData->m_fWindMult;
            wind = CVector{ (float)((x87d)CWeather::WindDir.x * s), (float)((x87d)CWeather::WindDir.y * s), (float)((x87d)s * CWeather::WindDir.z) };
            wind = CVector{ -wind.x, -wind.y, -wind.z };
        }
        c.speed = CVector{
            (float)((x87d)wind.x + m_vecMoveSpeed.x),
            (float)((x87d)wind.y + m_vecMoveSpeed.y),
            (float)((x87d)wind.z + m_vecMoveSpeed.z)
        };
    }
    c.com = Multiply3x3Ext(*m_matrix, m_vecCentreOfMass); // 0x59C790

    switch (flightModel) { // 0x6DAEE4 (jump table), indexed through the byte table at 0x6DAEF4 = { 0, 1, 2, 1, 1, 1, 2, 3, 2 }
    case FLIGHT_MODEL_UNK:
        FlyingControlModel0(c);
        break;
    case FLIGHT_MODEL_BARON:
    case FLIGHT_MODEL_PLANE:
    case FLIGHT_MODEL_UNK2:
    case FLIGHT_MODEL_BOAT:
        FlyingControlModel1345(c);
        break;
    case FLIGHT_MODEL_RC:
    case FLIGHT_MODEL_HELI:
    case (eFlightModel)8:
        FlyingControlModel268(c);
        break;
    default: // 7 & everything > 8
        break;
    }

    FlyingControlDrag(c);
}

static void Normalise2DExt(CVector2D& v); // 0x44E480, defined below

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
        default: NOTSA_UNREACHABLE_RET({}, "Unknown rotorType");
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

            // ASM-S19l 0x6DB223: the direction is 2D (x, y of this - ped), normalised with CVector2D::Normalise
            CVector2D dirToPed{ GetPosition().x - ped.GetPosition().x, GetPosition().y - ped.GetPosition().y };
            Normalise2DExt(dirToPed);

            if (!ped.m_pAttachedTo) { // 0x6DB24C
                ped.bIsStanding = false; // 0x6DB266: clears bit 0 of the ped flags at +0x46C
                ped.ApplyMoveForce(CVector{ dirToPed * -5.f, 5.f });
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
                        ped.GetPosition() + CVector{ dirToPed * 0.35f, 0.6f}, // TODO: Magic 0.6f; 0x6DB38E: the PED's position
                        CVector{ dirToPed * 0.01f, 0.01f }, // 0x6DB369: z is the constant 0.01
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
                // ASM-S19l 0x6DB488-0x6DB54B: x is stored as a float, y and z stay unrounded in the FPU for the first dot ((x + z) + y),
                // the second dot uses the stored floats summed z, y, x
                const CVector colDir{
                    (float)((x87d)cp.m_vecPoint.x - colModelCenter.x),
                    (float)((x87d)cp.m_vecPoint.y - colModelCenter.y),
                    (float)((x87d)cp.m_vecPoint.z - colModelCenter.z)
                };
                const auto colDirOnRotorUp = (float)(((x87d)colDir.x * rotorUp.x + ((x87d)cp.m_vecPoint.z - colModelCenter.z) * rotorUp.z) + ((x87d)cp.m_vecPoint.y - colModelCenter.y) * rotorUp.y);
                const x87d absDotUp        = std::fabs((x87d)colDirOnRotorUp);

                if ( absDotUp > (x87d)(ROTOR_SEMI_THICKNESS + ROTOR_SEMI_THICKNESS)
                  && absDotUp > std::fabs(((x87d)colDir.z * cp.m_vecNormal.z + (x87d)colDir.y * cp.m_vecNormal.y) + (x87d)colDir.x * cp.m_vecNormal.x) * 0.3f
                ) {
                    continue;
                }

                wasAnyCPValid = true;

                // 0x6DB555: x, y keep the unrounded product `rotorUp * dot`, z is stored as a float first
                cpOnRotor = CVector{
                    (float)((x87d)cp.m_vecPoint.x - (x87d)rotorUp.x * colDirOnRotorUp),
                    (float)((x87d)cp.m_vecPoint.y - (x87d)rotorUp.y * colDirOnRotorUp),
                    cp.m_vecPoint.z - (float)(rotorUp.z * colDirOnRotorUp)
                };
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
                    if (!(au->m_fHeliRotorSpeed > 0.15f)) { // `fcomp; test ah, 0x41; jne`: NaN takes this branch
                        if (au->m_fHeliRotorSpeed < 0.15f / 2.f && au->m_fHeliRotorSpeed > 0.f) {
                            au->m_fHeliRotorSpeed *= -1.f;
                        }
                    } else {
                        ApplySoftCollision(entity, cp, automobileCollisionDmgIntensity);
                        // 0x6DB743: the factor (-0.0005 * turnMass) stays unrounded
                        const x87d turnK = (x87d)-0.0005f * m_fTurnMass;
                        ApplyTurnForce(
                            CVector{ (float)(colForceDir.x * turnK), (float)(colForceDir.y * turnK), (float)(colForceDir.z * turnK) },
                            cpOnRotor - colModelCenter
                        );
                        au->m_fHeliRotorSpeed = 0.15f;
                    }
                }

                // 0x6DB7EA: 100 * mass * (1/3000) stays unrounded, is compared with the float intensity and then scaled by `damageMult`
                const x87d massTerm = (x87d)100.0f * m_fMass * ExeRecip(3000.f);
                SetDamagedPieceRecord(
                    (float)((massTerm > (x87d)automobileCollisionDmgIntensity ? massTerm : (x87d)automobileCollisionDmgIntensity) * damageMult),
                    entity,
                    cp,
                    1.f
                );
            }

            if (wasAnyCPValid) {
                // 0x6DB847-0x6DB86C: not a ped, and not (dmgTime - 2000 <= now <= dmgTime)
                const auto nowMS = CTimer::GetTimeInMS();
                if (!entity->GetIsTypePed() && (nowMS > planeRotorDmgTimeMS || nowMS < planeRotorDmgTimeMS - 2000)) {
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
        CMatrix mat(RwFrameGetMatrix(component));
        CVector savedPos = mat.GetPosition();
        float angleX = -x87::atan2(
            (angleL + angleR) / 2.0f - wheelPos.z,
            mat.GetPosition().y - wheelPos.y
        );
        if (isFront) {
            angleX += PI;
        }
        mat.SetRotateX(angleX);
        mat.RotateY(x87::atan2(angleL - angleR, std::fabs(wheelPos.x) + std::fabs(wheelPos.x)));
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
    if (!(CTimer::GetTimeStep() * m_fMass * 0.0008F < vecBuoyancyForce.z)) { // 0x6DBD84: `fcomp; test ah,5; jp` - >= and NaN
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
            && fCheckedMass * std::bit_cast<float>(0x38A7C5ADu) < vecBuoyancyForce.z // 0x6DC079 fmul [0x871FB8] (folded 0.01F/125, not a divide)
            && vecBuoyancyForce.z < fCheckedMass * 0.4F * 0.008F) // 0x6DC090..0x6DC098 fmul 0.4f, fmul [0x863984]
        ) {
            bOnWater = false;

            auto fTurnForceMult = GetForward().z * m_fTurnMass * -0.00017F * vecBuoyancyForce.z;
            CPhysical::ApplyTurnForce(GetForward() * fTurnForceMult, GetUp());

            auto fMoveForceMult = DotProduct(m_vecMoveSpeed, GetForward()) / -2.0f * m_fMass;
            CPhysical::ApplyMoveForce(GetForward() * fMoveForceMult);

            if (m_f2ndSteerAngle == 0.0F) { // todo: missing checks for CTimer 0x6DC195 ?
                m_f2ndSteerAngle = (float)((x87d)CTimer::GetTimeInMS() + 300.0); // 0x6DC1AA: fild + fadd 300 (one rounding to float)
            }
            else if ((x87d)CTimer::GetTimeInMS() > m_f2ndSteerAngle) { // 0x6DC1C7: `fild time; fcomp angle; jne` - strictly greater (the integer stays exact, NaN keeps the angle)
                m_f2ndSteerAngle = 0.0F;
            }
        }
    }

    // 0x6DC1E2
    if (!bPostCollision && bOnWater && GetUp().z > 0.0F) {
        auto fMoveForce = m_vecMoveSpeed.SquaredMagnitude() * boatHandling->m_fAqPlaneForce * CTimer::GetTimeStep() * vecBuoyancyForce.z * 0.5F;
        if (m_nModelIndex == MODEL_SKIMMER)
            fMoveForce *= (m_GasPedal + 1.0F);
        else if (!(m_GasPedal > 0.05F)) // 0x6DC265: `fcomp; test ah,0x41; jne` - <= and NaN
            fMoveForce = 0.0F;
        else
            fMoveForce *= m_GasPedal;

        auto fMaxMoveForce = CTimer::GetTimeStep() * boatHandling->m_fAqPlaneLimit * m_fMass * 0.008F; // 0x6DC291 fmul [0x863984] (not / 125.0F)
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
        const bool bGasActive = std::fabs(m_GasPedal) > 0.05F; // 0x6DC3BD: `je` skips the speed test only for an ordered |gas| > 0.05 (NaN takes the speed test)
        if (!bGasActive) {
            fMoveSpeed = m_vecMoveSpeed.Magnitude2D();
        }

        if (bGasActive || fMoveSpeed > 0.01F) {
            if (IsSubBoat() && bOnWater && (x87d)fMoveSpeed > (x87d)0.05) { // 0x6DC409: compared with the 64-bit constant 0.05 at [0x859068]
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
            auto fSteerAngleSin = x87::sin(fSteerAngleChange);
            auto fSteerAngleCos = x87::cos(fSteerAngleChange);

            const auto& vecBoundingMin = CEntity::GetColModel()->m_boundBox.m_vecMin;
            CVector vecThrustPoint(0.0F, vecBoundingMin.y * boatHandling->m_fThrustY, vecBoundingMin.z * boatHandling->m_fThrustZ);
            auto vecTransformedThrustPoint = GetMatrix().TransformVector(vecThrustPoint);

            auto vecWorldThrustPos = GetPosition() + vecTransformedThrustPoint;
            float fWaterLevel;
            CWaterLevel::GetWaterLevel(vecWorldThrustPos, fWaterLevel, true); // warn: result not checked
            if (!(vecWorldThrustPos.z - 0.5F < fWaterLevel)) { // 0x6DC5BD: `fcomp; test ah,5; jp` - >= and NaN (the result of GetWaterLevel is unchecked)
                if (IsSubBoat())
                    AsBoat()->m_nBoatFlags.bBoatEngineInWater = false;
            }
            else {
                auto fThrustDepth = fWaterLevel - vecWorldThrustPos.z + 0.5F;
                fThrustDepth = fThrustDepth > 1.0F ? 1.0F : sq(fThrustDepth); // 0x6DC5DA: `fcom 1; jne` - only an ordered depth > 1 is clamped, otherwise it is squared

                if (IsSubBoat())
                    AsBoat()->m_nBoatFlags.bBoatEngineInWater = true;

                bool bIsSlowingDown = false;
                auto fGasState = std::fabs(m_GasPedal);
                if (!(fGasState > 0.01F) || m_nModelIndex == MODEL_SKIMMER) { // 0x6DC610: `fcom; test ah,0x41; jne 0x6DC940` - <= 0.01 and NaN
                    bIsSlowingDown = true;
                }
                else {
                    if (fGasState < 0.5F)
                        bIsSlowingDown = true;

                    auto fSteerAngle = m_fSteerAngle < 0.0F ? -m_fSteerAngle : m_fSteerAngle; // 0x6DC642: `fcomp 0; jp` - fchs only for an ordered negative
                    CVector vecSteer(-fSteerAngleSin, fSteerAngleCos, -fSteerAngle);
                    CVector vecSteerMoveForce = GetMatrix().TransformVector(vecSteer);
                    // 0x6DC6B4..0x6DC6F5: five successive vector * float products (each component is rounded after every factor), not one product with a scalar chain
                    vecSteerMoveForce *= fThrustDepth;
                    vecSteerMoveForce *= m_GasPedal;
                    vecSteerMoveForce *= 40.0F;
                    vecSteerMoveForce *= m_pHandlingData->m_transmissionData.m_EngineAcceleration;
                    vecSteerMoveForce *= m_fMass;

                    if (vecSteerMoveForce.z > 0.2F)
                        vecSteerMoveForce.z = sq(1.2F - vecSteerMoveForce.z) + 0.2F;

                    if (bPostCollision) {
                        if (m_GasPedal < 0.0F)
                            vecSteerMoveForce *= CVector(5.0F, 5.0F, 1.0F);

                        if (vecSteerMoveForce.z < 0.0F) // 0x6DC8A0: `fcomp 0; jp` keeps z for >= 0 and NaN
                            vecSteerMoveForce.z = 0.0F;
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
                    vecTractionLossTransformed *= fThrustDepth; // 0x6DCA7C / 0x6DCAA9: two successive vector * float products
                    vecTractionLossTransformed *= CTimer::GetTimeStep();

                    CPhysical::ApplyMoveForce(vecTractionLossTransformed);
                    CPhysical::ApplyTurnForce(vecTractionLossTransformed, vecTransformedThrustPoint);

                    auto fUsedTimeStep = std::max(CTimer::GetTimeStep(), 0.01F);
                    const float fTurnScale = fUsedTimeStep / fTraction * fTractionLoss * fSteerAngleSin * -0.75F; // 0x6DCB33..0x6DCB4D: the scalar first, then right * scalar
                    auto vecTurn = GetRight() * fTurnScale;
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
            auto fZComp = m_vecMoveSpeed.SquaredMagnitude() * fWaveMult * 0.001F; // 0x6DCF7C fmul [0x858CDC] (not / 1000.0F)
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
        (float)((((x87d)u.x * v.z + (x87d)f.x * v.y) + (x87d)r.x * v.x) + p.x),
        (float)((((x87d)u.y * v.z + (x87d)r.y * v.x) + (x87d)f.y * v.y) + p.y),
        (float)((((x87d)u.z * v.z + (x87d)r.z * v.x) + (x87d)f.z * v.y) + p.z)
    };
}

// 0x6DD130
void CVehicle::DoBoatSplashes(float fWaterDamping) {
    // NOTE: The original keeps intermediates on the x87 stack (53-bit precision), hence the `double`s below.
    const x87d speedSqExt = (x87d)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (x87d)m_vecMoveSpeed.y * m_vecMoveSpeed.y + (x87d)m_vecMoveSpeed.z * m_vecMoveSpeed.z;
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

    x87d intensity = std::sqrt((x87d)speedSq) * 0.075f * fWaterDamping;
    if (isSkimmer) {
        intensity *= 3.0f;
        if (intensity > 0.5f) { // `fcom; test ah, 0x41; jne`: NaN is left alone
            intensity = 0.5f;
        }
    } else if (intensity > 1.0f) {
        intensity = 1.0f;
    }
    if (!(intensity > 0.15f)) {
        return;
    }

    float scaled = (float)(intensity * 0.75f);
    if (isCruising) {
        const x87d doubled = (x87d)scaled + scaled;
        scaled = doubled < 1.0 ? (float)doubled : 1.0f; // `test ah, 5; jp`: NaN takes the 1.0 path
    }

    uint8 alphaByte = (uint8)NOTSA_FTOL(int32, scaled * 128.0f);
    if (alphaByte >= 0x40) {
        alphaByte = 0x40;
    }

    const x87d velScaleD = (x87d)scaled * 10.0f;
    const float  velScale  = (float)velScaleD;
    const float  sizeBase  = (float)(velScaleD + 0.75f);
    const float  lifeRand  = CGeneral::GetRandomNumberInRange(0.8f, 1.2f);
    const float  lifeBase  = (float)((((x87d)scaled + scaled) + 0.3f) * lifeRand);

    if (dist > 50.0f) {
        alphaByte = (uint8)NOTSA_FTOL(int32, (((x87d)80.0 - dist) * 0.033333335f) * alphaByte); // 0x858F10
    }

    FxPrtMult_c particleData(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
    {
        const float alpha = (float)((x87d)alphaByte * 0.003921569f); // 1 / 255
        particleData.m_Color.alpha = alpha < 1.0f ? alpha : 1.0f;

        const float size = (float)((x87d)sizeBase * 0.1f);
        particleData.m_fSize = size < 1.0f ? size : 1.0f;

        const float life = (float)((x87d)lifeBase * 0.2f);
        particleData.m_fLife = life < 1.0f ? life : 1.0f;
    }

    const CBoundingBox& bb = GetColModel()->GetBoundingBox();
    const CVector colMin = bb.m_vecMin;
    const CVector colMax = bb.m_vecMax;

    float xMult = 0.7f;
    float zMult = 0.0f;
    if (isSkimmer) {
        xMult = NOTSA_GLOBAL_EXPR(0x8D3678, (float), k_8D3678); // 0.25f
        zMult = NOTSA_GLOBAL_EXPR(0x8D3674, (float), k_8D3674); // 0.85f
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
    const x87d sq = ((x87d)v.x * v.x + (x87d)v.y * v.y) + (x87d)v.z * v.z;
    if (sq <= 0.0) {
        v.x = 1.0f;
        return;
    }
    const x87d inv = (x87d)1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
}

// 0x44E480 - `CVector2D::Normalise` as the original evaluates it (see `NormaliseExt`)
static void Normalise2DExt(CVector2D& v) {
    const x87d sq = (x87d)v.x * v.x + (x87d)v.y * v.y;
    if (sq <= 0.0) {
        v.x = 1.0f;
        return;
    }
    const x87d inv = (x87d)1.0 / std::sqrt(sq);
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
    const float   dist  = (float)std::sqrt(((x87d)toCam.x * toCam.x + (x87d)toCam.z * toCam.z) + (x87d)toCam.y * toCam.y);
    const x87d  scale = 2.0f / (x87d)dist;
    const CVector vToCam{ (float)(toCam.x * scale), (float)(toCam.y * scale), (float)(toCam.z * scale) };

    // Sun direction (+ vToCam) in the vehicle's (right, forward) basis
    const auto&  vecToSun = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];
    const x87d sx = (x87d)vToCam.x + vecToSun.x, sy = (x87d)vToCam.y + vecToSun.y, sz = (x87d)vToCam.z + vecToSun.z;
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

    x87d facing = (x87d)toCam2D.y * fwd2D.y + (x87d)toCam2D.x * fwd2D.x;
    if (facing < 0.0) {
        facing = -facing;
    }

    x87d intensity;
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
        intensity = (intensity * ((x87d)dist - 13.0f)) * 0.05882353f; // 0x871FC0
    }

    const x87d colorScale = intensity * 0.8f;
    const auto   ToColor    = [&](uint16 core) { // 0x821B40 (_ftol) truncates
        return (uint8)NOTSA_FTOL(int32, (((x87d)(core + 0x1FE) * colorScale) * CWeather::SunGlare) * 0.33333334f);
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
        const x87d sumX01 = (x87d)v1.x + v0.x;
        const x87d sumY01 = (x87d)v1.y + v0.y;
        const float  sumZ01 = (float)((x87d)v1.z + v0.z);
        const float  sumX   = (float)(sumX01 + v2.x);
        const x87d sumY   = sumY01 + v2.y;
        const x87d sumZ   = (x87d)sumZ01 + v2.z;
        const float  centerX = (float)((x87d)(float)((x87d)v3.x + sumX) * 0.25f);
        const float  centerY = (float)((x87d)(float)((x87d)v3.y + sumY) * 0.25f);
        const x87d centerZ = ((x87d)v3.z + sumZ) * 0.25f;

        // Smaller of the |center - v0| deltas
        const x87d dx    = (x87d)centerX - v0.x;
        const x87d dy    = (x87d)centerY - v0.y;
        const float  dyF   = (float)dy; // stored to a float temporary in the original
        const x87d adx   = dx < (x87d)0.0 ? -dx : dx;
        const x87d ady   = dy < (x87d)0.0 ? -dy : dy;
        x87d       delta = adx < ady ? dx : (x87d)dyF;
        if (delta < 0.0) {
            delta = -delta;
        }
        const x87d glare = delta * 1.4f;
        if (!(glare > 0.6f)) {
            continue;
        }

        const x87d offX = (x87d)glareDir.x * glare;
        const x87d offY = glare * glareDir.y;
        const float  offZ = (float)(glare * glareDir.z);
        const CVector localPos{ (float)(offX + centerX), (float)(offY + centerY), (float)((x87d)offZ + centerZ) };
        const CVector worldPos = TransformPointExt(*m_matrix, localPos); // 0x59C890
        const CVector coronaPos{
            (float)((x87d)worldPos.x + vToCam.x),
            (float)((x87d)worldPos.y + vToCam.y),
            (float)((x87d)worldPos.z + vToCam.z)
        };

        CCoronas::RegisterCorona( // 0x6FC580
            NOTSA_PTR_ID32(this) + 0x1B + i, // (sic) the id is derived from the vehicle's address
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
    // ASM-S19j 0x6DDF89-0x6DDFBA: camera - vehicle, squared length summed z, y, x in the FPU (unrounded); `fcomp 100.0; test ah,0x41; je`
    // returns only for sq > 100 (a NaN distance carries on)
    {
        const auto& camPos = TheCamera.GetPosition();
        const auto& pos    = GetPosition();
        const x87d dx = (x87d)camPos.x - pos.x;
        const x87d dy = (x87d)camPos.y - pos.y;
        const x87d dz = (x87d)camPos.z - pos.z;
        if ((dz * dz + dy * dy) + dx * dx > 100.0) {
            return;
        }
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

        // ASM-S19j 0x6DE0E6-0x6DE220: count is a signed int (1 - ftol(rain * -2)); each particle uses ONE random pair: u = rand() * (1/32767) rounded to float,
        // v = rand() * (1/32767) kept unrounded, position = v0 + v0v1 * u + v1v2 * (v * u) (the port drew two independent randoms for the two edges).
        // Roundings: x = (float)((float)(a*u + v0) + b*t), y = same with the float copy of b*t, z = (float)((float)((float)(a*u) + v0) + (float)(b*t)).
        const int32 count = 1 - NOTSA_FTOL(int32, CWeather::Rain * -2.f);
        for (int32 i = count; i > 0; i--) {
            constexpr float RAND_SCALE = 3.05185094e-05f; // 0x858C7C
            const auto r1 = rand();
            const float u = (float)((x87d)r1 * RAND_SCALE);
            const auto r2 = rand();
            const x87d t = ((x87d)r2 * RAND_SCALE) * u;

            const float x1 = (float)((x87d)v0v1.x * u + vertices[0].x);
            const float y1 = (float)((x87d)v0v1.y * u + vertices[0].y);
            const float z1 = (float)((float)((x87d)v0v1.z * u) + vertices[0].z);
            const float ty = (float)((x87d)v1v2.y * t);
            const float tz = (float)((x87d)v1v2.z * t);
            const CVector pos{
                (float)(x1 + (x87d)v1v2.x * t),
                (float)(y1 + ty),
                (float)(z1 + tz),
            };
            g_fx.m_Splash->AddParticle(pos, {}, 0.f, fxPrtMult);
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

    // NOTSA: asm sweep S19h (exe 0x6DE404/0x6DE47E/0x6DE494): the particle velocity is horizontal only - z is always 0
    // (`z = 0` is stored once, only x/y are written). `dot >= 0.05` is the JP after `FCOMP; TEST AH,5` (NaN takes it too).
    CVector vecParticleVelocity{ 0.0f, 0.0f, 0.0f };
    if (!(DotProduct(GetForward(), m_vecMoveSpeed) < 0.05f)) {
        vecParticleVelocity.x = m_vecMoveSpeed.x * 30.0f;
        vecParticleVelocity.y = m_vecMoveSpeed.y * 30.0f;
    } else {
        static float randomFactor = CGeneral::GetRandomNumberInRange(-1.8f, -0.9f);
        vecParticleVelocity.x = randomFactor * GetForward().x;
        vecParticleVelocity.y = randomFactor * GetForward().y;
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

    if (!(CGeneral::GetRandomNumberInRange(1.0f, 3.0f) * (m_GasPedal + 1.1f) > 2.5f)) // 0x6DE596: `fcomp; test ah,0x41; jne` returns for <= and for NaN
        return;

    float fMoveSpeed = m_vecMoveSpeed.Magnitude() * 0.5f;
    const float alphaRaw = 0.25f - fMoveSpeed;
    const float particleAlpha = alphaRaw < 0.0f ? 0.0f : alphaRaw; // 0x6DE5C4: `fcomp; test ah,0x41; jne` keeps the value for >= 0 and for NaN
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
static x87d RandomInRangeExt(float min, float max) {
    return ((x87d)max - (x87d)min) * ((x87d)rand() * (x87d)RAND_MAX_FLOAT_RECIPROCAL) + (x87d)min;
}

// 0x4082C0 - `CVector::Magnitude` (the result is left in the FPU)
static x87d MagnitudeExt(const CVector& v) {
    return std::sqrt(((x87d)v.x * v.x + (x87d)v.y * v.y) + (x87d)v.z * v.z);
}

// 0x59C970 - `CVector::NormaliseAndMag` as the original evaluates it. A length of 0 (or less) only writes `x = 1` and returns 1.
static float NormaliseAndMagExt(CVector& v) {
    const x87d sq = ((x87d)v.x * v.x + (x87d)v.y * v.y) + (x87d)v.z * v.z;
    if (sq <= 0.0) {
        v.x = 1.0f;
        return 1.0f;
    }
    const x87d inv = (x87d)1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
    return (float)((x87d)1.0 / inv);
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
    const x87d dx = (x87d)camPos.x - vehPos.x, dy = (x87d)camPos.y - vehPos.y, dz = (x87d)camPos.z - vehPos.z;
    const x87d distSq = (dz * dz + dy * dy) + dx * dx;
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

    if (arg2 >= 1.0f) { // (FCOMP + `TEST AH, 1` + JZ: NaN does NOT return)
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
            (float)((x87d)m_vecMoveSpeed.x * -50.0f),
            (float)((x87d)m_vecMoveSpeed.y * -50.0f),
            (float)((x87d)m_vecMoveSpeed.z * -50.0f + 2.5f)
        };
        float sparkForce = (float)((x87d)arg3 * 32.0f);
        if (wheelState == WHEEL_STATE_SPINNING && arg3 < 0.2f) {
            const CVector back = (m_matrix->GetForward() * m_GasPedal) * -12.0f;
            sparkDir   = CVector{ back.x, back.y, (float)((x87d)back.z + 2.5f) };
            sparkForce = 10.0f;
        }
        const float sparkMag = NormaliseAndMagExt(sparkDir);
        g_fx.AddSparks(*wheelPos, sparkDir, sparkMag, NOTSA_FTOL(int32, sparkForce), m_vecMoveSpeed, SPARK_PARTICLE_SPARK, 0.1f, 0.3f); // 0x49F040
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
        const int32   num   = NOTSA_FTOL(int32, X87R(MagnitudeExt(step) * countScale)); // _ftol
        const int32   count = num >= 1 ? num : 1;
        const float   countF = (float)count;
        for (int32 i = 0; i < count; i++) {
            const x87d rnd = RandomInRangeExt(0.5f, 1.0f);
            mults.m_Color.blue  = (float)rnd;
            mults.m_Color.green = (float)(X87R(rnd * 0.9f));
            mults.m_Color.red   = (float)(X87R(rnd * 0.9f));

            const float   frac = (float)((x87d)1.0 - (x87d)i / (x87d)countF);
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
            vel.x = (float)RandomInRangeExt(0.0f, (float)(((x87d)gas * m_vecMoveSpeed.x) * -30.0f));
            vel.y = (float)RandomInRangeExt(0.0f, (float)(((x87d)gas * m_vecMoveSpeed.y) * -30.0f));
            vel.z = (float)((x87d)(rand() % 10000) * 0.0001f * 0.8f);

            const x87d rnd = RandomInRangeExt(0.5f, 1.0f);
            mults.m_Color.blue  = (float)rnd;
            mults.m_Color.green = (float)(X87R(rnd * 0.9f));
            mults.m_Color.red   = (float)(X87R(rnd * 0.9f));
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

        const bool alwaysSkid = vehicleFlags.bAlwaysSkidMarks && ((uint8)wheelId & 1) && std::sqrt(((x87d)m_vecMoveSpeed.x * m_vecMoveSpeed.x) + (x87d)m_vecMoveSpeed.y * m_vecMoveSpeed.y) > 0.04f;
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
    const auto* const mi = CModelInfo::GetModelInfo(upgradeModelIndex);
    if (ClearVehicleUpgradeFlags(upgradeModelIndex, mi->CarMod)) { // 0x6D3210
        return;
    }

    // Note: The "linked" upgrade (eg.: left/right side skirts) is looked up BEFORE anything is removed
    const auto linkedUpgrade = CVehicleModelInfo::ms_linkedUpgrades.FindOtherUpgrade((int16)upgradeModelIndex);

    if (mi->bUsesVehDummy) {
        RemoveReplacementUpgrade(mi->CarMod);
        if (mi->CarMod == 2) { // Wheels
            m_fWheelScale = 1.f;
            RemoveReplacementUpgrade(5);
            RemoveReplacementUpgrade(4);
            RemoveReplacementUpgrade(7);
        }
    } else {
        RpClumpForAllAtomics(GetRpClump(), RemoveUpgradeCB, (void*)(intptr_t)mi->CarMod);
    }

    if (linkedUpgrade != -1) {
        const auto* const linkedMI = CModelInfo::GetModelInfo(linkedUpgrade);
        if (linkedMI->bUsesVehDummy) {
            RemoveReplacementUpgrade(linkedMI->CarMod);
        } else {
            RpClumpForAllAtomics(GetRpClump(), RemoveUpgradeCB, (void*)(intptr_t)linkedMI->CarMod);
        }
    }

    for (auto& upgrade : m_anUpgrades) {
        if (upgrade == upgradeModelIndex) {
            upgrade = -1;
        }
    }
}

// 0x6DFA20
void CVehicle::AddUpgrade(int32 modelIndex, int32 upgradeIndex) {
    const auto* const vmi       = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
    auto* const       upgradeMI = CModelInfo::GetModelInfo(modelIndex);
    const auto&       upgrades  = vmi->m_pVehicleStruct->m_aUpgrades;
    const auto*       posn      = &upgrades[upgradeIndex];

    const auto parentFrame = CClumpModelInfo::GetFrameFromId(GetRpClump(), posn->m_nParentComponentId);
    RpClumpForAllAtomics(GetRpClump(), RemoveUpgradeCB, (void*)(intptr_t)upgradeIndex);
    CreateUpgradeAtomic(upgradeMI, posn, parentFrame, false);

    if (posn->m_nParentComponentId == 1) {
        return;
    }

    // Use the position of the "damaged" variant's slot, if there is one
    switch (upgradeIndex) {
    case 0:  if (upgrades[3].m_nParentComponentId  != -1) posn = &upgrades[3];  break;
    case 1:  if (upgrades[4].m_nParentComponentId  != -1) posn = &upgrades[4];  break;
    case 2:  if (upgrades[5].m_nParentComponentId  != -1) posn = &upgrades[5];  break;
    case 6:  if (upgrades[7].m_nParentComponentId  != -1) posn = &upgrades[7];  break;
    case 12: if (upgrades[13].m_nParentComponentId != -1) posn = &upgrades[13]; break;
    }
    CreateUpgradeAtomic(upgradeMI, posn, parentFrame, true);

    if (parentFrame) {
        // Inlined `SetComponentVisibility(parentFrame, ATOMIC_OK)` (0x6D2690 / 0x6D26D0)
        RwFrameForAllObjects(parentFrame, SetVehicleAtomicVisibilityCB, (void*)ATOMIC_OK);
        RwFrameForAllChildren(parentFrame, SetVehicleAtomicVisibilityCB, (void*)ATOMIC_OK);
    }

    CCustomCarPlateMgr::SetupClumpAfterVehicleUpgrade(GetRpClump(), vmi->m_pPlateMaterial, vmi->m_nPlateType);
}

// 0x6DFC50
void CVehicle::UpdateTrailerLink(bool arg0, bool arg1) {
    CVector hitchPos{}, barPos{};
    if (!m_pTowingVehicle
        || (GetStatus() != STATUS_IS_TOWED && GetStatus() != STATUS_IS_SIMPLE_TOWED)
        || !GetTowHitchPos(hitchPos, true, m_pTowingVehicle)
        || !m_pTowingVehicle->GetTowBarPos(barPos, true, this)
    ) {
        BreakTowLink();
        return;
    }
    CVehicle* const tractor = m_pTowingVehicle;

    // Is the link too stretched?
    CVector delta = barPos - hitchPos;
    const auto distSq = (float)(((x87d)delta.z * delta.z + (x87d)delta.y * delta.y) + (x87d)delta.x * delta.x);
    const auto maxStretch = (CTimer::ms_fTimeStep > 0.7f ? CTimer::ms_fTimeStep : 0.7f) * 1.f;
    if (maxStretch < std::sqrt((x87d)distSq)) { // Note: NaN doesn't break the link
        BreakTowLink();
        return;
    }

    // Are we facing roughly the same way?
    const auto& tractorFwd = tractor->m_matrix->GetForward();
    const auto& myFwd      = m_matrix->GetForward();
    const auto  fwdDot     = ((x87d)tractorFwd.z * myFwd.z + (x87d)tractorFwd.y * myFwd.y) + (x87d)tractorFwd.x * myFwd.x;
    if (fwdDot < -0.3f) {
        BreakTowLink();
        return;
    }
    const auto& tractorUp = tractor->m_matrix->GetUp();
    const auto& myUp      = m_matrix->GetUp();
    const auto  upDot     = ((x87d)tractorUp.z * myUp.z + (x87d)tractorUp.y * myUp.y) + (x87d)tractorUp.x * myUp.x;
    if (upDot < 0.0) {
        BreakTowLink();
        return;
    }

    // Tow truck / tractor with its hoist lowered doesn't apply any force
    if (tractor->m_nModelIndex == MODEL_TOWTRUCK || tractor->m_nModelIndex == MODEL_TRACTOR) {
        if ((int32)TOWTRUCK_HOIST_DOWN_LIMIT - 100 < (int32)static_cast<CAutomobile*>(tractor)->m_wMiscComponentAngle) {
            return;
        }
    }

    // Make the hitch/bar position relative to the vehicle
    hitchPos -= GetPosition();
    barPos   -= tractor->GetPosition();

    const CVector mySpeed      = GetSpeed(hitchPos);
    const CVector tractorSpeed = tractor->GetSpeed(barPos);
    CVector relSpeed{
        tractorSpeed.x - mySpeed.x,
        tractorSpeed.y - mySpeed.y,
        tractorSpeed.z - mySpeed.z
    };

    if (!arg0 && arg1) {
        const auto step = 1.f > CTimer::ms_fTimeStep ? 1.f : CTimer::ms_fTimeStep;
        const CVector scaled{ 0.3f * delta.x, 0.3f * delta.y, 0.3f * delta.z };
        const x87d  invStep = (x87d)1.0 / (x87d)step; // Note: `z` uses the unrounded value, `x` and `y` the rounded one
        const auto    invStepF = (float)invStep;
        relSpeed = CVector{
            scaled.x * invStepF,
            scaled.y * invStepF,
            (float)((x87d)scaled.z * invStep)
        };
    }

    if (m_nVehicleSubType == VEHICLE_TYPE_TRAILER && static_cast<CTrailer*>(this)->m_fTrailerTowedRatio == -1000.f) { // 0x6D0AF0 - Baggage trailer
        // Remove the vertical component
        const auto& up   = m_matrix->GetUp();
        const auto  proj = (float)(((x87d)relSpeed.z * up.z + (x87d)relSpeed.y * up.y) + (x87d)relSpeed.x * up.x);
        relSpeed.x -= proj * up.x;
        relSpeed.y -= proj * up.y;
        relSpeed.z -= proj * up.z;
    }

    const CVector com = Multiply3x3_x87(*m_matrix, m_vecCentreOfMass);

    CVector dir = relSpeed;
    Normalise_x87(dir);

    const CVector lever{ hitchPos.x - com.x, hitchPos.y - com.y, hitchPos.z - com.z };
    const CVector cross = Cross_x87(lever, dir);

    const auto impulse = (float)((x87d)1.0 / (X87R(SquaredMagnitude_x87(cross) / (double)m_fTurnMass) + (x87d)1.0 / (x87d)m_fMass));
    ApplyForce(relSpeed * impulse, hitchPos, true);
}

// 0x6E0050
void CVehicle::UpdateTractorLink(bool arg0, bool arg1) {
    CVehicle* const trailer = m_pVehicleBeingTowed;
    if (!trailer) {
        return;
    }

    CVector barPos{}, hitchPos{};
    if (!trailer->GetTowHitchPos(hitchPos, true, this) || !GetTowBarPos(barPos, true, trailer)) {
        return; // Note: Doesn't break the link (unlike `UpdateTrailerLink`)
    }

    const CVector delta{ hitchPos.x - barPos.x, hitchPos.y - barPos.y, hitchPos.z - barPos.z };

    // Tow truck / tractor with its hoist lowered doesn't apply any force
    if (m_nModelIndex == MODEL_TOWTRUCK || m_nModelIndex == MODEL_TRACTOR) {
        if ((int32)TOWTRUCK_HOIST_DOWN_LIMIT - 100 < (int32)static_cast<CAutomobile*>(this)->m_wMiscComponentAngle) {
            return;
        }
    }

    // Make the hitch/bar position relative to the vehicles
    hitchPos -= trailer->GetPosition();
    barPos   -= GetPosition();

    const CVector trailerSpeed = trailer->GetSpeed(hitchPos);
    const CVector mySpeed      = GetSpeed(barPos);
    CVector relSpeed{
        trailerSpeed.x - mySpeed.x,
        trailerSpeed.y - mySpeed.y,
        trailerSpeed.z - mySpeed.z
    };

    if (!arg0) {
        // Scale by the (inverse) mass ratio - the factor stays unrounded in the original
        const x87d scale = ((x87d)1.0 - (x87d)m_fMass / ((x87d)trailer->m_fMass + (x87d)m_fMass)) * 0.5f;
        relSpeed = CVector{
            (float)((x87d)relSpeed.x * scale),
            (float)((x87d)relSpeed.y * scale),
            (float)((x87d)relSpeed.z * scale)
        };

        if (arg1) {
            const auto step = 1.f > CTimer::ms_fTimeStep ? 1.f : CTimer::ms_fTimeStep;
            const CVector scaled{ 0.1f * delta.x, 0.1f * delta.y, 0.1f * delta.z };
            const x87d  invStep  = (x87d)1.0 / (x87d)step; // Note: `z` uses the unrounded value, `x` and `y` the rounded one
            const auto    invStepF = (float)invStep;
            relSpeed = CVector{
                scaled.x * invStepF,
                scaled.y * invStepF,
                (float)((x87d)scaled.z * invStep)
            };
        }
    }

    if (trailer->m_nVehicleSubType == VEHICLE_TYPE_TRAILER && static_cast<CTrailer*>(trailer)->m_fTrailerTowedRatio == -1000.f) { // 0x6D0AF0 - Baggage trailer
        // Remove the vertical component
        const auto& up   = trailer->m_matrix->GetUp();
        const auto  proj = (float)(((x87d)relSpeed.z * up.z + (x87d)relSpeed.y * up.y) + (x87d)relSpeed.x * up.x);
        relSpeed.x -= proj * up.x;
        relSpeed.y -= proj * up.y;
        relSpeed.z -= proj * up.z;
    }

    const CVector com = Multiply3x3_x87(*m_matrix, m_vecCentreOfMass);

    CVector dir = relSpeed;
    Normalise_x87(dir);

    const CVector lever{ barPos.x - com.x, barPos.y - com.y, barPos.z - com.z };
    const CVector cross = Cross_x87(lever, dir);

    // Note: Unlike in `UpdateTrailerLink` the impulse isn't rounded to a float before being applied
    const x87d impulse = (x87d)1.0 / (X87R(SquaredMagnitude_x87(cross) / (double)m_fTurnMass) + (x87d)1.0 / (x87d)m_fMass);
    ApplyForce(
        CVector{ (float)((x87d)relSpeed.x * impulse), (float)((x87d)relSpeed.y * impulse), (float)((x87d)relSpeed.z * impulse) },
        barPos,
        true
    );
    m_nFakePhysics = 0;
}

// 0x6E0400
CEntity* CVehicle::ScanAndMarkTargetForHeatSeekingMissile(CEntity* entity) {
    CEntity* target = CWeapon::PickTargetForHeatSeekingMissile(GetPosition(), m_matrix->GetForward(), 1.2f, this, true, entity);

    CPlane* const plane = IsSubPlane() ? AsPlane() : nullptr;
    // Re-check the line of sight right away if we haven't done it yet
    const uint32 recheckBias = plane ? (plane->field_9E4 & 0x7FFFFFFF) == 0 : 0u;

    bool isLOSClear;
    if (target && CTimer::GetTimeInMS() - recheckBias > 1000) {
        const bool myUsedCollision     = GetUsesCollision();
        const bool targetUsedCollision = target->GetUsesCollision();
        SetUsesCollision(false);
        target->SetUsesCollision(false);

        isLOSClear = CWorld::GetIsLineOfSightClear(GetPosition(), target->GetPosition(), true, true, false, true, false, true, false);

        SetUsesCollision(myUsedCollision);
        target->SetUsesCollision(targetUsedCollision);

        if (plane) {
            plane->field_9E4 = (uint32)(CTimer::GetTimeInMS() > 1) | ((uint32)isLOSClear << 31);
        }
    } else if (plane) {
        isLOSClear = (uint32)plane->field_9E4 >> 31;
    } else {
        isLOSClear = true; // Not checked in this case
    }

    if (!isLOSClear) {
        target = nullptr;
        CWeaponEffects::ClearCrossHairImmediately(0);
        return target;
    }

    if (target && GetStatus() == STATUS_PLAYER) {
        CWeaponEffects::MarkTarget(0, target->GetPosition(), 255, 255, 255, 100, 1.3f, true);
    } else {
        CWeaponEffects::ClearCrossHairImmediately(0);
    }
    return target;
}

// 0x6E05C0
void CVehicle::FireHeatSeakingMissile(CEntity* targetEntity, eOrdnanceType type, bool checkTime) {
    auto& lastFiredTime = type == 1 ? m_nProjectileWeaponFiringTime : m_nAdditionalProjectileWeaponFiringTime;
    if (checkTime && CTimer::GetTimeInMS() <= GetPlaneOrdnanceRateOfFire(type) + lastFiredTime) {
        return;
    }

    const CVector pos           = GetPosition();
    const CVector ordnancePos   = GetPlaneOrdnancePosition(type);
    const auto&   fwd           = m_matrix->GetForward();

    // Alternate between the left/right hard-point
    const x87d ordnanceX = m_nOrdnanceCycleIndex != 0 ? -(x87d)ordnancePos.x : (x87d)ordnancePos.x;
    m_nOrdnanceCycleIndex = m_nOrdnanceCycleIndex == 0 ? 1 : 0;

    // Place the missile somewhere in front of us, depending on our velocity (x87 evaluation order, see asm)
    const x87d speedFwd = ((x87d)m_vecMoveSpeed.z * fwd.z + (x87d)m_vecMoveSpeed.y * fwd.y) + (x87d)m_vecMoveSpeed.x * fwd.x;
    const x87d s        = speedFwd < (x87d)0.0 ? (x87d)0.0 : speedFwd; // Note: NaN is passed through
    const x87d offX     = (s * fwd.x) * CTimer::ms_fTimeStep;
    const float  fy       = (float)(s * fwd.y);
    const float  fz       = (float)(s * fwd.z);
    const x87d offY     = (x87d)fy * CTimer::ms_fTimeStep;
    const float  offZ     = (float)((x87d)fz * CTimer::ms_fTimeStep);
    const float  x        = (float)(offX + ordnanceX);

    const CVector posn{
        pos.x + x,
        (float)((x87d)pos.y + ((x87d)ordnancePos.y + offY)),
        (float)((x87d)pos.z + ((x87d)ordnancePos.z + (x87d)offZ))
    };
    const CVector direction = fwd;
    CProjectileInfo::AddProjectile(this, WEAPON_ROCKET_HS, posn, 0.f, &direction, targetEntity);

    if (m_pDriver && m_pDriver->IsPlayer()) {
        CPad::GetPad(m_pDriver->GetPadNumber())->StartShake(240, 160, 0);
    }

    lastFiredTime = CTimer::GetTimeInMS();
}

// 0x6E07E0
void CVehicle::PossiblyDropFreeFallBombForPlayer(eOrdnanceType type, bool checkTime) {
    auto& lastFiredTime = type == 1 ? m_nProjectileWeaponFiringTime : m_nAdditionalProjectileWeaponFiringTime;
    if (checkTime && CTimer::GetTimeInMS() <= GetPlaneOrdnanceRateOfFire(type) + lastFiredTime) {
        return;
    }

    const CVector pos         = GetPosition();
    const CVector ordnancePos = GetPlaneOrdnancePosition(type);

    // Alternate between the left/right hard-point
    const float ordnanceX = m_nOrdnanceCycleIndex != 0 ? -ordnancePos.x : ordnancePos.x;
    m_nOrdnanceCycleIndex = m_nOrdnanceCycleIndex == 0 ? 1 : 0;

    const CVector posn{ pos.x + ordnanceX, ordnancePos.y + pos.y, ordnancePos.z + pos.z };
    CProjectileInfo::AddProjectile(this, WEAPON_FREEFALL_BOMB, posn, 0.f, &m_matrix->GetForward(), nullptr);

    if (m_pDriver && m_pDriver->IsPlayer()) {
        CPad::GetPad(m_pDriver->GetPadNumber())->StartShake(240, 160, 0);
    }

    lastFiredTime = CTimer::GetTimeInMS();
}

// 0x6E0950
void CVehicle::ProcessSirenAndHorn(bool horn) {
    if (UsesSiren()) {
        // The pad's horn history (5 entries) is used to detect a "double tap" of the horn
        const auto& pad = *CPad::GetPad(0);
        const auto  idx = (uint8)pad.iCurrHornHistory;
        const auto  HornAt = [&](size_t offset) { return pad.bHornHistory.data()[(idx + offset) % pad.bHornHistory.size()] != 0; };

        if (!pad.bHornHistory.data()[idx]) {
            if (HornAt(4) && !HornAt(1)) { // Tap, release => toggle the siren
                m_HornCounter = 0;
                vehicleFlags.bSirenOrAlarm = !vehicleFlags.bSirenOrAlarm;
                return;
            }
            m_HornCounter = 0;
        } else {
            m_HornCounter = HornAt(4) && HornAt(3) ? 1 : 0;
        }
    } else if (horn) {
        if (CanUpdateHornCounter()) {
            m_HornCounter = CPad::GetPad(0)->GetHorn();
        }
    }
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
    const auto    alpha         = NOTSA_FTOL(uint8, (1.0f - std::fabs(DotProduct(pointToCamDir, vehicleMatrix.GetForward()))) * 32.0f);

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
    // ASM-S19g (0x6E1440 / 0x6E1600): the Single/Twin exe functions are separate, only TWIN + both sides draws the twin shadow
    // (the old "no TWIN => combine harvester is twin" test belongs to DoHeadLightReflection 0x6E1720, which already did it)
    const bool doTwin   = (flags & VEHICLE_LIGHTS_TWIN) && includeLeft && includeRight;
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

        // x87 evaluation order: od = ((1 + 2*size) + y) stays unrounded, the position is (right*x + (pos + fwd*od)) (single)
        const x87d  od             = ((x87d)1.0 + (x87d)(lightSize + lightSize)) + (x87d)vehOffset.y;
        const auto  shdwFront      = lightFwd2D * (lightSize + lightSize);
        const auto  shdwSide       = (lightFwd2D * lightSize).GetPerpRight();
        const auto  pos            = GetPosition();
        x87d        px             = (x87d)pos.x + (x87d)lightFwd2D.x * od;
        x87d        py             = (x87d)pos.y + od * (x87d)lightFwd2D.y;
        if (doSingle) {
            const auto lightRight2D = CVector2D(vehicleMatrix.GetRight()).Normalized();
            px = (x87d)lightRight2D.x * (x87d)vehOffset.x + px;
            py = (x87d)lightRight2D.y * (x87d)vehOffset.x + py;
        }

        CShadows::StoreCarLightShadow(
            this,
            static_cast<int32>(NOTSA_PTR_ID32(&m_matrix)) + 2,
            doTwin ? gpShadowHeadLightsTex : gpShadowHeadLightsTex2,
            CVector{ (float)px, (float)py, pos.z + 2.0f },
            shdwFront.x, shdwFront.y,
            shdwSide.x, shdwSide.y,
            45, 45, 45,
            7.0f
        );
    }
}

// 0x6E1720
void CVehicle::DoHeadLightReflection(CMatrix& vehicleMatrix, eVehicleLightsFlags flags, bool includeLeft, bool includeRight) {
    // ASM-S19f: 0x6E1720. Without TWIN the combine harvester gets the twin shadow and everything else the single right one,
    // whatever the include flags say (a quad, which is not an automobile, still gets its reflection)
    if (flags & VEHICLE_LIGHTS_TWIN) {
        if (includeLeft) {
            if (includeRight) {
                DoHeadLightReflectionTwin(vehicleMatrix);
            } else {
                DoHeadLightReflectionSingle(vehicleMatrix, false);
            }
        } else if (includeRight) {
            DoHeadLightReflectionSingle(vehicleMatrix, true);
        }
    } else if (ModelIndices::IsCombineHarvester(GetModelIndex())) {
        DoHeadLightReflectionTwin(vehicleMatrix);
    } else {
        DoHeadLightReflectionSingle(vehicleMatrix, true);
    }
}

// 0x6E1780
// lightId here refers to an ordinal of entry of one kind dummy subgroup e.g. taillights here
bool CVehicle::DoTailLightEffect(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm, eVehicleLightsFlags flags_unused, bool staticEmission) {
    return DoLightEffectImpl(false, lightId, vehicleMatrix, isRight, disabledOrAlarm, staticEmission);
}

// NOTSA
// headlight/taillight
bool CVehicle::DoLightEffectImpl(bool isFront, eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm, bool staticEmission, bool inlinedSecondary) {
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
        // ASM-S19e: 0x6E0D32 / 0x6E193F multiply by the constant first, the angle last.
        // ASM-S19f: the copies of the front effect inlined into DoVehicleLights for the secondary lights (0x6E21D0, 0x6E2569) do the angle first, the constant last.
        const auto size      = inlinedSecondary
            ? ((1.0f - distToCam * (1.0f / 150.0f)) * ang) * baseCorRotMult
            : (1.0f - distToCam * (1.0f / 150.0f)) * baseCorRotMult * ang;

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
                lightColorR = NOTSA_FTOL(uint8, 190.0f * intensity);
                lightColorG = NOTSA_FTOL(uint8, intensity * 255.0f);
            } else {
                lightColorR = NOTSA_FTOL(uint8, 210.0f * intensity);
                lightColorG = NOTSA_FTOL(uint8, intensity * 195.0f);
            }

            RegisterCorona(reinterpret_cast<uintptr>(this) + 2 * static_cast<uint32>(lightId) + isRight, lightColorR, lightColorR, lightColorG, 128u, tweakedDummy, coronaSize, eCoronaType::CORONATYPE_HEADLIGHT, eCoronaReflType::CORREFL_SIMPLE, 0.5f);
        } else {
            uint8 redIntensity = 0u;

            if (m_pDriver && m_BrakePedal > 0.0f && !vehicleFlags.bIsHandbrakeOn) {
                redIntensity = NOTSA_FTOL(uint8, 128.0f * intensity);
                isBraking    = true;
            } else if (staticEmission) {
                redIntensity = NOTSA_FTOL(uint8, 96.0f * intensity);
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
        // ASM-S19f: 0x6E1C13 stores forceOn on both paths, so the dark phase of the alarm also drops an override-ON
        if (CTimer::GetTimeInMS() & 0x100) {
            forceOn = true;
        } else {
            forceOn  = false;
            forceOff = true;
        }
    }

    // ASM-S19f: the zr350 pop-up (0x6E1C17-0x6E1CC8) only runs with the engine on, before the render-lights flags are cleared
    if (vehicleFlags.bEngineOn && GetModelIndex() == MODEL_ZR350 && GetStatus() != STATUS_WRECKED) {
        // calculate zr350's pop-up lights rotation
        constexpr auto popUpTarget = 0.69813174f; // aka (2.f / 9.f) * PI rad = 40 deg
        if (vehicleFlags.bLightsOn || forceOn || !CanUpdateHornCounter()) {
            asAuto->m_fPropRotate = notsa::step_up_to(asAuto->m_fPropRotate, popUpTarget, CTimer::GetTimeStep() * 0.01f);
            if (asAuto->m_fPropRotate < popUpTarget) {
                return;
            }
        } else if (asAuto->m_fPropRotate > 0.0f) {
            asAuto->m_fPropRotate = notsa::step_down_to(asAuto->m_fPropRotate, 0.0f, CTimer::GetTimeStep() * 0.01f);
        }
    }

    // ASM-S19f: 0x6E1CCE clears the four render-lights flags every call, also when the engine is off
    m_renderLights.m_bRightFront = false;
    m_renderLights.m_bLeftFront  = false;
    m_renderLights.m_bRightRear  = false;
    m_renderLights.m_bLeftRear   = false;

    // ASM-S19f: 0x6E1CF1-0x6E1DB6. The DISABLE flags override the damage state and the left lights need the TWIN flag
    // (the port used `a && b || c && d`, so a disabled side was still lit on an undamaged automobile).
    const bool twin = (flags & VEHICLE_LIGHTS_TWIN) != 0;
    const auto IsLightOk = [&](eLights light) {
        return (flags & VEHICLE_LIGHTS_IGNORE_DAMAGE) || (IsAutomobile() && asAuto->m_damageManager.GetLightStatus(light) == VEHICLE_LIGHT_OK);
    };
    const bool lightOkFR = !(flags & VEHICLE_LIGHTS_DISABLE_FRONT) && IsLightOk(LIGHT_FRONT_RIGHT);
    const bool lightOkFL = !(flags & VEHICLE_LIGHTS_DISABLE_FRONT) && twin && IsLightOk(LIGHT_FRONT_LEFT);
    const bool lightOkRR = !(flags & VEHICLE_LIGHTS_DISABLE_REAR)  && IsLightOk(notsa::bugfixes::CDamageManager_GetLightStatus_IncorrectStatusCheckForLightRR ? LIGHT_REAR_RIGHT : LIGHT_REAR_LEFT);
    const bool lightOkRL = !(flags & VEHICLE_LIGHTS_DISABLE_REAR)  && twin && IsLightOk(LIGHT_REAR_LEFT);

    if (!vehicleFlags.bEngineOn) {
        return;
    }

    // rear lights: main, then the secondary of the same side when the main one lit up (0x6E2657-0x6E270C, 0x6E1DE4-0x6E1E8D)
    const auto RenderRearLights = [&](bool disabledR, bool disabledL, bool staticEmission) {
        m_renderLights.m_bRightRear = DoLightEffectImpl(false, eVehicleLightId::MAIN, vehicleMatrix, true, disabledR, staticEmission);
        if (m_renderLights.m_bRightRear) {
            DoLightEffectImpl(false, eVehicleLightId::SECONDARY, vehicleMatrix, true, disabledR, staticEmission);
        }
        if (twin) {
            m_renderLights.m_bLeftRear = DoLightEffectImpl(false, eVehicleLightId::MAIN, vehicleMatrix, false, disabledL, staticEmission);
            if (m_renderLights.m_bLeftRear) {
                DoLightEffectImpl(false, eVehicleLightId::SECONDARY, vehicleMatrix, false, disabledL, staticEmission);
            }
        }
    };

    if (!vehicleFlags.bLightsOn && !forceOn && !forceOff) {
        // lights are off - process only dynamic part of taillight effect
        RenderRearLights(!lightOkRR, !lightOkRL, false);
        return;
    }

    // ASM-S19f: 0x6E1E9C-0x6E229F. With the lights off but the alarm in its dark phase (forceOff) the exe still takes this path with
    // everything disabled. Front: both mains first, then the secondaries (inlined copies in the exe), unlike the rear.
    const bool disabledFR = forceOff || !lightOkFR;
    const bool disabledFL = forceOff || !lightOkFL;
    m_renderLights.m_bRightFront = DoLightEffectImpl(true, eVehicleLightId::MAIN, vehicleMatrix, true, disabledFR, false);
    m_renderLights.m_bLeftFront  = DoLightEffectImpl(true, eVehicleLightId::MAIN, vehicleMatrix, false, disabledFL, false);
    if (m_renderLights.m_bRightFront) {
        DoLightEffectImpl(true, eVehicleLightId::SECONDARY, vehicleMatrix, true, disabledFR, false, true);
    }
    if (m_renderLights.m_bLeftFront) {
        DoLightEffectImpl(true, eVehicleLightId::SECONDARY, vehicleMatrix, false, disabledFL, false, true);
    }

    // process static part of taillight effect
    RenderRearLights(forceOff || !lightOkRR, forceOff || !lightOkRL, true);

    if (!IsSubTrain()) {
        if (forceOff) {
            return; // 0x6E271C
        }
        // draw light shadows
        CVehicle::DoHeadLightReflection(vehicleMatrix, flags, lightOkFL, lightOkFR);
    } else if (forceOff) {
        return; // 0x6E2739
    }

    // add directionals
    if (lightOkFR || lightOkFL) {
        CPointLights::AddLight(
            ePointLightType::PLTYPE_DIRECTIONAL,
            GetPosition(), // ASM-S19f: 0x6E2772 reads the vehicle's own matrix, only the direction comes from `vehicleMatrix`
            vehicleMatrix.GetForward(),
            20.0f,
            1.0f, 1.0f, 1.0f,
            m_vecMoveSpeed.SquaredMagnitude2D() < 0.2025f ? 1u : 0u // ASM-S19f: 0x6E2769-0x6E2775 push 1 below the threshold, 0 at/above it (and for NaN)
        );
    }

    if ((lightOkRR || lightOkRL)
        && m_BrakePedal > 0.0f
        && !vehicleFlags.bIsHandbrakeOn
        && m_pDriver) {
        CPointLights::AddLight(
            ePointLightType::PLTYPE_DIRECTIONAL,
            GetPosition() - 4.0f * vehicleMatrix.GetForward(), // ASM-S19f: 0x6E27EE-0x6E2828 scales the forward of `vehicleMatrix`
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

// unused
// 0x6E2900
void CVehicle::FillVehicleWithPeds(bool setClothesToAfro) {
    if (setClothesToAfro) {
        const auto playerPed = FindPlayerPed(PED_TYPE_PLAYER1);
        CStats::SetStatValue(STAT_FAT, 1000.0f);
        playerPed->GetPlayerData()->m_pPedClothesDesc->SetModel("afro", CLOTHES_MODEL_HEAD);
        CClothes::RebuildPlayer(playerPed, false);
    }
    // ASM-S19g 0x6E2950..0x6E2A14: seats -1 (driver) .. max-1 in ONE loop; the model check is per seat (request and go on to the next
    // seat), the previous driver / passenger ped is DELETED (virtual dtor with the delete flag) after the new ped was created, and
    // passengers are seated with isDriver = false (the port passed true for every seat)
    const eModelID modelId = setClothesToAfro ? MODEL_PLAYER : MODEL_WMOST;
    const auto*    playerPed = FindPlayerPed(PED_TYPE_PLAYER1);
    for (int32 seat = -1; seat < (int32)m_nMaxPassengers; seat++) {
        if (seat == -1 && m_pDriver && m_pDriver == playerPed) {
            continue;
        }
        if (!CStreaming::IsModelLoaded(modelId)) {
            CStreaming::RequestModel(modelId, STREAMING_KEEP_IN_MEMORY);
            continue;
        }
        CPed* const ped = CPopulation::AddPed(PED_TYPE_CIVFEMALE, modelId, GetPosition(), false);
        if (seat == -1) {
            delete m_pDriver;
            CCarEnterExit::SetPedInCarDirect(ped, this, 0, true);
        } else {
            delete m_apPassengers[seat];
            CCarEnterExit::SetPedInCarDirect(ped, this, CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(this, seat), false);
        }
    }
}

// 0x6E2E50
bool CVehicle::DoBladeCollision(CVector pos, CMatrix& matrix, int16 rotorType, float radius, float damageMult) {
    NOTSA_GLOBAL_LOCAL(s_TestBladeCol, 0xC1CD38, (CColModel), {}); VALIDATE_SIZE(s_TestBladeCol, 48);
    NOTSA_GLOBAL_LOCAL(s_TestBladeColData, 0xC1CD68, (CCollisionData), {}); VALIDATE_SIZE(s_TestBladeColData, 48);
    NOTSA_GLOBAL_LOCAL(s_TestBladeColSphere, 0xC1CD98, (CColSphere), {});

    // Set-up collision model for the blade
    {
        CVector bbMin(pos - CVector(radius, radius, radius));
        CVector bbMax(pos + CVector(radius, radius, radius));

        // ASM-S19j 0x6E2E8A: jump table over rotorType+3 (-3..3): 0 and everything outside leave the cube alone (the port asserted)
        if (rotorType >= -3 && rotorType <= 3 && rotorType != 0) {
            const auto axis = abs(rotorType) - 1;
            bbMin[axis] = pos[axis] - ROTOR_SEMI_THICKNESS;
            bbMax[axis] = pos[axis] + ROTOR_SEMI_THICKNESS;
        }

        s_TestBladeCol.m_boundBox.Set(bbMin, bbMax);
        s_TestBladeCol.m_boundSphere.Set(radius, pos);
        s_TestBladeCol.m_pColData = &s_TestBladeColData;

        s_TestBladeColSphere.Set(radius, pos, SURFACE_DEFAULT);

        s_TestBladeColData.m_pSpheres = &s_TestBladeColSphere;
        s_TestBladeColData.m_nNumSpheres = 1;
    }

    bool collided = false;

    // ASM-S19j 0x6E2F6B-0x6E3252: the centre is transformed by the `matrix` argument (the port used this vehicle's own matrix), the sector
    // range is clamped to [0, 120] (min: `v > 0 ? v : 0`, max: `v < 120 ? v : 120`) before the loops (y outer, x inner), and the sector
    // lists go Buildings, Vehicles, Peds (damage 0), Objects. GetRepeatSector masks the (clamped) indices with & 15.
    const CVector center = matrix.TransformPoint(pos);
    const auto minX = std::max(CWorld::GetSectorX(center.x - radius), 0);
    const auto minY = std::max(CWorld::GetSectorY(center.y - radius), 0);
    const auto maxX = std::min(CWorld::GetSectorX(center.x + radius), (int32)MAX_SECTORS_X);
    const auto maxY = std::min(CWorld::GetSectorY(center.y + radius), (int32)MAX_SECTORS_Y);

    CWorld::AdvanceCurrentScanCode();
    for (int32 y = minY; y <= maxY; y++) {
        for (int32 x = minX; x <= maxX; x++) {
            auto& s = CWorld::GetSector(x, y);
            auto& rs = CWorld::GetRepeatSector(x, y);
            collided |= BladeColSectorList(s.Buildings, s_TestBladeCol, matrix, rotorType, damageMult);
            collided |= BladeColSectorList(rs.Vehicles, s_TestBladeCol, matrix, rotorType, damageMult);
            collided |= BladeColSectorList(rs.Peds, s_TestBladeCol, matrix, rotorType, 0.0f);
            collided |= BladeColSectorList(rs.Objects, s_TestBladeCol, matrix, rotorType, damageMult);
        }
    }

    s_TestBladeColData.m_nNumSpheres = 0;
    s_TestBladeColData.m_pSpheres = nullptr; // 0x6E3257-0x6E3267 zero the sphere pointer, the sphere count and the col-data pointer
    s_TestBladeCol.m_pColData = nullptr;

    return collided;
}

// 0x6E3290
int32 CVehicle::AddVehicleUpgrade(int32 modelId) {
    // Note: The original returns `replacedUpgrade` (in EAX); COMMAND_ADD_VEHICLE_MOD (1767) stores it as its result
    const auto* const mi = CModelInfo::GetModelInfo(modelId);

    int32 replacedUpgrade = -1;
    if (!SetVehicleUpgradeFlags(modelId, mi->CarMod, replacedUpgrade)) { // 0x6D30E0
        const auto linkedUpgrade = CVehicleModelInfo::ms_linkedUpgrades.FindOtherUpgrade((int16)modelId);

        if (mi->bUsesVehDummy) {
            replacedUpgrade = GetReplacementUpgrade(mi->CarMod);
            AddReplacementUpgrade(modelId, mi->CarMod);
            if (mi->CarMod == 2) { // Wheels
                m_fWheelScale = CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr()->m_fWheelSizeFront;
                AddReplacementUpgrade(modelId, 5);
                AddReplacementUpgrade(modelId, 4);
                AddReplacementUpgrade(modelId, 7);
                if (m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
                    if (modelId == ModelIndices::MI_OFFROAD_WHEEL) {
                        handlingFlags.bOffroadAbility = true;
                    } else if (!m_pHandlingData->m_bOffroadAbility) {
                        handlingFlags.bOffroadAbility = false;
                    }
                }
            }
        } else {
            replacedUpgrade = GetUpgrade(mi->CarMod);
            AddUpgrade(modelId, mi->CarMod);
        }

        if (linkedUpgrade != -1) {
            const auto* const linkedMI = CModelInfo::GetModelInfo(linkedUpgrade);
            if (linkedMI->bUsesVehDummy) {
                AddReplacementUpgrade(linkedUpgrade, linkedMI->CarMod);
            } else {
                AddUpgrade(linkedUpgrade, linkedMI->CarMod);
            }
        }
    }

    // Store the upgrade in the first slot that is free (or holds the upgrade we replaced)
    int32 toStore = modelId;
    for (auto& slot : m_anUpgrades) {
        if (slot == replacedUpgrade || slot == -1) {
            slot    = (int16)toStore;
            toStore = -1;
        }
    }
    return replacedUpgrade;
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
CEntity* CVehicle::GetPlaneWeaponFiringStatus(bool& status, eOrdnanceType& ordnanceType) {
    auto& pad = *CPad::GetPad(CWorld::FindPlayerSlotWithVehiclePointer(this)); // Note: returns 0 if not found

    switch (m_nModelIndex) {
    case MODEL_HUNTER:
    case MODEL_TORNADO: { // 0x6E34A6
        if (pad.GetCarGunFired() == 2) {
            status = true;
            return nullptr;
        }
        if (pad.CarGunJustDown() == 1) {
            ordnanceType = 1;
            return nullptr;
        }
        break;
    }
    case MODEL_SEASPAR:
    case MODEL_RCBARON:
    case MODEL_MAVERICK:
    case MODEL_POLMAV:
    case MODEL_CARGOBOB: { // 0x6E3780
        if (pad.GetCarGunFired() == 2) {
            status = true;
        }
        if (m_nModelIndex == MODEL_RCBARON && bDisableRemoteDetonation && CCamera::m_bUseMouse3rdPerson) {
            if (pad.GetCarGunFired() == 1) {
                status = true;
            }
        }
        ordnanceType = 0;
        return nullptr;
    }
    case MODEL_RUSTLER: { // 0x6E347C
        if (pad.GetCarGunFired() == 2) {
            status = true;
        }
        ordnanceType = 0;
        return nullptr;
    }
    case MODEL_HYDRA: { // 0x6E34ED
        ordnanceType = 0;

        const auto now = CTimer::GetTimeInMS();
        if (pad.CarGunJustDown() == 1 && CTimer::GetTimeInMS() > m_nGunFiringTime + 2000) { // Flares
            const CVector pos    = GetPosition();
            const CVector offset = (m_matrix->GetForward() * -2.5f) + (m_matrix->GetUp() * -1.f);
            CProjectileInfo::AddProjectile(this, WEAPON_FLARE, pos + offset, 0.f, &m_matrix->GetForward(), nullptr);
            m_nGunFiringTime = CTimer::GetTimeInMS();
        }

        auto* const plane = AsPlane();
        auto&       crossHair = gCrossHair[0];
        if (pad.CarGunJustDown() == 2) {
            if (CWeaponEffects::IsLockedOn(0) && plane->field_9DC) {
                CEntity* const target = ScanAndMarkTargetForHeatSeekingMissile(plane->field_9E0);
                if (target && target == plane->field_9E0 && now - (uint32)plane->field_9DC > 1500) {
                    ordnanceType = 2;
                    crossHair.m_color.r      = 255;
                    crossHair.m_color.g      = 0;
                    crossHair.m_color.b      = 0;
                    crossHair.m_fRotation    = 1.f;
                    crossHair.m_nTimeWhenToDeactivate = 0;
                    return target;
                }
            }
            ordnanceType = 1;
            return nullptr;
        }

        if (pad.GetEnterTargeting()) {
            plane->field_9DC = now;
            plane->field_9E0 = nullptr;
            return nullptr;
        }

        if (pad.GetTarget()) {
            if (!plane->field_9DC) {
                plane->field_9DC = now;
            }
            CEntity* const target = ScanAndMarkTargetForHeatSeekingMissile(plane->field_9E0);
            if (!target || target != plane->field_9E0) {
                plane->field_9DC = now;
            }
            crossHair.m_nTimeWhenToDeactivate = 0;
            if (now - (uint32)plane->field_9DC > 1500) { // Locked on
                crossHair.m_color.r   = 255;
                crossHair.m_color.g   = 0;
                crossHair.m_color.b   = 0;
                crossHair.m_fRotation = 1.f;
            } else {
                crossHair.m_color.r   = 255;
                crossHair.m_color.g   = 255;
                crossHair.m_color.b   = 255;
                crossHair.m_fRotation = 0.f;
            }
            plane->field_9E0 = target;
            return nullptr;
        }

        plane->field_9DC = 0;
        plane->field_9E0 = nullptr;
        return nullptr;
    }
    }

    // 0x6E37D3
    status       = false;
    ordnanceType = 0;
    return nullptr;
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
    CEntity* heatSeekingTarget = nullptr;

    if (IsSubPlane() && this == FindPlayerVehicle(-1, false) && AsPlane()->field_9DC == 0) {
        CWeaponEffects::ClearCrossHairImmediately(0);
    }

    if (physicalFlags.bRenderScorched) { // 0x20000000
        return;
    }

    bool           fireGuns = false;
    eOrdnanceType  ordnance = 0;
    const bool     isControlledByPlayer = GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED;
    if (isControlledByPlayer) {
        bool          status = false;
        eOrdnanceType type   = 0;
        heatSeekingTarget = GetPlaneWeaponFiringStatus(status, type);
        ordnance          = type;
        SelectPlaneWeapon(status, type);
        fireGuns          = status;
    } else if (vehicleFlags.bFireGun) {
        fireGuns = true;
        ordnance = 1;
        if (m_nModelIndex == MODEL_HYDRA && m_nVehicleWeaponInUse == CAR_WEAPON_LOCK_ON_ROCKET) {
            ordnance = 2;
        }
    }

    switch (m_nVehicleWeaponInUse) {
    case CAR_WEAPON_HEAVY_GUN: {
        if (fireGuns) {
            FirePlaneGuns();
        }
        break;
    }
    case CAR_WEAPON_FREEFALL_BOMB: {
        if (ordnance) {
            PossiblyDropFreeFallBombForPlayer(ordnance, true);
        }
        break;
    }
    case CAR_WEAPON_LOCK_ON_ROCKET: {
        if (ordnance) {
            if (!isControlledByPlayer) {
                switch (m_autoPilot.m_nCarMission) {
                case MISSION_PLANE_ATTACK_PLAYER:
                case MISSION_PLANE_ATTACK_PLAYER_POLICE:
                case MISSION_PLANE_DOG_FIGHT_PLAYER:
                    heatSeekingTarget = FindPlayerVehicle(-1, false);
                    break;
                default:
                    heatSeekingTarget = m_autoPilot.m_TargetEntity;
                    break;
                }
            }
            if (heatSeekingTarget) {
                FireHeatSeakingMissile(heatSeekingTarget, ordnance, true);
            }
        }
        break;
    }
    case CAR_WEAPON_DOUBLE_ROCKET: {
        if (ordnance) {
            FireUnguidedMissile(ordnance, true);
        }
        break;
    }
    }

    FxSystem_c** gunParticles;
    if (IsSubHeli()) {
        gunParticles = AsHeli()->m_ppGunflashFx;
    } else if (IsSubPlane()) {
        gunParticles = AsPlane()->m_pGunParticles;
    } else {
        return;
    }

    if (gunParticles) {
        const auto numGuns = GetPlaneNumGuns();
        for (auto i = 0; i < numGuns; i++) {
            if (const auto fx = gunParticles[i]) {
                fx->SetMatrix(GetRwObject() ? RwFrameGetMatrix(reinterpret_cast<RwFrame*>(rwObjectGetParent(GetRwObject()))) : nullptr);
            }
        }
    }
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
    x87d len = std::sqrt((x87d)fwd.y * fwd.y + (x87d)fwd.x * fwd.x);
    if (0.1f > len) {
        len = 0.1f;
    }
    const x87d invLen = (x87d)1.0 / len;

    m_nGunFiringTime = now;

    // Offset of the end point along the (2D-normalised) forward direction
    const float offX = (float)(invLen * fwd.x);
    const float offY = (float)(invLen * fwd.y);

    // Fires from one gun at `localPos`
    // NOTE: The offsets are `double`s because the first gun uses the unrounded products (in the FPU registers) while the second one uses their float copies
    const auto FireFromGun = [&](CVector localPos, double scaledOffX, double scaledOffY) {
        CVector start = TransformPointExt(*m_matrix, localPos); // 0x59C890
        CVector end{ (float)(X87R(scaledOffX + start.x)), (float)(X87R(start.y + scaledOffY)), start.z };

        const auto r1 = rand();
        const auto r2 = rand();
        const float jitterX = (float)(int32)((r2 & 0xFF) - 0x80) * 0.015f;
        const float jitterY = (float)(int32)((r1 & 0xFF) - 0x80) * 0.015f;
        const auto  r3 = rand();
        const x87d jitterZ = (x87d)(int32)((r3 & 0xFF) - 0x80) * 0.02f;

        end.x = jitterX + end.x;
        end.y = end.y + jitterY;
        end.z = (float)(end.z + jitterZ);

        CWeapon::DoTankDoomAiming(this, m_pDriver, &start, &end); // 0x73D1E0
        FireOneInstantHitRound(start, end, 15); // 0x73AF00
    };

    const float scaledOffX = (float)(offX * 60.f);
    const float scaledOffY = (float)(offY * 60.f);
    FireFromGun({  2.f, 2.5f, 1.f }, (x87d)offX * 60.f, (x87d)offY * 60.f);
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

// Initial values of the globals declared with NOTSA_GLOBAL_DECL in Vehicle.h (detached mode; ignored in address mode)
NOTSA_GLOBAL_DEF(CVehicle, m_aSpecialColVehicle, 0xC1CC08, (std::array<CVehicle*, 4>), {});
