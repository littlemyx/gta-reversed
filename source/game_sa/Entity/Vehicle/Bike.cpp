/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Bike.h"

#include "Buoyancy.h"
#include "CarCtrl.h"
#include "ModelIndices.h"
#include "VehicleRecording.h"
#include "Shadows.h"
#include "Automobile.h"



namespace {
// These replicate the operation order of the original (inlined `CMatrix::Multiply3x3`, `CMatrix::MultiplyMatrixWithVector`),
// as the ones in `CMatrix` (`TransformVector` etc.) do the additions in a different order.

// 0x59C790
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (u.x * v.z + f.x * v.y) + r.x * v.x,
        (u.y * v.z + r.y * v.x) + f.y * v.y,
        (u.z * v.z + r.z * v.x) + f.z * v.y
    };
}

// 0x59C810
CVector InverseTransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (r.y * v.y + r.z * v.z) + v.x * r.x,
        (f.y * v.y + f.x * v.x) + f.z * v.z,
        (u.y * v.y + u.x * v.x) + u.z * v.z
    };
}

// 0x59C890
CVector TransformPointOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        ((u.x * v.z + f.x * v.y) + r.x * v.x) + p.x,
        ((u.y * v.z + r.y * v.x) + f.y * v.y) + p.y,
        ((u.z * v.z + r.z * v.x) + f.z * v.y) + p.z
    };
}
} // namespace

// 0xC1C81C - Divisor of the forward speed when deciding whether to emit exhaust particles in `PreRender` (named by hand, never written to by the game code)
static auto& s_BikeExhaustSpeedDivisor = StaticRef<float>(0xC1C81C);

// 0xC1C804 - Squared (sign preserved) steering input, written by `ProcessControlInputs` (named by hand, no known readers)
static auto& s_BikeSteerAngleSq = StaticRef<float>(0xC1C804);

void CBike::InjectHooks() {
    RH_ScopedVirtualClass(CBike, 0x871360, 67);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6BF430);
    RH_ScopedInstall(Destructor, 0x6B57A0);
    RH_ScopedInstall(dmgDrawCarCollidingParticles, 0x6B5A00);
    RH_ScopedInstall(DamageKnockOffRider, 0x6B5A10);
    RH_ScopedInstall(KnockOffRider, 0x6B5F40);
    RH_ScopedInstall(SetRemoveAnimFlags, 0x6B5F50);
    RH_ScopedInstall(ReduceHornCounter, 0x6B5F90);
    RH_ScopedVMTInstall(ProcessAI, 0x6BC930);
    RH_ScopedInstall(ProcessBuoyancy, 0x6B5FB0);
    RH_ScopedInstall(ResetSuspension, 0x6B6740);
    RH_ScopedInstall(GetAllWheelsOffGround, 0x6B6790);
    RH_ScopedInstall(DebugCode, 0x6B67A0);
    RH_ScopedInstall(DoSoftGroundResistance, 0x6B6D40);
    RH_ScopedInstall(PlayHornIfNecessary, 0x6B7130);
    RH_ScopedInstall(CalculateLeanMatrix, 0x6B7150);
    RH_ScopedInstall(ProcessRiderAnims, 0x6B7280);
    RH_ScopedInstall(FixHandsToBars, 0x6B7F90);
    RH_ScopedInstall(PlaceOnRoadProperly, 0x6BEEB0);
    RH_ScopedInstall(GetCorrectedWorldDoorPosition, 0x6BF230);
    RH_ScopedVMTInstall(Fix, 0x6B7050);
    RH_ScopedVMTInstall(BlowUpCar, 0x6BEA10);
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6BF400);
    RH_ScopedVMTInstall(BurstTyre, 0x6BEB20);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6BE310);
    RH_ScopedVMTInstall(ProcessEntityCollision, 0x6BDEA0);
    RH_ScopedVMTInstall(Render, 0x6BDE20);
    RH_ScopedVMTInstall(PreRender, 0x6BD090);
    RH_ScopedVMTInstall(Teleport, 0x6BCFC0);
    RH_ScopedVMTInstall(ProcessControl, 0x6B9250, { .Reversed = false });
    RH_ScopedVMTInstall(VehicleDamage, 0x6B8EC0);
    RH_ScopedVMTInstall(SetupSuspensionLines, 0x6B89B0);
    RH_ScopedVMTInstall(SetModelIndex, 0x6B8970);
    RH_ScopedVMTInstall(PlayCarHorn, 0x6B7080);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6B7070);
    RH_ScopedVMTInstall(DoBurstAndSoftGroundRatios, 0x6B6950);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6B67E0);
    RH_ScopedVMTInstall(RemoveRefsToVehicle, 0x6B67B0);
    RH_ScopedVMTInstall(ProcessControlCollisionCheck, 0x6B6620);
    RH_ScopedVMTInstall(GetComponentWorldPosition, 0x6B5990);
    RH_ScopedVMTInstall(ProcessOpenDoor, 0x6B58D0);
}

// 0x6BF430
CBike::CBike(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    auto mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();
    if (mi->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        const auto& animationStyle = CAnimManager::GetAnimBlocks()[mi->GetAnimFileIndex()].GroupId;
        m_RideAnimData.AnimGroup = animationStyle;
        if (animationStyle < ANIM_GROUP_BIKES || animationStyle > ANIM_GROUP_WAYFARER) {
            m_RideAnimData.AnimGroup = ANIM_GROUP_BIKES;
        }
    }

    m_nVehicleSubType = VEHICLE_TYPE_BIKE;
    m_nVehicleType = VEHICLE_TYPE_BIKE;

    m_BlowUpTimer = 0.0f;
    m_nBrakesOn = false;
    nBikeFlags = 0;
    SetModelIndex(modelIndex);

    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_BikeHandling = gHandlingDataMgr.GetBikeHandlingPointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;
    m_pFlyingHandlingData = gHandlingDataMgr.GetFlyingPointer(static_cast<uint8>(mi->m_nHandlingId));
    m_fBrakeCount = 20.0f;
    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);
    m_fSwingArmLength = 0.0f;
    m_fForkYOffset = 0.0f;
    m_fForkZOffset = 0.0f;
    m_nFixLeftHand = false;
    m_nFixRightHand = false;
    m_fSteerAngleTan = std::tan(DegreesToRadians(mi->m_fBikeSteerAngle));
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_vecCentreOfMass.z = 0.1f;
    m_fAirResistance = GetDefaultAirResistance();
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fSteerAngle = 0.0f;
    m_GasPedal = 0.0f;
    m_BrakePedal = 0.0f;
    m_Damager = nullptr;
    m_pWhoInstalledBombOnMe = nullptr;
    m_GasPedalAudioRevs = 0.0f;
    m_fTyreTemp = 1.0f;
    m_fBrakingSlide = 0.0f;
    m_PrevSpeed = 0.0f;

    for (auto i = 0; i < 2; ++i) {
        m_nWheelStatus[i] = 0;
        m_aWheelSkidmarkType[i] = eSkidmarkType::DEFAULT;
        m_bWheelBloody[i] = false;
        m_bMoreSkidMarks[i] = false;
        m_aWheelPitchAngles[i] = 0.0f;
        m_aWheelAngularVelocity[i] = 0.0f;
        m_aWheelSuspensionHeights[i] = 0.0f;
        m_aWheelOrigHeights[i] = 0.0f;
        m_WheelStates[i] = WHEEL_STATE_NORMAL;
    }

    for (auto i = 0; i < 4; ++i) {
        m_aWheelColPoints[i] = {};
        m_aWheelRatios[i] = 1.0f;
        m_aRatioHistory[i] = 0.0f;
        m_WheelCounts[i] = 0.0f;
        m_fSuspensionLength[i] = 0.0f;
        m_fLineLength[i] = 0.0f;
        m_aGroundPhysicalPtrs[i] = nullptr;
        m_aGroundOffsets[i] = CVector{};
    }

    m_nNoOfContactWheels = 0;
    m_NumDriveWheelsOnGround = 0;
    m_NumDriveWheelsOnGroundLastFrame = 0;
    m_fHeightAboveRoad = 0.0f;
    m_fExtraTractionMult = 1.0f;

    if (!mi->m_pColModel->m_pColData->m_pLines) {
        mi->m_pColModel->m_pColData->m_nNumLines = 4;
        mi->m_pColModel->m_pColData->m_pLines = static_cast<CColLine*>(CMemoryMgr::Malloc(4 * sizeof(CColLine)));
        mi->m_pColModel->m_pColData->m_pLines[1].m_vecStart.z = 99'999.99f; // Used as a marker by `SetupSuspensionLines` (the lines have just been allocated). NOTE: This was `.x`, but the original writes to `+0x28` (= `[1].m_vecStart.z`) (see 0x6BF73C)
    }
    mi->m_pColModel->m_pColData->m_pLines[0].m_vecStart.z = 99'999.99f;
    CBike::SetupSuspensionLines();

    m_autoPilot.m_nTempAction = TEMPACT_NONE;
    m_autoPilot.SetCarMission(MISSION_NONE, 0);
    m_autoPilot.carCtrlFlags.bAvoidLevelTransitions = false;

    SetStatus(STATUS_SIMPLE);
    m_nNumPassengers = 0;
    vehicleFlags.bLowVehicle = false;
    vehicleFlags.bIsBig = false;
    vehicleFlags.bIsVan = false;

    m_bLeanMatrixCalculated = false;
    m_mLeanMatrix = *m_matrix;
    m_vecOldSpeedForPlayback = CVector{};
    m_vehicleAudio.Initialise(this);
}

// 0x6B57A0
CBike::~CBike() {
    m_vehicleAudio.Terminate();
}

// 0x6B5A00
void CBike::dmgDrawCarCollidingParticles(const CVector& position, float power, eWeaponType weaponType) {
    // NOP
}

// 0x6B5A10
bool CBike::DamageKnockOffRider(CVehicle* vehicle, float damageIntensity, uint16 pieceType, CEntity* damager, const CVector& collisionPos, const CVector& collisionImpactVelocity) {
    const auto driver    = vehicle->m_pDriver;
    const auto passenger = vehicle->m_apPassengers[0];

    // Impact force relative to the bike's mass
    auto force = damageIntensity / vehicle->m_fMass * 800.0f;

    // A skilled rider resists being knocked off (unless flagged to always come off)
    if (vehicle->GetStatus() != STATUS_PLAYER) {
        if (driver && driver->CantBeKnockedOffBike != CANT_BE_KNOCKED_OFF_ALWAYS_NORMAL) {
            force *= 1.0f - driver->GetBikeRidingSkill() * 0.6f;
        }
    } else {
        force *= 0.75f;
        if (driver) {
            force *= 1.0f - driver->GetBikeRidingSkill() * 0.5f;
        }
    }

    // Only an actual driver gets knocked off
    if (!driver || !driver->IsStateDriving() || force <= 10.0f) {
        return false;
    }

    // A ped already reacting to a hit isn't also knocked off (cops are exempt)
    if (const auto task = driver->GetIntelligence()->GetTaskManager().GetActiveTask()) {
        if (task->GetTaskType() == TASK_SIMPLE_BE_HIT && !driver->IsCop()) {
            return false;
        }
    }

    const auto impactFwdMag   = vehicle->GetForward().Dot(collisionImpactVelocity);
    const auto impactUpMag    = vehicle->GetUp().Dot(collisionImpactVelocity);
    const auto impactRightMag = vehicle->GetRight().Dot(collisionImpactVelocity);

    // Per-axis weighting of the impact
    auto fwdWeight = 0.6f;
    if (std::abs(impactFwdMag) > 0.85f) {
        const auto vertical = collisionImpactVelocity.z < 0.85f ? 0.0f : collisionImpactVelocity.z;
        fwdWeight = 7.0f * sq(vertical) + 0.6f;
    }
    if (vehicle->GetUp().z < 0.0f) { // bike lying on its side / upside down
        fwdWeight = 5.0f;
    }

    auto backWeight = 1.5f;
    auto upWeight   = 0.05f;
    if (vehicle->m_nModelIndex == MODEL_SANCHEZ) {
        fwdWeight *= 0.65f;
        upWeight  *= 0.75f;
    } else if (vehicle->IsSubQuad()) {
        backWeight = 3.0f;
        fwdWeight *= 0.65f;
        upWeight  *= 0.75f;
    }

    if (impactFwdMag > 0.0f) {
        fwdWeight *= 1.0f - driver->GetBikeRidingSkill() * 0.6f;
    }

    force *= std::abs(impactFwdMag) * fwdWeight
           + std::max(impactUpMag, 0.0f) * upWeight
           + std::abs(impactRightMag) * 0.45f
           - std::min(impactUpMag, 0.0f) * backWeight;

    // Don't knock the player off while they're on stairs
    if (driver->IsPlayer() && CCullZones::CamStairsForPlayer() && CCullZones::FindZoneWithStairsAttributeForPlayer()) {
        force = 0.0f;
    }

    // ALWAYS_HARD peds come off at a much lower force threshold
    if (force <= (driver->CantBeKnockedOffBike == CANT_BE_KNOCKED_OFF_ALWAYS_HARD ? 20.0f : 75.0f)) {
        return false;
    }

    // NEVER peds are never knocked off
    if (driver->CantBeKnockedOffBike == CANT_BE_KNOCKED_OFF_NEVER) {
        return false;
    }
    if (passenger && passenger->CantBeKnockedOffBike == CANT_BE_KNOCKED_OFF_NEVER) {
        return false;
    }

    // The driver (guaranteed present here) is thrown off, and so is the passenger, both reacting with the driver's facing
    const auto knockOffDir = (uint8)driver->GetLocalDirection(-CVector2D{ collisionImpactVelocity });

    driver->GetEventGroup().Add(CEventKnockOffBike{
        vehicle, vehicle->m_vecMoveSpeed, collisionImpactVelocity, damageIntensity, 0.05f * force, KNOCK_OFF_TYPE_SKIDBACKFRONT, knockOffDir, 0, nullptr, true, false
    });
    if (passenger) {
        passenger->GetEventGroup().Add(CEventKnockOffBike{
            vehicle, vehicle->m_vecMoveSpeed, collisionImpactVelocity, damageIntensity, 0.05f * force, KNOCK_OFF_TYPE_SKIDBACKFRONT, knockOffDir, 0, nullptr, false, false
        });
    }
    return true;
}

