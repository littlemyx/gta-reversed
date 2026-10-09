#include "StdInc.h"

#include "PlayerInfo.h"
#include "FireManager.h"
#include "MenuSystem.h"
#include "Hud.h"
#include "CarEnterExit.h"
#include "PedGeometryAnalyser.h"
#include "Cranes.h"
#include "Automobile.h"
#include "Bike.h"
#include "TheScripts.h"
#include "Events/EventScriptCommand.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexEnterCar.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexEnterBoatAsDriver.h"
#include "TaskComplexStealCar.h"
#include "TaskComplexGoPickUpEntity.h"
#include "TaskSimpleJetPack.h"
#include "TaskSimpleSwim.h"
#include "TaskSimpleHoldEntity.h"

void CPlayerInfo::InjectHooks() {
    RH_ScopedClass(CPlayerInfo);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x571920, { .State = HS::RedirectToGTA, .Locked = true }); // hooking ctor will produce bugs with weapons, you will never give weapon through cheat or something
    RH_ScopedInstall(CancelPlayerEnteringCars, 0x56E860);
    RH_ScopedInstall(FindObjectToSteal, 0x56DBD0);
    RH_ScopedInstall(EvaluateCarPosition, 0x56DAD0);
    RH_ScopedInstall(Process, 0x56F8D0);
    RH_ScopedInstall(FindClosestCarSectorList, 0x56F4E0);
    RH_ScopedInstall(Clear, 0x56F330);
    RH_ScopedInstall(StreamParachuteWeapon, 0x56EB30);
    RH_ScopedInstall(AddHealth, 0x56EAB0);
    RH_ScopedInstall(ArrestPlayer, 0x56E5D0);
    RH_ScopedInstall(SetPlayerSkin, 0x5717F0);
    RH_ScopedInstall(LoadPlayerSkin, 0x56F7D0);
    RH_ScopedInstall(DeletePlayerSkin, 0x56EA80);
    RH_ScopedInstall(BlowUpRCBuggy, 0x56EA30);
    RH_ScopedInstall(MakePlayerSafe, 0x56E870);
    RH_ScopedInstall(PlayerFailedCriticalMission, 0x56E830);
    RH_ScopedInstall(WorkOutEnergyFromHunger, 0x56E610);
    RH_ScopedInstall(KillPlayer, 0x56E580);
    RH_ScopedInstall(IsRestartingAfterMissionFailed, 0x56E570);
    RH_ScopedInstall(IsRestartingAfterArrest, 0x56E560);
    RH_ScopedInstall(IsRestartingAfterDeath, 0x56E550);
    RH_ScopedInstall(IsPlayerInRemoteMode, 0x56DAB0);
    RH_ScopedInstall(GetPos_Hook, 0x56DFB0);
    RH_ScopedInstall(GetSpeed_Hook, 0x56DF50);
    RH_ScopedInstall(GivePlayerParachute, 0x56EC40);
    RH_ScopedInstall(SetLastTargetVehicle, 0x56DA80);
    RH_ScopedInstall(ProcessCarGunCrosshair_Hook, 0x56EC80);
    RH_ScopedInstall(DrawCrosshair_Hook, 0x56EF90);
    RH_ScopedInstall(Load, 0x5D3B00);
    RH_ScopedInstall(Save, 0x5D3AC0);
}

// 0x571920
CPlayerInfo::CPlayerInfo() {
    // NOTE: `m_PlayerData` is constructed by `CPlayerPedData::CPlayerPedData` (0x56F810), which is inlined in the original
    m_pSkinTexture = nullptr;
    m_bParachuteReferenced = false;
    m_nRequireParachuteTimer = 0;
}

// 0x571920
CPlayerInfo* CPlayerInfo::Constructor() {
    this->CPlayerInfo::CPlayerInfo();
    return this;
}

CVector* CPlayerInfo::GetSpeed_Hook(CVector* out) {
    *out = GetSpeed();
    return out;
}

CVector* CPlayerInfo::GetPos_Hook(CVector* outPos) {
    *outPos = GetPos();
    return outPos;
}

// 0x56E860
void CPlayerInfo::CancelPlayerEnteringCars(CVehicle* vehicle) {
    // NOP
}

// 0x56DBD0
CEntity* CPlayerInfo::FindObjectToSteal(CPed* ped) {
    auto&       pedMat = ped->GetMatrix();
    const auto& pedFwd = pedMat.GetForward();
    const auto& pedRgt = pedMat.GetRight();
    const auto& pedPos = ped->GetPosition();

    // Point 1.5 units in front of the ped
    const auto offX = pedFwd.x * 0.5f * 3.0f;
    const auto offY = pedFwd.y * 0.5f * 3.0f;
    const auto offZ = pedFwd.z * 0.5f * 3.0f;
    const CVector center{ offX + pedPos.x, offY + pedPos.y, offZ + pedPos.z };

    // Sector range (the original did the clamping in a rather strange way, but the result is the same)
    const auto SectorOf = [](float v) { return (int32)std::floor(v * 0.02f + 60.0f); };
    const auto xMinSector = SectorOf(center.x - 3.0f);
    const auto yMinSector = SectorOf(center.y - 3.0f);
    const auto xMaxSector = SectorOf(center.x + 3.0f);
    const auto yMaxSector = SectorOf(center.y + 3.0f);
    const auto xMin = xMinSector > 0 ? xMinSector : 0;
    const auto yMin = yMinSector > 0 ? yMinSector : 0;
    const auto xMax = xMaxSector < 119 ? xMaxSector : 119;
    const auto yMax = yMaxSector < 119 ? yMaxSector : 119;

    CWorld::AdvanceCurrentScanCode();
    const auto scanCode = CWorld::GetCurrentScanCode();

    CEntity* found{};
    for (auto y = yMin; y <= yMax; y++) {
        for (auto x = xMin; x <= xMax; x++) {
            for (auto* node = CWorld::GetRepeatSector(x, y).Objects.GetNode(); node;) {
                auto* const obj = node->Item;
                node            = node->Next;

                const auto& objPos = obj->GetPosition();
                const auto  dx     = objPos.x - center.x;
                const auto  dy     = objPos.y - center.y;
                const auto  dz     = objPos.z - center.z;
                // NOTE: The original doesn't set the object's scan code
                if (!obj->objectFlags.bIsLiftable || obj->GetScanCode() == scanCode) {
                    continue;
                }
                if (!(dz * dz + dy * dy + dx * dx < 4.5f)) {
                    continue;
                }

                auto fwdDot = dz * pedFwd.z + dy * pedFwd.y + dx * pedFwd.x;
                if (fwdDot < 0.0f) {
                    fwdDot = 10.0f - fwdDot;
                }
                const auto rgtDot = std::abs(dz * pedRgt.z + dy * pedRgt.y + dx * pedRgt.x);
                if (rgtDot * 3.0f + fwdDot < 1000.0f) {
                    found = obj;
                }
            }
        }
    }
    return found;
}

// 0x56DAD0
void CPlayerInfo::EvaluateCarPosition(CEntity* car, CPed* ped, float pedToVehDist, float* outDistance, CVehicle** outVehicle) {
    const auto& carPosn = car->GetPosition();
    const auto& pedPosn = ped->GetPosition();
    const auto& forward = ped->GetForward();

    // Find our rotation (so that is, at which angle the forward vector is)
    const auto angleFront = CGeneral::GetATanOfXY(forward.x, forward.y);

    // Find car angle, not relative to our rotation
    const auto carAngle = CGeneral::GetATanOfXY(carPosn.x - pedPosn.x, carPosn.y - pedPosn.y);

    // Make car's angle relative to our rotation (notice: it's an abs value)
    // Basically, we calculate how much the car is in our FOV.
    const auto carAngleFromFront = std::abs(CGeneral::LimitRadianAngle(angleFront - carAngle));

    // Calculate imaginary distance based on the car's angle: The higher the angle the greater the distance
    const auto distance = (1.0f - carAngleFromFront / TWO_PI) * (10.0f - pedToVehDist);
    if (distance >= *outDistance) {
        *outDistance = distance;
        *outVehicle = car->AsVehicle();
    }
}

// 0x56F7D0
void CPlayerInfo::LoadPlayerSkin() {
    DeletePlayerSkin();
    m_pSkinTexture = CPlayerSkin::GetSkinTexture(m_szSkinName);
}

// 0x56EA80
void CPlayerInfo::DeletePlayerSkin() {
    if (m_pSkinTexture) {
        RwTextureDestroy(m_pSkinTexture);
        m_pSkinTexture = nullptr;
    }
}

// 0x5717F0
void CPlayerInfo::SetPlayerSkin(const char* name) {
    strcpy_s(m_szSkinName, name); // NOTSA: They used `strcpy`, we use `_s` for safety
    LoadPlayerSkin();
}

// 0x56DA80
void CPlayerInfo::SetLastTargetVehicle(CVehicle* vehicle) {
    CEntity::SafeCleanUpRef(m_pLastTargetVehicle);
    m_pLastTargetVehicle = vehicle;
    CEntity::SafeRegisterRef(m_pLastTargetVehicle);
}

