#include "StdInc.h"
#include "Automobile.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexBeCop.h"

void CSetPiece::InjectHooks() {
    RH_ScopedClass(CSetPiece);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(TryToGenerateCopPed, 0x499690);
    RH_ScopedInstall(TryToGenerateCopCar, 0x4998A0);
    RH_ScopedInstall(Update, 0x499A80);
}

// 0x499690
CCopPed* CSetPiece::TryToGenerateCopPed(CVector2D posn) {
    CVector   point{ posn, 1000.0f };
    CColPoint cp{};
    CEntity*  entity{};
    if (CWorld::ProcessVerticalLine(
        point,
        -1000.0f,
        cp,
        entity,
        true,
        false,
        false,
        false,
        true
    )) {
        point.z = cp.m_vecPoint.z + 0.9f;
    }

    int16 numCollidingObjects{};
    CWorld::FindObjectsKindaColliding(
        point,
        0.6f,
        false,
        &numCollidingObjects,
        16,
        nullptr,
        false,
        true,
        true,
        false,
        false
    );

    if (numCollidingObjects) { // exe 0x499731: something is in the way
        return nullptr;
    }

    auto* cop = new CCopPed(0);
    cop->SetPosn(point);

    NOTSA_LOG_DEBUG("generated cop at ({}, {}, {}) dist to player {:.2f}", cop->GetPosition().x, cop->GetPosition().y, cop->GetPosition().z, DistanceBetweenPoints(FindPlayerCoors(), cop->GetPosition()));
    CWorld::Add(cop);
    return cop;
}

// 0x4998A0
CVehicle* CSetPiece::TryToGenerateCopCar(CVector2D posn, CVector2D target) {
    const auto carModel = CStreaming::GetDefaultCopCarModel();
    if (carModel < 0) {
        return nullptr;
    }

    auto* car = new CAutomobile(carModel, RANDOM_VEHICLE, true);

    CVector point{ posn, 1000.0f };
    CColPoint cp{};
    CEntity*  entity{};
    if (CWorld::ProcessVerticalLine(
        point,
        -1000.0f,
        cp,
        entity,
        true,
        false,
        false,
        false,
        true
    )) {
        point.z = car->GetHeightAboveRoad() + cp.m_vecPoint.z;
    }

    const CVector dir{ (target - posn).Normalized() };
    // TODO: matrix assign shit
    car->GetForward()  = dir;
    car->GetRight()    = CVector{ dir.y, -dir.x, 0.0f };
    car->GetUp()       = CVector::ZAxisVector();
    car->GetPosition() = point;

    int16 numCollidingObjects{};
    CWorld::FindObjectsKindaColliding(
        point,
        car->GetColModel()->GetBoundRadius(),
        false,
        &numCollidingObjects,
        16,
        nullptr,
        false,
        true,
        true,
        false,
        false
    );

    if (!numCollidingObjects) {
        car->ChangeLawEnforcerState(true);
        NOTSA_LOG_DEBUG("generated cop car at ({}, {}, {}) dist to player {:.2f}", car->GetPosition().x, car->GetPosition().y, car->GetPosition().z, DistanceBetweenPoints(FindPlayerCoors(), car->GetPosition()));
        CWorld::Add(car);
    } else {
        delete std::exchange(car, nullptr);
    }

    return car;
}