// dummy function
// 0x6B5F40
CPed* CBike::KnockOffRider(eWeaponType arg0, uint8 arg1, CPed* ped, bool arg3) {
    return ped;
}

// 0x6B5F50
void CBike::SetRemoveAnimFlags(CPed* ped) {
    if (!ped->GetIsTypePed()) {
        return;
    }
    for (auto assoc = RpAnimBlendClumpGetFirstAssociation(ped->GetRpClump(), ANIMATION_SECONDARY_TASK_ANIM); assoc; assoc = RpAnimBlendGetNextAssociation(assoc, ANIMATION_SECONDARY_TASK_ANIM)) {
        assoc->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
    }
}

// 0x6B5F90
void CBike::ReduceHornCounter() {
    if (m_HornCounter)
        m_HornCounter -= 1;
}

// 0x6B5FB0
void CBike::ProcessBuoyancy() {
    CVector vecBuoyancyTurnPoint;
    CVector vecBuoyancyForce;
    if (!mod_Buoyancy.ProcessBuoyancy(this, m_fBuoyancyConstant, &vecBuoyancyTurnPoint, &vecBuoyancyForce)) {
        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
        physicalFlags.bTouchingWater = false;
        return;
    }

    physicalFlags.bTouchingWater = true;
    ApplyMoveForce(vecBuoyancyForce);
    ApplyTurnForce(vecBuoyancyForce, vecBuoyancyTurnPoint);

    auto fTimeStep = std::max(0.01F, CTimer::GetTimeStep());
    auto fUsedMass = m_fMass / 125.0F;
    auto fBuoyancyForceZ = vecBuoyancyForce.z / (fTimeStep * fUsedMass);

    if (fUsedMass > m_fBuoyancyConstant)
        fBuoyancyForceZ *= 1.05F * fUsedMass / m_fBuoyancyConstant;

    if (physicalFlags.bMakeMassTwiceAsBig)
        fBuoyancyForceZ *= 1.5F;

    auto fBuoyancyForceMult = std::max(0.5F, 1.0F - fBuoyancyForceZ / 20.0F);
    auto fSpeedMult = std::pow(fBuoyancyForceMult, CTimer::GetTimeStep());
    m_vecMoveSpeed *= fSpeedMult;
    m_vecTurnSpeed *= fSpeedMult;

    // 0x6B6443
    if (fBuoyancyForceZ > 0.8F || (fBuoyancyForceZ > 0.4F && IsAnyWheelNotMakingContactWithGround())) {
        vehicleFlags.bIsDrowning = true;
        physicalFlags.bSubmergedInWater = true;

        m_vecMoveSpeed.z = std::max(-0.1F, m_vecMoveSpeed.z);

        if (m_pDriver) {
            ProcessPedInVehicleBuoyancy(m_pDriver->AsPed(), true);
        }
        else {
            vehicleFlags.bEngineOn = false;
        }

        for (const auto passenger : GetPassengers()) {
            ProcessPedInVehicleBuoyancy(passenger, false);
        }
    }
    else {
        vehicleFlags.bIsDrowning = false;
        physicalFlags.bSubmergedInWater = false;
    }
}

inline void CBike::ProcessPedInVehicleBuoyancy(CPed* ped, bool bIsDriver) {
    if (!ped)
        return;

    ped->physicalFlags.bTouchingWater = true;
    if (!ped->IsPlayer() && bikeFlags.bWaterTight)
        return;

    if (ped->IsPlayer())
        ped->AsPlayer()->HandlePlayerBreath(true, 1.0F);

    if (IsAnyWheelMakingContactWithGround()) {
        if (!ped->IsPlayer()) {
            auto pedDamageResponseCalc = CPedDamageResponseCalculator(this, CTimer::GetTimeStep(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, false);
            auto damageEvent = CEventDamage(this, CTimer::GetTimeInMS(), eWeaponType::WEAPON_DROWNING, PED_PIECE_TORSO, 0, false, true);
            if (damageEvent.AffectsPed(ped))
                pedDamageResponseCalc.ComputeDamageResponse(ped, damageEvent.m_damageResponse, true);
            else
                damageEvent.m_damageResponse.m_bDamageCalculated = true;

            ped->GetEventGroup().Add(&damageEvent, false);
        }
    } else {
        auto knockOffBikeEvent = CEventKnockOffBike(this, m_vecMoveSpeed, m_vecLastCollisionImpactVelocity, m_fDamageIntensity, 0.0F, KNOCK_OFF_TYPE_FALL, 0, 0, nullptr, bIsDriver, false);
        ped->GetEventGroup().Add(&knockOffBikeEvent);
        if (bIsDriver) {
            vehicleFlags.bEngineOn = false;
        }
    }
}

// 0x6BC930
bool CBike::ProcessAI(uint32& extraHandlingFlags) {
    const auto mi = GetVehicleModelInfo();

    m_autoPilot.carCtrlFlags.bHonkAtCar = false;
    m_autoPilot.carCtrlFlags.bHonkAtPed = false;

    if (m_autoPilot.m_vehicleRecordingId >= 0 && !CVehicleRecording::bUseCarAI[m_autoPilot.m_vehicleRecordingId]) {
        extraHandlingFlags += 2;
        return false;
    }

    switch (GetStatus()) {
    case STATUS_PLAYER: {
        extraHandlingFlags += 2;
        bikeFlags.bGettingPickedUp = false;

        const auto plyrState = FindPlayerPed()->m_nPedState;
        if (plyrState != PEDSTATE_EXIT_CAR && FindPlayerPed()->m_nPedState != PEDSTATE_DRAGGED_FROM_CAR) {
            if (m_pDriver) {
                if (CWorld::Players[0].m_pPed == m_pDriver) {
                    ProcessControlInputs(0);
                } else if (CWorld::Players[1].m_pPed == m_pDriver) {
                    ProcessControlInputs(1);
                }
            }

            // Apply a turn force based on the lean (wheelie/stoppie)
            const auto& bh = *m_BikeHandling;
            if (m_RideAnimData.LeanFwd < 0.0f) {
                m_vecCentreOfMass.y = bh.m_fLeanBakCOM * m_RideAnimData.LeanFwd + m_pHandlingData->m_vecCentreOfMass.y;

                if (!((m_BrakePedal != 0.0f || vehicleFlags.bIsHandbrakeOn) && m_nNoOfContactWheels)) {
                    const auto speed = std::min(0.1f, m_vecMoveSpeed.Magnitude());
                    const auto mult  = std::max(speed / 0.1f, m_GasPedal) + m_GasPedal;
                    const auto force = (mult * (((bh.m_fLeanBakForce * m_fTurnMass) * m_RideAnimData.LeanFwd) * speed)) * 0.5f;
                    ApplyTurnForce(
                        GetUp() * -(CTimer::GetTimeStep() * (CStats::GetFatAndMuscleModifier(STAT_MOD_11) * force)),
                        m_vecCentreOfMass + GetForward()
                    );
                }
            } else {
                m_vecCentreOfMass.y = bh.m_fLeanFwdCOM * m_RideAnimData.LeanFwd + m_pHandlingData->m_vecCentreOfMass.y;

                if (m_BrakePedal < 0.0f || !m_nNoOfContactWheels) {
                    const auto speed = std::min(0.1f, m_vecMoveSpeed.Magnitude());
                    const auto mult  = std::max(speed / 0.1f, m_BrakePedal) + m_BrakePedal;
                    const auto force = (mult * (((bh.m_fLeanFwdForce * m_fTurnMass) * m_RideAnimData.LeanFwd) * speed)) * 0.5f;
                    ApplyTurnForce(
                        GetUp() * -(CTimer::GetTimeStep() * (CStats::GetFatAndMuscleModifier(STAT_MOD_11) * force)),
                        m_vecCentreOfMass + GetForward()
                    );
                }
            }

            PruneReferences();
            if (GetStatus() == STATUS_PLAYER) {
                DoDriveByShootings();
            }
            DoSoftGroundResistance(extraHandlingFlags);
        }

        if (CPad::GetPad(0)->CarGunJustDown()) {
            ActivateBomb();
        }
        return true;
    }
    case STATUS_PLAYER_PLAYBACK_FROM_BUFFER: {
        extraHandlingFlags += 2;
        return true;
    }
    case STATUS_SIMPLE: {
        CCarAI::UpdateCarAI(this);
        CPhysical::ProcessControl();
        CCarCtrl::UpdateCarOnRails(this);

        const auto speed = m_autoPilot.m_speed * 0.02f;
        m_NumDriveWheelsOnGroundLastFrame = m_NumDriveWheelsOnGround;
        m_nNoOfContactWheels              = 2;
        m_NumDriveWheelsOnGround          = 2;
        m_pHandlingData->GetTransmission().CalculateGearForSimpleCar(speed, m_nCurrentGear);

        {
            const auto ts       = CTimer::GetTimeStep();
            const auto rotation = ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, mi->m_fWheelSizeFront * 0.5f);
            m_aWheelPitchAngles[0] = rotation * ts + m_aWheelPitchAngles[0];
        }
        {
            const auto ts       = CTimer::GetTimeStep();
            const auto rotation = ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, mi->m_fWheelSizeRear * 0.5f);
            m_aWheelPitchAngles[1] = rotation * ts + m_aWheelPitchAngles[1];
        }

        PlayHornIfNecessary();
        ReduceHornCounter();

        bikeFlags.bWheelieForCamera       = false;
        vehicleFlags.bVehicleColProcessed = false;
        vehicleFlags.bAudioChangingGear   = false;
        return true;
    }
    case STATUS_PHYSICS:
    case STATUS_GHOST: {
        CCarAI::UpdateCarAI(this);
        CCarCtrl::SteerAICarWithPhysics(this);
        PlayHornIfNecessary();
        extraHandlingFlags += 2;
        bikeFlags.bWheelieForCamera = false;
        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal                  = 0.0f;
            m_BrakePedal                = 1.0f;
            return true;
        }
        bikeFlags.bGettingPickedUp = false;
        return true;
    }
    case STATUS_ABANDONED: {
        m_BrakePedal = 0.0f;
        vehicleFlags.bIsHandbrakeOn = m_vecMoveSpeed.SquaredMagnitude() < 0.01f || bikeFlags.bOnSideStand;
        m_GasPedal    = 0.0f;
        m_HornCounter = 0;
        if (m_pDriver || m_apPassengers[0] || vehicleFlags.bIsBeingCarJacked) {
            if (!bikeFlags.bOnSideStand) {
                extraHandlingFlags += 2;
            }
        }
        bikeFlags.bWheelieForCamera = false;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd  = 0.0f;
        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal                  = 0.0f;
            m_BrakePedal                = 1.0f;
        }
        return true;
    }
    case STATUS_WRECKED: {
        m_BrakePedal                = 0.05f;
        vehicleFlags.bIsHandbrakeOn = true;
        bikeFlags.bWheelieForCamera = false;
        m_fSteerAngle               = 0.0f;
        m_GasPedal                  = 0.0f;
        m_HornCounter               = 0;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd  = 0.0f;
        return true;
    }
    case STATUS_REMOTE_CONTROLLED: {
        if (m_vecMoveSpeed.SquaredMagnitude() < 0.01f) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_BrakePedal                = 1.0f;
        } else {
            vehicleFlags.bIsHandbrakeOn = false;
            m_BrakePedal                = 0.0f;
        }
        m_fSteerAngle = 0.0f;
        m_GasPedal    = 0.0f;
        m_HornCounter = 0;
        extraHandlingFlags += 2;
        bikeFlags.bWheelieForCamera = false;
        return true;
    }
    default:
        return true;
    }
}