namespace {
// Original: `CTimer::ms_fTimeStep * 0.02f * mult` kept in extended precision until `_ftol` (0x821B40).
// `mult` is either 1000.0f, or -1000.0f (0x859948).
int32 TimeStepToMS(float mult = 1000.0f) {
    return static_cast<int32>(static_cast<double>(CTimer::GetTimeStep()) * static_cast<double>(0.02f) * static_cast<double>(mult));
}

// Taxi fare: Score +1 for every second the player drives a taxi with a passenger
void ProcessTaxiFare(CPlayerInfo& info) {
    const auto* const ped = info.m_pPed;
    if (info.m_bTaxiTimerScore && ped->bInVehicle) {
        const auto* const veh = ped->m_pVehicle;
        if ((veh->m_nModelIndex == MODEL_TAXI || veh->m_nModelIndex == MODEL_CABBIE) && veh->m_pDriver == ped && veh->m_nNumPassengers > 0u) {
            const auto elapsed = CTimer::GetTimeInMS() - info.m_nTaxiTimer;
            if (elapsed >= 1000u) {
                const auto seconds = elapsed / 1000u;
                info.m_nMoney += static_cast<int32>(seconds);
                info.m_nTaxiTimer += seconds * 1000u;
            }
            return;
        }
    }
    info.m_nTaxiTimer = CTimer::GetTimeInMS();
}

// `m_nTempBufferCounter - factor * timestep(ms)`, clamped to 0 (so wheels can leave the ground for a few frames)
uint32 DecayedTempBufferCounter(const CPlayerInfo& info, float factor) {
    const auto ms = static_cast<uint32>(TimeStepToMS()); // The original adds 2^32 if it's negative, so treats it as unsigned
    auto       v  = static_cast<double>(info.m_nTempBufferCounter) - static_cast<double>(ms) * static_cast<double>(factor);
    if (v < 0.0) {
        v = 0.0;
    }
    return static_cast<uint32>(static_cast<int64>(v));
}

// Tracks two wheels / wheelie / stoppie stunts (+ records them in the stats)
void ProcessStunts(CPlayerInfo& s) {
    // The original jumps into the middle of this sequence from many places, so these are the "entry points"
    const auto ResetCarLessThan3Wheels = [&] { s.m_nCarLess3WheelCounter = 0; };                                  // 0x5700D0
    const auto ResetCarTwoWheels       = [&] { s.m_nTempBufferCounter = 0; s.m_nCarTwoWheelCounter = 0; };        // 0x5700D6
    const auto ResetBikeCounters       = [&] { s.m_nBikeRearWheelCounter = 0; s.m_nBikeFrontWheelCounter = 0; };  // 0x5700E2
    const auto ResetBikeEnd            = [&] { ResetCarTwoWheels(); ResetCarLessThan3Wheels(); };                 // 0x5700BC
    const auto ResetAll                = [&] { ResetCarLessThan3Wheels(); ResetCarTwoWheels(); ResetBikeCounters(); };

    auto* const ped = s.m_pPed;
    auto* const veh = ped->bInVehicle ? ped->m_pVehicle : nullptr;
    if (!veh) {
        ResetAll();
        return;
    }

    if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        auto* const car = veh->AsAutomobile();

        if (car->m_nNumContactWheels < 3u) {
            s.m_nCarLess3WheelCounter += static_cast<uint32>(TimeStepToMS());
        } else {
            s.m_nCarLess3WheelCounter = 0;
        }

        // Wheels 0 and 1 are the left ones, 2 and 3 are the right ones
        const auto& comp = car->m_fWheelsSuspensionCompressionPrev;
        enum class State { ON_TWO_WHEELS, GRACE_PERIOD, NOT_ON_TWO_WHEELS } state;
        if (comp[2] == 1.0f && comp[3] == 1.0f) { // Right wheels in the air
            state = (comp[0] < 1.0f && comp[1] < 1.0f && car->m_fDamageIntensity == 0.0f) ? State::ON_TWO_WHEELS : State::GRACE_PERIOD;
        } else if (comp[0] == 1.0f && comp[1] == 1.0f) { // Left wheels in the air
            state = (comp[2] < 1.0f && comp[3] < 1.0f && car->m_fDamageIntensity == 0.0f) ? State::ON_TWO_WHEELS : State::GRACE_PERIOD;
        } else {
            state = State::NOT_ON_TWO_WHEELS;
        }

        if (state == State::ON_TWO_WHEELS) {
            s.m_nCarTwoWheelCounter += static_cast<uint32>(TimeStepToMS());
            s.m_fCarTwoWheelDist = car->m_fMovingSpeed + s.m_fCarTwoWheelDist;
            s.m_nTempBufferCounter = DecayedTempBufferCounter(s, 0.5f);
            ResetBikeCounters();
            return;
        }

        const auto counter = s.m_nCarTwoWheelCounter;
        if (state == State::GRACE_PERIOD) {
            if (counter != 0u && s.m_nTempBufferCounter < 500u) {
                s.m_nTempBufferCounter -= static_cast<uint32>(TimeStepToMS(-1000.0f));
                ResetBikeCounters();
                return;
            }
        } else if (counter == 0u) {
            ResetBikeCounters();
            return;
        }
        if (counter >= 2000u) {
            s.m_fBestCarTwoWheelsDistM  = s.m_fCarTwoWheelDist;
            s.m_nBestCarTwoWheelsTimeMs = counter;
            CStats::SetNewRecordStat(STAT_LONGEST_2_WHEELS_TIME, static_cast<float>(counter / 1000u));
            CStats::SetNewRecordStat(STAT_LONGEST_2_WHEELS_DISTANCE, s.m_fCarTwoWheelDist);
        }
        s.m_fCarTwoWheelDist = 0.0f;
        ResetCarTwoWheels();
        ResetBikeCounters();
        return;
    }

    if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        auto* const bike   = veh->AsBike();
        const auto& ratios = bike->m_aRatioHistory; // [0], [1] - front wheel, [2], [3] - rear wheel (1.0 = in the air)

        const auto RecordWheelie = [&](uint32 counter) {
            s.m_fBestBikeWheelieDistM  = s.m_fBikeRearWheelDist;
            s.m_nBestBikeWheelieTimeMs = counter;
            CStats::SetNewRecordStat(STAT_LONGEST_WHEELIE_TIME, static_cast<float>(counter / 1000u));
            CStats::SetNewRecordStat(STAT_LONGEST_WHEELIE_DISTANCE, s.m_fBikeRearWheelDist);
        };
        const auto RecordStoppie = [&](uint32 counter) {
            s.m_fBestBikeStoppieDistM  = s.m_fBikeFrontWheelDist;
            s.m_nBestBikeStoppieTimeMs = counter;
            CStats::SetNewRecordStat(STAT_LONGEST_STOPPIE_TIME, static_cast<float>(counter / 1000u));
            CStats::SetNewRecordStat(STAT_LONGEST_STOPPIE_DISTANCE, s.m_fBikeFrontWheelDist);
        };
        // Wheelie/stoppie is still going, or in the grace period (0x56FF92)
        const auto ExtendGracePeriod = [&] {
            s.m_nTempBufferCounter -= static_cast<uint32>(TimeStepToMS(-1000.0f));
            s.m_nCarTwoWheelCounter  = 0;
            s.m_nCarLess3WheelCounter = 0;
        };
        // 0x56FE67
        const auto DecayTempBufferAndEnd = [&] {
            s.m_nTempBufferCounter    = DecayedTempBufferCounter(s, 0.2f);
            s.m_nCarTwoWheelCounter   = 0;
            s.m_nCarLess3WheelCounter = 0;
        };

        // Wheelie: front wheel is in the air
        if (ratios[0] == 1.0f && ratios[1] == 1.0f && s.m_nBikeFrontWheelCounter == 0u) {
            if (ratios[2] < 1.0f || (ratios[3] < 1.0f && bike->m_fDamageIntensity == 0.0f)) {
                s.m_nBikeRearWheelCounter += static_cast<uint32>(TimeStepToMS());
                s.m_fBikeRearWheelDist = bike->m_fMovingSpeed + s.m_fBikeRearWheelDist;
                DecayTempBufferAndEnd();
                return;
            }

            const auto counter = s.m_nBikeRearWheelCounter;
            if (counter != 0u && s.m_nTempBufferCounter < 500u) {
                ExtendGracePeriod();
                return;
            }
            if (counter >= 5000u) {
                RecordWheelie(counter);
            }
            s.m_nBikeRearWheelCounter = 0;
            s.m_fBikeRearWheelDist    = 0.0f;
            ResetBikeEnd();
            return;
        }

        const auto rearCounter = s.m_nBikeRearWheelCounter;
        if (rearCounter != 0u) {
            if (rearCounter >= 5000u) {
                RecordWheelie(rearCounter);
            }
            s.m_nBikeRearWheelCounter = 0;
            s.m_fBikeRearWheelDist    = 0.0f;
            ResetBikeEnd();
            return;
        }