// 0x499A80
void CSetPiece::Update() {
    // exe 0x499AB0: 40 s (0x9C40) cool-down
    if (m_nLastGenerationTime != 0 && CTimer::GetTimeInMS() <= (uint32)m_nLastGenerationTime + 40'000u) {
        return;
    }

    const auto playerPos = FindPlayerCoors();
    if (!CRect{ m_AreaRect }.IsPointInside(playerPos)) {
        return;
    }

    // Creates the cop and gives him his task (exe 0x49A02D...). Doesn't touch the generation time.
    const auto SetupCop = [this](const CVector2D& spawn, const CVector2D& target) -> CCopPed* {
        if (auto* cop = TryToGenerateCopPed(spawn)) {
            cop->bCullExtraFarAway = true;
            cop->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS() + 10'000;

            cop->GetTaskManager().SetTask(new CTaskComplexBeCop{
                PEDMOVE_WALK,
                false,
                new CTaskComplexGoToPointAndStandStill{
                    PEDMOVE_RUN,
                    CWorld::AddGroundZToCoord(target),
                    0.5f
                }
            }, TASK_PRIMARY_PRIMARY);

            return cop;
        }
        return nullptr;
    };

    // The slowly approaching cars (exe 0x499BB4..): cruise speed, destination, no temp action / initial speed
    const auto FinishSlowCar = [](CVehicle* car, const CVector2D& target, uint8 speed) {
        car->SetStatus(STATUS_SIMPLE);
        car->m_autoPilot.m_nCruiseSpeed        = speed;
        car->m_autoPilot.m_nCarDrivingStyle    = DRIVING_STYLE_SLOW_DOWN_FOR_CARS;
        car->m_autoPilot.m_nCarMission         = MISSION_SLOWLY_DRIVE_TOWARDS_PLAYER_1;
        car->m_autoPilot.m_speed               = car->m_autoPilot.m_nCruiseSpeed;
        car->m_autoPilot.m_vecDestinationCoors = CVector{ target };
        car->m_nTimeTillWeNeedThisCar          = CTimer::GetTimeInMS() + 25'000;
        CCarAI::AddPoliceCarOccupants(car, false);
    };

    // The quick cars (exe 0x499E40..): NO initial speed / destination, a go-forward temp action and a push forward
    const auto SetupQuickCar = [&](const CVector2D& spawn, const CVector2D& target, eCarDrivingStyle style, eCarMission mission) {
        // exe: only comes from the front of the player, `(playerPos - spawn) . playerSpeed` is a 2D dot (x/y), strictly negative
        {
            const auto& pos   = FindPlayerCoors();
            const auto& speed = FindPlayerSpeed();
            const float dot   = (pos.y - spawn.y) * speed.y + (pos.x - spawn.x) * speed.x;
            if (!(dot < 0.0f)) {
                return;
            }
        }

        auto* const car = TryToGenerateCopCar(spawn, target);
        if (!car) {
            return;
        }

        car->SetStatus(STATUS_SIMPLE);
        car->m_autoPilot.m_nCruiseSpeed     = 16;
        car->m_autoPilot.m_nCarDrivingStyle = style;
        car->m_autoPilot.m_nCarMission      = mission;
        car->m_autoPilot.SetTempAction(TEMPACT_GOFORWARD, 100);
        car->m_nTimeTillWeNeedThisCar       = CTimer::GetTimeInMS() + 10'000;
        car->GetMoveSpeed()                 = car->GetForward() * (2.0f / 3.0f); // 0x3F2AAAAB
        CCarAI::AddPoliceCarOccupants(car, false);
        m_nLastGenerationTime = CTimer::GetTimeInMS();
    };

    const auto PlayerWantedLevel = [] { return FindPlayerPed()->GetWantedLevel(); };

    switch (m_nType) {
    case SETPIECE_2CARS_SLOW_SPEED:
    case SETPIECE_2CARS_MEDIUM_SPEED: {
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_1 || FindPlayerVehicle()) {
            break;
        }

        auto* car1 = TryToGenerateCopCar(GetSpawnCoord1(), GetTargetCoord1());
        if (!car1) {
            break;
        }

        auto* car2 = TryToGenerateCopCar(GetSpawnCoord2(), GetTargetCoord2());
        if (!car2) {
            CWorld::Remove(car1);
            delete car1;
            break;
        }

        // Both exist => set them up (the first one gets its occupants before the 2nd is touched, exe 0x499BCF-0x499D5F)
        const uint8 speed = (m_nType == SETPIECE_2CARS_SLOW_SPEED) ? 4 : 24;
        FinishSlowCar(car1, GetTargetCoord1(), speed);
        FinishSlowCar(car2, GetTargetCoord2(), speed);

        m_nLastGenerationTime = CTimer::GetTimeInMS();
        break;
    }
    case SETPIECE_1CAR_QUICK_SPEED: {
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_2 || !FindPlayerVehicle()) {
            break;
        }

        SetupQuickCar(GetSpawnCoord1(), GetTargetCoord1(), DRIVING_STYLE_PLOUGH_THROUGH, MISSION_BLOCKPLAYER_FORWARDANDBACK);
        break;
    }
    case SETPIECE_1CAR_MEDIUM_SPEED: {
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_2 || !FindPlayerVehicle()) {
            break;
        }

        SetupQuickCar(GetSpawnCoord1(), GetTargetCoord1(), DRIVING_STYLE_AVOID_CARS, MISSION_RAMPLAYER_CLOSE);
        break;
    }
    case SETPIECE_1PED:
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_1 || FindPlayerVehicle()) {
            break;
        }

        if (!SetupCop(GetSpawnCoord1(), GetTargetCoord1())) {
            break;
        }
        m_nLastGenerationTime = CTimer::GetTimeInMS();
        break;
    case SETPIECE_2PEDS: {
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_1 || FindPlayerVehicle()) {
            break;
        }

        auto* cop1 = SetupCop(GetSpawnCoord1(), GetTargetCoord1());
        if (!cop1) {
            break;
        }

        if (!SetupCop(GetSpawnCoord2(), GetTargetCoord2())) {
            CWorld::Remove(cop1);
            delete cop1;
            break;
        }
        m_nLastGenerationTime = CTimer::GetTimeInMS();
        break;
    }
    case SETPIECE_2CARS_QUICK_SPEED_BEFORE_HIT: {
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_2 || !FindPlayerVehicle()) {
            break;
        }

        // NOTE: No check of the first car
        SetupQuickCar(GetSpawnCoord1(), GetTargetCoord1(), DRIVING_STYLE_PLOUGH_THROUGH, MISSION_BLOCKPLAYER_FORWARDANDBACK);
        SetupQuickCar(GetSpawnCoord2(), GetTargetCoord2(), DRIVING_STYLE_PLOUGH_THROUGH, MISSION_BLOCKPLAYER_FORWARDANDBACK);
        break;
    }
    case SETPIECE_2CARS_QUICK_SPEED: {
        if (PlayerWantedLevel() < eWantedLevel::WANTED_LEVEL_2 || !FindPlayerVehicle()) {
            break;
        }

        // NOTE: No check of the first car
        SetupQuickCar(GetSpawnCoord1(), GetTargetCoord1(), DRIVING_STYLE_AVOID_CARS, MISSION_RAMPLAYER_CLOSE);
        SetupQuickCar(GetSpawnCoord2(), GetTargetCoord2(), DRIVING_STYLE_AVOID_CARS, MISSION_RAMPLAYER_CLOSE);
        break;
    }
    default:
        break;
    }
}
