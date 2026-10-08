#include "StdInc.h"

#include <bit>
#include <numbers>

namespace {
// The original game uses the exact (float rounded) values, but the ones in `common.h` are rounded to 5 decimals
constexpr float BMX_PI      = std::numbers::pi_v<float>;       // 0x871504
constexpr float BMX_HALF_PI = std::numbers::pi_v<float> / 2.f; // 0x87150C
constexpr float BMX_TWO_PI  = std::numbers::pi_v<float> * 2.f; // 0x858CBC
}

void CBmx::InjectHooks() {
    RH_ScopedVirtualClass(CBmx, 0x871528, 67);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6BF820);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6BF9B0);
    RH_ScopedVMTInstall(BurstTyre, 0x6BF9C0);
    RH_ScopedVMTInstall(FindWheelWidth, 0x6C0550);
    RH_ScopedVMTInstall(ProcessControl, 0x6BFA30);
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6BFB50);
    RH_ScopedVMTInstall(PreRender, 0x6C0810);
    RH_ScopedVMTInstall(ProcessAI, 0x6C1470);
    RH_ScopedInstall(ProcessBunnyHop, 0x6C0590);
    RH_ScopedInstall(LaunchBunnyHopCB, 0x6C0390);
}

// 0x6BF820
CBmx::CBmx(int32 modelIndex, eVehicleCreatedBy createdBy) :
    CBike(modelIndex, createdBy) 
{
    auto mi                     = CModelInfo::GetModelInfo(modelIndex);
    m_nVehicleSubType           = VEHICLE_TYPE_BMX;
    m_RideAnimData.AnimGroup = CAnimManager::GetAnimBlocks()[mi->GetAnimFileIndex()].GroupId;
    if (m_RideAnimData.AnimGroup < ANIM_GROUP_BMX || m_RideAnimData.AnimGroup > ANIM_GROUP_CHOPPA) {
        m_RideAnimData.AnimGroup = ANIM_GROUP_BMX;
    }

    m_fControlJump     = 0.0f;
    m_fControlPedaling = 0.0f;
    m_fSprintLeanAngle = 0.0f;
    m_fCrankAngle      = 0.0f;
    m_fPedalAngleL     = 0.0f;
    m_fPedalAngleR     = 0.0f;
    m_nFixLeftHand     = false;
    m_nFixRightHand    = false;
    m_bIsFreewheeling  = false;

    const auto Calc = [&](eBmxNodes node) -> float {
        RwMatrix matrix;
        RwFrame* wheelFront = m_aBikeNodes[node];
        matrix              = *RwFrameGetMatrix(wheelFront);

        auto parent = RwFrameGetParent(wheelFront);
        if (parent) {
            do {
                RwMatrixTransform(&matrix, RwFrameGetMatrix(parent), rwCOMBINEPOSTCONCAT);
                parent = RwFrameGetParent(parent);
            } while (parent != wheelFront && parent);
        }
        return matrix.pos.y;
    };
    auto wheelFrontPosY = Calc(BMX_WHEEL_FRONT);
    auto wheelRearPosY  = Calc(BMX_WHEEL_REAR);

    m_fMidWheelDistY = wheelFrontPosY - wheelRearPosY;
    m_fMidWheelFracY = wheelFrontPosY / m_fMidWheelDistY;
}

// 0x6BF9D0
CBmx::~CBmx() {
    m_vehicleAudio.Terminate();
}

// 0x6BF9B0
bool CBmx::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6BF9C0
bool CBmx::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6BFA30
void CBmx::ProcessControl() {
    const float BMX_SPRINT_LEANSTART = FRAC_PI_2;
    const float BMX_PEDAL_LEANSTART  = 0.0f;
    const float BMX_SPRINT_LEANMULT  = 0.3f;
    const float MTB_SPRINT_LEANMULT  = 0.087f;
    const float BMX_PEDAL_LEANMULT   = 0.07f;
    const float MTB_PEDAL_LEANMULT   = 0.02f;

    CBike::ProcessControl();

    if (GetWasPostponed() || GetStatus() != STATUS_PLAYER || !m_pDriver) {
        return;
    }

    auto animBikeSprint = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_SPRINT);
    bool isMountainBike = GetModelId() == MODEL_MTBIKE;

    if (animBikeSprint && animBikeSprint->GetBlendAmount() > 0.01f) {
        float mult         = isMountainBike ? MTB_SPRINT_LEANMULT : BMX_SPRINT_LEANMULT;
        m_fSprintLeanAngle = std::sin(animBikeSprint->GetCurrentTime() / animBikeSprint->GetHier()->GetTotalTime() * TWO_PI + BMX_SPRINT_LEANSTART) * animBikeSprint->GetBlendAmount() * mult;
    } else {
        auto animBikePedal = RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_PEDAL);
        if (animBikePedal && animBikePedal->GetBlendAmount() > 0.01f) {
            float mult = isMountainBike ? MTB_PEDAL_LEANMULT : BMX_PEDAL_LEANMULT;
            GetRideAnimData()->LeanAngle += std::sin(animBikePedal->GetCurrentTime() / animBikePedal->GetHier()->GetTotalTime() * TWO_PI + BMX_PEDAL_LEANSTART) * animBikePedal->GetBlendAmount() * mult;
        }
        m_fSprintLeanAngle *= 0.95f;
    }
}