        // Stoppie: rear wheel is in the air
        const auto frontCounter = s.m_nBikeFrontWheelCounter;
        if (ratios[2] == 1.0f && ratios[3] == 1.0f) {
            if (ratios[0] < 1.0f || (ratios[1] < 1.0f && bike->m_fDamageIntensity == 0.0f)) {
                s.m_nBikeFrontWheelCounter += static_cast<uint32>(TimeStepToMS());
                s.m_fBikeFrontWheelDist = bike->m_fMovingSpeed + s.m_fBikeFrontWheelDist;
                DecayTempBufferAndEnd();
                return;
            }
            if (frontCounter != 0u && s.m_nTempBufferCounter < 500u) {
                ExtendGracePeriod();
                return;
            }
        }
        if (frontCounter >= 2000u) {
            RecordStoppie(frontCounter);
        }
        s.m_nBikeFrontWheelCounter = 0;
        s.m_fBikeFrontWheelDist    = 0.0f;
        ResetBikeEnd();
        return;
    }

    ResetAll();
}

// Money shown on the HUD slowly catches up with the real amount
void ProcessDisplayMoney(CPlayerInfo& s) {
    if (s.m_nDisplayMoney == s.m_nMoney) {
        return;
    }
    const auto diff    = s.m_nMoney - s.m_nDisplayMoney;
    const auto absDiff = std::abs(diff);
    int32      step;
    if (absDiff > 100'000) {
        step = 12345;
    } else if (absDiff > 10'000) {
        step = 1234;
    } else if (absDiff > 1'000) {
        step = 123;
    } else {
        step = absDiff <= 50 ? 1 : 42;
    }
    if (diff < 0) {
        s.m_nDisplayMoney -= step;
    } else {
        s.m_nDisplayMoney += step;
    }
}

// The player pressed the enter/exit vehicle button while in a vehicle
void ProcessExitVehicle(CPlayerInfo& s) {
    auto* const ped = s.m_pPed;
    auto* const veh = ped->m_pVehicle;

    if (s.m_pRemoteVehicle) {
        return;
    }
    if (const auto* const entityUnder = veh->m_pEntityWeAreOn) {
        if (CBridge::ThisIsABridgeObjectMovingUp(entityUnder->m_nModelIndex)) {
            return;
        }
    }
    if (veh->GetStatus() == STATUS_WRECKED || veh->GetStatus() == STATUS_TRAIN_MOVING || veh->m_nDoorLock == CARLOCK_LOCKED_PLAYER_INSIDE) {
        return;
    }

    auto& taskMgr = ped->GetIntelligence()->GetTaskManager();
    if (auto* const active = taskMgr.GetActiveTask()) {
        if (!active->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            s.m_bTryingToExitCar = true;
            return;
        }
    }

    if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
        taskMgr.SetTask(new CTaskComplexLeaveCar{ veh, 0, 0, true, false }, TASK_PRIMARY_PRIMARY);
        ped->bTryingToReachDryLand = true;
        return;
    }

    if (!veh->CanPedStepOutCar(false) && !veh->CanPedJumpOutCar(ped)) {
        if (veh->GetStatus() == STATUS_PLAYER) {
            s.m_bTryingToExitCar = true;
        }
    } else {
        taskMgr.SetTask(new CTaskComplexLeaveCar{ veh, 0, 0, true, false }, TASK_PRIMARY_PRIMARY);
        s.m_bTryingToExitCar = true;
        s.GivePlayerParachute();
    }
}

// The player pressed the enter/exit vehicle button while on foot
void ProcessEnterVehicle(CPlayerInfo& s, uint32 playerIndex) {
    auto* const ped = s.m_pPed;
    if (ped->m_pAttachedTo) {
        return;
    }

    auto* const intel         = ped->GetIntelligence();
    auto&       taskMgr       = intel->GetTaskManager();
    auto* const objectToSteal = CPlayerInfo::FindObjectToSteal(ped);
    auto* const simplestTask  = taskMgr.GetSimplestActiveTask();
    auto* const holdTask      = intel->GetTaskHold(false);
    if (simplestTask->GetTaskType() == TASK_SIMPLE_CLIMB || intel->GetTaskFighting() || (holdTask && holdTask->m_pEntityToHold)) {
        return;
    }

    // Pick up the object
    if (objectToSteal) {
        if (auto* const active = taskMgr.GetActiveTask()) {
            if (!active->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                return;
            }
        }

        auto* const task = new CTaskComplexGoPickUpEntity{ objectToSteal, ANIM_GROUP_CARRY }; // 0x51

        CEventScriptCommand event{ TASK_PRIMARY_PRIMARY, task, false };
        CWorld::Players[CWorld::PlayerInFocus].m_pPed->GetIntelligence()->GetEventGroup().Add(&event, false);
        return;
    }

    // Find the closest vehicle
    CVehicle* targetVeh{};
    float     targetVehDist{};

    const auto& pedPos = ped->GetPosition();
    const float minX   = pedPos.x - 10.0f;
    const float maxX   = pedPos.x + 10.0f;
    const float minY   = pedPos.y - 10.0f;
    const float maxY   = pedPos.y + 10.0f;

    const auto SectorOf = [](float v) { return static_cast<int32>(std::floor(static_cast<float>(static_cast<double>(v) * 0.02f + 60.0f))); };
    const auto xMin     = SectorOf(minX);
    const auto yMin     = SectorOf(minY);
    const auto xMax     = SectorOf(maxX);
    const auto yMax     = SectorOf(maxY);

    CWorld::AdvanceCurrentScanCode();
    for (auto y = yMin; y <= yMax; y++) {
        for (auto x = xMin; x <= xMax; x++) {
            s.FindClosestCarSectorList(CWorld::GetRepeatSector(x, y).Vehicles, ped, minX, minY, maxX, maxY, &targetVehDist, &targetVeh);
        }
    }
    if (!targetVeh) {
        return;
    }

    // If it's a trailer then get in the thing towing it
    if (targetVeh->m_nVehicleSubType == VEHICLE_TYPE_TRAILER && targetVeh->m_pTowingVehicle) {
        targetVeh = targetVeh->m_pTowingVehicle;
    }
    if (!targetVeh->CanBeDriven()) {
        return;
    }

    if (auto* const jetPack = intel->GetTaskJetPack()) {
        jetPack->DropJetPack(ped);
    }

    // Boats
    if (targetVeh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
        if (!targetVeh->m_pDriver) {
            if (auto* const active = taskMgr.GetActiveTask()) {
                if (!active->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                    return;
                }
            }
            taskMgr.SetTask(new CTaskComplexEnterCarAsDriver{ targetVeh }, TASK_PRIMARY_PRIMARY);
        }
        return;
    }

    if (auto* const active = taskMgr.GetActiveTask()) {
        bool bGetOutOfWaterToDoor{};
        if (active->GetTaskType() == TASK_COMPLEX_IN_WATER && intel->GetTaskSwim()) {
            switch (targetVeh->m_nModelIndex) { // Aircraft that can land on water
            case MODEL_SKIMMER:
            case MODEL_VORTEX:
            case MODEL_SEASPAR:
            case MODEL_LEVIATHN: {
                CVector doorPos{};
                int32   doorId{};
                if (CCarEnterExit::GetNearestCarDoor(ped, targetVeh, doorPos, doorId)) {
                    auto* const swimTask = intel->GetTaskSwim();
                    swimTask->m_vecPos    = doorPos;
                    swimTask->m_nTimeStep = 5000;
                    bGetOutOfWaterToDoor  = true;
                }
                break;
            }
            }
        }
        if (!bGetOutOfWaterToDoor && !active->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return;
        }
    }

    // Single player (or both players are allowed to be in separate cars)
    if (!CWorld::Players[1].m_pPed || CGameLogic::bPlayersCanBeInSeparateCars) {
        taskMgr.SetTask(new CTaskComplexEnterCarAsDriver{ targetVeh }, TASK_PRIMARY_PRIMARY);
        return;
    }

    // Co-op: Take into consideration what the other player is doing
    const auto  otherPlayerIdx = static_cast<int32>(playerIndex + 1) % 2;
    auto* const otherPed       = CWorld::Players[otherPlayerIdx].m_pPed;
    auto&       otherTaskMgr   = otherPed->GetIntelligence()->GetTaskManager();
    auto* const otherDefault   = otherTaskMgr.m_aPrimaryTasks[TASK_PRIMARY_DEFAULT];
    auto* const otherPrimary   = otherTaskMgr.m_aPrimaryTasks[TASK_PRIMARY_PRIMARY];

    int32 otherDefaultType{}, otherPrimaryType{};
    CVehicle* driverOf{};    // Vehicle the other player is driving (or entering as the driver)
    CVehicle* passengerOf{}; // Vehicle the other player is a passenger in (or entering as one)
    if (otherDefault) {
        otherDefaultType = otherDefault->GetTaskType();
    }
    if (otherPrimary) {
        otherPrimaryType = otherPrimary->GetTaskType();
        switch (otherPrimaryType) {
        case TASK_COMPLEX_ENTER_CAR_AS_DRIVER:
        case TASK_COMPLEX_DRAG_PED_FROM_CAR:
            driverOf = static_cast<CTaskComplexEnterCar*>(otherPrimary)->GetTargetCar();
            break;
        case TASK_COMPLEX_STEAL_CAR:
            driverOf = static_cast<CTaskComplexStealCar*>(otherPrimary)->m_veh;
            break;
        case TASK_COMPLEX_ENTER_BOAT_AS_DRIVER:
            driverOf = static_cast<CTaskComplexEnterBoatAsDriver*>(otherPrimary)->GetTargetVehicle();
            break;
        }
    }
    if (otherDefaultType == TASK_SIMPLE_PLAYER_IN_CAR || otherPrimaryType == TASK_SIMPLE_PLAYER_IN_CAR || otherDefaultType == TASK_SIMPLE_CAR_DRIVE || otherPrimaryType == TASK_SIMPLE_CAR_DRIVE) {
        auto* const otherVeh = otherPed->m_pVehicle;
        if (otherVeh->m_pDriver == otherPed) {
            driverOf = otherVeh;
        } else {
            passengerOf = otherVeh; // NOTE: Yes, that's what the original does
        }
    }
    if (otherPrimaryType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) {
        passengerOf = static_cast<CTaskComplexEnterCar*>(otherPrimary)->GetTargetCar();
    }

    if (driverOf == targetVeh) { // The other player is driving it, so go in as a passenger
        taskMgr.SetTask(new CTaskComplexEnterCarAsPassenger{ targetVeh, 0, false }, TASK_PRIMARY_PRIMARY);
    } else if (passengerOf == targetVeh || (!driverOf && !passengerOf)) {
        taskMgr.SetTask(new CTaskComplexEnterCarAsDriver{ targetVeh }, TASK_PRIMARY_PRIMARY);
    }
}