// 0x6BF400
void CBike::ProcessDrivingAnims(CPed* driver, bool blend) {
    if (m_bOffscreen && GetStatus() == STATUS_PLAYER)
        return;

    ProcessRiderAnims(driver, this, &m_RideAnimData, m_BikeHandling, 0);
}

// 0x6B7280
void CBike::ProcessRiderAnims(CPed* rider, CVehicle* vehicle, CRideAnimData* rideData, tBikeHandlingData* handling, int16 a5) {
    const auto clump    = rider->GetRpClump();
    const auto isPlayer = rider->IsPlayer();

    CBike*       bike{};
    CAutomobile* automobile{};
    int16        numContactWheels{};
    if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        bike             = vehicle->AsBike();
        numContactWheels = bike->m_nNoOfContactWheels;
    } else if (vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        automobile       = vehicle->AsAutomobile();
        numContactWheels = automobile->m_nNumContactWheels;
    }

    float blendLeft     = 1.0f; // How much of the blend is left for the other animations
    float fwdLeanTarget = 0.0f;

    auto leftAnim  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_LEFT);
    auto rightAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_RIGHT);
    auto stillAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_STILL);
    CAnimBlendAssociation* fwdAnim{};
    CAnimBlendAssociation* backAnim{};
    if (isPlayer) {
        fwdAnim  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_FWD);
        backAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_BACK);
    }
    auto pushAnim    = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_PUSHES);
    auto drivebyAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYLHS);
    if (!drivebyAnim) {
        drivebyAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYRHS);
        if (!drivebyAnim) {
            drivebyAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYFT);
        }
    }

    const auto& moveSpeed = vehicle->m_vecMoveSpeed;
    const auto& vehFwd    = vehicle->GetForward();
    const auto  fwdSpeed  = (moveSpeed.z * vehFwd.z + moveSpeed.y * vehFwd.y) + moveSpeed.x * vehFwd.x;

    if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX && (pushAnim || drivebyAnim)) {
        vehicle->AsBike()->m_nFixRightHand = false;
    }

    const auto KnockOffRiderAndPassenger = [&] {
        rider->GetEventGroup().Add(CEventKnockOffBike{
            vehicle, vehicle->m_vecMoveSpeed, CVector{ 0.0f, 0.0f, 1.0f }, 0.0f, 0.0f, KNOCK_OFF_TYPE_SKIDBACK_FALLR, 2, 0, nullptr, true, false
        });
        if (const auto passenger = vehicle->m_apPassengers[0]; passenger && passenger != rider) {
            passenger->GetEventGroup().Add(CEventKnockOffBike{
                vehicle, vehicle->m_vecMoveSpeed, CVector{ 0.0f, 0.0f, 1.0f }, 0.0f, 0.0f, KNOCK_OFF_TYPE_SKIDBACK_FALLR, 2, 0, nullptr, false, false
            });
        }
    };

    //> Knock the rider off if the bike is spinning too fast
    {
        const auto threshold = (rider->GetBikeRidingSkill() + 1.0f) * 0.3f;
        const auto& turnSpeed = vehicle->m_vecTurnSpeed;
        if (threshold * threshold < (turnSpeed.x * turnSpeed.x + turnSpeed.y * turnSpeed.y) + turnSpeed.z * turnSpeed.z) {
            KnockOffRiderAndPassenger();
        }
    }

    //> Blend the still/pushing animations depending on the bike's speed
    const auto FadeOutStillAndPushAnims = [&] {
        if (stillAnim && !(stillAnim->m_BlendDelta < 0.0f)) {
            stillAnim->m_BlendDelta = -4.0f;
        }
        if (pushAnim && !(pushAnim->m_BlendDelta < 0.0f)) {
            pushAnim->m_BlendDelta = -4.0f;
        }
    };
    const auto NeedsNewBlend = [](CAnimBlendAssociation* anim) {
        return !anim || (anim->m_BlendAmount < 1.0f && anim->m_BlendDelta <= 0.0f);
    };

    if (!drivebyAnim && std::abs(fwdSpeed) < 0.02f) { // 0x871334
        if (NeedsNewBlend(stillAnim)) {
            stillAnim = CAnimManager::BlendAnimation(clump, rideData->AnimGroup, ANIM_ID_BIKE_STILL, 2.0f);
        }
    } else if (fwdSpeed >= 0.0f) {
        FadeOutStillAndPushAnims();
    } else {
        const auto  maxReverseVelocity = vehicle->m_pHandlingData->m_transmissionData.m_MaxReverseVelocity;
        auto        skillMult          = (rider->GetBikeRidingSkill() + 1.0f) * 3.5f; // 0x859028
        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
            skillMult = skillMult + skillMult;
        }
        if (skillMult * maxReverseVelocity > fwdSpeed
            && (numContactWheels > 0 || (vehicle->GetUp().z < -0.5f && CTimer::GetTimeInMS() - vehicle->m_nLastCollisionTime < 100u))
        ) {
            KnockOffRiderAndPassenger();
        } else if (vehicle->m_GasPedal < 0.0f && fwdSpeed > maxReverseVelocity * 1.5) { // 0x86C680 is a double
            // Pushing the bike backwards
            if (NeedsNewBlend(pushAnim)) {
                pushAnim = CAnimManager::BlendAnimation(clump, rideData->AnimGroup, ANIM_ID_BIKE_PUSHES, 4.0f);
            }
        } else {
            if (isPlayer && fwdSpeed < maxReverseVelocity * 1.5) {
                fwdLeanTarget = -1.0f;
            }
            FadeOutStillAndPushAnims();
        }
    }

    //> Calculate how much blend is left
    const auto GetNextBlendAmount = [](const CAnimBlendAssociation* anim) {
        return CTimer::GetTimeStepNonClipped() * 0.02f * anim->m_BlendDelta + anim->m_BlendAmount;
    };
    if (stillAnim) {
        auto amount = GetNextBlendAmount(stillAnim);
        if (1.0f < amount) {
            amount = 1.0f;
        }
        blendLeft = 1.0f - amount;
    }
    if (drivebyAnim) {
        auto amount = GetNextBlendAmount(drivebyAnim);
        if (blendLeft < amount) {
            amount = blendLeft;
        }
        blendLeft = blendLeft - amount;
    }
    if (pushAnim) {
        auto amount = GetNextBlendAmount(pushAnim);
        if (blendLeft < amount) {
            amount = blendLeft;
        }
        blendLeft = blendLeft - amount;
    }

    //> Lean (left/right)
    auto leanRatio = rideData->LeanAngle / handling->m_fFullAnimLean;
    if (fwdLeanTarget == -1.0f) {
        leanRatio = 0.0f;
    } else if (leanRatio > 1.0f) {
        leanRatio = 1.0f;
    } else if (leanRatio < -1.0f) {
        leanRatio = -1.0f;
    }
    {
        const auto smoothing = std::pow(0.86f, CTimer::GetTimeStep()); // 0x871350
        rideData->AnimLeanLeft = smoothing * rideData->AnimLeanLeft + (1.0f - smoothing) * leanRatio;
    }

    //> Lean (forwards/backwards)
    if (!isPlayer || vehicle->m_apPassengers[0]) {
        fwdLeanTarget = 0.0f;
        if (bike) {
            bike->bikeFlags.bWheelieForCamera = false;
        }
    } else if (fwdLeanTarget > -1.0f) {
        fwdLeanTarget = rideData->LeanFwd;

        // 0x7ac4
        const auto ProcessAccelerationLean = [&] {
            if (vehicle->m_BrakePedal > 0.5f && fwdSpeed > 0.01f) {
                if (!(fwdLeanTarget > 0.1f)) { // 0x87133C
                    fwdLeanTarget = 0.1f;
                }
            } else if (vehicle->m_GasPedal > 0.5f && fwdLeanTarget <= 0.0f && fwdSpeed < vehicle->m_pHandlingData->m_transmissionData.m_MaxFlatVelocity * 0.3f) {
                if (!(fwdLeanTarget < -0.3f)) { // 0x871340
                    fwdLeanTarget = -0.3f;
                }
            }
        };
        // 0x7b90
        const auto ProcessWheelieLean = [&](float wheelieDelta) {
            if (wheelieDelta < 0.15f) { // 0x858FCC
                if (!(fwdLeanTarget > 0.25f)) { // 0x871344
                    fwdLeanTarget = 0.25f;
                }
            } else {
                ProcessAccelerationLean();
            }
        };

        if (bike) {
            bike->bikeFlags.bWheelieForCamera = false;

            const auto& wc   = bike->m_WheelCounts;
            const auto  fwdZ = bike->GetForward().z;
            if (wc[0] <= 0.0f && wc[1] <= 0.0f && fwdZ > 0.0f && (wc[2] > 0.0f || wc[3] > 0.0f)) {
                const auto wheelieDelta = handling->m_fWheelieAng - fwdZ;
                if (wheelieDelta < handling->m_fWheelieAng * 0.5f) {
                    bike->bikeFlags.bWheelieForCamera = true;
                }
                ProcessWheelieLean(wheelieDelta);
            } else {
                if (wc[2] <= 0.0f && wc[3] <= 0.0f && fwdZ < 0.0f && (wc[0] > 0.0f || wc[1] > 0.0f)) {
                    if (handling->m_fStoppieAng * 0.6f < handling->m_fStoppieAng - fwdZ) {
                        bike->bikeFlags.bWheelieForCamera = true;
                    }
                }
                ProcessAccelerationLean();
            }
        } else if (automobile) {
            const auto& wc   = automobile->m_WheelCounts;
            const auto  fwdZ = automobile->GetForward().z;
            if (wc[0] <= 0.0f && wc[1] <= 0.0f && fwdZ > 0.0f && (wc[2] > 0.0f || wc[3] > 0.0f)) {
                ProcessWheelieLean(handling->m_fWheelieAng - fwdZ);
            } else {
                ProcessAccelerationLean();
            }
        } else {
            ProcessAccelerationLean();
        }

        // Lean less when leaning to the sides
        if (std::abs(leanRatio) > 0.3f) { // 0x871358
            auto mult = 1.0f - (std::abs(leanRatio) - 0.3f) / (0.56f - 0.3f); // 0x871354
            if (!(0.0f <= mult)) {
                mult = 0.0f;
            }
            fwdLeanTarget = mult * fwdLeanTarget;
        }
    }

    if (isPlayer) {
        const auto smoothing = std::pow(0.89f, CTimer::GetTimeStep()); // 0x87134C
        rideData->AnimLeanFwd = smoothing * rideData->AnimLeanFwd + (1.0f - smoothing) * fwdLeanTarget;
    } else {
        rideData->AnimLeanFwd = 0.0f;
    }

    //> Calculate the weights of the animations
    float leftRightWeight, fwdBackWeight;
    if (std::abs(rideData->AnimLeanLeft) > 0.56f || !isPlayer) {
        fwdBackWeight   = 0.0f;
        leftRightWeight = 1.0f;
    } else if (std::abs(rideData->AnimLeanFwd) > 0.56f) {
        fwdBackWeight   = 1.0f;
        leftRightWeight = 0.0f;
    } else {
        const auto leanLeft = rideData->AnimLeanLeft;
        const auto leanFwd  = rideData->AnimLeanFwd;
        const auto length   = std::sqrt(leanLeft * leanLeft + leanFwd * leanFwd);
        if (length <= 0.01f) { // 0x87135C
            fwdBackWeight   = std::abs(leanFwd);
            leftRightWeight = std::abs(leanLeft);
        } else {
            const auto invLength = 1.0f / length;
            fwdBackWeight        = std::abs(leanFwd * invLength);
            leftRightWeight      = std::abs(invLength * leanLeft);
        }
    }
    fwdBackWeight *= blendLeft;
    leftRightWeight *= blendLeft;

    if (isPlayer) {
        if (!fwdAnim) {
            fwdAnim = CAnimManager::AddAnimation(clump, rideData->AnimGroup, ANIM_ID_BIKE_FWD);
        }
        if (!backAnim) {
            backAnim = CAnimManager::AddAnimation(clump, rideData->AnimGroup, ANIM_ID_BIKE_BACK);
        }
        if (rideData->AnimLeanFwd >= 0.0f) {
            fwdAnim->m_BlendAmount = fwdBackWeight;
            fwdAnim->SetCurrentTime(fwdAnim->GetHier()->GetTotalTime() * rideData->AnimLeanFwd);
            fwdAnim->SetFlag(ANIMATION_IS_PLAYING, false);
            backAnim->m_BlendAmount = 0.0f;
        } else {
            backAnim->m_BlendAmount = fwdBackWeight;
            backAnim->SetCurrentTime(-(backAnim->GetHier()->GetTotalTime() * rideData->AnimLeanFwd));
            backAnim->SetFlag(ANIMATION_IS_PLAYING, false);
            fwdAnim->m_BlendAmount = 0.0f;
        }
    }

    if (!leftAnim) {
        leftAnim = CAnimManager::AddAnimation(clump, rideData->AnimGroup, ANIM_ID_BIKE_LEFT);
    }
    if (!rightAnim) {
        rightAnim = CAnimManager::AddAnimation(clump, rideData->AnimGroup, ANIM_ID_BIKE_RIGHT);
    }
    if (rideData->AnimLeanLeft >= 0.0f) {
        rightAnim->m_BlendAmount = leftRightWeight;
        rightAnim->SetCurrentTime(rightAnim->GetHier()->GetTotalTime() * rideData->AnimLeanLeft);
        rightAnim->SetFlag(ANIMATION_IS_PLAYING, false);
        leftAnim->m_BlendAmount = 0.0f;
    } else {
        leftAnim->m_BlendAmount = leftRightWeight;
        leftAnim->SetCurrentTime(-(leftAnim->GetHier()->GetTotalTime() * rideData->AnimLeanLeft));
        leftAnim->SetFlag(ANIMATION_IS_PLAYING, false);
        rightAnim->m_BlendAmount = 0.0f;
    }

    //> Wobble the rider's head when going fast
    if (fwdSpeed > 0.3f) {
        const auto headQuat = &rider->m_apBones[PED_NODE_HEAD]->KeyFrame->q;
        for (const auto* axis : { &CPedIK::XaxisIK, &CPedIK::YaxisIK }) {
            const auto range  = 6.0f * fwdSpeed; // 0x871338
            const auto minVal = -range;
            const auto rnd    = static_cast<float>(CGeneral::GetRandomNumber()) * RAND_MAX_FLOAT_RECIPROCAL; // 0x858C7C
            RtQuatRotate(headQuat, axis, (range - minVal) * rnd + minVal, rwCOMBINEPOSTCONCAT);
        }
        rider->bUpdateMatricesRequired = true;
    }
}

