/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Bike.h"

#include "Buoyancy.h"



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
    RH_ScopedVMTInstall(ProcessAI, 0x6BC930, { .Reversed = false });
    RH_ScopedInstall(ProcessBuoyancy, 0x6B5FB0);
    RH_ScopedInstall(ResetSuspension, 0x6B6740);
    RH_ScopedInstall(GetAllWheelsOffGround, 0x6B6790);
    RH_ScopedInstall(DebugCode, 0x6B67A0);
    RH_ScopedInstall(DoSoftGroundResistance, 0x6B6D40);
    RH_ScopedInstall(PlayHornIfNecessary, 0x6B7130);
    RH_ScopedInstall(CalculateLeanMatrix, 0x6B7150);
    RH_ScopedInstall(ProcessRiderAnims, 0x6B7280, { .Reversed = false });
    RH_ScopedInstall(FixHandsToBars, 0x6B7F90, { .Reversed = false });
    RH_ScopedInstall(PlaceOnRoadProperly, 0x6BEEB0);
    RH_ScopedInstall(GetCorrectedWorldDoorPosition, 0x6BF230);
    RH_ScopedVMTInstall(Fix, 0x6B7050);
    RH_ScopedVMTInstall(BlowUpCar, 0x6BEA10);
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6BF400);
    RH_ScopedVMTInstall(BurstTyre, 0x6BEB20);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6BE310, { .Reversed = false });
    RH_ScopedVMTInstall(ProcessEntityCollision, 0x6BDEA0);
    RH_ScopedVMTInstall(Render, 0x6BDE20);
    RH_ScopedVMTInstall(PreRender, 0x6BD090, { .Reversed = false });
    RH_ScopedVMTInstall(Teleport, 0x6BCFC0);
    RH_ScopedVMTInstall(ProcessControl, 0x6B9250, { .Reversed = false });
    RH_ScopedVMTInstall(VehicleDamage, 0x6B8EC0, { .Reversed = false });
    RH_ScopedVMTInstall(SetupSuspensionLines, 0x6B89B0, { .Reversed = false });
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
        mi->m_pColModel->m_pColData->m_pLines[1].m_vecStart.x = 99'999.99f; // todo: explain this
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
        assoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
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
    return plugin::CallMethodAndReturn<bool, 0x6BC930, CBike*, uint32&>(this, extraHandlingFlags);
}

// 0x6BF400
void CBike::ProcessDrivingAnims(CPed* driver, bool blend) {
    if (m_bOffscreen && GetStatus() == STATUS_PLAYER)
        return;

    ProcessRiderAnims(driver, this, &m_RideAnimData, m_BikeHandling, 0);
}

// 0x6B7280
void CBike::ProcessRiderAnims(CPed* rider, CVehicle* vehicle, CRideAnimData* rideData, tBikeHandlingData* handling, int16 a5) {
    plugin::Call<0x6B7280, CPed*, CVehicle*, CRideAnimData*, tBikeHandlingData*, int16>(rider, vehicle, rideData, handling, a5);
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
    plugin::CallMethod<0x6BE310, CBike*, uint8>(this, playerNum);
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
    ((void(__thiscall*)(CBike*, CPed*))0x6B7F90)(this, rider);
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
    plugin::CallMethod<0x6BD090, CBike*>(this);
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
    plugin::CallMethod<0x6B8EC0, CBike*, float, eVehicleCollisionComponent, CEntity*, CVector*, CVector*, eWeaponType>(this, damageIntensity, component, damager, vecCollisionCoors, vecCollisionDirection, weapon);
}

// 0x6B89B0
void CBike::SetupSuspensionLines() {
    plugin::CallMethod<0x6B89B0, CBike*>(this);
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