// Handles the fade-out/in and cleanup after the remote controlled vehicle exploded
void ProcessRemoteVehicleExplosion(CPlayerInfo& s) {
    if (!s.m_bAfterRemoteVehicleExplosion) {
        return;
    }

    const auto prevElapsed = CTimer::GetPreviousTimeInMS() - s.m_nTimeOfRemoteVehicleExplosion;
    const auto elapsed     = CTimer::GetTimeInMS() - s.m_nTimeOfRemoteVehicleExplosion;

    if (prevElapsed < 1000u && elapsed >= 1000u && s.m_nPlayerState == PLAYERSTATE_PLAYING && s.m_bFadeAfterRemoteVehicleExplosion) {
        TheCamera.SetFadeColour(0, 0, 0);
        TheCamera.Fade(1.0f, eFadeFlag::FADE_IN);
    }

    if (elapsed > 2000u) {
        if (s.m_nPlayerState == PLAYERSTATE_PLAYING && s.m_bFadeAfterRemoteVehicleExplosion) {
            TheCamera.RestoreWithJumpCut();
            TheCamera.SetFadeColour(0, 0, 0);
            TheCamera.Fade(1.0f, eFadeFlag::FADE_OUT);
            TheCamera.Process();
            CTimer::Stop();
            CRenderer::RequestObjectsInFrustum(nullptr, 0);
            CStreaming::LoadAllRequestedModels(false);
            CTimer::Update();
        }
        s.m_bAfterRemoteVehicleExplosion = false;

        auto& focusedInfo = CWorld::Players[CWorld::PlayerInFocus];
        if (focusedInfo.m_pRemoteVehicle) {
            focusedInfo.m_pRemoteVehicle->m_bRemoveFromWorld = true;
        }
        focusedInfo.m_pRemoteVehicle = nullptr;

        if (const auto* const focusedPed = focusedInfo.m_pPed; focusedPed && focusedPed->bInVehicle && focusedPed->m_pVehicle) {
            focusedPed->m_pVehicle->SetStatus(STATUS_PLAYER);
        }
    }
}

// Blows up the car of the player if it's stuck upside down for too long
void ProcessUpsideDownVehicle(CPlayerInfo& s) {
    const auto IsUpsideDown = [&] {
        auto* const veh = FindPlayerVehicle();
        return veh
            && s.m_pPed->bInVehicle
            && veh->GetMatrix().GetUp().z < 0.0f
            && veh->m_vecMoveSpeed.Magnitude() < 0.05f
            && (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE || veh->m_nVehicleType == VEHICLE_TYPE_BOAT)
            && !veh->physicalFlags.bSubmergedInWater;
    };

    if (IsUpsideDown()) {
        s.m_nTimesUpsideDownInARow += FindPlayerVehicle()->GetMatrix().GetUp().z < -0.5f ? 2u : 1u;
    } else {
        s.m_nTimesUpsideDownInARow = 0;
    }

    if (s.m_nTimesUpsideDownInARow > 6u) {
        auto* const veh = FindPlayerVehicle();
        if (veh->vehicleFlags.bCanBeDamaged) {
            if (veh->m_fHealth > 249.0f) {
                veh->m_fHealth = 249.0f;
            }
            if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
                auto* const car = veh->AsAutomobile();
                car->m_damageManager.SetEngineStatus(225);
                car->m_pExplosionVictim = nullptr;
            }
        }
    }
}

// Distance travelled stats (on foot/in a vehicle)
void ProcessDistanceStats(CPlayerInfo& s) {
    const auto* const focusedPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
    if (focusedPed && focusedPed->bInVehicle && focusedPed->m_pVehicle) {
        auto* const veh   = focusedPed->m_pVehicle;
        const auto  speed = veh->m_fMovingSpeed;
        if (veh->m_nModelIndex == MODEL_CADDY) {
            CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_GOLF_CART, speed);
        } else if (veh->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_BICYCLE, speed);
        } else {
            const auto appearance = veh->GetVehicleAppearance();
            if (appearance == VEHICLE_APPEARANCE_HELI) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_HELICOPTER, speed);
            }
            if (appearance == VEHICLE_APPEARANCE_PLANE) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_PLANE, speed);
            }
            if (appearance == VEHICLE_APPEARANCE_AUTOMOBILE) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_CAR, speed);
            }
            if (appearance == VEHICLE_APPEARANCE_BIKE) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_MOTORBIKE, speed);
            }
            if (appearance == VEHICLE_APPEARANCE_BOAT) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_BOAT, speed);
            }
            if ((appearance == VEHICLE_APPEARANCE_PLANE || appearance == VEHICLE_APPEARANCE_HELI) && veh->m_vecMoveSpeed.Magnitude() > 0.2f) {
                CStats::IncrementStat(STAT_FLIGHT_TIME, CTimer::GetTimeStep() * 16.0f);
            }
        }

        if (!FindPlayerTrain()) {
            auto* const playerVeh = FindPlayerVehicle();
            if (playerVeh->m_vecMoveSpeed.SquaredMagnitude() > 0.0f) {
                switch (playerVeh->GetVehicleAppearance()) {
                case VEHICLE_APPEARANCE_BIKE:
                    if (playerVeh->m_nVehicleSubType != VEHICLE_TYPE_BMX) {
                        CStats::UpdateStatsWhenOnMotorBike(playerVeh->AsBike());
                    }
                    break;
                case VEHICLE_APPEARANCE_HELI:
                case VEHICLE_APPEARANCE_PLANE:
                    CStats::UpdateStatsWhenFlying(playerVeh);
                    break;
                case VEHICLE_APPEARANCE_BOAT:
                    break;
                default:
                    CStats::UpdateStatsWhenDriving(playerVeh);
                    break;
                }
            }
        }
    } else if (focusedPed->m_fMovingSpeed > 0.0f) {
        auto* const intel = s.m_pPed->GetIntelligence();
        if (intel->GetTaskSwim()) {
            if (CPad::GetPad(0)->GetPedWalkLeftRight() != 0 || CPad::GetPad(0)->GetAccelerate() != 0) {
                CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_BY_SWIMMING, focusedPed->m_fMovingSpeed);
            }
        } else if (intel->GetTaskJetPack()) {
            CStats::IncrementStat(STAT_TIME_ON_JETPACK, CTimer::GetTimeStep() * 16.0f);
        } else {
            CStats::IncrementStat(STAT_DISTANCE_TRAVELLED_ON_FOOT, focusedPed->m_fMovingSpeed);
        }
    }
}