// 0x6BEB20
bool CBike::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    if (vehicleFlags.bTyresDontBurst || physicalFlags.bRenderScorched) {
        return false;
    }

    // Note: Only 0xD (front) and 0xF (rear) are translated, other values are left as-is
    uint8 wheel = tyreComponentId;
    if (wheel == 0xD) {
        wheel = 0;
    } else if (wheel == 0xF) {
        wheel = 1;
    }

    // BUG: `wheel` isn't bounds checked (0xE would index past the array into `m_aWheelColPoints`)
    if (notsa::IsFixBugs() && wheel >= m_nWheelStatus.size()) {
        return false;
    }
    auto& wheelStatus = *(m_nWheelStatus.data() + wheel);

    bool burst = false;
    if (wheelStatus == 0) {
        wheelStatus = 1;
        m_vehicleAudio.AddAudioEvent(AE_TYRE_BURST, 0.0f);
        if (GetStatus() == STATUS_SIMPLE) {
            CCarCtrl::SwitchVehicleToRealPhysics(this);
        }
        if (bPhysicalEffect) {
            constexpr auto force = 0.02f;
            const auto     moveForce = CGeneral::GetRandomNumberInRange(-force, force) * m_fMass;
            ApplyMoveForce(GetRight() * moveForce);
            const auto     turnForce = CGeneral::GetRandomNumberInRange(-force, force) * m_fTurnMass;
            ApplyTurnForce(GetRight() * turnForce, GetForward());
        }
        burst = true;
    }

    if (!m_pDriver) {
        return burst;
    }

    // NOTE: Because `wheel` was translated above, `wheel == 0xD` can never be true here (dead code in the original)
    if (wheel == 0xD) {
        if (!(m_aRatioHistory[0] < 1.0f) && !(m_aRatioHistory[1] < 1.0f)) {
            return burst;
        }
    } else if (wheel == 0xE) {
        if (!(m_aRatioHistory[2] < 1.0f) && !(m_aRatioHistory[3] < 1.0f)) {
            return burst;
        }
    } else {
        return burst;
    }

    const auto speed = std::sqrt(m_vecMoveSpeed.x * m_vecMoveSpeed.x + m_vecMoveSpeed.y * m_vecMoveSpeed.y + m_vecMoveSpeed.z * m_vecMoveSpeed.z);
    if (!(speed > 0.3f) || (GetStatus() == STATUS_PLAYER && !(speed > 0.55f))) {
        return burst;
    }

    if (wheel == 0xD) {
        m_pDriver->GetEventGroup().Add(CEventKnockOffBike{
            this, m_vecMoveSpeed, m_vecLastCollisionImpactVelocity, 0.0f, 0.0f, KNOCK_OFF_TYPE_SKIDBACKFRONT, 0, 0, nullptr, true, false
        });
        if (const auto passenger = m_apPassengers[0]) {
            passenger->GetEventGroup().Add(CEventKnockOffBike{
                this, m_vecMoveSpeed, m_vecLastCollisionImpactVelocity, 0.0f, 0.0f, KNOCK_OFF_TYPE_SKIDBACKFRONT, 0, 0, nullptr, false, false
            });
        }
    } else {
        const auto turnForce = 0.02f * m_fTurnMass;
        ApplyTurnForce(GetRight() * (turnForce + turnForce), GetForward());
    }
    return burst;
}

// 0x6BE310
void CBike::ProcessControlInputs(uint8 playerNum) {
    const auto pad = CPad::GetPad(playerNum);

    // `m_RideAnimData.LeanFwd` is the up/down steering (Reused as the lean input)
    auto& leanFwd = m_RideAnimData.LeanFwd;

    const auto fwdSpeed = m_vecMoveSpeed.z * GetForward().z + m_vecMoveSpeed.y * GetForward().y + m_vecMoveSpeed.x * GetForward().x;

    //> Handbrake
    if (pad->GetExitVehicle()) {
        vehicleFlags.bIsHandbrakeOn = true;
    } else {
        vehicleFlags.bIsHandbrakeOn = CPad::GetPad(playerNum)->GetHandBrake() != 0;
    }

    //> Steering
    const auto SteerWithKeyboard = [&] {
        const auto ts1 = CTimer::GetTimeStep();
        m_fRawSteerAngle = (((float)-CPad::GetPad(playerNum)->GetSteeringLeftRight() * (1.0f / 128.0f) - m_fRawSteerAngle) * ts1) * 0.2f + m_fRawSteerAngle;

        const auto ts2 = CTimer::GetTimeStep();
        leanFwd = (((float)-CPad::GetPad(playerNum)->GetSteeringUpDown() * (1.0f / 128.0f) - leanFwd) * ts2) * 0.2f + leanFwd;
    };

    if (!CCamera::m_bUseMouse3rdPerson || !m_bEnableMouseSteering) {
        SteerWithKeyboard();
    } else {
        const auto SteerWithMouse = [&] {
            m_nLastControlInput = eControllerType::MOUSE;
            if (!CPad::GetPad(playerNum)->NewState.m_bVehicleMouseLook) {
                m_fRawSteerAngle += CPad::NewMouseControllerState.m_AmountMoved.x * -0.0035f;
                leanFwd          += CPad::NewMouseControllerState.m_AmountMoved.y * -0.0035f;
            }

            if (std::abs(m_fRawSteerAngle) < 0.35f || CPad::GetPad(playerNum)->NewState.m_bVehicleMouseLook) {
                m_fRawSteerAngle = std::pow(0.98f, CTimer::GetTimeStep()) * m_fRawSteerAngle;
            }
            if (std::abs(leanFwd) < 0.35f || CPad::GetPad(playerNum)->NewState.m_bVehicleMouseLook) {
                leanFwd = std::pow(0.98f, CTimer::GetTimeStep()) * leanFwd;
            }
        };

        if (CPad::NewMouseControllerState.m_AmountMoved.x != 0.0f || CPad::NewMouseControllerState.m_AmountMoved.y != 0.0f) {
            SteerWithMouse();
        } else {
            const auto LastInputWasMouseAndSteering = std::abs(m_fRawSteerAngle) > 0.0f && m_nLastControlInput == eControllerType::MOUSE;
            if (   LastInputWasMouseAndSteering
                && pad->GetSteeringLeftRight() == 0
                && pad->GetSteeringUpDown() == 0
            ) {
                SteerWithMouse();
            } else if (   pad->GetSteeringLeftRight() == 0
                       && pad->GetSteeringUpDown() == 0
                       && m_nLastControlInput == eControllerType::MOUSE
            ) {
                // Nothing to do
            } else {
                m_nLastControlInput = eControllerType::KEYBOARD;
                SteerWithKeyboard();
            }
        }
    }

    m_fRawSteerAngle = m_fRawSteerAngle <= -1.0f ? -1.0f : (m_fRawSteerAngle < 1.0f ? m_fRawSteerAngle : 1.0f);
    leanFwd          = leanFwd <= -1.0f          ? -1.0f : (leanFwd < 1.0f ? leanFwd : 1.0f);

    //> Gas/Brake
    const auto accelerate = CPad::GetPad(playerNum)->GetAccelerate();
    const auto brake      = CPad::GetPad(playerNum)->GetBrake();
    const auto gasInput   = (float)(accelerate - brake) * (1.0f / 255.0f);

    const auto absFwdSpeed = fwdSpeed < 0.0f ? -fwdSpeed : fwdSpeed;
    if (absFwdSpeed < 0.01f) {
        if (CPad::GetPad(playerNum)->GetAccelerate() > 150 && CPad::GetPad(playerNum)->GetBrake() > 150 && m_nVehicleSubType != VEHICLE_TYPE_BMX) {
            m_GasPedal   = (float)CPad::GetPad(playerNum)->GetAccelerate() * (1.0f / 255.0f);
            m_BrakePedal = (float)CPad::GetPad(playerNum)->GetBrake() * (1.0f / 255.0f);
            m_nBrakesOn  = 1; // Burnout
        } else {
            m_GasPedal   = gasInput;
            m_BrakePedal = 0.0f;
        }
    } else if (fwdSpeed >= 0.0f) {
        if (gasInput < 0.0f) {
            m_GasPedal   = 0.0f;
            m_BrakePedal = -gasInput;
        } else {
            m_GasPedal   = gasInput;
            m_BrakePedal = 0.0f;
        }
    } else {
        if (gasInput < 0.0f) {
            m_GasPedal   = gasInput;
            m_BrakePedal = 0.0f;
        } else {
            m_GasPedal   = 0.0f;
            m_BrakePedal = gasInput;
        }
    }

    //> Steer angle
    s_BikeSteerAngleSq = m_fRawSteerAngle * m_fRawSteerAngle;
    if (m_fRawSteerAngle < 0.0f) {
        s_BikeSteerAngleSq = -s_BikeSteerAngleSq;
    }
    if (m_autoPilot.m_vehicleRecordingId < 0 || CVehicleRecording::bUseCarAI[m_autoPilot.m_vehicleRecordingId]) {
        m_fSteerAngle = m_pHandlingData->m_fSteeringLock * 0.017453292f * s_BikeSteerAngleSq;
    }

    //> Comedy controls
    if (vehicleFlags.bComedyControls) {
        const auto time = CTimer::GetTimeInMS();
        if ((time & 0x3C00) < 0x3000) {
            m_GasPedal = 1.0f;
        }
        if ((uint8)(((time >> 10) + 6) & 0xF) < 12) {
            m_BrakePedal = 0.0f;
        }
        vehicleFlags.bIsHandbrakeOn = false;
        if (CTimer::GetTimeInMS() & 0x800) {
            m_fSteerAngle += 0.08f;
        } else {
            m_fSteerAngle -= 0.03f;
        }
    }

    //> Slow the bike down when the player's controls are disabled
    if (CPad::GetPad(0)->DisablePlayerControls && CGameLogic::SkipState != 2) {
        m_BrakePedal                = 1.0f;
        vehicleFlags.bIsHandbrakeOn = true;
        m_GasPedal                  = 0.0f;
        FindPlayerPed()->KeepAreaAroundPlayerClear();

        const auto speed = std::sqrt((m_vecMoveSpeed.x * m_vecMoveSpeed.x + m_vecMoveSpeed.y * m_vecMoveSpeed.y) + m_vecMoveSpeed.z * m_vecMoveSpeed.z);
        if (speed > 0.28f) {
            const auto mult = 0.28f / speed;
            m_vecMoveSpeed.x = mult * m_vecMoveSpeed.x;
            m_vecMoveSpeed.y = mult * m_vecMoveSpeed.y;
            m_vecMoveSpeed.z = mult * m_vecMoveSpeed.z;
        }
    }
}