// 0x6BFB50
void CBmx::ProcessDrivingAnims(CPed* driver, bool blend) {
    // NOTE: `m_fControlPedaling` is NOT always 0 (as noted in the header), it's set from the player's controls
    if (m_bOffscreen) {
        if (!driver || !driver->IsPlayer()) {
            return;
        }
    }

    m_nFixLeftHand  = true;
    m_nFixRightHand = true;

    const auto clump = driver->GetRpClump();

    //> Bunny hopping: Just blend the crank angle towards 90 degrees
    if (const auto bunnyHopAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_BUNNYHOP)) {
        m_fCrankAngle = (1.0f - bunnyHopAnim->m_BlendAmount) * m_fCrankAngle + BMX_HALF_PI * bunnyHopAnim->m_BlendAmount;
        m_fPedalAngleL = -m_fCrankAngle;
        m_fPedalAngleR = -m_fCrankAngle;
        return;
    }

    if (driver->GetPlayerData()) {
        driver->SetMoveState(PEDMOVE_NONE);
    }

    const auto& fwd      = GetMatrix().GetForward();
    const auto  fwdSpeed = (m_vecMoveSpeed.z * fwd.z + m_vecMoveSpeed.y * fwd.y) + m_vecMoveSpeed.x * fwd.x;

    auto pedalAnim  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_PEDAL);
    auto sprintAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_SPRINT);
    const auto leftAnim  = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_LEFT);
    const auto rightAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_RIGHT);
    const auto fwdAnim   = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_FWD);
    auto drivebyAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYLHS);
    if (!drivebyAnim) {
        drivebyAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYRHS);
        if (!drivebyAnim) {
            drivebyAnim = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYFT);
        }
    }

    const auto NeedsNewBlend = [](const CAnimBlendAssociation* anim) {
        return !anim || (anim->m_BlendAmount < 1.0f && anim->m_BlendDelta <= 0.0f);
    };
    const auto GetCrankAngleFromAnim = [](const CAnimBlendAssociation* anim) { // 0 - (time / totalTime) * 2PI
        return 0.0f - (anim->m_CurrentTime / anim->m_BlendHier->GetTotalTime()) * BMX_TWO_PI;
    };
    // Blend the crank angle towards `target` by the blend amount of `anim`
    const auto BlendCrankTowards = [&](const CAnimBlendAssociation* anim, float target) {
        return (1.0f - anim->m_BlendAmount) * m_fCrankAngle + target * anim->m_BlendAmount;
    };

    const float leanThreshold = m_fControlPedaling > 5.0f
        ? 0.7f
        : 0.4f;
    const bool isRiderActive = leanThreshold <= std::abs(m_RideAnimData.LeanAngle)
        || leanThreshold <= m_RideAnimData.LeanFwd
        || fwdSpeed <= 0.01f
        || drivebyAnim;

    if (isRiderActive) {
        //> Rider is leaning/steering/doing a driveby => Use the generic bike rider anims and fade out the pedaling anims
        const auto IsAnimActive = [](const CAnimBlendAssociation* anim) {
            return anim && anim->m_BlendDelta >= 0.0f && anim->m_BlendAmount > 0.0f;
        };
        if (IsAnimActive(pedalAnim) || IsAnimActive(sprintAnim)) {
            if (pedalAnim) {
                pedalAnim->SetFlag(ANIMATION_IS_PLAYING, false);
                pedalAnim->m_BlendDelta = -8.0f;
            }
            if (sprintAnim) {
                sprintAnim->SetFlag(ANIMATION_IS_PLAYING, false);
                sprintAnim->m_BlendDelta = -8.0f;
            }
            m_RideAnimData.AnimLeanLeft *= 0.95f;
            m_RideAnimData.AnimLeanFwd  *= 0.95f;
        } else {
            ProcessRiderAnims(driver, this, &m_RideAnimData, m_BikeHandling, 0);
        }

        // Crank angle of the active pedaling anim
        if (!pedalAnim) {
            pedalAnim = sprintAnim;
        }
        m_fCrankAngle = pedalAnim
            ? GetCrankAngleFromAnim(pedalAnim)
            : 0.0f;

        // Blend crank angle depending on the lean anims
        float newCrankAngle;
        if (leftAnim && leftAnim->m_BlendAmount > 0.1f) {
            newCrankAngle = BlendCrankTowards(leftAnim, BMX_PI);
            m_bIsFreewheeling = true;
        } else if (rightAnim && rightAnim->m_BlendAmount > 0.1f) {
            newCrankAngle = BlendCrankTowards(rightAnim, 0.0f);
            m_bIsFreewheeling = true;
        } else if (fwdAnim && fwdAnim->m_BlendAmount > 0.1f) {
            newCrankAngle = BlendCrankTowards(fwdAnim, BMX_HALF_PI);
        } else {
            newCrankAngle = (float)std::pow((double)0.97f, (double)CTimer::GetTimeStep()) * m_fCrankAngle;
        }
        m_fCrankAngle = newCrankAngle;

        if (drivebyAnim) {
            m_nFixRightHand   = false;
            m_bIsFreewheeling = true;
        }
    } else {
        //> Rider is going straight => Pedal or sprint
        float animSpeed; // Speed of the pedal/sprint anim
        float sprintMaxSpeed;
        if (GetModelId() == MODEL_MTBIKE) {
            sprintMaxSpeed = 2.0f;
            if (m_nCurrentGear < 1) {
                animSpeed = 0.0f;
            } else {
                animSpeed = (5.0f * fwdSpeed) / ((float)m_nCurrentGear * m_pHandlingData->m_transmissionData.m_MaxFlatVelocity - 0.25f);
            }
        } else {
            animSpeed      = 3.0f * fwdSpeed;
            sprintMaxSpeed = 2.5f;
        }

        bool    isNewAnim{};
        CAnimBlendAssociation* mainAnim; // The anim we'll use to calculate the crank angle from
        if (m_fControlPedaling > 5.0f && animSpeed < sprintMaxSpeed) {
            //> Sprinting
            if (NeedsNewBlend(sprintAnim)) {
                sprintAnim = CAnimManager::BlendAnimation(clump, m_RideAnimData.AnimGroup, ANIM_ID_BIKE_SPRINT, 4.0f);
                isNewAnim  = true;
            }
            sprintAnim->SetFlag(ANIMATION_IS_PLAYING, true);
            if (pedalAnim) {
                pedalAnim->SetFlag(ANIMATION_IS_PLAYING, true);
                pedalAnim->m_Speed = animSpeed;
                mainAnim           = pedalAnim;
            } else {
                sprintAnim->m_Speed = animSpeed;
                mainAnim            = sprintAnim;
            }
        } else {
            if (m_GasPedal != 0.0f || m_fControlPedaling > 0.0f || GetStatus() == STATUS_SIMPLE) {
                //> Pedaling
                if (NeedsNewBlend(pedalAnim)) {
                    pedalAnim = CAnimManager::BlendAnimation(clump, m_RideAnimData.AnimGroup, ANIM_ID_BIKE_PEDAL, 4.0f);
                    isNewAnim = true;
                }
                pedalAnim->SetFlag(ANIMATION_IS_PLAYING, true);
                pedalAnim->m_Speed = animSpeed;
            } else {
                //> Coasting
                if (NeedsNewBlend(pedalAnim)) {
                    pedalAnim = CAnimManager::BlendAnimation(clump, m_RideAnimData.AnimGroup, ANIM_ID_BIKE_PEDAL, 4.0f);
                    isNewAnim = true;
                }
                pedalAnim->SetFlag(ANIMATION_IS_PLAYING, false);
                if (!vehicleFlags.bIsHandbrakeOn) {
                    if (m_aRatioHistory[0] < 1.0f || m_aRatioHistory[1] < 1.0f || m_aRatioHistory[2] < 1.0f || m_aRatioHistory[3] < 1.0f) {
                        m_bIsFreewheeling = true;
                    }
                }
            }
            mainAnim = pedalAnim;
        }

        if (!mainAnim) {
            m_fCrankAngle = (float)std::pow((double)0.97f, (double)CTimer::GetTimeStep()) * m_fCrankAngle;
        } else {
            bool synced = false;
            if (isNewAnim) {
                //> A new anim was started, sync it so it continues from the current crank angle
                float refAngle;
                bool  hasRef = true;
                if (leftAnim && leftAnim->m_BlendAmount > 0.5f) {
                    refAngle = BMX_PI;
                } else if (rightAnim && rightAnim->m_BlendAmount > 0.5f) {
                    refAngle = 0.0f;
                } else if (fwdAnim && fwdAnim->m_BlendAmount > 0.5f) {
                    refAngle = BMX_HALF_PI;
                } else {
                    hasRef = false;
                }
                if (hasRef && refAngle > -1000.0f) {
                    float phase = (0.0f - refAngle) * 0.15915493667125702f /* 0x8594F0 - 1 / 2PI */;
                    if (phase < 0.0f) {
                        phase += 1.0f;
                    }
                    mainAnim->SetCurrentTime(mainAnim->m_BlendHier->GetTotalTime() * phase);
                    m_fCrankAngle = phase; // BUG: Original stores the normalized phase here, not an angle
                    synced = true;
                }
            }
            if (!synced) {
                m_fCrankAngle = GetCrankAngleFromAnim(mainAnim);
            }
        }

        if (std::abs(m_RideAnimData.AnimLeanLeft) > 0.05f || std::abs(m_RideAnimData.AnimLeanFwd) > 0.05f) {
            m_RideAnimData.AnimLeanLeft *= 0.95f;
            m_RideAnimData.AnimLeanFwd  *= 0.95f;
        }
    }

    //> Set the "wheelie for camera" flag
    if (driver->IsPlayer()) {
        bikeFlags.bWheelieForCamera = false;

        const auto& wc  = m_WheelCounts;
        const auto  fz  = GetMatrix().GetForward().z;
        const auto  bh  = m_BikeHandling;
        if (wc[0] <= 0.0f && wc[1] <= 0.0f && fz > 0.0f && (wc[2] > 0.0f || wc[3] > 0.0f)) {
            if (bh->m_fWheelieAng - fz < bh->m_fWheelieAng * 0.5f) {
                bikeFlags.bWheelieForCamera = true;
            }
        } else if (wc[2] <= 0.0f && wc[3] <= 0.0f && fz < 0.0f && (wc[0] > 0.0f || wc[1] > 0.0f)) {
            if (bh->m_fStoppieAng * 0.6f < bh->m_fStoppieAng - fz) {
                bikeFlags.bWheelieForCamera = true;
            }
        }
    }

    m_fPedalAngleL = -m_fCrankAngle;
    m_fPedalAngleR = -m_fCrankAngle;
}