// Updates `m_fCurrentChaseValue`: how "ill" the chase is, based on the wanted level
void ProcessChaseValue(CPlayerInfo& s) {
    // Original static locals: A `static CVector` + its init guard
    static auto& s_LastPlayerPosGuard  = StaticRef<uint32>(0xB9B9A0);
    static auto& s_LastPlayerPos       = StaticRef<CVector>(0xB9B994);
    static auto& s_bMovedFarFromLast   = StaticRef<bool>(0x8CDF21); // Player moved at least 10 units since the last check
    static auto& s_bIsNearVehicleNode  = StaticRef<bool>(0x8CDF20); // There's a vehicle path node within 60 units of the player

    const auto* const wanted = s.m_pPed->GetWanted();
    if (wanted->m_WantedLevel == eWantedLevel::WANTED_CLEAN || CTheScripts::IsPlayerOnAMission()) {
        s.m_fCurrentChaseValue = 0.0f;
        return;
    }

    if ((s_LastPlayerPosGuard & 1u) == 0u) {
        s_LastPlayerPosGuard |= 1u;
    }

    // Every 20 seconds
    if (CTimer::GetTimeInMS() / 20'000u != CTimer::GetPreviousTimeInMS() / 20'000u) {
        const auto pos = FindPlayerCoors();
        s_bMovedFarFromLast = false;
        const auto dx   = static_cast<double>(s_LastPlayerPos.x) - static_cast<double>(pos.x);
        const auto dy   = static_cast<double>(s_LastPlayerPos.y) - static_cast<double>(pos.y);
        const auto dz   = static_cast<double>(s_LastPlayerPos.z) - static_cast<double>(pos.z);
        if (10.0 <= std::sqrt(dz * dz + dy * dy + dx * dx)) {
            s_bMovedFarFromLast = true;
        }

        s_LastPlayerPos = FindPlayerCoors();
        s_bIsNearVehicleNode = ThePaths.FindNodeClosestToCoors(FindPlayerCoors(), PATH_TYPE_VEH, 60.0f, 1, 0, 0, 0, 0).IsAreaValid();
    }

    float targetChase = 0.0f;
    switch (wanted->m_WantedLevel) {
    case eWantedLevel::WANTED_LEVEL_1: targetChase = 31.0f;   break;
    case eWantedLevel::WANTED_LEVEL_2: targetChase = 62.0f;   break;
    case eWantedLevel::WANTED_LEVEL_3: targetChase = 125.0f;  break;
    case eWantedLevel::WANTED_LEVEL_4: targetChase = 250.0f;  break;
    case eWantedLevel::WANTED_LEVEL_5: targetChase = 500.0f;  break;
    case eWantedLevel::WANTED_LEVEL_6: targetChase = 1000.0f; break;
    default:                                                  break;
    }

    const auto delta = static_cast<float>((static_cast<double>(targetChase) - static_cast<double>(s.m_fCurrentChaseValue)) * static_cast<double>(CTimer::GetTimeStep()) * static_cast<double>(0.0001f));
    if (delta < 0.0f
        || (s_bMovedFarFromLast && s_bIsNearVehicleNode && !CCullZones::NoPolice() && !CCullZones::PoliceAbandonCars() && CGame::currArea == AREA_CODE_NORMAL_WORLD)
    ) {
        s.m_fCurrentChaseValue = delta + s.m_fCurrentChaseValue;
    }
}
} // namespace

// 0x56F8D0
void CPlayerInfo::Process(uint32 playerIndex) {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    CPad* const pad = CPad::GetPad(playerIndex);

    ProcessTaxiFare(*this);
    ProcessStunts(*this);
    WorkOutEnergyFromHunger();
    ProcessDisplayMoney(*this);

    m_pPed->m_fMaxHealth = CStats::GetFatAndMuscleModifier(STAT_MOD_MAX_HEALTH);
    m_nMaxHealth         = static_cast<uint8>(static_cast<int32>(m_pPed->m_fMaxHealth));

    if (m_pPed->bInVehicle && CStats::GetStatValue(STAT_FLYING_SKILL) >= 400.0f) {
        StreamParachuteWeapon(true);
    } else if (m_bParachuteReferenced) {
        CGameLogic::IsCoopGameGoingOn(); // NOTE: Result is unused
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced   = false;
            m_nRequireParachuteTimer = 0;
        }
    }

    // Road density around the player (updated every 16 frames, and smoothed every frame)
    if ((CTimer::m_FrameCounter & 0xF) == 0) {
        const CEntity* const posEntity = m_pPed->bInVehicle ? static_cast<CEntity*>(m_pPed->m_pVehicle) : static_cast<CEntity*>(m_pPed);
        const auto&          pos       = posEntity->GetPosition();
        m_fRoadDensityAroundPlayer     = ThePaths.CalcRoadDensity(pos.x, pos.y); // 0x44EFC0
    }
    m_fRoadDensityAroundPlayer = static_cast<float>((static_cast<double>(m_fRoadDensityAroundPlayer) - static_cast<double>(1.0f)) * static_cast<double>(0.6f) + static_cast<double>(1.0f));
    if (m_fRoadDensityAroundPlayer < 0.5f) {
        m_fRoadDensityAroundPlayer = 0.5f;
    }
    if (1.45f < m_fRoadDensityAroundPlayer) {
        m_fRoadDensityAroundPlayer = 1.45f;
    }

    // Enter/exit vehicle
    if (!pad->GetTarget()
        && m_pPed->bCanExitCar
        && !m_pPed->bUsingMobilePhone
        && (pad->ExitVehicleJustDown() || (m_bTryingToExitCar && pad->GetExitVehicle() && m_pPed->bInVehicle))
    ) {
        m_bTryingToExitCar = false;
        if (m_pPed->bInVehicle) {
            ProcessExitVehicle(*this);
        } else {
            ProcessEnterVehicle(*this, playerIndex);
        }
    } else {
        m_bTryingToExitCar = false;
    }

    ProcessRemoteVehicleExplosion(*this);

    if ((CTimer::m_FrameCounter & 0x1F) == 0) {
        ProcessUpsideDownVehicle(*this);
    }

    ProcessDistanceStats(*this);
    ProcessChaseValue(*this);

    ProcessCarGunCrosshair(playerIndex, pad); // 0x56EC80

    m_nMoney        = std::min(m_nMoney, 999'999'999);
    m_nDisplayMoney = std::min(m_nDisplayMoney, 999'999'999);
}

// 0x56EC80 - In the original `this` is the address of `m_nCrosshairActivated` (see `ProcessCarGunCrosshair_Hook`)
void CPlayerInfo::ProcessCarGunCrosshair(uint32 playerIndex, CPad* pad) {
    if ((uint8)m_nCrosshairActivated == 0) {
        return;
    }

    // NOTE: The original keeps most of the below at extended precision (x87), hence the `double`s.
    constexpr float STEER_SCALE = 0.00033333333f; // 0x865030
    auto&           crossX      = m_vecCrosshairTarget.x;
    auto&           crossY      = m_vecCrosshairTarget.y;

    const float timeStep = CTimer::ms_fTimeStep;
    crossX = (float)((double)pad->GetSteeringLeftRight() * (double)timeStep * (double)STEER_SCALE + (double)crossX);
    if (CPad::bInvertLook4Pad) {
        crossY = (float)((double)pad->GetSteeringUpDown() * (double)CTimer::ms_fTimeStep * (double)STEER_SCALE + (double)crossY);
    } else {
        crossY = (float)((double)crossY - (double)pad->GetSteeringUpDown() * (double)CTimer::ms_fTimeStep * (double)STEER_SCALE);
    }

    // Clamp to [-0.9, 0.9] (NaN-safe in the same way as the original)
    if (crossX > 0.9f)  { crossX = 0.9f; }
    if (crossX < -0.9f) { crossX = -0.9f; }
    if (crossY > 0.9f)  { crossY = 0.9f; }
    if (crossY < -0.9f) { crossY = -0.9f; }

    if (pad->GetCarGunFired() == 0) {
        return;
    }

    CamShakeNoPos(&TheCamera, 0.2f);

    const auto& right = TheCamera.m_mCameraMatrix.GetRight();
    const auto& fwd   = TheCamera.m_mCameraMatrix.GetForward();
    const auto& up    = TheCamera.m_mCameraMatrix.GetUp();
    constexpr double FOV_SCALE = (double)0.008726646f; // 0x8631D4

    // Up component
    const double v1 = std::tan((double)TheCamera.FindCamFOV() * FOV_SCALE) / (double)CDraw::ms_fAspectRatio * (double)crossY;
    const float  upX = (float)((double)up.x * v1), upY = (float)((double)up.y * v1), upZ = (float)((double)up.z * v1);

    // Right component
    const double tan2 = std::tan((double)TheCamera.FindCamFOV() * FOV_SCALE);
    const float  rX   = (float)((double)right.x * (double)crossX);
    const float  rY   = (float)((double)right.y * (double)crossX);
    const double rZ   = (double)crossX * (double)right.z;
    const float  aX   = (float)((double)rX * tan2);
    const float  aY   = (float)((double)rY * tan2);
    const double aZ   = rZ * tan2;

    const double t1 = (double)fwd.x - (double)aX;
    const float  t2 = (float)((double)fwd.y - (double)aY);
    const float  t3 = (float)((double)fwd.z - aZ);

    const float dirX = (float)(t1 - (double)upX);
    const float dirY = (float)((double)t2 - (double)upY);
    const float dirZ = (float)((double)t3 - (double)upZ);

    const auto& camPos = TheCamera.m_mCameraMatrix.GetPosition();
    const double dx = (double)dirX * 200.0; // 0x858A48
    const double dy = (double)dirY * 200.0;
    const float  dz = (float)((double)dirZ * 200.0);
    CVector target{
        (float)(dx + (double)camPos.x),
        (float)((double)camPos.y + dy),
        (float)((double)camPos.z + (double)dz)
    };

    CWeapon weapon{ WEAPON_M4, 5000 };
    CVector source = TheCamera.GetPosition();

    auto* const ped    = CWorld::Players[playerIndex].m_pPed;
    const bool  oldDoomAim = ped->bDoomAim;
    ped->bDoomAim          = false;
    weapon.FireInstantHit(ped, &source, &source, nullptr, &target, nullptr, true, true);
    CWorld::Players[playerIndex].m_pPed->bDoomAim = oldDoomAim; // The original re-reads the player's ped here (0x56EF5F)
}

