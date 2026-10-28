/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include <bit> // popcnt
#include <numbers>

#include "Automobile.h"
#include "ModelIndices.h"
#include "WaterCannons.h"
#include "Buoyancy.h"
#include "Skidmarks.h"
#include "CarCtrl.h"
#include "Glass.h"
#include "ModelIndices.h"
#include "InterestingEvents.h"
#include "VehicleRecording.h"
#include "EventDanger.h"
#include "Weather.h"
#include "Game.h"
#include "Shadows.h"

#include <Tasks/TaskTypes/TaskSimpleGangDriveBy.h>

#include <FireManager.h>

constexpr size_t NUM_AUTOMOBILE_SUSP_LINES  = (size_t)MAX_CARWHEELS;
constexpr size_t NUM_RHINO_EXTRA_SUSP_LINES = (size_t)MAX_CARWHEELS * 2u; // 8
constexpr size_t NUM_RHINO_SUSP_LINES       = NUM_AUTOMOBILE_SUSP_LINES + NUM_RHINO_EXTRA_SUSP_LINES; // 8
constexpr size_t MAX_NUM_SUSP_LINES         = std::max(NUM_AUTOMOBILE_SUSP_LINES, NUM_RHINO_SUSP_LINES); // 12
NOTSA_GLOBAL(aAutomobileColPoints, 0xC1BFF8, (std::array<CColPoint, MAX_NUM_SUSP_LINES>), {});

static constexpr CVector PACKER_COL_PIVOT = CVector(0.0f, 0.0f, 2.0f);
static constexpr float CAR_BALANCE_MULT = 0.08f;

static constexpr CVector TANK_SHOT_DOOM_POS(0.0f, -1.394f, 2.296f);
static constexpr CVector TANK_SHOT_DOOM_DEFAULT_TARGET(0.0f, 2.95f, 2.97f);
static constexpr CVector TANK_SHOT_DOOM_DISTANCE_TO_DEFAULT_TARGET = TANK_SHOT_DOOM_DEFAULT_TARGET - TANK_SHOT_DOOM_POS;

static constexpr uint32 TIGER_GUNFIRE_RATE = 60;
static constexpr CVector TIGER_GUN_POS(0.0f, 0.5f, 0.2f); // 0xC1C208

void CAutomobile::InjectHooks()
{
    RH_ScopedVirtualClass(CAutomobile, 0x871120, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6B0A90);
    RH_ScopedInstall(Destructor, 0x6A61E0);

    RH_ScopedInstall(SetupModelNodes, 0x6A0770);
    RH_ScopedVMTInstall(SetModelIndex, 0x6A6590);
    RH_ScopedInstall(HydraulicControl, 0x6A07A0);
    RH_ScopedInstall(UpdateMovingCollision, 0x6A1460);
    RH_ScopedInstall(ProcessBuoyancy, 0x6A8C00);
    RH_ScopedInstall(ProcessHarvester, 0x6A9680);
    RH_ScopedInstall(TankControl, 0x6AE850);
    RH_ScopedInstall(DoSoftGroundResistance, 0x6A4AF0);
    RH_ScopedInstall(ProcessCarWheelPair, 0x6A4EC0);
    RH_ScopedInstall(PlaceOnRoadProperly, 0x6AF420);
    RH_ScopedInstall(RcbanditCheck1CarWheels, 0x6B3F70);
    RH_ScopedInstall(RcbanditCheckHitWheels, 0x6B45E0);
    RH_ScopedInstall(FireTruckControl, 0x729B60);
    RH_ScopedInstall(SetHeliOrientation, 0x6A2450);
    RH_ScopedInstall(ClearHeliOrientation, 0x6A2460);
    RH_ScopedInstall(ReduceHornCounter, 0x6A29A0);
    RH_ScopedInstall(FixTyre, 0x6A3580);
    RH_ScopedInstall(SetTaxiLight, 0x6A3740);
    RH_ScopedInstall(SetAllTaxiLights, 0x6A3760);
    RH_ScopedInstall(GetMovingCollisionOffset, 0x6A2150);
    RH_ScopedInstall(TellHeliToGoToCoors, 0x6A2390);
    RH_ScopedInstall(TellPlaneToGoToCoors, 0x6A2470);
    RH_ScopedInstall(SetTotalDamage, 0x6A27F0);
    RH_ScopedInstall(CustomCarPlate_BeforeRenderingStart, 0x6A2F00);
    RH_ScopedInstall(CustomCarPlate_AfterRenderingStop, 0x6A2F30);
    RH_ScopedInstall(GetAllWheelsOffGround, 0x6A2F70);
    RH_ScopedInstall(FixDoor, 0x6A35A0);
    RH_ScopedInstall(FixPanel, 0x6A3670);
    RH_ScopedInstall(PlayHornIfNecessary, 0x6A3820);
    RH_ScopedInstall(SetBusDoorTimer, 0x6A3860);
    RH_ScopedInstall(ProcessAutoBusDoors, 0x6A38A0);
    RH_ScopedInstall(BoostJumpControl, 0x6A3A60);
    RH_ScopedInstall(StopNitroEffect, 0x6A3E60);
    RH_ScopedInstall(NitrousControl, 0x6A3EA0);
    RH_ScopedInstall(KnockPedOutCar, 0x6A44C0);
    RH_ScopedInstall(PopBootUsingPhysics, 0x6A44D0);
    RH_ScopedInstall(CloseAllDoors, 0x6A4520);
    RH_ScopedInstall(GetCarRoll, 0x6A6010);
    RH_ScopedInstall(GetCarPitch, 0x6A6050);
    RH_ScopedInstall(IsInAir, 0x6A6140);
    RH_ScopedInstall(dmgDrawCarCollidingParticles, 0x6A6DC0);
    RH_ScopedInstall(SpawnFlyingComponent, 0x6A8580);
    RH_ScopedInstall(UpdateWheelMatrix, 0x6AA290);
    RH_ScopedInstall(ProcessSwingingDoor, 0x6A9D70);
    RH_ScopedInstall(RemoveBonnetInPedCollision, 0x6AA200);
    RH_ScopedInstall(PopDoor, 0x6ADEF0);
    RH_ScopedInstall(PopPanel, 0x6ADF80);
    RH_ScopedInstall(ScanForCrimes, 0x6ADFF0);
    RH_ScopedInstall(BlowUpCarsInPath, 0x6AF110);
    RH_ScopedInstall(PopBoot, 0x6AF910);
    RH_ScopedInstall(CloseBoot, 0x6AFA20);
    RH_ScopedInstall(SetBumperDamage, 0x6B1350);
    RH_ScopedInstall(SetPanelDamage, 0x6B1480);
    RH_ScopedInstall(SetDoorDamage, 0x6B1600);
    RH_ScopedInstall(TowTruckControl, 0x6A40F0);
    RH_ScopedInstall(ProcessCarOnFireAndExplode, 0x6A7090);
    RH_ScopedInstall(HasCarStoppedBecauseOfLight, 0x44D520);

    RH_ScopedVMTInstall(PreRender, 0x6AAB50);
    RH_ScopedVMTInstall(Render, 0x6A2B10);
    RH_ScopedVMTInstall(Fix, 0x6A3440);
    RH_ScopedVMTInstall(DoBurstAndSoftGroundRatios, 0x6A47F0);
    RH_ScopedVMTInstall(PlayCarHorn, 0x6A3770);
    RH_ScopedVMTInstall(VehicleDamage, 0x6A7650);
    RH_ScopedVMTInstall(GetComponentWorldPosition, 0x6A2210);
    RH_ScopedVMTInstall(IsComponentPresent, 0x6A2250);
    RH_ScopedVMTInstall(GetDooorAngleOpenRatio, 0x6A2270);
    RH_ScopedVMTInstall(GetDooorAngleOpenRatioU32, 0x6A62C0);
    RH_ScopedVMTInstall(IsDoorReady, 0x6A2290);
    RH_ScopedVMTInstall(IsDoorReadyU32, 0x6A6350);
    RH_ScopedVMTInstall(IsDoorFullyOpen, 0x6A22D0);
    RH_ScopedVMTInstall(IsDoorFullyOpenU32, 0x6A63E0);
    RH_ScopedVMTInstall(IsDoorClosed, 0x6A2310);
    RH_ScopedVMTInstall(IsDoorClosedU32, 0x6A6470);
    RH_ScopedVMTInstall(IsDoorMissing, 0x6A2330);
    RH_ScopedVMTInstall(IsDoorMissingU32, 0x6A6500);
    RH_ScopedVMTInstall(IsOpenTopCar, 0x6A2350);
    RH_ScopedVMTInstall(IsRoomForPedToLeaveCar, 0x6A3850);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6B3E90);
    RH_ScopedVMTInstall(GetHeightAboveRoad, 0x6A62B0);
    RH_ScopedVMTInstall(GetNumContactWheels, 0x6A62A0);
    RH_ScopedVMTInstall(Teleport, 0x6A9CA0);
    RH_ScopedVMTInstall(Save, 0x5D47E0);
    RH_ScopedVMTInstall(Load, 0x5D2980);
    RH_ScopedVMTInstall(ProcessControl, 0x6B1880);
    RH_ScopedVMTInstall(AddMovingCollisionSpeed, 0x6A1ED0);
    RH_ScopedVMTInstall(ProcessAI, 0x6B4800);
    RH_ScopedVMTInstall(ResetSuspension, 0x6A2AE0);
    RH_ScopedVMTInstall(ProcessFlyingCarStuff, 0x6A8500);
    RH_ScopedVMTInstall(DoHoverSuspensionRatios, 0x6A45C0);
    RH_ScopedVMTInstall(ProcessSuspension, 0x6AFB10);
    // todo: fix/review Tow funcs
    RH_ScopedVMTInstall(GetTowHitchPos, 0x6AF1D0);
    RH_ScopedVMTInstall(GetTowBarPos, 0x6AF250);
    RH_ScopedVMTInstall(SetTowLink, 0x6B4410);
    RH_ScopedVMTInstall(BreakTowLink, 0x6A4400);
    RH_ScopedVMTInstall(FindWheelWidth, 0x6A6090);

    RH_ScopedVMTInstall(SetupSuspensionLines, 0x6A65D0);
    RH_ScopedVMTInstall(ProcessEntityCollision, 0x6ACE70);
    RH_ScopedVMTInstall(ProcessControlCollisionCheck, 0x6A29C0);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6AD690);
    RH_ScopedVMTInstall(OpenDoor, 0x6A6AE0);
    RH_ScopedVMTInstall(BlowUpCar, 0x6B3780);
    RH_ScopedVMTInstall(BlowUpCarCutSceneNoExtras, 0x6B3BB0);
}

// 0x6A6590 (vtable slot 5; the constructor calls CVehicle::SetModelIndex + SetupModelNodes itself)
void CAutomobile::SetModelIndex(uint32 index) {
    CVehicle::SetModelIndex(index);
    std::ranges::fill(m_aCarNodes, nullptr); // rep stosd 0x19 dwords at +0x648
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aCarNodes.data());
}

// 0x6B0A90
CAutomobile::CAutomobile(int32 modelIndex, eVehicleCreatedBy createdBy, bool setupSuspensionLines) :
    CVehicle(createdBy),
    m_damageManager{ 0.5f }
{
    m_nVehicleType    = VEHICLE_TYPE_AUTOMOBILE;
    m_nVehicleSubType = VEHICLE_TYPE_AUTOMOBILE;

    m_fBurnTimer    = 0.0f;
    m_bDoingBurnout = false;

    CVehicle::SetModelIndex(modelIndex);
    SetupModelNodes();

    auto& mi = *GetVehicleModelInfo(); // has to be called after `SetModelIndex` has been called.

    m_pHandlingData = &mi.GetHandlingData(); // For now this has to be here, because the model index is not set in the member init list
    m_pFlyingHandlingData = &mi.GetFlyingHandlingData(); // Moved this over here
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;
    if (handlingFlags.bHydraulicGeom && CGeneral::GetRandomNumber() % 4 == 0) { // 0x6B0BD7
        AddVehicleUpgrade(ModelIndices::MI_HYDRAULICS);
    }

    m_fBrakeCount = 20.0f;
    m_GasPedalAudioRevs = 0.0f;
    m_fIntertiaValue1 = 0.0f;
    m_fIntertiaValue2 = 0.0f;

    // 0x6B0C2E
    mi.ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);

    // 0x6B0C49
    if (m_pHandlingData->m_bIsVan) {
        vehicleFlags.bIsVan = true;
    }
    if (m_pHandlingData->m_bIsBig) {
        vehicleFlags.bIsBig = true;
    }
    if (m_pHandlingData->m_bIsBus) {
        vehicleFlags.bIsBus = true;
    }
    if (m_pHandlingData->m_bIsLow) {
        vehicleFlags.bLowVehicle = true;
    }

    // 0x6B0CA8
    // Deal with front doors
    if (vehicleFlags.bIsBus) {
        m_doors[DOOR_LEFT_FRONT].Init(-HALF_PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_doors[DOOR_RIGHT_FRONT].Init(HALF_PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    } else {
        m_doors[DOOR_LEFT_FRONT].Init(-0.4f * PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_doors[DOOR_RIGHT_FRONT].Init(0.4f * PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    }

    // 0x6B0CF0
    // Deal with rear doors
    if (modelIndex == MODEL_RHINO) { // For Rhino just hide it
        for (auto door : { DOOR_LEFT_REAR, DOOR_RIGHT_REAR }) {
            // NOTE(pirulax): Also search for `Rhino` inside `CAutomobile::ProcessEntityCollision`
            m_doors[door].SetExtraWheelPositions(1.f, 1.f, 1.f, 1.f);
        }

        // Hide both
        for (auto comp : { CAR_WHEEL_LM, CAR_WHEEL_RM }) {
            rwObjectSetFlags(GetFirstObject(m_aCarNodes[comp]), eAtomicComponentFlag::ATOMIC_NONE);
        }
    } else if (vehicleFlags.bIsVan) { // 0x6B0D57
        m_doors[DOOR_LEFT_REAR].Init(-HALF_PI, 0.0f, DOOR_AXIS_X, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_doors[DOOR_RIGHT_REAR].Init(HALF_PI, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    } else {
        m_doors[DOOR_LEFT_REAR].Init(-0.4f * PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_doors[DOOR_RIGHT_REAR].Init(0.4f * PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    }

    // 0x6B0DBE
    // Bonnet
    if (m_pHandlingData->m_bReverseBonnet) {
        m_doors[DOOR_BONNET].Init(-0.3f * PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_X, DOOR_EXTRA_LOW_GRAVITY);
    } else {
        m_doors[DOOR_BONNET].Init(0.3f * PI, 0.0f, DOOR_AXIS_Y, DOOR_AXIS_X, DOOR_EXTRA_LOW_GRAVITY);
    }

    // 0x6B0DF4
    // Boot(y)
    if (m_pHandlingData->m_bHangingBoot) {
        m_doors[DOOR_BOOT].Init(-0.4f * PI, 0.0f, DOOR_AXIS_NEG_Z, DOOR_AXIS_X, DOOR_EXTRA_BASED);
    } else if (m_pHandlingData->m_bTailgateBoot) {
        m_doors[DOOR_BOOT].Init(0.5f * PI, 0.0f, DOOR_AXIS_Z, DOOR_AXIS_X, DOOR_EXTRA_BASED);
    } else {
        m_doors[DOOR_BOOT].Init(-0.3f * PI, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_X, DOOR_EXTRA_BASED);
    }

    // 0x6B0E4B
    // If model has no doors, hide them all
    if (m_pHandlingData->m_bNoDoors) {
        m_damageManager.SetDoorStatus({ DOOR_LEFT_FRONT, DOOR_RIGHT_FRONT, DOOR_LEFT_REAR, DOOR_RIGHT_REAR }, DAMSTATE_NOTPRESENT);
    }

    // Init this field which is never used
    for (auto& doorRot : DoorRotation) {
        doorRot = CGeneral::GetRandomNumberInRange(-0.15f, 0.15f);
    }

    // 0x6B0EE6
    // Do something with misc components? Not sure..
    if (m_nModelIndex == MODEL_TOWTRUCK && m_aCarNodes[CAR_MISC_B]) {
        if (auto& panelFL = m_panels[FRONT_LEFT_PANEL]; panelFL.m_nFrameId == (uint16)-1) {
            panelFL.SetPanel(CAR_MISC_B, 2, 1.f);
        }
    } else if (m_nModelIndex == MODEL_TRACTOR && m_aCarNodes[CAR_BOOT]) {
        if (auto& panelFL = m_panels[FRONT_LEFT_PANEL]; panelFL.m_nFrameId == (uint16)-1) {
            panelFL.SetPanel(CAR_BOOT, 1, 1.f);
        }
    }

    // 0x6B0F3B
    // Deal with swinging chassis
    if (m_pHandlingData->m_bSwingingChassis) {
        const auto GetAngleMult = [modelIndex] {
            switch (modelIndex) {
            case MODEL_COPCARVG:
            case MODEL_ESPERANT:
                return 0.03f;
            case MODEL_STRETCH:
                return 0.01f;
            }
            return 0.02f;
        };
        m_swingingChassis.Init(GetAngleMult() * PI, -GetAngleMult() * PI, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_CHASSIS | DOOR_EXTRA_FIXEDSTATE);
        m_swingingChassis.m_doorState = eDoorState::DOOR_HIT_MAX_END;
    } else if (modelIndex == MODEL_FIRELA) {
        m_swingingChassis.Init(0.1f * PI, -0.1f * PI, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_FIXEDSTATE | DOOR_EXTRA_FIRETRUCK);
        m_swingingChassis.m_doorState = eDoorState::DOOR_HIT_MAX_END;
    }

    m_moveForce = CVector();
    m_turnForce = CVector();

    // 0x6B1048
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fAirResistance = GetDefaultAirResistance();

    // 0x6B10B5
    m_fRotationBalance        = 0.0f;
    m_fAircraftGoToHeading    = 0.0f;
    m_nBusDoorTimerEnd        = 0;
    m_nBusDoorTimerStart      = 0;
    m_fSteerAngle             = 0.0f;
    m_GasPedal               = 0.0f;
    m_BrakePedal             = 0.0f;
    m_pExplosionVictim        = nullptr;
    m_GasPedalAudioRevs          = 0.0f;
    m_PrevSpeed          = 0.0f;
    m_wMiscComponentAngle     = 0;
    m_wMiscComponentAnglePrev = 0;

    // 0x6B1111
    rng::fill(m_WheelCounts, 0.0f);
    rng::fill(m_wheelRotation, 0.0f);
    rng::fill(m_wheelSpeed, 0.0f);
    rng::fill(m_fWheelBurnoutSpeed, 0.0f);
    rng::fill(m_wheelSkidmarkType, eSkidmarkType::DEFAULT);
    rng::fill(m_wheelSkidmarkBloodState, false);
    rng::fill(m_wheelSkidmarkMuddy, false);
    rng::fill(m_WheelStates, WHEEL_STATE_NORMAL);
    rng::fill(m_fWheelsSuspensionCompression, 1.0f);
    rng::fill(m_fWheelsSuspensionCompressionPrev, 1.0f);

    m_nNumContactWheels     = 0;
    m_NumDriveWheelsOnGround       = 0;
    m_NumDriveWheelsOnGroundLastFrame     = 0;
    m_fFrontHeightAboveRoad = 0.0f;
    m_fRearHeightAboveRoad  = 0.0f;
    m_fCarTraction          = 1.0f;
    m_fTireTemperature      = 1.0f;

    // 0x6B119A
    if (setupSuspensionLines) {
        CAutomobile::SetupSuspensionLines();
    }

    SetStatus(STATUS_SIMPLE);
    m_nNumPassengers = 0;

    if (m_nDoorLock == CARLOCK_UNLOCKED) {
        if (IsLawEnforcementVehicle()) {
            m_nDoorLock = CARLOCK_COP_CAR;
        }
    }

    m_fDoomVerticalRotation = 0.0f;
    m_fDoomHorizontalRotation = 0.05f;
    m_fPropRotate = 0.0f;
    m_fForcedOrientation = -1.0f;
    m_harvesterParticleCounter = 0;
    RightDoorOpenForDriveBys = 0.f;
    LeftDoorOpenForDriveBys = 0.f;

    switch (m_nModelIndex) {
    case -2: { // 0x6B120F
        // I don't think this is reachable, as it would crash somewhere above.

        rwObjectSetFlags(GetFirstObject(m_aCarNodes[CAR_WHEEL_LF]), eAtomicComponentFlag::ATOMIC_NONE);

        // Probably an inlined function here, as there was a redundant check here (model == -2)
        CMatrix wheelRFMat{ RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RF]) };
        CMatrix wheelLFMat{ RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LF]) };
        wheelRFMat.SetTranslate({ wheelLFMat.GetPosition().x + 0.1f, 0.f, wheelLFMat.GetPosition().z});
        wheelRFMat.UpdateRW();
        break;
    }
    case MODEL_RHINO: { // 0x6B12D6
        physicalFlags.bExplosionProof = true;
        physicalFlags.bBulletProof = true;
        break;
    }
    }

    std::ranges::fill(m_exhaustNitroFxSystem, nullptr);

    m_vehicleAudio.Initialise(this);
    m_heliDustFxTimeConst = 0.0f;
    m_fireParticleCounter = 0;
}

// 0x6A61E0
CAutomobile::~CAutomobile() {
    FxSystem_c::SafeKillAndClear(m_pFireParticle);
    StopNitroEffect();
    m_vehicleAudio.Terminate();
}

// 0x6B1880
void CAutomobile::ProcessControl()
{
    uint32 extraHandlingFlags = 0;
    if (vehicleFlags.bUseCarCheats) {
        extraHandlingFlags |= EXTRA_HANDLING_PERFECT;
    }
    CCollisionData* colData = GetColModel()->m_pColData;
    m_bDoingBurnout = false;
    autoFlags.bIsBoggedDownInSand = false;
    vehicleFlags.bWarnedPeds = false;
    vehicleFlags.bRestingOnPhysical = false;
    m_vehicleAudio.Service();
    bool bExplodeImmediately = false;
    if (IsSubPlane() || IsSubHeli()) {
        eCarMission carMission = m_autoPilot.m_nCarMission;
        if ((carMission == MISSION_PLANE_CRASH_AND_BURN
            || carMission == MISSION_HELI_CRASH_AND_BURN
            || GetStatus() == STATUS_PLAYER && m_fHealth < 250.0 && m_fireParticleCounter == 2)
            && GetStatus() != STATUS_WRECKED
            && (m_fDamageIntensity > 0.0f && m_vecLastCollisionImpactVelocity.z > 0.0f || !IsInAir())
            && (m_vecMoveSpeed.z >= 0.0f || physicalFlags.bSubmergedInWater))
        {
            bExplodeImmediately = true;
        }
    }

    if (CCheat::IsActive(CHEAT_ALL_TAXIS_NITRO)) {
        if (GetStatus() == STATUS_PLAYER && IsTransportVehicle())
            extraHandlingFlags |= EXTRA_HANDLING_TAXI_BOOST;
    }

    bool bExtraHandlingTaxiBoost = !!(extraHandlingFlags & EXTRA_HANDLING_TAXI_BOOST);
    if (CCheat::IsActive(CHEAT_ALL_CARS_HAVE_NITRO) || bExtraHandlingTaxiBoost) {
        handlingFlags.bNosInst = true;
        m_nNitroBoosts = 101;
    }
    if (vehicleFlags.bIsBus)
        ProcessAutoBusDoors();
    ProcessCarAlarm();
    UpdateClumpAlpha();
    UpdatePassengerList();
    if (m_pDriver) {
        if (!vehicleFlags.bDriverLastFrame && m_nBombOnBoard == BOMB_IGNITION_ACTIVATED) {
            m_wBombTimer = 1000;
            m_pWhoDetonatedMe = m_pWhoInstalledBombOnMe;
            CEntity::SafeRegisterRef(m_pWhoDetonatedMe);
        }
        vehicleFlags.bDriverLastFrame = true;
    }
    else {
        vehicleFlags.bDriverLastFrame = false;
    }

    if (m_nNumPassengers && IsUpsideDown())
        CanPedEnterCar();

    if (ProcessAI(extraHandlingFlags))
        return;

    bool extraPerfectHandling = !!(extraHandlingFlags & EXTRA_HANDLING_PERFECT);
    bool extraHandlingNitros = !!(extraHandlingFlags & EXTRA_HANDLING_NITROS);
    bool extraHandlingWheelsTouchingSand = !!(extraHandlingFlags & EXTRA_HANDLING_WHEELS_TOUCHING_SAND);
    bool extraHandlingTaxiBoost = !!(extraHandlingFlags & EXTRA_HANDLING_TAXI_BOOST);

    bool skipPhysics = false;
    if (GetStatus() == STATUS_ABANDONED || GetStatus() == STATUS_WRECKED) {
        bool isVehicleIdle = false;
        if (!vehicleFlags.bVehicleColProcessed
            && m_vecMoveSpeed == 0.0f
            && m_fWheelsSuspensionCompressionPrev[CAR_WHEEL_REAR_RIGHT] != 1.0f)
        {
            isVehicleIdle = true;
        }

        float forceLimitRadius = 0.003f;
        float torqueLimitRadius = 0.0009f;
        float movingSpeedLimit = 0.005f;
        if (IsSubPlane()) {
            if (m_fDamageIntensity > 0.0f && m_pDamageEntity && m_pDamageEntity->GetIsTypeVehicle())
                forceLimitRadius = 0.003f;
            else
                forceLimitRadius = 0.009f;
            torqueLimitRadius = 0.003f;
            movingSpeedLimit = 0.1f;
        }
        else if (GetStatus() == STATUS_WRECKED) {
            forceLimitRadius = 0.006f;
            torqueLimitRadius = 0.0015f;
            movingSpeedLimit = 0.015f;
        }

        m_vecForce  = (m_vecForce + m_vecMoveSpeed)  / 2.0f;
        m_vecTorque = (m_vecTorque + m_vecTurnSpeed) / 2.0f;

        bool resetSpeed = true;

        forceLimitRadius *= CTimer::GetTimeStep();
        torqueLimitRadius *= CTimer::GetTimeStep();
        if (forceLimitRadius * forceLimitRadius < m_vecForce.SquaredMagnitude()
            || sq(torqueLimitRadius) < m_vecTorque.SquaredMagnitude()
            || movingSpeedLimit <= m_fMovingSpeed
            || m_fDamageIntensity > 0.0f && m_pDamageEntity && m_pDamageEntity->GetIsTypePed())
        {
            resetSpeed = false;
        }
        else if (physicalFlags.bSubmergedInWater) {
            if (DistanceBetweenPointsSquared(TheCamera.GetPosition(), GetPosition()) < sq(50.0f)) {
                resetSpeed = false;
            }
        }
        if (!resetSpeed && !isVehicleIdle) {
            m_nFakePhysics = 0;
        }
        else {
            m_nFakePhysics++;
            if (m_nFakePhysics > 10 || isVehicleIdle) {
                if (!isVehicleIdle || m_nFakePhysics > 10)
                    m_nFakePhysics = 10;
                ResetMoveSpeed();
                ResetTurnSpeed();
                skipPhysics = true;
            }
        }

        if (IsRealHeli()
            && m_wheelSpeed[CAR_WHEEL_REAR_LEFT] > 0.0f || IsAmphibiousHeli()
            && !m_nNumContactWheels
            && m_fDamageIntensity <= 0.0f
            || m_nModelIndex == MODEL_VORTEX
            || CCheat::IsActive(CHEAT_CARS_ON_WATER)
            && IsAnyWheelTouchingShallowWaterGround())
        {
            skipPhysics = false;
            m_nFakePhysics = 0;
        }
    }

    for (auto& collisionEntity : m_apWheelCollisionEntity) {
        if (collisionEntity) {
            vehicleFlags.bRestingOnPhysical = true;
            if (!CWorld::bForceProcessControl && collisionEntity->GetIsInSafePosition()) {
                SetWasPostponed(true);
                return;
            }
        }
    }

    if (vehicleFlags.bRestingOnPhysical) {
        skipPhysics = false;
        m_nFakePhysics = 0;
    }

    VehicleDamage(0.0f, eVehicleCollisionComponent::DEFAULT, nullptr, nullptr, nullptr, WEAPON_RAMMEDBYCAR);
    if (GetStatus() == STATUS_PLAYER && ModelIndices::HasWaterCannon(m_nModelIndex)) {
        FireTruckControl(nullptr);
    }
    else {
        switch (m_nModelIndex)
        {
        case MODEL_RHINO:
        case MODEL_RCTIGER:
            TankControl();
            break;
        case MODEL_PACKER:
        case MODEL_DOZER:
        case MODEL_DUMPER:
        case MODEL_CEMENT:
        case MODEL_ANDROM:
        case MODEL_FORKLIFT:
            if (UpdateMovingCollision(-1.0f))
                skipPhysics = false;
            break;
        case MODEL_TOWTRUCK:
        case MODEL_TRACTOR:
            TowTruckControl();
            break;
        default:
            if (handlingFlags.bHydraulicInst)
                HydraulicControl();
            else if ((CCheat::IsActive(CHEAT_PERFECT_HANDLING) || (extraHandlingFlags & EXTRA_HANDLING_TAXI_BOOST))
                && GetStatus() == STATUS_PLAYER
                && m_vecMoveSpeed.SquaredMagnitude() > 0.04f)
            {
                BoostJumpControl();
            }
            break;
        }
    }
    if (handlingFlags.bNosInst) {
        NitrousControl(0);
    } else {
        StopNitroEffect();
    }

    if (FindPlayerVehicle() == this && CPad::GetPad()->CarGunJustDown())
        ActivateBomb();

    if (skipPhysics) {
        SkipPhysics();
        vehicleFlags.bAudioChangingGear = false;
        vehicleFlags.bVehicleColProcessed = false;
        m_fTireTemperature = 1.0f;
    }
    else {
        if (!vehicleFlags.bVehicleColProcessed) {
            ProcessControlCollisionCheck(true);
        }
        bool oldSubmergedInWater = physicalFlags.bSubmergedInWater;
        bool postCollision = false;
        if (m_fDamageIntensity > 0.0f && m_vecLastCollisionImpactVelocity.z > 0.1f)
            postCollision = true;
        const bool hasHitWall = GetHasHitWall(); // 0x6B2131-0x6B214B: read before CPhysical::ProcessControl, which clears it (the port read it after, always false)
        CPhysical::ProcessControl();

        if (m_nModelIndex == MODEL_SKIMMER) {
            auto skimmerModelInfo = CModelInfo::GetModelInfo(MODEL_SKIMMER)->AsVehicleModelInfoPtr();
            auto skimmerHandling = gHandlingDataMgr.GetBoatPointer(static_cast<uint8>(skimmerModelInfo->m_nHandlingId));
            ProcessBoatControl(skimmerHandling, &m_fDoomVerticalRotation, hasHitWall, postCollision);
            physicalFlags.bTouchingWater = physicalFlags.bSubmergedInWater;
        }
        else {
            ProcessBuoyancy();
        }

        if (!oldSubmergedInWater && physicalFlags.bSubmergedInWater) {
            if (m_pDriver && m_pDriver->IsPlayer()) {
                m_pDriver->AsPlayer()->ResetPlayerBreath();
            }
            else {
                for (const auto passenger : GetPassengers()) {
                    if (passenger && passenger->IsPlayer()) {
                        passenger->AsPlayer()->ResetPlayerBreath();
                    }
                }
            }
        }
        if (m_nModelIndex == MODEL_RCBARON)
            ProcessFlyingCarStuff();
        if (!autoFlags.bIsMonsterTruck) {
            for (int32 i = 0; i < 4; i++) {
                float wheelRadius = 1.0f - m_aSuspensionSpringLength[i] / m_aSuspensionLineLength[i];
                m_fWheelsSuspensionCompression[i] = (m_fWheelsSuspensionCompression[i] - wheelRadius) / (1.0f - wheelRadius);
            }
        }

        ProcessSuspension();

        CVector contactPoints[4]{};
        CVector contactSpeeds[4]{};

        for (int32 i = 0; i < 4; i++) {
            if (m_fWheelsSuspensionCompression[i] >= 1.0f) {
                CColLine& colLine = colData->m_pLines[i];
                CColDisk& colDisk = colData->m_pDisks[i];
                if (colData->bUsesDisks) {
                    CVector& point = contactPoints[i];
                    point = colDisk.m_vecCenter;
                    point.z -= colDisk.m_fRadius;
                    point = GetMatrix().TransformVector(contactPoints[i]);
                }
                else {
                    contactPoints[i] = GetMatrix().TransformVector(colLine.m_vecEnd);
                }
            }
            else {
                m_wheelSkidmarkMuddy[i] = false;
                m_wheelSkidmarkType[i] = static_cast<eSkidmarkType>(g_surfaceInfos.GetSkidmarkType(m_wheelColPoint[i].m_nSurfaceTypeB));
                if (m_wheelSkidmarkType[i] == eSkidmarkType::MUDDY)
                    m_wheelSkidmarkMuddy[i] = true;
                contactPoints[i] = m_wheelColPoint[i].m_vecPoint - GetPosition();
            }
        }

        for (int32 i = 0; i < 4; i++) {
            CPhysical* collisionEntity = m_apWheelCollisionEntity[i];
            contactSpeeds[i] = GetSpeed(contactPoints[i]);
            if (collisionEntity) {
                contactSpeeds[i] -= collisionEntity->GetSpeed(m_vWheelCollisionPos[i]);
                m_apWheelCollisionEntity[i] = nullptr;
            }
        }

        // 0x6B24A9-0x6B24CC: the forward speed is summed z, y, x
        float speedForward = m_vecMoveSpeed.z * GetForward().z + m_vecMoveSpeed.y * GetForward().y + m_vecMoveSpeed.x * GetForward().x;

        if (vehicleFlags.bAudioChangingGear
            && m_GasPedal > 0.4f
            && m_BrakePedal < 0.1f
            && speedForward  > 0.15f
            && this == FindPlayerVehicle()
            && CCamera::GetActiveCamera().m_nMode != MODE_1STPERSON)
        {
            if (GetStatus() == STATUS_PLAYER && !m_pHandlingData->m_bIsBus)
            {
                if (m_nBusDoorTimerEnd) {
                    uint32 timeStep = NOTSA_FTOL(uint32, CTimer::GetTimeStepInMS());
                    if (m_nBusDoorTimerEnd <= timeStep)
                        m_nBusDoorTimerEnd = 0;
                    else
                        m_nBusDoorTimerEnd -= timeStep;
                }
                else {
                    m_nBusDoorTimerEnd = 1000;
                }
            }

            if (IsAnyFrontAndRearWheelTouchingGround()) {
                CVector point = GetForward() * -1.0f;
                CVector force = std::min(m_fTurnMass, 2500.0f) * -0.008f * GetUp();
                ApplyTurnForce(force, point);
            }
        }

        float brake = m_pHandlingData->m_fBrakeDeceleration * m_BrakePedal * CTimer::GetTimeStep();

        m_NumDriveWheelsOnGroundLastFrame = m_NumDriveWheelsOnGround;
        m_nNumContactWheels = 0;
        m_NumDriveWheelsOnGround = 0;

        for (int32 i = 0; i < 4; i++) {
            if (m_fWheelsSuspensionCompression[i] >= 1.0f) {
                m_WheelCounts[i] = std::max(m_WheelCounts[i] - CTimer::GetTimeStep(), 0.0f);
                if (m_WheelCounts[i] <= 0.0f)
                    continue;
            }
            else {
                m_WheelCounts[i] = 4.0f;
            }

            m_nNumContactWheels++;

            switch (m_pHandlingData->GetTransmission().m_nDriveType)
            {
            case '4':
                m_NumDriveWheelsOnGround++;
                break;
            case 'F':
                if (i == CAR_WHEEL_FRONT_LEFT || i == CAR_WHEEL_FRONT_RIGHT) {
                    m_NumDriveWheelsOnGround++;
                    break;
                }
                break;
            case 'R':
                if (i == CAR_WHEEL_REAR_LEFT || i == CAR_WHEEL_REAR_RIGHT) {
                    m_NumDriveWheelsOnGround++;
                    break;
                }
            }
        }

        uint8 cheatType = CHEAT_HANDLING_NONE;
        if (handlingFlags.bNosInst && m_fTireTemperature < 0.0f)
            cheatType = CHEAT_HANDLING_NITROS;
        else if (extraPerfectHandling || CCheat::IsActive(CHEAT_PERFECT_HANDLING))
            cheatType = CHEAT_HANDLING_PERFECT;

        float acceleration = 0.0f;
        if (vehicleFlags.bEngineOn && !m_pHandlingData->m_bIsPlane && !m_pHandlingData->m_bIsHeli) {
            acceleration = m_pHandlingData->GetTransmission().CalculateDriveAcceleration(
                m_GasPedal,
                m_nCurrentGear,
                m_fGearChangeCount,
                speedForward,
                &m_fIntertiaValue1,
                &m_fIntertiaValue2,
                m_NumDriveWheelsOnGround,
                cheatType
            );
            acceleration /= m_fVelocityFrequency;
        }

        // 0x6B2803-0x6B2845: K * mult (physics cars: (carTraction * mult) * K), then * 0.25 and / velocity frequency.
        // K is 0xC1BFEC, set at startup by the static init at 0x853500: float(1/(50*50)) * 10 = 0x3B83126E (not 0.004f = 0x3B83126F)
        constexpr float TRACTION_K = (1.0f / (50.0f * 50.0f)) * 10.0f;
        static_assert(std::bit_cast<uint32>(TRACTION_K) == 0x3B83126Eu);
        float traction = TRACTION_K * m_pHandlingData->m_fTractionMultiplier;
        if (GetStatus() == STATUS_PHYSICS)
            traction = m_fCarTraction * m_pHandlingData->m_fTractionMultiplier * TRACTION_K;
        traction *= 0.25f;
        traction /= m_fVelocityFrequency;
        CPlane* vortex = AsPlane();
        if (CCheat::IsActive(CHEAT_PERFECT_HANDLING) || m_nModelIndex == MODEL_VORTEX && vortex->m_fAccelerationBreakStatus == 0.0f)
            traction *= 4.0f;

        if (this != FindPlayerVehicle() && (extraPerfectHandling || CCheat::IsActive(CHEAT_PERFECT_HANDLING))) {
            traction *= 1.2f;
            acceleration *= 1.4f;
            if (extraHandlingNitros || CCheat::IsActive(CHEAT_PERFECT_HANDLING)) {
                traction *= 1.3f;
                acceleration *= 1.4f;
            }
        }

        float steerAngle = 1.0f; // todo: Repeated branch in conditional chain
        if (speedForward <= 0.01f || m_WheelCounts[CAR_WHEEL_FRONT_LEFT] <= 0.0f && m_WheelCounts[CAR_WHEEL_REAR_LEFT] <= 0.0f)
            steerAngle = 1.0f;
        else if (GetStatus() != STATUS_PLAYER)
            steerAngle = 1.0f;
        else {
            CColPoint colPoint{};
            colPoint.m_nSurfaceTypeA = SURFACE_WHEELBASE;
            colPoint.m_nSurfaceTypeB = SURFACE_TARMAC;
            const float speedRight = m_vecMoveSpeed.z * GetRight().z + m_vecMoveSpeed.y * GetRight().y + m_vecMoveSpeed.x * GetRight().x; // 0x6B2935-0x6B295D: z, y, x
            float adhesive = g_surfaceInfos.GetAdhesiveLimit(&colPoint);
            steerAngle = adhesive * traction * 4.0f * 4.0f / (speedForward * speedForward);
            steerAngle = std::min(steerAngle, 1.0f);
            steerAngle = X87R(x87::asin(steerAngle) / DegreesToRadians(m_pHandlingData->m_fSteeringLock));
            if (m_fSteerAngle < 0.0f && speedRight > 0.05f
                || m_fSteerAngle > 0.0f && speedRight < -0.05f
                || vehicleFlags.bIsHandbrakeOn
                || steerAngle > 1.0f)
            {
                steerAngle = 1.0f;
            }
        }
        m_fSteerAngle *= steerAngle;

        if (!handlingFlags.bProcRearwheelFirst) {
            if (handlingFlags.bSteerRearwheels)
                CAutomobile::ProcessCarWheelPair(
                    CAR_WHEEL_FRONT_LEFT, CAR_WHEEL_FRONT_RIGHT,
                    -999.0f,
                    contactSpeeds,
                    contactPoints,
                    traction,
                    acceleration,
                    brake,
                    true
                );
            else
                CAutomobile::ProcessCarWheelPair(
                    CAR_WHEEL_FRONT_LEFT, CAR_WHEEL_FRONT_RIGHT,
                    m_fSteerAngle,
                    contactSpeeds,
                    contactPoints,
                    traction,
                    acceleration,
                    brake,
                    true
                );
        }

        float rearWheelsSteerAngle = -999.0f;
        if (handlingFlags.bSteerRearwheels)
            rearWheelsSteerAngle = -m_fSteerAngle;
        else if (handlingFlags.bHbRearwheelSteer)
            rearWheelsSteerAngle = m_f2ndSteerAngle;

        CAutomobile::ProcessCarWheelPair(CAR_WHEEL_REAR_LEFT, CAR_WHEEL_REAR_RIGHT,
            rearWheelsSteerAngle,
            contactSpeeds,
            contactPoints,
            traction,
            acceleration,
            brake,
            false
        );

        if (handlingFlags.bProcRearwheelFirst) {
            if (handlingFlags.bSteerRearwheels)
                CAutomobile::ProcessCarWheelPair(
                    CAR_WHEEL_FRONT_LEFT, CAR_WHEEL_FRONT_RIGHT,
                    -999.0f,
                    contactSpeeds,
                    contactPoints,
                    traction,
                    acceleration,
                    brake,
                    true
                );
            else
                CAutomobile::ProcessCarWheelPair(
                    CAR_WHEEL_FRONT_LEFT, CAR_WHEEL_FRONT_RIGHT,
                    m_fSteerAngle,
                    contactSpeeds,
                    contactPoints,
                    traction,
                    acceleration,
                    brake,
                    true
                );
        }

        if (GetStatus() != STATUS_PLAYER) {
            if (CanUpdateHornCounter())
                ReduceHornCounter();
        }
        else if (handlingFlags.bHydraulicInst || CCheat::IsActive(CHEAT_PERFECT_HANDLING) || extraHandlingTaxiBoost)
            ProcessSirenAndHorn(false);
        else
            ProcessSirenAndHorn(true);

        if (m_nModelIndex != MODEL_RCBARON)
            ProcessFlyingCarStuff();
    }
    ProcessCarOnFireAndExplode(bExplodeImmediately);
    if (vehicleFlags.bSirenOrAlarm
        && (CTimer::GetFrameCounter() & 7) == 5
        && UsesSiren()
        && m_nModelIndex != MODEL_MRWHOOP
        && FindPlayerVehicle() == this)
    {
        CCarAI::MakeWayForCarWithSiren(this);
    }

    float suspensionShake = 0.0f;
    float roughnessShake = 0.0f;
    float wheelSpinRate = 0.0f;
    for (int32 i = 0; i < 4; i++) {
        float suspensionChange = m_fWheelsSuspensionCompressionPrev[i] - m_fWheelsSuspensionCompression[i];
        if (suspensionChange > 0.3f && !extraHandlingWheelsTouchingSand && m_vecMoveSpeed.SquaredMagnitude() > 0.04f) {
            //m_damageManager.GetWheelStatus(i); // return value is ignored
            if (suspensionChange > suspensionShake)
                suspensionShake = suspensionChange;
        }

        if (m_fWheelsSuspensionCompression[i] < 1.0f && GetStatus() == STATUS_PLAYER) {
            float roughness = (float)g_surfaceInfos.GetRoughness(m_wheelColPoint[i].m_nSurfaceTypeB) * 0.1f;
            roughnessShake = std::max(roughnessShake, roughness);
        }
        uint32 handlingId = m_pHandlingData->m_nVehicleId;
        cTransmission& transmission = gHandlingDataMgr.GetVehiclePointer(handlingId)->GetTransmission();
        if (m_WheelCounts[i] <= 0.0f
            && (m_GasPedal > 0.5f || m_GasPedal < -0.5f)
            && ((i == CAR_WHEEL_FRONT_LEFT || i == CAR_WHEEL_FRONT_RIGHT) && transmission.m_nDriveType != 'R'
                || (i == CAR_WHEEL_REAR_LEFT || i == CAR_WHEEL_REAR_RIGHT) && transmission.m_nDriveType != 'F'))
        {
            wheelSpinRate += CVehicle::WHEELSPIN_INAIR_TARGET_RATE;
        }
        else if (m_WheelStates[i] == WHEEL_STATE_SPINNING) {
            wheelSpinRate += CVehicle::WHEELSPIN_TARGET_RATE;
        }

        m_fWheelsSuspensionCompressionPrev[i] = m_fWheelsSuspensionCompression[i];
        m_fWheelsSuspensionCompression[i] = 1.0f;
    }

    if (m_pHandlingData->GetTransmission().m_nDriveType == '4')
        wheelSpinRate *= 0.25f;
    else
        wheelSpinRate *= 0.5f;

    float riseFallWheelSpinRate = CVehicle::WHEELSPIN_FALL_RATE;
    if (wheelSpinRate >= m_fWheelSpinForAudio)
        riseFallWheelSpinRate = CVehicle::WHEELSPIN_RISE_RATE;

    riseFallWheelSpinRate = std::pow(riseFallWheelSpinRate, CTimer::GetTimeStep());
    m_fWheelSpinForAudio *= riseFallWheelSpinRate;
    m_fWheelSpinForAudio += (1.0f - riseFallWheelSpinRate) * wheelSpinRate;

    if ((CTimer::GetTimeInMS() & 2047) > 800) { // % 2046
        if (roughnessShake >= 0.29f)
            suspensionShake = 0.0f;
        roughnessShake = 0.0f;
    }

    if (!extraHandlingWheelsTouchingSand
        && (suspensionShake > 0.0f || roughnessShake > 0.0f)
        && GetStatus() == STATUS_PLAYER)
    {
        float speed = m_vecMoveSpeed.SquaredMagnitude();
        if (speed > 0.01f) {
            speed = sqrt(speed);
            if (suspensionShake > 0.0f) {
                // 0x6B2E94-0x6B2F12: ((speed / mass) * shake) * 400000 + 100; the shake time divides by the truncated byte
                float frequency = speed / m_fMass * suspensionShake * 400000.0f + 100.0f;
                frequency = std::min(frequency, 250.0f);
                const auto frequencyByte = NOTSA_FTOL(uint8, frequency);
                auto time = NOTSA_FTOL(int16, CTimer::GetTimeStep() * 20000.0f / static_cast<float>(frequencyByte));
                CPad::GetPad()->StartShake(time, frequencyByte, 0);
            }
            else {
                // 0x6B2F14-0x6B2F79: same shape with the roughness shake, + 40, cap 150
                float frequency = speed / m_fMass * roughnessShake * 400000.0f + 40.0f;
                frequency = std::min(frequency, 150.0f);
                const auto frequencyByte = NOTSA_FTOL(uint8, frequency);
                auto time = NOTSA_FTOL(int16, CTimer::GetTimeStep() * 5000.0f / static_cast<float>(frequencyByte));
                CPad::GetPad()->StartShake(time, frequencyByte, 0);
            }
        }
    }

    vehicleFlags.bAudioChangingGear = false;
    vehicleFlags.bVehicleColProcessed = false;

    // 0x6B2F94
    if (!vehicleFlags.bWarnedPeds)
        CCarCtrl::ScanForPedDanger(this);

    if (handlingFlags.bHydraulicInst && m_vecMoveSpeed.Magnitude() < 0.2f) {
        if ([this] {
            switch (GetStatus()) {
            case STATUS_PHYSICS: {
                const auto CheckWheelIsUsingHydraulics = [this](eCarWheel wheel) {
                    return m_vehicleSpecialColIndex != -1 && m_aSpecialHydraulicData[m_vehicleSpecialColIndex].m_aWheelSuspension[wheel] > 0.5f;
                };
                return (CheckWheelIsUsingHydraulics(CAR_WHEEL_FRONT_LEFT) && CheckWheelIsUsingHydraulics(CAR_WHEEL_REAR_LEFT))
                    || (CheckWheelIsUsingHydraulics(CAR_WHEEL_FRONT_RIGHT) && CheckWheelIsUsingHydraulics(CAR_WHEEL_REAR_RIGHT));
            }
            case STATUS_PLAYER: {
                return IsDriverAPlayer()
                    && std::fabs((float)m_pDriver->AsPlayer()->GetPadFromPlayer()->GetCarGunLeftRight()) > 50.0f
                    && std::fabs((float)m_pDriver->AsPlayer()->GetPadFromPlayer()->GetCarGunUpDown()) < 50.0f;
            }
            }
            return false;
        }()) {
            float       turnSpeedForward = m_vecTurnSpeed.z * GetForward().z + m_vecTurnSpeed.y * GetForward().y + m_vecTurnSpeed.x * GetForward().x; // 0x6B30C6-0x6B30DC: z, y, x
            const float speedSquared     = turnSpeedForward * turnSpeedForward;
            float       speedTimeStep    = std::pow(0.985f, CTimer::GetTimeStep()) / (speedSquared * 5.0f + 1.0f);
            float       speed            = std::pow(speedTimeStep, CTimer::GetTimeStep());
            speed *= turnSpeedForward;
            speed -= turnSpeedForward;

            CVector force = speed * GetUp() * -1.0f * m_fTurnMass;
            CVector point = GetRight() + GetMatrix().TransformVector(m_vecCentreOfMass);
            ApplyTurnForce(force, point);
        }
    }

    if (m_pTowingVehicle) {
        if (GetStatus() == STATUS_IS_TOWED) {
            bool updateTractorLink = false; // TODO: rename this variable later
            if (m_pTowingVehicle->m_vecMoveSpeed != 0.0f || m_vecMoveSpeed.SquaredMagnitude() > 0.01f) {
                updateTractorLink = true;
                m_pTowingVehicle->UpdateTractorLink(false, false);
            }
            CVehicle::UpdateTrailerLink(false, false);
            if (m_pTowingVehicle && GetStatus() == STATUS_IS_TOWED) {
                if (updateTractorLink)
                    m_pTowingVehicle->UpdateTractorLink(false, true);
                UpdateTrailerLink(false, true);
            }
        }
        else {
            BreakTowLink();
        }
    }
    else if (m_pVehicleBeingTowed) {
        if (m_pVehicleBeingTowed->GetStatus() == STATUS_IS_TOWED) {
            if (m_pVehicleBeingTowed->m_pTowingVehicle == this) {
                RemoveFromMovingList();
                AddToMovingList();
            }
        }
        else {
            CEntity::ClearReference(m_pVehicleBeingTowed);
        }
    }

    // 0x6B32EF
    if (physicalFlags.bDisableCollisionForce && physicalFlags.bCollidable) {
        ResetMoveSpeed();
        ResetTurnSpeed();
        ResetFrictionMoveSpeed();
        ResetFrictionTurnSpeed();
    }
    else if (!skipPhysics
        && (m_GasPedal == 0.0f || GetStatus() == STATUS_WRECKED)
        && std::fabs(m_vecMoveSpeed.x) < 0.005f // 0x6B33F6-0x6B342D: limit is 0x858B4C = 0.005 (the port had 0.0045)
        && std::fabs(m_vecMoveSpeed.y) < 0.005f
        && std::fabs(m_vecMoveSpeed.z) < 0.005f)
    {
        if (m_fDamageIntensity <= 0.0f || m_pDamageEntity != FindPlayerPed()) {
            if ((!IsSubPlane() || AsPlane()->m_fAccelerationBreakStatus == 0.0f) &&
                (!IsSubHeli() || AsHeli()->m_fAccelerationBreakStatus == 0.0f))
            {
                if (((float)m_wMiscComponentAngle == 0.0f || m_wMiscComponentAngle == m_wMiscComponentAnglePrev)
                    && !physicalFlags.bSubmergedInWater)
                {
                    if ((!m_pTowingVehicle || m_pTowingVehicle->m_vecMoveSpeed == 0.0f) && DidAnyWheelTouchGroundPrev()) {
                        if (m_nModelIndex == MODEL_VORTEX || CCheat::IsActive(CHEAT_CARS_ON_WATER)
                            && DidAnyWheelTouchShallowWaterGroundPrev())
                        {
                            m_nFakePhysics = 0;
                        }
                        else {
                            ResetMoveSpeed();
                            m_vecTurnSpeed.z = 0.0f;
                        }
                    }
                }
            }
        }
    }

    if (CWeather::Earthquake > 0.0f) {
        CVector& position = GetPosition();
        position.z += CGeneral::GetRandomNumberInRange(0.0f, CWeather::Earthquake * 0.5f); // 0x6B3695: the random shake is added to z (the port replaced z)
    }

    if (m_nModelIndex == MODEL_COMBINE)
        ProcessHarvester();

    if (m_nModelIndex == MODEL_RHINO && (m_vecMoveSpeed != 0.0f || m_vecTurnSpeed != 0.0f)) {
        m_doors[DOOR_LEFT_REAR].SetExtraWheelPositions(1.0f, 1.0f, 1.0f, 1.0f);
        m_doors[DOOR_RIGHT_REAR].SetExtraWheelPositions(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

// 0x6A1ED0
CVector CAutomobile::AddMovingCollisionSpeed(CVector& point) {
    if (GetStatus() != STATUS_PLAYER && GetStatus() != STATUS_FORCED_STOP) {
        if (GetCreatedBy() != MISSION_VEHICLE || !m_wMiscComponentAngle && !m_wMiscComponentAnglePrev)
            return {};
    }

    const auto angleDiff = (int32)m_wMiscComponentAngle - (int32)m_wMiscComponentAnglePrev;
    if (angleDiff > 100 || angleDiff < -100)
        return {};

    float colAngleMult = 0.0f;
    CVector colPivot{};
    RwFrame* carNodeMisc = nullptr;
    if (ModelIndices::IsDumper(m_nModelIndex))
    {
        carNodeMisc = m_aCarNodes[CAR_MISC_C];
        colAngleMult = CMonsterTruck::DUMPER_COL_ANGLEMULT;
    }
    else if (ModelIndices::IsPacker(m_nModelIndex)) {
        colAngleMult = PACKER_COL_ANGLE_MULT;
        colPivot = PACKER_COL_PIVOT;
    }
    else if (ModelIndices::IsDozer(m_nModelIndex)) {
        carNodeMisc = m_aCarNodes[CAR_MISC_A];
        if (carNodeMisc) {
            colAngleMult = DOZER_COL_ANGLE_MULT;
        }
    }
    else if (ModelIndices::IsAndromada(m_nModelIndex)) {
        carNodeMisc = m_aCarNodes[CAR_MISC_E];
        if (carNodeMisc) {
            colAngleMult = CPlane::ANDROM_COL_ANGLE_MULT;
        }
    }
    else if (ModelIndices::IsForklift(m_nModelIndex)) {
        float rot = float(angleDiff) / CTimer::GetTimeStep() * FORKLIFT_COL_ANGLE_MULT * 2.0f;
        return rot * GetUp();
    }

    if (colAngleMult == 0.0f)
        return {};

    if (carNodeMisc) {
        colPivot = RwFrameGetMatrix(carNodeMisc)->pos;
    }
    CVector rotation(float(angleDiff) / CTimer::GetTimeStep() * colAngleMult, 0.0f, 0.0f);
    CVector pos = GetMatrix().TransformVector(rotation);
    CVector pivotPos = GetMatrix().TransformVector(colPivot);
    CVector distance = point - pivotPos;
    return CrossProduct(pos, distance);
}

// 0x6B4800
bool CAutomobile::ProcessAI(uint32& extraHandlingFlags) {
    CColModel* colModel = GetColModel();
    CCollisionData* colData = colModel->m_pColData;
    int8 recordingId = this->m_autoPilot.m_vehicleRecordingId;
    m_autoPilot.carCtrlFlags.bHonkAtCar = false;
    m_autoPilot.carCtrlFlags.bHonkAtPed = false;
    if (recordingId >= 0 && !CVehicleRecording::bUseCarAI[recordingId])
        return false;

    eCarMission carMission = m_autoPilot.m_nCarMission;
    CVehicle* playerVehicle = FindPlayerVehicle();
    if (playerVehicle && playerVehicle != this && FindPlayerWanted()->GetWantedLevel() > eWantedLevel::WANTED_LEVEL_3
        && (carMission == MISSION_RAMPLAYER_FARAWAY
            || carMission == MISSION_BLOCKPLAYER_FARAWAY
            || carMission == MISSION_RAMPLAYER_CLOSE
            || carMission == MISSION_BLOCKPLAYER_CLOSE)
        && FindPlayerSpeed().Magnitude() > 0.3f
    ) {
        extraHandlingFlags |= EXTRA_HANDLING_PERFECT;
        if (FindPlayerSpeed().Magnitude() > 0.4f && m_vecMoveSpeed.Magnitude() < 0.3f) {
            extraHandlingFlags |= EXTRA_HANDLING_NITROS;
        } else {
            if (DistanceBetweenPoints(FindPlayerCoors(), GetPosition()) > 50.0f)
                extraHandlingFlags |= EXTRA_HANDLING_NITROS;
        }
    } else if (m_nModelIndex == MODEL_RCBANDIT && GetStatus() != STATUS_REMOTE_CONTROLLED) {
        extraHandlingFlags |= EXTRA_HANDLING_PERFECT;
    }

    bool extraPerfectHandling = !!(extraHandlingFlags & EXTRA_HANDLING_PERFECT);
    if (extraPerfectHandling || CCheat::IsActive(CHEAT_PERFECT_HANDLING)) {
        m_vecCentreOfMass.z = m_aSuspensionSpringLength[CAR_WHEEL_FRONT_LEFT] * 0.3f - m_fFrontHeightAboveRoad;
    }
    else if (GetStatus() == STATUS_PHYSICS) {
        if (handlingFlags.bHydraulicGeom) {
            if (m_autoPilot.m_nCarMission != MISSION_NONE && colData && colData->m_nNumLines > 0) {
                m_vecCentreOfMass.y = (colData->m_pLines[0].m_vecStart.y + colData->m_pLines[1].m_vecStart.y)  * 0.5f;
                handlingFlags.bNpcNeutralHandl = true;
            }
            else {
                m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
                handlingFlags.bNpcNeutralHandl = false;
            }
        }

        if (handlingFlags.bNpcAntiRoll) {
            m_vecCentreOfMass.z = (colModel->m_boundBox.m_vecMin.z - m_pHandlingData->m_vecCentreOfMass.z) * 0.4f + m_pHandlingData->m_vecCentreOfMass.z; // 0x6B4A1B-0x6B4A65: stored into the car's own centre of mass (the port edited the shared handling data every frame)
        }
    }
    else {
        m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    }

    if (GetStatus() != STATUS_ABANDONED && GetStatus() != STATUS_WRECKED
        && GetStatus() != STATUS_PLAYER && GetStatus() != STATUS_REMOTE_CONTROLLED
        && GetStatus() != STATUS_FORCED_STOP && vehicleFlags.bIsLawEnforcer)
    {
        ScanForCrimes();
    }

    if (vehicleFlags.bCanPark
        && !vehicleFlags.bParking
        && GetCreatedBy() != MISSION_VEHICLE
        && m_autoPilot.m_nCarMission == MISSION_CRUISE
        && ((CTimer::GetFrameCounter() + static_cast<uint8>(m_nRandomSeed)) & 0xF) == 0)
    {
        if (m_nModelIndex != MODEL_TAXI && m_nModelIndex != MODEL_CABBIE) {
            CVector target = GetPosition();
            target = (target + GetForward() * 10.0f) + GetRight() * 3.0f; // 0x6B4B0B-0x6B4B3C: (pos + forward * 10) + right * 3
            CColPoint cp{};
            CEntity* hit;
            if (!CWorld::ProcessLineOfSight(GetPosition(), target, cp, hit, true, true, true, false, false, false, false, false) || hit == this) {
                CCarAI::GetCarToParkAtCoors(this, target);
            }
        }
    }

    bool isRemotelyControlledByPlayer = false;
    switch (GetStatus()) {
    case STATUS_SIMPLE:
    {
        CCarAI::UpdateCarAI(this);
        CPhysical::ProcessControl();
        CCarCtrl::UpdateCarOnRails(this);
        m_nNumContactWheels = 4;
        m_NumDriveWheelsOnGroundLastFrame = m_NumDriveWheelsOnGround;
        m_NumDriveWheelsOnGround = 4;
        float speed = m_autoPilot.m_speed * ExeRecip(50.0f);
        m_pHandlingData->GetTransmission().CalculateGearForSimpleCar(speed, m_nCurrentGear);
        float wheelRot = CVehicle::ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, 0.35f);
        for (float& rotation : m_wheelRotation) {
            rotation += wheelRot;
        }
        PlayHornIfNecessary();
        ReduceHornCounter();
        vehicleFlags.bAudioChangingGear = false;
        vehicleFlags.bVehicleColProcessed = false;
        return true;
    }
    case STATUS_PHYSICS:
    case STATUS_GHOST:
    {
        CCarAI::UpdateCarAI(this);
        CCarCtrl::SteerAICarWithPhysics(this);
        CCarCtrl::ReconsiderRoute(this);
        PlayHornIfNecessary();
        if (vehicleFlags.bIsBeingCarJacked) {
            m_GasPedal = 0.0f;
            m_BrakePedal = 1.0f;
            vehicleFlags.bIsHandbrakeOn = true;
        }
        if (!IsAnyWheelTouchingSand())
            return false;
        if (m_nModelIndex == MODEL_RCBANDIT || m_nModelIndex == MODEL_SANDKING || m_nModelIndex == MODEL_BFINJECT)
            return false;
        autoFlags.bIsBoggedDownInSand = true;
        // 0x6B53CC-0x6B5408: wet: ((1 - wet) * mass * ts) * 0.005, dry: (mass * ts) * 0.005
        const float force = CWeather::WetRoads > 0.0f
            ? (1.0f - CWeather::WetRoads) * m_fMass * CTimer::GetTimeStep() * 0.005f
            : m_fMass * CTimer::GetTimeStep() * 0.005f;
        ApplyMoveForce(-force * m_vecMoveSpeed);
        return false;
    }
    case STATUS_ABANDONED:
        for (auto& collisionEntity : m_apWheelCollisionEntity) {
            if (collisionEntity) {
                vehicleFlags.bRestingOnPhysical = true;
                break;
            }
        }

        if (IsSubHeli() || IsSubPlane() && m_vecMoveSpeed.SquaredMagnitude() < 0.1f)
            m_BrakePedal = 1.0f;

        if (vehicleFlags.bRestingOnPhysical)
            m_BrakePedal = 0.5f;
        else if (m_vecMoveSpeed.SquaredMagnitude() < 0.01f)
            m_BrakePedal = 0.2f;
        else
            m_BrakePedal = 0.0f;

        vehicleFlags.bIsHandbrakeOn = false;
        m_fSteerAngle = 0.0f;
        m_GasPedal = 0.0f;

        if (CanUpdateHornCounter())
            m_HornCounter = 0;

        if (!vehicleFlags.bIsBeingCarJacked)
            return false;

        vehicleFlags.bIsHandbrakeOn = true;
        m_GasPedal = 0.0f;
        m_BrakePedal = 1.0f;
        return false;
    case STATUS_WRECKED:
        m_BrakePedal = 0.05f;
        vehicleFlags.bIsHandbrakeOn = true;
        m_fSteerAngle = 0.0f;
        m_GasPedal = 0.0f;
        if (!CanUpdateHornCounter())
            return false;
        m_HornCounter = 0;
        return false;
    case STATUS_REMOTE_CONTROLLED:
        if (CPad::GetPad()->CarGunJustDown() && !CVehicle::bDisableRemoteDetonation) {
            BlowUpCar(FindPlayerPed(), false);
            CRemote::TakeRemoteControlledCarFromPlayer(true);
        }

        if (m_nModelIndex == MODEL_RCBANDIT && !CVehicle::bDisableRemoteDetonationOnContact) {
            //FindPlayerCoors(); // return value is ignored
            if (RcbanditCheckHitWheels()|| physicalFlags.bSubmergedInWater) {
                CRemote::TakeRemoteControlledCarFromPlayer(true);
                BlowUpCar(FindPlayerPed(), false);
            }
        }
        if (FindPlayerInfo().m_pRemoteVehicle == this)
            isRemotelyControlledByPlayer = true;
        break;
    case STATUS_FORCED_STOP:
        if (m_vecMoveSpeed.SquaredMagnitude() < 0.01f
            || m_pDriver
            && m_pDriver->IsPlayer()
            && (m_pDriver->m_nPedState == PEDSTATE_ARRESTED || m_pDriver->GetTaskManager().HasAnyOf<TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, TASK_COMPLEX_CAR_QUICK_BE_DRAGGED_OUT, TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN>()))
        {
            m_BrakePedal = 1.0f;
            m_GasPedal = 0.0f;
            vehicleFlags.bIsHandbrakeOn = true;
        }
        else
        {
            m_BrakePedal = 0.0f;
            vehicleFlags.bIsHandbrakeOn = false;
        }

        m_fSteerAngle = 0.0f;
        m_GasPedal = 0.0f;
        if (!CanUpdateHornCounter())
            return false;
        m_HornCounter = 0;
        return false;
    case STATUS_IS_TOWED:
        vehicleFlags.bIsHandbrakeOn = false;
        m_BrakePedal = 0.0f;
        m_fSteerAngle = 0.0f;
        m_GasPedal = 0.0f;
        if (!m_pTowingVehicle)
            BreakTowLink();
        return false;
    }

    if (GetStatus() != STATUS_PLAYER && GetStatus() != STATUS_REMOTE_CONTROLLED)
        return false;

    bool processControlInput = isRemotelyControlledByPlayer;
    if (!processControlInput && m_pDriver)
    {
        ePedState pedState = m_pDriver->m_nPedState;
        if (pedState != PEDSTATE_EXIT_CAR && pedState != PEDSTATE_DRAGGED_FROM_CAR && pedState != PEDSTATE_ARRESTED)
            processControlInput = true;
    }
    if (!processControlInput)
        return false;

    CPad* pad = nullptr;
    if (m_pDriver) {
        pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
    }

    PruneReferences();
    if (isRemotelyControlledByPlayer) {
        int32 playerSlot = CWorld::FindPlayerSlotWithRemoteVehiclePointer(this);
        if (playerSlot >= 0)
            ProcessControlInputs(playerSlot);
    }
    else if (m_pDriver->IsPlayer()) { // 0x6B4CD4
        ProcessControlInputs(static_cast<uint8>(m_pDriver->m_nPedType));
    }

    if (GetStatus() == STATUS_PLAYER && !IsSubHeli()) {
        if (m_nModelIndex == MODEL_VORTEX || !IsSubPlane()) {
            if (m_nModelIndex != MODEL_SWATVAN && m_nModelIndex != MODEL_RHINO) {
                DoDriveByShootings();
            }
        }
    }

    auto carLess3WheelCounter = FindPlayerInfo().m_nCarLess3WheelCounter;
    m_vecCentreOfMass.z = m_pHandlingData->m_vecCentreOfMass.z;
    if (carLess3WheelCounter > 500 && !IsSubHeli() && !IsSubPlane()) {
        carLess3WheelCounter = std::min(carLess3WheelCounter - 500u, 1000u);
        float fCarLess3WheelCounter = static_cast<float>(carLess3WheelCounter) * 0.002f;
        const float boundMaxZ = colModel->m_boundBox.m_vecMax.z;
        if (GetUp().z > -0.4f && pad) {
            if (GetRight().z <= 0.0f)
                fCarLess3WheelCounter *= -1.0f;

            float steerLeftRight = (float)pad->GetSteeringLeftRight() / 128.0f;
            m_vecCentreOfMass.z += steerLeftRight * boundMaxZ * CAR_BALANCE_MULT * fCarLess3WheelCounter; // 0x6B4DC3-0x6B4DD6: steer * maxZ * mult * counter
        }
        else {
            m_vecCentreOfMass.z -= CAR_BALANCE_MULT * boundMaxZ * 0.5f;
        }
    }

    if (!pad || m_nNumContactWheels != 0 || IsSubHeli() || IsSubPlane()) {
        DoSoftGroundResistance(extraHandlingFlags);
        return false;
    }

    float carWeightMult = 0.0007f;
    if (IsSubMonsterTruck())
        carWeightMult = 0.0025f;
    else
        carWeightMult *= CStats::GetFatAndMuscleModifier(STAT_MOD_DRIVING_SKILL);
    carWeightMult = std::min(1.0f, 3000.0f / m_fTurnMass) * carWeightMult * m_fTurnMass; // 0x6B4E6E-0x6B4E97: (min * mult) * turnMass

    float steerLeftRight = (float)pad->GetSteeringLeftRight() / 128.0f;
    float steerUpDown    = (float)pad->GetSteeringUpDown() / 128.0f;
    if (CCamera::m_bUseMouse3rdPerson && std::fabs(steerLeftRight) < 0.05f && std::fabs(steerUpDown) < 0.05f) {
        steerLeftRight = std::clamp<float>(CPad::NewMouseControllerState.m_AmountMoved.x * ExeRecip(50.0f), -1.5f, 1.5f);
        steerUpDown    = std::clamp<float>(CPad::NewMouseControllerState.m_AmountMoved.y * ExeRecip(50.0f), -1.5f, 1.5f);
    }

    // 0x6B4F69
    if (pad->GetHandBrake()) {
        float dotTurnUp = DotProduct(m_vecTurnSpeed, GetUp());
        if (dotTurnUp < 0.02f && steerLeftRight < 0.0f || dotTurnUp > -0.02f && steerLeftRight > 0.0f) {
            CVector point = m_vecCentreOfMass + GetForward();
            CVector force = GetRight() * (CTimer::GetTimeStep() * steerLeftRight * carWeightMult); // 0x6B4FED-0x6B50AD: the scalar (ts * steer) * mult is formed first
            ApplyTurnForce(force, point);
        }
    }
    else if (!pad->GetAccelerate()) {
        float dotTurnForward = DotProduct(m_vecTurnSpeed, GetForward());
        if (dotTurnForward < 0.02f && steerLeftRight < 0.0f
            || dotTurnForward > -0.02f && steerLeftRight > 0.0f)
        {
            CVector point = m_vecCentreOfMass + GetUp();
            CVector force = GetRight() * (CTimer::GetTimeStep() * steerLeftRight * carWeightMult); // 0x6B4FED-0x6B50AD: the scalar (ts * steer) * mult is formed first
            ApplyTurnForce(force, point);
        }
    }

    if (!pad->GetAccelerate()) {
        float dotTurnRight = DotProduct(m_vecTurnSpeed, GetRight());
        if (dotTurnRight < 0.02f && steerUpDown < 0.0f
            || dotTurnRight > -0.02f && steerUpDown > 0.0f)
        {
            CVector point = m_vecCentreOfMass + GetForward();
            CVector force = GetUp() * (CTimer::GetTimeStep() * steerUpDown * carWeightMult); // 0x6B515B-0x6B517B
            ApplyTurnForce(force, point);
        }
    }
    DoSoftGroundResistance(extraHandlingFlags);
    return false;
}

// 0x6A2AE0
void CAutomobile::ResetSuspension()
{
    for (int32 i = 0; i < 4; i++) {
        m_fWheelsSuspensionCompression[i] = 1.0f;
        m_WheelCounts[i] = 0.0f;
        m_wheelRotation[i] = 0.0f;
        m_WheelStates[i] = WHEEL_STATE_NORMAL;
    }
}

// 0x6A8500
void CAutomobile::ProcessFlyingCarStuff()
{
    if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED || GetStatus() == STATUS_PHYSICS) {
        if (CCheat::IsActive(CHEAT_CARS_FLY)
            && m_vecMoveSpeed.Magnitude() > 0.0f
            && CTimer::GetTimeStep() > 0.0f
        ) {
            FlyingControl(FLIGHT_MODEL_PLANE, -9999.9902F, -9999.9902F, -9999.9902F, -9999.9902F);
        }
    }
}

// 0x6A45C0
void CAutomobile::DoHoverSuspensionRatios()
{
    if (GetUp().z >= 0.3f && !vehicleFlags.bIsDrowning) {
        auto colData = GetColModel()->m_pColData;
        for (int32 i = 0; i < 4; i++)  {
            CColLine& line = colData->m_pLines[i];
            CVector start = m_matrix->TransformPoint(line.m_vecStart);
            CVector end = m_matrix->TransformPoint(line.m_vecEnd);
            float colPointZ = MAP_Z_LOW_LIMIT;
            if (m_fWheelsSuspensionCompression[i] < 1.0f) {
                colPointZ = m_wheelColPoint[i].m_vecPoint.z;
            }
            float waterLevel = 0.0f;
            if (CWaterLevel::GetWaterLevel(end.x, end.y, end.z, waterLevel, false, nullptr)
                && waterLevel > colPointZ && end.z - 1.0f < waterLevel)
            {
                if (waterLevel <= end.z)
                    m_fWheelsSuspensionCompression[i] = 0.99999f; // 0x6A46EB: mov [edi], 0x3F7FFF58 (not 1.0)
                else if (waterLevel <= start.z)
                    m_fWheelsSuspensionCompression[i] = (start.z - waterLevel) / (start.z - end.z);
                else
                    m_fWheelsSuspensionCompression[i] = 0.0f;

                CVector point = (end - start) * m_fWheelsSuspensionCompression[i];
                point += start;
                point.z = waterLevel;
                m_wheelColPoint[i].m_vecPoint = point;
                m_wheelColPoint[i].m_vecNormal = CVector(0.0f, 0.0f, 1.0f);
                m_wheelColPoint[i].m_nSurfaceTypeB = SURFACE_WATER_SHALLOW; // 0x6A4754: mov byte [ebx+0x23], 0x27
            }
        }
    }
}

// 0x6AFB10
void CAutomobile::ProcessSuspension() {
    float springLength[4]{};
    float wheelSpringForceDampingLimits[4]{};
    for (int32 i = 0; i < 4; i++) {
        springLength[i] = m_fWheelsSuspensionCompression[i];
        wheelSpringForceDampingLimits[i] = 0.0f;
    }

    // 0x6AFB10-0x6AFB9B: the dot product is summed z, y, x in the x87 register and rounded once when stored
    const auto& fwd0 = GetForward();
    float forwardSpeed = (float)std::fabs((x87d)m_vecMoveSpeed.z * fwd0.z + (x87d)m_vecMoveSpeed.y * fwd0.y + (x87d)m_vecMoveSpeed.x * fwd0.x);

    int32 numWheelLoops = 1;
    if (ModelIndices::IsRhino(m_nModelIndex) && CReplay::Mode != MODE_PLAYBACK) {
        numWheelLoops = 3;
    }

    CVector contactPoints[4]{};
    CVector directions[4]{};
    for (int32 wheelLoopIndex = 0; wheelLoopIndex < numWheelLoops; wheelLoopIndex++) {
        for (int32 i = 0; i < 4; i++) {
            directions[i] = GetUp() * -1.0f;
            if (springLength[i] < 1.0f) {
                contactPoints[i] = m_wheelColPoint[i].m_vecPoint - GetPosition();
            } else {
                contactPoints[i].Set(0.f, 0.f, 0.f);
            }
        }

        for (int32 i = 0; i < 4; i++) {
            if (springLength[i] >= 1.0f)
                continue;

            float suspensionBias = m_pHandlingData->m_fSuspensionBiasBetweenFrontAndRear;
            if (i == CAR_WHEEL_REAR_LEFT || i == CAR_WHEEL_REAR_RIGHT)
                suspensionBias = 1.0f - suspensionBias;

            float fSuspensionForceLevel = m_pHandlingData->m_fSuspensionForceLevel;
            if (handlingFlags.bHydraulicGeom && handlingFlags.bHydraulicInst && handlingFlags.bNpcNeutralHandl && GetStatus() == STATUS_PHYSICS) {
                suspensionBias = 0.5f; // 0x6AFF7D-0x6AFFA3
            }

            // 0x6AFFAB-0x6B000A: the Vortex scale is the `else` of the hydraulic-installed slow branch (that branch multiplies by 1.5 with the geometry flag, by 1.0 without)
            if (handlingFlags.bHydraulicInst && std::fabs(forwardSpeed) < 0.15f) {
                if (handlingFlags.bHydraulicGeom) {
                    fSuspensionForceLevel *= 1.5f;
                }
            } else if (ModelIndices::IsVortex(m_nModelIndex)) {
                fSuspensionForceLevel *= std::fabs(AsPlane()->m_fAccelerationBreakStatus) * 0.25f + 1.0f;
            }

            if (CCheat::IsActive(CHEAT_CARS_ON_WATER) || ModelIndices::IsVortex(m_nModelIndex))
                ApplySpringCollision(
                    fSuspensionForceLevel,
                    directions[i],
                    contactPoints[i],
                    springLength[i],
                    suspensionBias,
                    wheelSpringForceDampingLimits[i]
                );
            else
                ApplySpringCollisionAlt(
                    fSuspensionForceLevel,
                    directions[i],
                    contactPoints[i],
                    springLength[i],
                    suspensionBias,
                    m_wheelColPoint[i].m_vecNormal,
                    wheelSpringForceDampingLimits[i]
                );
        }

        CVector contactSpeeds[4]{};
        for (int32 i = 0; i < 4; ++i) {
            contactSpeeds[i] = CPhysical::GetSpeed(contactPoints[i]);
            if (m_apWheelCollisionEntity[i])
                contactSpeeds[i] -= m_apWheelCollisionEntity[i]->GetSpeed(m_vWheelCollisionPos[i]);

            if (m_fWheelsSuspensionCompression[i] < 1.0f && m_wheelColPoint[i].m_vecNormal.z > 0.35f) {
                directions[i] = -m_wheelColPoint[i].m_vecNormal;
            }
        }

        for (int32 i = 0; i < 4; ++i) {
            float dampingForce = m_pHandlingData->m_fSuspensionDampingLevel;
            if (handlingFlags.bHydraulicInst && dampingForce > 0.1f && fabs(forwardSpeed) < 0.15f)
                dampingForce = 0.1f;

            if (springLength[i] < 0.99999f /* 0x871258 */ && !autoFlags.bIsMonsterTruck)
                ApplySpringDampening(
                    dampingForce,
                    wheelSpringForceDampingLimits[i],
                    directions[i],
                    contactPoints[i],
                    contactSpeeds[i]
                );
        }

        if (!ModelIndices::IsRhino(m_nModelIndex))
            continue;

        if (numWheelLoops <= 1 || wheelLoopIndex > 1)
            break;

        float fDummy1, fDummy2;
        int32 wheelLineIndices[4]{};
        if (wheelLoopIndex == 0) {
            wheelLineIndices[CAR_WHEEL_FRONT_LEFT] = 4;
            wheelLineIndices[CAR_WHEEL_REAR_LEFT] = 7;
            wheelLineIndices[CAR_WHEEL_FRONT_RIGHT] = 8;
            wheelLineIndices[CAR_WHEEL_REAR_RIGHT] = 11;

            m_doors[DOOR_LEFT_REAR].GetExtraWheelPositions(springLength[CAR_WHEEL_FRONT_LEFT], fDummy1, fDummy2, springLength[CAR_WHEEL_REAR_LEFT]);
            m_doors[DOOR_RIGHT_REAR].GetExtraWheelPositions(springLength[CAR_WHEEL_FRONT_RIGHT], fDummy1, fDummy2, springLength[CAR_WHEEL_REAR_RIGHT]);
        }
        else if (wheelLoopIndex == 1) {
            m_doors[DOOR_LEFT_REAR].GetExtraWheelPositions(fDummy1, springLength[CAR_WHEEL_FRONT_LEFT], springLength[CAR_WHEEL_REAR_LEFT], fDummy2);
            m_doors[DOOR_RIGHT_REAR].GetExtraWheelPositions(fDummy1, springLength[CAR_WHEEL_FRONT_RIGHT], springLength[CAR_WHEEL_REAR_RIGHT], fDummy2);

            wheelLineIndices[CAR_WHEEL_FRONT_LEFT]  = 5;
            wheelLineIndices[CAR_WHEEL_REAR_LEFT]   = 6;
            wheelLineIndices[CAR_WHEEL_FRONT_RIGHT] = 9;
            wheelLineIndices[CAR_WHEEL_REAR_RIGHT]  = 10;
        }

        CCollisionData* colData = GetColModel()->m_pColData;
        float wheelsSuspensionCompressionPrev[4]{};
        for (int32 i = 0; i < 4; i++) {
            CColLine& colLine = colData->m_pLines[wheelLineIndices[i]];
            contactPoints[i]  = colLine.m_vecStart;
            contactPoints[i].z -= m_aSuspensionLineLength[i] * springLength[i];
            contactPoints[i] = GetMatrix().TransformVector(contactPoints[i]);

            auto springTension = 1.f - (m_aSuspensionSpringLength[i] / m_aSuspensionLineLength[i]);
            springLength[i]    = (springLength[i] - springTension) / (1.f - springTension);
        }
    }

    float speedThreshold = 0.02f;
    float rollOnToWheelsForce = ROLL_ONTO_WHEELS_FORCE;
    if (GetStatus() != STATUS_PLAYER && GetStatus() != STATUS_REMOTE_CONTROLLED) {
        speedThreshold *= 2.0f;
        rollOnToWheelsForce *= 2.0f;
    }

    if (forwardSpeed < speedThreshold) {
        float fDir = 0.0f;
        if (m_fWheelsSuspensionCompression[CAR_WHEEL_FRONT_LEFT] == 1.0f
            && m_fWheelsSuspensionCompression[CAR_WHEEL_REAR_LEFT] == 1.0f
            && (m_fWheelsSuspensionCompression[CAR_WHEEL_FRONT_RIGHT] < 1.0f
                || m_fWheelsSuspensionCompression[CAR_WHEEL_REAR_RIGHT] < 1.0f))
        {
            fDir = 1.0f;
        }
        else if (forwardSpeed < speedThreshold
            && m_fWheelsSuspensionCompression[CAR_WHEEL_FRONT_RIGHT] == 1.0f
            && m_fWheelsSuspensionCompression[CAR_WHEEL_REAR_RIGHT] == 1.0f
            && (m_fWheelsSuspensionCompression[CAR_WHEEL_FRONT_LEFT] < 1.0f
                || m_fWheelsSuspensionCompression[CAR_WHEEL_REAR_LEFT] < 1.0f))
        {
            fDir = -1.0f;
        }

        if (fDir != 0.0f) {
            CVector right = CrossProduct(GetForward(), CVector(0.0f, 0.0f, 1.0f));
            if (fabs(DotProduct(right, GetRight())) < 0.6f) {
                auto colModel = GetVehicleModelInfo()->GetColModel();
                // 0x6B0573-0x6B0677: the factor stays in the x87 register: (force*turnMass)*dir, and (force*mass)*dir on the cross product of (0,0,1) x forward (= -right)
                const x87d turnScale = ((x87d)rollOnToWheelsForce * m_fTurnMass) * fDir;
                CVector force = { (float)(GetUp().x * turnScale), (float)(GetUp().y * turnScale), (float)(GetUp().z * turnScale) };
                CVector point = GetRight() * colModel->GetBoundingBox().m_vecMax.x;
                ApplyTurnForce(force, point);
                const x87d moveScale = ((x87d)rollOnToWheelsForce * m_fMass) * fDir;
                ApplyMoveForce({ (float)(-right.x * moveScale), (float)(-right.y * moveScale), (float)(-right.z * moveScale) });
            }
        }
    }
}

// 0x6ACE70
int32 CAutomobile::ProcessEntityCollision(CEntity* entity, CColPoint* outColPoints) {
    if (GetStatus() != STATUS_SIMPLE) {
        vehicleFlags.bVehicleColProcessed = true;
    }

    const auto tcd = GetColData(),
               ocd = entity->GetColData();

#if defined(FIX_BUGS) || defined(NOTSA_STANDALONE_RUN) // the run build must not dereference the missing colmodel data below either (null ocd, e.g. a building whose COL is not streamed in)
    // FIX_BUGS@CAutomobile::ProcessEntityCollision:1
    // The original code handled this properly, because `ProcessColModels` returned `0` 
    // if either colmodel's data was missing
    // but there's a lot of no-op stuff done below that we can just avoid altogether
    // Though, in the original code there was an edge case if `m_pTractor == entity || m_pTrailer == entity` => crash
    // but since I moved the assignment to the outside of the function now it always crashes xD
    if (!tcd || !ocd) {
        return 0;
    }
#endif

    // The Rhinosaurus Rex has extra wheels (8 of them)
    std::array<float, MAX_NUM_SUSP_LINES> wheelColPtsTouchDists{};
    rng::copy(m_fWheelsSuspensionCompression, wheelColPtsTouchDists.begin());

    const auto originalWheelSuspCompr{m_fWheelsSuspensionCompression};

    // ...and their suspension compression values are stored inside doors (fml pls)
    const auto GetRhinosaurusRexExtraWheelSuspCompression = [this](size_t extraWheelIdx) -> float& { // 
        auto& door = m_doors[4 + extraWheelIdx / 4];
        switch (extraWheelIdx % 4) {
        case 0: return door.m_openAngle;
        case 1: return door.m_closedAngle;
        case 2: return door.m_angle;
        case 3: return door.m_prevAngle;
        default: NOTSA_UNREACHABLE_RET(door.m_openAngle);
        }
    };

    // 0x6ACEF7
    if (m_nModelIndex == eModelID::MODEL_RHINO) {
        // Store shit for the rhinosaurus rex
        for (auto i = 0; i < NUM_RHINO_EXTRA_SUSP_LINES; i++) {
            wheelColPtsTouchDists[NUM_AUTOMOBILE_SUSP_LINES + i] = GetRhinosaurusRexExtraWheelSuspCompression(i);
        }
    }

    const auto tNumLines = tcd->m_nNumLines;
    if (   physicalFlags.bSkipLineCol
        || physicalFlags.bProcessingShift
        || entity->GetIsTypePed()
        || m_nModelIndex == (uint16)eModelID::UNLOAD_MODEL && entity->GetIsTypeVehicle()
    ) {
        tcd->m_nNumLines = 0; // Later reset back to original value
    }

    // Hide triangles in some cases
    const auto didHideTriangles = m_pTowingVehicle == entity || m_pVehicleBeingTowed == entity;
    const auto tNumTri = tcd->m_nNumTriangles, // Saving my sanity here, and unconditionally assigning
               oNumTri = ocd->m_nNumTriangles;
    if (didHideTriangles) {
        tcd->m_nNumTriangles = ocd->m_nNumTriangles = 0;
    }

    // For ghosts we dont do shit (In case this garbage is a forklift this value is modified below)
    auto numColPts = GetStatus() == STATUS_GHOST
        ? 0
        : CCollision::ProcessColModels(
            GetMatrix(), *GetColModel(),
            entity->GetMatrix(), *entity->GetColModel(),
            *(std::array<CColPoint, 32>*)(outColPoints),
            aAutomobileColPoints.data(),
            wheelColPtsTouchDists.data(),
            false
        );

    // Restore hidden triangles
    if (didHideTriangles) {
        tcd->m_nNumTriangles = tNumTri;
        ocd->m_nNumTriangles = oNumTri;
    }

    size_t numProcessedWheels{};
    if (tcd->m_nNumLines) {
        // Process the real wheels
        for (auto i = 0; i < MAX_CARWHEELS; i++) {
            // 0x6AD0D4
            const auto& cp = aAutomobileColPoints[i];

            const auto wheelColPtsTouchDist = wheelColPtsTouchDists[i];
            if (wheelColPtsTouchDist >= 1.f || wheelColPtsTouchDist >= m_fWheelsSuspensionCompression[i]) {
                continue;
            }

            numProcessedWheels++;

            m_fWheelsSuspensionCompression[i] = wheelColPtsTouchDist;
            m_wheelColPoint[i].AssignLikeExe(cp); // 0x6AD11A: CALLS the exe's CColPoint::operator= 0x40FC80 (partial copy)

            m_anCollisionLighting[i] = cp.m_nLightingB;
            m_nContactSurface = cp.m_nSurfaceTypeB;

            switch (entity->GetType()) {
            case ENTITY_TYPE_VEHICLE:
            case ENTITY_TYPE_OBJECT: {
                CEntity::SetEntityReference(m_apWheelCollisionEntity[i], entity->AsPhysical()); // 0x6AD147: the exe stores + registers, it does not clean up the old reference

                m_vWheelCollisionPos[i] = cp.m_vecPoint - entity->GetPosition();
                if (entity->GetIsTypeVehicle()) {
                    m_anCollisionLighting[i] = entity->AsVehicle()->m_anCollisionLighting[i];
                }
                break;
            }
            case ENTITY_TYPE_BUILDING: {
                m_pEntityWeAreOn = entity;
                m_bTunnel = entity->m_bTunnel;
                m_bTunnelTransition = entity->m_bTunnelTransition;
                break;
            }
            }
        }

        // Deal with extra wheels for the Rhinosaurus Rex
        if (m_nModelIndex == eModelID::MODEL_RHINO) {
            for (auto i = 0; i < NUM_RHINO_EXTRA_SUSP_LINES; i++) {
                auto& compr = GetRhinosaurusRexExtraWheelSuspCompression(i);
                compr = std::min(wheelColPtsTouchDists[NUM_AUTOMOBILE_SUSP_LINES + i], compr);
            }
        }
    } else {
        tcd->m_nNumLines = tNumLines;
    }
    
    // 0x6AD38A - Remove a few specific cp's for forklifts
    if (m_nModelIndex == eModelID::MODEL_FORKLIFT && entity->GetIsTypeObject() && !m_wMiscComponentAngle && !m_wMiscComponentAnglePrev) {
        // The exe erases from the OUTPUT array `outColPoints` (arg 2, [esp+0xA0] at 0x6AD3C5; the sphere contacts of ProcessColModels live there, not in
        // the wheel line array aAutomobileColPoints), shifting the tail down with the CALLED CColPoint::operator= 0x40FC80 (0x6AD3E7) and re-checking the same index
        for (auto i = 0; i < numColPts; i++) {
            if (outColPoints[i].m_nSurfaceTypeA != SURFACE_CAR_MOVINGCOMPONENT) {
                continue;
            }
            for (auto j = i; j < numColPts - 1; j++) {
                outColPoints[j].AssignLikeExe(outColPoints[j + 1]);
            }
            numColPts--;
            i--;
        }
    }

    if (numColPts <= 0 && numProcessedWheels <= 0) {
        goto rtn;
    }

    AddCollisionRecord(entity);
    if (!entity->GetIsTypeBuilding()) {
        entity->AsPhysical()->AddCollisionRecord(this);
    }

    if (numColPts > 0) {
        if (   entity->GetIsTypeBuilding()
            || (entity->GetIsTypeObject() && entity->AsPhysical()->physicalFlags.bDisableCollisionForce)
        ) {
            SetHasHitWall(true);
        }
    }

    // 0x6AD482 - Add additional output colpoints for wheels
    if (numProcessedWheels > 0 && entity->GetIsTypeBuilding() && m_pHandlingData->m_fSuspensionHighSpdComDamp > 0.f) {
        for (auto i = 0; i < MAX_CARWHEELS; i++) {
            if (m_damageManager.GetWheelStatus((eCarWheel)i) != eCarWheelStatus::WHEEL_STATUS_OK) { // Move to top
                continue;
            }
            const auto compressionNow = std::min(
                originalWheelSuspCompr[i],
                lerpBlend(m_fWheelsSuspensionCompressionPrev[i], 1.f, 1.f - m_aSuspensionSpringLength[i] / m_aSuspensionLineLength[i]) // exe: `(1 - t) * prev + t`
            );
            const auto suspComprDelta = (compressionNow - m_fWheelsSuspensionCompression[i]) * m_aSuspensionLineLength[i]; // Suspension compression delta (In meters)
            if (suspComprDelta <= 0.1f /* 0x8D3200 */) {
                continue;
            }
            const auto& wheelcp = m_wheelColPoint[i];
            if (g_surfaceInfos.GetAdhesionGroup(wheelcp.m_nSurfaceTypeB) == ADHESION_GROUP_SAND || wheelcp.m_nSurfaceTypeB == SURFACE_RAILTRACK) {
                continue;
            }
            auto& outcp = outColPoints[numColPts];
            outcp.AssignLikeExe(wheelcp); // 0x6AD584: CALLS 0x40FC80 (partial copy)
            // 0x6AD5B8-0x6AD630: the dot products are summed z,y,x (speed . forward) and z,x,y (normal . up); the factors multiply in the order
            // |n.up| * min(|v.fwd|, 0.3) * min(0.2, delta) * (1/0.3) * highSpdComDamp (the port multiplied the damping before the 1/0.3)
            const auto& nrm = wheelcp.m_vecNormal;
            const auto& up  = GetUp();
            const auto& fwd = GetForward();
            const auto& ms  = GetMoveSpeed();
            const float normalDotUp = std::abs(nrm.z * up.z + nrm.x * up.x + nrm.y * up.y);
            const float forwardSpeedClamped = std::min(std::abs(ms.z * fwd.z + ms.y * fwd.y + ms.x * fwd.x), 0.3f); // Forward speedsq
            outcp.m_fDepth = ((((normalDotUp * forwardSpeedClamped) * std::min(0.2f, suspComprDelta)) * ExeRecip(0.3f)) * m_pHandlingData->m_fSuspensionHighSpdComDamp);
            outcp.m_nPieceTypeA = CarWheelToCarPiece((eCarWheel)i);
            outcp.m_nSurfaceTypeA = outcp.m_nSurfaceTypeB = SURFACE_WHEELBASE;
            // 0x6AD657-0x6AD66F: the exe counts the point (while numColPts < 31) and goes on with the next wheel; the port only counted it (and stopped) under FIX_BUGS,
            // so in the run build (no FIX_BUGS) the wheel contact points added at buildings were never returned
            if (numColPts < 31) { // `32` is ProcessColModels output array size
                numColPts++;
            }
        }
    }

rtn:
    return numColPts;
}

// 0x6A29C0
void CAutomobile::ProcessControlCollisionCheck(bool applySpeed) {
    const CMatrix ogmat = GetMatrix(); // Save original matrix (We need it later)

    SetIsStuck(false);

    SkipPhysics();

    physicalFlags.bSkipLineCol = physicalFlags.bProcessingShift = false;

    m_fMovingSpeed = 0.f;
    rng::fill(m_fWheelsSuspensionCompression, 1.f);
    
    if (applySpeed) {
        ApplyMoveSpeed();
        ApplyTurnSpeed();
        for (auto tr = 0; CheckCollision() && tr < 5; tr++) { // tr = tries
            GetMatrix() = ogmat; // reset matrix
            ApplyMoveSpeed();
            ApplyTurnSpeed();
        }
    } else {
        const auto ogUsesCollision = GetUsesCollision();
        SetUsesCollision(false);
        (void)CheckCollision();
        SetUsesCollision(ogUsesCollision); // restore
    }

    SetIsStuck(false);
    SetIsInSafePosition(true);
}

// 0x6AD690
void CAutomobile::ProcessControlInputs(uint8 playerNum) {
    const auto plyrpad = CPad::GetPad((int32)playerNum);
    const auto plyrdriver = m_pDriver; // I guess in case of the RC Bandit for example the player isn't sitting in the plyrveh, but can control it, so this might be null!

    //> 0x6AD6CB - Update handbrake state
    const auto automaticallyHandBrake = [&, this] {
        if (plyrdriver) {
            auto& tm = plyrdriver->GetTaskManager();
            if (!tm.Has<TASK_SIMPLE_CAR_JUMP_OUT>()) {
                if (plyrpad->GetExitVehicle()) {
                    return true;
                }
                if (plyrdriver->m_nPedState == PEDSTATE_ARRESTED) {
                    return true;
                }
                if (tm.HasAnyOf<TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, TASK_COMPLEX_CAR_QUICK_BE_DRAGGED_OUT, TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN>()) {
                    return true;
                }
            }
        }
        return false;
    }();
    vehicleFlags.bIsHandbrakeOn = automaticallyHandBrake || plyrpad->GetHandBrake() != 0;

    //> 0x6AD6A8 - forward speed, summed z, y, x like the exe
    const float fwdvel = GetMoveSpeed().z * GetForward().z + GetMoveSpeed().y * GetForward().y + GetMoveSpeed().x * GetForward().x;

    //> 0x6AD7BD - Update steer angle (And  possible control input too)
    {
        const auto ts           = CTimer::GetTimeStep();
        const auto steerLR      = plyrpad->GetSteeringLeftRight();
        const auto mouseMovedX  = CPad::NewMouseControllerState.m_AmountMoved.x;
        const auto KeyboardSteer = [&](bool mouseEnabledPath) { // 0x6AD8F0 (plain) / 0x6AD831 (mouse enabled): the exe multiplies `* 0.2f * ts` in the first and `* ts * 0.2f` in the second
            const auto diff = (float)(-(int32)steerLR) * (1.f / 128.f) - m_fRawSteerAngle;
            m_fRawSteerAngle += mouseEnabledPath ? diff * ts * ExeRecip(5.f) : diff * ExeRecip(5.f) * ts;
            m_nLastControlInput = eControllerType::KEYBOARD;
        };
        if (!CCamera::m_bUseMouse3rdPerson || !m_bEnableMouseSteering) {
            KeyboardSteer(false);
        } else if (   mouseMovedX != 0.f
                   || (m_fRawSteerAngle != 0.f && m_nLastControlInput == eControllerType::MOUSE && steerLR == 0)
        ) { // 0x6AD873
            const auto mouseLook = plyrpad->NewState.m_bVehicleMouseLook != 0;
            if (!mouseLook) {
                m_fRawSteerAngle += mouseMovedX * -0.0035f;
            }
            m_nLastControlInput = eControllerType::MOUSE;
            if (std::abs(m_fRawSteerAngle) < 0.7f || mouseLook) { // 0x6AD8B5: also decays while NOT in mouse look when |steer| < 0.7
                m_fRawSteerAngle *= std::pow(0.975f, ts);
            }
        } else if (steerLR != 0 || m_nLastControlInput != eControllerType::MOUSE) { // 0x6AD80F
            KeyboardSteer(true);
        } // else: no change (0x6AD941)
    }
    m_fRawSteerAngle = std::clamp(m_fRawSteerAngle, -1.f, 1.f);

    //> Calculate gas/brake pedal values
    {
        //> 0x6AD98D - gas input: float(accel - brake) * (1/255), the gang drive-by shoulder buttons only when it is exactly 0
        float gasPedalInput = (float)(plyrpad->GetAccelerate() - plyrpad->GetBrake()) * ExeRecip(255.f);
        if (   gasPedalInput == 0.f
            && plyrdriver
            && notsa::dyn_cast_if_present<CTaskSimpleGangDriveBy>(plyrdriver->GetTaskManager().GetSimplestActiveTask())
            && TheCamera.m_aCams[0].m_nMode == MODE_AIMWEAPON_FROMCAR
        ) {
            gasPedalInput = (float)(plyrpad->NewState.RightShoulder2 - plyrpad->NewState.LeftShoulder2) * ExeRecip(255.f);
        }
        if (m_nModelIndex == (uint16)eModelID::UNLOAD_MODEL && gasPedalInput < 0.f) { // 0x6ADA46: scaled AFTER the 1/255 (the port had it before)
            gasPedalInput *= 0.3f;
        }
        if (!vehicleFlags.bEngineOn && vehicleFlags.bIsDrowning) { // 0x6ADA6C: engine OFF and drowning (the port had engine ON)
            gasPedalInput = 0.f;
        }

        if (automaticallyHandBrake) { // 0x6ADA8D
            m_GasPedal   = 0.f;
            m_BrakePedal = 1.f;
        } else if (std::abs(fwdvel) < 0.01f) { // 0x6ADAC6
            if (plyrpad->GetAccelerate() > 150 && plyrpad->GetBrake() > 150) {
                m_GasPedal      = (float)plyrpad->GetAccelerate() * ExeRecip(255.f);
                m_BrakePedal    = (float)plyrpad->GetBrake() * ExeRecip(255.f);
                m_bDoingBurnout = true;
            } else {
                m_GasPedal   = gasPedalInput;
                m_BrakePedal = 0.f;
            }
        } else if (fwdvel >= 0.f) { // 0x6ADB8B
            if (gasPedalInput < 0.f) { // Braking by pressing the reverse button
                m_GasPedal   = 0.f;
                m_BrakePedal = -gasPedalInput;
            } else {
                m_GasPedal   = gasPedalInput;
                m_BrakePedal = 0.f;
            }
        } else if (gasPedalInput >= 0.f && (m_GasPedal <= 0.5f || fwdvel <= -0.15f)) { // 0x6ADBC1 - Apply braking
            m_GasPedal   = 0.f;
            m_BrakePedal = gasPedalInput;
        } else { // Regular go forwards
            m_GasPedal   = gasPedalInput;
            m_BrakePedal = 0.f;
        }
    }

    //> 0x6ADC18 and 0x6ADC64 - Check if we can go towards that direction
    if (m_GasPedal != 0.f && plyrdriver) {
        if (!CGameLogic::IsPlayerAllowedToGoInThisDirection(
            plyrdriver,
            m_GasPedal > 0.f ? GetForward() : -GetForward(),
            0.f
        )) {
            m_GasPedal = 0.f;
        }
    }

    //> 0x6ADCDB - steer angle: (steeringLock * deg2rad) * signed square of the raw steer (the exe's order of multiplication)
    {
        const float rawSq = m_fRawSteerAngle * m_fRawSteerAngle;
        m_fSteerAngle = m_pHandlingData->m_fSteeringLock * DEG_TO_RAD * (m_fRawSteerAngle < 0.f ? -rawSq : rawSq);
    }

    //> 0x6ADD28 - Rear wheel steer
    if (m_pHandlingData->m_bHbRearwheelSteer) {
        m_f2ndSteerAngle = vehicleFlags.bIsHandbrakeOn
            ? 0.9f * m_f2ndSteerAngle - (1.0f - 0.9f) * m_fSteerAngle // 0x6ADD39: the exe weights the OLD value by 0.9 (0x8D3208) and the new one by (1 - 0.9); the previous `lerp(old, -steer, 0.9f)` had the weights the other way round
            : m_f2ndSteerAngle * 0.9f; // TODO/NOTE: Seems incorrect, maybe they meant `m_fSteerAngle * 0.9f`?
    }

    //> 0x6ADD81 - Comedy controls stuff
    if (vehicleFlags.bComedyControls) {
        const auto rndnum = CGeneral::GetRandomNumber() % 10; // 0x6ADD74: rand() % 10 (idiv), not the scaled range helper

        using enum eComedyControlState;
        if (m_comedyControlState != INACTIVE) {
            m_fSteerAngle += m_comedyControlState == STEER_RIGHT ? 0.05f : -0.05f;
            if (rndnum < 2) {
                m_comedyControlState = INACTIVE;
            }
        } else {
            m_comedyControlState = [&] {
                switch (rndnum) {
                case 0:
                case 1:
                    return STEER_RIGHT;
                case 2:
                case 3:
                    return STEER_LEFT;
                default:
                    return INACTIVE; // No change
                }
            }();
        }
    }

    //> 0x6ADDCC - Slow player down in some cases
    if (const auto pad0 = CPad::GetPad(0); (pad0->DisablePlayerControls && CGameLogic::SkipState != 2) || pad0->bApplyBrakes) {
        vehicleFlags.bIsHandbrakeOn = true;

        m_GasPedal = 0.f;
        m_BrakePedal = 1.f;
        FindPlayerPed()->KeepAreaAroundPlayerClear();
        if (const auto speedsq = GetMoveSpeed().SquaredMagnitude(); speedsq >= sq(0.28f)) {
            GetMoveSpeed() *= 0.28f / std::sqrt(speedsq);
        }
    }

    //> 0x6ADEB3
    if (vehicleFlags.bEngineBroken) {
        vehicleFlags.bIsHandbrakeOn = false;

        m_BrakePedal = 0.05f;
        m_GasPedal = 0.f;
    }
}

// 0x6A2210
void CAutomobile::GetComponentWorldPosition(int32 componentId, CVector& outPos)
{
    if (m_aCarNodes[componentId]) {
        outPos = RwFrameGetLTM(m_aCarNodes[componentId])->pos;
    }
}

// 0x6A2250
bool CAutomobile::IsComponentPresent(int32 componentId) const
{
    return m_aCarNodes[componentId] != 0;
}

// 0x6A6AE0
void CAutomobile::OpenDoor(CPed* ped, int32 nodeIdx, eDoors doorIdx, float doorOpenRatio, bool playSound) {
    const auto frame = m_aCarNodes[nodeIdx];
    if (!frame) {
        return;
    }

    CMatrix frameMat{ RwFrameGetMatrix(frame) }; // Is it necessary to do this here?
    auto& door = m_doors[doorIdx];

    const auto wasClosed = door.IsClosed();
    if (wasClosed) {
        RwFrameForAllObjects( // TODO: Make function (See refs to `SetAtomicFlagCB` and `ClearAtomicFlagCB` and combine them somehow)
            frame,
            CVehicleModelInfo::ClearAtomicFlagCB,
            (void*)(eAtomicComponentFlag::ATOMIC_DONT_CULL)
        );
    }

    door.Open(doorOpenRatio);

    const auto ProcessSound = [&](eAudioEvents baseAE, float eventQuietVolume) {
        if (!playSound) {
            return;
        }
        m_vehicleAudio.AddAudioEvent((eAudioEvents)((size_t)baseAE + (size_t)doorIdx), 0.f);
        if (!ped) {
            return;
        }
        GetEventGlobalGroup()->Add(CEventSoundQuiet{ ped, eventQuietVolume, (uint32)-1, {} });
    };

    if (wasClosed && !door.IsClosed()) {
        RwFrameForAllObjects( // TODO: Make function (See refs to `SetAtomicFlagCB` below)
            frame,
            CVehicleModelInfo::SetAtomicFlagCB,
            (void*)(eAtomicComponentFlag::ATOMIC_DONT_CULL)
        );
        ProcessSound(AE_CAR_BONNET_OPEN, 60.f);
    } else if (!wasClosed && doorOpenRatio == 0.f) {
        m_damageManager.SetDoorClosed(doorIdx);
        ProcessSound(AE_CAR_BONNET_CLOSE, 120.f);
    }

    door.UpdateFrameMatrix(frameMat);
}

// 0x6A2270
float CAutomobile::GetDooorAngleOpenRatio(eDoors door)
{
    const auto doorId = (int32)door;
    return m_doors[doorId].GetAngleOpenRatio();
}

// 0x6A62C0
float CAutomobile::GetDooorAngleOpenRatioU32(uint32 door)
{
    switch (door) {
    case 8:
        return GetDooorAngleOpenRatio(DOOR_RIGHT_FRONT);
    case 9:
        return GetDooorAngleOpenRatio(DOOR_RIGHT_REAR);
    case 10:
        return GetDooorAngleOpenRatio(DOOR_LEFT_FRONT);
    case 11:
        return GetDooorAngleOpenRatio(DOOR_LEFT_REAR);
    case 18:
        return 0.0f;
    default:
        assert(0); // Prevent usage mistakes - There's is an eDoors overload, and if an int literal is used instead of the enum this function will be called incorrectly.
        return 0.0f;
    }
}

// 0x6A2290
bool CAutomobile::IsDoorReady(eDoors door)
{
    if (m_doors[door].IsClosed())
        return true;

    if (IsDoorMissing(door))
        return true;

    return false;
}

// 0x6A6350
bool CAutomobile::IsDoorReadyU32(uint32 door)
{
    switch (door) {
    case 8:
        return IsDoorReady(DOOR_RIGHT_FRONT);
    case 9:
        return IsDoorReady(DOOR_RIGHT_REAR);
    case 10:
        return IsDoorReady(DOOR_LEFT_FRONT);
    case 11:
        return IsDoorReady(DOOR_LEFT_REAR);
    case 18:
        return true;
    default:
        assert(0); // Prevent usage mistakes - There's is an eDoors overload, and if an int literal is used instead of the enum this function will be called incorrectly.
        return false;
    }
}

// 0x6A22D0
bool CAutomobile::IsDoorFullyOpen(eDoors door)
{
    if (m_doors[door].IsFullyOpen())
        return true;

    if (IsDoorMissing(door))
        return true;

    return false;
}

// 0x6A63E0
bool CAutomobile::IsDoorFullyOpenU32(uint32 door) {
    switch (door) {
    case 8:  return IsDoorFullyOpen(DOOR_RIGHT_FRONT);
    case 9:  return IsDoorFullyOpen(DOOR_RIGHT_REAR);
    case 10: return IsDoorFullyOpen(DOOR_LEFT_FRONT);
    case 11: return IsDoorFullyOpen(DOOR_LEFT_REAR);
    case 18: return false;
    default:
        assert(0); // Prevent usage mistakes - There's is an eDoors overload, and if an int literal is used instead of the enum this function will be called incorrectly.
        return false;
    }
}

// 0x6A2310
bool CAutomobile::IsDoorClosed(eDoors door) {
    return m_doors[door].IsClosed();
}

// 0x6A6470
bool CAutomobile::IsDoorClosedU32(uint32 door) {
    switch (door) {
    case 8:  return IsDoorClosed(DOOR_RIGHT_FRONT);
    case 9:  return IsDoorClosed(DOOR_RIGHT_REAR);
    case 10: return IsDoorClosed(DOOR_LEFT_FRONT);
    case 11: return IsDoorClosed(DOOR_LEFT_REAR);
    case 18: return false;
    default:
        assert(0); // Prevent usage mistakes - There's is an eDoors overload, and if an int literal is used instead of the enum this function will be called incorrectly.
        return false;
    }
}

// 0x6A2330
bool CAutomobile::IsDoorMissing(eDoors door) {
    return m_damageManager.GetDoorStatus(door) == DAMSTATE_NOTPRESENT;
}

// 0x6A6500
bool CAutomobile::IsDoorMissingU32(uint32 door) {
    switch (door) {
    case 8:  return IsDoorMissing(DOOR_RIGHT_FRONT);
    case 9:  return IsDoorMissing(DOOR_RIGHT_REAR);
    case 10: return IsDoorMissing(DOOR_LEFT_FRONT);
    case 11: return IsDoorMissing(DOOR_LEFT_REAR);
    case 18: return true;
    default:
        assert(0); // Prevent usage mistakes - There is an eDoors overload, and if an int literal is used instead of the enum this function will be called incorrectly.
        return false;
    }
}

// 0x6A2350
bool CAutomobile::IsOpenTopCar() {
    return m_nModelIndex == MODEL_COMET    && m_anExtras[0] && m_anExtras[1]
        || m_nModelIndex == MODEL_STALLION && m_anExtras[0] && m_anExtras[1];
}

// 0x6A3030
void CAutomobile::RemoveRefsToVehicle(CEntity* entity) {
    for (auto& ref : m_apWheelCollisionEntity) {
        if (ref == entity)
            ref = nullptr;
    }
}

// NOTSA - Combined implementation of `BlowUpCar` and `BlowUpCarCutSceneNoExtras`
void CAutomobile::BlowUpCar_Impl(CEntity* dmgr, bool bDontShakeCam, bool bDontSpawnStuff, bool bNoExplosion, bool bHideExplosionFx, bool bIsForCutScene, bool bMakeSound) {
    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    const bool bFixBugs = // TODO: Make a function for this in `notsa::`
#ifdef FIX_BUGS
        true;
#else
        false;
#endif

    if (bIsForCutScene) {
        if (IsSubPlane() && GetStatus() != STATUS_PLAYER) {
            switch (m_autoPilot.m_nCarMission) {
            case MISSION_PLANE_FLYTOCOORS:
            case MISSION_PLANE_ATTACK_PLAYER:
            case MISSION_PLANE_FLYINDIRECTION:
            case MISSION_PLANE_FOLLOW_ENTITY:
            case MISSION_PLANE_ATTACK_PLAYER_POLICE:
            case MISSION_PLANE_DOG_FIGHT_ENTITY:
            case MISSION_PLANE_DOG_FIGHT_PLAYER:
                m_autoPilot.SetCarMission(MISSION_PLANE_CRASH_AND_BURN);
                return;
            }
        }
    } else {
        if (dmgr == FindPlayerPed() || dmgr == FindPlayerVehicle()) {
            auto& plyrinfo = FindPlayerInfo();
            plyrinfo.m_nHavocCaused += 20;
            plyrinfo.m_fCurrentChaseValue += 10.f;
            CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)(4000 + CGeneral::GetRandomNumber() % 6000)); // 0x6B37DF: 4000 + rand() % 6000 (an integer-valued stat)
        }
    }

    if (m_nModelIndex == eModelID::MODEL_VCNMAV) {
        CWanted::UseNewsHeliInAdditionToPolice = false;
    }

    if (!bNoExplosion) {
        GetMoveSpeed().z += 0.13f;
    }

    SetStatus(STATUS_WRECKED);

    physicalFlags.bRenderScorched = true;
    m_nTimeWhenBlowedUp      = CTimer::GetTimeInMS();

    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES);
    m_damageManager.FuckCarCompletely(bIsForCutScene && bNoExplosion); // 0x6B3C5C: the cut-scene variant passes bNoExplosion (no wheel is torn off, no rand); 0x6B3851: BlowUpCar passes false

    const auto isRcShit = (bFixBugs || !bIsForCutScene)
        ? notsa::contains({ MODEL_RCTIGER, MODEL_RCBANDIT }, GetModelId()) // I'm 99% sure they forgot to copy paste this too, but let it be.
        : GetModelId() == MODEL_RCBANDIT;
    if (!isRcShit) { // 0x6B3C61
        for (auto bumper : { FRONT_BUMPER, REAR_BUMPER }) {
            SetBumperDamage(bumper, bDontSpawnStuff);
        }
        for (auto door : { DOOR_BONNET, DOOR_BOOT, DOOR_LEFT_FRONT, DOOR_RIGHT_FRONT, DOOR_LEFT_REAR, DOOR_RIGHT_REAR }) {
            SetDoorDamage(door, bDontSpawnStuff);
        }
        if (!bDontSpawnStuff) {
            SpawnFlyingComponent(CAR_WHEEL_LF, 1);
        }
        if (!bNoExplosion) {
            if (const auto obj = GetCurrentAtomicObject(m_aCarNodes[CAR_WHEEL_LF])) {
                RpAtomicSetFlags(obj, 0); // TODO: Use appropriate enum (if any?)
            }
        }
    }

    m_nBombOnBoard = 0;
    m_fHealth      = 0.f;
    m_wBombTimer   = 0;

    if (!bDontShakeCam) {
        TheCamera.CamShake(0.4f, GetPosition());
    }

    KillPedsInVehicle();
    if (bFixBugs || !bIsForCutScene) { // Originally not in `BlowUpCarCutSceneNoExtras`, but they probably just forgot to copy paste it
        KillPedsGettingInVehicle();  
    }

    vehicleFlags.bLightsOn     = false;
    vehicleFlags.bEngineOn     = false;
    vehicleFlags.bSirenOrAlarm = false;
    m_nOverrideLights          = NO_CAR_LIGHT_OVERRIDE;
    autoFlags.bTaxiLight     = false;

    if (vehicleFlags.bIsAmbulanceOnDuty) { //> 0x6B3DAE
        assert(!vehicleFlags.bIsFireTruckOnDuty);
        vehicleFlags.bIsAmbulanceOnDuty = false;
        CCarCtrl::NumAmbulancesOnDuty--;
    }

    if (vehicleFlags.bIsFireTruckOnDuty) {
        assert(!vehicleFlags.bIsAmbulanceOnDuty);
        vehicleFlags.bIsFireTruckOnDuty = false;
        CCarCtrl::NumFireTrucksOnDuty--;
    }

    ChangeLawEnforcerState(false);

    if (!bNoExplosion) {
        if (bIsForCutScene) { // 0x6B3DEA: Darkel first, then the fire
            CDarkel::RegisterCarBlownUpByPlayer(*this, 0); // Um.... Okay
            gFireManager.StartFire(this, dmgr);
        } else { // 0x6B39DF: the fire first, then Darkel
            gFireManager.StartFire(this, dmgr);
            CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
        }

        if (bIsForCutScene) { //> 0x6B3E0D
            CExplosion::AddExplosion(this, dmgr, EXPLOSION_MOLOTOV, GetPosition(), 0, bMakeSound, -1.f, bHideExplosionFx);
        } else { //> 0x6B3A17 - Add explosion fx (cancer inducing code)
            const auto tcm = GetColModel();
            const auto maxOffset = IsSubAutomobile() || IsSubQuad()
                ? 0.75f
                : 0.1f;
            const auto RandomOffset = [&]() { return CGeneral::GetRandomNumberInRange(-maxOffset, maxOffset); };
            const auto explOffset = CVector{
                RandomOffset(),
                RandomOffset(),
                CGeneral::GetRandomNumberInRange(0.f, 1.f) + 0.5f // 0x6B3B0A: the 3rd rand() draw, rand() / 32767 + 0.5 (not maxOffset + 0.5)
            } * GetColModel()->GetBoundingBox().m_vecMax;
            const auto explPos = // Do a matrix transform... manually, because that's fun!
                  GetPosition()
                + GetRight() * explOffset.x
                + GetForward() * explOffset.y
                - CVector{0.f, 0.f, explOffset.z};
            CExplosion::AddExplosion(
                this,
                dmgr,
                isRcShit ? EXPLOSION_QUICK_CAR : EXPLOSION_CAR,
                explPos,
                0,
                bMakeSound,
                -1.f,
                bHideExplosionFx
            );
        }
    }
}

// 0x6B3780
void CAutomobile::BlowUpCar(CEntity* dmgr, bool bHideExplosionFx) {
    BlowUpCar_Impl(dmgr, false, false, false, bHideExplosionFx, false, true);
}

// 0x6B3BB0
void CAutomobile::BlowUpCarCutSceneNoExtras(bool bDontShakeCam, bool bDontSpawnStuff, bool bNoExplosion, bool bMakeSound) {
    BlowUpCar_Impl(nullptr, bDontShakeCam, bDontSpawnStuff, bNoExplosion, false, true, bMakeSound);
}

// 0x6A3060
bool CAutomobile::SetUpWheelColModel(CColModel* wheelCol) {
    const auto mi = GetVehicleModelInfo();
    const auto wcm = GetColModel();
    const auto wcd = wheelCol->m_pColData;

    if (m_nModelIndex == MODEL_TRACTOR || m_nModelIndex == MODEL_COMBINE || m_nModelIndex == MODEL_KART)
        return false;

    wheelCol->m_boundBox = wcm->m_boundBox;
    wheelCol->m_boundSphere = wcm->m_boundSphere;

    CMatrix mat;

    mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LF]), false);
    wcd->m_pSpheres[0].Set(mi->m_fWheelSizeFront / 2.0f, mat.GetPosition(), SURFACE_RUBBER, 0xD);

    mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LB]), false);
    wcd->m_pSpheres[1].Set(mi->m_fWheelSizeRear / 2.0f, mat.GetPosition(), SURFACE_RUBBER, 0xF);

    mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RF]), false);
    wcd->m_pSpheres[2].Set(mi->m_fWheelSizeFront / 2.0f, mat.GetPosition(), SURFACE_RUBBER, 0xE);

    mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RB]), false);
    wcd->m_pSpheres[3].Set(mi->m_fWheelSizeRear / 2.0f, mat.GetPosition(), SURFACE_RUBBER, 0x10);

    if (m_aCarNodes[CAR_WHEEL_LM] && m_aCarNodes[CAR_WHEEL_RM]) {
        mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_LM]), false);
        wcd->m_pSpheres[4].Set(mi->m_fWheelSizeRear / 2.0f, mat.GetPosition(), SURFACE_RUBBER, 0xF);

        mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_WHEEL_RM]), false);
        wcd->m_pSpheres[5].Set(mi->m_fWheelSizeRear / 2.0f, mat.GetPosition(), SURFACE_RUBBER, 0x10);

        wcd->m_nNumSpheres = 6;
    } else {
        wcd->m_nNumSpheres = 4;
    }
    return true;
}

// 0x6A32B0
bool CAutomobile::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    if (m_nModelIndex == MODEL_RHINO)
        return false;

    if (vehicleFlags.bTyresDontBurst)
        return false;

    if (physicalFlags.bRenderScorched)
        return false;

    const auto GetCarWheel = [&]() -> eCarWheel { // TODO: `CarPieceToCarWheel` (See `CarWheelToCarPiece`)
        switch (tyreComponentId) {
        case CAR_PIECE_WHEEL_LF: return CAR_WHEEL_FRONT_LEFT;
        case CAR_PIECE_WHEEL_RF: return CAR_WHEEL_FRONT_RIGHT;
        case CAR_PIECE_WHEEL_RL: return CAR_WHEEL_REAR_LEFT;
        case CAR_PIECE_WHEEL_RR: return CAR_WHEEL_REAR_RIGHT;
        default:
            return (eCarWheel)tyreComponentId; // TODO: NOTSA_UNREACHABLE()
        }
    };

    auto wheel = GetCarWheel();
    if (m_damageManager.GetWheelStatus(wheel))
        return false;

    m_damageManager.SetWheelStatus(wheel, WHEEL_STATUS_BURST);
    CStats::IncrementStat(STAT_TIRES_POPPED_WITH_GUNFIRE, 1.0f);
    m_vehicleAudio.AddAudioEvent(AE_TYRE_BURST, 0.0f);
    if (GetStatus() == STATUS_SIMPLE) {
        CCarCtrl::SwitchVehicleToRealPhysics(this);
    }

    constexpr auto force = 0.03f;
    if (bPhysicalEffect) {
        ApplyMoveForce(m_matrix->GetRight() * CGeneral::GetRandomNumberInRange(-force, force) * m_fMass);
        ApplyTurnForce(
            m_matrix->GetRight() * CGeneral::GetRandomNumberInRange(-force, force) * m_fTurnMass,
            m_matrix->GetForward()
        );

        m_nFakePhysics = 0;
    }
    return true;
}

// 0x6A3850
bool CAutomobile::IsRoomForPedToLeaveCar(uint32 arg0, CVector* arg1) {
    return true;
}

// 0x6A65D0
void CAutomobile::SetupSuspensionLines() {
    const auto& mi = *GetVehicleModelInfo();
          auto& cm = *mi.GetColModel();
          auto& cd = *cm.m_pColData;

    // 0x6A65FB
    const bool hadToAllocateLines = !cd.m_nNumLines;
    if (hadToAllocateLines) {
        cd.AllocateLines(ModelIndices::IsRhino(m_nModelIndex) ? NUM_RHINO_SUSP_LINES : NUM_AUTOMOBILE_SUSP_LINES);
    }

    // 0x6A6754
    // Calculate col line positions for wheels, set wheel explPos, spring and line lengths
    const auto& handling = *m_pHandlingData;
    for (auto i = 0; i < NUM_AUTOMOBILE_SUSP_LINES; i++) {
        auto& colLine = cd.m_pLines[i]; // Collision line for this wheel

        CVector wheelPos;
        mi.GetWheelPosn(i, wheelPos, false);

        if (IsSubQuad()) {
            if (wheelPos.x <= 0.f)
                wheelPos.x -= 0.15f;
            else
                wheelPos.x += 0.15f;
        }

        colLine.m_vecStart = CVector{ wheelPos.x, wheelPos.y, wheelPos.z + handling.m_fSuspensionUpperLimit };
        // 0x6A66D8-0x6A670B: end.z = ((lower - upper) - size / 2) + start.z
        colLine.m_vecEnd   = CVector{ wheelPos.x, wheelPos.y, ((handling.m_fSuspensionLowerLimit - handling.m_fSuspensionUpperLimit) - mi.GetSizeOfWheel((eCarWheel)i) / 2.f) + colLine.m_vecStart.z };

        m_wheelPosition[i]           = wheelPos.z; // Originally set earlier in the loop, but should be fine.
        m_aSuspensionSpringLength[i] = handling.m_fSuspensionUpperLimit - handling.m_fSuspensionLowerLimit;
        m_aSuspensionLineLength[i]   = colLine.m_vecStart.z - colLine.m_vecEnd.z;
    }

    /* NOP code - 0x6A6764
    if (GetVehicleAppearance() != eVehicleAppearance::VEHICLE_APPEARANCE_PLANE) {
        GetVehicleAppearance();
    }*/

    const auto CalculateHeightAboveRoad = [&](eCarWheel wheel) {
        // 0x6A6773-0x6A67AC: spring * (1 - 1 / (level * 4)) + (size / 2 - start.z)
        return (1.f - 1.f / (handling.m_fSuspensionForceLevel * 4.f)) * m_aSuspensionSpringLength[(size_t)wheel] + (mi.GetSizeOfWheel(wheel) / 2.f - cd.m_pLines[(size_t)wheel].m_vecStart.z);
    };

    // 0x6A65D0
    m_fFrontHeightAboveRoad = CalculateHeightAboveRoad(eCarWheel::CAR_WHEEL_FRONT_LEFT);
    m_fRearHeightAboveRoad = CalculateHeightAboveRoad(eCarWheel::CAR_WHEEL_REAR_RIGHT); // 0x6A67B2-0x6A67E3: spring length [3] and line [3] (not the rear left)

    // 0x6A681A
    // Adjust wheel's position based on height above road
    for (auto i = 0u; i < std::size(m_wheelPosition); i++) {
        m_wheelPosition[i] = mi.GetSizeOfWheel((eCarWheel)i) / 2.f - m_fFrontHeightAboveRoad; // Not sure why it uses front height for all wheels?
    }

    // 0x6A681C
    // 0x6A681C..0x6A68B4 (exe asm): the bounding box is lowered to the first suspension line end, then the sphere radius is raised to
    // max(radius, |min|, |max|) - the radius part is NOT inside the if. Without the lowered min.z, GetDistanceFromCentreOfMassToBaseOfModel() (and every
    // spawn height of a car) is too small (REPLAY-DIFF A-B, frame 30: Greenwood dist 0.5 instead of 0.9).
    if (cd.m_pLines[0].m_vecEnd.z < cm.m_boundBox.m_vecMin.z) {
        cm.m_boundBox.m_vecMin.z = cd.m_pLines[0].m_vecEnd.z;
    }
    cm.m_boundSphere.m_fRadius = std::max({ cm.m_boundSphere.m_fRadius, cm.m_boundBox.m_vecMin.Magnitude(), cm.m_boundBox.m_vecMax.Magnitude() });

    // 0x6A68C1
    // Adjust sphere sizes for rcbandit
    if (ModelIndices::IsRCBandit(m_nModelIndex)) {
        cm.m_boundSphere.m_fRadius = 2.f;

        for (auto&& sph : cd.GetSpheres()) {
            sph.m_fRadius = 0.3f;
        }
    }

    // 0x6A68FD
    if (handling.m_bForceGroundClearance && hadToAllocateLines) {
        const auto sphBottomMinZ = (ModelIndices::IsKart(m_nModelIndex) ? 0.12f : 0.25f) - m_fFrontHeightAboveRoad;
        for (auto&& sph : cd.GetSpheres()) {
            const auto sphBottomZ = sph.m_vecCenter.z - sph.m_fRadius;
            if (sphBottomZ < sphBottomMinZ) {
                if (sph.m_fRadius > 0.4f) {
                    // 0x6A6958-0x6A6987: the radius becomes max(0.4, centre.z - minBottom), the centre is minBottom + radius
                    const auto newRadius = sph.m_vecCenter.z - sphBottomMinZ;
                    sph.m_fRadius = newRadius >= 0.4f ? newRadius : 0.4f;
                    sph.m_vecCenter.z = sphBottomMinZ + sph.m_fRadius;
                }
            }
        }
    }

    // 0x6A6995
    // Rhino has 12 suspension lines, which are calculated based on the 4 main wheel's line's position
    if (ModelIndices::IsRhino(m_nModelIndex)) {
        size_t lineIndex{ NUM_AUTOMOBILE_SUSP_LINES }; // Skip first 4 lines as those are already in use for the real wheels

        for (auto i = 0u; i < NUM_AUTOMOBILE_SUSP_LINES; i += NUM_AUTOMOBILE_SUSP_LINES / 2) { // Do left, and right side - Wheels 0, 1 are on the left side, while 2, 3 are on the right
            for (auto j = 0u; j < NUM_AUTOMOBILE_SUSP_LINES; j++, lineIndex++) { // Add 4 extra lines on each side
                const auto wheelRelativePos = (float)(j + 1u) * 0.2f; // 0x858CC4 = 1 / (NUM_AUTOMOBILE_SUSP_LINES + 1) as float, the exe MULTIPLIES

                // Calculate positions of this wheel's line by lerping between the 2 wheel's lines on this side
                auto& line      = cd.m_pLines[lineIndex];
                line.m_vecStart = lerpBlend(cd.m_pLines[i].m_vecStart, cd.m_pLines[i + 1].m_vecStart, wheelRelativePos);
                line.m_vecEnd   = lerpBlend(cd.m_pLines[i].m_vecEnd, cd.m_pLines[i + 1].m_vecEnd, wheelRelativePos);
            }
        }
    }
}

// 0x6A3440
void CAutomobile::Fix() {
    m_damageManager.ResetDamageStatus();

    // Reset actual doorIdx's status
    // 0x6A3477: `test ch, 1` on handling+0xCC (the MODEL flags, bit 8 = m_bNoDoors), NOT the handling flags (m_bWheelFNarrow2) at +0xD0
    if (m_pHandlingData->m_bNoDoors) {
        m_damageManager.SetDoorStatus({ DOOR_LEFT_FRONT, DOOR_RIGHT_FRONT, DOOR_LEFT_REAR, DOOR_RIGHT_REAR }, eDoorStatus::DAMSTATE_NOTPRESENT);
    }

    vehicleFlags.bIsDamaged = false;

    // Hide all DAM state atomics
    RpClumpForAllAtomics(GetRpClump(), CVehicleModelInfo::HideAllComponentsAtomicCB, (void*)eAtomicComponentFlag::ATOMIC_DAMAGED);

    // Reset rotation of some nodes
    for (auto i = (size_t)CAR_DOOR_RF; i < (size_t)CAR_NUM_NODES; i++) {
        if (auto frame = m_aCarNodes[i]) {
            CMatrix mat{ RwFrameGetMatrix(frame), false };
            mat.SetTranslate(mat.GetPosition());
            mat.UpdateRW();
        }
    }

    // Set all wheels
    m_damageManager.SetAllWheelsState(eCarWheelStatus::WHEEL_STATUS_OK);

    // Reset all bouncing panels
    for (auto&& panel : m_panels) {
        panel.ResetPanel();
    }

    // 0x6A355E: the exe loops 4 times (stride 0x20 from +0x6AC, `mov word [eax], 0xFFFF`), the 4th hit lands on +0x70C = the low half of
    // m_swingingChassis.m_openAngle (original quirk, replicated)
    VALIDATE_OFFSET(CAutomobile, m_swingingChassis, 0x70C); // x86 layout only (a plain offsetof static_assert breaks the x64 portable builds)
    *reinterpret_cast<uint16*>(&m_swingingChassis.m_openAngle) = 0xFFFF;
}

// 0x6B3E90
void CAutomobile::SetupDamageAfterLoad() {
    // TODO: Moderinfy....

    if (m_aCarNodes[CAR_BUMP_FRONT]) SetBumperDamage(FRONT_BUMPER, false);
    if (m_aCarNodes[CAR_BUMP_REAR])  SetBumperDamage(REAR_BUMPER, false);

    if (m_aCarNodes[CAR_BONNET])  SetDoorDamage(DOOR_BONNET, false);
    if (m_aCarNodes[CAR_BOOT])    SetDoorDamage(DOOR_BOOT, false);
    if (m_aCarNodes[CAR_DOOR_LF]) SetDoorDamage(DOOR_LEFT_FRONT, false);
    if (m_aCarNodes[CAR_DOOR_RF]) SetDoorDamage(DOOR_RIGHT_FRONT, false);
    if (m_aCarNodes[CAR_DOOR_LR]) SetDoorDamage(DOOR_LEFT_REAR, false);
    if (m_aCarNodes[CAR_DOOR_RR]) SetDoorDamage(DOOR_RIGHT_REAR, false);

    if (m_aCarNodes[CAR_WING_LF]) SetPanelDamage(FRONT_LEFT_PANEL, false);
    if (m_aCarNodes[CAR_WING_RF]) SetPanelDamage(FRONT_RIGHT_PANEL, false);
}

// 0x6A47F0
void CAutomobile::DoBurstAndSoftGroundRatios()
{
    const auto& mi = *GetVehicleModelInfo();
    const auto speedToFwdRatio = std::fabs(DotProduct(m_vecMoveSpeed, m_matrix->GetForward())); // Remember: Dot product is 0 when the two vectors are perpendicular - TODO: Rename var to fwdvel (forward velocity)

    for (auto i = 0u; i < 4; i++) {
        const auto& wheelCP = m_wheelColPoint[i];
        const auto& springLen = m_aSuspensionSpringLength[i];
        const auto& lineLen = m_aSuspensionLineLength[i];
        auto& compression = m_fWheelsSuspensionCompression[i];
        auto& rotation = m_wheelRotation[i];

        const auto GetRemainingSuspensionCompression = [&] {
            return (lineLen - springLen) / lineLen;
        };

        switch (m_damageManager.GetWheelStatus((eCarWheel)i)) {
        case eCarWheelStatus::WHEEL_STATUS_MISSING:
            compression = 1.0f;
            break;
        case eCarWheelStatus::WHEEL_STATUS_BURST: {
            // The more opposite the speedsq is to the forward vector the bigger chance
            // The highest chance is when the speedsq is opposite to forward (ie.: It's backwards)
            const auto val = CGeneral::GetRandomNumberInRange(0, NOTSA_FTOL(int32, speedToFwdRatio * 40.0f) + 98);
            if (val < 100) {
                compression += GetRemainingSuspensionCompression() / 4.f;
                compression = std::min(1.0f, compression);
            }
            break;
        }
        default: {
            if (   compression >= 1.0f
                || g_surfaceInfos.GetAdhesionGroup(wheelCP.m_nSurfaceTypeB) != ADHESION_GROUP_SAND
                || ModelIndices::IsRhino(m_nModelIndex)
            ) {
                if (compression < 1.0f && wheelCP.m_nSurfaceTypeB == SURFACE_RAILTRACK) {
                    auto wheelSizeFactor = 1.5f / (mi.GetSizeOfWheel((eCarWheel)i) / 2.0f);
                    if (speedToFwdRatio > 0.3f) {
                        wheelSizeFactor *= speedToFwdRatio / 0.3f;
                    }

                    auto wheelSizeInv = 1.0f / wheelSizeFactor;
                    auto wheelRotFactor = wheelSizeInv * rotation;
                    auto wheelRotFactorFract = wheelRotFactor - std::floor(wheelRotFactor);

                    auto timeSpeedRotFactor = (CTimer::GetTimeStep() * m_wheelSpeed[i] + rotation) * wheelSizeInv;
                    auto timeSpeedRotFactorFract = timeSpeedRotFactor - std::floor(timeSpeedRotFactor);

                    if (   m_wheelSpeed[i] > 0.0f && timeSpeedRotFactorFract < wheelRotFactorFract
                        || m_wheelSpeed[i] < 0.0f && timeSpeedRotFactorFract > wheelRotFactorFract
                    ) {
                        compression = std::max(0.2f, compression - GetRemainingSuspensionCompression() * 0.3f);
                    }
                }
            } else {
                float offroadFactor = 0.3f;
                if (handlingFlags.bOffroadAbility2) {
                    offroadFactor = 0.15f;
                } else if (handlingFlags.bOffroadAbility) {
                    offroadFactor = 0.2f;
                }

                auto adhesionFactor = 1.0f - speedToFwdRatio / 0.3f * 0.7f - CWeather::WetRoads * 0.7f;
                adhesionFactor = std::max(0.4f, adhesionFactor);

                compression += offroadFactor * GetRemainingSuspensionCompression() * adhesionFactor;
                compression = std::min(compression, 1.0f);
            }
            break;
        }
        }
    }
}

// 0x6A3770
void CAutomobile::PlayCarHorn()
{
    if (m_nAlarmState && m_nAlarmState != 0xFFFF && GetStatus() != STATUS_WRECKED || m_HornCounter) {
        return;
    }

    if (m_nCarHornTimer) {
        m_nCarHornTimer -= 1;
        return;
    }

    m_nCarHornTimer = CGeneral::GetRandomNumber() % 128 - 106; // TODO: GetRandomNumberInRange
    if (const auto r = m_nCarHornTimer % 8; r < 4) {
        if (r >= 2) {
            if (m_pDriver && m_autoPilot.carCtrlFlags.bHonkAtCar) {
                m_pDriver->Say(CTX_GLOBAL_BLOCKED);
            }
        }
        m_HornCounter = 45;
    } else {
        if (m_pDriver) {
            m_pDriver->Say(CTX_GLOBAL_BLOCKED);
        }
    }
}

// 0x6A62B0
float CAutomobile::GetHeightAboveRoad() {
    return m_fFrontHeightAboveRoad;
}

// 0x6A62A0
int32 CAutomobile::GetNumContactWheels() {
    return m_nNumContactWheels;
}

// 0x6A7650
void CAutomobile::VehicleDamage(float damageIntensity, eVehicleCollisionComponent collisionComponent, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    assert(this);

    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    assert(m_matrix);

    float minDmgIntensity{ 25.f };
    float collForceMult{ 0.333f }; // 0x864E30 (0x3EAA7EFA, NOT 1/3)
    float calcDmgIntensity{ damageIntensity };

    if (damageIntensity == 0.f) {
        // Man this is so stupid..
        vecCollisionDirection = &m_vecLastCollisionImpactVelocity;
        vecCollisionCoors     = &m_vecLastCollisionPosn;
        damager               = m_pDamageEntity;
        collisionComponent    = (eVehicleCollisionComponent)m_nPieceType;

        collForceMult = 1.f;
        minDmgIntensity  = m_fMass * ExeRecip(1500.f) * 25.f;
        calcDmgIntensity = m_fDamageIntensity;

        // 0x6A7705
        if (m_matrix->GetUp().z < 0.f/*is flipped*/ && this != FindPlayerVehicle()) {
            if (autoFlags.bDoesNotGetDamagedUpsideDown) {
                return;
            }
            switch (GetStatus()) {
            case eEntityStatus::STATUS_REMOTE_CONTROLLED: {
                if (physicalFlags.bSubmergedInWater)
                    return;
                break;
            }
            case eEntityStatus::STATUS_WRECKED:
                break;
            default: {
                m_fHealth = std::max(0.f, m_fHealth - CTimer::GetTimeStep() * 4.f);
                break;
            }
            }
        }

        // 0x6A7780
        if (m_fDamageIntensity == 0.f || physicalFlags.bCollisionProof) {
            return;
        }

        if (IsSubQuad()) {
            if (CBike::DamageKnockOffRider(this, m_fDamageIntensity, m_nPieceType, m_pDamageEntity, m_vecLastCollisionPosn, m_vecLastCollisionImpactVelocity)) {
                return;
            }
        }

        // 0x6A77F0
        const auto lastImpactVel_Dot_Fwd = DotProduct(m_matrix->GetForward(), m_vecLastCollisionImpactVelocity);

        // 0x6A77D6
        if (GetStatus() == STATUS_PHYSICS
            && !IsMissionVehicle()
            && lastImpactVel_Dot_Fwd < -0.4f // Impact was from behind - (-66, 66) deg
            && m_fDamageIntensity / m_fMass > 0.1f
        ) {
            m_autoPilot.SetTempAction(TEMPACT_HEADON_COLLISION, 4'000);
        }

        if (m_pDamageEntity) {
            // Inverted the if's a little, and used switch case to make it nicer.

            switch (m_pDamageEntity->GetType()) {
            case eEntityType::ENTITY_TYPE_VEHICLE: {
                // 0x6A7856
                if (!CGame::CanSeeOutSideFromCurrArea()
                    && std::abs(lastImpactVel_Dot_Fwd) > 0.4 // Sideways impact - (142, 66) deg
                ) {
                    calcDmgIntensity /= 3.f;
                }
                break;
            }
            case eEntityType::ENTITY_TYPE_PED: {
                // 0x6A7883
                if (const auto& ped = *m_pDamageEntity->AsPed(); ped.bIsStanding) {
                    const auto impactDir = DotProduct(ped.m_matrix->GetForward() * ped.m_vecAnimMovingShift.y, m_vecLastCollisionImpactVelocity);
                    if (impactDir < 0.f) { // From behind - (-90, 90) deg
                        calcDmgIntensity = std::max(0.f, calcDmgIntensity + impactDir * ped.m_fMass);
                    }
                }
                break;
            }
            }
        }
    } else {
        // 0x6A7BE5
        if (bool unused{}; !CanVehicleBeDamaged(damager, weapon, unused)) {
            return;
        }

        collForceMult = 1.f;
    }

    // 0x6A78F0
    if (   damager && damager->GetIsTypeBuilding()
        && DotProduct(*vecCollisionDirection, m_matrix->GetUp()) > 0.6f // In front - (-66, 66) deg
    ) {
        return;
    }

    // 0x6A792B
    if (GetStatus() == STATUS_PLAYER) {
        if (CStats::GetPercentageProgress() >= 100.f) {
            calcDmgIntensity /= 2.f;
        }
    } else {
        if (   physicalFlags.bInvulnerable
            && damager
            && damager != FindPlayerPed()
            && damager != FindPlayerVehicle()
        ) {
            return;
        }
    }

    // 0x6A7984
    if (damager && (damager == m_pTowingVehicle || damager == m_pVehicleBeingTowed)) {
        return;
    }

    // 0x6A79AB (Condition inverted)
    if (calcDmgIntensity > minDmgIntensity && GetStatus() != STATUS_WRECKED) {
        // NOTSA_LOG_DEBUG("calcDmgIntensity: {.2f}", calcDmgIntensity); // NOTSA
        // 0x6A79C7
        // If we're a law enforcer, and the dmgr is the
        // player's plyrveh increase their wanted level.
        if (vehicleFlags.bIsLawEnforcer) {
            const auto vehicle = FindPlayerVehicle();
            if (vehicle && damager == vehicle && GetStatus() != STATUS_ABANDONED) {
                // R* used magnitude, but squared magnitude is more suitable.
                const auto speedMag = vehicle->m_vecMoveSpeed.SquaredMagnitude();
                if (   speedMag >= m_vecMoveSpeed.SquaredMagnitude()
                    && speedMag > sq(0.1f)
                ) {
                    FindPlayerPed()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1);
                }
            }
        }

        // 0x6A7A6E
        // Some shaky-shaky
        if (GetStatus() == STATUS_PLAYER && calcDmgIntensity > 50.f) {
            const auto intensity = std::min(250.f, calcDmgIntensity / m_fMass * 800.f + 100.f);
            CPad::GetPad()->StartShake(40000 / NOTSA_FTOL(uint8, intensity), NOTSA_FTOL(uint8, intensity), 2000u);
        }

        // 0x6A7ACF
        if (damager && damager->GetIsTypeVehicle()) {
            m_nLastWeaponDamageType = WEAPON_RAMMEDBYCAR;
            m_pLastDamageEntity = damager;
            damager->RegisterReference(&m_pLastDamageEntity);
        }

        // Store light states, so later we can determine if any was blown
        // and play the fx sound
        const auto prevLightStates = m_damageManager.GetAllLightsState();

        if (GetStatus() == STATUS_PLAYER) {
            FindPlayerInfo().m_nVehicleTimeCounter = CTimer::GetTimeInMS();
        }

        // 0x6A7B63
        const auto moveSpeedMagSq = m_vecMoveSpeed.SquaredMagnitude();
        if (moveSpeedMagSq > 0.02f / 50.0f) {
            dmgDrawCarCollidingParticles(*vecCollisionCoors, calcDmgIntensity * collForceMult, weapon);
        }

        if (m_matrix->GetUp().z > 0.f /*Not flipped on roof*/ || moveSpeedMagSq > 0.3f) {
            #define ApplyDamageToComponent(component) m_damageManager.ApplyDamage(this, component, calcDmgIntensity * 4.0f, m_pHandlingData->m_fCollisionDamageMultiplier)

            // Returned value:
            // [-oo, 0] - Left side (In bounding box if: [-1, 0] )
            // [0, +oo] - Right side (In bounding box if: [0, 1] )
            const auto GetCollisionPointLocalDirection = [&, this] {
                const auto collDirDotRight = DotProduct(*vecCollisionCoors - GetPosition() , m_matrix->GetRight());

                // Since the dot product is scaled by the distance of the collision point from us
                // If we divide it by the width of our bounding box we can determinate how far
                // the point is:
                // [-oo, 0] - Left side (In bounding box if: [-1, 0] )
                // [0, +oo] - Right side (In bounding box if: [0, 1] )
            #ifdef FIX_BUGS
                return collDirDotRight / (GetColModel()->m_boundBox.GetWidth() / 2.f);
            #else
                // Must be positive, otherwise calculation below might not work (Adding abs might help?)
                // Also, here they rely on the fact that all bounding boxes are symmetrical split by the Y axis
                // Which is true for all plyrveh models in the vanilla SA (AFAIK)
                // But in order to prevent weird bugs we gotta do it the right way.
                assert(GetColModel()->m_boundBox.m_vecMax.x > 0.f);
                return collDirDotRight / GetColModel()->m_boundBox.m_vecMax.x;
            #endif
            };

            switch (collisionComponent) {
            case eVehicleCollisionComponent::BONNET:
                ApplyDamageToComponent(tComponent::COMPONENT_BONNET);
                break;
            case eVehicleCollisionComponent::BOOT:
                ApplyDamageToComponent(tComponent::COMPONENT_BOOT);
                break;
            case eVehicleCollisionComponent::BUMP_FRONT: {
                ApplyDamageToComponent(tComponent::COMPONENT_BUMP_FRONT);

                const auto localDir       = GetCollisionPointLocalDirection();
                const auto colDirDotRight = DotProduct(*vecCollisionDirection, m_matrix->GetRight());

                // Possibly apply right/left wing damage based on direction
                if (   localDir > 0.7f // [0, 45] deg
                    || localDir > 0.5f && colDirDotRight < -0.5f && m_fMass * 0.35f < calcDmgIntensity
                ) {
                    ApplyDamageToComponent(tComponent::COMPONENT_WING_RF);
                } else if (
                       localDir > -0.7f // [90, 135] deg
                    || localDir > -0.5f && colDirDotRight > 0.5f && m_fMass * 0.35f < calcDmgIntensity
                ) {
                    ApplyDamageToComponent(tComponent::COMPONENT_WING_LF);
                }

                // Possibly apply bonnet damage if front bumper is not `ok`
                if (   m_damageManager.GetPanelStatus(ePanels::FRONT_BUMPER) != DAMSTATE_OK && m_fMass * 0.2f < calcDmgIntensity
                    || weapon < WEAPON_LAST_WEAPON && (CGeneral::GetRandomNumber() % 3 == 0) // If it was a weapon, apply damage by randomly
                ) {
                    ApplyDamageToComponent(tComponent::COMPONENT_BONNET);
                }

                // Possibly apply windscreen damage if front bumper is `damaged`
                if (m_damageManager.GetPanelStatus(ePanels::FRONT_BUMPER) >= DAMSTATE_DAMAGED && m_fMass * 0.2f < calcDmgIntensity) {
                    ApplyDamageToComponent(tComponent::COMPONENT_WINDSCREEN);
                }
                break;
            }
            case eVehicleCollisionComponent::BUMP_REAR: {
                ApplyDamageToComponent(tComponent::COMPONENT_BUMP_REAR);

                const auto localDir = GetCollisionPointLocalDirection();

                // If this is a van possibly apply damage to the rear doors
                if (vehicleFlags.bIsVan && m_fMass * 0.35f < calcDmgIntensity) {
                    if (localDir > 0.1f) { // [-90, -85] deg
                        ApplyDamageToComponent(tComponent::COMPONENT_DOOR_RR);
                    } else if (localDir < -0.1f) { // [-95, -90]
                        ApplyDamageToComponent(tComponent::COMPONENT_DOOR_LR);
                    }
                }

                // Possibly apply damage to boot if not already damaged
                if (m_damageManager.GetPanelStatus(ePanels::REAR_BUMPER) < DAMSTATE_DAMAGED) {
                    ApplyDamageToComponent(tComponent::COMPONENT_BOOT);
                }


                break;
            }
            case eVehicleCollisionComponent::DOOR_LF:
                ApplyDamageToComponent(tComponent::COMPONENT_DOOR_LF);
                break;
            case eVehicleCollisionComponent::DOOR_RF:
                ApplyDamageToComponent(tComponent::COMPONENT_DOOR_RF);
                break;
            case eVehicleCollisionComponent::DOOR_LR:
                ApplyDamageToComponent(tComponent::COMPONENT_DOOR_LR);
                break;
            case eVehicleCollisionComponent::DOOR_RR:
                ApplyDamageToComponent(tComponent::COMPONENT_DOOR_RR);
                break;
            case eVehicleCollisionComponent::WING_LF:
                ApplyDamageToComponent(tComponent::COMPONENT_WING_LF);
                break;
            case eVehicleCollisionComponent::WING_RF:
                ApplyDamageToComponent(tComponent::COMPONENT_WING_RF);
                break;
            case eVehicleCollisionComponent::WINDSCREEN:
                ApplyDamageToComponent(tComponent::COMPONENT_WINDSCREEN);
                break;
            case eVehicleCollisionComponent::DEFAULT:
            case eVehicleCollisionComponent::WING_LR:
            case eVehicleCollisionComponent::WING_RR:
                break; // NOTSA
            }
        }
        #undef ApplyDamageToComponent

        // 0x6A8052
        auto calcCollHealthLoss = (calcDmgIntensity - minDmgIntensity) * m_pHandlingData->m_fCollisionDamageMultiplier * 0.6f * collForceMult;

        switch (m_nModelIndex) {
        case eModelID::MODEL_SECURICA: {
            if (m_pDamageEntity && m_pDamageEntity->GetStatus() == STATUS_PLAYER) {
                calcCollHealthLoss *= 7.f;
            }
            break;
        }
        case eModelID::MODEL_RCRAIDER:
        case eModelID::MODEL_RCGOBLIN:
        case eModelID::MODEL_RCTIGER: {
            calcCollHealthLoss *= 30.f;
            break;
        }
        }

        if (m_pDamageEntity && ModelIndices::IsRhino(m_pDamageEntity->m_nModelIndex)) {
            calcCollHealthLoss *= 15.f;
        }

        // 0x6A80E1
        if (calcCollHealthLoss > 0.f) {
            if (calcCollHealthLoss > 5.f) {
                if (m_pDamageEntity) {
                    if (m_pDamageEntity->GetIsTypeVehicle()) {
                        const auto& damagedVeh = *m_pDamageEntity->AsVehicle();

                        if (damagedVeh.m_pDriver && m_pDriver) {
                            // Add collision event
                            {
                                const auto dot = DotProduct(
                                    m_pDamageEntity->AsPhysical()->m_vecMoveSpeed - m_vecMoveSpeed,
                                    m_vecLastCollisionImpactVelocity
                                );
                                if (dot < 0.f) { // Forces are opposite
                                    CEventVehicleDamageCollision event{ this, m_pDamageEntity->AsVehicle(), WEAPON_RAMMEDBYCAR };
                                    m_pDriver->GetEventGroup().Add(&event);
                                } else {
                                    CEventVehicleDamageCollision event{ m_pDamageEntity->AsVehicle(), this, WEAPON_RAMMEDBYCAR };
                                    m_pDamageEntity->AsVehicle()->m_pDriver->GetEventGroup().Add(&event);
                                }
                            }

                            // Make our driver say something reacting to the collision..
                            if (&damagedVeh != FindPlayerVehicle() || damagedVeh.IsMissionVehicle()) {
                                switch (m_vehicleAudio.GetVehicleTypeForAudio()) {
                                case eAEVehicleAudioType::BIKE:     m_pDriver->Say(CTX_GLOBAL_CRASH_BIKE);    break;
                                case eAEVehicleAudioType::CAR:      m_pDriver->Say(CTX_GLOBAL_CRASH_CAR);     break;
                                case eAEVehicleAudioType::GENERIC:  m_pDriver->Say(CTX_GLOBAL_CRASH_GENERIC); break;
                                default:                            NOTSA_UNREACHABLE();
                                }
                            }
                        }
                    }
                    if (this == FindPlayerVehicle()) {
                        if (const auto p = PickRandomPassenger()) {
                            p->Say(m_pDamageEntity->GetIsTypePed() ? CTX_GLOBAL_CAR_HIT_PED : CTX_GLOBAL_CAR_CRASH);
                        }
                    }
                }
                g_InterestingEvents.Add(CInterestingEvents::EType::VEHICLE_DAMAGE, this);
            }

            // Had to re-order all this mess.

            // Decrease m_fHealth
            const auto prevHealth = m_fHealth;

            // 0x6A82CB
            if (this == FindPlayerVehicle()->AsAutomobile()) {
                m_fHealth -= vehicleFlags.bTakeLessDamage ? calcCollHealthLoss * ExeRecip(6.f) : calcCollHealthLoss / 2.f;
            } else if (vehicleFlags.bTakeLessDamage) {
                m_fHealth -= calcCollHealthLoss * ExeRecip(12.f);
            } else {
                if (m_pDamageEntity && m_pDamageEntity == FindPlayerVehicle()) {
                    m_fHealth -= calcCollHealthLoss * ExeRecip(1.5f);
                }
                else {
                    m_fHealth -= calcCollHealthLoss / 4.f;
                }
            }
            // NOTSA_LOG_DEBUG("Health: {.2f} (Loss: {.2f})", m_fHealth, prevHealth - m_fHealth); // NOTSA
            // 0x6A8338
            if (CCheat::IsActive(CHEAT_SMASH_N_BOOM) && m_pDamageEntity && m_pDamageEntity == FindPlayerVehicle()) {
                BlowUpCar(m_pDamageEntity, false);
            }
            else if (m_fHealth <= 0.f && NOTSA_FTOL(uint16, prevHealth) > 0) { // 0x6A8354
                m_fHealth = 1.f; // 0x6A8374
                if (m_pDamageEntity && ModelIndices::IsRhino(m_pDamageEntity->m_nModelIndex) && FindPlayerVehicle()->AsAutomobile() != this) {
                    BlowUpCar(m_pDamageEntity, false);
                }
            }
        }

        if (!std::ranges::equal(prevLightStates, m_damageManager.GetAllLightsState())) { // Check if a light was smashed
            m_vehicleAudio.AddAudioEvent(AE_LIGHT_SMASH, 0.f); // Yeah, play sound
        }
    }

    // 0x6A83DF
    if (m_fHealth >= 250.f) {
        if (ModelIndices::IsBFInjection(m_nModelIndex)) {
            if (m_fHealth >= 400.f) {
                if (m_fHealth < 600.f) {
                    m_damageManager.SetEngineStatus(100u);
                }
            } else {
                m_damageManager.SetEngineStatus(200u);
            }
        }
    } else if (m_damageManager.GetEngineStatus() < 225u) {
        m_damageManager.SetEngineStatus(225u);

        m_fBurnTimer = 0.f;

        m_pLastDamageEntity = m_pDamageEntity;
        CEntity::SafeRegisterRef(m_pLastDamageEntity);

        if (const auto passenger = PickRandomPassenger()) {
            passenger->Say(CTX_GLOBAL_CAR_FIRE, 1500);
        }
    }
}

//0x6AF1D0
bool CAutomobile::GetTowHitchPos(CVector& outPos, bool bCheckModelInfo, CVehicle* attachTo) {
    if (!bCheckModelInfo) {
        return false;
    }
    outPos.x = 0.0f;
    outPos.y = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->m_boundBox.m_vecMax.y - 0.5f;
    outPos.z = 0.5f - m_fFrontHeightAboveRoad;
    outPos = m_matrix->TransformPoint(outPos);
    return true;
}

// 0x6AF250
bool CAutomobile::GetTowBarPos(CVector& outPos, bool ignoreModelType, CVehicle* attachTo) {
    switch (m_nModelIndex) {
    case eModelID::MODEL_TOWTRUCK:
    case eModelID::MODEL_TRACTOR: {
        float baseY = -1.05f;
        if (m_nModelIndex == MODEL_TRACTOR) {
            if (attachTo && attachTo->IsSubTrailer() && attachTo->m_nModelIndex != MODEL_FARMTR1) {
                return false;
            }
            baseY = -0.6f;
        } else if (attachTo && attachTo->IsSubTrailer()) {
            return false;
        }

        outPos.x = 0.0f;
        outPos.y = baseY + CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->m_boundBox.m_vecMin.y;
        outPos.z = ((1.0f - (float)m_wMiscComponentAngle / (float)TOWTRUCK_HOIST_DOWN_LIMIT) / 2.0f + 0.5f) - m_fFrontHeightAboveRoad;
        outPos = m_matrix->TransformPoint(outPos);
        return true;
    }
    default: { // TODO: Move out to after switch (deindentate)
        if (!m_aCarNodes[CAR_MISC_A]) {
            break;
        }

        switch (m_nModelIndex) {
        case eModelID::MODEL_PETRO:
        case eModelID::MODEL_RDTRAIN:
        case eModelID::MODEL_LINERUN:
        case eModelID::MODEL_ARTICT3: {
            outPos = *RwMatrixGetPos(RwFrameGetLTM(m_aCarNodes[CAR_MISC_A]));
            return true;
        }
        case eModelID::MODEL_UTILITY: {
            if (attachTo && attachTo->m_nModelIndex == MODEL_UTILTR1) {
                outPos = *RwMatrixGetPos(RwFrameGetLTM(m_aCarNodes[CAR_MISC_A]));
                return true;
            }
            break;
        }
        case eModelID::MODEL_BAGGAGE:
        case eModelID::MODEL_TUG:
        case eModelID::MODEL_BAGBOXA:
        case eModelID::MODEL_BAGBOXB: {
            if (attachTo) {
                switch (attachTo->m_nModelIndex) {
                case eModelID::MODEL_BAGBOXA:
                case eModelID::MODEL_BAGBOXB:
                case eModelID::MODEL_TUGSTAIR:
                    outPos = *RwMatrixGetPos(RwFrameGetLTM(m_aCarNodes[CAR_MISC_A]));
                    return true;
                }
            }
            break;
        }
        }
        break;
    }
    }

    return GetTowHitchPos(outPos, ignoreModelType, attachTo);
}

// 0x6B4410
bool CAutomobile::SetTowLink(CVehicle* tractor, bool placeMeOnRoadProperly) {
    if (!tractor) {
        return false;
    }

    if (m_pTowingVehicle) {
        return false;
    }

    switch (GetStatus()) {
    case eEntityStatus::STATUS_PHYSICS:
    case eEntityStatus::STATUS_IS_TOWED:
    case eEntityStatus::STATUS_ABANDONED:
        break;
    case eEntityStatus::STATUS_SIMPLE: // 0x6B4442: status 2 switches to real physics and continues,
        CCarCtrl::SwitchVehicleToRealPhysics(this);
        break;
    default: // any other status (3, 4, 10 handled above) returns false (0x6B4446)
        return false;
    }

    SetStatus(STATUS_IS_TOWED);

    m_pTowingVehicle = tractor;
    tractor->RegisterReference(m_pTowingVehicle);

    m_pTowingVehicle->m_pVehicleBeingTowed = this;
    RegisterReference(m_pTowingVehicle->m_pVehicleBeingTowed);

    for (auto&& entity : { AsVehicle(), tractor }) {
        entity->RemoveFromMovingList();
        entity->AddToMovingList();
    }

    if (placeMeOnRoadProperly) {
        switch (tractor->m_nModelIndex) {
        case eModelID::MODEL_TOWTRUCK:
        case eModelID::MODEL_TRACTOR: {
            tractor->AsAutomobile()->m_wMiscComponentAngle = TOWTRUCK_HOIST_UP_LIMIT;
            break;
        }
        }

        SetHeading(tractor->GetHeading());

        CVector towHitchPos{}, towBarPos{};
        if (GetTowHitchPos(towHitchPos, true, this) && tractor->GetTowBarPos(towBarPos, true, this)) {
            SetPosn(towBarPos - (towHitchPos - GetPosition()));
            PlaceOnRoadProperly();
            return true;
        }
    } else {
        UpdateTrailerLink(true, false);
        return true;
    }
    return false;
}

// 0x6A4400
bool CAutomobile::BreakTowLink() {
    if (m_pTowingVehicle) {
        CEntity::ClearReference(m_pTowingVehicle->m_pVehicleBeingTowed);
        CEntity::ClearReference(m_pTowingVehicle);
    }

    switch (GetStatus()) {
    case STATUS_IS_TOWED:
    case STATUS_IS_SIMPLE_TOWED: {
        if (m_pDriver) { // TODO: Use ternary
            if (m_pDriver->IsPlayer()) {
                SetStatus(STATUS_PLAYER);
            } else {
                SetStatus(STATUS_PHYSICS);
            }
        } else {
            if (m_fHealth >= 1.f) {
                SetStatus(STATUS_ABANDONED);
            } else {
                SetStatus(STATUS_WRECKED);
            }
        }
        return true;
    }
    }
    return false;
}

// 0x6A6090
float CAutomobile::FindWheelWidth(bool bRear) {
    // TODO: Maybe replace this with `bool flagValaue` and use bitfields directly (instead of bitmasks?)
    static constexpr struct { eVehicleHandlingFlags flag; float mult; } mapping[2][4]{
        { // Rear wheel
            { VEHICLE_HANDLING_WHEEL_R_NARROW2, 0.65f },
            { VEHICLE_HANDLING_WHEEL_R_NARROW,  0.8f  },
            { VEHICLE_HANDLING_WHEEL_R_WIDE,    1.1f  },
            { VEHICLE_HANDLING_WHEEL_R_WIDE2,   1.25f }
        },
        { // Front wheel
            { VEHICLE_HANDLING_WHEEL_F_NARROW2, 0.65f },
            { VEHICLE_HANDLING_WHEEL_F_NARROW,  0.8f  },
            { VEHICLE_HANDLING_WHEEL_F_WIDE,    1.1f  },
            { VEHICLE_HANDLING_WHEEL_F_WIDE2,   1.25f }
        }
    };

    const auto& mi = *GetVehicleModelInfo();

    auto wheelWidth = (bRear ? mi.m_fWheelSizeRear : mi.m_fWheelSizeFront) * ExeRecip(2.8f);

    // Find first flag and apply it (0x6A60BF: the rear wheel tests the REAR flags; mapping[0] is the rear table)
    for (auto&& [flag, mult] : mapping[bRear ? 0 : 1]) {
        if (m_nHandlingFlagsIntValue & flag) {
            wheelWidth *= mult;
            break;
        }
    }

    // 0x6A6128: the kart factor comes after the flag factor
    if (m_nModelIndex == eModelID::MODEL_KART) {
        wheelWidth *= 1.5f;
    }

    return wheelWidth;
}

// 0x5D47E0
bool CAutomobile::Save() {
    CVehicle::Save();
    CGenericGameStorage::SaveDataToWorkBuffer(&m_damageManager, sizeof(m_damageManager));
    return true;
}

// 0x5D2980
bool CAutomobile::Load() {
    CVehicle::Load();
    CGenericGameStorage::LoadDataFromWorkBuffer(&m_damageManager, sizeof(m_damageManager));
    return true;
}

// 0x6A0770
void CAutomobile::SetupModelNodes() {
    std::ranges::fill(m_aCarNodes, nullptr);
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aCarNodes.data());
}

// 0x6A07A0
void CAutomobile::HydraulicControl() {
    if (GetStatus() == STATUS_PHYSICS) { // TODO: switch()
        if (!IsCreatedBy(MISSION_VEHICLE))
            return;

        if (m_vehicleSpecialColIndex < 0)
            return;
    } else if (GetStatus() != STATUS_PLAYER) {
        return;
    }

    if (handlingFlags.bHydraulicNone) {
        return;
    }

    CPlayerPed* driver = nullptr;
    if (GetStatus() == STATUS_PLAYER) {
        if (m_pDriver && m_pDriver->IsPlayer())
            driver = m_pDriver->AsPlayer();
        if (CGameLogic::GameState != GAMELOGIC_STATE_PLAYING)
            return;
    }
    if (!CVehicle::GetSpecialColModel()) {
        return;
    }

    auto modelInfo = GetVehicleModelInfo();
    float wheelRadius = modelInfo->m_fWheelSizeFront / 2.0f;
    CCollisionData* colData = CVehicle::m_aSpecialColModel[m_vehicleSpecialColIndex].m_pColData;
    auto& hydraulicData = CVehicle::m_aSpecialHydraulicData[m_vehicleSpecialColIndex];
    float normalUpperLimit   = hydraulicData.m_fSuspensionNormalUpperLimit;
    float normalLowerLimit   = hydraulicData.m_fSuspensionNormalLowerLimit;
    float extendedUpperLimit = hydraulicData.m_fSuspensionExtendedUpperLimit;
    float extendedLowerLimit = hydraulicData.m_fSuspensionExtendedLowerLimit;
    float normalSpringLength = normalUpperLimit - normalLowerLimit;
    float extendedSpringLength = extendedUpperLimit - extendedLowerLimit;
    if (hydraulicData.m_fSuspensionExtendedUpperLimit >= 100.0f) {
        float minz = 0.0f;
        for (int32 i = 0; i < colData->m_nNumSpheres; i++) {
            float z = colData->m_pSpheres[i].m_vecCenter.z - colData->m_pSpheres[i].m_fRadius;
            if (z < minz)
                minz = z;
        }

        CVector pos;
        modelInfo->GetWheelPosn(CAR_WHEEL_FRONT_LEFT, pos, false);
        float limitExtend = minz - pos.z;
        normalLowerLimit = m_pHandlingData->m_fSuspensionLowerLimit;
        normalUpperLimit = m_pHandlingData->m_fSuspensionUpperLimit;
        normalSpringLength = normalUpperLimit - normalLowerLimit;
        if (handlingFlags.bHydraulicGeom)
            normalSpringLength *= 1.5f;
        normalUpperLimit = normalSpringLength + normalLowerLimit;
        hydraulicData.m_fSuspensionNormalUpperLimit = normalUpperLimit;
        hydraulicData.m_fSuspensionNormalLowerLimit = normalLowerLimit;
        if (handlingFlags.bHydraulicGeom) {
            extendedUpperLimit = normalUpperLimit - 0.4f;
            extendedLowerLimit = normalLowerLimit - 0.4f;
        } else {
            extendedUpperLimit = normalUpperLimit - 0.2f;
            extendedLowerLimit = normalLowerLimit - 0.2f;
        }
        extendedUpperLimit = std::max(extendedUpperLimit, limitExtend);
        extendedSpringLength = extendedUpperLimit - extendedLowerLimit;
        hydraulicData.m_fSuspensionExtendedUpperLimit = extendedUpperLimit;
        hydraulicData.m_fSuspensionExtendedLowerLimit = extendedLowerLimit;

        // or 0.15f * normalSpringLength
        float normalSpringLength_15_Percent = (1.0f - 0.7f) * 0.5f * normalSpringLength;
        hydraulicData.m_fSuspensionNormalIdleUpperLimit = normalUpperLimit - normalSpringLength_15_Percent;
        hydraulicData.m_fSuspensionNormalIdleLowerLimit = normalLowerLimit + normalSpringLength_15_Percent;
        float wheelHeight = limitExtend + wheelRadius - 0.1f;
        if (wheelHeight < hydraulicData.m_fSuspensionNormalIdleLowerLimit) {
            float height = hydraulicData.m_fSuspensionNormalIdleLowerLimit - wheelHeight;
            hydraulicData.m_fSuspensionNormalIdleUpperLimit -= height;
            hydraulicData.m_fSuspensionNormalIdleLowerLimit -= height;
        }
    }

    bool setPrevRatio = false;
    if (m_wMiscComponentAngle >= 20 || m_PrevSpeed <= 0.02f || m_GasPedal == 0.0f) {
        if (m_wMiscComponentAngle && m_wMiscComponentAngle < 61 && m_PrevSpeed < 0.01f) {
            m_wMiscComponentAngle--;
            if (m_wMiscComponentAngle == 0) // 0x6A0B16-0x6A0B3A: `test ax, ax; jne` after the decrement
                m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_OFF, 0.0f);
        }
    } else if (m_wMiscComponentAngle) {
        m_wMiscComponentAngle++;
    } else {
        m_wMiscComponentAngle = 20;
        m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_ON, 0.0f);
        setPrevRatio = true;
    }

    bool suspensionTriggered = false;
    bool hornHandled = false; // 0x6A0D9E / 0x6A0C80: both horn branches skip the per-frame suspension block
    float wheelPositions[4];
    float suspensionChange[4];

    CVector pos;
    if (driver && driver->GetPadFromPlayer()->HornJustDown()) {
        float lineLength = extendedSpringLength + wheelRadius;
        m_wMiscComponentAngle = m_wMiscComponentAngle < 500 ? 500 : 60;
        if (m_wMiscComponentAngle >= 500) {
            for (int32 i = 0; i < 4; i++) {
                CColLine& line = colData->m_pLines[i];
                wheelPositions[i] = line.m_vecStart.z - m_aSuspensionLineLength[i] * m_fWheelsSuspensionCompression[i];
                modelInfo->GetWheelPosn(i, pos, false);
                pos.z += extendedUpperLimit;
                line.m_vecStart = pos;
                pos.z -= lineLength;
                line.m_vecEnd = pos;
                m_aSuspensionSpringLength[i] = extendedSpringLength;
                m_aSuspensionLineLength[i] = lineLength;
                if (m_fWheelsSuspensionCompression[i] < 1.0f) {
                    float compressed = (line.m_vecStart.z - wheelPositions[i]) / lineLength;
                    m_fWheelsSuspensionCompression[i] = std::min(compressed, 1.0f);
                }
            }
            m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_TRIGGER, 0.0f);
            suspensionTriggered = true;
        } else {
            lineLength = normalSpringLength + wheelRadius;
            for (int32 i = 0; i < 4; i++) {
                CColLine& line = colData->m_pLines[i];
                wheelPositions[i] = line.m_vecStart.z - m_fWheelsSuspensionCompression[i] * m_aSuspensionLineLength[i];
                modelInfo->GetWheelPosn(i, pos, false);
                pos.z += normalUpperLimit;
                line.m_vecStart = pos;
                pos.z -= lineLength;
                line.m_vecEnd = pos;
                m_aSuspensionSpringLength[i] = normalSpringLength;
                m_aSuspensionLineLength[i] = lineLength;
                if (m_fWheelsSuspensionCompression[i] < 1.0f) {
                    float compressed = (line.m_vecStart.z - wheelPositions[i]) / lineLength;
                    m_fWheelsSuspensionCompression[i] = std::min(compressed, 1.0f);
                }
            }
            m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_HYDRAULIC, 0.0f);
            hornHandled = true; // 0x6A0D9E jumps to 0x6A13B8 (event) -> 0x6A13C7, past the block below
        }
    }

    if (!suspensionTriggered && !hornHandled) {
        std::ranges::fill(suspensionChange, 1.0f);

        if (GetStatus() == STATUS_PHYSICS) {
            for (int32 i = 0; i < 4; i++) {
                suspensionChange[i] = hydraulicData.m_aWheelSuspension[i];
            }
        } else if (driver) {
            if (!driver->GetPadFromPlayer()->GetHydraulicJump()) { // 0x6A0E3D
                float gunLeftRight = driver->GetPadFromPlayer()->GetCarGunLeftRight();
                float gunUpDown = driver->GetPadFromPlayer()->GetCarGunUpDown();
                float jumpFactor = (std::sqrt(gunUpDown * gunUpDown + gunLeftRight * gunLeftRight) * 1.5f) / 128.0f;
                float angle = X87R(x87::atan2(gunLeftRight, gunUpDown) - DegreesToRadians(45.0f));
                float change_rear_right = X87R(x87::cos(angle) * jumpFactor);
                float change_front_right = X87R(x87::sin(angle) * jumpFactor);
                suspensionChange[CAR_WHEEL_FRONT_LEFT]  = std::max(0.0f, -change_rear_right);
                suspensionChange[CAR_WHEEL_REAR_LEFT]   = std::max(0.0f, -change_front_right);
                suspensionChange[CAR_WHEEL_FRONT_RIGHT] = std::max(0.0f, +change_front_right);
                suspensionChange[CAR_WHEEL_REAR_RIGHT]  = std::max(0.0f, +change_rear_right);
            }
        }

        float maxDelta = 0.0f;
        if (m_wMiscComponentAngle < 500u) {
            if (suspensionChange[CAR_WHEEL_FRONT_LEFT] == 0.0f &&
                suspensionChange[CAR_WHEEL_REAR_LEFT] == 0.0f &&
                suspensionChange[CAR_WHEEL_FRONT_RIGHT] == 0.0f &&
                suspensionChange[CAR_WHEEL_REAR_RIGHT] == 0.0f
            ) {
                if (m_wMiscComponentAngle == 0) {
                    normalUpperLimit = hydraulicData.m_fSuspensionNormalIdleUpperLimit;
                    normalLowerLimit = hydraulicData.m_fSuspensionNormalIdleLowerLimit;
                    normalSpringLength = normalUpperLimit - normalLowerLimit;
                }
            } else {
                if (m_wMiscComponentAngle == 0) {
                    std::ranges::fill(suspensionChange, 0.0f);
                }
                m_wMiscComponentAngle = 60;
            }

            for (int32 i = 0; i < 4; i++) {
                CColLine& line = colData->m_pLines[i];
                suspensionChange[i] = std::min(suspensionChange[i], 1.0f);
                float springLength = (extendedSpringLength - normalSpringLength) * suspensionChange[i] + normalSpringLength;
                float lineLength = springLength + wheelRadius;
                wheelPositions[i] = line.m_vecStart.z - m_aSuspensionLineLength[i] * m_fWheelsSuspensionCompression[i];
                modelInfo->GetWheelPosn(i, pos, false);
                pos.z += (extendedUpperLimit - normalUpperLimit) * suspensionChange[i] + normalUpperLimit;
                line.m_vecStart = pos;
                pos.z -= lineLength;
                { // 0x6A1007-0x6A1022 / 0x6A1225-0x6A1242: keeps the SIGNED delta when |delta| > |maxDelta|
                    const float delta = pos.z - line.m_vecEnd.z;
                    if (std::fabs(delta) > std::fabs(maxDelta)) {
                        maxDelta = delta;
                    }
                }
                line.m_vecEnd = pos;
                m_aSuspensionLineLength[i] = lineLength;
                m_aSuspensionSpringLength[i] = springLength;
                if (m_fWheelsSuspensionCompression[i] < 1.0f) {
                    float compressed = (line.m_vecStart.z - wheelPositions[i]) / lineLength;
                    m_fWheelsSuspensionCompression[i] = std::min(compressed, 1.0f);
                }
            }
        } else {
            if (m_wMiscComponentAngle < 504u) {
                m_wMiscComponentAngle++;
            }

            for (int32 i = 0; i < 4; i++) {
                CColLine& line = colData->m_pLines[i];
                suspensionChange[i] = std::min(suspensionChange[i], 1.0f);
                float springLength = (normalSpringLength - extendedSpringLength) * suspensionChange[i] + extendedSpringLength;
                wheelPositions[i] = line.m_vecStart.z - m_aSuspensionLineLength[i] * m_fWheelsSuspensionCompression[i];
                modelInfo->GetWheelPosn(i, pos, false);
                pos.z += (normalUpperLimit - extendedUpperLimit) * suspensionChange[i] + extendedUpperLimit;
                float lineLength = springLength + wheelRadius;
                line.m_vecStart = pos;
                pos.z -= lineLength;
                {
                    const float delta = pos.z - line.m_vecEnd.z;
                    if (std::fabs(delta) > std::fabs(maxDelta)) {
                        maxDelta = delta;
                    }
                }
                line.m_vecEnd = pos;
                m_aSuspensionLineLength[i] = lineLength;
                m_aSuspensionSpringLength[i] = springLength;
                if (m_fWheelsSuspensionCompression[i] < 1.0f) {
                    float compressed = (line.m_vecStart.z - wheelPositions[i]) / lineLength;
                    m_fWheelsSuspensionCompression[i] = std::min(compressed, 1.0f);
                }
            }
        }
        float limitDiff = extendedLowerLimit - normalLowerLimit;
        if (limitDiff != 0.0f && std::fabs(maxDelta / limitDiff) > 0.01f) {
            float f = (limitDiff + maxDelta) * 0.5f / limitDiff;
            f = std::clamp(f, 0.0f, 1.0f);
            if (f < 0.4f || f > 0.6f)
                setPrevRatio = true;
            if (f < 0.5f - CTimer::GetTimeStep() * 0.05f)
                m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_HYDRAULIC, 0.0f);
            else if (f > CTimer::GetTimeStep() * 0.05f + 0.5f)
                m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_TRIGGER, 0.0f);
            else if (f > CTimer::GetTimeStep() * 0.025f + 0.5f)
                m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_HYDRAULIC, 0.0f);
        }
    }

    if (setPrevRatio || suspensionTriggered) {
        for (int32 i = 0; i < MAX_CARWHEELS; i++) {
            const float t = 1.f - m_aSuspensionSpringLength[i] / m_aSuspensionLineLength[i];
            // same as `(m_fWheelsSuspensionCompression[i] - 1.f + k) / k` where `k = m_aSuspensionSpringLength[i] / m_aSuspensionLineLength[i]`
            m_fWheelsSuspensionCompressionPrev[i] = (m_fWheelsSuspensionCompression[i] - t) / (1.0f - t); 
        }
    }
    std::ranges::fill(hydraulicData.m_aWheelSuspension, 0.0f);
}

static constexpr uint16 DEFAULT_COLLISION_EXTENDLIMIT = 2500; // 0x8D314C

// 0x6A1460
bool CAutomobile::UpdateMovingCollision(float angle) {
    if (ModelIndices::HasMiscComponent(m_nModelIndex)) {
        m_wMiscComponentAnglePrev = m_wMiscComponentAngle;
    }

    CPad* pad = nullptr;
    if (GetStatus() == STATUS_PLAYER) {
        CPlayerPed* driver = m_pDriver->AsPlayer();
        if (!driver || !driver->IsPlayer() || CGameLogic::GameState != GAMELOGIC_STATE_PLAYING)
            return false;

        pad = driver->GetPadFromPlayer();
    } else {
        if (   !IsCreatedBy(MISSION_VEHICLE)
            || angle < 0.0f
            || !ModelIndices::HasMiscComponent(m_nModelIndex) && !ModelIndices::IsFireTruckLadder(m_nModelIndex)
        ) {
            return false;
        }
    }

    if (!ModelIndices::IsCementTruck(m_nModelIndex) && !ModelIndices::IsFireTruckLadder(m_nModelIndex)) { // Inverted(!)
        CColModel* cm = GetVehicleModelInfo()->GetColModel();
        CCollisionData* cd = cm->m_pColData;
        if (!GetSpecialColModel()) {
            return false;
        }

        CColModel& specialColModel = m_aSpecialColModel[m_vehicleSpecialColIndex];
        CCollisionData* specialColData = specialColModel.m_pColData;

        m_wMiscComponentAnglePrev = m_wMiscComponentAngle;

        if (angle < 0.0f) {
            if (!pad || std::abs(pad->GetCarGunUpDown()) <= 10) {
                return false;
            }
        }

        if (angle >= 0.0f) {
            m_wMiscComponentAngle = NOTSA_FTOL(uint16, angle * DEFAULT_COLLISION_EXTENDLIMIT);
        } else {
            float colAngleMult = 10.0f;
            if (ModelIndices::IsForklift(m_nModelIndex) ) {
                if ((float)pad->GetCarGunUpDown() >= 0.0f)
                    colAngleMult *= -2;
                else
                    colAngleMult *= -1;
            }

            m_wMiscComponentAngle += NOTSA_FTOL(uint16, ((float)pad->GetCarGunUpDown() / 128.0f) * colAngleMult * CTimer::GetTimeStep());
            m_wMiscComponentAngle = std::clamp<uint16>(m_wMiscComponentAngle, 0, DEFAULT_COLLISION_EXTENDLIMIT);
        }

        CMatrix rotMatrix;

        // 0x6A174A
        RwFrame* carNodeMisc = nullptr;
        if (ModelIndices::IsDozer(m_nModelIndex)) {
            carNodeMisc = m_aCarNodes[CAR_MISC_A];
            if (carNodeMisc)
                rotMatrix.SetRotateX((float)m_wMiscComponentAngle * CAutomobile::DOZER_COL_ANGLE_MULT);
        }
        else if (ModelIndices::IsDumper(m_nModelIndex) && IsSubMonsterTruck()) {
            carNodeMisc = m_aCarNodes[CAR_MISC_C];
            if (carNodeMisc)
                rotMatrix.SetRotateX((float)m_wMiscComponentAngle * CMonsterTruck::DUMPER_COL_ANGLEMULT);
        }
        else if (ModelIndices::IsAndromada(m_nModelIndex)) {
            carNodeMisc = m_aCarNodes[CAR_MISC_E];
            if (carNodeMisc)
                rotMatrix.SetRotateX((float)m_wMiscComponentAngle * CPlane::ANDROM_COL_ANGLE_MULT);
        }
        else if (ModelIndices::IsForklift(m_nModelIndex)) {
            carNodeMisc = m_aCarNodes[CAR_MISC_A];
            if (carNodeMisc)
                rotMatrix.SetTranslate(CVector(0.0f, 0.0f, (float)m_wMiscComponentAngle * CAutomobile::FORKLIFT_COL_ANGLE_MULT));
        }
        else if (ModelIndices::IsPacker(m_nModelIndex)) {
            carNodeMisc = m_aCarNodes[CAR_MISC_A];
            if (carNodeMisc)
                rotMatrix.SetRotateX((float)m_wMiscComponentAngle * CAutomobile::PACKER_COL_ANGLE_MULT);
        }

        CVector componentPos;
        if (carNodeMisc && !ModelIndices::IsForklift(m_nModelIndex))
            componentPos = RwFrameGetMatrix(carNodeMisc)->pos;

        float minZ = 1000.0f;
        float maxZ = -1000.0f;
        for (uint16 triIndx = 0; triIndx < specialColData->m_nNumTriangles; triIndx++) {
            CColTriangle& specialColTriangle = specialColData->m_pTriangles[triIndx];
            if (specialColTriangle.m_nMaterial == SURFACE_CAR_MOVINGCOMPONENT) {
                const CColTriangle& colTriangle = cd->m_pTriangles[triIndx];
                for (int32 i = 0; i < 3; i++) {
                    CVector vertexPos = cd->m_pVertices[colTriangle.m_vertIndices[i]];
                    CVector distance  = vertexPos - componentPos;
                    vertexPos = rotMatrix.TransformPoint(distance) + componentPos;
                    specialColData->m_pVertices[specialColTriangle.m_vertIndices[i]] = vertexPos;
                    if (maxZ < vertexPos.z)
                        maxZ = vertexPos.z;
                    else if (minZ > vertexPos.z)
                        minZ = vertexPos.z;
                }
            }
        }

        if (specialColData->m_pTrianglePlanes) {
            for (int32 i = 0; i < specialColData->m_nNumTriangles; i++) {
                CColTrianglePlane& trianglePlane = specialColData->m_pTrianglePlanes[i];
                trianglePlane.Set(specialColData->m_pVertices, specialColData->m_pTriangles[i]);
            }
        }

        for (uint16 i = 0; i < specialColData->m_nNumSpheres; i++) {
            CColSphere& specialColSphere = specialColData->m_pSpheres[i];
            if (specialColSphere.m_Surface.m_nMaterial == SURFACE_CAR_MOVINGCOMPONENT) {
                CColSphere& colSphere = cd->m_pSpheres[i];
                CVector distance = colSphere.m_vecCenter - componentPos;
                specialColSphere.m_vecCenter = (rotMatrix.TransformPoint(distance)) + componentPos;
                const float newMaxZ = specialColSphere.m_fRadius + specialColSphere.m_vecCenter.z;
                const float newMinZ = specialColSphere.m_vecCenter.z - specialColSphere.m_fRadius;
                if (maxZ < newMaxZ)
                    maxZ = newMaxZ;
                else if (minZ > newMinZ)
                    minZ = newMinZ;
            }
        }
        if (cm->GetBoundingBox().m_vecMax.z < maxZ)
            specialColModel.GetBoundingBox().m_vecMax.z = maxZ;
        if (cm->GetBoundingBox().m_vecMin.z > minZ)
            specialColModel.GetBoundingBox().m_vecMin.z = minZ;
        return true;
    }

    m_wMiscComponentAnglePrev = m_wMiscComponentAngle;
    if (angle >= 0.0f) { // Inverted
        m_wMiscComponentAngle = NOTSA_FTOL(uint16, angle * DEFAULT_COLLISION_EXTENDLIMIT); // 0x6A1DA6
        return false;
    }

    if (!pad)
        return false;

    if (pad->GetCarGunUpDown() < -10) {
        m_wMiscComponentAngle -= NOTSA_FTOL(uint16, 2 * ((float)pad->GetCarGunUpDown() / 128.0f) * 10.0f * CTimer::GetTimeStep());
        m_wMiscComponentAngle = std::min(m_wMiscComponentAngle, DEFAULT_COLLISION_EXTENDLIMIT);
        return false;
    }

    if (!m_wMiscComponentAngle)
        return false;

    m_wMiscComponentAngle -= NOTSA_FTOL(uint16, (float(pad->GetCarGunUpDown() + 100) * 10.0f * CTimer::GetTimeStep()) / 128.0f);
    if (static_cast<int16>(m_wMiscComponentAngle) < 0)
        m_wMiscComponentAngle = 0;

    return false;
}

// 0x6A2150
float CAutomobile::GetMovingCollisionOffset() {
    if (m_wMiscComponentAngle) { // TODO: Invert
        switch (m_nModelIndex) {
        case eModelID::MODEL_DUMPER:
            return m_aCarNodes[CAR_MISC_C] ? (float)m_wMiscComponentAngle * CMonsterTruck::DUMPER_COL_ANGLEMULT : 0.0f;

        case eModelID::MODEL_PACKER:
            return (float)m_wMiscComponentAngle / -10000.f;

        case eModelID::MODEL_DOZER:
            return m_aCarNodes[CAR_MISC_A] ? (float)m_wMiscComponentAngle * ExeRecip(5000.f) : 0.0f;

        case eModelID::MODEL_ANDROM:
            return m_aCarNodes[CAR_MISC_E] ? (float)m_wMiscComponentAngle * CPlane::ANDROM_COL_ANGLE_MULT : 0.0f;

        case eModelID::MODEL_FORKLIFT:
            return (float)m_wMiscComponentAngle * 0.0006f; // / 1666.66?
        }
    }
    return 0.f;
}

// 0x6A2390
void CAutomobile::TellHeliToGoToCoors(float x, float y, float z, float altitudeMin, float altitudeMax) {
    m_autoPilot.m_vecDestinationCoors = CVector{ x, y, z };
    m_autoPilot.SetCarMission(MISSION_HELI_FLYTOCOORS);
    m_autoPilot.SetCruiseSpeed(100);

    AsHeli()->m_fMinAltitude = altitudeMin;
    AsHeli()->m_fMaxAltitude = altitudeMax;

    SetStatus(STATUS_PHYSICS);

    if (m_fAircraftGoToHeading == 0.f) {
        // 0x6A23F2: atan2 + PI, then brought back below 2 PI by repeated subtraction (there is no extra `- 2 PI`)
        // `call 0x53CC70` leaves its result UNROUNDED on the x87 stack; `fadd [0x858CB8]` (pi, 0x40490FDB) and a single `fst` round it once
        m_fAircraftGoToHeading = (float)(X87R(CGeneral::GetATanOfXYExt(m_matrix->GetForward().x, m_matrix->GetForward().y) + (double)std::bit_cast<float>(0x40490FDBu)));
        if (m_fAircraftGoToHeading > TWO_PI) {
            auto heading = m_fAircraftGoToHeading;
            do
                heading -= TWO_PI;
            while (heading > TWO_PI);
            m_fAircraftGoToHeading = heading;
        }
    }
}

// 0x6A2450
void CAutomobile::SetHeliOrientation(float angle) {
    m_fForcedOrientation = angle;
}

// 0x6A2460
void CAutomobile::ClearHeliOrientation() {
    m_fForcedOrientation = -1.0f;
}

// 0x6A2470
void CAutomobile::TellPlaneToGoToCoors(float x, float y, float z, float altitudeMin, float altitudeMax) {
    m_autoPilot.m_vecDestinationCoors = CVector{ x, y, z };
    m_autoPilot.SetCarMission(MISSION_PLANE_FLYTOCOORS);
    m_autoPilot.SetCruiseSpeed(0);

    // 0x6A2470: the 4th argument is stored as is (+0x9B0 = min altitude), the 5th one is raised to the destination height (+0x9A8 = max altitude)
    AsPlane()->m_minAltitude = altitudeMin;
    AsPlane()->m_maxAltitude = std::max(z, altitudeMax);

    SetStatus(STATUS_PHYSICS);

    if (m_fAircraftGoToHeading == 0.f) {
        m_fAircraftGoToHeading = CGeneral::GetATanOfXY(m_matrix->GetForward().x, m_matrix->GetForward().y);
    }

    vehicleFlags.bEngineOn = true;
}

// unused
// 0x6A2510
void CAutomobile::HideAllComps() {
    // NOP
}

// unused
// 0x6A2520
void CAutomobile::ShowAllComps() {
    // NOP
}

// 0x6A2530
void CAutomobile::SetRandomDamage(bool bRemoveStuff) {
    //> Bugfix shit
    const auto FixedRandom = [&](int32 x, int32 y) {
        // I'm very sure somebody fell into the trap of the shit naming of `GetRandomNumberInRange`
        // (The int version of it is exclusive, while the float one is inclusive)
        // Reason being that at `0x6A260B` the possible interval is [0, 1), that is, in integer terms a fat `0` :D
        // Obviously, they intended it to be `[0, 1]`
#ifdef FIX_BUGS
        return (size_t)CGeneral::GetRandomNumberInRange(x, y + 1);
#else
        return (size_t)CGeneral::GetRandomNumberInRange(x, y);
#endif
    };

    //> Processes a group of components (panels/doors)
    const auto Process = [
        this,
        IsCompDamageable = [vs = GetVehicleModelInfo()->GetVehicleStruct()](auto nodeIdx) {
            return vs->IsComponentDamageable(nodeIdx);
        }
    ](
        size_t pickedbs, 
        size_t numToDmg,
        size_t numTotal,
        auto   GetNodeIdx,
        auto   IsDamStateOk,
        auto   SetAsDamaged
    ) {
        /**
        * Get the `n`-th component that wasn't yet picked
        * @param n 0-based number
        * @param pickedbs Bitset of components that were picked previously
        */
        const auto GetNthUnpicked = [&](size_t n) {
            if (pickedbs == 0) {
                return n; // No pickedbs components so we can directly return
            }
            for (size_t i = 0; i < numTotal; i++) {
                if ((pickedbs & (1 << i)) == 0) {
                    if (n-- == 0) {
                        return i;
                    }
                }
            }
            NOTSA_UNREACHABLE_RET(n); // This is reached if the first `ncomps` bits are all set - Should never happen
        };

        for (size_t i = 0u; i < numToDmg; i++) {
            const auto pick = GetNthUnpicked((size_t)CGeneral::GetRandomNumberInRange(0, (int32)(numToDmg - i)));
            pickedbs |= 1 << pick; // We mark here for simplicity
            if (!IsDamStateOk(pick)) {
                continue;
            }
            const auto nodeIdx = GetNodeIdx(pick);
            if (!IsCompDamageable(nodeIdx)) {
                continue;
            }
            SetAsDamaged(pick);
            SetComponentVisibility(m_aCarNodes[nodeIdx], eAtomicComponentFlag::ATOMIC_DAMAGED);
        }
    };

    //> Doors
    const auto numDoorsToDmg = FixedRandom(0, bRemoveStuff ? MAX_DOORS : 1);
    Process(
        0,
        numDoorsToDmg,
        MAX_DOORS,
        [](auto i)     { return CDamageManager::GetCarNodeIndexFromDoor((eDoors)i); },
        [this](auto i) { return m_damageManager.GetDoorStatus((eDoors)i) == DAMSTATE_OK; },
        [this](auto i) { return m_damageManager.SetDoorStatus((eDoors)i, DAMSTATE_DAMAGED); }
    );

    //> Panels (With a few exceptions)
    const auto numPanelsToDmg = numDoorsToDmg == 0
        ? bRemoveStuff ? FixedRandom(1, 4) : 1 // The 4 here and below comes from MAX_PANELS - 3 (As 3 panels are never damaged, see below)
        : FixedRandom(0, bRemoveStuff ? 4 : 1);
    Process(
        1 << REAR_LEFT_PANEL | 1 << REAR_RIGHT_PANEL | 1 << WINDSCREEN_PANEL, // These are never damaged
        numPanelsToDmg,
        MAX_PANELS,
        [](auto i)     { return CDamageManager::GetCarNodeIndexFromPanel((ePanels)i); },
        [this](auto i) { return m_damageManager.GetPanelStatus((ePanels)i) == DAMSTATE_OK; },
        [this](auto i) { return m_damageManager.SetPanelStatus((ePanels)i, DAMSTATE_DAMAGED); }
    );
}

// 0x6A27F0
void CAutomobile::SetTotalDamage(bool randomness) {
    const auto IsComponentDamageable = [vehStruct = GetVehicleModelInfo()->m_pVehicleStruct](int32 component) {
        return vehStruct->IsComponentDamageable(component);
    };

    // Doors
    for (auto i = 0u; i < MAX_DOORS; i++) {
        const auto nodeIdx{ CDamageManager::GetCarNodeIndexFromDoor((eDoors)i) };
        if (const auto doorFrame{ m_aCarNodes[nodeIdx] }) {
            if (!randomness || CGeneral::GetRandomNumberInRange(0, 3)) {
                if (m_damageManager.GetDoorStatus((eDoors)i) == DAMSTATE_OK && IsComponentDamageable(nodeIdx)) {
                    m_damageManager.SetDoorStatus((eDoors)i, DAMSTATE_DAMAGED);
                    SetComponentVisibility(doorFrame, eAtomicComponentFlag::ATOMIC_DAMAGED);
                }
            } else {
                m_damageManager.SetDoorStatus((eDoors)i, DAMSTATE_NOTPRESENT);
                SetComponentVisibility(doorFrame, eAtomicComponentFlag::ATOMIC_NONE);
            }
        }
    }

    // Panels
    for (auto i = 0u; i < MAX_PANELS; i++) { // TODO: Maybe use array of panel enums instead?
        switch ((ePanels)i) {
        case ePanels::REAR_LEFT_PANEL:
        case ePanels::REAR_RIGHT_PANEL:
            break;

        default: {
            const auto nodeIdx{ CDamageManager::GetCarNodeIndexFromPanel((ePanels)i) };
            if (const auto panelFrame{ m_aCarNodes[nodeIdx] }) {
                if (!randomness || CGeneral::GetRandomNumberInRange(0, 3)) {
                    if (m_damageManager.GetPanelStatus((ePanels)i) == DAMSTATE_OK && IsComponentDamageable(nodeIdx)) {
                        m_damageManager.SetPanelStatus((ePanels)i, DAMSTATE_OPENED);
                        SetComponentVisibility(panelFrame, eAtomicComponentFlag::ATOMIC_DAMAGED);
                    }
                } else {
                    m_damageManager.SetPanelStatus((ePanels)i, DAMSTATE_OPENED_DAMAGED);
                    SetComponentVisibility(panelFrame, eAtomicComponentFlag::ATOMIC_NONE);
                }
            }
            break;
        }
        }
    }
}

// 0x6A2980
RpMaterial* DisableMatFx(RpMaterial* material, void* data) {
    RpMatFXMaterialSetEffects(material, rpMATFXEFFECTNULL);
    return material;
}

// 0x6A29A0
void CAutomobile::ReduceHornCounter() {
    if (m_HornCounter) {
        m_HornCounter--;
    }
}

#ifdef NOTSA_GLOBALS_DETACHED
namespace { NOTSA_GLOBAL_SYNTH(k_C1BFD8, 0xC1BFD8, (RwTexture*), {}); } // the plate material's original texture, between CustomCarPlate_BeforeRenderingStart and _AfterRenderingStop
static RwTexture*& renderLicensePlateTexture{ k_C1BFD8 };
#else
static RwTexture*& renderLicensePlateTexture{ *(RwTexture**)0xC1BFD8 };
#endif
#line 4103

// 0x6A2F00
void CAutomobile::CustomCarPlate_BeforeRenderingStart(const CVehicleModelInfo& mi)
{
    if (mi.m_pPlateMaterial) {
        renderLicensePlateTexture = RpMaterialGetTexture(mi.m_pPlateMaterial);
        RwTextureAddRef(renderLicensePlateTexture);
        RpMaterialSetTexture(mi.m_pPlateMaterial, m_pCustomCarPlate);
    }
}

// 0x6A2F30
void CAutomobile::CustomCarPlate_AfterRenderingStop(CVehicleModelInfo* model) {
    if (model->m_pPlateMaterial) {
        RpMaterialSetTexture(model->m_pPlateMaterial, renderLicensePlateTexture);
        RwTextureDestroy(renderLicensePlateTexture);
        renderLicensePlateTexture = nullptr;
    }
}

/*
 * Check if plyrveh is in air
 * @addr 0x6A2F70
 */
bool CAutomobile::GetAllWheelsOffGround() const {
    return m_NumDriveWheelsOnGround == 0;
}

// 0x6A2F80
void CAutomobile::DebugCode() {
    auto* mi = GetVehicleModelInfo();
    auto* vehicle = FindPlayerVehicle();
    auto* remoteVehicle = FindPlayerInfo().m_pRemoteVehicle;
    if (vehicle == this && !remoteVehicle || remoteVehicle == this) {
        auto* handling      = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
        m_fMass             = handling->m_fMass;
        m_fTurnMass         = handling->m_fTurnMass;
        m_fBuoyancyConstant = handling->m_fBuoyancyConstant;
        m_fAirResistance    = GetDefaultAirResistance(); // todo: check handling. does handling in f() equal handling above
    }
}

// 0x6A3580
void CAutomobile::FixTyre(eWheels wheel) {
    m_damageManager.SetWheelStatus(wheel, WHEEL_STATUS_OK);
}

// 0x6A35A0
void CAutomobile::FixDoor(int32 nodeIndex, eDoors door) {
    if (!m_pHandlingData->m_bNoDoors) { // TODO: Invert
        m_doors[door].Open(0.f);
        m_damageManager.SetDoorStatus(door, DAMSTATE_OK);
        if (const auto frame = m_aCarNodes[nodeIndex]) {
            SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_OK);

            // Reset it's matrix
            CMatrix mat{ RwFrameGetLTM(frame), false };
            mat.SetTranslate(GetPosition());
            mat.UpdateRW();
        }
    }
}

// 0x6A3670
void CAutomobile::FixPanel(eCarNodes nodeIndex, ePanels panel) {
    m_damageManager.SetPanelStatus(panel, DAMSTATE_OK);

    // Remove any bouncing panels belonging to this frame
    for (auto&& panelx : m_panels) {
        if (panelx.m_nFrameId == nodeIndex) {
            panelx.ResetPanel();
        }
    }

    if (const auto frame = m_aCarNodes[nodeIndex]) { // Same code as in FixDoor.. Maybe this was a standalone function, like "FixNode"?
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_OK);

        CMatrix mat{ RwFrameGetLTM(frame), false };
        mat.SetTranslate(GetPosition());
        mat.UpdateRW();
    }
}

// 0x6A3740
void CAutomobile::SetTaxiLight(bool enable) {
    autoFlags.bTaxiLight = enable;
}

// 0x6A3760
void CAutomobile::SetAllTaxiLights(bool enable) {
    m_sAllTaxiLights = enable;
}

// 0x6A3820
void CAutomobile::PlayHornIfNecessary() {
    if ((m_autoPilot.carCtrlFlags.bHonkAtPed || m_autoPilot.carCtrlFlags.bHonkAtCar) && !HasCarStoppedBecauseOfLight()) {
        PlayCarHorn();
    }
}

// 0x6A3860
void CAutomobile::SetBusDoorTimer(uint32 timerEndDelta, bool setAsStartedInPast) {
    const auto time = setAsStartedInPast ? CTimer::GetTimeInMS() - 500u : CTimer::GetTimeInMS();
    m_nBusDoorTimerStart = time;
    m_nBusDoorTimerEnd   = time + std::max(1000u, timerEndDelta);
}

// 0x6A38A0
void CAutomobile::ProcessAutoBusDoors() {
    if (m_nBusDoorTimerEnd <= CTimer::GetTimeInMS()) {
        if (m_nBusDoorTimerStart) {
            static constexpr struct { eDoors door; tComponent comp; uint32 flagMask; } doors[]{
                { eDoors::DOOR_LEFT_FRONT,  tComponent::COMPONENT_DOOR_RR, 1 }, // TODO: `COMPONENT_DOOR_RR` doesn't match up with `DOOR_LEFT_FRONT`
                { eDoors::DOOR_RIGHT_FRONT, tComponent::COMPONENT_DOOR_RF, 4 }
            };

            for (auto&& [door, comp, flagMask] : doors) {
                if (!IsDoorMissing(door) && !(m_nGettingInFlags & flagMask)) { // 0x6A39FE / 0x6A3A28: `test [esi+0x486], mask / jne skip` - the door is only moved when nobody is getting in through it
                    OpenDoor(nullptr, comp, door, 0.f, true);
                }
            }

            m_nBusDoorTimerEnd = 0;
            m_nBusDoorTimerStart = 0;
        }
    } else if (m_nBusDoorTimerEnd && CTimer::GetTimeInMS() > m_nBusDoorTimerEnd - 500) {
        if (!IsDoorMissing(eDoors::DOOR_LEFT_FRONT) && !(m_nGettingInFlags & 1)) { // 0x6A38DD: `test [esi+0x486], 1 / jne skip`
            const auto OpenThisDoor = [this](float ratio) {
                OpenDoor(nullptr, COMPONENT_DOOR_RR, DOOR_LEFT_FRONT, ratio, true); // TODO: `COMPONENT_DOOR_RR` doesn't match up with `DOOR_LEFT_FRONT`
            };

            if (IsDoorClosed(eDoors::DOOR_LEFT_FRONT)) {
                m_nBusDoorTimerEnd = CTimer::GetTimeInMS(); // 0x6A392A: `mov [esi+0x870], [CTimer::m_snTimeInMilliseconds]` (the port wrote m_nBusDoorTimerStart = 0 here)
                OpenThisDoor(0.f);
            } else {
                OpenThisDoor(1.f - (float)(CTimer::GetTimeInMS() - m_nBusDoorTimerEnd + 500) * ExeRecip(500.f));
            }
        }

        if (!IsDoorMissing(eDoors::DOOR_RIGHT_FRONT) && !(m_nGettingInFlags & 4)) { // 0x6A3966: `test [esi+0x486], 4 / jne skip`
            if (IsDoorClosed(eDoors::DOOR_RIGHT_FRONT)) {
                m_nBusDoorTimerEnd = CTimer::GetTimeInMS();
            }
            OpenDoor(nullptr, COMPONENT_DOOR_RF, DOOR_RIGHT_FRONT, 0.f, true);
        }
    }
}

// 0x6A3A60
void CAutomobile::BoostJumpControl() {
    if (m_pDriver && m_pDriver->IsPlayer()) { // TODO: Invert
        if (const auto playerPad = m_pDriver->AsPlayer()->GetPadFromPlayer()) {
            if (playerPad->HornJustDown() && m_fWheelsSuspensionCompression[0] < 1.f) {
                ApplyMoveForce(CVector{ 0.f, 0.f, 1.f } * (m_fMass * 0.15f));
                ApplyTurnForce(m_matrix->GetUp() * (m_fTurnMass * ExeRecip(100.f)), m_matrix->GetForward());
            }
        }
    }
}

// 0x6A3BD0
void CAutomobile::DoNitroEffect(float power) {
    // TODO: Refactor

    const auto& mi = *GetVehicleModelInfo();
    CVector exhaustPosition = mi.GetModelDummyPosition(DUMMY_EXHAUST);

    bool firstExhaustSubmergedInWater = false;
    bool secondExhaustSubmergedInWater = false;
    float level = 0.0f;
    if (physicalFlags.bTouchingWater) {
        CVector point = m_matrix->TransformPoint(exhaustPosition);
        if (CWaterLevel::GetWaterLevel(point.x, point.y, point.z, level, true, nullptr)) {
            if (level >= point.z)
                firstExhaustSubmergedInWater = true;
        }
    }
    CVector secondExhaustPosition;
    if (m_pHandlingData->m_bDoubleExhaust) {
        secondExhaustPosition = exhaustPosition;
        secondExhaustPosition.x *= -1.0f;
        if (!physicalFlags.bTouchingWater) {
            CVector point = m_matrix->TransformPoint(secondExhaustPosition);
            if (CWaterLevel::GetWaterLevel(point, level, true)) {
                if (level >= point.z)
                    secondExhaustSubmergedInWater = true;
            }
        }
    }

    RwMatrix* rwMatrix = GetModellingMatrix();

    FxSystem_c* firstExhaustFxSystem = m_exhaustNitroFxSystem[0];
    if (firstExhaustFxSystem) {
        firstExhaustFxSystem->SetConstTime(1, std::fabs(power));
        if (firstExhaustFxSystem->GetPlayStatus() == eFxSystemPlayStatus::FX_PLAYING && firstExhaustSubmergedInWater)
            firstExhaustFxSystem->Stop();
        else if (firstExhaustFxSystem->GetPlayStatus() == eFxSystemPlayStatus::FX_STOPPED && !firstExhaustSubmergedInWater)
            firstExhaustFxSystem->Play();
    } else if (!firstExhaustSubmergedInWater && rwMatrix) {
        firstExhaustFxSystem = g_fxMan.CreateFxSystem("nitro", exhaustPosition, rwMatrix, true);
        m_exhaustNitroFxSystem[0] = firstExhaustFxSystem;
        if (firstExhaustFxSystem) {
            firstExhaustFxSystem->SetLocalParticles(true);
            firstExhaustFxSystem->Play();
        }
    }

    if (m_pHandlingData->m_bDoubleExhaust) {
        FxSystem_c* secondExhaustFxSystem = m_exhaustNitroFxSystem[1];
        if (secondExhaustFxSystem) {
            secondExhaustFxSystem->SetConstTime(1, std::fabs(power));
            if (secondExhaustFxSystem->GetPlayStatus() == eFxSystemPlayStatus::FX_PLAYING && secondExhaustSubmergedInWater)
                secondExhaustFxSystem->Stop();
            else if (secondExhaustFxSystem->GetPlayStatus() == eFxSystemPlayStatus::FX_STOPPED && !secondExhaustSubmergedInWater)
                secondExhaustFxSystem->Play();
        } else if (!firstExhaustSubmergedInWater && rwMatrix) {
            secondExhaustFxSystem = g_fxMan.CreateFxSystem("nitro", secondExhaustPosition, rwMatrix, true);
            m_exhaustNitroFxSystem[1] = secondExhaustFxSystem;
            if (secondExhaustFxSystem) {
                secondExhaustFxSystem->SetLocalParticles(1);
                secondExhaustFxSystem->Play();
            }
        }
    }
}

// 0x6A3E60
void CAutomobile::StopNitroEffect() {
    for (auto&& fx : m_exhaustNitroFxSystem) {
        FxSystem_c::SafeKillAndClear(fx);
    }
}

// 0x6A3EA0
void CAutomobile::NitrousControl(int8 boost) {
    CPad* pad = nullptr;
    if (GetStatus() == STATUS_PLAYER && m_pDriver->IsPlayer()) {
        pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
    }

    if (boost > 0) {
        handlingFlags.bNosInst = true;
        m_fTireTemperature = 1.0f;
        m_nNitroBoosts = boost;
        StopNitroEffect();
        return;
    }

    if (boost < 0) {
        handlingFlags.bNosInst = false;
        m_fTireTemperature = 1.0f; // todo: FIX_BUGS: 0.0f?
        m_nNitroBoosts = 0;
        StopNitroEffect();
        return;
    }

    if (m_fTireTemperature == 1.0f && m_nNitroBoosts > 0) {
        if (GetStatus() != STATUS_PHYSICS) {
            if (   !pad
                || !pad->GetCarGunFired()
                || pad->GetLookLeft()
                || pad->GetLookRight()
                || pad->GetLookBehindForCar()
            ) {
                StopNitroEffect();
                return;
            }
        }

        m_fTireTemperature = -0.000001f; // Just set some small negative value

        if (m_nNitroBoosts >= 101) { // todo: magic number
            StopNitroEffect();
            return;
        }

        m_nNitroBoosts -= 1;
        StopNitroEffect();
        return;
    }

    if (m_fTireTemperature >= 0.f) {
        const auto a = std::max(0.25f, 1.f - m_GasPedal);
        const auto b = a * (CTimer::GetTimeStep() / 100.f) + m_fTireTemperature;
        m_fTireTemperature = std::min(1.0f, b);
        const auto power = (1.0f - m_fTireTemperature) / 2.f;
        DoNitroEffect(power);
        return;
    }

    m_fTireTemperature -= CTimer ::GetTimeStep() * ExeRecip(1000.f);
    if (m_fTireTemperature < -1.f) {
        m_fTireTemperature = 0.000001f; // Just set some small positive value.
        if (!m_nNitroBoosts) {
            handlingFlags.bNosInst = false;
            RemoveUpgrade(eVehicleUpgradePosn::UPGRADE_NITRO);
            m_fTireTemperature = 1.0f;
        }
    }
    DoNitroEffect(
        m_GasPedal <= 0.0f
            ? 0.5f
            : std::abs(m_GasPedal) / 2.0f + 0.5f
    );
}

template<typename T>
auto Get(CEntity** entities, size_t numEntities)
    requires std::is_base_of_v<CEntity, T>
{
    return (
        std::span{ entities, numEntities }
      | std::views::transform([](auto&& entity) { return reinterpret_cast<T*>(entity); })
    );
};

// 0x6A40F0
void CAutomobile::TowTruckControl() {
    const auto UP_SPEED   = 6.0f;
    const auto DOWN_SPEED = 2.0f;

    if (GetStatus() != STATUS_PLAYER) {
        return;
    }

    if (!m_pDriver || !m_pDriver->IsPlayer()) {
        return;
    }

    const auto driversPad = m_pDriver->AsPlayer()->GetPadFromPlayer();
    if (!driversPad) {
        return;
    }

    // Update misc door. angle
    const auto carUpDown = driversPad->GetCarGunUpDown();
    if (std::fabs(carUpDown) > 10.f) {
        const auto speed = carUpDown > 0 ? DOWN_SPEED : UP_SPEED;
        const auto step = NOTSA_FTOL(uint16, float(carUpDown) * speed * CTimer::GetTimeStep());

        while (true) {
            // up
            if (carUpDown > 0) {
                m_wMiscComponentAngle = std::max(
                    m_pVehicleBeingTowed ? TOWTRUCK_HOIST_UP_LIMIT : 0,
                    m_wMiscComponentAngle - step
                );
            } else if (m_wMiscComponentAngle < TOWTRUCK_HOIST_DOWN_LIMIT) { // down
                m_wMiscComponentAngle = std::min<uint16>(
                    TOWTRUCK_HOIST_DOWN_LIMIT,
                    m_wMiscComponentAngle - step
                );
            }
            break;
        }
    }

    // Attach a suitable plyrveh in range if we don't already have a trailer
    if (m_wMiscComponentAngle != TOWTRUCK_HOIST_DOWN_LIMIT || m_pVehicleBeingTowed)
        return;

    CVector towBarPos{};
    if (!GetTowBarPos(towBarPos, false, this))
        return;

    CEntity* entitiesInRange[16]{};
    int16 numEntitiesInRange{};
    CVector hitchPos{};
    CWorld::FindObjectsInRange(towBarPos, 10.f, true, &numEntitiesInRange, (int16)std::size(entitiesInRange), entitiesInRange, false, true, false, false, false);

    for (auto* vehicle : Get<CVehicle>(entitiesInRange, numEntitiesInRange)) {
        if (vehicle == this)
            continue;

        if (!vehicle->GetTowHitchPos(hitchPos, true, this))
            continue;

        if (vehicle->vehicleFlags.bIsLocked != 0u)
            continue;

        if (std::fabs(hitchPos.z - towBarPos.z) < 1.0f && DistanceBetweenPointsSquared2D(towBarPos, hitchPos) < sq(0.5f)) {
            vehicle->SetTowLink(this, false);
            m_wMiscComponentAngle -= 100; // "hide" hoist
            break;
        }
    }
}

// 0x6A44C0
CPed* CAutomobile::KnockPedOutCar(eWeaponType type, uint16 a2, CPed* ped) {
    return ped;
}

// 0x6A44D0
// UNUSED - Likely inlined?
void CAutomobile::PopBootUsingPhysics() {
    // 0x6A44D0: OK -> OPENED, then falls through; OK / DAMAGED -> OPENED_DAMAGED; every other state (opened, opened+damaged, missing) keeps its status
    switch (m_damageManager.GetDoorStatus(eDoors::DOOR_BOOT)) {
    case DAMSTATE_OK:
        m_damageManager.SetDoorStatus(eDoors::DOOR_BOOT, DAMSTATE_OPENED);
        [[fallthrough]];
    case DAMSTATE_DAMAGED:
        m_damageManager.SetDoorStatus(eDoors::DOOR_BOOT, DAMSTATE_OPENED_DAMAGED);
        break;
    default:
        break;
    }
    m_doors[eDoors::DOOR_BOOT].m_angVel = -2.f;
}

// 0x6A4520
void CAutomobile::CloseAllDoors() {
    const auto& mi = *GetVehicleModelInfo();

    const auto CloseDoor = [this](tComponent comp, eDoors door) {
        if (!IsDoorMissing(door)) {
            OpenDoor(nullptr, comp, door, 0.f, true);
        }
    };

    assert(mi.m_nNumDoors > 0);
    CloseDoor(tComponent::COMPONENT_DOOR_RR, eDoors::DOOR_LEFT_FRONT); // TODO: Again, enums dont seem to match up..
    if (mi.m_nNumDoors > 1) {
        CloseDoor(tComponent::COMPONENT_DOOR_RF, eDoors::DOOR_RIGHT_FRONT);
        if (mi.m_nNumDoors > 2) {
            assert(mi.m_nNumDoors == 4); // Pirulax: May assert, I'm not sure. Let me know if it did.

            CloseDoor(tComponent::COMPONENT_WING_LF, eDoors::DOOR_LEFT_REAR); // TODO: Enums dont match at all..
            CloseDoor(tComponent::COMPONENT_DOOR_LR, eDoors::DOOR_RIGHT_REAR);
        }
    }
}

// 0x6A4AF0
// x87 register/spill pattern of the exe (only visible beyond the float range / denormals, see CBike::DoSoftGroundResistance): upSpeed, upX, the sand offroad ability, the force factor and
// the rail speed x/y live in registers (`double`), up y/z, the remaining speed, the normalised speed and the rail force components are spilled to float.
// Both range tests are `!(a > b)` in the exe (`fcompp / test ah,0x41 / jne`): NaN takes the bogged-down branch.
void CAutomobile::DoSoftGroundResistance(uint32& extraHandlingFlags) {
    if (IsAnyWheelTouchingSand() && m_nModelIndex != MODEL_RCBANDIT && m_nModelIndex != MODEL_RHINO) {
        const auto&  up      = GetUp();
        const x87d upSpeed = m_vecMoveSpeed.z * up.z + m_vecMoveSpeed.y * up.y + m_vecMoveSpeed.x * up.x;
        const x87d upX     = upSpeed * up.x;
        const float  upY     = (float)(upSpeed * up.y);
        const float  upZ     = (float)(upSpeed * up.z);
        float        sx      = (float)(m_vecMoveSpeed.x - upX);
        float        sy      = (float)(m_vecMoveSpeed.y - upY);
        const x87d szReg   = m_vecMoveSpeed.z - upZ; // `fst`: spilled, but the z square multiplies the register by the spilled copy
        float        sz      = (float)szReg;
        const x87d sumSq   = szReg * sz + (x87d)sy * sy + (x87d)sx * sx;

        x87d offroad;
        if (!(sumSq > 0.3f * 0.3f)) {
            autoFlags.bIsBoggedDownInSand = true;
            offroad = 0.005f;
        } else {
            const x87d mag  = std::sqrt(sumSq);
            const x87d x03  = sx * 0.3f;
            const x87d y03  = sy * 0.3f;
            const float  z03  = (float)(sz * 0.3f);
            const x87d inv  = (x87d)1.0 / mag;
            const float  invF = (float)inv;
            sx = (float)(inv * x03);
            sy = (float)(y03 * invF);
            sz = (float)(z03 * invF);

            const x87d ability = 1.0f - (mag + mag);
            offroad = (0.2f > ability ? 0.2f : ability) * 0.005f;
        }

        if (handlingFlags.bOffroadAbility2)
            offroad *= 0.3f;
        else if (handlingFlags.bOffroadAbility)
            offroad *= 0.6f;

        if (CWeather::WetRoads > 0.2f)
            offroad *= (1.2f - CWeather::WetRoads);

        const x87d direction = -(CTimer::GetTimeStep() * m_fMass * offroad);
        ApplyMoveForce(CVector{ (float)(sx * direction), (float)(sy * direction), (float)(sz * direction) });
        extraHandlingFlags |= EXTRA_HANDLING_WHEELS_TOUCHING_SAND;
    } else if (IsAnyWheelTouchingRailTrack()) {
        if (m_nModelIndex != MODEL_RCBANDIT && m_nModelIndex != MODEL_RHINO) {
            const auto&  up      = GetUp();
            const x87d upSpeed = m_vecMoveSpeed.z * up.z + m_vecMoveSpeed.y * up.y + m_vecMoveSpeed.x * up.x;
            const x87d upX     = upSpeed * up.x;
            const float  upY     = (float)(upSpeed * up.y);
            const float  upZ     = (float)(upSpeed * up.z);
            const x87d vx      = m_vecMoveSpeed.x - upX;
            const x87d vy      = m_vecMoveSpeed.y - upY;
            const float  vz      = (float)(m_vecMoveSpeed.z - upZ);
            const float  mult    = (float)-(CTimer::GetTimeStep() * m_fMass * CVehicle::ms_fRailTrackResistance);
            ApplyMoveForce(CVector{ (float)(vx * mult), (float)(vy * mult), vz * mult });
        }
    }
}

// 0x6A4EC0
void CAutomobile::ProcessCarWheelPair(eCarWheel leftWheel, eCarWheel rightWheel, float steerAngle, CVector* contactSpeeds, CVector* contactPoints, float traction, float acceleration, float brake, bool bFront)
{
    bool driveWheels = false; // TODO: Refactor please
    if (bFront && gHandlingDataMgr.HasFrontWheelDrive(m_pHandlingData->m_nVehicleId))
        driveWheels = true;
    else if (!bFront && gHandlingDataMgr.HasRearWheelDrive(m_pHandlingData->m_nVehicleId))
        driveWheels = true;

    float suspensionBias = 2.0f * m_pHandlingData->m_fSuspensionBiasBetweenFrontAndRear; // TODO: Use lambda 
    if (!bFront) {
        suspensionBias = 2.0f - suspensionBias;

        if (!vehicleFlags.bIsHandbrakeOn || handlingFlags.bHbRearwheelSteer) {
            if (driveWheels && m_bDoingBurnout) {
                brake = 0.0f;
                traction = 0.0f;
                // 0x6A4FC8-0x6A5036: min(1, 3000/turnMass) * (-0.002*turnMass), then *steerAngle, then * right
                const float turnMassTerm = -0.002f * m_fTurnMass;
                const float burnoutScale = std::min(1.0f, 3000.0f / m_fTurnMass) * turnMassTerm;
                CVector point = GetRight() * (burnoutScale * m_fSteerAngle);
                ApplyTurnForce(contactPoints[leftWheel], point);
            } else if (!handlingFlags.bNosInst && gHandlingDataMgr.HasRearWheelDrive(m_pHandlingData->m_nVehicleId)) {
                traction *= m_fTireTemperature;
            }
        } else {
            brake = 20'000.0f;
        }
    }

    if (m_WheelCounts[leftWheel] > 0.0f || m_WheelCounts[rightWheel] > 0.0f) {
        // TOOD: Refactor
        float sinSteerAngle = 0.0f;
        float cosSteerAngle = 0.0f;
        bool canSteer = steerAngle > -100.0f;
        if (canSteer) {
            sinSteerAngle = x87::sin(steerAngle);
            cosSteerAngle = x87::cos(steerAngle);
        }

        const auto neutralHandling = GetStatus() != STATUS_PLAYER && GetStatus() != STATUS_REMOTE_CONTROLLED && handlingFlags.bNpcNeutralHandl;

        // TOOD: Refactor
        float brakeBias = 0.0f;
        float tractionBias = 0.0f;

        if (bFront) {
            brakeBias = neutralHandling ? 1.0f : 2.0f * m_pHandlingData->m_fBrakeBias;
            tractionBias = neutralHandling ? 1.0f : 2.0f * m_pHandlingData->m_fTractionBias;
        } else {
            brakeBias = neutralHandling ? 1.0f : 2.0f - (2.0f * m_pHandlingData->m_fBrakeBias);
            tractionBias = neutralHandling ? 1.0f : 2.0f - (2.0f * m_pHandlingData->m_fTractionBias);
        }

        // TOOD: Below seems like a lot of copy-paste code, maybe refactor?

        CVector wheelFwd{};
        CVector wheelRight{};
        if (m_WheelCounts[leftWheel] > 0.0f) {
            float thrust = driveWheels ? acceleration : 0.0f;

            wheelFwd = GetForward();
            wheelFwd -= DotProduct(wheelFwd, m_wheelColPoint[leftWheel].m_vecNormal) * m_wheelColPoint[leftWheel].m_vecNormal;
            wheelFwd.Normalise();
            wheelRight = CrossProduct(wheelFwd, m_wheelColPoint[leftWheel].m_vecNormal);
            wheelRight.Normalise();
            if (canSteer && m_nModelIndex != MODEL_HYDRA)
            {
                CVector tmp = cosSteerAngle * wheelFwd - sinSteerAngle * wheelRight;
                wheelRight = sinSteerAngle * wheelFwd + cosSteerAngle * wheelRight;
                wheelFwd = tmp;
            }
            m_wheelColPoint[leftWheel].m_nSurfaceTypeA = SURFACE_WHEELBASE;
            float adhesion = g_surfaceInfos.GetAdhesiveLimit(&m_wheelColPoint[leftWheel]) * traction;
            if (TreatAsPlayerForCollisions())
            {
                adhesion *= g_surfaceInfos.GetWetMultiplier(m_wheelColPoint[leftWheel].m_nSurfaceTypeB);
                adhesion *= std::min((((1.0f - m_fWheelsSuspensionCompression[leftWheel]) * 4.0f) * m_pHandlingData->m_fSuspensionForceLevel) * suspensionBias, 2.0f) /* 0x6A5506 / 0x6A598A: the exe's factor order */;
                if (handlingFlags.bOffroadAbility2 && g_surfaceInfos.GetAdhesionGroup(m_wheelColPoint[leftWheel].m_nSurfaceTypeB) > ADHESION_GROUP_ROAD)
                {
                    adhesion *= 1.4f;
                }
                else if (handlingFlags.bOffroadAbility)
                {
                    if (g_surfaceInfos.GetAdhesionGroup(m_wheelColPoint[leftWheel].m_nSurfaceTypeB) > ADHESION_GROUP_ROAD)
                        adhesion *= 1.15f;
                }
            }
            tWheelState wheelState = m_WheelStates[leftWheel];
            if (m_damageManager.GetWheelStatus(leftWheel) == WHEEL_STATUS_BURST) {
                CVehicle::ProcessWheel(wheelFwd, wheelRight, contactSpeeds[leftWheel], contactPoints[leftWheel], m_nNumContactWheels,
                    thrust,
                    brake * brakeBias,
                    adhesion * m_damageManager.m_fWheelDamageEffect * tractionBias,
                    leftWheel, &m_fWheelBurnoutSpeed[leftWheel], &wheelState, WHEEL_STATUS_BURST);
            }
            else {
                CVehicle::ProcessWheel(wheelFwd, wheelRight, contactSpeeds[leftWheel], contactPoints[leftWheel], m_nNumContactWheels,
                    thrust,
                    brake * brakeBias,
                    adhesion * tractionBias,
                    leftWheel, &m_fWheelBurnoutSpeed[leftWheel], &wheelState, WHEEL_STATUS_OK);
            }
            if (driveWheels && m_GasPedal < 0.0f && wheelState == WHEEL_STATE_SPINNING)
                m_WheelStates[leftWheel] = WHEEL_STATE_NORMAL;
            else
                m_WheelStates[leftWheel] = wheelState;
        }

        if (m_WheelCounts[rightWheel] > 0.0f) {
            float thrust = driveWheels ? acceleration : 0.0f;
            wheelFwd = GetForward();
            wheelFwd -= DotProduct(wheelFwd, m_wheelColPoint[rightWheel].m_vecNormal) * m_wheelColPoint[rightWheel].m_vecNormal;
            wheelFwd.Normalise();
            wheelRight = CrossProduct(wheelFwd, m_wheelColPoint[rightWheel].m_vecNormal);
            wheelRight.Normalise();
            if (canSteer)
            {
                CVector tmp = cosSteerAngle * wheelFwd - sinSteerAngle * wheelRight;
                wheelRight = sinSteerAngle * wheelFwd + cosSteerAngle * wheelRight;
                wheelFwd = tmp;
            }
            m_wheelColPoint[rightWheel].m_nSurfaceTypeA = SURFACE_WHEELBASE;
            float adhesion = g_surfaceInfos.GetAdhesiveLimit(&m_wheelColPoint[rightWheel]) * traction;
            if (TreatAsPlayerForCollisions())
            {
                adhesion *= g_surfaceInfos.GetWetMultiplier(m_wheelColPoint[rightWheel].m_nSurfaceTypeB);
                adhesion *= std::min((((1.0f - m_fWheelsSuspensionCompression[rightWheel]) * 4.0f) * m_pHandlingData->m_fSuspensionForceLevel) * suspensionBias, 2.0f) /* 0x6A5506 / 0x6A598A: the exe's factor order */;
                if (handlingFlags.bOffroadAbility2 && g_surfaceInfos.GetAdhesionGroup(m_wheelColPoint[rightWheel].m_nSurfaceTypeB) > ADHESION_GROUP_ROAD)
                {
                    adhesion *= 1.4f;
                }
                else if (handlingFlags.bOffroadAbility)
                {
                    if (g_surfaceInfos.GetAdhesionGroup(m_wheelColPoint[rightWheel].m_nSurfaceTypeB) > ADHESION_GROUP_ROAD)
                        adhesion *= 1.15f;
                }
            }
            tWheelState wheelState = m_WheelStates[rightWheel];
            if (m_damageManager.GetWheelStatus(rightWheel) == WHEEL_STATUS_BURST) {
                CVehicle::ProcessWheel(wheelFwd, wheelRight, contactSpeeds[rightWheel], contactPoints[rightWheel], m_nNumContactWheels,
                    thrust,
                    brake * brakeBias,
                    adhesion * m_damageManager.m_fWheelDamageEffect * tractionBias,
                    rightWheel, &m_fWheelBurnoutSpeed[rightWheel], &wheelState, WHEEL_STATUS_BURST
               );
            }
            else {
                CVehicle::ProcessWheel(wheelFwd, wheelRight, contactSpeeds[rightWheel], contactPoints[rightWheel], m_nNumContactWheels,
                    thrust,
                    brake * brakeBias,
                    adhesion * tractionBias,
                    rightWheel, &m_fWheelBurnoutSpeed[rightWheel], &wheelState, WHEEL_STATUS_OK
               );
            }
            if (driveWheels && m_GasPedal < 0.0f && wheelState == WHEEL_STATE_SPINNING)
                m_WheelStates[rightWheel] = WHEEL_STATE_NORMAL;
            else
                m_WheelStates[rightWheel] = wheelState;
        }
    }

    if (!bFront && !handlingFlags.bNosInst) {
        if (m_bDoingBurnout && driveWheels &&
            (m_WheelStates[CAR_WHEEL_REAR_LEFT] == WHEEL_STATE_SPINNING ||
            m_WheelStates[CAR_WHEEL_REAR_RIGHT] == WHEEL_STATE_SPINNING))
        {
            m_fTireTemperature += CTimer::GetTimeStep() * ExeRecip(1000.0f);
            m_fTireTemperature = std::min(m_fTireTemperature, 3.0f);
        }
        else if (m_fTireTemperature > 1.0f) {
            m_fTireTemperature = std::powf(0.995f, CTimer::GetTimeStep()) * (m_fTireTemperature - 1.0f) + 1.0f;
        }
    }

    if (IsRealHeli()) {
        return;
    }

    if (m_WheelCounts[leftWheel] <= 0.0f) {
        if (driveWheels && acceleration != 0.0f) {
            if (acceleration > 0.0f) {
                if (m_wheelSpeed[leftWheel] < 1.0f) {
                    m_wheelSpeed[leftWheel] -= 0.1f;
                }
            } else {
                if (m_wheelSpeed[leftWheel] > -1.0f) {
                    m_wheelSpeed[leftWheel] += 0.05f;
                }
            }
        } else {
            m_wheelSpeed[leftWheel] *= 0.95f;
        }
        m_wheelRotation[leftWheel] += CTimer::GetTimeStep() * m_wheelSpeed[leftWheel];
    }

    if (m_WheelCounts[rightWheel] <= 0.0f) {
        if (driveWheels && acceleration != 0.0f) {
            if (acceleration > 0.0f) {
                if (m_wheelSpeed[rightWheel] < 1.0f) {
                    m_wheelSpeed[rightWheel] -= 0.1f;
                }
            } else {
                if (m_wheelSpeed[rightWheel] > -1.0f) {
                    m_wheelSpeed[rightWheel] += 0.05f;
                }
            }
        } else {
            m_wheelSpeed[rightWheel] *= 0.95f;
        }
        m_wheelRotation[rightWheel] += CTimer::GetTimeStep() * m_wheelSpeed[rightWheel];
    }
}

// TODO: Could add these to CMatrix as well, might be useful.. Like: CMatrix::GetRoll()

/*!
* @addr 0x6A6010
* @brief Returns `roll` in DEGREES
*/
float CAutomobile::GetCarRoll() {
    const auto& right = m_matrix->GetRight();
    // 0x6A6010: the magnitude and the fpatan result stay on the x87 stack (no float spill), only the final *(180/PI) result is returned
    const x87d rightMag2D = std::sqrt((x87d)right.x * right.x + (x87d)right.y * right.y);

    // If up.z < 0.f we're flipped, in which case `right` is more like `left` so we have to negate it.
    return (float)(X87R(x87::atan2(right.z, m_matrix->GetUp().z < 0.f ? -rightMag2D : rightMag2D) * (double)RAD_TO_DEG));
}

/*!
* @addr 0x6A6050
* @brief Returns `pitch` in RADIANS
*/
float CAutomobile::GetCarPitch() {
    const auto& fwd = m_matrix->GetForward();
    const auto  fwdMag2D = fwd.Magnitude2D();

    // `up.z` < 0 means we're flipped on the roof, which also means `forward` is more like `backward`, so we have to negate it.
    return x87::atan2(fwd.z, m_matrix->GetUp().z < 0.f ? -fwdMag2D : fwdMag2D);
}

// 0x6A6140
bool CAutomobile::IsInAir() {
    if (physicalFlags.bDontApplySpeed) {
        return true;
    }
    if (physicalFlags.bSubmergedInWater) {
        return false;
    }
    return AreAllWheelsNotTouchingGround() && !m_vecMoveSpeed.IsZero();
}

// 0x6A6DC0
void CAutomobile::dmgDrawCarCollidingParticles(const CVector& position, float force, eWeaponType weapon) {
    if (!GetIsOnScreen()) {
        return;
    }

    switch (weapon) {
    case eWeaponType::WEAPON_UNARMED:
    case eWeaponType::WEAPON_FLOWERS:
        break;
    default: {
        auto fxDirection = m_vecMoveSpeed;
        const auto speedMag = fxDirection.NormaliseAndMag();

        g_fx.AddSparks(
            const_cast<CVector&>(position),
            fxDirection,
            speedMag * -10.f,
            (NOTSA_FTOL(int32, force) / 10 + 4) & 0x3F, // 0x6A6E53..6A6E66: the full ftol(force) / 10 + 4, masked with 0x3F (the port cast it to int8)
            m_vecMoveSpeed,
            eSparkType::SPARK_PARTICLE_SPARK, // 0x6A6E26: sparksType 1
            0.3f,
            1.f
        );
        break;
    }
    }


    CVector fxPos{ lerp(GetPosition(), position, 0.7f) }; // Actually const, just that fx functions dont take consts :D

    // Add smoke
    {
        // The higher our speedsq the more particles we create
        const auto numSmokes = std::max(1u, NOTSA_FTOL(uint32, (m_vecMoveSpeed * CTimer::GetTimeStep()).Magnitude() * 4.f));
        for (auto i = 0u; i < numSmokes; i++) {
            g_fx.m_SmokeHuge->AddParticle(fxPos, {}, 0.f, FxPrtMult_c{ 0.4f, 0.4f, 0.4f, 0.6f, 0.4f, 1.f, 0.1f } /* 0x6A6E93: life 0.1 */);
        }
    }

    // Add debris
    if (m_vecMoveSpeed.SquaredMagnitude() > sq(0.25f)) { // 0x6A6FD9: skipped when |v|^2 <= 0.0625 (the port had it inverted)
        auto color = CVehicleModelInfo::ms_vehicleColourTable[m_nPrimaryColor];
        color.ScaleRGB(m_fContactSurfaceBrightness); // 0x6A700D: only R, G, B are scaled, the alpha stays
        auto rwcolor = color.ToRwRGBA();
        g_fx.AddDebris(fxPos, rwcolor, 0.06f, NOTSA_FTOL(int32, force) / 100 + 1);
    }
}

// 0x6A7090
void CAutomobile::ProcessCarOnFireAndExplode(bool bExplodeImmediately) {
    // 0x6A75FF: the common tail
    const auto DecreaseHealthAndProcess = [this, estatus = m_damageManager.GetEngineStatus()] {
        if (estatus > 225 && m_fHealth > 250.f) {
            m_fHealth -= 2.f;
        }
        ProcessDelayedExplosion();
    };

    // 0x6A758D
    const auto SetFxVelocity = [&, this] {
        if (m_pFireParticle) {
            auto bullshit = GetMoveSpeed() * 50.f;
            m_pFireParticle->SetVelAdd(bullshit);
        }
        DecreaseHealthAndProcess();
    };

    // 0x6A7174: the position is the engine dummy in OBJECT space (the fx is attached to the frame matrix of the clump)
    const auto CreateFx = [&, this](size_t typ) {
        auto pos = GetDummyPosition(DUMMY_ENGINE, false);
        if (const auto mat = GetModellingMatrix()) {
            m_pFireParticle = g_fxMan.CreateFxSystem(
                typ == 1
                    ? "fire_car"
                    : "fire_large",
                pos,
                mat
            );
        }
        if (m_pFireParticle) {
            m_pFireParticle->Play();
            GetEventGlobalGroup()->Add(CEventVehicleOnFire{ this });
        }
    };

    if (m_fHealth < 250.f && GetStatus() != STATUS_WRECKED && !vehicleFlags.bIsDrowning) {
        const auto isSubPlaneOrHeli = notsa::contains({ VEHICLE_TYPE_PLANE, VEHICLE_TYPE_HELI }, m_nVehicleSubType);

        size_t partFxToUse = !m_pFireParticle && !isSubPlaneOrHeli ? 1 : 0;

        const auto floorTsMS = std::floor(CTimer::GetTimeStepInMS());
        if (isSubPlaneOrHeli) { // 0x6A71CD
            if (!m_fireParticleCounter) {
                m_fireParticleCounter = IsInAir() ? 2 : 1;
            }

            if (bExplodeImmediately) { // 0x6A71F3
                BlowUpCar(m_pExplosionVictim, false);
            } else { // 0x6A720B
                const auto isRcShit = notsa::contains({
                    MODEL_RCBARON,
                    MODEL_RCRAIDER,
                    MODEL_RCGOBLIN,
                    MODEL_RCBANDIT,
                    MODEL_RCTIGER
                }, GetModelId());

                m_fBurnTimer += m_fireParticleCounter == 1 || isRcShit // 0x6A7231
                    ? floorTsMS
                    : floorTsMS * ExeRecip(5.f);
                if (m_fBurnTimer > 5000.f) { // 0x6A72A4
                    BlowUpCar(m_pExplosionVictim, false);
                } else { //> 0x6A72BB - Create smoke particle fx
                    // ctor (r, g, b, a, size, rot, life) = (0, 0, 0, 0.4, 1, 1, 0.3); the RC variants override alpha / size / life only
                    FxPrtMult_c fxPrtMult{ 0.f, 0.f, 0.f, 0.4f, 1.f, 1.f, 0.3f };
                    if (isRcShit) {
                        if (GetStatus() == STATUS_REMOTE_CONTROLLED) {
                            fxPrtMult.m_Color.alpha = 0.5f;
                            fxPrtMult.m_fSize       = 0.1f;
                            fxPrtMult.m_fLife       = 0.1f;
                        } else {
                            fxPrtMult.m_Color.alpha = 0.15f;
                            fxPrtMult.m_fSize       = 0.3f;
                            fxPrtMult.m_fLife       = 0.3f;
                        }
                    }
                    if (CTimer::GetFrameCounter() % 2 == 0) { // TODO: Don't use frame counter
                        // 0x6A7351: rand order is vel.x, vel.y, vel.z, pos.x, pos.y (no draw for pos.z)
                        CVector pos = GetPosition();
                        CVector vel;
                        if (isRcShit) {
                            vel.x = CGeneral::GetRandomNumberInRange(-0.5f, 0.5f);
                            vel.y = CGeneral::GetRandomNumberInRange(-0.5f, 0.5f);
                            vel.z = CGeneral::GetRandomNumberInRange(0.f, 0.4f);
                            pos.x += CGeneral::GetRandomNumberInRange(-0.7f, 0.7f);
                            pos.y += CGeneral::GetRandomNumberInRange(-0.7f, 0.7f);
                        } else {
                            vel.x = CGeneral::GetRandomNumberInRange(-1.5f, 1.5f);
                            vel.y = CGeneral::GetRandomNumberInRange(-1.5f, 1.5f);
                            vel.z = CGeneral::GetRandomNumberInRange(0.f, 1.f);
                            pos.x += CGeneral::GetRandomNumberInRange(-2.f, 2.f);
                            pos.y += CGeneral::GetRandomNumberInRange(-2.f, 2.f);
                        }
                        g_fx.m_SmokeHuge->AddParticle(pos, vel, 0.f, fxPrtMult);
                    }
                }

                if (notsa::contains({ STATUS_PLAYER, STATUS_REMOTE_CONTROLLED }, GetStatus())) { // 0x6A74F7
                    if (!m_pFireParticle && m_fBurnTimer > 2500.f) {
                        partFxToUse = 2;
                    }
                } else if (!isRcShit && CGeneral::GetRandomNumberInRange(0, 250) < 3) { // 0x6A7499: int rand in [0, 250) < 3
                    CExplosion::AddExplosion(this, m_pExplosionVictim, EXPLOSION_ROCKET, GetPosition(), 0, true, -1.f, false);
                }
            }
        } else { // 0x6A7126
            m_fBurnTimer += floorTsMS;
            if (m_fBurnTimer > 5000.f) {
                BlowUpCar(m_pExplosionVictim, false);
            }
        }

        // 0x6A716C
        if (partFxToUse != 0) {
            CreateFx(partFxToUse);
        }
        SetFxVelocity();
        return;
    }

    // 0x6A75E2
    m_fBurnTimer = 0.f;
    if (m_pFireParticle) {
        m_pFireParticle->Kill();
        m_pFireParticle = nullptr;
    }

    DecreaseHealthAndProcess();
}

// todo: nodeIndex one of { eBikeNodes eBmxNodes eBoatNodes eHeliNodes eMonsterTruckNodes ePlaneNodes eQuadBikeNodes eTrailerNodes eTrainNodes eCarNodes }
// 0x6A8580
CObject* CAutomobile::SpawnFlyingComponent(eCarNodes nodeIndex, uint32 collisionType)
{
    if (CObject::nNoTempObjects >= 150) {
        return nullptr;
    }

    auto compFrame = m_aCarNodes[(size_t)nodeIndex];
    if (!compFrame) {
        return nullptr;
    }

    // Grab frame's atomic (so we can clone it later)
    RpAtomic* compAtomic{};
    RwFrameForAllObjects(compFrame, GetCurrentAtomicObjectCB, &compAtomic);
    if (!compAtomic) {
        return nullptr; // No atomic means there's nothing to render :D
    }

    GetObjectPool()->SetDealWithNoMemory(true);
    auto obj = new CObject;
    GetObjectPool()->SetDealWithNoMemory(false);

    if (!obj) {
        return nullptr;
    }

    const auto& frameLTM = RwFrameGetLTM(compFrame);

    if (nodeIndex == eCarNodes::CAR_WINDSCREEN) {
        obj->SetModelIndexNoCreate(MODEL_TEMPCOL_BONNET1);
    } else {
        switch (collisionType) {
        case 0u: {
            obj->SetModelIndexNoCreate(MODEL_TEMPCOL_BUMPER1);

            CMatrix frameMat{ frameLTM, false };
            obj->m_vecCentreOfMass = CVector{
                -DotProduct(frameMat.GetPosition() - GetPosition(), m_matrix->GetRight()), // Set it's X centre on mass depending on where it is on our local X axis
                0.f,
                0.f
            };
            break;
        }
        case 1u: {
            obj->SetModelIndexNoCreate(MODEL_TEMPCOL_WHEEL1);
            break;
        }
        case 2u: {
            obj->SetModelIndexNoCreate(MODEL_TEMPCOL_DOOR1);
            obj->m_vecCentreOfMass = CVector{ 0.f, -0.5f, 0.f };
            obj->m_bDrawLast = true;
            break;
        }
        case 3u: {
            obj->SetModelIndexNoCreate(MODEL_TEMPCOL_BONNET1);
            obj->m_vecCentreOfMass = CVector{ 0.f, 0.4f, 0.f };
            break;
        }
        case 4u: {
            obj->SetModelIndexNoCreate(MODEL_TEMPCOL_BOOT1);
            obj->m_vecCentreOfMass = CVector{ 0.f, -0.3f, 0.f };
            break;
        }
        default: {
            obj->SetModelIndexNoCreate(MODEL_TEMPCOL_PANEL1);
            break;
        }
        }
    }

    obj->RefModelInfo(m_nModelIndex);

    // Create a new frame using the flying object's atomic
    // 0x6A8759-0x6A8786: create the frame, clone the atomic, copy the LTM into the frame matrix, THEN set the frame on the atomic (the port set it before the copy)
    auto flyingObjFrame = RwFrameCreate();
    auto clonedFlyingObjAtomic = RpAtomicClone(compAtomic);  // Clone this component's atomic
    *RwFrameGetMatrix(flyingObjFrame) = *frameLTM; // Set this frame's matrix to be the same as the component's - TODO: This most likely isn't the correct way to do this..
    RpAtomicSetFrame(clonedFlyingObjAtomic, flyingObjFrame); // Associate the cloned atomic with a the new frame
    CVisibilityPlugins::SetAtomicRenderCallback(clonedFlyingObjAtomic, nullptr);
    obj->AttachToRwObject((RwObject*)clonedFlyingObjAtomic, true);
    obj->m_bDontStream = true;
    obj->m_fMass = 10.f;
    obj->m_fTurnMass = 25.f;
    obj->m_fAirResistance = 0.97f;
    obj->m_fElasticity = 0.1f;
    obj->m_fBuoyancyConstant = std::bit_cast<float>(0x3DDA740Fu); // 0x6A87E3: the exe literal (0.106666f is a different float, 0x3DDA73B4)
    obj->m_nObjectType = OBJECT_TEMPORARY;

    obj->SetIsStatic(false);

    // 0x6A87F7-0x6A8834: the detached part takes the car's colours and remap texture (the port left them unset: parts rendered with colour 0)
    obj->m_nObjectFlags = (obj->m_nObjectFlags & ~0x1u) | 0x80u; // bIsPickup off, bChangesVehColor on
    obj->m_nCarColor[0] = m_nPrimaryColor;
    obj->m_nCarColor[1] = m_nSecondaryColor;
    obj->SetRemapTexture(m_pRemapTexture, m_nRemapTxd);

    CObject::nNoTempObjects++;
    // 0x6A883F: > 20: time + 4000.0f and > 10: time + 10000.0f are float adds (PC24) converted back; <= 10: integer add
    if (CObject::nNoTempObjects <= 20u) {
        if (CObject::nNoTempObjects <= 10u) {
            obj->m_nRemovalTime = CTimer::GetTimeInMS() + 20'000;
        } else {
            obj->m_nRemovalTime = NOTSA_FTOL(uint32, (x87d)CTimer::GetTimeInMS() + (x87d)10000.0f);
        }
    } else {
        obj->m_nRemovalTime = NOTSA_FTOL(uint32, (x87d)CTimer::GetTimeInMS() + (x87d)4000.0f);
    }

    // 0x6A88D5-0x6A8931: the third test is the NODE (windscreen), not collision type 18; the car must be upright (up.z > 0.0, a double compare), not flipped
    const bool isBonnetBootOrWindscreen = collisionType == 3 || collisionType == 4 || nodeIndex == eCarNodes::CAR_WINDSCREEN;
    const auto GetMoveSpeedZ = [&, this] {
        if (m_vecMoveSpeed.z <= 0.f) {
            if (m_matrix->GetUp().z > 0.0f && isBonnetBootOrWindscreen) {
                return 0.04f - m_vecMoveSpeed.z * 1.5f;
            }
            return m_vecMoveSpeed.z * 0.25f;
        } else {
            return m_vecMoveSpeed.z * 1.5f;
        }
    };
    obj->m_vecMoveSpeed = CVector{ m_vecMoveSpeed.x * 0.75f, m_vecMoveSpeed.y * 0.75f, GetMoveSpeedZ() };
    obj->m_vecTurnSpeed = m_vecTurnSpeed * 2.f;

    auto objDir = Normalized(obj->GetPosition() - GetPosition());
    if (isBonnetBootOrWindscreen) { // 0x6A89BF
        objDir += m_matrix->GetUp();
        if (m_matrix->GetUp().z > 0.f/*not flipped*/) {
            obj->m_matrix->GetPosition() += m_matrix->GetUp() * m_vecMoveSpeed.Magnitude2D();
        }
    }

    obj->ApplyMoveForce(objDir);

    if (collisionType == 1) {
        obj->m_fTurnMass = 5.f;
        obj->m_vecTurnSpeed.x = 0.5f;
        obj->m_fAirResistance = 0.99f;
    }

    if (GetStatus() == STATUS_WRECKED && IsVisible()) {
        const auto camDirUnnorm = TheCamera.GetPosition() - GetPosition();
        // 0x6A8AC3: inline dot product summing y, z, x
        if (objDir.y * camDirUnnorm.y + objDir.z * camDirUnnorm.z + objDir.x * camDirUnnorm.x > -0.5f) { // Object's direction is opposite to that of the camera's
            auto camDir = Normalized(camDirUnnorm);
            camDir.z += 0.3f;
            obj->ApplyMoveForce(camDir * 5.f); // Apply some move force toward the camera's direction
        }
    }

    if (CCollision::ProcessColModels(*obj->m_matrix, *obj->GetColModel(), *m_matrix, *GetColModel(), CWorld::m_aTempColPts, nullptr, nullptr, false) > 0)
        obj->m_pEntityIgnoredCollision = this;

    if (physicalFlags.bRenderScorched)
        obj->physicalFlags.bRenderScorched = true;

    CWorld::Add(obj);

    return obj;
}

// 0x6A8C00
void CAutomobile::ProcessBuoyancy()
{
    // Unused code in sa function: the three CTimeCycle::GetAmbient*_Obj() results (0x6A8C21..0x6A8C2F) are dropped (pure getters), but the call to the
    // CRT rand() at 0x6A8C36 (result dropped) still advances the one RNG - without it every later draw of the frame is shifted (REPLAY-DIFF A-B, frame 4)
    (void)CGeneral::GetRandomNumber(); // 0x6A8C36

    CVector vecBuoyancyTurnPoint;
    CVector vecBuoyancyForce;
    if (!mod_Buoyancy.ProcessBuoyancy(this, m_fBuoyancyConstant, &vecBuoyancyTurnPoint, &vecBuoyancyForce)) {
        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
        physicalFlags.bTouchingWater = false;

        m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
        for (int32 i = 0; i < 4; ++i) {
            auto& colPoint = m_wheelColPoint[i];
            if (m_fWheelsSuspensionCompression[i] < 1.0F && g_surfaceInfos.IsWater(colPoint.m_nSurfaceTypeB)) {
                auto vecWaterImpactVelocity = (colPoint.m_vecPoint + GetUp() * 0.3F) - GetPosition();
                CVector vecSpeed = GetSpeed(vecWaterImpactVelocity);
            }
        }
        return; // The loop above is pretty much an overcomplicated NOP.
    }

    if (IsSubPlane() && m_vecMoveSpeed.z < -1.0F) {
        BlowUpCar(this, false);
    }

    physicalFlags.bTouchingWater = true;
    auto fTimeStep = std::max(0.01F, CTimer::GetTimeStep());
    // 0x6A8C8B..0x6A8CF9: the exe multiplies by the constant 0.008f (the compiler's mass / 125), in this order: (ts * mass) * 0.008f, then force.z / that
    constexpr auto BUOY_MASS_K = std::bit_cast<float>(0x3C03126Fu); // 0x863984 = 0.008f
    auto fUsedMass = m_fMass * BUOY_MASS_K;
    auto fBuoyancyForceZ = vecBuoyancyForce.z / ((fTimeStep * m_fMass) * BUOY_MASS_K);

    if (fUsedMass > m_fBuoyancyConstant)
        fBuoyancyForceZ = ((fUsedMass / m_fBuoyancyConstant) * fBuoyancyForceZ) * 1.05F; // 0x6A8CE5..0x6A8CF5

    if (physicalFlags.bMakeMassTwiceAsBig)
        fBuoyancyForceZ *= 1.5F;

    auto fBuoyancyForceMult = std::max(0.5F, 1.0F - fBuoyancyForceZ * 0.05F); // 0x6A8D14: multiply by 0.05f, not divide by 20
    // pow() (the exe's _CIpow, 0x822130) returns its result on the x87 stack and 0x6A8D54 stores it as a float
    auto fSpeedMult = (float)pow((double)fBuoyancyForceMult, (double)CTimer::GetTimeStep()); // todo:: shadow var

    if (m_nModelIndex != MODEL_VORTEX
        || GetUp().z <= 0.3F
        || vehicleFlags.bIsDrowning) {

        m_vecMoveSpeed *= fSpeedMult;
        m_vecTurnSpeed *= fSpeedMult;
    }

    bool bHeliRotorKilled = false;
    if (m_pHandlingData->m_bIsHeli && m_fHeliRotorSpeed > 0.15F) {
        bool bForceKillRotor = false;
        if (m_nModelIndex != MODEL_SEASPAR && m_nModelIndex != MODEL_LEVIATHN) {
            auto fUsedForce = std::max(1.0F, fBuoyancyForceZ * 8.0F);
            auto vecMoveForceHeli = ExeDivide(vecBuoyancyForce * -2.0F, fUsedForce); // 0x6A8E5C: call 0x4119D0
            ApplyMoveForce(vecMoveForceHeli);
            m_vecTurnSpeed *= fSpeedMult;
            if (fBuoyancyForceZ <= 0.9F)
                return;

            bForceKillRotor = true;
        }

        if (fBuoyancyForceZ > 3.0F || bForceKillRotor) {
            m_fHeliRotorSpeed = 0.0F;
            bHeliRotorKilled = true;
        }
    }

    physicalFlags.bTouchingWater = true;
    ApplyMoveForce(vecBuoyancyForce);
    ApplyTurnForce(vecBuoyancyForce, vecBuoyancyTurnPoint);

    if (ModelIndices::IsAmphibiousHeli(m_nModelIndex)
        && fBuoyancyForceZ < 3.0F
        && (GetUp().z > -0.5F || fBuoyancyForceZ < 0.6F)) {

        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
        return;
    }

    // 0x6A8F67
    if ((CCheat::IsActive(CHEAT_CARS_ON_WATER) || m_nModelIndex == MODEL_VORTEX)
        && GetStatus() == STATUS_PLAYER
        && GetUp().z > 0.3F
    ) {
        if (!vehicleFlags.bIsDrowning) {
            vehicleFlags.bIsDrowning = false; // todo: FIX_BUGS?
            physicalFlags.bSubmergedInWater = false;
            return;
        }
    }

    if (bHeliRotorKilled || fBuoyancyForceZ >= 1.0F
        || (fBuoyancyForceZ > 0.6F && IsAnyWheelNotMakingContactWithGround())) {

        vehicleFlags.bIsDrowning = true;
        physicalFlags.bSubmergedInWater = true;

        m_vecMoveSpeed.z = std::max(-0.1F, m_vecMoveSpeed.z);

        // 0x6A9079..0x6A90C5: float constants 0.0064f (0x871248), 8.0000009e-6f (0x871244), 0.008f (0x863984) multiplied with the mass
        if (m_fMass * std::bit_cast<float>(0x3BD1B718u) < m_fBuoyancyConstant) // 0x871248 = 0.0064f
            m_fBuoyancyConstant = m_fBuoyancyConstant - m_fMass * std::bit_cast<float>(0x370637BEu); // 0x871244 = 8.000000889e-06f

        if (m_fBuoyancyConstant < m_fMass * BUOY_MASS_K)
            vehicleFlags.bEngineOn = false;

        auto driver = m_pDriver->AsPed();
        ProcessPedInVehicleBuoyancy(driver, true);

        for (const auto passenger : GetPassengers()) {
            ProcessPedInVehicleBuoyancy(passenger, false);
        }
    }
    else {
        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
    }
}

inline void CAutomobile::ProcessPedInVehicleBuoyancy(CPed* ped, bool bIsDriver) {
    if (!ped)
        return;

    ped->physicalFlags.bTouchingWater = true;
    if (!ped->IsPlayer() && autoFlags.bWaterTight)
        return;

    if (!IsSubQuad() || IsAnyWheelMakingContactWithGround()) {
        if (ped->IsPlayer())
            ped->AsPlayer()->HandlePlayerBreath(true, 1.0F);
        else {
            auto pedDamageResponseCalc = CPedDamageResponseCalculator(this, CTimer::GetTimeStep(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, false);
            auto damageEvent = CEventDamage(this, CTimer::GetTimeInMS(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, 0, false, true);
            if (damageEvent.AffectsPed(ped))
                pedDamageResponseCalc.ComputeDamageResponse(ped, damageEvent.m_damageResponse, true);
            else
                damageEvent.m_damageResponse.m_bDamageCalculated = true;

            ped->GetEventGroup().Add(&damageEvent, false);
        }
    } else {
        auto fDamageIntensity = m_vecMoveSpeed.Magnitude() * m_fMass; // TODO: It's "force"

        auto knockOffBikeEvent = CEventKnockOffBike(this, m_vecMoveSpeed, (-m_vecMoveSpeed).Normalized(), fDamageIntensity,
            0.0F, KNOCK_OFF_TYPE_FALL, 0, 0, nullptr, false, false);

        ped->GetEventGroup().Add(&knockOffBikeEvent, false);

        if (ped->IsPlayer())
            ped->AsPlayer()->HandlePlayerBreath(true, 1.0F);

        if (bIsDriver)
            vehicleFlags.bEngineOn = false;
    }
}

// 0x6A9680
void CAutomobile::ProcessHarvester()
{
    if (GetStatus() != STATUS_PLAYER)
        return;

    CStreaming::m_bStreamHarvesterModelsThisFrame = true;
    const auto dotYZX = [](const CVector& d, const CVector& m) { return d.y * m.y + d.z * m.z + d.x * m.x; };
    if (m_vecMoveSpeed.Magnitude2D() > 0.01f) {
        if ((CTimer::GetFrameCounter() & 1) != 0)
        {
            for (int32 i = GetPedPool()->GetSize() - 1; i >= 0; i--) {
                CPed* ped = GetPedPool()->GetAt(i);
                if (!ped)
                    continue;

                if (ped->IsPlayer())
                    continue;

                // 0x6A9680: every dot product here sums y, z, x (in that order)
                CVector distance = ped->GetPosition() - GetPosition();
                float distanceForward = dotYZX(distance, GetForward());
                if (distanceForward > 4.0f && distanceForward < 5.0f) {
                    float distanceRight = std::abs(dotYZX(distance, GetRight()));
                    if (distanceRight < 4.0f) {
                        float distanceUp = std::abs(dotYZX(distance, GetUp()));
                        if (distanceUp < 4.0f) {
                            ped->FlagToDestroyWhenNextProcessed();
                            m_harvesterParticleCounter = 20;
                            m_vehicleAudio.AddAudioEvent(AE_BODY_HARVEST, 0.0f);
                        }
                    }
                }
            }
        }
        else
        {
            for (int32 i = GetObjectPool()->GetSize() - 1; i >= 0; i--) {
                CObject* object = GetObjectPool()->GetAt(i);
                if (!object)
                    continue;

                // 0x6A98A0: the OBJECT's model (the port tested this car's), and the object must be visible and not render-damaged
                if (object->m_nModelIndex != ModelIndices::MI_GRASSHOUSE && object->m_nModelIndex != ModelIndices::MI_GRASSPLANT)
                    continue;
                if (!object->m_bIsVisible || object->m_bRenderDamaged)
                    continue;

                CVector distance1 = object->GetPosition() - GetPosition();
                float distanceForward1 = dotYZX(distance1, GetForward());
                if (distanceForward1 > 4.0f && distanceForward1 < 5.0f) {
                    float distanceRight1 = std::abs(dotYZX(distance1, GetRight()));
                    if (distanceRight1 < 4.0f) {
                        float distanceUp1 = std::abs(dotYZX(distance1, GetUp()));
                        if (distanceUp1 < 4.0f)
                            object->ObjectDamage(99999.0f, nullptr, nullptr, this, WEAPON_RUNOVERBYCAR);
                    }
                }
            }
        }
    }

    if (!m_harvesterParticleCounter)
        return;

    CVector pos = m_matrix->TransformPoint(CVector(-1.2f, -3.8f, 1.5f));
    CVector velocity = GetForward() * -0.1f;
    // 0x6A9A2C: ((rand() * (1/32767)) * 0.1 + v) - 0.05 per axis (the port added range(0.05, -0.05) = 0.05 - 0.1t: the mirrored value)
    for (float* v : { &velocity.x, &velocity.y }) {
        const float t = static_cast<float>(CGeneral::GetRandomNumber()) * RAND_MAX_FLOAT_RECIPROCAL;
        *v = t * 0.1f + *v - 0.05f;
    }
    int32 bodyPartModelId = -1;
    switch (m_harvesterParticleCounter - 1) { // todo: remove -1 and refactor?
    case 0:
    case 6:
        bodyPartModelId = ModelIndices::MI_HARVESTERBODYPART1;
        break;
    case 1:
    case 3:
        bodyPartModelId = ModelIndices::MI_HARVESTERBODYPART2;
        break;
    case 2:
        bodyPartModelId = ModelIndices::MI_HARVESTERBODYPART3;
        break;
    case 5:
        bodyPartModelId = ModelIndices::MI_HARVESTERBODYPART4;
        break;
    }

    if (CLocalisation::ShootLimbs() && bodyPartModelId >= 0) {
        auto* limb = new CObject(bodyPartModelId, true);
        limb->CPlaceable::SetMatrix(*m_matrix); // 0x6A9AFD: on the limb (the port called it on this car)
        limb->SetPosn(pos);
        limb->m_vecMoveSpeed = velocity;
        // 0x6A9B25-0x6A9B9A: SetTurnSpeed(x, y, z) with the args evaluated right to left (z first), each rand() * (1/32767) * 0.08 - 0.04 (the port used range(0.12, -0.04))
        const float turnZ = CGeneral::GetRandomNumberInRange(-0.04f, 0.04f);
        const float turnY = CGeneral::GetRandomNumberInRange(-0.04f, 0.04f);
        const float turnX = CGeneral::GetRandomNumberInRange(-0.04f, 0.04f);
        limb->m_vecTurnSpeed = CVector{ turnX, turnY, turnZ };
        limb->m_nObjectType = OBJECT_TEMPORARY;
        limb->UpdateRwMatrix();
        limb->UpdateRwFrame();
        limb->SetIsStatic(false);
        CObject::nNoTempObjects++;
        CWorld::Add(limb);
    }
    m_harvesterParticleCounter--;
    if (CLocalisation::Blood() && m_harvesterParticleCounter % 3 == 0) {
        g_fx.m_SmokeII3expand->AddParticle(pos, velocity, 0.0f, { 0.15f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f });
    }
}

// 0x6A9D70
void CAutomobile::ProcessSwingingDoor(eCarNodes nodeIdx, eDoors doorIdx)
{
    constexpr auto BONNET_SWING_RADIUS = 0.05f;

    auto frame = m_aCarNodes[(size_t)nodeIdx];
    if (!frame) {
        return;
    }

    if (!m_damageManager.IsDoorPresent(doorIdx)) {
        return;
    }

    if (m_damageManager.IsDoorClosed(doorIdx)) {
        if (m_fDamageIntensity <= 100.f || doorIdx == DOOR_BONNET) {
            return;
        }

        switch (GetStatus()) {
        case STATUS_PLAYER:
        case STATUS_PHYSICS:
            break;
        default:
            return;
        }
    }

    CMatrix frameMatrix{ RwFrameGetMatrix(frame) };

    auto& door = m_doors[doorIdx];

    if (m_damageManager.IsDoorClosed(doorIdx) && CanDoorsBeDamaged()) {
        if (door.ProcessImpact(
            this,
            m_moveForce,
            m_turnForce,
            m_matrix->TransformVector(frameMatrix.GetPosition())
        )) {
            m_damageManager.SetDoorOpen(doorIdx);
        }
    }

    // Try opening the doorIdx (If it's not open already)
    if (!m_damageManager.IsDoorOpen(doorIdx)) {
        return;
    }

    // 0x6A9F02
    // If it's the bonnet, we possibly apply some angle velocity based on our current speedsq
    if (doorIdx == eDoors::DOOR_BONNET) {
        auto& bonnet = m_doors[eDoors::DOOR_BONNET];
        if ((bonnet.m_dirn & DOOR_EXTRA_DIRN_MASK) == DOOR_AXIS_Y) {
            // 0x6A9F12: fsin(angle); fadd 0.1; fmul 0.05 - the 0.1 is added AFTER the sin (the port had sin(angle + 0.1)); the dot product sums z, y, x in that order, then + angVel
            const x87d swing = X87R(X87R(x87::sin(bonnet.m_angle) + 0.1f) * BONNET_SWING_RADIUS);
            const auto& fwd  = m_matrix->GetForward();
            bonnet.m_angVel  = (float)(swing * fwd.z * m_vecMoveSpeed.z + swing * fwd.y * m_vecMoveSpeed.y + swing * fwd.x * m_vecMoveSpeed.x + bonnet.m_angVel);
        }
    }

    // 0x6A9F91
    // Now process it, if successful close it and do AudioFX
    if (door.Process(
        this,
        m_moveForce,
        m_turnForce,
        m_matrix->TransformVector(frameMatrix.GetPosition())
    )) {
        m_damageManager.SetDoorClosed(doorIdx);
        m_vehicleAudio.AddAudioEvent((eAudioEvents)((int32)AE_CAR_BONNET_CLOSE + (int32)doorIdx), 0.f);
    }

    // 0x6A9FCE
    door.UpdateFrameMatrix(frameMatrix);
   
    // Possibly detach bonnet and let it fly
    if (doorIdx == eDoors::DOOR_BONNET) {
        if (   door.m_doorState == DAMSTATE_OPENED                      // Still open (couldn't close it) ; todo: Comparison of different enumeration types ('eDoorState' and 'ePanelDamageState') is deprecated
            && DotProduct(m_vecMoveSpeed, m_matrix->GetForward()) > 0.4f // Speed's direction is kinda forwards
        ) {
            auto* flyingObj = SpawnFlyingComponent(CAR_BONNET, 2)->AsVehicle();

            m_vehicleAudio.AddAudioEvent(AE_BONNET_FLUBBER_FLUBBER, flyingObj);
            SetComponentVisibility(m_aCarNodes[CAR_BONNET], eAtomicComponentFlag::ATOMIC_NONE);
            m_damageManager.SetDoorStatus(DOOR_BONNET, DAMSTATE_NOTPRESENT);

            // Apply some additional forces to the flying component
            if (flyingObj) {
                // Apply move speedsq (with some randomness in up/down direction)
                flyingObj->m_vecMoveSpeed = m_vecMoveSpeed * 0.4f + m_matrix->GetRight() * 0.1f;
                if (CGeneral::GetRandomNumber() % 2) {
                    flyingObj->m_vecMoveSpeed += m_matrix->GetUp() / 2.f;
                } else {
                    flyingObj->m_vecMoveSpeed -= m_matrix->GetUp() / 2.f;
                }
                flyingObj->ApplyTurnForce(m_matrix->GetUp() * 10.f, m_matrix->GetForward());
            }
        }
    }
}

/*!
* @addr 0x6AA200
* @brief Remove bonnet if it's present and open
* @returns Flying component object (May be null even in case the bonnet was removed - See `SpawnFlyingComponent` as to when this happens)
*/
CObject* CAutomobile::RemoveBonnetInPedCollision() {
    if (!m_aCarNodes[eCarNodes::CAR_BONNET]) {
        return nullptr; // Not present
    }

    if (!m_damageManager.IsDoorPresent(eDoors::DOOR_BONNET) || !m_damageManager.IsDoorOpen(eDoors::DOOR_BONNET)) {
        return nullptr; // Not open/missing
    }

    if (const auto& bonnet = m_doors[eDoors::DOOR_BONNET]; bonnet.GetAngleWhenOpen() * 0.4f >= bonnet.m_angle) {
        return nullptr; // Not open enough
    }

    auto* const flyingComp = SpawnFlyingComponent(eCarNodes::CAR_BONNET, 2u);
    m_vehicleAudio.AddAudioEvent(eAudioEvents::AE_BONNET_FLUBBER_FLUBBER, flyingComp);
    SetComponentVisibility(m_aCarNodes[eCarNodes::CAR_BONNET], eAtomicComponentFlag::ATOMIC_NONE);
    m_damageManager.SetDoorStatus(eDoors::DOOR_BONNET, eDoorStatus::DAMSTATE_NOTPRESENT);
    return flyingComp;
}

// 0x6AA290
void CAutomobile::UpdateWheelMatrix(int32 nodeIndex, int32 flags) {
    // Sanity checks for the raw offsets used by the original
    VALIDATE_OFFSET(CAutomobile, m_aCarNodes, 0x648); VALIDATE_OFFSET(CAutomobile, m_wheelColPoint, 0x724); VALIDATE_OFFSET(CAutomobile, m_fWheelsSuspensionCompressionPrev, 0x7E4);
    VALIDATE_OFFSET(CAutomobile, m_wheelRotation, 0x828); VALIDATE_OFFSET(CAutomobile, m_wheelPosition, 0x838); VALIDATE_OFFSET(CAutomobile, m_fFrontHeightAboveRoad, 0x898);
    VALIDATE_OFFSET(CAutomobile, m_damageManager, 0x5A0); VALIDATE_OFFSET(CAutomobile, autoFlags, 0x868);
    VALIDATE_OFFSET(CAutomobile, m_fWheelScale, 0x458); VALIDATE_OFFSET(CAutomobile, m_fSteerAngle, 0x494); VALIDATE_OFFSET(CAutomobile, m_f2ndSteerAngle, 0x498);
    VALIDATE_OFFSET(CAutomobile, m_nVehicleSubType, 0x594); VALIDATE_OFFSET(CAutomobile, m_nHandlingFlagsIntValue, 0x38C); VALIDATE_OFFSET(CAutomobile, m_nVehicleUpperFlags, 0x428);
    VALIDATE_OFFSET(CVehicleModelInfo, m_fWheelSizeFront, 0x40); VALIDATE_OFFSET(CVehicleModelInfo, m_fWheelSizeRear, 0x44); VALIDATE_OFFSET(CVehicleModelInfo, m_nWheelModelIndex, 0x48);
    static_assert(offsetof(tHandlingData, m_nModelFlags) == 0xCC && WHEEL_STATUS_BURST == 1 && CAR_WHEEL_FRONT_LEFT == 0 && CAR_WHEEL_REAR_LEFT == 1 && CAR_WHEEL_FRONT_RIGHT == 2 && CAR_WHEEL_REAR_RIGHT == 3);

    constexpr auto PI = std::numbers::pi_v<float>;

    auto* const frame = m_aCarNodes[nodeIndex];
    if (!frame) {
        return;
    }

    bool   isFront = false; // front wheel (LF/RF)
    bool   isRear  = false; // everything else (LM/LB/RM/RB)
    int32  wheel   = CAR_WHEEL_FRONT_LEFT;
    float  dir     = 0.0f;  // -1 for the left side, +1 for the right side (uninitialized in the original for invalid nodes, but then it's never used)
    float  steer   = 0.0f;  // rotation around Z
    bool   steerFixed = false; // `steer` is final (skips the steering adjustments)

    auto* const mi = GetVehicleModelInfo();
    CMatrix     mat{};

    switch (nodeIndex) {
    case CAR_WHEEL_LF:
        wheel   = CAR_WHEEL_FRONT_LEFT;
        dir     = -1.0f;
        isFront = true;
        if (handlingFlags.bSteerRearwheels) {
            steer      = PI;
            steerFixed = true;
        } else {
            steer = m_fSteerAngle;
        }
        break;
    case CAR_WHEEL_RF:
        wheel   = CAR_WHEEL_FRONT_RIGHT;
        dir     = 1.0f;
        isFront = true;
        if (handlingFlags.bSteerRearwheels) {
            steer      = 0.0f;
            steerFixed = true;
        } else {
            steer = m_fSteerAngle;
        }
        break;
    case CAR_WHEEL_LM:
    case CAR_WHEEL_LB:
        wheel  = CAR_WHEEL_REAR_LEFT;
        dir    = -1.0f;
        isRear = true;
        if (handlingFlags.bSteerRearwheels) {
            steer = -m_fSteerAngle;
        } else if (handlingFlags.bHbRearwheelSteer) {
            steer = m_f2ndSteerAngle;
        } else {
            steer      = PI;
            steerFixed = true;
        }
        break;
    case CAR_WHEEL_RM:
    case CAR_WHEEL_RB:
        wheel  = CAR_WHEEL_REAR_RIGHT;
        dir    = 1.0f;
        isRear = true;
        if (handlingFlags.bSteerRearwheels) {
            steer = -m_fSteerAngle;
        } else if (handlingFlags.bHbRearwheelSteer) {
            steer = m_f2ndSteerAngle;
        } else {
            steer      = 0.0f;
            steerFixed = true;
        }
        break;
    default:
        return;
    }

    if (!steerFixed) {
        if (steer == 0.0f || (flags & 2)) { // 0x6AA3CB
            steer = dir < 0.0f ? PI : 0.0f;
        } else if (dir < 0.0f) {
            if (isRear ? (steer > 0.0f) : (steer < 0.0f)) {
                steer = 0.6f * steer;
            }
            steer = steer + PI;
        } else {
            if (isRear ? (steer < 0.0f) : (steer > 0.0f)) {
                steer = 0.6f * steer;
            }
        }
    }

    mat.Attach(RwFrameGetMatrix(frame), false); // 0x6AA490

    CVector savedPos = mat.GetPosition();
    savedPos.z = m_wheelPosition[wheel];

    // `f` lives on the x87 stack (extended precision) until the `SetScale` call
    float  scale = m_fWheelScale;
    x87d f;
    if (mi->m_nWheelModelIndex == -1) {
        if (isRear) {
            scale = (float)(((x87d)mi->m_fWheelSizeRear / (x87d)mi->m_fWheelSizeFront) * (x87d)scale);
        }
        f = (x87d)mi->m_fWheelSizeFront / ((x87d)m_fWheelScale * (x87d)0.7f); // 0x858CB0 = 0.7f
    } else {
        scale = isRear ? mi->m_fWheelSizeRear : mi->m_fWheelSizeFront;
        f     = (x87d)1.0;
    }

    const NOTSA_GLOBAL_LOCAL_NS(s_CombineWheelSize, 0x8D31D4, (float), { 1.7f }); // 1.7f
    const NOTSA_GLOBAL_LOCAL_NS(s_CombineWheelZ, 0x8D31D0, (float), { 0.45f }); // 0.45f
    if (m_nModelIndex == MODEL_COMBINE && (nodeIndex == CAR_WHEEL_LM || nodeIndex == CAR_WHEEL_RM)) {
        scale      = (float)((x87d)s_CombineWheelSize / (x87d)mi->m_fWheelSizeFront);
        savedPos.z = (float)((((x87d)s_CombineWheelSize - (x87d)mi->m_fWheelSizeRear) * (x87d)s_CombineWheelZ) + (x87d)savedPos.z);

        x87d s = steer;
        if (dir < 0.0f) {
            s -= (x87d)PI;
        }
        steer = (float)(s * (x87d)0.5f);
        if (dir < 0.0f) {
            steer = steer + PI;
        }
    } else if (m_nModelIndex == MODEL_STUNT && nodeIndex == CAR_WHEEL_LF) {
        steer = 0.0f;
    }

    if (m_nVehicleSubType == VEHICLE_TYPE_MTRUCK || m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
        f = (x87d)1.0;
    } else if (isRear && (handlingFlags.bWheelRNarrow2 || handlingFlags.bWheelRNarrow || handlingFlags.bWheelRWide || handlingFlags.bWheelRWide2)) {
        if (handlingFlags.bWheelRNarrow2) {
            f *= (x87d)0.65f; // 0x858F50
        } else if (handlingFlags.bWheelRNarrow) {
            f *= (x87d)0.8f;  // 0x858C98
        } else if (handlingFlags.bWheelRWide) {
            f *= (x87d)1.1f;  // 0x858F14
        } else {
            f *= (x87d)1.25f; // 0x8595F0
        }
    } else if (isFront && (handlingFlags.bWheelFNarrow2 || handlingFlags.bWheelFNarrow || handlingFlags.bWheelFWide || handlingFlags.bWheelFWide2)) {
        if (handlingFlags.bWheelFNarrow2) {
            f *= (x87d)0.65f;
        } else if (handlingFlags.bWheelFNarrow) {
            f *= (x87d)0.8f;
        } else if (handlingFlags.bWheelFWide) {
            f *= (x87d)1.1f;
        } else {
            f *= (x87d)1.25f;
        }
    }
    if (m_nModelIndex == MODEL_KART) {
        f *= (x87d)3.0f; // 0x87104C
    }

    mat.SetScale((float)(f * (x87d)scale), scale, scale); // 0x6AA670

    if (m_nVehicleSubType == VEHICLE_TYPE_HELI) {
        mat.Rotate({ 0.0f, 0.0f, steer });
    } else {
        const auto   status = m_damageManager.GetWheelStatus((eCarWheel)wheel); // 0x6AA690
        const x87d rot    = (x87d)dir * (x87d)m_wheelRotation[wheel];
        if (status == WHEEL_STATUS_BURST) {
            mat.Rotate({ (float)rot, 0.0f, (float)(X87R(x87::sin(rot) * (double)0.3f) + (x87d)steer) });
        } else {
            mat.Rotate({ (float)rot, 0.0f, steer });
        }
    }

    const auto  modelFlags = m_pHandlingData->m_nModelFlags;
    const auto  AddParticle = [&] { // 0x6AA7AA
        const auto& colPoint = m_wheelColPoint[wheel];
        const auto& right    = GetMatrix().GetRight();
        const auto& move     = m_vecMoveSpeed;

        const FxPrtMult_c waterMults{ 1.0f, 1.0f, 1.0f, 0.2f, 0.5f, 0.0f, 0.05f };
        const CVector     dirRight{ dir * right.x, dir * right.y, dir * right.z };
        const CVector     halfMove{ 0.5f * move.x, 0.5f * move.y, 0.5f * move.z };
        CVector           vel{ halfMove.x + dirRight.x, halfMove.y + dirRight.y, halfMove.z + dirRight.z };

        if (g_surfaceInfos.IsWater(colPoint.m_nSurfaceTypeB)) { // 0x6AA823
            if ((CTimer::GetFrameCounter() - 1) & 1) {
                vel.z = vel.z + 2.0f;
                g_fx.m_WaterSplash->AddParticle(colPoint.m_vecPoint, vel, 0.0f, waterMults, -1.0f, 1.2f, 0.6f, false); // 0x6AA86F
            } else {
                g_fx.m_SmokeHuge->AddParticle(colPoint.m_vecPoint, vel, 0.0f, waterMults, -1.0f, 1.2f, 0.6f, false); // 0x6AA87F
            }
        } else if (g_surfaceInfos.IsSand(colPoint.m_nSurfaceTypeB)) { // 0x6AA896
            const FxPrtMult_c sandMults{ 0.81f, 0.67f, 0.57f, 0.35f, 0.5f, 0.0f, 0.05f };
            g_fx.m_Sand->AddParticle(colPoint.m_vecPoint, vel, 0.0f, sandMults, -1.0f, 1.2f, 0.6f, false); // 0x6AA8FA
        }
    };

    bool solidAxle = autoFlags.bIsMonsterTruck || handlingFlags.bHydraulicInst || ((modelFlags & 0x20000) && isFront) || ((modelFlags & 0x200000) && isRear); // 0x6AA6D6
    if (!solidAxle) {
        const bool hovering = (CCheat::IsActive(CHEAT_CARS_ON_WATER) || m_nModelIndex == MODEL_VORTEX)
            && m_fWheelsSuspensionCompressionPrev[wheel] < 1.0f
            && !vehicleFlags.bIsDrowning; // 0x6AA724
        if (hovering) {
            if (!(flags & 4)) {
                mat.RotateY((float)((x87d)dir * (x87d)-1.5707964f)); // 0x859998
            }
            if (!(flags & 8)) {
                AddParticle();
            }
        } else if (!(flags & 1) && !(isFront && (modelFlags & 0x10000)) && !(isRear && (modelFlags & 0x100000))) { // 0x6AA904
            const float  groundZ = (float)((x87d)savedPos.z + (x87d)m_fFrontHeightAboveRoad);
            const x87d wheelHeight = (x87d)groundZ - (x87d)mi->GetWheelSize(isFront) * (x87d)0.5f; // 0x6AA94D
            const bool   flipped = (isFront && (modelFlags & 0x80000)) || (isRear && (modelFlags & 0x800000));

            const x87d v = (wheelHeight * -(x87d)1.0) * (x87d)dir;
            float        a;
            if (-1.0 > v) {
                a = -1.0f;
            } else if (1.0 < v) {
                a = 1.0f;
            } else {
                a = (float)v;
            }
            const double angle = x87::asin((double)a);
            mat.RotateY((float)(flipped ? -angle : angle));
        }
    } else {
        // 0x6AAA8F
        const int32 opposite = wheel > 1 ? wheel - 2 : wheel + 2;
        const x87d y       = std::abs((x87d)savedPos.y) * (x87d)2.0;
        const x87d x       = (((x87d)m_wheelPosition[wheel] - (x87d)m_wheelPosition[opposite]) * (x87d)dir) * -(x87d)1.0;
        mat.RotateY((float)x87::atan2(x, y));
    }

    // 0x6AAAD0
    mat.GetPosition().x = mat.GetPosition().x + savedPos.x;
    mat.GetPosition().y = mat.GetPosition().y + savedPos.y;
    mat.GetPosition().z = mat.GetPosition().z + savedPos.z;
    mat.UpdateRW();
}

// 0x6ADEF0
void CAutomobile::PopDoor(eCarNodes nodeIdx, eDoors doorIdx, bool showVisualEffect) {
    if (!m_damageManager.IsDoorPresent(doorIdx)) {
        return;
    }

    if (showVisualEffect) {
        if (nodeIdx != eCarNodes::CAR_NODE_NONE) {
            SpawnFlyingComponent(nodeIdx, nodeIdx == eCarNodes::CAR_CHASSIS ? 4u : 2u);
        } else {
            const auto obj = SpawnFlyingComponent(CAR_NODE_NONE, 3u)->AsVehicle();
            m_vehicleAudio.AddAudioEvent(eAudioEvents::AE_BONNET_FLUBBER_FLUBBER, obj);
        }
    }

    m_damageManager.SetDoorStatus(doorIdx, DAMSTATE_NOTPRESENT);
    SetComponentVisibility(m_aCarNodes[nodeIdx], eAtomicComponentFlag::ATOMIC_NONE);
}

// 0x6ADF80
void CAutomobile::PopPanel(eCarNodes nodeIdx, ePanels panel, bool showVisualEffect) {
    if (m_damageManager.GetPanelStatus(panel) == ePanelDamageState::DAMSTATE_OPENED_DAMAGED) {
        return;
    }

    if (showVisualEffect) {
        switch (nodeIdx) {
        case eCarNodes::CAR_WHEEL_LF:
        case eCarNodes::CAR_WHEEL_LM:
            SpawnFlyingComponent(nodeIdx, 0u);
            break;
        default:
            SpawnFlyingComponent(nodeIdx, 5u);
            break;
        }
    }

    m_damageManager.SetPanelStatus(panel, ePanelDamageState::DAMSTATE_OPENED_DAMAGED);
    SetComponentVisibility(m_aCarNodes[nodeIdx], eAtomicComponentFlag::ATOMIC_NONE);
}

/*!
* @addr 0x6ADFF0
*/
void CAutomobile::ScanForCrimes() {
    const auto plyrveh = FindPlayerVehicle();
    if (   plyrveh
        && plyrveh->IsAutomobile()
        && plyrveh->GetStatus() != STATUS_WRECKED
        && plyrveh->m_nAlarmState
        && plyrveh->m_nAlarmState != 0xFFFF
        && DistanceBetweenPointsSquared(GetPosition(), plyrveh->GetPosition()) < sq(20.f)
    ) {
        FindPlayerPed()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1);
    }
}

// 0x6AE850
void CAutomobile::TankControl()
{
    if (m_nModelIndex == MODEL_RCTIGER && GetStatus() == STATUS_REMOTE_CONTROLLED) {
        if (CPad::GetPad()->CarGunJustDown()) {
            if (CTimer::GetTimeInMS() > m_nGunFiringTime + TIGER_GUNFIRE_RATE) {
                CWeapon minigun(WEAPON_MINIGUN, 5000);
                // 0x6AE8FA-0x6AE950: the exe rotates the gun position only and adds the (world space) timestep * move speed afterwards
                CVector point = m_matrix->TransformPoint(TIGER_GUN_POS) + CTimer::GetTimeStep() * m_vecMoveSpeed;
                minigun.FireInstantHit(this, &point, &point, nullptr, nullptr, nullptr, false, true);
                CVector2D direction(0.0f, 0.1f);
                minigun.AddGunshell(this, point, direction, 0.025f);
                AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_MINIGUN, this);
                m_nGunFiringTime = CTimer::GetTimeInMS();
            }
        }
        return;
    }

    if (GetStatus() != STATUS_PLAYER || m_nModelIndex != MODEL_RHINO)
        return;

    if (CGameLogic::GameState != GAMELOGIC_STATE_PLAYING || !m_pDriver || !m_pDriver->IsPlayer())
        return;

    CPad* pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
    if (!pad)
        return;

    CCam& activeCam = CCamera::GetActiveCamera();
    if (activeCam.m_nMode != MODE_CAM_ON_A_STRING) {
        m_fDoomVerticalRotation   -= ((float)pad->GetCarGunLeftRight() * CTimer::GetTimeStep() * 0.015f) / 128.0f;
        m_fDoomHorizontalRotation += ((float)pad->GetCarGunUpDown() * CTimer::GetTimeStep() * 0.005f) / 128.0f;
    } else {
        CVector frontDot = GetMatrix().InverseTransformVector(activeCam.m_vecFront);
        float doomVerticalRotation   = x87::atan2(-frontDot.x, frontDot.y);
        float doomHorizontalRotation = X87R(x87::atan2(frontDot.z, frontDot.Magnitude2D()) + DegreesToRadians(15));

        if (doomVerticalRotation < m_fDoomVerticalRotation - PI)
            doomVerticalRotation = doomVerticalRotation + TWO_PI;
        else if (doomVerticalRotation > m_fDoomVerticalRotation + PI)
            doomVerticalRotation = doomVerticalRotation - TWO_PI;

        // 0x6AEADF-0x6AEB2F: the vertical rotation snaps to the target within one step (like the horizontal one below)
        const float doomVerticalRotDiff = doomVerticalRotation - m_fDoomVerticalRotation;
        float timeStep = CTimer::GetTimeStep() * 0.015f;
        if (doomVerticalRotDiff > timeStep)
            m_fDoomVerticalRotation += timeStep;
        else if (doomVerticalRotDiff > -timeStep)
            m_fDoomVerticalRotation = (float)doomVerticalRotation;
        else
            m_fDoomVerticalRotation -= timeStep;

        float doomHorizontalRotDiff = doomHorizontalRotation - m_fDoomHorizontalRotation;
        timeStep = CTimer::GetTimeStep() * 0.005f;
        if (doomHorizontalRotDiff > timeStep)
            m_fDoomHorizontalRotation += timeStep;
        else if (doomHorizontalRotDiff >= -timeStep)
            m_fDoomHorizontalRotation = doomHorizontalRotation;
        else
            m_fDoomHorizontalRotation -= timeStep;
    }

    if (m_fDoomVerticalRotation > PI)
        m_fDoomVerticalRotation -= TWO_PI;
    else if (m_fDoomVerticalRotation < -PI)
        m_fDoomVerticalRotation += TWO_PI;

    constexpr float DOOM_HORIZONTAL_ROT_MAX = DegreesToRadians(35);
    constexpr float DOOM_HORIZONTAL_ROT_MIN = -DegreesToRadians(7);

    if (m_fDoomHorizontalRotation > DOOM_HORIZONTAL_ROT_MAX) {
        m_fDoomHorizontalRotation = DOOM_HORIZONTAL_ROT_MAX;
    }
    else if (m_fDoomVerticalRotation > DegreesToRadians(90) || m_fDoomVerticalRotation < -DegreesToRadians(90)) {
        // 0x6AEC95-0x6AECDD: std::max(-1.0f, cos * 1.3f) (0x420800) feeds the formula, there is no separate branch for < -1
        const float cosDoomVerticalRot = std::max(-1.0f, (float)X87R(x87::cos(m_fDoomVerticalRotation) * 1.3f));
        const float doomHorizontalRot = (float)(X87R((x87d)DOOM_HORIZONTAL_ROT_MIN - X87R((x87d)cosDoomVerticalRot * (x87d)(-DegreesToRadians(3) - DOOM_HORIZONTAL_ROT_MIN))));
        if (doomHorizontalRot > m_fDoomHorizontalRotation)
            m_fDoomHorizontalRotation = doomHorizontalRot;
    }
    else if (m_fDoomHorizontalRotation < DOOM_HORIZONTAL_ROT_MIN) {
        m_fDoomHorizontalRotation = DOOM_HORIZONTAL_ROT_MIN;
    }

    if (pad->CarGunJustDown()) { // TODO: Denest
        CPlayerInfo& playerInfo = FindPlayerInfo();
        if (CTimer::GetTimeInMS() > playerInfo.m_nLastTimeBigGunFired + 800) {
            playerInfo.m_nLastTimeBigGunFired = CTimer::GetTimeInMS();
            CVector point;
            point.x = x87::sin(-m_fDoomVerticalRotation);
            point.y = x87::cos(m_fDoomVerticalRotation);
            point.z = x87::sin(m_fDoomHorizontalRotation);
            point = GetMatrix().TransformVector(point);

            CVector newTurretPosition;
            if (m_aCarNodes[CAR_MISC_C]) { // 0x6AED5D
                RwMatrix* carNodeMiscMatrix = RwFrameGetLTM(m_aCarNodes[CAR_MISC_C]);
                newTurretPosition = *RwMatrixGetPos(carNodeMiscMatrix);
                newTurretPosition += GetSpeed(newTurretPosition - GetPosition()) * CTimer::GetTimeStep();
            }
            else {
                float sinVerticalDoomRot = x87::sin(m_fDoomVerticalRotation);
                float cosVerticalDoomRot = x87::cos(m_fDoomVerticalRotation);

                CVector doomOffset;

                doomOffset.x = TANK_SHOT_DOOM_POS.x;
                doomOffset.x += cosVerticalDoomRot * TANK_SHOT_DOOM_DISTANCE_TO_DEFAULT_TARGET.x;
                doomOffset.x -= sinVerticalDoomRot * TANK_SHOT_DOOM_DISTANCE_TO_DEFAULT_TARGET.y;

                // 0x6AEE5F-0x6AEE91: POS.y + (cos * dy + sin * dx)
                doomOffset.y = TANK_SHOT_DOOM_POS.y + (cosVerticalDoomRot * TANK_SHOT_DOOM_DISTANCE_TO_DEFAULT_TARGET.y + sinVerticalDoomRot * TANK_SHOT_DOOM_DISTANCE_TO_DEFAULT_TARGET.x);

                doomOffset.z = TANK_SHOT_DOOM_POS.z;
                doomOffset.z += TANK_SHOT_DOOM_DISTANCE_TO_DEFAULT_TARGET.z - 1.0f;

                // BUG?: This statement is not assigned to any variable.
                // It should be `newTurretPosition =  *m_matrix * doomOffset`
                // Izzotop: Fixed, equal to OG code; untested -> remove ^ after tests
                newTurretPosition = m_matrix->TransformPoint(doomOffset);
            }

            CVector distance = newTurretPosition - GetPosition();
            CVector force = point * (-0.1f * m_fMass); // 0x6AEECC-0x6AEF01: the scalar -0.1 * mass is rounded first
            ApplyForce(force, distance, true);
            CVector endPoint = newTurretPosition + point * 60.0f;
            CWeapon::DoTankDoomAiming(this, m_pDriver, &newTurretPosition, &endPoint);

            CColPoint colPoint{};
            CEntity* entity = nullptr;
            CWorld::pIgnoreEntity = this;
            CWorld::ProcessLineOfSight(
                newTurretPosition,
                endPoint,
                colPoint,
                entity,
                true,
                true,
                true,
                true,
                true,
                true,
                false,
                false
            );
            CWorld::pIgnoreEntity = nullptr;
            CVector explosionPos = endPoint;
            if (entity)
                explosionPos = colPoint.m_vecPoint - (colPoint.m_vecPoint - newTurretPosition) * 0.04f;
            CExplosion::AddExplosion(nullptr, FindPlayerPed(), EXPLOSION_TANK_FIRE, explosionPos, 0, true, -1.0f, false);
            CVector target = explosionPos - newTurretPosition;
            g_fx.TriggerTankFire(newTurretPosition, target);
        }
    }
}

// 0x6AF110
void CAutomobile::BlowUpCarsInPath() {
    if (m_vecMoveSpeed.SquaredMagnitude() < sq(0.1f)) { // Originally `Magnitude() < 0.1f`
        return;
    }

    if (!autoFlags.bTankExplodesCars) {
        return; // 0x6AF145 `test [this+0x868], 0x20 / je` leaves when the bit is CLEAR (the ctor sets it for every vehicle)
    }

    for (auto&& entity : GetCollidingEntities()) {
        if (!entity)
            continue; // I don't think should ever happen? But original code checks...
        if (!entity->GetIsTypeVehicle())
            continue;

        auto& veh = *entity->AsVehicle();
        if (ModelIndices::IsRhino(veh.m_nModelIndex))
            continue;
        if (veh.physicalFlags.bRenderScorched)
            continue;

        if (this == FindPlayerVehicle()) {
            CCrime::ReportCrime(eCrimeType::CRIME_EXPLOSION, &veh, FindPlayerPed());
        }
        veh.BlowUpCar(this, false);
    }
}

// 0x6AF420
void CAutomobile::PlaceOnRoadProperly()
{
    auto* cm = GetColModel();
    auto fStartY =  cm->m_pColData->m_pLines[0].m_vecStart.y;
    auto fEndY   = -cm->m_pColData->m_pLines[3].m_vecStart.y;

    const auto& vecPos = GetPosition();

    auto vecRearCheck = vecPos - GetForward() * fEndY;
    vecRearCheck.z = vecPos.z;

    auto vecFrontCheck = vecPos + GetForward() * fStartY;
    vecFrontCheck.z = vecPos.z;

    bool bColFoundFront = false;
    CColPoint colPoint{};
    CEntity* colEntity;
    float fColZ;
    if (CWorld::ProcessVerticalLine(vecFrontCheck, vecFrontCheck.z + 5.0F, colPoint, colEntity, true)) {
        m_bTunnel = colEntity->m_bTunnel;
        m_bTunnelTransition = colEntity->m_bTunnelTransition;

        fColZ = colPoint.m_vecPoint.z;
        m_pEntityWeAreOn = colEntity;
        bColFoundFront = true;
    }
    // exe 0x6AF420: the lighting / z assignment is shared by the "second line hit, first one closer" case (z from the first hit, lighting from the second colPoint)
    const bool bColFoundBelow = CWorld::ProcessVerticalLine(vecFrontCheck, vecFrontCheck.z - 5.0F, colPoint, colEntity, true);
    if (bColFoundBelow && (!bColFoundFront || std::fabs(vecFrontCheck.z - colPoint.m_vecPoint.z) < std::fabs(vecFrontCheck.z - fColZ))) {
        m_bTunnel = colEntity->m_bTunnel;
        m_bTunnelTransition = colEntity->m_bTunnelTransition;

        fColZ = colPoint.m_vecPoint.z;
        m_pEntityWeAreOn = colEntity;
    }
    if (bColFoundBelow || bColFoundFront) {
        m_FrontCollPoly.ligthing = colPoint.m_nLightingB;
        vecFrontCheck.z = fColZ;
    }

    bool bColFoundRear = false;
    colEntity = nullptr;
    if (CWorld::ProcessVerticalLine(vecRearCheck, vecRearCheck.z + 5.0F, colPoint, colEntity, true)) {
        m_bTunnel = colEntity->m_bTunnel;
        m_bTunnelTransition = colEntity->m_bTunnelTransition;

        fColZ = colPoint.m_vecPoint.z;
        m_pEntityWeAreOn = colEntity;
        bColFoundRear = true;
    }
    // exe 0x6AF420: the lighting / z assignment is shared by the "second line hit, first one closer" case (z from the first hit, lighting from the second colPoint)
    const bool bColFoundBelowRear = CWorld::ProcessVerticalLine(vecRearCheck, vecRearCheck.z - 5.0F, colPoint, colEntity, true);
    if (bColFoundBelowRear && (!bColFoundRear || std::fabs(vecRearCheck.z - colPoint.m_vecPoint.z) < std::fabs(vecRearCheck.z - fColZ))) {
        m_bTunnel = colEntity->m_bTunnel;
        m_bTunnelTransition = colEntity->m_bTunnelTransition;

        fColZ = colPoint.m_vecPoint.z;
        m_pEntityWeAreOn = colEntity;
    }
    if (bColFoundBelowRear || bColFoundRear) {
        m_RearCollPoly.ligthing = colPoint.m_nLightingB;
        vecRearCheck.z = fColZ;
    }

    auto fHeightAboveRoad = GetHeightAboveRoad();
    vecFrontCheck.z += fHeightAboveRoad;
    vecRearCheck.z += m_fRearHeightAboveRoad;

    auto fLength = fEndY + fStartY;
    GetRight().Set((vecFrontCheck.y - vecRearCheck.y) / fLength, -((vecFrontCheck.x - vecRearCheck.x) / fLength), 0.0F);

    auto vecDiff = vecFrontCheck - vecRearCheck;
    vecDiff.Normalise();
    GetForward() = vecDiff;

    auto vecCross = CrossProduct(GetRight(), GetForward());
    GetUp() = vecCross;

    // exe 0x6AF420: 1/length once, then three multiplies (not three divides)
    const float fInvLength = 1.0F / fLength;
    CVector vecNewPos{
        (vecFrontCheck.x * fEndY + vecRearCheck.x * fStartY) * fInvLength,
        (vecFrontCheck.y * fEndY + vecRearCheck.y * fStartY) * fInvLength,
        (vecFrontCheck.z * fEndY + vecRearCheck.z * fStartY) * fInvLength,
    };
    SetPosn(vecNewPos);

    if (IsSubPlane())
        AsPlane()->m_planeCreationHeading = CGeneral::GetATanOfXY(GetForward().x, GetForward().y);
}

// 0x6AF910
void CAutomobile::PopBoot() {
    if (m_damageManager.IsDoorClosed(eDoors::DOOR_BOOT)) {
        auto& door = m_doors[eDoors::DOOR_BOOT];

        door.OpenFully();

        CMatrix frameMat{ RwFrameGetMatrix(m_aCarNodes[eCarNodes::CAR_BOOT]) };
        CVector orien{ 0.f, 0.f, 0.f };
        orien[door.GetAxes()] = door.GetDoorAngle();
        frameMat.SetRotateKeepPos(orien);
        frameMat.UpdateRW();
    }
}

// 0x6AFA20
void CAutomobile::CloseBoot() {
    auto& door = m_doors[eDoors::DOOR_BOOT];

    door.CloseFully();

    // Code copy pasted from `PopBoot`:
    CMatrix frameMat{ RwFrameGetMatrix(m_aCarNodes[eCarNodes::CAR_BOOT]) };
    CVector orien{ 0.f, 0.f, 0.f };
    orien[door.GetAxes()] = door.GetDoorAngle();
    frameMat.SetRotateKeepPos(orien);
    frameMat.UpdateRW();
}

// 0x6B0690
void CAutomobile::DoHeliDustEffect(float timeConstMult, float fxMaxZMult) {
    const auto KillDustFx = [this] {
        if (m_pDustParticle) {
            m_pDustParticle->Kill();
            m_pDustParticle = nullptr;
            m_heliDustFxTimeConst = 0.f;
        }
    };

    const auto& myPos = GetPosition();

    // 0x6B07E9
    // Moved early out here instead
    if (DistanceBetweenPointsSquared(TheCamera.GetPosition(), myPos) >= sq(50.f)) {
        KillDustFx();
        return;
    }

    switch (m_nVehicleSubType) {
    case eVehicleType::VEHICLE_TYPE_HELI: {
        if (m_fHeliRotorSpeed < 0.1125f) {
            KillDustFx();
            return;
        }
        break;
    }
    case eVehicleType::VEHICLE_TYPE_PLANE: {
        break;
    }
    default: {
        KillDustFx();
        return;
    }
    }

    // 0x6B0705

    // Figure out ground position, and if it's underwater

    CColPoint groundCP{};
    CEntity* hitEntity{}; // Unused
    CWorld::ProcessVerticalLine(myPos, -1000.f, groundCP, hitEntity, true); // TODO: Check if it returned true :D
    const bool isGroundSand = g_surfaceInfos.IsSand(groundCP.m_nSurfaceTypeB);

    auto waterLevel{-1000.f};
    const bool isThereWaterUnderUs = CWaterLevel::GetWaterLevel(myPos, waterLevel, false);
    const auto isHitPosUnderWater = isThereWaterUnderUs && waterLevel > groundCP.m_vecPoint.z;

    const auto groundZ = isHitPosUnderWater ? waterLevel : groundCP.m_vecPoint.z;
    const auto distToGroundZ = myPos.z - groundZ;

    // Calculate max height for fx to be played
    const auto GetBaseMaxZForFx = [this]{
        switch (m_nModelIndex) {
        case eModelID::MODEL_RCGOBLIN:
        case eModelID::MODEL_RCRAIDER:
            return 3.f;
        }
        return 30.f;
    };
    const auto maxZForFx = GetBaseMaxZForFx() * fxMaxZMult;

    // Check if we're flying low enough for the fx to be played. If not, kill it, and return.
    if (distToGroundZ >= maxZForFx) {
        KillDustFx();
        return;
    }

    // Early out moved to top

    // Add danger event if we're flying too low to the ground
    if (CTimer::GetFrameCounter() % 20 == 0) {
        const auto dangerRadius = m_nModelIndex == MODEL_HYDRA ? 30.f : 20.f;
        if (distToGroundZ < dangerRadius) {
            CEventDanger danger{this, dangerRadius};
            GetEventGlobalGroup()->Add(&danger);
        }
    }

    // 0x6B08B1
    // Create and play fx if it doesn't exist.
    if (!m_pDustParticle) {
        CVector fxPos{0.f, 0.f, 0.f};
        m_pDustParticle = g_fxMan.CreateFxSystem("heli_dust", CVector{0.f, 0.f, 0.f}, nullptr, true);
        if (!m_pDustParticle) { // Failed, return
            return;
        }
        m_pDustParticle->SetLocalParticles(true);
        m_pDustParticle->Play();
    }

    // Update time constant of fx
    {
        const auto heightProgress = (maxZForFx - distToGroundZ) / maxZForFx * timeConstMult;
        const auto heightProgressSq = heightProgress * heightProgress;

        // TODO: Figure out a better way to do this... Very ugly.
        // Basically it's stepping `m_heliDustFxTimeConst` by 0.04 towards `heightProgressSq`.
        if (m_heliDustFxTimeConst < heightProgressSq) {
            m_heliDustFxTimeConst = std::min(m_heliDustFxTimeConst + 0.04f, heightProgressSq);
        } else if (m_heliDustFxTimeConst > heightProgressSq) { // Must check if less, because they might be equal (tbh, the chance for that to happen are... small)
            m_heliDustFxTimeConst = std::max(m_heliDustFxTimeConst - 0.04f, heightProgressSq);
        }

        m_pDustParticle->SetConstTime(true, m_heliDustFxTimeConst);
    }

    // Update max of fx
    m_pDustParticle->SetOffsetPos({ myPos.x, myPos.y, groundZ });

    // Finally enable/disable prims (whatever that is)
    m_pDustParticle->EnablePrim(0, !isHitPosUnderWater && !isGroundSand); // Dust or similar particles?
    m_pDustParticle->EnablePrim(1, !isHitPosUnderWater && isGroundSand);  // Sand particles?
    m_pDustParticle->EnablePrim(2, isHitPosUnderWater);                   // Water particles?

    // Above code originally (kinda):
    //if (isHitPosUnderWater) {
    //    m_pDustParticle->EnablePrim(0, false);
    //    m_pDustParticle->EnablePrim(1, false);
    //    m_pDustParticle->EnablePrim(2, true);
    //} else {
    //    m_pDustParticle->EnablePrim(0, !isGroundSand);
    //    m_pDustParticle->EnablePrim(1, isGroundSand);
    //    m_pDustParticle->EnablePrim(2, false);
    //}
}

// 0x6B1350
void CAutomobile::SetBumperDamage(ePanels panelIdx, bool withoutVisualEffect) {
    auto nodeIdx = CDamageManager::GetCarNodeIndexFromPanel(panelIdx);
    auto frame = m_aCarNodes[nodeIdx];
    if (!frame) {
        NOTSA_LOG_WARN("Trying to damage component {} of {}", (int32)nodeIdx, CModelInfo::GetModelInfo(m_nModelIndex)->GetModelNameAsString()); // R* log
        return;
    }

    if (!GetVehicleModelInfo()->m_pVehicleStruct->IsComponentDamageable(nodeIdx)) {
        return;
    }

    switch (m_damageManager.GetPanelStatus(panelIdx)) {
    case ePanelDamageState::DAMSTATE_DAMAGED: {
        if (!m_pHandlingData->m_bBouncePanels) { // TODO: Weird... The flag name might be incorrect, because here we actually set the bouncing panelIdx.
            if (auto* panelFrame = CheckIfExistsGetFree(nodeIdx)) {
                panelFrame->SetPanel(nodeIdx, 0, CGeneral::GetRandomNumberInRange(-0.5f, -0.2f));
            }
        }
        break;
    }
    case ePanelDamageState::DAMSTATE_OPENED: {
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_DAMAGED);
        break;
    }
    case ePanelDamageState::DAMSTATE_OPENED_DAMAGED: {
        if (!withoutVisualEffect) {
            SpawnFlyingComponent(nodeIdx, 0u);
        }
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_NONE);
        break;
    }
    }
}

// 0x6B1480
void CAutomobile::SetPanelDamage(ePanels panel, bool createWindowGlass)
{
    auto nodeIdx = CDamageManager::GetCarNodeIndexFromPanel(panel);
    auto frame = m_aCarNodes[nodeIdx];
    if (!frame) {
        return;
    }

    if (!GetVehicleModelInfo()->m_pVehicleStruct->IsComponentDamageable(nodeIdx)) {
        return;
    }

    switch (m_damageManager.GetPanelStatus(panel)) {
    case ePanelDamageState::DAMSTATE_DAMAGED: {
        if (m_pHandlingData->m_bBouncePanels) { // TODO: Weird... The flag name might be incorrect, because here we actually set the bouncing panelIdx.
            return;
        }
        if (auto* panel_ = CheckIfExistsGetFree(nodeIdx)) {
            switch (nodeIdx) {
            case eCarNodes::CAR_WINDSCREEN:
            case eCarNodes::CAR_WING_LF:
            case eCarNodes::CAR_WING_RF:
                break;
            default: {
                panel_->SetPanel(nodeIdx, 1, CGeneral::GetRandomNumberInRange(-0.2f, -0.5f));
                break;
            }
            }
        }
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_DAMAGED);
        break;
    }
    case ePanelDamageState::DAMSTATE_OPENED: {
        if (panel == ePanels::WINDSCREEN_PANEL) {
            m_vehicleAudio.AddAudioEvent(eAudioEvents::AE_WINDSCREEN_SHATTER, 0.f);
        }
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_DAMAGED);
        break;
    }
    case ePanelDamageState::DAMSTATE_OPENED_DAMAGED: {
        if (createWindowGlass) {
            if (panel == ePanels::WINDSCREEN_PANEL) {
                CGlass::CarWindscreenShatters(this);
            }
        } else {
            SpawnFlyingComponent(nodeIdx, 5u);
        }
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_NONE);
        break;
    }
    }
}

// 0x6B1600
void CAutomobile::SetDoorDamage(eDoors doorIdx, bool withoutVisualEffect)
{
    auto nodeIdx = CDamageManager::GetCarNodeIndexFromDoor(doorIdx);
    auto frame = m_aCarNodes[nodeIdx];
    if (!frame) {
        NOTSA_LOG_WARN("Trying to damage component {} of {}", (int32)nodeIdx, CModelInfo::GetModelInfo(m_nModelIndex)->GetModelNameAsString()); // R* log
        return;
    }

    const auto isDamageable = GetVehicleModelInfo()->m_pVehicleStruct->IsComponentDamageable(nodeIdx);

    auto& door{ m_doors[doorIdx] };

    // Leaving this here for further reference...
    // The `if` @ `0x6B1650` is inverted here a little, because
    // `0x6B169C` is only reachable if `eDoors::DOOR_BOOT` in which all other if's are ignored (that is the one at `0x6B1650` and `0x6B1673`)
    // If the doorIdx isn't BOOT, but is BONNET just ignore it, because of `0x6B1660`
    // Now, if it's neither, then we go on and check the logical invert of the 2 other conditions at `0x6B1650`
    // If those are all true we will rtn up @ `0x6B1673`

    switch (doorIdx) {
    case eDoors::DOOR_BOOT: {
        // 0x6B169C
        if (m_pHandlingData->m_bAltSteerOpt) {
            switch (m_damageManager.GetDoorStatus(eDoors::DOOR_BOOT)) {
            case eDoorStatus::DAMSTATE_OPENED: {
                m_damageManager.SetDoorStatus(eDoors::DOOR_BOOT, isDamageable ? eDoorStatus::DAMSTATE_DAMAGED : eDoorStatus::DAMSTATE_NOTPRESENT);
                break;
            }
            case eDoorStatus::DAMSTATE_OPENED_DAMAGED: {
                m_damageManager.SetDoorStatus(eDoors::DOOR_BOOT, eDoorStatus::DAMSTATE_NOTPRESENT);
                break;
            }
            }
        }
        break;
    }
    case eDoors::DOOR_BONNET: // Check from 0x6B1660
        break;

    default: {
        // Inverted `if` from `0x6B1650`, this way we get to the `else` branch at `0x6B1667`
        if (!CanDoorsBeDamaged()) {
            switch (m_damageManager.GetDoorStatus(doorIdx)) { // 0x6B1659
            case eDoorStatus::DAMSTATE_OPENED_DAMAGED:
            case eDoorStatus::DAMSTATE_NOTPRESENT: {
                // 0x6B1667
                door.Open(0.f);
                m_damageManager.SetDoorStatus(doorIdx, eDoorStatus::DAMSTATE_DAMAGED);
                return;
            }
            }
        }
        break;
    }
    }

    // 0x6B16E5
    switch (m_damageManager.GetDoorStatus(doorIdx)) {
    case DAMSTATE_DAMAGED: { // 0x6B16E5
        if (isDamageable) {
            SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_DAMAGED);
        }

        if (door.m_prevAngle != 0.f) {
            door.m_angle = 0.f;
            door.m_prevAngle = 0.f;
            door.m_angVel = 0.f;

            // Reset component rotation
            {
                CMatrix frameMatrix{RwFrameGetMatrix(frame)};
                frameMatrix.SetRotateKeepPos({});
                frameMatrix.UpdateRW();
            }

            m_vehicleAudio.AddAudioEvent((eAudioEvents)((int32)AE_CAR_BONNET_CLOSE + (int32)doorIdx), 0.f);
        }
        break;
    }
    case DAMSTATE_NOTPRESENT: { // 0x6B17F0
        if (!withoutVisualEffect) {
            if (doorIdx == eDoors::DOOR_BONNET) { // Inverted
                const auto obj = SpawnFlyingComponent(nodeIdx, 3u)->AsVehicle();
                m_vehicleAudio.AddAudioEvent(AE_BONNET_FLUBBER_FLUBBER, obj);
            } else {
                SpawnFlyingComponent(nodeIdx, doorIdx == eDoors::DOOR_BOOT ? 4u : 2u);
            }
        }
        SetComponentVisibility(frame, eAtomicComponentFlag::ATOMIC_NONE);
        break;
    }
    case DAMSTATE_OPENED: // 0x6B1834
    case DAMSTATE_OPENED_DAMAGED: {
        RwFrameForAllObjects(frame, CVehicleModelInfo::SetAtomicFlagCB, (void*)eAtomicComponentFlag::ATOMIC_DONT_CULL);
        if (doorIdx == eDoors::DOOR_BONNET) {
            door.m_angVel = 0.2f;
        }
        break;
    }
    }
}

// 0x6B3F70
bool CAutomobile::RcbanditCheck1CarWheels(CPtrListDoubleLink<CVehicle*>& ptrList)
{
    CColModel* colModel = GetVehicleModelInfo()->GetColModel();

    for (auto* const vehicle : ptrList) {
        if (vehicle == this || !vehicle->IsAutomobile())
            continue;

        // 0x6B3FFF: skip the RC Bandit itself (model 0x1B9), skip a vehicle already scanned this scan code, else mark it scanned
        if (ModelIndices::IsRCBandit(vehicle->m_nModelIndex) || !vehicle->ProcessScan())
            continue;

        // 0x6B4036..0x6B40F7: |dx| < 10 && |dy| < 10 (the exe negates a negative difference)
        CVector distance = GetPosition() - vehicle->GetPosition();
        if (std::fabs(distance.x) < 10.0f && std::fabs(distance.y) < 10.0f) {
            auto modelInfo = vehicle->GetVehicleModelInfo();
            for (int32 i = 0; i < 4; i++) {
                // 0x6B4110: the OTHER vehicle's (ebx) previous suspension compression / status
                if (!(vehicle->AsAutomobile()->m_fWheelsSuspensionCompressionPrev[i] < 1.0f) && vehicle->GetStatus() != STATUS_SIMPLE)
                    continue;

                static CMatrix wheelMatrix;
                CVector wheelPos;
                modelInfo->GetWheelPosn(i, wheelPos, false);
                wheelMatrix = Invert(*m_matrix, wheelMatrix);

                CColSphere sphere;
                sphere.m_vecCenter = wheelMatrix.TransformPoint(vehicle->m_matrix->TransformPoint(wheelPos));
                sphere.m_fRadius = modelInfo->m_fWheelSizeFront * 0.25f;
                if (CCollision::TestSphereBox(sphere, colModel->m_boundBox))
                    return true;
            }
        }
    }
    return false;
}

// 0x6B45E0
bool CAutomobile::RcbanditCheckHitWheels() {
    const auto& point = GetPosition();
    float fRadius = 2.0f;
    const float minX = point.x - fRadius;
    const float maxX = point.x + fRadius;
    const float minY = point.y - fRadius;
    const float maxY = point.y + fRadius;
    int32 startSectorX = std::max(CWorld::GetSectorX(minX), 0);
    int32 startSectorY = std::max(CWorld::GetSectorY(minY), 0);
    int32 endSectorX = std::min(CWorld::GetSectorX(maxX), MAX_SECTORS_X - 1);
    int32 endSectorY = std::min(CWorld::GetSectorY(maxY), MAX_SECTORS_Y - 1);
    CWorld::AdvanceCurrentScanCode();
    for (int32 sectorY = startSectorY; sectorY <= endSectorY; ++sectorY) {
        for (int32 sectorX = startSectorX; sectorX <= endSectorX; ++sectorX) {
            auto& repeatSector = CWorld::GetRepeatSector(sectorX, sectorY);
            if (RcbanditCheck1CarWheels(repeatSector.Vehicles))
                return true; // 0x6B47D2: leaves both loops
        }
    }
    return false; // 0x6B47E6: xor ax, ax
}

// 0x729B60
void CAutomobile::FireTruckControl(CFire* fire) {
    if (this != FindPlayerVehicle()) {
        if (GetStatus() != STATUS_PHYSICS || !fire)
            return;

        CVector2D distance = fire->GetPosition() - GetPosition();
        float shootDir = x87::atan2(-distance.x, distance.y);
        float heading = GetHeading();
        if (shootDir > heading + PI)
            shootDir -= TWO_PI;
        else if (shootDir < heading - PI)
            shootDir += TWO_PI;

        float doomVerticalRotation = shootDir - heading;
        if (doomVerticalRotation > m_fDoomVerticalRotation + PI)
            doomVerticalRotation -= TWO_PI;
        else if (doomVerticalRotation < m_fDoomVerticalRotation - PI)
            doomVerticalRotation += TWO_PI;

        float doomVerticalRotDiff = doomVerticalRotation - m_fDoomVerticalRotation;
        float timeStep = CTimer::GetTimeStep() * ExeRecip(100.0f);
        if (std::fabs(doomVerticalRotDiff) >= timeStep) {
            if (doomVerticalRotDiff <= 0.0f)
                m_fDoomVerticalRotation -= timeStep;
            else
                m_fDoomVerticalRotation += timeStep;
        } else {
            m_fDoomVerticalRotation = doomVerticalRotation; // 0x729F5E: within one step: snap to the target (the port left it unchanged)
        }
        // 0x729F64-0x729F90: rand-free wobble, the exe scale is 6.28f / 4096 (0x863988 = 0x3AC8F5C3), not 2*PI / 4096
        m_fDoomHorizontalRotation = X87R(x87::sin(float(CTimer::GetTimeInMS() & 0xFFF) * std::bit_cast<float>(0x3AC8F5C3u)) * 0.15f);
    }
    else {
        CCam& activeCam = CCamera::GetActiveCamera();
        if (activeCam.m_nMode != MODE_CAM_ON_A_STRING)
        {
            CPad* pad = CPad::GetPad();
            m_fDoomVerticalRotation   -= ((float)pad->GetCarGunLeftRight() * CTimer::GetTimeStep() * ExeRecip(20.0f)) / 128.0f;
            m_fDoomHorizontalRotation += ((float)pad->GetCarGunUpDown()    * CTimer::GetTimeStepInSeconds()) / 128.0f;
        }
        else {
            CVector frontDot = GetMatrix().InverseTransformVector(activeCam.m_vecFront);
            float doomVerticalRotation   = x87::atan2(-frontDot.x, frontDot.y);
            float doomHorizontalRotation = x87::atan2(+frontDot.z, frontDot.Magnitude2D());

            if (ModelIndices::IsSwatVan(m_nModelIndex))
                doomHorizontalRotation += DegreesToRadians(22);
            else
                doomHorizontalRotation += DegreesToRadians(15);

            if (doomVerticalRotation > m_fDoomVerticalRotation + PI)
                doomVerticalRotation -= TWO_PI;
            else if (doomVerticalRotation < m_fDoomVerticalRotation - PI)
                doomVerticalRotation += TWO_PI;

            float doomVerticalRotDiff = doomVerticalRotation - m_fDoomVerticalRotation;
            float timeStep = CTimer::GetTimeStep() * ExeRecip(20.0f);
            // 0x729C7A-0x729CB2: when neither step applies the rotation snaps to the target (the port left it unchanged)
            if (doomVerticalRotDiff > timeStep)
                m_fDoomVerticalRotation += timeStep;
            else if (doomVerticalRotDiff < -timeStep)
                m_fDoomVerticalRotation -= timeStep;
            else
                m_fDoomVerticalRotation = doomVerticalRotation;

            float doomHorizontalRotDiff = doomHorizontalRotation - m_fDoomHorizontalRotation;
            timeStep = CTimer::GetTimeStepInSeconds();
            if (doomHorizontalRotDiff > timeStep)
                m_fDoomHorizontalRotation += timeStep;
            else if (doomHorizontalRotDiff < -timeStep)
                m_fDoomHorizontalRotation -= timeStep;
            else
                m_fDoomHorizontalRotation = doomHorizontalRotation; // 0x729D05
        }

        if (m_fDoomVerticalRotation > PI)
            m_fDoomVerticalRotation -= TWO_PI;
        else if (m_fDoomVerticalRotation < -PI)
            m_fDoomVerticalRotation += TWO_PI;

        // 0x729DC5: the exe literals for 10 and 20 degrees are 1 ulp above DegreesToRadians(10/20) (0x3E32B8C3, 0x3EB2B8C3)
        if (ModelIndices::IsSwatVan(m_nModelIndex))
            m_fDoomHorizontalRotation = std::clamp<float>(m_fDoomHorizontalRotation, -std::bit_cast<float>(0x3E32B8C3u), DegreesToRadians(35));
        else
            m_fDoomHorizontalRotation = std::clamp<float>(m_fDoomHorizontalRotation, -std::bit_cast<float>(0x3EB2B8C3u), std::bit_cast<float>(0x3EB2B8C3u));

        if (!CPad::GetPad()->GetCarGunFired())
            return;
    }

    CVector newTurretPosition;
    if (m_aCarNodes[CAR_MISC_A]) {
        RwMatrix* carNodeMiscMatrix = RwFrameGetLTM(m_aCarNodes[CAR_MISC_A]);
        newTurretPosition = *RwMatrixGetPos(carNodeMiscMatrix);
    } else {
        newTurretPosition = m_matrix->TransformPoint(CVector(0.0f, 1.5f, 1.8f));
    }

    CVector point{
        (float)(-(X87R(x87::sin(m_fDoomVerticalRotation) * x87::cos(m_fDoomHorizontalRotation)))),
        (float)(X87R(x87::cos(m_fDoomVerticalRotation)   * x87::cos(m_fDoomHorizontalRotation))),
        (float)(x87::sin(m_fDoomHorizontalRotation)),
    };
    point = GetMatrix().TransformVector(point); // 0x72A062

    if (m_aCarNodes[CAR_MISC_A]) {
        if (ModelIndices::IsSwatVan(m_nModelIndex))
            newTurretPosition += point * 2.0f;
        else
            newTurretPosition += point * 1.2f;

        newTurretPosition += GetSpeed(newTurretPosition - GetPosition()) * CTimer::GetTimeStep();
    }

    point.z += float(CGeneral::GetRandomNumber() % 16) * ExeRecip(1000.0f);
    CVector endPoint = m_vecMoveSpeed * CVector(1.0f, 1.0f, 0.3f);
    if (ModelIndices::IsSwatVan(m_nModelIndex))
        endPoint += point * 0.4f;
    else
        endPoint += point * 0.5f;
    CWaterCannons::UpdateOne(NOTSA_PTR_ID32(this), &newTurretPosition, &endPoint);
}

// 0x44D520
bool CAutomobile::HasCarStoppedBecauseOfLight() {
    if (GetStatus() != STATUS_SIMPLE && GetStatus() != STATUS_PHYSICS) {
        return false;
    }

    // Is there a link `from` => `to` whose navi link is in a traffic light state != 0?
    const auto IsLinkRedLit = [](CNodeAddress from, CNodeAddress to) {
        const auto& fromNode = ThePaths.m_pPathNodes[from.m_wAreaId][from.m_wNodeId];
        for (int32 i = 0; i < (int32)fromNode.m_nNumLinks; i++) {
            const auto linkIdx = fromNode.m_wBaseLinkId + i;
            const auto linked  = ThePaths.m_pNodeLinks[from.m_wAreaId][linkIdx];
            if (linked.m_wAreaId != to.m_wAreaId || linked.m_wNodeId != to.m_wNodeId) {
                continue;
            }
            const auto navi = ThePaths.m_pNaviLinks[from.m_wAreaId][linkIdx];
            if (!ThePaths.m_pPathNodes[navi.m_wAreaId]) {
                continue;
            }
            if (ThePaths.GetCarPathLink(navi).m_nTrafficLightState != 0) {
                return true;
            }
        }
        return false;
    };

    // 0x44D53D
    const auto cur = m_autoPilot.m_currentAddress;
    if (!ThePaths.m_pPathNodes[cur.m_wAreaId]) {
        return false;
    }
    const auto next = m_autoPilot.m_startingRouteNode;
    if (!ThePaths.m_pPathNodes[next.m_wAreaId]) {
        return false;
    }
    if (IsLinkRedLit(cur, next)) {
        return true;
    }

    // 0x44D657
    const auto prev = m_autoPilot.m_endingRouteNode;
    if (prev.m_wAreaId == (uint16)-1 || !ThePaths.m_pPathNodes[prev.m_wAreaId]) {
        return false;
    }
    return IsLinkRedLit(prev, cur);
}

/*!
* @notsa
*/
CBouncingPanel* CAutomobile::CheckIfExistsGetFree(eCarNodes nodeIdx) {
    // NOTE/TODO/BUG:
    // This is the original code.. I'm not sure how it works..
    // It checks panel's frame == `nodeIdx`, but only up until the first one whose frame is == -1... Weird, maybe a bug?
    for (auto&& panel : m_panels) {
        if (panel.m_nFrameId == (uint16)-1) {
            return &panel;
        } else if (panel.m_nFrameId == nodeIdx) {
            return nullptr;
        }
    }
    return nullptr;
}

namespace {
// These replicate the operation order of the original (`CMatrix::Multiply3x3` 0x59C790 and `CMatrix::MultiplyMatrixWithVector` 0x59C890),
// as the ones in `CMatrix` do the additions in a different order. (x87: the sum is kept in extended precision and rounded to float only once)

// 0x59C790
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((x87d)u.x * v.z + (x87d)f.x * v.y) + (x87d)r.x * v.x),
        (float)(((x87d)u.y * v.z + (x87d)r.y * v.x) + (x87d)f.y * v.y),
        (float)(((x87d)u.z * v.z + (x87d)r.z * v.x) + (x87d)f.z * v.y)
    };
}

// 0x59C890
CVector TransformPointOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((x87d)u.x * v.z + (x87d)f.x * v.y) + (x87d)r.x * v.x) + p.x),
        (float)((((x87d)u.y * v.z + (x87d)r.y * v.x) + (x87d)f.y * v.y) + p.y),
        (float)((((x87d)u.z * v.z + (x87d)r.z * v.x) + (x87d)f.z * v.y) + p.z)
    };
}

// Used by `PreRender` to calculate the forward speed (to decide whether to add the exhaust/splash particles)
NOTSA_GLOBAL(s_AutomobileSpeedDivisor, 0xC1BFF0, (float), { std::bit_cast<float>(0x3BB60B6Au) }); // (1 / 3.6) / 50, the exe's CRT static initializer at 0x853520

constexpr auto PRERENDER_ONE_THIRD         = std::bit_cast<float>(0x3EAAAAABu); // 0x859040
constexpr auto PRERENDER_COMBINE_TILT_MAX  = std::bit_cast<float>(0x3F060A92u); // 0x8D31EC - 30 degrees
constexpr auto PRERENDER_COMBINE_TILT_LERP = std::bit_cast<float>(0x3F733333u); // 0x8D31E8 - 0.95
constexpr auto PRERENDER_CEMENT_TILT       = std::bit_cast<float>(0xBE32B8C3u); // 0x871050 - -10 degrees
constexpr auto PRERENDER_FORKLIFT_MULT     = std::bit_cast<float>(0x3A1D4952u); // 0x871014 - 0.0006
constexpr auto PRERENDER_CEMENT_MULT       = std::bit_cast<float>(0x3CA3D70Au); // 0x871054 - 0.02
constexpr auto PRERENDER_TOWTRUCK_MULT     = std::bit_cast<float>(0x3ECCCCCDu); // 0x8D3144 - 0.4

// 0x6AB374 - Lights (and coronas) of the emergency vehicles' light bar.
// `rightPos` is the position of the right-most corona (the left-most is mirrored along X), `alt*` is the colour used in the 2nd half of the 1024ms long cycle (the 1st half is always red)
void PreRenderSirenLights(CAutomobile& veh, const CVector& rightPos, uint8 altR, uint8 altG, uint8 altB) {
    if (!veh.vehicleFlags.bSirenOrAlarm) {
        return;
    }

    constexpr uint8 RED = 0xFF;

    const CVector leftPos{ -rightPos.x, rightPos.y, rightPos.z };

    //> 0x6AB5C6 - Light colour
    const auto time = CTimer::GetTimeInMS() & 0x3FF;
    uint8 lightR, lightG, lightB;
    if (time < 0x200) {
        lightR = RED / 6;
        lightG = 0;
        lightB = 0;
    } else {
        lightR = altR / 6;
        lightG = altG / 6;
        lightB = altB / 6;
    }

    //> 0x6AB63C - Fade in/out at the beginning/end of each 512ms period
    const auto fade = [](uint8 v, double f) {
        return (uint8)NOTSA_FTOL(int32, X87R((double)v * f) * (x87d)0.01f); // 0x858C58
    };
    const auto period = time & 0x1FF;
    if (period < 100) {
        const x87d f = (x87d)period;
        lightR = fade(lightR, f);
        lightG = fade(lightG, f);
        lightB = fade(lightB, f);
    } else if (period > 0x19C) {
        const x87d f = (x87d)(0x200 - period);
        lightR = fade(lightR, f);
        lightG = fade(lightG, f);
        lightB = fade(lightB, f);
    }

    //> 0x6AB715 - Point light above the vehicle
    const auto& up  = veh.GetUp();
    const auto& pos = veh.GetPosition();
    CPointLights::AddLight(
        0,
        CVector{
            (float)((x87d)2.0 * (x87d)up.x + (x87d)pos.x),
            (float)((x87d)2.0 * (x87d)up.y + (x87d)pos.y),
            (float)((x87d)2.0 * (x87d)up.z + (x87d)pos.z),
        },
        CVector{},
        10.0f,
        (float)((x87d)lightR * (x87d)0.005f), // 0x858B4C
        (float)((x87d)lightG * (x87d)0.005f),
        (float)((x87d)lightB * (x87d)0.005f),
        0,
        true,
        nullptr
    );

    //> 0x6AB823 - Corona colours
    const float spriteBrightness = CTimeCycle::m_CurrentColours.m_fSpriteBrightness; // 0xB7C4E4
    const auto  coronaColor      = [&](uint8 v) { return (uint8)NOTSA_FTOL(int32, (x87d)v * (x87d)spriteBrightness * (x87d)0.1f); }; // 0x858B1C
    const uint8 redCoronaR       = coronaColor(RED);
    const uint8 ZERO             = 0; // The original calculates it (`ftol(spriteBrightness * 0.0f)`) - always 0
    const uint8 altCoronaR       = coronaColor(altR);
    const uint8 altCoronaG       = coronaColor(altG);
    const uint8 altCoronaB       = coronaColor(altB);

    //> 0x6AB8BF - 4 coronas, evenly distributed between the left and right position
    for (auto i = 0u; i < 4; i++) {
        const x87d right = (x87d)i;
        const x87d left  = (x87d)3.0 - right;

        const float  leftZ  = (float)(left * (x87d)leftPos.z);
        const float  leftY  = (float)(left * (x87d)leftPos.y);
        const x87d leftX  = left * (x87d)leftPos.x; // Stays in the FPU register
        const float  rightY = (float)(right * (x87d)rightPos.y);
        const float  rightX = (float)(right * (x87d)rightPos.x);

        const float z = (float)((x87d)rightPos.z * right + (x87d)leftZ); // `rightPos.z * right` stays in the FPU register
        const float y = (float)((x87d)rightY + (x87d)leftY);
        const float x = (float)((x87d)rightX + leftX);

        const CVector coronaPos{ x * PRERENDER_ONE_THIRD, y * PRERENDER_ONE_THIRD, z * PRERENDER_ONE_THIRD };

        const auto phase = ((i * 0x40 + CTimer::GetTimeInMS()) >> 8) & 3;
        uint8 r, g, b;
        if (phase == 0) {
            r = redCoronaR;
            g = ZERO;
            b = ZERO;
        } else if (phase == 2) {
            r = altCoronaR;
            g = altCoronaG;
            b = altCoronaB;
        } else {
            continue;
        }
        CCoronas::RegisterCorona(
            reinterpret_cast<uintptr>(&veh) + 0x15 + i, &veh,
            r, g, b, 0xFF,
            coronaPos, 0.4f, TheCamera.m_fLODDistMultiplier * 150.0f,
            CORONATYPE_SHINYSTAR, FLARETYPE_NONE, CORREFL_NONE, LOSCHECK_OFF, TRAIL_OFF,
            0.0f, false,
            1.5f, false,
            15.0f, false, true
        );
    }
}

// 0x6ABAC0 - Taxi's roof sign
void PreRenderTaxiLight(CAutomobile& veh, const CVector& localPos) {
    if (!veh.autoFlags.bTaxiLight) {
        return;
    }

    const auto pos = TransformPointOriginal(*veh.m_matrix, localPos);

    const auto color = (uint8)NOTSA_FTOL(int32, (x87d)CTimeCycle::m_CurrentColours.m_fSpriteBrightness * (x87d)10.0f); // 0x85862C
    CCoronas::RegisterCorona(
        reinterpret_cast<uintptr>(&veh) + 0x11, &veh,
        color, color, 0, 0xFF,
        pos, 0.8f, TheCamera.m_fLODDistMultiplier * 150.0f,
        CORONATYPE_HEADLIGHT, FLARETYPE_NONE, CORREFL_SIMPLE, LOSCHECK_OFF, TRAIL_OFF,
        0.0f, false,
        1.5f, false,
        15.0f, false, true
    );
    CPointLights::AddLight(
        0,
        pos,
        CVector{},
        10.0f,
        0.1f, 0.1f, 0.05f,
        0,
        true,
        nullptr
    );
}

// 0x6ABBB3 - FBI Rancher's blue light
void PreRenderFBIRancherLight(CAutomobile& veh) {
    if (!veh.vehicleFlags.bSirenOrAlarm) {
        return;
    }
    if (!(CTimer::GetTimeInMS() & 0x100)) {
        return;
    }

    const CVector localPos{ 0.0f, 1.2f, 0.5f };

    const auto& fwd    = veh.GetForward();
    const auto  camFwd = TheCamera.GetForwardVector();
    if (!(((x87d)fwd.z * camFwd.z + (x87d)fwd.y * camFwd.y) + (x87d)fwd.x * camFwd.x < 0.0)) {
        return;
    }
    CCoronas::RegisterCorona(
        reinterpret_cast<uintptr>(&veh) + 0x15, &veh,
        0, 0, 0xFF, 0xFF,
        localPos, 0.4f, TheCamera.m_fLODDistMultiplier * 150.0f,
        CORONATYPE_SHINYSTAR, FLARETYPE_NONE, CORREFL_NONE, LOSCHECK_OFF, TRAIL_OFF,
        0.0f, false,
        1.5f, false,
        15.0f, false, true
    );
}
} // namespace

// 0x6AAB50
void CAutomobile::PreRender() {
    CVehicle::PreRender(); // 0x6D6480

    if (CCheat::IsActive(CHEAT_CARS_ON_WATER)) { // 0x969152
        DoHoverSuspensionRatios();
    }

    // NOTE: The original saves this pointer here, and uses it all over the function
    const auto* const colData = GetColModel()->m_pColData;

    //> 0x6AAB93 - Move the wheels towards the position the suspension would place them at
    if (vehicleFlags.bVehicleColProcessed && m_nVehicleSubType != VEHICLE_TYPE_MTRUCK) {
        DoBurstAndSoftGroundRatios();

        for (auto i = 0u; i < 4; i++) {
            // How much the suspension is compressed when relaxed [0: not compressed, 1: fully]
            const x87d relaxedRatio = (x87d)1.0 - (x87d)m_aSuspensionSpringLength[i] / (x87d)m_aSuspensionLineLength[i];
            const float  ratio        = (float)(((x87d)m_fWheelsSuspensionCompression[i] - relaxedRatio) / ((x87d)1.0 - relaxedRatio));

            x87d z = colData->m_pLines[i].m_vecStart.z;
            if (ratio > 0.0f) {
                z -= (x87d)ratio * (x87d)m_aSuspensionSpringLength[i];
            }
            if (!(m_fWheelsSuspensionCompression[i] < 1.0f) || !(z > (double)m_wheelPosition[i])) {
                z = (z - (x87d)m_wheelPosition[i]) * (x87d)0.75f + (x87d)m_wheelPosition[i]; // 0x858F34
            }
            m_wheelPosition[i] = (float)z;
        }
    }

    // Speed along the forward vector (used to determine whether to add the exhaust/water splash particles)
    const float fwdSpeed = (float)(
        (((x87d)m_vecMoveSpeed.z * GetForward().z + (x87d)m_vecMoveSpeed.y * GetForward().y) + (x87d)m_vecMoveSpeed.x * GetForward().x)
        / (x87d)s_AutomobileSpeedDivisor
    );
    const float speed = (float)std::sqrt(
        ((x87d)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (x87d)m_vecMoveSpeed.y * m_vecMoveSpeed.y) + (x87d)m_vecMoveSpeed.z * m_vecMoveSpeed.z
    );

    //> 0x6AAE25 - Wheel particles
    if ((int16)m_nModelIndex != -2 && m_nModelIndex != MODEL_VORTEX && m_nModelIndex != MODEL_RCBANDIT) {
        //> 0x6AAE39 - `Simple` vehicles don't have any col points, so calculate them
        if (GetStatus() == STATUS_SIMPLE) {
            CMatrix wheelMat;

            const auto UpdateWheelColPoint = [&](eCarWheel wheel, eCarNodes node) {
                wheelMat.Attach(RwFrameGetMatrix(m_aCarNodes[node]), false);

                auto& colPt = m_wheelColPoint[wheel];
                colPt.m_vecPoint = TransformPointOriginal(
                    *m_matrix,
                    CVector{
                        wheelMat.GetPosition().x,
                        wheelMat.GetPosition().y,
                        m_wheelPosition[wheel] * 1.5f // 0x858CE8
                    }
                );
                colPt.m_nSurfaceTypeB = SURFACE_DEFAULT;
            };
            UpdateWheelColPoint(CAR_WHEEL_REAR_RIGHT,  CAR_WHEEL_RB);
            UpdateWheelColPoint(CAR_WHEEL_REAR_LEFT,   CAR_WHEEL_LB);
            UpdateWheelColPoint(CAR_WHEEL_FRONT_RIGHT, CAR_WHEEL_RF);
            UpdateWheelColPoint(CAR_WHEEL_FRONT_LEFT,  CAR_WHEEL_LF);
        }

        //> 0x6AB06E
        switch (GetStatus()) {
        case STATUS_PLAYER:
        case STATUS_PLAYER_PLAYBACK_FROM_BUFFER:
        case STATUS_SIMPLE:
        case STATUS_PHYSICS: {
            const bool isAnyRearWheelSkidding = m_WheelStates[1] == WHEEL_STATE_SKIDDING || m_WheelStates[3] == WHEEL_STATE_SKIDDING;

            for (auto i = 0u; i < 4; i++) {
                uint32 flags = 0;
                if ((i == 0 || i == 2) && !isAnyRearWheelSkidding) {
                    flags = 4;
                }
                if (m_wheelSkidmarkBloodState[i]) {
                    flags += 1;
                }
                if (m_wheelSkidmarkMuddy[i]) {
                    flags += 2;
                }
                AddSingleWheelParticles(
                    m_WheelStates[i],
                    (uint32)m_damageManager.GetWheelStatus((eCarWheel)i), // 0x6C21B0
                    m_fWheelsSuspensionCompressionPrev[i],
                    speed,
                    &m_wheelColPoint[i],
                    &m_wheelColPoint[i].m_vecPoint,
                    (i == 0 || i == 1) ? -1.0f : 1.0f,
                    i,
                    (uint32)m_wheelSkidmarkType[i],
                    &m_wheelSkidmarkBloodState[i],
                    flags
                );
            }
            break;
        }
        default:
            break;
        }
    }

    //> 0x6AB154 - Wheel particles of the middle wheels (Only if this vehicle has any)
    if (m_aCarNodes[CAR_WHEEL_RM]) {
        const auto MiddleWheelParticles = [&](eCarWheel wheel, int32 id) {
            auto& colPt = m_wheelColPoint[wheel];

            uint32 flags = 0;
            if (m_wheelSkidmarkBloodState[wheel]) {
                flags = 1;
            }
            if (m_wheelSkidmarkMuddy[wheel]) {
                flags += 2;
            }

            const auto& fwd = GetForward();
            CVector     pos{
                (float)((x87d)2.0 * (x87d)fwd.x + (x87d)colPt.m_vecPoint.x),
                (float)((x87d)2.0 * (x87d)fwd.y + (x87d)colPt.m_vecPoint.y),
                (float)((x87d)2.0 * (x87d)fwd.z + (x87d)colPt.m_vecPoint.z),
            };
            AddSingleWheelParticles(
                m_WheelStates[wheel],
                (uint32)m_damageManager.GetWheelStatus(wheel), // 0x6C21B0
                m_fWheelsSuspensionCompressionPrev[wheel],
                speed,
                &colPt,
                &pos,
                1.0f,
                id,
                (uint32)m_wheelSkidmarkType[wheel],
                &m_wheelSkidmarkBloodState[wheel],
                flags
            );
        };
        MiddleWheelParticles(CAR_WHEEL_REAR_LEFT,  5);
        MiddleWheelParticles(CAR_WHEEL_REAR_RIGHT, 6);
    }

    //> 0x6AB2BC - Water splash
    if (!CCullZones::CamNoRain() && !CCullZones::PlayerNoRain() && std::fabs(fwdSpeed) < 20.0f && CWeather::Rain > 0.02f && CGame::currArea == AREA_CODE_NORMAL_WORLD) {
        AddWaterSplashParticles(); // 0x6DDF60
    }

    //> 0x6AB304 - Exhaust
    if (vehicleFlags.bEngineOn && !m_pHandlingData->m_bNoExhaust && fwdSpeed < 130.0f && !vehicleFlags.bIsDrowning && !vehicleFlags.bDisableParticles) {
        AddExhaustParticles(); // 0x6DE240
    }

    AddDamagedVehicleParticles(); // 0x6D2A80

    //> 0x6AB350 - Lights of the emergency vehicles, taxi, etc
    switch (m_nModelIndex) {
    case MODEL_FIRETRUK:
        PreRenderSirenLights(*this, CVector{ 0.9f, 3.2f, 1.3f }, 0xFF, 0xFF, 0x00);
        break;
    case MODEL_AMBULAN:
        PreRenderSirenLights(*this, CVector{ 0.6f, 0.9f, 1.2f }, 0xFF, 0xFF, 0xFF);
        break;
    case MODEL_ENFORCER:
        PreRenderSirenLights(*this, CVector{ 0.55f, 1.1f, 1.4f }, 0x00, 0x00, 0xFF);
        break;
    case MODEL_COPCARLA:
    case MODEL_COPCARSF:
    case MODEL_COPCARVG:
        PreRenderSirenLights(*this, CVector{ 0.7f, -0.4f, 1.0f }, 0x00, 0x00, 0xFF);
        break;
    case MODEL_COPCARRU:
        PreRenderSirenLights(*this, CVector{ 0.7f, -0.1f, 1.2f }, 0x00, 0x00, 0xFF);
        break;
    case MODEL_TAXI:
        PreRenderTaxiLight(*this, CVector{ 0.0f, -0.4f, 0.95f });
        break;
    case MODEL_CABBIE:
        PreRenderTaxiLight(*this, CVector{ 0.0f, 0.0f, 0.85f });
        break;
    case MODEL_FBIRANCH:
        PreRenderFBIRancherLight(*this);
        break;
    default:
        break;
    }

    //> 0x6ABC71 - Vehicle lights
    if (m_nModelIndex != MODEL_RCBANDIT
        && (int16)m_nModelIndex != -2
        && m_nModelIndex != MODEL_RHINO
        && !vehicleFlags.bIsRCVehicle
        && GetVehicleAppearance() != VEHICLE_APPEARANCE_HELI
    ) {
        DoVehicleLights(
            *m_matrix,
            (m_nModelIndex == MODEL_COMBINE || m_nModelIndex == MODEL_QUAD)
                ? (eVehicleLightsFlags)0
                : (eVehicleLightsFlags)3 // TWIN | (1 << 1) - The 2nd bit isn't used
        );
    }

    //> 0x6ABCBE - Shadow
    if (this != FindPlayerVehicle(-1, false) || !TheCamera.GetLookingForwardFirstPerson()) {
        CShadows::StoreShadowForVehicle(
            this,
            (vehicleFlags.bIsRCVehicle && m_nVehicleSubType != VEHICLE_TYPE_AUTOMOBILE)
                ? VEH_SHD_RC
                : VEH_SHD_CAR
        );
    }

    const auto* const mi = GetVehicleModelInfo();

    if (m_nModelIndex == MODEL_RHINO) {
        SetComponentRotation(m_aCarNodes[CAR_BONNET], AXIS_Z, m_fDoomVerticalRotation, true);
    }

    //> 0x6ABD47 - Wheel rotation
    const bool isHeli = m_pHandlingData->m_bIsHeli;

    const auto steerDir = [&] { // 0x6ABD63
        return TransformVectorOriginal(
            *m_matrix,
            CVector{
                -(float)x87::sin((double)m_fSteerAngle),
                (float)x87::cos((double)m_fSteerAngle),
                0.0f
            }
        );
    }();

    // The directions of the front (steered) and rear wheels
    CVector frontWheelDir = steerDir;
    CVector rearWheelDir  = GetForward();
    if (handlingFlags.bSteerRearwheels) {
        frontWheelDir = GetForward();
        rearWheelDir  = TransformVectorOriginal(
            *m_matrix,
            CVector{
                (float)x87::sin((double)m_fSteerAngle),
                (float)x87::cos((double)m_fSteerAngle),
                0.0f
            }
        );
    } else if (handlingFlags.bHbRearwheelSteer && m_f2ndSteerAngle != 0.0f) {
        rearWheelDir = TransformVectorOriginal(
            *m_matrix,
            CVector{
                -(float)x87::sin((double)m_f2ndSteerAngle),
                (float)x87::cos((double)m_f2ndSteerAngle),
                0.0f
            }
        );
    }

    for (auto i = 0u; i < 4; i++) { // 0x6ABEA0
        if (!(m_WheelCounts[i] > 0.0f)) {
            continue;
        }
        if (isHeli && i != 0 && i != 2) {
            continue;
        }

        const auto wheelSpeed = GetSpeed(m_wheelColPoint[i].m_vecPoint - GetPosition()); // 0x542CE0

        const auto isFront = i == 0 || i == 2;
        m_wheelSpeed[i] = ProcessWheelRotation( // 0x6D1230
            m_WheelStates[i],
            isFront ? frontWheelDir : rearWheelDir,
            wheelSpeed,
            (isFront ? mi->m_fWheelSizeFront : mi->m_fWheelSizeRear) * 0.5f
        );
        m_wheelRotation[i] = (float)((x87d)CTimer::GetTimeStep() * (x87d)m_wheelSpeed[i] + (x87d)m_wheelRotation[i]);
    }

    //> 0x6ABFC6 - Update wheel (and other) matrices
    {
        const auto flags = m_nModelIndex == MODEL_RHINO ? 7 : 0;
        UpdateWheelMatrix(CAR_WHEEL_RF, flags);
        UpdateWheelMatrix(CAR_WHEEL_LF, flags);
        UpdateWheelMatrix(CAR_WHEEL_RB, flags);
        UpdateWheelMatrix(CAR_WHEEL_LB, flags);
        if (m_aCarNodes[CAR_WHEEL_RM]) {
            UpdateWheelMatrix(CAR_WHEEL_RM, flags);
        }
        if (m_aCarNodes[CAR_WHEEL_LM]) {
            UpdateWheelMatrix(CAR_WHEEL_LM, flags);
        }
    }

    CMatrix mat; // Used by multiple blocks below

    if (m_nModelIndex != MODEL_RHINO) {
        //> 0x6AC031 - Doors
        ProcessSwingingDoor(CAR_DOOR_LF, DOOR_LEFT_FRONT);
        ProcessSwingingDoor(CAR_DOOR_RF, DOOR_RIGHT_FRONT);
        ProcessSwingingDoor(CAR_DOOR_LR, DOOR_LEFT_REAR);
        ProcessSwingingDoor(CAR_DOOR_RR, DOOR_RIGHT_REAR);
        ProcessSwingingDoor(CAR_BONNET,  DOOR_BONNET);
        ProcessSwingingDoor(CAR_BOOT,    DOOR_BOOT);

        //> 0x6AC073 - Bouncing panels
        for (auto& panel : m_panels) {
            if ((int16)panel.m_nFrameId >= 0) {
                panel.ProcessPanel(this, m_aCarNodes[(int16)panel.m_nFrameId], m_moveForce, m_turnForce, -1.0f, -1.0f); // 0x6F49A0
            }
        }

        //> 0x6AC0E2 - Swinging chassis
        const auto chassisNode = m_nModelIndex == MODEL_FIRELA ? CAR_MISC_B : CAR_CHASSIS; // 0x220
        if (m_swingingChassis.m_doorState == DOOR_HIT_MAX_END && m_aCarNodes[chassisNode]) {
            mat.Attach(RwFrameGetMatrix(m_aCarNodes[chassisNode]), false);
            const CVector chassisPos = mat.GetPosition();

            const auto offset = TransformVectorOriginal(*m_matrix, chassisPos);
            m_swingingChassis.Process(this, m_moveForce, m_turnForce, offset); // 0x6F4040

            CVector angles{};
            angles[(uint8)m_swingingChassis.m_axis] = m_swingingChassis.m_angle;
            mat.SetRotate(angles.x, angles.y, angles.z);
            if (m_nModelIndex == MODEL_FIRELA) {
                mat.RotateX((float)m_wMiscComponentAngle * -0.0001f); // 0x87100C
            }
            mat.GetPosition().x = chassisPos.x + mat.GetPosition().x;
            mat.GetPosition().y = chassisPos.y + mat.GetPosition().y;
            mat.GetPosition().z = mat.GetPosition().z + chassisPos.z;
            mat.UpdateRW();
        }
    }

    //> 0x6AC232
    m_moveForce = m_vecMoveSpeed + m_vecFrictionMoveSpeed;
    m_turnForce = m_vecTurnSpeed + m_vecFrictionTurnSpeed;

    //> 0x6AC29F - Model specific components
    switch (m_nModelIndex) {
    case MODEL_BFINJECT: { // 0x6AC2AA
        const float rot = CTimer::GetTimeStep() * (std::fabs(m_GasPedal) > 0.0f ? 0.5f : 0.3f); // 0x858B8C, 0x858C24
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_Y, rot, false);
        SetComponentRotation(m_aCarNodes[CAR_MISC_E], AXIS_Y, rot * 1.5f, false); // 0x858CE8

        // NOTE: The original calls `GetColModel` here, but doesn't use the result
        static constexpr eCarNodes AXLE_NODES[]{ CAR_MISC_B, CAR_NODE_NONE, CAR_MISC_C }; // 0x8D31F0
        for (auto i = 0u; i < std::size(AXLE_NODES); i++) {
            if (AXLE_NODES[i] == CAR_NODE_NONE) {
                continue;
            }
            auto* const node = m_aCarNodes[AXLE_NODES[i]];
            if (!node) {
                continue;
            }
            mat.Attach(RwFrameGetMatrix(node), false);
            const auto& line = colData->m_pLines[i];
            mat.GetForward().z = (float)(
                ((x87d)m_wheelPosition[i] - ((x87d)line.m_vecStart.z - (x87d)m_pHandlingData->m_fSuspensionUpperLimit))
                / ((x87d)line.m_vecStart.y - (x87d)mat.GetPosition().y)
            );
            mat.UpdateRW();
        }

        CVector wheelPos;
        mi->GetWheelPosn(1, wheelPos, false);
        SetTransmissionRotation(m_aCarNodes[CAR_MISC_D], m_wheelPosition[1], m_wheelPosition[3], wheelPos, false);
        break;
    }
    case MODEL_DOZER: // 0x6AC40C
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, (float)m_wMiscComponentAngle * 0.0002f, true); // 0x871010
        break;
    case MODEL_CEMENT: { // 0x6AC43B
        if (GetStatus() == STATUS_WRECKED) {
            break;
        }
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, PRERENDER_CEMENT_TILT, true);
        m_fPropRotate = (float)(
            (((x87d)m_wMiscComponentAngle / (x87d)DEFAULT_COLLISION_EXTENDLIMIT + (x87d)0.5f) * (x87d)CTimer::GetTimeStep()) * (x87d)PRERENDER_CEMENT_MULT
            + (x87d)m_fPropRotate
        );
        SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_Y, m_fPropRotate, true);
        SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_X, PRERENDER_CEMENT_TILT, false);
        break;
    }
    case MODEL_PACKER: // 0x6AC4D9
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, (float)m_wMiscComponentAngle * -0.0001f, true); // 0x87100C
        break;
    case MODEL_TOWTRUCK: { // 0x6AC507
        const float hoist = (float)(((x87d)m_wMiscComponentAngle * (x87d)PRERENDER_TOWTRUCK_MULT) / (x87d)TOWTRUCK_HOIST_DOWN_LIMIT);
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, hoist, true);

        if (!m_pVehicleBeingTowed) { // 0x6AC662
            SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_X, -hoist, true);
            if (m_panels[0].m_nFrameId == CAR_MISC_B) {
                mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_MISC_B]), false);
                // NOTE: Modifies the `up` vector, not the position
                mat.GetUp().y += m_panels[0].m_vecRotation.x;
                mat.GetUp().x += m_panels[0].m_vecRotation.y;
                mat.UpdateRW();
            }
            break;
        }

        CVector hitchPos;
        if (!m_pVehicleBeingTowed->GetTowHitchPos(hitchPos, true, this)) {
            break;
        }
        CVector barPos;
        if (!GetTowBarPos(barPos, true, m_pVehicleBeingTowed)) {
            break;
        }
        const auto dir = hitchPos - barPos;

        const auto& fwd = GetForward();
        const float fwdDot = (float)((((x87d)dir.z * fwd.z + (x87d)dir.y * fwd.y) + (x87d)dir.x * fwd.x) / (x87d)1.0); // 0x8D3148
        SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_X, (float)(X87R(x87::asin((double)fwdDot) + (double)-hoist)), true);

        const auto& right = GetRight();
        const float rightDot = (float)(-(x87d)1.0 / (x87d)1.0 * (((x87d)dir.z * right.z + (x87d)dir.y * right.y) + (x87d)dir.x * right.x)); // 0x858C1C, 0x8D3148
        SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_Y, (float)x87::asin((double)rightDot), false);
        break;
    }
    case MODEL_TRACTOR: // 0x6AC6D9
        SetComponentRotation(
            m_aCarNodes[CAR_MISC_A],
            AXIS_X,
            (float)(((x87d)m_wMiscComponentAngle * (x87d)PRERENDER_TOWTRUCK_MULT) / (x87d)TOWTRUCK_HOIST_DOWN_LIMIT),
            true
        );
        break;
    case MODEL_FORKLIFT: { // 0x6AC71C
        if (!m_aCarNodes[CAR_MISC_A]) {
            break;
        }
        auto* const frame = CClumpModelInfo::GetFrameFromId(CModelInfo::GetModelInfo(MODEL_FORKLIFT)->GetRpClump(), 0x14);
        const auto&       pos   = RwFrameGetMatrix(frame)->pos;
        const float       z     = (float)((x87d)m_wMiscComponentAngle * (x87d)PRERENDER_FORKLIFT_MULT + (x87d)pos.z);

        mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_MISC_A]), false);
        mat.SetTranslateOnly(CVector{ pos.x, pos.y, z });
        mat.UpdateRW();
        break;
    }
    case MODEL_COMBINE: { // 0x6AC7AB
        if (m_fPropRotate > std::numbers::pi_v<float> * 2.0f) { // 0x858CBC
            m_fPropRotate -= std::numbers::pi_v<float> * 2.0f;
        }

        const auto& fwd = GetForward();
        const x87d fwdDot = ((x87d)m_vecMoveSpeed.z * fwd.z + (x87d)m_vecMoveSpeed.y * fwd.y) + (x87d)m_vecMoveSpeed.x * fwd.x;
        x87d rotSpeed;
        if (fwdDot > (double)0.1f) {
            rotSpeed = (x87d)0.1f;
        } else if (0.0 > fwdDot) {
            rotSpeed = (x87d)0.0;
        } else {
            rotSpeed = fwdDot;
        }
        m_fPropRotate = (float)((x87d)m_fPropRotate - rotSpeed * (x87d)CTimer::GetTimeStep());
        SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_X, m_fPropRotate, true);

        CVector wheelPos;
        mi->GetWheelPosn(0, wheelPos, false);

        mat.Attach(RwFrameGetMatrix(m_aCarNodes[CAR_MISC_A]), false);
        const x87d wheelsMid = ((x87d)m_wheelPosition[0] + (x87d)m_wheelPosition[2]) * (x87d)0.5; // 0x858B8C
        const x87d wheelsDif = (x87d)m_wheelPosition[0] - (x87d)m_wheelPosition[2];
        const float  angleX    = (float)-x87::atan2(wheelsMid - (x87d)wheelPos.z, (x87d)mat.GetPosition().y - (x87d)wheelPos.y);
        const float  angleY    = (float)x87::atan2(wheelsDif, std::fabs((x87d)wheelPos.x) * (x87d)2.0);
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, (float)((x87d)angleX + (x87d)std::numbers::pi_v<float>), true);
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_Y, angleY, false);

        float tilt = 0.0f;
        if (GetUp().z > 0.0f && m_nGettingInFlags == 0 && m_nGettingOutFlags == 0) {
            const double tiltD = x87::asin((double)GetRight().z);
            tilt               = (float)tiltD;
            if (tiltD > (double)PRERENDER_COMBINE_TILT_MAX) { // NOTE: Compares the extended value, not the float
                tilt = PRERENDER_COMBINE_TILT_MAX;
            } else if (tilt < -PRERENDER_COMBINE_TILT_MAX) {
                tilt = -PRERENDER_COMBINE_TILT_MAX;
            }
        }
        const double lerp = std::pow((double)PRERENDER_COMBINE_TILT_LERP, (double)CTimer::GetTimeStep());
        m_fDoomVerticalRotation = (float)(X87R(lerp * (double)m_fDoomVerticalRotation) + (X87R(1.0 - lerp)) * (x87d)tilt);
        SetComponentRotation(m_aCarNodes[CAR_CHASSIS], AXIS_Y, m_fDoomVerticalRotation, true);
        break;
    }
    case MODEL_BANDITO:
    case MODEL_HOTKNIFE: { // 0x6ACADE
        // NOTE: The original calls `GetColModel` here, but doesn't use the result
        static constexpr eCarNodes AXLE_NODES[]{ CAR_MISC_A, CAR_MISC_C, CAR_MISC_B, CAR_MISC_D }; // 0x8D31D8
        for (auto i = 0u; i < std::size(AXLE_NODES); i++) {
            auto* const node = m_aCarNodes[AXLE_NODES[i]];
            if (!node) {
                continue;
            }
            mat.Attach(RwFrameGetMatrix(node), false);
            const auto& line = colData->m_pLines[i];
            mat.GetRight().z = (float)(
                ((x87d)m_wheelPosition[i] - ((x87d)line.m_vecStart.z - (x87d)m_pHandlingData->m_fSuspensionUpperLimit))
                / ((x87d)line.m_vecStart.x - (x87d)mat.GetPosition().x)
            );
            mat.UpdateRW();
        }
        SetComponentRotation(m_aCarNodes[CAR_MISC_E], AXIS_Y, CTimer::GetTimeStep() * (std::fabs(m_GasPedal) > 0.0f ? 0.5f : 0.3f), false);
        break;
    }
    case MODEL_RHINO:
    case MODEL_SWATVAN: // 0x6ACAAE
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_Z, m_fDoomVerticalRotation, true);
        SetComponentRotation(m_aCarNodes[CAR_MISC_B], AXIS_X, m_fDoomHorizontalRotation, true);
        break;
    case MODEL_FIRETRUK: // 0x6ACA5D
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, m_fDoomHorizontalRotation, true);
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_Z, m_fDoomVerticalRotation, false);
        break;
    case MODEL_ZR350: // 0x6ACA8D
        SetComponentRotation(m_aCarNodes[CAR_MISC_A], AXIS_X, m_fPropRotate, true);
        break;
    default:
        break;
    }

    //> 0x6ACBC7
    if (m_nModelIndex == MODEL_SANDKING || (handlingFlags.bHydraulicGeom && handlingFlags.bHydraulicInst && m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE)) {
        CVector wheelPos;
        mi->GetWheelPosn(0, wheelPos, false);
        SetTransmissionRotation(m_aCarNodes[CAR_MISC_A], m_wheelPosition[0], m_wheelPosition[2], wheelPos, true);

        mi->GetWheelPosn(1, wheelPos, false);
        SetTransmissionRotation(m_aCarNodes[CAR_MISC_B], m_wheelPosition[1], m_wheelPosition[3], wheelPos, false);
    }
}

// 0x6A2B10
void CAutomobile::Render() {
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;

    int32 savedAlphaTestRef = 1; // The original pre-initializes the out value
    RwRenderStateGet(rwRENDERSTATEALPHATESTFUNCTIONREF, &savedAlphaTestRef);
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(1));

    const auto RenderNodeAtomic = [](RwFrame* frame) {
        // The original calls `GetFirstObject` twice (once for the callee, once for the argument): `atomic->renderCallBack(atomic)` == RpAtomicRender(atomic)
        RpAtomicRender((RpAtomic*)GetFirstObject(frame));
    };

    if (!CCheat::IsActive(CHEAT_INVISIBLE_CAR)) { // 0x96914B
        CVehicle::Render();
    } else {
        // Only render wheels
        RenderNodeAtomic(m_aCarNodes[CAR_WHEEL_RB]);
        RenderNodeAtomic(m_aCarNodes[CAR_WHEEL_LB]);
        RenderNodeAtomic(m_aCarNodes[CAR_WHEEL_RF]);
        RenderNodeAtomic(m_aCarNodes[CAR_WHEEL_LF]);
        if (m_aCarNodes[CAR_WHEEL_RM]) {
            RenderNodeAtomic(m_aCarNodes[CAR_WHEEL_RM]);
        }
        if (m_aCarNodes[CAR_WHEEL_LM]) {
            RenderNodeAtomic(m_aCarNodes[CAR_WHEEL_LM]);
        }
    }

    // Rhino: render the extra (middle) wheels at the suspension line positions
    if (m_nModelIndex == MODEL_RHINO) {
        const auto* const lines = GetColModel()->m_pColData->m_pLines;

        // Values stored inside doors (see `CAutomobile::ProcessEntityCollision`)
        const float extraWheels[8]{
            m_doors[DOOR_LEFT_REAR].m_openAngle,
            m_doors[DOOR_LEFT_REAR].m_closedAngle,
            m_doors[DOOR_LEFT_REAR].m_angle,
            m_doors[DOOR_LEFT_REAR].m_prevAngle,
            m_doors[DOOR_RIGHT_REAR].m_openAngle,
            m_doors[DOOR_RIGHT_REAR].m_closedAngle,
            m_doors[DOOR_RIGHT_REAR].m_angle,
            m_doors[DOOR_RIGHT_REAR].m_prevAngle,
        };

        for (auto i = 0u; i < 8; i++) {
            // BUG: Always uses the length of the 1st wheel's suspension
            x87d t = (x87d)extraWheels[i] - (((x87d)m_aSuspensionLineLength[0] - (x87d)m_aSuspensionSpringLength[0]) / (x87d)m_aSuspensionLineLength[0]);
            if (t < 0.0) {
                t = (x87d)0.0;
            }
            const x87d u = (x87d)1.f - t; // 0x858624

            const auto& line = lines[4 + i];

            const float A = (float)(u * (x87d)line.m_vecStart.x);
            const float B = (float)(u * (x87d)line.m_vecStart.y);
            const x87d uz = u * (x87d)line.m_vecStart.z;
            const float C = (float)(t * (x87d)line.m_vecEnd.z);

            CVector offset{
                (float)(t * (x87d)line.m_vecEnd.x + (x87d)A),
                (float)(t * (x87d)line.m_vecEnd.y + (x87d)B),
                (float)((x87d)C + uz),
            };

            RwFrame* const node = i < 4
                ? m_aCarNodes[CAR_WHEEL_LB]
                : m_aCarNodes[CAR_WHEEL_RB];
            RwMatrix* const nodeMat = RwFrameGetMatrix(node);

            RwV3d translation{
                (float)((x87d)offset.x - (x87d)nodeMat->pos.x),
                (float)((x87d)offset.y - (x87d)nodeMat->pos.y),
                (float)((x87d)offset.z - (x87d)nodeMat->pos.z),
            };
            RwMatrixTranslate(nodeMat, &translation, rwCOMBINEPOSTCONCAT);
            RwFrameUpdateObjects(node);
            RenderNodeAtomic(node);

            // Undo the translation
            translation.x *= -1.f; // 0x858C1C
            translation.y *= -1.f;
            translation.z *= -1.f;
            RwMatrixTranslate(nodeMat, &translation, rwCOMBINEPOSTCONCAT);
        }
    }

    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(savedAlphaTestRef));

    if (   m_nModelIndex != MODEL_RCBANDIT
        && (int16)m_nModelIndex != -2
        && m_nModelIndex != MODEL_RHINO
        && !vehicleFlags.bIsRCVehicle
        && GetVehicleAppearance() != VEHICLE_APPEARANCE_HELI
    ) {
        if (m_renderLights.m_bRightFront) {
            DoHeadLightBeam(eVehicleLightId::MAIN, *m_matrix, true);
        }
        if (m_renderLights.m_bLeftFront) {
            DoHeadLightBeam(eVehicleLightId::MAIN, *m_matrix, false);
        }
    }
}

// 0x6A9CA0
void CAutomobile::Teleport(CVector destination, bool resetRotation) {
    CWorld::Remove(this);
    GetPosition() = destination;

    if (resetRotation)
        SetOrientation(0.0f, 0.0f, 0.0f);

    ResetMoveSpeed();
    ResetTurnSpeed();
    ResetSuspension();
    CWorld::Add(this);
}

// 0x6A0750
RwObject* GetCurrentAtomicObjectCB(RwObject* object, void* data) {
    if (RpAtomicGetFlags(object) & rpATOMICRENDER)
        *reinterpret_cast<RpAtomic**>(data) = reinterpret_cast<RpAtomic*>(object);

    return object;
}

// NOTSA
RwObject* GetCurrentAtomicObject(RwFrame* frame) {
    RwObject* out = nullptr;
    RwFrameForAllObjects(frame, GetCurrentAtomicObjectCB, &out);
    return out;
}