// 0x6BDEA0
int32 CBike::ProcessEntityCollision(CEntity* entity, CColPoint* outColPoints) {
    if (GetStatus() != STATUS_SIMPLE) {
        vehicleFlags.bVehicleColProcessed = true;
    }

    const auto tcd = GetColData(),
               ocd = entity->GetColData();

#ifdef FIX_BUGS // Text search for `FIX_BUGS@CAutomobile::ProcessEntityCollision:1`
    if (!tcd || !ocd) {
        return 0;
    }
#endif

    if (physicalFlags.bSkipLineCol || physicalFlags.bProcessingShift || entity->GetIsTypePed()) {
        tcd->m_nNumLines = 0; // Later reset back to original value
    }

    const auto ogWheelRatios = m_aWheelRatios;

    auto numColPts = CCollision::ProcessColModels(
        GetMatrix(), *GetColModel(),
        entity->GetMatrix(), *entity->GetColModel(),
        *(std::array<CColPoint, 32>*)(outColPoints),
        m_aWheelColPoints.data(),
        m_aWheelRatios.data(),
        false
    );

    // Possibly add driver & entity collisions to `outColPoints`
    if (m_pDriver && m_nTestPedCollision) {
        const auto pcd = m_pDriver->GetColData();
        if (!pcd->m_nNumLines) {
            std::array<CColPoint, 32> pedCPs{};

            CMatrix driverMat = GetMatrix();
            driverMat.GetPosition() += GetDriverSeatDummyPositionWS();

            std::array<CColPoint, 32> pedEntityColPts{};
            const auto numPedEntityColPts = CCollision::ProcessColModels(
                driverMat, *m_pDriver->GetColModel(),
                entity->GetMatrix(), *entity->GetColModel(),
                pedEntityColPts,
                nullptr,
                nullptr,
                false
            );

            if (numPedEntityColPts) {
                if (m_nTestPedCollision == 1) {
                    m_nTestPedCollision = 0;
                } else {
                    for (auto i = 0; i < numPedEntityColPts && numColPts < 32; i++) {
                        const auto& pedEntityCP = pedCPs[i];
                        if (pedEntityCP.m_nPieceTypeA == PED_COL_SPHERE_LEG) {
                            continue;
                        }
                        outColPoints[numColPts++] = pedEntityCP;
                    }
                }
            }
        }
    }
    
    size_t numProcessedLines{};
    if (tcd->m_nNumLines) {
        // Process the real wheels
        for (auto i = 0; i < NUM_SUSP_LINES; i++) {
            const auto& cp = m_aWheelColPoints[i];

            const auto wheelColPtsTouchDist = m_aWheelRatios[i];
            if (wheelColPtsTouchDist >= 1.f || wheelColPtsTouchDist >= ogWheelRatios[i]) {
                continue;
            }

            numProcessedLines++;

            m_anCollisionLighting[i] = cp.m_nLightingB;
            m_nContactSurface = cp.m_nSurfaceTypeB;

            switch (entity->GetType()) {
            case ENTITY_TYPE_VEHICLE:
            case ENTITY_TYPE_OBJECT: {
                CEntity::ChangeEntityReference(m_aGroundPhysicalPtrs[i], entity->AsPhysical());

                m_aGroundOffsets[i] = cp.m_vecPoint - entity->GetPosition();
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
    } else {
        tcd->m_nNumLines = NUM_SUSP_LINES;
    }

    if (numColPts > 0 || numProcessedLines > 0) {
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
    }

    return numColPts;
}

// 0x6B9250
void CBike::ProcessControl() {
    plugin::CallMethod<0x6B9250, CBike*>(this);
}

// 0x6B6740
void CBike::ResetSuspension() {
    for (auto i = 0; i < 2; i++) {
        m_aWheelPitchAngles[i] = 0.0f;
        m_WheelStates[i]       = WHEEL_STATE_NORMAL;
    }
    for (auto i = 0; i < 4; i++) {
        m_aWheelRatios[i] = 1.0f;
        m_WheelCounts[i]  = 0.0f;
    }
}

// 0x6B6790
bool CBike::GetAllWheelsOffGround() const {
    return m_nNoOfContactWheels == 0;
}

// 0x6B67A0
void CBike::DebugCode() {
    // NOP
}

// 0x6B6D40
void CBike::DoSoftGroundResistance(uint32& extraHandlingFlags) {
    const auto& up = GetUp();

    // Any wheel (that is touching something) on sand?
    const auto IsAnyWheelOnSurface = [&](auto&& pred) {
        for (auto i = 0; i < NUM_SUSP_LINES; i++) {
            if (m_aWheelRatios[i] < 1.0f && pred(m_aWheelColPoints[i].m_nSurfaceTypeB)) {
                return true;
            }
        }
        return false;
    };

    if (IsAnyWheelOnSurface([](auto surface) { return g_surfaceInfos.GetAdhesionGroup(surface) == ADHESION_GROUP_SAND; })) {
        // Speed component along the up vector
        const auto upSpeed = m_vecMoveSpeed.z * up.z + m_vecMoveSpeed.y * up.y + m_vecMoveSpeed.x * up.x;

        const float upX = upSpeed * up.x;
        const float upY = upSpeed * up.y;
        const float upZ = upSpeed * up.z;

        float vx = m_vecMoveSpeed.x - upX;
        float vy = m_vecMoveSpeed.y - upY;
        float vz = m_vecMoveSpeed.z - upZ;

        if (m_GasPedal > 0.3f) {
            if (vz * vz + vy * vy + vx * vx < 0.3f * 0.3f) {
                extraHandlingFlags += 4;
            }

            // Remove the forward component
            const auto& fwd = GetForward();
            const auto  fwdSpeed = vz * fwd.z + vy * fwd.y + vx * fwd.x;

            const float fwdX = fwdSpeed * fwd.x;
            const float fwdY = fwdSpeed * fwd.y;
            const float fwdZ = fwdSpeed * fwd.z;

            vx -= fwdX;
            vy -= fwdY;
            vz -= fwdZ;
        }

        const auto mult = -(CTimer::GetTimeStep() * m_fMass * 0.02f);
        ApplyMoveForce(CVector{ mult * vx, mult * vy, vz * mult });
        return;
    }

    if (IsAnyWheelOnSurface([](auto surface) { return surface == SURFACE_RAILTRACK; })) {
        const auto upSpeed = m_vecMoveSpeed.z * up.z + m_vecMoveSpeed.y * up.y + m_vecMoveSpeed.x * up.x;
        const auto mult    = -(CTimer::GetTimeStep() * m_fMass * 0.003f);
        ApplyMoveForce(CVector{
            (m_vecMoveSpeed.x - upSpeed * up.x) * mult,
            (m_vecMoveSpeed.y - upSpeed * up.y) * mult,
            (m_vecMoveSpeed.z - upSpeed * up.z) * mult
        });
    }
}

// 0x6B7130
void CBike::PlayHornIfNecessary() {
    if (m_autoPilot.carCtrlFlags.bHonkAtCar || m_autoPilot.carCtrlFlags.bHonkAtPed)
        PlayCarHorn();
}

// 0x6B7150
void CBike::CalculateLeanMatrix() {
    if (m_bLeanMatrixCalculated)
        return;

    CMatrix mat;
    mat.SetRotateX(fabs(m_RideAnimData.LeanAngle) * -0.05f);
    mat.RotateY(m_RideAnimData.LeanAngle);
    m_mLeanMatrix = GetMatrix();
    m_mLeanMatrix = m_mLeanMatrix * mat;
    // place wheel back on ground
    m_mLeanMatrix.GetPosition() += GetUp() * (1.0f - cos(m_RideAnimData.LeanAngle)) * GetColModel()->GetBoundingBox().m_vecMin.z;
    m_bLeanMatrixCalculated = true;
}

// 0x6B7F90
void CBike::FixHandsToBars(CPed* rider) {
    if (!m_nFixRightHand && !m_nFixLeftHand) {
        return;
    }

    const auto chassis = m_aBikeNodes[BIKE_CHASSIS];
    if (!chassis) {
        return;
    }

    CMatrix chassisMat{};
    chassisMat.Attach(RwFrameGetMatrix(chassis), false);

    CMatrix bikeMat{};
    bikeMat = GetMatrix();
    bikeMat *= chassisMat;

    const auto hier = GetAnimHierarchyFromSkinClump(rider->GetRpClump());

    const auto GetBonePos = [&](eBoneTag bone) -> RwV3d& {
        const auto idx = RpHAnimIDGetIndex(hier, bone);
        return RpHAnimHierarchyGetMatrixArray(hier)[idx].pos;
    };
    const auto MoveBone = [&](eBoneTag bone, float dx, float dy, float dz) {
        auto& pos = GetBonePos(bone);
        pos.x = dx + pos.x;
        pos.y = pos.y + dy;
        pos.z = dz + pos.z;
    };

    // Position of the hand on the handlebars
    CVector handPos = GetVehicleModelInfo()->GetModelDummyPosition(DUMMY_HAND_REST);
    if (handPos.x == 0.0f && handPos.y == 0.0f && handPos.z == 0.0f) {
        switch (m_nModelIndex) {
        case 0x1FE: handPos = CVector{ 0.25f, 0.29f, 0.525f }; break; // 0x8D3250
        case 0x1FD: handPos = CVector{ 0.25f, 0.21f, 0.69f };  break; // 0x8D325C
        default:    handPos = CVector{ 0.25f, 0.29f, 0.525f }; break; // 0x8D3244
        }
    } else {
        const auto handIdx = RpHAnimIDGetIndex(hier, BONE_R_HAND);
        CMatrix    handMat{ &RpHAnimHierarchyGetMatrixArray(hier)[handIdx], false };
        const auto offset  = InverseTransformVectorOriginal(bikeMat, TransformVectorOriginal(handMat, CVector{ -0.075f, -0.03f, 0.0f })); // 0x8D3274
        handPos.x = offset.x + handPos.x;
        handPos.y = offset.y + handPos.y;
        handPos.z = offset.z + handPos.z;
    }

    if (m_nFixRightHand) {
        const auto target = TransformPointOriginal(bikeMat, handPos);

        const auto& handBonePos = GetBonePos(BONE_R_HAND);
        const auto  dz = target.z - handBonePos.z;
        const auto  dy = target.y - handBonePos.y;
        const auto  dx = target.x - handBonePos.x;

        MoveBone(BONE_R_HAND,      dx, dy, dz);
        MoveBone(BONE_R_FINGER,    dx, dy, dz);
        MoveBone(BONE_R_FINGER_01, dx, dy, dz);
        MoveBone(BONE_R_FORE_ARM,  dx * 0.667f, dy * 0.667f, dz * 0.667f); // 0x871488
        MoveBone(BONE_R_UPPER_ARM, dx * 0.333f, dy * 0.333f, dz * 0.333f); // 0x864E30
        if (rider->IsPlayer()) {
            MoveBone(BONE_R_BREAST, dx * 0.333f, dy * 0.333f, dz * 0.333f);
        }
    }

    if (m_nFixLeftHand) {
        handPos.x = handPos.x * -1.0f;
        const auto target = TransformPointOriginal(bikeMat, handPos);

        const auto& handBonePos = GetBonePos(BONE_L_HAND);
        const auto  dz = target.z - handBonePos.z;
        const auto  dy = target.y - handBonePos.y;
        const auto  dx = target.x - handBonePos.x;

        MoveBone(BONE_L_HAND,      dx, dy, dz);
        MoveBone(BONE_L_FINGER,    dx, dy, dz);
        MoveBone(BONE_L_FINGER_01, dx, dy, dz);
        MoveBone(BONE_L_FORE_ARM,  dx * 0.75f, dy * 0.75f, dz * 0.75f); // 0x858F34
        MoveBone(BONE_L_UPPER_ARM, dx * 0.4f, dy * 0.4f, dz * 0.4f);    // 0x858EE8
        if (rider->IsPlayer()) {
            MoveBone(BONE_L_BREAST, dx * 0.4f, dy * 0.4f, dz * 0.4f);
        }
    }

    m_nFixLeftHand  = false;
    m_nFixRightHand = false;
}

// 0x6BEEB0
void CBike::PlaceOnRoadProperly() {
    const auto& bb = GetColModel()->GetBoundingBox();
    const float front = bb.m_vecMax.y;
    const float rear  = -bb.m_vecMin.y;

    const CVector fwd = GetForward();

    const float rearX  = GetPosition().x - fwd.x * rear;
    const float rearY  = GetPosition().y - fwd.y * rear;
    const float frontX = fwd.x * front + GetPosition().x;
    const float frontY = fwd.y * front + GetPosition().y;

    const auto ProbeGround = [&](float x, float y, CColPoint& colPoint, float& outZ, tColLighting& outLighting) {
        outZ = GetPosition().z - 5.0f;

        CEntity* entity{};
        if (CWorld::ProcessVerticalLine(CVector{ x, y, GetPosition().z + 5.0f }, outZ, colPoint, entity, true)) {
            outZ        = colPoint.m_vecPoint.z;
            outLighting = colPoint.m_nLightingB;

            m_pEntityWeAreOn    = entity;
            m_bTunnel           = entity->m_bTunnel;
            m_bTunnelTransition = entity->m_bTunnelTransition;
            return true;
        }
        outZ = GetPosition().z;
        return false;
    };

    CColPoint colPoint;

    float zFront;
    tColLighting lighting;
    if (ProbeGround(frontX, frontY, colPoint, zFront, lighting)) {
        m_FrontCollPoly.ligthing = lighting;
    }

    float zRear;
    if (ProbeGround(rearX, rearY, colPoint, zRear, lighting)) {
        m_RearCollPoly.ligthing = lighting;
    }

    const float length = rear + front;
    const float pitch  = std::atan2((zFront - zRear) / length, 1.0f);
    const float cosP   = std::cos(pitch);

    auto& right = GetRight();
    right.x = (frontY - rearY) / length;
    right.y = -((frontX - rearX) / length);
    right.z = 0.0f;

    GetForward() = CVector{ -(cosP * right.y), cosP * right.x, std::sin(pitch) };
    GetUp()      = CrossProduct(GetRight(), GetForward());

    const float midX = (frontX + rearX) * 0.5f;
    const float midY = (frontY + rearY) * 0.5f;
    const float midZ = (zRear + zFront) * 0.5f;

    GetPosition() = CVector{ midX, midY, GetHeightAboveRoad() + midZ };
}

// 0x6BF230
void CBike::GetCorrectedWorldDoorPosition(CVector& out, CVector arg1, CVector arg2) {
    const auto& fwd = GetForward();
    const auto& up  = GetUp();

    const auto cross1 = CrossProduct(fwd, CVector{ 0.0f, 0.0f, 1.0f });
    const auto cross2 = CrossProduct(cross1, fwd);

    const auto cross1DotUp = cross1.y * up.y + cross1.z * up.z + cross1.x * up.x;

    const auto& bb = GetColModel()->GetBoundingBox();
    float       boundsDiff = 0.0f;
    if (bb.m_vecMax.z > bb.m_vecMax.x) { // Not a typo, this compares X with Z
        boundsDiff = bb.m_vecMax.z - bb.m_vecMax.x;
    }

    const auto dy = arg2.y - arg1.y;
    out = CVector{};
    out += fwd * dy;

    const auto dx = arg2.x - arg1.x;
    out += cross1 * (boundsDiff * cross1DotUp + dx);

    const auto dz = arg2.z - arg1.z;
    out += cross2 * dz;

    out += GetPosition();
}

// 0x6BEA10
void CBike::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    GetMoveSpeed().z += 0.13f;
    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES);

    m_fHealth    = 0.0f;
    m_wBombTimer = 0;

    TheCamera.CamShake(0.4f, GetPosition());

    KillPedsInVehicle();

    m_nOverrideLights        = NO_CAR_LIGHT_OVERRIDE;
    vehicleFlags.bEngineOn   = false;
    vehicleFlags.bLightsOn   = false;

    ChangeLawEnforcerState(false);

    CExplosion::AddExplosion(this, damager, EXPLOSION_CAR, GetPosition(), 0, true, -1.0f, bHideExplosion);
    CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
}

// 0x6B7050
void CBike::Fix() {
    vehicleFlags.bIsDamaged = false;
    bikeFlags.bEngineOnFire = false;
    m_nWheelStatus[0] = 0;
    m_nWheelStatus[1] = 0;
}

// 0x6BD090
void CBike::PreRender() {
    CVehicle::PreRender();

    const auto mi = GetVehicleModelInfo();
    const auto cm = GetColModel();
    const auto cd = cm->m_pColData;

    //> Update the suspension heights
    if (vehicleFlags.bVehicleColProcessed) {
        DoBurstAndSoftGroundRatios();

        {
            const auto compression = 1.0f - m_fSuspensionLength[0] / m_fLineLength[0];
            const auto minRatio    = m_aWheelRatios[1] < m_aWheelRatios[0] ? m_aWheelRatios[1] : m_aWheelRatios[0];
            const auto t           = (minRatio - compression) / (1.0f - compression);
            auto       z           = cd->m_pLines[0].m_vecStart.z;
            if (t > 0.0f) {
                z = z - t * m_fSuspensionLength[0];
            }
            m_aWheelSuspensionHeights[0] = (z - m_aWheelSuspensionHeights[0]) * 0.75f + m_aWheelSuspensionHeights[0];
        }
        {
            const auto compression = 1.0f - m_fSuspensionLength[2] / m_fLineLength[2];
            const auto minRatio    = m_aWheelRatios[3] < m_aWheelRatios[2] ? m_aWheelRatios[3] : m_aWheelRatios[2];
            const auto t           = (minRatio - compression) / (1.0f - compression);
            auto       z           = cd->m_pLines[2].m_vecStart.z;
            if (t > 0.0f) {
                z = z - t * m_fSuspensionLength[2];
            }
            m_aWheelSuspensionHeights[1] = (z - m_aWheelSuspensionHeights[1]) * 0.75f + m_aWheelSuspensionHeights[1];
        }
    }

    //> Wheel particles
    switch (GetStatus()) {
    case STATUS_PHYSICS:
    case STATUS_PLAYER:
    case STATUS_PLAYER_PLAYBACK_FROM_BUFFER:
    case STATUS_SIMPLE: {
        const bool isRearWheelSkidding = m_WheelStates[1] == WHEEL_STATE_SKIDDING;
        const auto speed               = std::sqrt((m_vecMoveSpeed.x * m_vecMoveSpeed.x + m_vecMoveSpeed.y * m_vecMoveSpeed.y) + m_vecMoveSpeed.z * m_vecMoveSpeed.z);

        for (auto wheel = 0; wheel < 2; wheel++) {
            int32 colPtIdx;
            if (wheel == 0) {
                colPtIdx = 0;
                if (m_aRatioHistory[0] >= 1.0f && m_aRatioHistory[1] < 1.0f) {
                    colPtIdx = 1;
                }
            } else {
                colPtIdx = 3;
                if (m_aRatioHistory[3] >= 1.0f && m_aRatioHistory[2] < 1.0f) {
                    colPtIdx = 2;
                }
            }

            uint32 flags = 0;
            if (wheel == 0 && !isRearWheelSkidding) { // BUG?: Checks the REAR wheel state for the front wheel
                flags = 4;
            }

            auto&      colPt   = m_aWheelColPoints[colPtIdx];
            const auto offsetX = std::sin(m_RideAnimData.LeanAngle) * GetColModel()->m_boundBox.m_vecMin.z * 0.8f;
            CVector    pos     = colPt.m_vecPoint + GetRight() * offsetX;

            if (m_bWheelBloody[wheel]) {
                flags += 1;
            }
            if (m_bMoreSkidMarks[wheel]) {
                flags += 2;
            }

            AddSingleWheelParticles(
                m_WheelStates[wheel],
                m_nWheelStatus[wheel],
                m_aRatioHistory[colPtIdx],
                speed,
                &colPt,
                &pos,
                m_RideAnimData.LeanAngle <= 0.0f ? 1.0f : -1.0f,
                wheel,
                static_cast<uint32>(m_aWheelSkidmarkType[wheel]),
                &m_bWheelBloody[wheel],
                flags
            );
        }
        break;
    }
    default:
        break;
    }

    m_bLeanMatrixCalculated = false;
    CalculateLeanMatrix();

    //> Exhaust
    const auto fwdMoveSpeed = (GetForward().z * m_vecMoveSpeed.z + GetForward().y * m_vecMoveSpeed.y + GetForward().x * m_vecMoveSpeed.x) / s_BikeExhaustSpeedDivisor;
    if (vehicleFlags.bEngineOn && !m_pHandlingData->m_bNoExhaust && fwdMoveSpeed < 130.0f && !vehicleFlags.bIsDrowning) {
        AddExhaustParticles();
    }

    AddDamagedVehicleParticles();

    //> Police bike siren
    if (m_nModelIndex == MODEL_COPBIKE && vehicleFlags.bSirenOrAlarm && vehicleFlags.bEngineOn && !physicalFlags.bRenderScorched) {
        const auto coronaIntensity = static_cast<uint8>(static_cast<int32>(CTimeCycle::m_CurrentColours.m_fSpriteBrightness * 25.5f));
        const auto time            = CTimer::GetTimeInMS() & 0x1FF;
        const auto timeByte        = static_cast<uint8>(time);

        const CVector leftPos{ -0.28f, 0.6f, 0.3f };
        const CVector rightPos{ 0.28f, 0.6f, 0.3f };

        uint8 red;
        if (time < 0x100) {
            red = timeByte;
            CCoronas::RegisterCorona(
                reinterpret_cast<uintptr>(this) + 0x15, this,
                coronaIntensity, 0, 0, 0xFF,
                leftPos, 0.4f, 40.0f,
                eCoronaType::CORONATYPE_SHINYSTAR, eCoronaFlareType::FLARETYPE_NONE, eCoronaReflType::CORREFL_NONE, eCoronaLOSCheck::LOSCHECK_OFF, eCoronaTrail::TRAIL_OFF,
                0.0f, false,
                1.5f, false,
                30.0f, false, true
            );
        } else {
            red = static_cast<uint8>(-timeByte);
            CCoronas::RegisterCorona(
                reinterpret_cast<uintptr>(this) + 0x16, this,
                0, 0, coronaIntensity, 0xFF,
                rightPos, 0.4f, 40.0f,
                eCoronaType::CORONATYPE_SHINYSTAR, eCoronaFlareType::FLARETYPE_NONE, eCoronaReflType::CORREFL_NONE, eCoronaLOSCheck::LOSCHECK_OFF, eCoronaTrail::TRAIL_OFF,
                0.0f, false,
                1.5f, false,
                30.0f, false, true
            );
        }
        const auto blue = static_cast<uint8>(0xFF - red);

        const auto& up  = GetUp();
        const auto& pos = GetPosition();
        const CVector lightPos{
            (pos.x + GetForward().x) + up.x * 0.5f,
            (GetForward().y + pos.y) + up.y * 0.5f,
            (GetForward().z + pos.z) + up.z * 0.5f
        };
        CPointLights::AddLight(
            0,
            lightPos,
            CVector{},
            10.0f,
            static_cast<float>(red) * (1.0f / 1024.0f),
            0.0f,
            static_cast<float>(blue) * (1.0f / 1024.0f),
            0,
            true,
            nullptr
        );
    }

    DoVehicleLights(m_mLeanMatrix, VEHICLE_LIGHTS_IGNORE_DAMAGE);
    CShadows::StoreShadowForVehicle(this, VEH_SHD_BIKE);

    //> Wheel rotation
    const auto steerDir = TransformVectorOriginal(GetMatrix(), CVector{ -std::sin(m_fSteerAngle), std::cos(m_fSteerAngle), 0.0f });
    const CVector fwd   = GetForward();

    if (m_WheelCounts[0] > 0.0f || m_WheelCounts[1] > 0.0f) {
        const auto y        = (cd->m_pLines[1].m_vecStart.y + cd->m_pLines[0].m_vecStart.y) * 0.5f;
        const auto minRatio = m_aRatioHistory[0] < m_aRatioHistory[1] ? m_aRatioHistory[0] : m_aRatioHistory[1];
        const auto z        = (cd->m_pLines[0].m_vecStart.z - minRatio * m_fSuspensionLength[0]) - mi->m_fWheelSizeFront * 0.5f;
        const auto wheelVel = GetSpeed(CVector{ 0.0f, y, z });

        const auto rotation        = ProcessWheelRotation(WHEEL_STATE_NORMAL, steerDir, wheelVel, mi->m_fWheelSizeFront * 0.5f);
        m_aWheelAngularVelocity[0] = rotation;
        m_aWheelPitchAngles[0]     = rotation * CTimer::GetTimeStep() + m_aWheelPitchAngles[0];
    }
    if (m_WheelCounts[2] > 0.0f || m_WheelCounts[3] > 0.0f) {
        const auto y        = (cd->m_pLines[3].m_vecStart.y + cd->m_pLines[2].m_vecStart.y) * 0.5f;
        const auto minRatio = m_aRatioHistory[2] < m_aRatioHistory[3] ? m_aRatioHistory[2] : m_aRatioHistory[3];
        const auto z        = (cd->m_pLines[2].m_vecStart.z - minRatio * m_fSuspensionLength[2]) - mi->m_fWheelSizeRear * 0.5f;
        const auto wheelVel = GetSpeed(CVector{ 0.0f, y, z });

        const auto rotation        = ProcessWheelRotation(m_WheelStates[1], fwd, wheelVel, mi->m_fWheelSizeRear * 0.5f);
        m_aWheelAngularVelocity[1] = rotation;
        m_aWheelPitchAngles[1]     = rotation * CTimer::GetTimeStep() + m_aWheelPitchAngles[1];
    }

    //> Update the nodes
    CMatrix mat{};

    // Front forks (+ handlebars) turn with the steering
    if (const auto forkFront = m_aBikeNodes[BIKE_FORKS_FRONT]) {
        mat.Attach(RwFrameGetMatrix(forkFront), false);
        const CVector savedPos = mat.GetPosition();

        RwMatrix rwSteerMat{};
        CMatrix  steerMat{ &rwSteerMat, false };
        steerMat.SetUnity();
        steerMat.UpdateRW();

        const auto steerAngleRad = mi->m_fBikeSteerAngle * 0.017453292f;
        CVector    steerAxis{ 0.0f, std::sin(steerAngleRad), -std::cos(steerAngleRad) };
        steerAxis.Normalise();

        CQuaternion steerQuat{};
        steerQuat.Set(&steerAxis, -m_RideAnimData.BarSteerAngle);
        steerQuat.Get(&rwSteerMat);
        steerMat.Update();

        mat.SetUnity();
        mat = mat * steerMat;
        mat.GetPosition().x = mat.GetPosition().x + savedPos.x;
        mat.GetPosition().y = mat.GetPosition().y + savedPos.y;
        mat.GetPosition().z = mat.GetPosition().z + savedPos.z;
        mat.UpdateRW();

        if (const auto handlebars = m_aBikeNodes[BIKE_HANDLEBARS]) {
            mat.Attach(RwFrameGetMatrix(handlebars), false);
            const CVector hbPos = mat.GetPosition();
            if (GetStatus() == STATUS_ABANDONED || GetStatus() == STATUS_WRECKED) {
                mat.SetUnity();
                mat *= steerMat;
                mat.GetPosition().x = mat.GetPosition().x + hbPos.x;
                mat.GetPosition().y = mat.GetPosition().y + hbPos.y;
                mat.GetPosition().z = mat.GetPosition().z + hbPos.z;
            } else {
                mat.SetTranslate(hbPos);
            }
            mat.UpdateRW();
        }
    }

    // Rear forks (swing arm)
    if (const auto forkRear = m_aBikeNodes[BIKE_FORKS_REAR]) {
        const auto angle = std::asin((m_aWheelSuspensionHeights[1] - m_aWheelOrigHeights[1]) / m_fSwingArmLength) * -1.0f;
        mat.Attach(RwFrameGetMatrix(forkRear), false);
        const CVector savedPos = mat.GetPosition();
        mat.SetRotate(angle, 0.0f, 0.0f);
        mat.GetPosition().x = mat.GetPosition().x + savedPos.x;
        mat.GetPosition().y = mat.GetPosition().y + savedPos.y;
        mat.GetPosition().z = mat.GetPosition().z + savedPos.z;
        mat.UpdateRW();
    }

    // Front wheel + mudguard
    mat.Attach(RwFrameGetMatrix(m_aBikeNodes[BIKE_WHEEL_FRONT]), false);
    CVector wheelOffset;
    wheelOffset.x = mat.GetPosition().x;
    wheelOffset.z = m_aWheelSuspensionHeights[0] - m_fForkZOffset;
    wheelOffset.y = ((cd->m_pLines[1].m_vecStart.y + cd->m_pLines[0].m_vecStart.y) * 0.5f - m_fForkYOffset)
                  - (m_aWheelSuspensionHeights[0] - m_aWheelOrigHeights[0]) * m_fSteerAngleTan;
    if (m_nWheelStatus[0] == 1) {
        mat.SetRotate(m_aWheelPitchAngles[0], 0.0f, std::sin(m_aWheelPitchAngles[0]) * 0.05f);
    } else {
        mat.SetRotateX(m_aWheelPitchAngles[0]);
    }
    mat.GetPosition().x = mat.GetPosition().x + wheelOffset.x;
    mat.GetPosition().y = mat.GetPosition().y + wheelOffset.y;
    mat.GetPosition().z = mat.GetPosition().z + wheelOffset.z;
    mat.UpdateRW();

    mat.Attach(RwFrameGetMatrix(m_aBikeNodes[BIKE_MUDGUARD]), false);
    mat.GetPosition() = wheelOffset;
    mat.UpdateRW();

    // Rear wheel
    mat.Attach(RwFrameGetMatrix(m_aBikeNodes[BIKE_WHEEL_REAR]), false);
    {
        const CVector savedPos = mat.GetPosition();
        if (m_nWheelStatus[1] == 1) {
            mat.SetRotate(m_aWheelPitchAngles[1], 0.0f, std::sin(m_aWheelPitchAngles[1]) * 0.07f);
        } else {
            mat.SetRotateX(m_aWheelPitchAngles[1]);
        }
        mat.GetPosition().x = mat.GetPosition().x + savedPos.x;
        mat.GetPosition().y = mat.GetPosition().y + savedPos.y;
        mat.GetPosition().z = mat.GetPosition().z + savedPos.z;
        mat.UpdateRW();
    }

    // Chassis leans
    if (const auto chassis = m_aBikeNodes[BIKE_CHASSIS]) {
        mat.Attach(RwFrameGetMatrix(chassis), false);
        CVector offset = mat.GetPosition();
        offset.z = (1.0f - std::cos(m_RideAnimData.LeanAngle)) * cm->m_boundBox.m_vecMin.z * 0.9f;
        mat.SetRotateX(std::abs(m_RideAnimData.LeanAngle) * -0.05f);
        mat.RotateY(m_RideAnimData.LeanAngle);
        mat.GetPosition().x = mat.GetPosition().x + offset.x;
        mat.GetPosition().y = mat.GetPosition().y + offset.y;
        mat.GetPosition().z = mat.GetPosition().z + offset.z;
        mat.UpdateRW();
    }
}

// 0x6BDE20
void CBike::Render() {
    auto savedRef = 0;
    RwRenderStateGet(rwRENDERSTATEALPHATESTFUNCTIONREF, &savedRef);
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(1));

    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    CVehicle::Render();

    if (m_renderLights.m_bRightFront) {
        CalculateLeanMatrix();
        CVehicle::DoHeadLightBeam(eVehicleLightId::MAIN, m_mLeanMatrix, true);
    }

    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(savedRef));
}