// 0x56EC80 - Hook wrapper: `this` is the address of `m_nCrosshairActivated`
void CPlayerInfo::ProcessCarGunCrosshair_Hook(uint32 playerIndex, CPad* pad) {
    auto* const self = reinterpret_cast<CPlayerInfo*>(reinterpret_cast<uint8*>(this) - offsetof(CPlayerInfo, m_nCrosshairActivated));
    self->ProcessCarGunCrosshair(playerIndex, pad);
}

// 0x56EF90 - In the original `this` is the address of `m_nCrosshairActivated` (see `DrawCrosshair_Hook`)
void CPlayerInfo::DrawCrosshair(int32 playerIndex) {
    if ((uint8)m_nCrosshairActivated == 0) { // The original only tests the lowest byte
        return;
    }

    // Per-player (2 players) history of the last 5 crosshair positions (index 0 is the newest)
    static auto& s_TrailTime = StaticRef<std::array<std::array<uint32, 5>, 2>>(0xB9B8F8); // Only [i][0] is ever read (see the BUG note below)
    static auto& s_TrailY    = StaticRef<std::array<std::array<float, 5>, 2>>(0xB9B920);
    static auto& s_TrailX    = StaticRef<std::array<std::array<float, 5>, 2>>(0xB9B948);
    static auto& s_CoronaTex = StaticRef<int32>(0x8CDF1C); // NOTSA name: index into `gpCoronaTexture` (value is 8 = CORONATYPE_STREAK)

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVDESTALPHA)); // The exe really passes 8 here (not INVSRCALPHA)
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpCoronaTexture[s_CoronaTex])));

    // Player 0 is drawn green, the other one blue
    uint32 r, g, b;
    if (playerIndex == 0) {
        r = 0;
        g = 255;
        b = 50;
    } else {
        r = 50;
        g = 0;
        b = 255;
    }

    // Shift the history
    auto& timeHist = s_TrailTime[playerIndex];
    auto& yHist    = s_TrailY[playerIndex];
    auto& xHist    = s_TrailX[playerIndex];
    for (auto k = 0; k < 4; k++) {
        xHist[4 - k] = xHist[3 - k];
        yHist[4 - k] = yHist[3 - k];
        // BUG: The source of this copy is `s_TrailY[0][3 - k]` (absolute address 0xB9B92C - 4 * k) instead of `timeHist[3 - k]`.
        // The destination slots ([i][1..4]) are never read, so it is harmless. Kept as is.
        timeHist[4 - k] = std::bit_cast<uint32>(s_TrailY[0][3 - k]);
    }
    xHist[0]    = m_vecCrosshairTarget.x;
    yHist[0]    = m_vecCrosshairTarget.y;
    timeHist[0] = CTimer::m_snTimeInMilliseconds;

    // NOTE: The original keeps all intermediate values on the x87 stack (extended precision) => `double`s
    for (auto step = 0; step < 5; step++) {
        const float pulse = (float)(std::sin((double)(timeHist[0] & 0x3FF) * (double)0.006135923322290182f /* 0x865034 */) * (double)0.2f /* 0x858CC4 */ + (double)1.0f /* 0x858624 */);
        for (auto ring = 0; ring < 3; ring++) {
            const float radius = (float)((double)ring * (double)10.0f /* 0x85862C */ + (double)20.0f /* 0x858BA4 */);
            for (auto n = 0; n < 4; n++) {
                const double angle = (double)n * (double)1.5707964f /* 0x858FE4 */ + (double)0.78539819f /* 0x859AB0 */;
                const double cosV  = std::cos(angle);
                const double sinV  = std::sin(angle);
                const float  posY  = (float)(cosV * (double)radius * (double)pulse + ((double)yHist[step] + 1.0) * (double)RsGlobal.maximumHeight * 0.5);
                const float  posX  = (float)(sinV * (double)radius * (double)pulse + ((double)xHist[step] + 1.0) * (double)RsGlobal.maximumWidth * 0.5);
                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                    { posX, posY, 100.0f },
                    { 15.0f, 15.0f },
                    (uint8)r, (uint8)g, (uint8)b,
                    255,
                    0.01f, // 0x3C23D70A
                    0.0f,
                    255
                );
            }
        }
        r >>= 1;
        g >>= 1;
        b >>= 1;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(TRUE));
}

// 0x56EF90 - Hook wrapper: `this` is the address of `m_nCrosshairActivated`
void CPlayerInfo::DrawCrosshair_Hook(int32 playerIndex) {
    auto* const self = reinterpret_cast<CPlayerInfo*>(reinterpret_cast<uint8*>(this) - offsetof(CPlayerInfo, m_nCrosshairActivated));
    self->DrawCrosshair(playerIndex);
}

// 0x56F4E0
void CPlayerInfo::FindClosestCarSectorList(CPtrListDoubleLink<CVehicle*>& ptrList, CPed* ped, float minX, float minY, float maxX, float maxY, float* outVehDist, CVehicle** outVehicle) {
    for (auto* node = ptrList.GetNode(); node;) {
        auto* const veh = node->Item;
        node            = node->Next;

        if (veh->IsScanCodeCurrent() || !veh->GetUsesCollision() || veh->GetType() != ENTITY_TYPE_VEHICLE) {
            continue;
        }
        veh->SetCurrentScanCode();
        if (veh->GetStatus() == STATUS_WRECKED || veh->GetStatus() == STATUS_TRAIN_MOVING) {
            continue;
        }

        // Skip vehicles that are not upright (but let bikes through)
        if (veh->GetMatrix().GetUp().z <= 0.3f && veh->m_nVehicleSubType != VEHICLE_TYPE_BIKE) {
            continue;
        }

        const auto& vehPos     = veh->GetPosition();
        const auto  vehBaseZ   = vehPos.z - veh->GetDistanceFromCentreOfMassToBaseOfModel() + 1.0f;
        const auto& pedPos     = ped->GetPosition();
        auto        pedToVehDist = ((pedPos.x - vehPos.x) + (pedPos.y - vehPos.y)) * 0.0f + (pedPos.z - vehBaseZ); // NOTE: Yes, that's how it was
        // ^ NOTE: Yes, `(dx + dy) * 0.0f` is in the original as well. Result is basically the Z distance.

        const auto IsAcceptableDistance = [&]() -> bool {
            if (veh->m_nModelIndex == MODEL_AT400) {
                CVector doorPos;
                int32   doorId{};
                if (CCarEnterExit::GetNearestCarDoor(ped, veh, doorPos, doorId)) {
                    pedToVehDist = std::abs(ped->GetPosition().z - doorPos.z);
                    if (pedToVehDist < 1.0f) {
                        return true;
                    }
                }
            }
            if (std::abs(pedToVehDist) < 2.0f) {
                return true;
            }
            if (veh->m_nVehicleSubType != VEHICLE_TYPE_BOAT) {
                return false;
            }
            const auto pedZ = ped->GetPosition().z;
            if (vehBaseZ < pedZ && vehBaseZ > pedZ - 4.0f) {
                return true;
            }
            return ped->m_pContactEntity == veh;
        };
        if (!IsAcceptableDistance()) {
            continue;
        }

        if (!veh->vehicleFlags.bConsideredByPlayer) {
            continue;
        }

        CVector doorPos;
        int32   doorId{};
        if (!CCarEnterExit::GetNearestCarDoor(ped, veh, doorPos, doorId)) {
            continue;
        }

        const CVector2D pedToVeh{ ped->GetPosition().x - veh->GetPosition().x, ped->GetPosition().y - veh->GetPosition().y };
        const auto      dist2D = CVector{ pedToVeh.x, pedToVeh.y, 0.0f }.Magnitude();
        pedToVehDist           = dist2D;
        if (dist2D > 10.0f) {
            if (CPedGeometryAnalyser::LiesInsideBoundingBox(*ped, ped->GetPosition(), *veh)) {
                pedToVehDist = 10.0f;
            } else {
                continue;
            }
        }

        if (!CCranes::IsThisCarBeingCarriedByAnyCrane(veh)) {
            EvaluateCarPosition(veh, ped, pedToVehDist, outVehDist, outVehicle);
        }
    }
}