// data is a ptr to CBmx
// 0x6C0390
void CBmx::LaunchBunnyHopCB(CAnimBlendAssociation* assoc, void* data) {
    auto bmx = static_cast<CBmx*>(data);
    if ((bmx->m_WheelCounts[0] > 0.0f || bmx->m_WheelCounts[1] > 0.0f) &&
        (bmx->m_WheelCounts[2] > 0.0f || bmx->m_WheelCounts[3] > 0.0f)
    ) {
        auto power = std::min(bmx->m_fControlJump / 25.0f, 1.0f) + 1.0f;
        if (bmx->GetStatus() == STATUS_PLAYER) {
            power *= CStats::GetFatAndMuscleModifier(STAT_MOD_6);
        }
        if (CCheat::IsActive(CHEAT_HUGE_BUNNY_HOP)) {
            power *= 5.0f;
        }
        bmx->ApplyMoveForce(0.06f * bmx->m_fMass * power * bmx->m_matrix->GetUp());
        bmx->ApplyTurnForce(0.01f * bmx->m_fTurnMass * power * bmx->m_matrix->GetUp(), bmx->m_matrix->GetForward());
    }
}

// 0x6C0500 | inlined | see 0x6C11F3
void CBmx::GetFrameOffset(float& fZOffset, float& fAngleOffset) {
    const auto d1 = m_aWheelSuspensionHeights[0] - m_aWheelOrigHeights[0];
    const auto d2 = m_aWheelSuspensionHeights[1] - m_aWheelOrigHeights[1];

    fZOffset     = (1.0f - m_fMidWheelFracY) * d1 + d2 * m_fMidWheelFracY;
    fAngleOffset = std::atan2(d1 - d2, m_fMidWheelDistY);
}