// 0x6BCFC0
void CBike::Teleport(CVector destination, bool resetRotation) {
    CWorld::Remove(this);

    GetPosition() = destination;
    if (resetRotation)
        SetOrientation(0.0f, 0.0f, 0.0f);

    ResetMoveSpeed();
    ResetTurnSpeed();
    ResetSuspension();

    CWorld::Add(this);
}

// 0x6B8EC0
void CBike::VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    // NOTE: All params except `damageIntensity` are unused, the physical's collision data is used instead.
    if (damageIntensity > 0.0f) {
        return;
    }
    if (m_fDamageIntensity < 1.0f || !vehicleFlags.bCanBeDamaged) {
        return;
    }

    auto dmg = m_fDamageIntensity;
    if (GetStatus() == STATUS_PLAYER && CStats::GetPercentageProgress() >= 100.0f) {
        dmg *= 0.5f;
    }

    if (bikeFlags.bOnSideStand && dmg > 20.0f) {
        bikeFlags.bOnSideStand = false;
    }

    DamageKnockOffRider(this, m_fDamageIntensity, m_nPieceType, m_pDamageEntity, m_vecLastCollisionPosn, m_vecLastCollisionImpactVelocity);

    if (m_pDamageEntity && m_pDamageEntity->GetIsTypeVehicle()) {
        m_nLastWeaponDamageType = WEAPON_RAMMEDBYCAR;
        m_pLastDamageEntity     = m_pDamageEntity;
        m_pLastDamageEntity->RegisterReference(&m_pLastDamageEntity);
    }

    if (physicalFlags.bCollisionProof) {
        return;
    }

    // Hitting a building from the front (relative to up) doesn't hurt the bike
    if (m_pDamageEntity && m_pDamageEntity->GetIsTypeBuilding() && DotProduct(m_vecLastCollisionImpactVelocity, GetUp()) > 0.6f) {
        return;
    }

    if (dmg > 25.0f && GetStatus() != STATUS_WRECKED) {
        // Cops hunting the player get angry if the player rams them
        if (vehicleFlags.bIsLawEnforcer) {
            const auto plyrVeh = FindPlayerVehicle();
            if (plyrVeh && m_pDamageEntity == plyrVeh && GetStatus() != STATUS_ABANDONED) {
                const auto thisSpeed = m_vecMoveSpeed.Magnitude();
                const auto plyrSpeed = plyrVeh->m_vecMoveSpeed.Magnitude();
                if (plyrSpeed >= thisSpeed && FindPlayerVehicle()->m_vecMoveSpeed.Magnitude() > 0.1f) {
                    FindPlayerPed()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1);
                }
            }
        }

        auto healthDmg = (dmg - 25.0f) * m_pHandlingData->m_fCollisionDamageMultiplier;
        if (healthDmg > 0.0f) {
            // Make our driver say something to the one who hit us
            if (healthDmg > 5.0f && m_pDriver && m_pDamageEntity && m_pDamageEntity->GetIsTypeVehicle()) {
                const auto damagerVeh = m_pDamageEntity->AsVehicle();
                if (FindPlayerVehicle() != this || damagerVeh->m_nCreatedBy != MISSION_VEHICLE) {
                    if (damagerVeh->m_pDriver) {
                        m_pDriver->Say(CTX_GLOBAL_CRASH_BIKE);
                    }
                }
            }

            const auto prevHealth = m_fHealth;
            if (FindPlayerVehicle() == this) {
                healthDmg *= vehicleFlags.bTakeLessDamage ? 1.0f / 6.0f : 0.5f;
            } else if (vehicleFlags.bTakeLessDamage) {
                healthDmg *= 1.0f / 12.0f;
            } else if (m_pDamageEntity && m_pDamageEntity == FindPlayerVehicle()) {
                healthDmg *= 2.0f / 3.0f;
            } else {
                healthDmg *= 0.25f;
            }

            m_fHealth -= healthDmg;
            if (m_fHealth <= 1.0f && prevHealth > 1.0f) {
                m_fHealth = 1.0f;
            }
        }
    }

    if (m_fHealth < 250.0f && !bikeFlags.bEngineOnFire) {
        bikeFlags.bEngineOnFire = true;
        m_BlowUpTimer           = 0.0f;
        m_Damager               = m_pDamageEntity;
        if (m_Damager) {
            m_Damager->RegisterReference(&m_Damager);
        }
    }
}