// 0x56F330
void CPlayerInfo::Clear() {
    // TODO: This should just use the constructor and `swap`,
    // like to just swap ourselves to a default constructed object
    // and do all the cleanup in the destructor

    m_pPed = nullptr;
    m_pRemoteVehicle = nullptr;
    if (m_pSpecCar) {
        m_pSpecCar->physicalFlags.bAddMovingCollisionSpeed = false;
        m_pSpecCar = nullptr;
    }
    m_nDisplayMoney = 0;
    m_nMoney = 0;
    m_nPlayerState = PLAYERSTATE_PLAYING;
    m_nCarDensityForCurrentZone = 0;
    m_fRoadDensityAroundPlayer = 1.0f;
    m_bAfterRemoteVehicleExplosion = false;
    m_bCreateRemoteVehicleExplosion = false;
    m_bFadeAfterRemoteVehicleExplosion = false;
    m_bTryingToExitCar = false;
    m_bTaxiTimerScore = false;
    m_nTaxiTimer = 0;
    m_nVehicleTimeCounter = CTimer::GetTimeInMS();
    m_nMaxArmour = 100;
    m_nMaxHealth = 100;
    m_bCanDoDriveBy = true;
    m_nCollectablesPickedUp = 0;
    m_nTotalNumCollectables = 3;
    m_nLastTimeEnergyLost = 0;
    m_nLastTimeArmourLost = 0;
    m_nLastTimeBigGunFired = 0;
    m_nTimesStuckInARow = 0;
    m_nTimesUpsideDownInARow = 0;
    m_nCarTwoWheelCounter = 0;
    m_fCarTwoWheelDist = 0.0f;
    m_nCarLess3WheelCounter = 0;
    m_nBikeRearWheelCounter = 0;
    m_fBikeRearWheelDist = 0.0f;
    m_nBikeFrontWheelCounter = 0;
    m_fBikeFrontWheelDist = 0.0f;
    m_nTempBufferCounter = 0;
    m_nBestCarTwoWheelsTimeMs = 0;
    m_fBestCarTwoWheelsDistM = 0.0f;
    m_nBestBikeWheelieTimeMs = 0;
    m_fBestBikeWheelieDistM = 0.0f;
    m_nBestBikeStoppieTimeMs = 0;
    m_fBestBikeStoppieDistM = 0.0f;
    m_bDoesNotGetTired = false;
    m_bFastReload = false;
    m_bFireProof = false;
    m_bGetOutOfJailFree = false;
    m_bFreeHealthCare = false;
    m_nTimeOfLastCarExplosionCaused = 0;
    m_nExplosionMultiplier = 0;
    m_nHavocCaused = 0;
    FindPlayerInfo().m_nNumHoursDidntEat = 0;
    m_nLastBustMessageNumber = 1;
    m_fCurrentChaseValue = 0.0f;
    m_nBustedAudioStatus = 0;
    m_nCrosshairActivated = 0;

    m_nRequireParachuteTimer = 0;

    if (m_bParachuteReferenced) {
        CGameLogic::IsCoopGameGoingOn();
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
    }
}

// 0x56EC40
void CPlayerInfo::GivePlayerParachute() const {
    if (m_nRequireParachuteTimer) {
        if (CStreaming::IsModelLoaded(MODEL_GUN_PARA)) {
            m_pPed->GiveWeapon(WEAPON_PARACHUTE, 1, true);
            m_pPed->SetSavedWeapon(WEAPON_PARACHUTE);
        }
    }
}

// 0x56EB30
void CPlayerInfo::StreamParachuteWeapon(bool unk) {
    if (CGameLogic::IsCoopGameGoingOn()) {
        if (unk) {
            return;
        }
        if (m_bParachuteReferenced) {
            CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
            m_bParachuteReferenced = false;
            m_nRequireParachuteTimer = 0;
        }
        return;
    }

    if (m_pPed && m_pPed->IsInVehicle()) {
        if (m_pPed->m_pVehicle->IsSubPlane() || m_pPed->m_pVehicle->IsSubHeli()) {
            if (m_nRequireParachuteTimer <= (uint32)CTimer::GetTimeStepInMS()) {
                const auto groundHeight = TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
                const auto vehToGroundZDist = m_pPed->m_pVehicle->GetPosition().z - groundHeight;
                m_nRequireParachuteTimer = (vehToGroundZDist <= 50.f) ? 0 : 5000;
            } else {
                m_nRequireParachuteTimer -= (uint32)CTimer::GetTimeStepInMS();
            }
        }
    }

    if (m_nRequireParachuteTimer) {
        CStreaming::RequestModel(MODEL_GUN_PARA, STREAMING_MISSION_REQUIRED);
        m_bParachuteReferenced = true;
        return;
    }

    if (m_bParachuteReferenced) {
        CStreaming::SetModelIsDeletable(MODEL_GUN_PARA);
        m_bParachuteReferenced = false;
        m_nRequireParachuteTimer = 0;
    }
}

// 0x56EAB0
void CPlayerInfo::AddHealth(int32 amount) const {
    const auto newValue = std::min((float)m_nMaxHealth, m_pPed->m_fHealth + (float)amount); // Clamp to m_nMaxHealth
    m_pPed->m_fHealth = std::max(newValue, m_pPed->m_fHealth); // Don't change health to a lower value
}

// 0x56EA30
void CPlayerInfo::BlowUpRCBuggy(bool bExplode) const {
    if (m_pRemoteVehicle && !m_pRemoteVehicle->m_bRemoveFromWorld) {
        CRemote::TakeRemoteControlledCarFromPlayer(bExplode);
        if (bExplode)
            m_pRemoteVehicle->BlowUpCar(FindPlayerPed(), false);
    }
}

// 0x56E870
void CPlayerInfo::MakePlayerSafe(bool enable, float radius) {
    // Not quite SA, but this is the way to do it (instead of copy pasting it twice)
    auto& flags = m_pPed->physicalFlags;
    flags.bInvulnerable = enable;
    flags.bBulletProof = enable;
    flags.bFireProof = enable;
    flags.bExplosionProof = enable;
    flags.bCollisionProof = enable;
    flags.bMeleeProof = enable;
    m_PlayerData.m_bCanBeDamaged = !enable;
    m_PlayerData.m_pWanted->m_bEverybodyBackOff = enable;
    m_pPed->GetPadFromPlayer()->bPlayerSafe = enable;
    CWorld::SetAllCarsCanBeDamaged(!enable);

    if (enable) {
        CWorld::StopAllLawEnforcersInTheirTracks();
        CPad::StopPadsShaking();

        m_pPed->ClearAdrenaline();
        m_PlayerData.m_fTimeCanRun = std::max(m_PlayerData.m_fTimeCanRun, 0.f);
        m_pPed->GetIntelligence()->ClearTasks(true, false);

        gFireManager.ExtinguishPoint(GetPos(), radius);
        CExplosion::RemoveAllExplosionsInArea(GetPos(), 4000.f);
        CProjectileInfo::RemoveAllProjectiles();
        CWorld::ExtinguishAllCarFiresInArea(GetPos(), radius);
        CReplay::DisableReplays();
        m_pPed->ClearWeaponTarget();
    } else {
        CReplay::EnableReplays();
    }
}

// 0x56E830
void CPlayerInfo::PlayerFailedCriticalMission() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_FAILED_MISSION;
        CGameLogic::SetMissionFailed();
        CDarkel::ResetOnPlayerDeath();
    }
}

// 0x56E610
void CPlayerInfo::WorkOutEnergyFromHunger() {

    static auto& s_lastTimeHungryStateProcessedInitialized = StaticRef<bool>(0xB9B8F4); // false
    static auto& s_lastTimeHungryStateProcessed = StaticRef<uint8>(0xB9B8F2);
    static auto& s_LastHungryState = StaticRef<int8>(0xB9B8F1);
    static auto& s_bHungryMessageShown = StaticRef<bool>(0xB9B8F0);

    if (CCheat::IsActive(CHEAT_NEVER_GET_HUNGRY)) {
        return;
    }

    if (!s_lastTimeHungryStateProcessedInitialized) {
        s_lastTimeHungryStateProcessedInitialized = true;
        s_lastTimeHungryStateProcessed = CClock::GetGameClockHours();
    }

    auto pad = CPad::GetPad();
    if (   pad->ArePlayerControlsDisabled()
        || CMenuSystem::num_menus_in_use
        || TheCamera.m_bWideScreenOn
        || CCutsceneMgr::ms_running
        || CGameLogic::IsCoopGameGoingOn()
        || m_pRemoteVehicle
    ) {
        return;
    }

    if (!m_pPed)
        return;

    if (m_pPed->m_pAttachedTo)
        return;

    if (CClock::GetGameClockHours() != s_lastTimeHungryStateProcessed) {
        if (!m_nNumHoursDidntEat)
            s_LastHungryState = 0;
        m_nNumHoursDidntEat += 1;
    }

    if (m_nNumHoursDidntEat <= 48) {
        s_bHungryMessageShown = false;
    } else {
        if (CClock::GetGameClockHours() == s_lastTimeHungryStateProcessed)
            return;

        m_pPed->Say(CTX_GLOBAL_STOMACH_RUMBLE);
        pad->StartShake(400, 110u, 0);

        if (s_bHungryMessageShown) {
            bool bDecreaseHealth{};
            if (CStats::GetStatValue(STAT_FAT) > 0.0f) {
                CStats::DecrementStat(STAT_FAT, 25.0f);
                CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_FAT, 25.0f);
                bDecreaseHealth = true;
                if (!s_LastHungryState) {
                    s_LastHungryState = m_nNumHoursDidntEat + 24;
                }
            }

            if (CStats::GetStatValue(STAT_MUSCLE) <= 0.0f || m_nNumHoursDidntEat <= s_LastHungryState && s_LastHungryState) {
                if (!bDecreaseHealth) {
                    m_pPed->m_fHealth -= 2.0f;
                }
            } else {
                CStats::DecrementStat(STAT_MUSCLE, 25.0);
                CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_DECREASE, STAT_MUSCLE, 25.0f);
            }
        } else {
            CHud::SetHelpMessage(TheText.Get("NOTEAT"), true, false, true);
            s_bHungryMessageShown = true;
        }
    }

    if (CClock::GetGameClockHours() != s_lastTimeHungryStateProcessed) {
        s_lastTimeHungryStateProcessed = CClock::GetGameClockHours();
    }
}