// 0x6C0550
float CBmx::FindWheelWidth(bool bRear) {
    return 0.07f;
}

// 0x6C0560
void CBmx::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    // NOP
}

// 0x6C0590
void CBmx::ProcessBunnyHop() {
    auto* anim = m_pDriver
        ? RpAnimBlendClumpGetAssociation(m_pDriver->GetRpClump(), ANIM_ID_BIKE_BUNNYHOP)
        : nullptr;

    if (GetStatus() != STATUS_PLAYER || !m_pDriver || !m_pDriver->IsPlayer()) {
        if (anim) {
            anim->SetFlag(ANIMATION_IS_PLAYING, true);
            anim->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE, true);
            anim->SetBlendDelta(-8.0f);
        }
        return;
    }

    auto pad = m_pDriver->AsPlayer()->GetPadFromPlayer();

    if (pad->IsLeftShoulder1Pressed() && !pad->DisablePlayerControls && m_fControlJump == 0.0f) {
        m_fControlJump += CTimer::GetTimeStep();
        anim = CAnimManager::BlendAnimation(m_pDriver->GetRpClump(), m_RideAnimData.AnimGroup, ANIM_ID_BIKE_BUNNYHOP, 8.0f);
        if (anim) {
            anim->SetCurrentTime(0.0f);
            anim->SetFlag(ANIMATION_IS_PLAYING, false);
        }
    }

    if (m_fControlJump > 0.0f) {
        if (!pad->DisablePlayerControls) {
            if (!anim) {
                m_fControlJump = 0.0f;
                return;
            }

            if (pad->IsLeftShoulder1()) {
                if (!anim->IsPlaying()) {
                    m_fControlJump = std::min(m_fControlJump + CTimer::GetTimeStep(), 25.0f);
                    anim->SetCurrentTime(m_fControlJump / 25.0f * 0.2f);
                }
            } else if (!anim->IsPlaying()) {
                if (anim->GetCurrentTime() < 0.2f) {
                    anim->SetCurrentTime((0.2f - anim->GetCurrentTime()) / 0.2f * (anim->GetHier()->GetTotalTime() - 0.2f) + 0.2f);
                }
                anim->SetFlag(ANIMATION_IS_PLAYING, true);
                anim->SetSpeed(1.5f);
                anim->SetFinishCallback(CBmx::LaunchBunnyHopCB, this);
            }
        } else {
            m_fControlJump = 0.0f;
        }
    }

    if (anim) {
        if (anim->GetBlendAmount() > 0.5f) {
            m_GasPedal                                   = 0.0f;
            FindPlayerPed()->GetPlayerData()->m_fMoveSpeed = 0.0f;
            if (!vehicleFlags.bIsHandbrakeOn && (m_aWheelRatios[0] < 1.0f || m_aWheelRatios[1] < 1.0f || m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f)) {
                m_bIsFreewheeling = true;
            }
        }
    }
}