// 0x6B89B0
void CBike::SetupSuspensionLines() {
    constexpr auto UNSET_MARKER = 99'999.99f; // 0x47C34FFF

    const auto mi = GetVehicleModelInfo();
    const auto cm = mi->GetColModel();
    const auto cd = cm->m_pColData;
    const auto& handling = *m_pHandlingData;
    auto* const lines = cd->m_pLines;

    const bool linesJustAllocated = lines[1].m_vecStart.z == UNSET_MARKER;
    const bool linesAreSetup      = lines[0].m_vecStart.z != UNSET_MARKER;

    const auto clumpFrame = RpClumpGetFrame(GetRpClump());

    // Position of the node relative to `stopAt` (exclusive, but only checked after the first parent)
    const auto GetNodePosition = [](RwFrame* node, RwFrame* stopAt) {
        RwMatrix matrix = *RwFrameGetMatrix(node);
        for (auto parent = RwFrameGetParent(node); parent;) {
            RwMatrixTransform(&matrix, RwFrameGetMatrix(parent), rwCOMBINEPOSTCONCAT);
            parent = RwFrameGetParent(parent);
            if (parent == stopAt) {
                break;
            }
        }
        return CVector{ matrix.pos };
    };

    for (auto i = 0; i < NUM_SUSP_LINES; i++) {
        CVector start{};
        float   offsetY{};
        if (linesAreSetup) {
            start   = lines[i].m_vecStart;
            start.z = m_aWheelOrigHeights[i < 2 ? 0 : 1];
        } else {
            RwFrame* node{};
            switch (i) {
            case 0:
                node    = m_aBikeNodes[BIKE_WHEEL_FRONT];
                offsetY = mi->m_fWheelSizeFront * 0.25f;
                break;
            case 1:
                node    = m_aBikeNodes[BIKE_WHEEL_FRONT];
                offsetY = mi->m_fWheelSizeFront;
                offsetY *= -0.25f;
                break;
            case 2:
                node    = m_aBikeNodes[BIKE_WHEEL_REAR];
                offsetY = mi->m_fWheelSizeRear * 0.25f;
                break;
            case 3:
                node    = m_aBikeNodes[BIKE_WHEEL_REAR];
                offsetY = mi->m_fWheelSizeRear;
                offsetY *= -0.25f;
                break;
            }

            start = GetNodePosition(node, clumpFrame);
            if (i == 0) {
                m_aWheelOrigHeights[0] = start.z;
            } else if (i == 2) {
                m_aWheelOrigHeights[1] = start.z;
                if (const auto forkRear = m_aBikeNodes[BIKE_FORKS_REAR]) {
                    const auto forkPos = GetNodePosition(forkRear, clumpFrame);
                    const auto dy = start.y - forkPos.y;
                    const auto dz = start.z - forkPos.z;
                    m_fSwingArmLength = std::sqrt(dz * dz + dy * dy);
                } else {
                    m_fSwingArmLength = 0.0f;
                }
            }
            start.y += offsetY;
        }

        start.z += handling.m_fSuspensionUpperLimit;

        const auto wheelSize = i <= 1 ? mi->m_fWheelSizeFront : mi->m_fWheelSizeRear;

        lines[i].m_vecStart = start;
        lines[i].m_vecEnd   = CVector{ start.x, start.y, ((handling.m_fSuspensionLowerLimit - handling.m_fSuspensionUpperLimit) - wheelSize * 0.5f) + start.z };

        m_fSuspensionLength[i] = handling.m_fSuspensionUpperLimit - handling.m_fSuspensionLowerLimit;
        m_fLineLength[i]       = lines[i].m_vecStart.z - lines[i].m_vecEnd.z;
    }

    if (!linesAreSetup) {
        // BUG: Unlike for the wheels, the loop stops at the node itself (never true), so it walks all the way up to the root
        const auto forkFront = m_aBikeNodes[BIKE_FORKS_FRONT];
        const auto forkPos   = GetNodePosition(forkFront, forkFront);
        m_fForkYOffset = forkPos.y;
        m_fForkZOffset = forkPos.z;
    }

    m_fHeightAboveRoad = (mi->m_fWheelSizeFront * 0.5f - lines[0].m_vecStart.z)
                       + (1.0f - 1.0f / (handling.m_fSuspensionForceLevel * 4.0f)) * m_fSuspensionLength[0];

    for (auto i = 0; i < 2; i++) {
        m_aWheelSuspensionHeights[i] = (i == 0 ? mi->m_fWheelSizeFront : mi->m_fWheelSizeRear) * 0.5f - m_fHeightAboveRoad;
    }

    // Make sure the bounding box/sphere contains the suspension lines
    auto& bb = cm->m_boundBox;
    if (lines[0].m_vecEnd.z < bb.m_vecMin.z) {
        bb.m_vecMin.z = lines[0].m_vecEnd.z;
    }

    const auto minLen = std::sqrt((bb.m_vecMin.x * bb.m_vecMin.x + bb.m_vecMin.y * bb.m_vecMin.y) + bb.m_vecMin.z * bb.m_vecMin.z);
    const auto maxLen = std::sqrt((bb.m_vecMax.x * bb.m_vecMax.x + bb.m_vecMax.y * bb.m_vecMax.y) + bb.m_vecMax.z * bb.m_vecMax.z);
    const auto radius = minLen <= maxLen ? maxLen : minLen;
    if (cm->m_boundSphere.m_fRadius < radius) {
        cm->m_boundSphere.m_fRadius = radius;
    }

    // Lift spheres which are below the ground clearance
    if (handling.m_bForceGroundClearance && linesJustAllocated) {
        const auto clearance = 0.25f - m_fHeightAboveRoad;
        for (auto i = 0; i < cd->m_nNumSpheres; i++) {
            auto& sphere = cd->m_pSpheres[i];
            if (sphere.m_vecCenter.z - sphere.m_fRadius < clearance) {
                if (sphere.m_fRadius > 0.4f) {
                    sphere.m_fRadius = std::max(0.4f, sphere.m_vecCenter.z - clearance);
                }
                sphere.m_vecCenter.z = clearance + sphere.m_fRadius;
            }
        }
    }
}