// 0x56E5D0
void CPlayerInfo::ArrestPlayer() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_HAS_BEEN_ARRESTED;
        m_nBustedAudioStatus = 0;
        CDarkel::ResetOnPlayerDeath();
        CStats::IncrementStat(STAT_TIMES_BUSTED, 1.0f);
        CGangWars::EndGangWar(false);
    }
}

// 0x56E580
void CPlayerInfo::KillPlayer() {
    if (m_nPlayerState == PLAYERSTATE_PLAYING) {
        m_nPlayerState = PLAYERSTATE_HAS_DIED;
        CDarkel::ResetOnPlayerDeath();
        CMessages::AddBigMessage(TheText.Get("DEAD"), 4000, STYLE_WHITE_MIDDLE);
        CStats::IncrementStat(STAT_NUMBER_OF_HOSPITAL_VISITS, 1.0f);
        CGangWars::EndGangWar(false);
    }
}

// 0x56E570
bool CPlayerInfo::IsRestartingAfterMissionFailed() const {
    return m_nPlayerState == PLAYERSTATE_FAILED_MISSION;
}

// 0x56E560
bool CPlayerInfo::IsRestartingAfterArrest() const {
    return m_nPlayerState == PLAYERSTATE_HAS_BEEN_ARRESTED;
}

// 0x56E550
bool CPlayerInfo::IsRestartingAfterDeath() const {
    return m_nPlayerState == PLAYERSTATE_HAS_DIED;
}

// 0x56DAB0
bool CPlayerInfo::IsPlayerInRemoteMode() const {
    return m_pRemoteVehicle || m_bAfterRemoteVehicleExplosion;
}

// 0x56DFB0
// Return occupied vehicle's (if in any) or player's ped position
CVector CPlayerInfo::GetPos() const {
    return m_pPed->IsInVehicle() ? m_pPed->m_pVehicle->GetPosition() : m_pPed->GetPosition();
}

// 0x56DF50
// Return occupied vehicle's (if in any) or player's ped move speed
CVector CPlayerInfo::GetSpeed() const {
    return m_pPed->IsInVehicle() ? m_pPed->m_pVehicle->GetMoveSpeed() : m_pPed->GetMoveSpeed();
}

// 0x5D3B00
bool CPlayerInfo::Load() {
    CGenericGameStorage::LoadDataFromWorkBuffer<int32>(); // Discarded
    auto data = CGenericGameStorage::LoadDataFromWorkBuffer<CPlayerInfoSaveStructure>();
    data.Extract(this);
    return true;
}

// 0x5D3AC0
bool CPlayerInfo::Save() {
    CPlayerInfoSaveStructure data;
    data.Construct(this);
    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(CPlayerInfoSaveStructure));
    CGenericGameStorage::SaveDataToWorkBuffer(data);
    return true;
}

// 0x45DEF0
CPlayerInfo& CPlayerInfo::operator=(const CPlayerInfo& rhs) {
    m_pPed                             = rhs.m_pPed;
    m_PlayerData                       = rhs.m_PlayerData;
    m_pRemoteVehicle                   = rhs.m_pRemoteVehicle;
    m_pSpecCar                         = rhs.m_pSpecCar;
    m_nMoney                           = rhs.m_nMoney;
    m_nDisplayMoney                    = rhs.m_nDisplayMoney;
    m_nCollectablesPickedUp            = rhs.m_nCollectablesPickedUp;
    m_nTotalNumCollectables            = rhs.m_nTotalNumCollectables;
    m_nLastBumpPlayerCarTimer          = rhs.m_nLastBumpPlayerCarTimer;
    m_nTaxiTimer                       = rhs.m_nTaxiTimer;
    m_nVehicleTimeCounter              = rhs.m_nVehicleTimeCounter;
    m_bTaxiTimerScore                  = rhs.m_bTaxiTimerScore;
    m_bTryingToExitCar                 = rhs.m_bTryingToExitCar;
    m_pLastTargetVehicle               = rhs.m_pLastTargetVehicle;
    m_nPlayerState                     = rhs.m_nPlayerState;
    m_bAfterRemoteVehicleExplosion     = rhs.m_bAfterRemoteVehicleExplosion;
    m_bCreateRemoteVehicleExplosion    = rhs.m_bCreateRemoteVehicleExplosion;
    m_bFadeAfterRemoteVehicleExplosion = rhs.m_bFadeAfterRemoteVehicleExplosion;
    m_nTimeOfRemoteVehicleExplosion    = rhs.m_nTimeOfRemoteVehicleExplosion;
    m_nLastTimeEnergyLost              = rhs.m_nLastTimeEnergyLost;
    m_nLastTimeArmourLost              = rhs.m_nLastTimeArmourLost;
    m_nLastTimeBigGunFired             = rhs.m_nLastTimeBigGunFired;
    m_nTimesUpsideDownInARow           = rhs.m_nTimesUpsideDownInARow;
    m_nTimesStuckInARow                = rhs.m_nTimesStuckInARow;
    m_nCarTwoWheelCounter              = rhs.m_nCarTwoWheelCounter;
    m_fCarTwoWheelDist                 = rhs.m_fCarTwoWheelDist;
    m_nCarLess3WheelCounter            = rhs.m_nCarLess3WheelCounter;
    m_nBikeRearWheelCounter            = rhs.m_nBikeRearWheelCounter;
    m_fBikeRearWheelDist               = rhs.m_fBikeRearWheelDist;
    m_nBikeFrontWheelCounter           = rhs.m_nBikeFrontWheelCounter;
    m_fBikeFrontWheelDist              = rhs.m_fBikeFrontWheelDist;
    m_nTempBufferCounter               = rhs.m_nTempBufferCounter;
    m_nBestCarTwoWheelsTimeMs          = rhs.m_nBestCarTwoWheelsTimeMs;
    m_fBestCarTwoWheelsDistM           = rhs.m_fBestCarTwoWheelsDistM;
    m_nBestBikeWheelieTimeMs           = rhs.m_nBestBikeWheelieTimeMs;
    m_fBestBikeWheelieDistM            = rhs.m_fBestBikeWheelieDistM;
    m_nBestBikeStoppieTimeMs           = rhs.m_nBestBikeStoppieTimeMs;
    m_fBestBikeStoppieDistM            = rhs.m_fBestBikeStoppieDistM;
    m_nCarDensityForCurrentZone        = rhs.m_nCarDensityForCurrentZone;
    m_fRoadDensityAroundPlayer         = rhs.m_fRoadDensityAroundPlayer;
    m_nTimeOfLastCarExplosionCaused    = rhs.m_nTimeOfLastCarExplosionCaused;
    m_nExplosionMultiplier             = rhs.m_nExplosionMultiplier;
    m_nHavocCaused                     = rhs.m_nHavocCaused;
    m_nNumHoursDidntEat                = rhs.m_nNumHoursDidntEat;
    m_fCurrentChaseValue               = rhs.m_fCurrentChaseValue;
    m_bDoesNotGetTired                 = rhs.m_bDoesNotGetTired;
    m_bFastReload                      = rhs.m_bFastReload;
    m_bFireProof                       = rhs.m_bFireProof;
    m_nMaxHealth                       = rhs.m_nMaxHealth;
    m_nMaxArmour                       = rhs.m_nMaxArmour;
    m_bGetOutOfJailFree                = rhs.m_bGetOutOfJailFree;
    m_bFreeHealthCare                  = rhs.m_bFreeHealthCare;
    m_bCanDoDriveBy                    = rhs.m_bCanDoDriveBy;
    m_nBustedAudioStatus               = rhs.m_nBustedAudioStatus;
    m_nLastBustMessageNumber           = rhs.m_nLastBustMessageNumber;
    m_nCrosshairActivated              = rhs.m_nCrosshairActivated;
    m_vecCrosshairTarget               = rhs.m_vecCrosshairTarget;
    m_pSkinTexture                     = rhs.m_pSkinTexture;
    m_bParachuteReferenced             = rhs.m_bParachuteReferenced;
    m_nRequireParachuteTimer           = rhs.m_nRequireParachuteTimer;
    strcpy_s(m_szSkinName, rhs.m_szSkinName);
    return *this;
}