// 0x6C0810
void CBmx::PreRender() {
    // 0xC1C83C - Unknown flag (it's in BSS, never written to by anything reversed so far - so it's always false)
    static auto& s_UnkFlag = StaticRef<bool>(0xC1C83C);

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
        const auto speed = std::sqrt((m_vecMoveSpeed.x * m_vecMoveSpeed.x + m_vecMoveSpeed.y * m_vecMoveSpeed.y) + m_vecMoveSpeed.z * m_vecMoveSpeed.z);

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
            if (wheel == 0 || m_WheelStates[1] == WHEEL_STATE_FIXED) { // NOTE: Differs from `CBike::PreRender`
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
    if (const auto forkFront = m_aBikeNodes[BMX_FORKS_FRONT]) {
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

        if (const auto handlebars = m_aBikeNodes[BMX_HANDLEBARS]) {
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
    if (const auto forkRear = m_aBikeNodes[BMX_FORKS_REAR]) {
        const auto angle = std::asin((m_aWheelSuspensionHeights[1] - m_aWheelOrigHeights[1]) / m_fSwingArmLength) * -1.0f;
        mat.Attach(RwFrameGetMatrix(forkRear), false);
        const CVector savedPos = mat.GetPosition();
        mat.SetRotate(angle, 0.0f, 0.0f);
        mat.GetPosition().x = mat.GetPosition().x + savedPos.x;
        mat.GetPosition().y = mat.GetPosition().y + savedPos.y;
        mat.GetPosition().z = mat.GetPosition().z + savedPos.z;
        mat.UpdateRW();
    }

    // Front wheel
    mat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_WHEEL_FRONT]), false);
    {
        CVector offset = mat.GetPosition();
        if (s_UnkFlag) {
            offset.z = m_aWheelSuspensionHeights[0] - m_fForkZOffset;
            offset.y = ((cd->m_pLines[1].m_vecStart.y + cd->m_pLines[0].m_vecStart.y) * 0.5f - m_fForkYOffset)
                     - (m_aWheelSuspensionHeights[0] - m_aWheelOrigHeights[0]) * m_fSteerAngleTan;
        }
        if (m_nWheelStatus[0] == 1) {
            mat.SetRotate(m_aWheelPitchAngles[0], 0.0f, std::sin(m_aWheelPitchAngles[0]) * 0.02f);
        } else {
            mat.SetRotateX(m_aWheelPitchAngles[0]);
        }
        mat.GetPosition().x = mat.GetPosition().x + offset.x;
        mat.GetPosition().y = mat.GetPosition().y + offset.y;
        mat.GetPosition().z = mat.GetPosition().z + offset.z;
        mat.UpdateRW();
    }

    // Rear wheel
    mat.Attach(RwFrameGetMatrix(m_aBikeNodes[BMX_WHEEL_REAR]), false);
    {
        CVector offset = mat.GetPosition();
        if (s_UnkFlag && !m_aBikeNodes[BMX_FORKS_REAR]) {
            offset.z = m_aWheelSuspensionHeights[1];
        }
        if (m_nWheelStatus[1] == 1) {
            mat.SetRotate(m_aWheelPitchAngles[1], 0.0f, std::sin(m_aWheelPitchAngles[1]) * 0.04f);
        } else {
            mat.SetRotateX(m_aWheelPitchAngles[1]);
        }
        mat.GetPosition().x = mat.GetPosition().x + offset.x;
        mat.GetPosition().y = mat.GetPosition().y + offset.y;
        mat.GetPosition().z = mat.GetPosition().z + offset.z;
        mat.UpdateRW();
    }

    // Chassis (leans + bobs with the suspension)
    if (const auto chassis = m_aBikeNodes[BMX_CHASSIS]) {
        float zOffset{}, angleOffset{};
        if (!s_UnkFlag) {
            GetFrameOffset(zOffset, angleOffset);
        }

        mat.Attach(RwFrameGetMatrix(chassis), false);
        CVector offset = mat.GetPosition();
        offset.z = (1.0f - std::cos(m_RideAnimData.LeanAngle)) * cm->m_boundBox.m_vecMin.z * 0.9f + zOffset;

        mat.SetRotateX(std::abs(m_RideAnimData.LeanAngle) * -0.05f + angleOffset);
        mat.RotateY(m_fSprintLeanAngle + m_RideAnimData.LeanAngle);

        mat.GetPosition().x = mat.GetPosition().x + offset.x;
        mat.GetPosition().y = mat.GetPosition().y + offset.y;
        mat.GetPosition().z = mat.GetPosition().z + offset.z;
        mat.UpdateRW();
    }

    // Chainset and pedals
    const auto RotateNode = [&](eBmxNodes node, float angle) {
        if (const auto frame = m_aBikeNodes[node]) {
            mat.Attach(RwFrameGetMatrix(frame), false);
            const CVector savedPos = mat.GetPosition();
            mat.SetRotate(angle, 0.0f, 0.0f);
            mat.GetPosition().x = mat.GetPosition().x + savedPos.x;
            mat.GetPosition().y = mat.GetPosition().y + savedPos.y;
            mat.GetPosition().z = mat.GetPosition().z + savedPos.z;
            mat.UpdateRW();
        }
    };
    RotateNode(BMX_CHAINSET, m_fCrankAngle);
    RotateNode(BMX_PEDAL_R,  m_fPedalAngleL);
    RotateNode(BMX_PEDAL_L,  m_fPedalAngleR);
}