// 0x6B8970
void CBike::SetModelIndex(uint32 index) {
    CVehicle::SetModelIndex(index);
    SetupModelNodes();
}

// 0x6B5960
void CBike::SetupModelNodes() {
    std::ranges::fill(m_aBikeNodes, nullptr);
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aBikeNodes.data());
}

// 0x6B7080
void CBike::PlayCarHorn() {
    if (!CanUpdateHornCounter() || m_HornCounter) {
        return;
    }

    if (m_nCarHornTimer) {
        m_nCarHornTimer -= 1;
        return;
    }

    // Original stores `(rand() & 0x7F) + 0x96` into a byte; the `& 7` below is done on the unsigned value
    m_nCarHornTimer = (char)(uint8)((CGeneral::GetRandomNumber() & 0x7F) + 0x96);
    const auto pattern = (uint8)m_nCarHornTimer & 7;
    if (pattern >= 4) {
        if (m_pDriver) {
            m_pDriver->Say(CTX_GLOBAL_BLOCKED);
        }
        return; // NOTE: horn counter is not set in this case
    }
    if (pattern >= 2) {
        if (m_pDriver && m_autoPilot.carCtrlFlags.bHonkAtCar) {
            m_pDriver->Say(CTX_GLOBAL_BLOCKED);
        }
    }
    m_HornCounter = 45;
}

// 0x6B7070
void CBike::SetupDamageAfterLoad() {
    // NOP
}

// 0x6B6950
void CBike::DoBurstAndSoftGroundRatios() {
    const auto mi = GetVehicleModelInfo();

    std::array<bool, NUM_SUSP_LINES> process{ true, true, true, true };

    const auto fwdSpeed = std::abs(m_vecMoveSpeed.z * GetForward().z + m_vecMoveSpeed.y * GetForward().y + m_vecMoveSpeed.x * GetForward().x);

    const auto CompressionLeft = [&](int32 line) { // NOTSA
        return (m_fLineLength[line] - m_fSuspensionLength[line]) / m_fLineLength[line];
    };

    auto susp0 = 0, susp1 = 1; // Suspension lines of the current wheel (front: 0, 1 | rear: 2, 3)
    for (auto wheel = 0; wheel < 2; wheel++) {
        if (wheel == 1) {
            susp0 = 2;
            susp1 = 3;
        }

        switch (m_nWheelStatus[wheel]) {
        case 2: { // Missing
            m_aWheelRatios[susp0] = 1.0f;
            m_aWheelRatios[susp1] = 1.0f;
            process[susp0] = process[susp1] = false;
            break;
        }
        case 1: { // Burst
            // NOTSA: original also computed an unused `float(rand() & 0xFFFF) * 2^-15` and ftol'd `fwdSpeed * 40`
            const auto rnd = static_cast<float>(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f);
            const auto maxRnd = static_cast<float>(static_cast<uint16>(static_cast<int32>(fwdSpeed * 40.0f)) + 98);
            if (static_cast<int32>(rnd * maxRnd) < 100) {
                const auto change = CompressionLeft(susp0) * 0.2f;

                const auto r0 = change + m_aWheelRatios[susp0];
                m_aWheelRatios[susp0] = r0 < 1.0f ? r0 : 1.0f;

                const auto r1 = change + m_aWheelRatios[susp1];
                m_aWheelRatios[susp1] = r1 < 1.0f ? r1 : 1.0f;
            }
            process[susp0] = process[susp1] = false;
            break;
        }
        default: {
            const auto IsOnRailtrack = [&](int32 line) {
                return m_aWheelRatios[line] < 1.0f && m_aWheelColPoints[line].m_nSurfaceTypeB == SURFACE_RAILTRACK;
            };
            if (!IsOnRailtrack(susp0) && !IsOnRailtrack(susp1)) {
                break; // Doesn't mark the lines as processed
            }

            auto wheelSizeFactor = 1.5f / ((wheel == 0 ? mi->m_fWheelSizeFront : mi->m_fWheelSizeRear) * 0.5f);
            if (fwdSpeed > 0.3f) {
                wheelSizeFactor = (fwdSpeed / 0.3f) * wheelSizeFactor;
            }
            const float wheelSizeInv = 1.0f / wheelSizeFactor;

            const float rotation     = wheelSizeInv * m_aWheelPitchAngles[wheel];
            const float rotationFrac = rotation - static_cast<float>(std::floor(static_cast<double>(rotation)));

            const float nextRotation     = (CTimer::GetTimeStep() * m_aWheelAngularVelocity[wheel] + m_aWheelPitchAngles[wheel]) * wheelSizeInv;
            const float nextRotationFrac = nextRotation - static_cast<float>(std::floor(static_cast<double>(nextRotation)));

            if (   (m_aWheelAngularVelocity[wheel] > 0.0f && nextRotationFrac < rotationFrac)
                || (m_aWheelAngularVelocity[wheel] < 0.0f && nextRotationFrac > rotationFrac)
            ) {
                const auto change = CompressionLeft(susp0) * 0.3f;

                const auto r0 = m_aWheelRatios[susp0] - change;
                m_aWheelRatios[susp0] = r0 > 0.2f ? r0 : 0.2f;

                const auto r1 = m_aWheelRatios[susp1] - change;
                m_aWheelRatios[susp1] = r1 > 0.2f ? r1 : 0.2f;
            }
            process[susp0] = process[susp1] = false;
            break;
        }
        }
    }

    // Soft ground (sand)
    for (auto i = 0; i < NUM_SUSP_LINES; i++) {
        if (!process[i] || m_aWheelRatios[i] >= 1.0f) {
            continue;
        }
        if (g_surfaceInfos.GetAdhesionGroup(m_aWheelColPoints[i].m_nSurfaceTypeB) != ADHESION_GROUP_SAND || ModelIndices::IsRhino(m_nModelIndex)) {
            continue;
        }

        float offroadFactor = 0.25f;
        if (handlingFlags.bOffroadAbility2) {
            offroadFactor = 0.1f;
        } else if (handlingFlags.bOffroadAbility) {
            offroadFactor = 0.15f;
        }

        auto adhesionFactor = (1.0f - (fwdSpeed / 0.3f) * 0.7f) - CWeather::WetRoads * 0.7f;
        adhesionFactor      = adhesionFactor < 0.4f ? 0.4f : adhesionFactor;

        const auto ratio = offroadFactor * (CompressionLeft(i) * adhesionFactor) + m_aWheelRatios[i];
        m_aWheelRatios[i] = ratio > 1.0f ? 1.0f : ratio;
    }
}

// 0x6B67E0
bool CBike::SetUpWheelColModel(CColModel* wheelCol) {
    const auto mi  = GetVehicleModelInfo();
    const auto chassis = m_aBikeNodes[BIKE_CHASSIS];
    const auto wcd = wheelCol->m_pColData;
    const auto cm  = GetColModel();

    wheelCol->m_boundSphere = cm->m_boundSphere;
    wheelCol->m_boundBox    = cm->m_boundBox;

    // Position of the wheel node relative to the chassis
    const auto GetWheelPosition = [&](RwFrame* wheel) {
        RwMatrix matrix = *RwFrameGetMatrix(wheel);
        for (auto parent = RwFrameGetParent(wheel); parent;) {
            RwMatrixTransform(&matrix, RwFrameGetMatrix(parent), rwCOMBINEPOSTCONCAT);
            parent = RwFrameGetParent(parent);
            if (parent == chassis) {
                break;
            }
        }
        return CVector{ matrix.pos };
    };

    wcd->m_pSpheres[0].Set(mi->m_fWheelSizeFront * 0.5f, GetWheelPosition(m_aBikeNodes[BIKE_WHEEL_FRONT]), SURFACE_RUBBER, 0xD);
    wcd->m_pSpheres[1].Set(mi->m_fWheelSizeRear * 0.5f, GetWheelPosition(m_aBikeNodes[BIKE_WHEEL_REAR]), SURFACE_RUBBER, 0xF);
    wcd->m_nNumSpheres = 2;

    return true;
}

// 0x6B67B0
void CBike::RemoveRefsToVehicle(CEntity* entityToRemove) {
    for (auto& entity: m_aGroundPhysicalPtrs) {
        if (entity == entityToRemove)
            entity = nullptr;
    }
}

// 0x6B6620
void CBike::ProcessControlCollisionCheck(bool applySpeed) {
    const CMatrix oldMat = GetMatrix();
    SetIsStuck(false);
    SkipPhysics();
    physicalFlags.bSkipLineCol     = false;
    physicalFlags.bProcessingShift = false;
    m_fMovingSpeed                 = 0.0f;
    rng::fill(m_aWheelRatios, 1.0f);

    if (applySpeed) {
        ApplyMoveSpeed();
        ApplyTurnSpeed();

        for (auto i = 0; CheckCollision() && i < 5; i++) {
            GetMatrix() = oldMat;
            ApplyMoveSpeed();
            ApplyTurnSpeed();
        }
    } else {
        const auto usesCollision = GetUsesCollision();
        SetUsesCollision(false);
        CheckCollision();
        SetUsesCollision(usesCollision);
    }

    SetIsStuck(false);
    SetIsInSafePosition(true);
}

// 0x6B5990
void CBike::GetComponentWorldPosition(int32 componentId, CVector& outPos) {
    if (IsComponentPresent(componentId))
        outPos = RwFrameGetLTM(m_aBikeNodes[componentId])->pos;
    else
        NOTSA_LOG_DEBUG("BikeNode missing: model={}, nodeIdx={}", m_nModelIndex, componentId);
}

// 0x6B58D0
void CBike::ProcessOpenDoor(CPed* ped, uint32 doorComponentId, uint32 animGroup, uint32 animId, float fTime) {
    // NOP
}