// 0x6C1470
bool CBmx::ProcessAI(uint32& extraHandlingFlags) {
    // NOTE: Unlike `CBike::ProcessAI` this doesn't handle vehicle recordings
    const auto mi = GetVehicleModelInfo();

    m_autoPilot.carCtrlFlags.bHonkAtCar = false;
    m_autoPilot.carCtrlFlags.bHonkAtPed = false;
    m_bIsFreewheeling                   = false;

    switch (GetStatus()) {
    case STATUS_PLAYER: {
        extraHandlingFlags += 2;
        bikeFlags.bGettingPickedUp = false;

        if (!m_pDriver || !m_pDriver->IsPlayer()) {
            break;
        }

        ProcessControlInputs((uint8)m_pDriver->m_nPedType); // NOTE: Passes the ped type as the player number
        const auto driver = m_pDriver->AsPlayer();
        const auto pad    = driver->GetPadFromPlayer();

        //> Apply a turn force based on the lean (wheelie/stoppie)
        const auto& bh = *m_BikeHandling;
        const auto  GetCappedSpeed = [&] { // min(speed, 0.1)
            return m_vecMoveSpeed.Magnitude() <= 0.1f
                ? m_vecMoveSpeed.Magnitude()
                : 0.1f;
        };
        if (m_RideAnimData.LeanFwd < 0.0f) {
            m_vecCentreOfMass.y = bh.m_fLeanBakCOM * m_RideAnimData.LeanFwd + m_pHandlingData->m_vecCentreOfMass.y;

            if (!((m_BrakePedal != 0.0f || vehicleFlags.bIsHandbrakeOn) && m_nNoOfContactWheels)) {
                const auto speed = GetCappedSpeed();
                const auto mult  = GetModelId() == MODEL_SANCHEZ
                    ? m_GasPedal * 0.7f + 0.3f
                    : (m_GasPedal + 1.0f) * 0.5f;
                const auto force = (((mult * bh.m_fLeanBakForce) * m_fTurnMass) * m_RideAnimData.LeanFwd) * speed;
                ApplyTurnForce(
                    GetUp() * -(CTimer::GetTimeStep() * force),
                    m_vecCentreOfMass + GetForward()
                );
            }
        } else {
            m_vecCentreOfMass.y = bh.m_fLeanFwdCOM * m_RideAnimData.LeanFwd + m_pHandlingData->m_vecCentreOfMass.y;

            if (m_BrakePedal < 0.0f || !m_nNoOfContactWheels) {
                const auto speed = GetCappedSpeed();
                const auto force = ((bh.m_fLeanFwdForce * m_fTurnMass) * speed) * m_RideAnimData.LeanFwd;
                ApplyTurnForce(
                    GetUp() * -(CTimer::GetTimeStep() * force),
                    m_vecCentreOfMass + GetForward()
                );
            }
        }

        PruneReferences();
        if (GetStatus() == STATUS_PLAYER) {
            DoDriveByShootings();
        }
        DoSoftGroundResistance(extraHandlingFlags);

        //> Steer the bike in the air (all wheels have full suspension extension = not touching anything)
        if (m_aWheelRatios[0] == 1.0f && m_aWheelRatios[1] == 1.0f && m_aWheelRatios[2] == 1.0f && m_aWheelRatios[3] == 1.0f) {
            const auto turnDot = DotProduct(m_vecTurnSpeed, GetUp());
            if (   (turnDot < 0.04f && pad->GetSteeringLeftRight() < 0)
                || (-0.04f < turnDot && pad->GetSteeringLeftRight() > 0)
            ) {
                const auto ts    = CTimer::GetTimeStep();
                const auto steer = pad->GetSteeringLeftRight();
                ApplyTurnForce(
                    GetRight() * ((float)steer * 0.0078125f * m_fTurnMass * ts * 0.002f),
                    GetForward()
                );
            }
        }

        ProcessBunnyHop();

        //> Sprinting (pedaling hard)
        const auto sprintAnim = RpAnimBlendClumpGetAssociation(driver->GetRpClump(), ANIM_ID_BIKE_SPRINT);
        if (driver->ControlButtonSprint(SPRINT_BMX) > 1.2f
            || (driver->GetButtonSprintResults(SPRINT_BMX) > 1.0f && sprintAnim && sprintAnim->m_BlendAmount > 0.5f)
        ) {
            bikeFlags.bPlayerBoost = true;
            m_fControlPedaling     = driver->GetPlayerData()->m_fMoveSpeed;
        } else {
            driver->HandleSprintEnergy(false, std::max(0.5f, 1.0f - (float)pad->GetAccelerate() * (1.0f / 255.0f) * 0.5f)); // 0x859A3C - 1/255

            if (driver->GetButtonSprintResults(SPRINT_BMX) <= 0.0f) {
                if (driver->GetPlayerData()->m_fTimeCanRun >= 0.0f) {
                    m_fControlPedaling = 0.0f;
                } else if (pad->GetAccelerateJustDown()) {
                    m_fControlPedaling = std::bit_cast<float>(0x409ccccdu); // ~4.9
                } else {
                    m_fControlPedaling = std::max(0.0f, m_fControlPedaling - 0.4f);
                }
            } else {
                m_fControlPedaling = std::bit_cast<float>(0x409ccccdu);
                if (m_GasPedal == 0.0f && m_BrakePedal == 0.0f) {
                    m_GasPedal = 1.0f;
                }
            }
        }

        CStats::UpdateStatsWhenCycling(bikeFlags.bPlayerBoost, this);

        if (pad->CarGunJustDown()) {
            ActivateBomb();
        }
        break;
    }
    case STATUS_PLAYER_PLAYBACK_FROM_BUFFER: {
        extraHandlingFlags += 2;
        break;
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

        // NOTE: Unlike `CBike::ProcessAI` the rotation isn't multiplied by the time step
        m_aWheelPitchAngles[0] = ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, mi->m_fWheelSizeFront * 0.5f) + m_aWheelPitchAngles[0];
        m_aWheelPitchAngles[1] = ProcessWheelRotation(WHEEL_STATE_NORMAL, GetForward(), m_vecMoveSpeed, mi->m_fWheelSizeRear * 0.5f) + m_aWheelPitchAngles[1];

        PlayHornIfNecessary();
        ReduceHornCounter();

        vehicleFlags.bVehicleColProcessed = false;
        vehicleFlags.bAudioChangingGear   = false;
        bikeFlags.bWheelieForCamera       = false;
        m_fControlPedaling                = 0.0f;
        break;
    }
    case STATUS_PHYSICS: {
        CCarAI::UpdateCarAI(this);
        CCarCtrl::SteerAICarWithPhysics(this);
        PlayHornIfNecessary();
        extraHandlingFlags += 2;

        bikeFlags.bWheelieForCamera = false;
        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal                  = 0.0f;
            m_BrakePedal                = 1.0f;
        } else {
            bikeFlags.bGettingPickedUp = false;
        }

        // Pedaling slows down
        if (m_fControlPedaling > 0.0f) {
            float decay;
            if (m_fControlPedaling > 5.0f) {
                decay = CTimer::GetTimeStep() * 0.02f;
            } else {
                decay = CTimer::GetTimeStep() * 0.01f;
                if (vehicleFlags.bUseCarCheats /* 0x42F & 2 */) {
                    decay += decay;
                }
            }
            m_fControlPedaling -= decay;
            if (m_fControlPedaling < 0.0f) {
                m_fControlPedaling = 0.0f;
            }
        }
        break;
    }
    case STATUS_ABANDONED: {
        m_BrakePedal                = 0.0f;
        vehicleFlags.bIsHandbrakeOn = m_vecMoveSpeed.SquaredMagnitude() < 0.01f || bikeFlags.bOnSideStand;
        m_GasPedal                  = 0.0f;
        m_HornCounter               = 0;
        if (m_pDriver || m_apPassengers[0] || vehicleFlags.bIsBeingCarJacked) {
            if (!bikeFlags.bOnSideStand) {
                extraHandlingFlags += 2;
            }
        }
        bikeFlags.bWheelieForCamera = false;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd  = 0.0f;
        m_fControlPedaling          = 0.0f;
        if (vehicleFlags.bIsBeingCarJacked) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_GasPedal                  = 0.0f;
            m_BrakePedal                = 1.0f;
        }
        break;
    }
    case STATUS_WRECKED: {
        m_BrakePedal                = 0.05f;
        vehicleFlags.bIsHandbrakeOn = true;
        m_fSteerAngle               = 0.0f;
        m_GasPedal                  = 0.0f;
        m_HornCounter               = 0;
        bikeFlags.bWheelieForCamera = false;
        m_fControlPedaling          = 0.0f;
        m_RideAnimData.AnimLeanLeft = 0.0f;
        m_RideAnimData.AnimLeanFwd  = 0.0f;
        break;
    }
    case STATUS_FORCED_STOP: { // Jump table index 9; `STATUS_REMOTE_CONTROLLED` (8) goes to the default case
        if (m_vecMoveSpeed.SquaredMagnitude() < 0.01f) {
            m_BrakePedal                = 1.0f;
            vehicleFlags.bIsHandbrakeOn = true;
        } else {
            m_BrakePedal                = 0.0f;
            vehicleFlags.bIsHandbrakeOn = false;
        }
        m_fSteerAngle = 0.0f;
        m_GasPedal    = 0.0f;
        m_HornCounter = 0;
        extraHandlingFlags += 2;
        bikeFlags.bWheelieForCamera = false;
        m_fControlPedaling          = 0.0f;
        break;
    }
    default:
        break;
    }

    //> Common part
    if (m_pDriver) {
        const auto clump = m_pDriver->GetRpClump();

        auto anim1 = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_FWD);
        if (!anim1 || anim1->m_BlendAmount < 0.5f) {
            anim1 = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_BACK);
        }
        auto anim2 = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYLHS);
        if (!anim2 || anim2->m_BlendAmount < 0.5f) {
            anim2 = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYRHS);
        }
        if (!anim2 || anim2->m_BlendAmount < 0.5f) {
            anim2 = RpAnimBlendClumpGetAssociation(clump, ANIM_ID_BIKE_DRIVEBYFT);
        }

        // Leaning/driveby anims active => Don't pedal
        if ((anim1 && anim1->m_BlendAmount > 0.5f) || (anim2 && anim2->m_BlendAmount > 0.5f)) {
            m_GasPedal = 0.0f;
            if (!vehicleFlags.bIsHandbrakeOn) {
                if (m_aWheelRatios[0] < 1.0f || m_aWheelRatios[1] < 1.0f || m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f) {
                    m_bIsFreewheeling = true;
                }
            }
            return true;
        }
    }

    //> Sprinting: Push the bike forward
    if (m_fControlPedaling > 5.0f && (m_aWheelRatios[2] < 1.0f || m_aWheelRatios[3] < 1.0f)) {
        const auto& fwd = GetForward();
        const auto  fwdSpeed = (m_vecMoveSpeed.z * fwd.z + m_vecMoveSpeed.y * fwd.y) + m_vecMoveSpeed.x * fwd.x;

        auto mult = 2.4f - (fwdSpeed / m_pHandlingData->m_transmissionData.m_MaxFlatVelocity) * 1.5f; // 0x858FB0, 0x858CE8
        mult      = std::max(0.0f, std::min(mult, 2.0f)); // Clamped to [0, 2] // 0x858CA0
        if (GetStatus() == STATUS_PLAYER) {
            mult *= CStats::GetFatAndMuscleModifier(STAT_MOD_5);
        } else if (vehicleFlags.bUseCarCheats /* 0x42F & 2 */) {
            mult *= 1.25f; // 0x8595F0
        }

        const auto force = CTimer::GetTimeStep() * m_fMass * 0.3f * mult * 0.008f; // 0x8714CC, 0x863984
        ApplyMoveForce(CVector{ force * fwd.x, force * fwd.y, force * fwd.z });
    }

    return true;
}
