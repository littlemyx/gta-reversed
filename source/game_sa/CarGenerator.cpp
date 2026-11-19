#include "StdInc.h"

#include "CarGenerator.h"

#include "TheCarGenerators.h"
#include "Occlusion.h"
#include "CarCtrl.h"
#include "TheScripts.h"
#include "portable/cargenlog.h"
#include "portable/statehash.h"

void CCarGenerator::InjectHooks()
{
    RH_ScopedClass(CCarGenerator);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(CheckForBlockage, 0x6F32E0);
    RH_ScopedInstall(CheckIfWithinRangeOfAnyPlayers, 0x6F2F40);
    RH_ScopedInstall(DoInternalProcessing, 0x6F34D0);
    RH_ScopedInstall(Process, 0x6F3E90);
    RH_ScopedInstall(Setup, 0x6F2E50);
    RH_ScopedInstall(SwitchOff, 0x6F2E30);
    RH_ScopedInstall(SwitchOn, 0x6F32C0);
    RH_ScopedInstall(CalcNextGen, 0x6F2E40);
}

// 0x6F32E0
bool CCarGenerator::CheckForBlockage(int32 modelId) {
    auto colModel = CModelInfo::GetModelInfo(modelId)->GetColModel();
    float radius = colModel ? colModel->GetBoundRadius() : 2.0f;

    int16 entityCount;
    CEntity* objects[8];

    CVector posn = m_vecPosn;
    CWorld::FindObjectsKindaColliding(posn, radius, true, &entityCount, 8, objects, false, true, true, false, false);

    for (auto& obj : std::span{ objects, (size_t)entityCount }) {
        auto entityColModel = obj->GetModelInfo()->GetColModel();
        if (colModel)
        {
            if (obj->GetPosition().z + entityColModel->GetBoundingBox().m_vecMax.z + 1.0f > posn.z + colModel->GetBoundingBox().m_vecMin.z &&
                obj->GetPosition().z + entityColModel->GetBoundingBox().m_vecMin.z - 1.0f < posn.z + colModel->GetBoundingBox().m_vecMax.z
            ) {
                bWaitUntilFarFromPlayer = true;
                return true;
            }
        }
        else
        {
            if (obj->GetPosition().z + entityColModel->GetBoundingBox().m_vecMax.z + 1.0f > posn.z - 1.0f &&
                obj->GetPosition().z + entityColModel->GetBoundingBox().m_vecMin.z - 1.0f < posn.z + 1.0f
            ) {
                bWaitUntilFarFromPlayer = true;
                return true;
            }
        }
    }

    return false;
}

// 0x6F2F40
bool CCarGenerator::CheckIfWithinRangeOfAnyPlayers()
{
    bool bVisible = false;
    CVector relPosn = FindPlayerCentreOfWorld(CWorld::PlayerInFocus) - m_vecPosn;
    if (fabs(relPosn.z) > 50.0f)
        return false;

    if (m_nModelId > 0 &&
        CModelInfo::GetModelInfo(m_nModelId)->AsVehicleModelInfoPtr()->IsBoat() &&
        relPosn.Magnitude2D() < TheCamera.m_fGenerationDistMultiplier * 240.0f)
    {
        CVector origin = m_vecPosn;
        if (TheCamera.IsSphereVisible(origin, 0.0f) && !COcclusion::IsPositionOccluded(origin, 0.0f))
        {
            bVisible = true;
        }
    }

    if (!(relPosn.Magnitude2D() < TheCamera.m_fGenerationDistMultiplier * 160.0f) && !bVisible) // exe 0x6F30C5: `dist < lim` fails for NaN too
    {
        bWaitUntilFarFromPlayer = false;
        return false;
    }

    if (CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter)
        return true;

    if (bWaitUntilFarFromPlayer)
        return false;

    float posnZ = m_vecPosn.z;
    if ((CGame::CanSeeOutSideFromCurrArea() && posnZ < 950.0f || !CGame::CanSeeOutSideFromCurrArea() && posnZ >= 950.0f)
        && (!(relPosn.Magnitude2D() < TheCamera.m_fGenerationDistMultiplier * 160.0f - 20.0f) || bHighPriority) // exe 0x6F3145: NaN arms
        && !(DotProduct2D(FindPlayerSpeed(-1), relPosn) > 0.0f)
    ) {
        return true;
    }

    return false;
}

// 0x6F34D0
static const char* g_cgGenWhy = "-"; // SH3 NOTSA_SH_CARGENLOG: set before every return of DoInternalProcessing
void CCarGenerator::DoInternalProcessing()
{
    int32 actualModelId;
    CVehicle* vehicle;
    float baseZ;
    CColPoint colPoint{};
    CEntity* entity;
    tCarGenPlateText plate{};
    int32 tractorDriverPedType;

    // exe 0x6F34E8: hours > 21 || hours < 7 (the range helper, with start > end, was always false)
    const bool nightTime = CClock::GetGameClockHours() > 21 || CClock::GetGameClockHours() < 7;
    // the exe compares the counter signed (jge)
    if (!bIgnorePopulationLimit
        && (nightTime && (int32)CCarCtrl::NumParkedCars >= 10 || !nightTime && (int32)CCarCtrl::NumParkedCars >= 5)
    ) {
        g_cgGenWhy = "parked_limit";
        return;
    }

    if (m_nModelId >= 0)
    {
        if (CheckForBlockage(m_nModelId))
        {
            m_nNextGenTime += 4;
            g_cgGenWhy = "blockage_known_model";
            return;
        }

        CStreaming::RequestModel(m_nModelId, STREAMING_KEEP_IN_MEMORY);
        actualModelId = m_nModelId;
        if (m_nModelId == MODEL_HOTDOG)
            CStreaming::RequestModel(MODEL_BMOCHIL, STREAMING_KEEP_IN_MEMORY);
    }
    else
    {
        if (m_nModelId == MODEL_INVALID || !CStreaming::ms_aInfoForModel[-m_nModelId].IsLoaded()) // todo: Figure out what they wanted with negative indexing
        {
            CTheZones::GetZoneInfo(FindPlayerCoors(-1), nullptr);
            actualModelId = CPopulation::m_AppropriateLoadedCars.PickRandomCar(true, true);

            if (actualModelId < 0) {
                g_cgGenWhy = "no_appropriate_model";
                return;
            }

            auto mi = CModelInfo::GetModelInfo(actualModelId)->AsVehicleModelInfoPtr();
            // exe 0x6F359D: byte [mi + 0x4D] == 5, the vehicle class (VEHICLE_CLASS_BIG), not the boat type
            if (mi->m_nVehicleClass == VEHICLE_CLASS_BIG) {
                g_cgGenWhy = "vehicle_class_big";
                return;
            }

            // exe 0x6F35A6: the colmodel is optional (no bound check without it); length is max.y - min.y, strictly greater than 8
            if (auto* const col = mi->GetColModel()) {
                if (col->GetBoundingBox().GetLength() > 8.0f) {
                    g_cgGenWhy = "too_long";
                    return;
                }
            }

            m_nModelId = -actualModelId;
            m_nPrimaryColor = -1;
            m_nSecondaryColor = -1;
        }
        else
        {
            actualModelId = -m_nModelId;
        }

        if (CheckForBlockage(actualModelId))
        {
            m_nNextGenTime += 4;
            g_cgGenWhy = "blockage_random_model";
            return;
        }
    }

    if (!CStreaming::GetInfo(actualModelId).IsLoaded() ||
        actualModelId == MODEL_HOTDOG &&
        m_nModelId == MODEL_HOTDOG &&
        !CStreaming::GetInfo(MODEL_BMOCHIL).IsLoaded()
    ) {
        g_cgGenWhy = "model_not_loaded";
        return;
    }

    if (CModelInfo::IsBoatModel(actualModelId) ||
        actualModelId == MODEL_LEVIATHN ||
        actualModelId == MODEL_SEASPAR ||
        actualModelId == MODEL_SKIMMER)
    {
        switch (CModelInfo::GetModelInfo(actualModelId)->AsVehicleModelInfoPtr()->m_nVehicleType)
        {
        case VEHICLE_TYPE_HELI:
            vehicle = new CHeli(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_PLANE:
            vehicle = new CPlane(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_BOAT:
            vehicle = new CBoat(actualModelId, PARKED_VEHICLE);
            break;
        }

        CVector posn = m_vecPosn;
        baseZ = posn.z;
        if (baseZ <= MAP_Z_LOW_LIMIT)
            baseZ = CWorld::FindGroundZForCoord(posn.x, posn.y);
        vehicle->m_nExtendedRemovalRange = 255;
    }
    else
    {
        CVector posn = m_vecPosn;
        if (posn.z > MAP_Z_LOW_LIMIT)
            posn.z += 1.0f;
        else
            posn.z = 1000.0f;

        if (!CWorld::ProcessVerticalLine(posn, -1000.0f, colPoint, entity, true, false, false, false, false, false, nullptr)) {
            g_cgGenWhy = "no_ground_below";
            return;
        }

        baseZ = colPoint.m_vecPoint.z;
        plate.m_nCarGenId = CTheCarGenerators::GetIndex(this);
        CTheCarGenerators::m_SpecialPlateHandler.Find(plate.m_nCarGenId, plate.m_szPlateText);
        if (plate.m_szPlateText[0])
        {
            auto mi = CModelInfo::GetModelInfo(actualModelId)->AsVehicleModelInfoPtr();
            if (mi->m_pPlateMaterial)
                mi->SetCustomCarPlateText(plate.m_szPlateText);
        }

        switch (CModelInfo::GetModelInfo(actualModelId)->AsVehicleModelInfoPtr()->m_nVehicleType)
        {
        case VEHICLE_TYPE_MTRUCK:
            vehicle = new CMonsterTruck(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_QUAD:
            vehicle = new CQuadBike(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_HELI:
            vehicle = new CHeli(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_PLANE:
            vehicle = new CPlane(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_TRAIN:
            vehicle = new CTrain(actualModelId, PARKED_VEHICLE);
            break;
        case VEHICLE_TYPE_BIKE:
            vehicle = new CBike(actualModelId, PARKED_VEHICLE);
            vehicle->AsBike()->bikeFlags.bOnSideStand = true;
            break;
        case VEHICLE_TYPE_BMX:
            vehicle = new CBmx(actualModelId, PARKED_VEHICLE);
            vehicle->AsBike()->bikeFlags.bOnSideStand = true;
            break;
        case VEHICLE_TYPE_TRAILER:
            vehicle = new CTrailer(actualModelId, PARKED_VEHICLE);
            break;
        default:
            vehicle = new CAutomobile(actualModelId, PARKED_VEHICLE, true);
            break;
        }

        vehicle->GetDistanceFromCentreOfMassToBaseOfModel();
        vehicle->vehicleFlags.bLightsOn = false;
        vehicle->m_nOverrideLights = NO_CAR_LIGHT_OVERRIDE;
        vehicle->SetCollisionLighting(colPoint.m_nLightingB);
    }

    vehicle->SetIsStatic(false);
    vehicle->vehicleFlags.bEngineOn = false;
    CVector posn = m_vecPosn;
    posn.z = vehicle->GetDistanceFromCentreOfMassToBaseOfModel() + baseZ;
    vehicle->SetPosn(posn);
    vehicle->SetOrientation(0.0f, 0.0f, (uint8)m_nAngle * (TWO_PI / 256));
    vehicle->SetStatus(STATUS_ABANDONED);
    vehicle->m_nDoorLock = eCarLock::CARLOCK_UNLOCKED;
    vehicle->vehicleFlags.bHasBeenOwnedByPlayer = bPlayerHasAlreadyOwnedCar;
    tractorDriverPedType = -1;

    if (!nightTime &&
        (vehicle->GetModelIndex() == MODEL_TRACTOR || vehicle->GetModelIndex() == MODEL_COMBINE))
    {
        // 0x6F3BF4

        CVector vehPosn = vehicle->GetPosition();
        CNodeAddress pathLink = ThePaths.FindNodeClosestToCoors(vehPosn, PATH_TYPE_VEH, 20.0F, 0, 0, 0, 0, 1);
        if (pathLink.IsValid())
        {
            assert(pathLink.IsValid());

            CPathNode& pathNode = ThePaths.m_pPathNodes[pathLink.m_wAreaId][pathLink.m_wNodeId];
            if (pathNode.m_nNumLinks != 0)
            {
                CNodeAddress baseLink = ThePaths.m_pNodeLinks[pathLink.m_wAreaId][pathNode.m_wBaseLinkId];
                if (ThePaths.m_pPathNodes[baseLink.m_wAreaId])
                {
                    vehicle->SetVehicleCreatedBy(RANDOM_VEHICLE);
                    vehicle->m_autoPilot.m_currentAddress = pathLink;
                    vehicle->m_autoPilot.SetCruiseSpeed(7);
                    vehicle->m_autoPilot.SetCarMission(eCarMission::MISSION_CRUISE);
                    vehicle->m_autoPilot.m_startingRouteNode = baseLink;
                    vehicle->SetStatus(STATUS_PHYSICS);
                    vehicle->vehicleFlags.bNeverUseSmallerRemovalRange = true;
                    vehicle->m_nExtendedRemovalRange = 255; // 0x6F3C71
                    bWaitUntilFarFromPlayer = true;
                    tractorDriverPedType = PED_TYPE_CIVMALE;
                }
            }
        }
    }

    CWorld::Add(vehicle);
    if (tractorDriverPedType != -1)
        CCarCtrl::SetUpDriverAndPassengersForVehicle(vehicle, tractorDriverPedType, 0, false, false, 99);

    if (actualModelId == MODEL_HOTDOG && m_nModelId == MODEL_HOTDOG && CStreaming::GetInfo(MODEL_BMOCHIL).IsLoaded())
    {
        CPed* ped = CPopulation::AddPed(PED_TYPE_CIVMALE, MODEL_BMOCHIL, vehicle->GetPosition() - vehicle->GetRight() * 3.0f, false);
        if (ped)
        {
            if (!m_bHotdogVendorPositionOffsetInitialized)
            {
                m_bHotdogVendorPositionOffsetInitialized = true;
                m_HotdogVendorPositionOffset.Set(0.0f, 0.0f, 0.6f);
            }
            ped->AttachPedToEntity(vehicle, m_HotdogVendorPositionOffset, 3, 0.0f, eWeaponType::WEAPON_UNARMED);
            CTheScripts::ScriptsForBrains.CheckIfNewEntityNeedsScript(ped, 0, nullptr);
        }
    }

    if (CGeneral::GetRandomNumberInRange(0, 100) < m_nAlarmChance)
        vehicle->m_nAlarmState = -1;

    if (CGeneral::GetRandomNumberInRange(0, 100) < m_nDoorLockChance)
        vehicle->m_nDoorLock = eCarLock::CARLOCK_LOCKED;

    if (m_nPrimaryColor != -1 && m_nSecondaryColor != -1)
    {
        vehicle->m_nPrimaryColor = m_nPrimaryColor;
        vehicle->m_nSecondaryColor = m_nSecondaryColor;
    }
    else if (m_nModelId < -1)
    {
        m_nPrimaryColor = vehicle->m_nPrimaryColor;
        m_nSecondaryColor = vehicle->m_nSecondaryColor;
    }
    CVisibilityPlugins::SetClumpAlpha(vehicle->GetRpClump(), 0);
    m_nVehicleHandle = GetVehiclePool()->GetRef(vehicle);

    // Originally, R* did a signed comparison between unsigned m_nGenerateCount and signed 32bit constant -1.
    // This made the generated code to always skip the decrementation.
    // However, this bug does not affect the game at all because all cargens created by the script
    // are either disabled or infinite.
#ifdef FIX_BUGS
    if (m_nGenerateCount < (uint16)-1)
#else
    if (m_nGenerateCount < -1)
#endif
        m_nGenerateCount--;

    m_nNextGenTime = CalcNextGen();
}

// 0x6F3E90
void CCarGenerator::Process()
{
#ifdef NOTSA_STATEHASH
    // SH3 NOTSA_SH_CARGENLOG: one G row per generator that is processed (rec 0-5)
    if (FILE* const cgf = notsa::rr::CarGenLog()) {
        notsa::rr::CarGenLogOnce();
        const CVector gp = m_vecPosn;
        const char* why = "-";
        if (m_nVehicleHandle != -1) why = "has_car";
        else if (!(CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter || m_nNextGenTime <= CTimer::GetTimeInMS())) why = "not_time_yet";
        else if (m_nGenerateCount == 0) why = "generate_count_0";
        else if (!CheckIfWithinRangeOfAnyPlayers()) why = "out_of_range";
        else {
            g_cgGenWhy = "-";
            const auto r0 = notsa::rr::RandCountNow();
            const auto genTime0 = m_nNextGenTime;
            DoInternalProcessing();
            if (m_nVehicleHandle != -1) {
                why = "created";
            } else {
                why = g_cgGenWhy;
            }
            std::fprintf(cgf, "%d %llu G %d %d %.3f,%.3f,%.3f %s %d rand0=%llu next_gen=%u->%u gen_count=%d\n", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), (int)CTheCarGenerators::GetIndex(this), (int)m_nModelId, gp.x, gp.y, gp.z, why, (int)m_nVehicleHandle,
                (unsigned long long)r0, (unsigned)genTime0, (unsigned)m_nNextGenTime, (int)m_nGenerateCount);
            std::fflush(cgf);
            goto cg_after;
        }
        std::fprintf(cgf, "%d %llu G %d %d %.3f,%.3f,%.3f %s %d\n", notsa::rr::LoadLogRecFrame(), (unsigned long long)notsa::rr::RandCountNow(), (int)CTheCarGenerators::GetIndex(this), (int)m_nModelId, gp.x, gp.y, gp.z, why, (int)m_nVehicleHandle);
        std::fflush(cgf);
        goto cg_after;
    }
#endif
    if (m_nVehicleHandle == -1 &&
        (
            CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter
            || m_nNextGenTime <= CTimer::GetTimeInMS()
        )
            && m_nGenerateCount != 0
            && CheckIfWithinRangeOfAnyPlayers()
    ) {
#ifdef NOTSA_STATEHASH
        static int s_preG = 0;
        FILE* const pf = notsa::rr::LoadLogRecFrame() < 0 && s_preG < 1500 ? notsa::rr::CarGenLog(false, true) : nullptr;
        if (pf) {
            s_preG++;
            g_cgGenWhy = "-";
            const auto r0 = notsa::rr::RandCountNow();
            const CVector gp = m_vecPosn;
            DoInternalProcessing();
            std::fprintf(pf, "-1 %llu G %d %d %.3f,%.3f,%.3f %s %d rand0=%llu\n", (unsigned long long)notsa::rr::RandCountNow(), (int)CTheCarGenerators::GetIndex(this), (int)m_nModelId, gp.x, gp.y, gp.z, m_nVehicleHandle != -1 ? "created" : g_cgGenWhy, (int)m_nVehicleHandle, (unsigned long long)r0);
            std::fflush(pf);
        } else
#endif
        DoInternalProcessing();
    }
#ifdef NOTSA_STATEHASH
cg_after:
#endif

    if (m_nVehicleHandle != -1)
    {
        auto vehicle = GetVehiclePool()->GetAtRef(m_nVehicleHandle);
        if (!vehicle)
            m_nVehicleHandle = -1;
        else if (vehicle->GetStatus() == STATUS_PLAYER)
        {
            m_nNextGenTime += 60000;
            m_nVehicleHandle = -1;
            bWaitUntilFarFromPlayer = true;
            vehicle->m_nExtendedRemovalRange = 0;
            if (m_nModelId < 0)
                m_nModelId = -1;
        }
    }
}

// 0x6F2E50
void CCarGenerator::Setup(CVector posn, float angle, int32 modelId, int16 color1, int16 color2, uint8 bForceSpawn,
                          uint8 alarmChance, uint8 doorLockChance, uint16 minDelay, uint16 maxDelay,
                          uint8 iplId, bool ignorePopulationLimit)
{
    // 0x8722E8: the exe constant is 0x3F360B60 (0.711111069), one ulp below the correctly rounded 256.0f / 360.0f (0x3F360B61):
    // 45 * magic is 31.999998 in the exe (-> 31) but 32.000001 with the rounded constant
    const float magic = std::bit_cast<float>(0x3F360B60u);

    m_vecPosn = posn;
    m_nAngle = NOTSA_FTOL(int8, angle * magic);
    m_nModelId = modelId;
    m_nPrimaryColor = (uint8)(color1);
    m_nSecondaryColor = (uint8)(color2);

    bWaitUntilFarFromPlayer = false;
    bIgnorePopulationLimit = ignorePopulationLimit;
    bHighPriority = bForceSpawn;
    bPlayerHasAlreadyOwnedCar = false;

    m_nAlarmChance = alarmChance;
    m_nDoorLockChance = doorLockChance;
    m_nMinDelay = minDelay;
    m_nMaxDelay = maxDelay;
    m_nIplId = iplId;

    m_nVehicleHandle = -1;
    m_nNextGenTime = CTimer::GetTimeInMS() + 1;
    m_nGenerateCount = 0;
    m_bIsUsed = true;
}

// 0x6F2E30
void CCarGenerator::SwitchOff()
{
    m_nGenerateCount = 0;
}

// 0x6F32C0
void CCarGenerator::SwitchOn()
{
    m_nGenerateCount = (uint16)-1;
    m_nNextGenTime = CalcNextGen();
}

// 0x6F2E40
uint32 CCarGenerator::CalcNextGen()
{
    return CTimer::GetTimeInMS() + 4;
}

// notsa
CVehicle* CCarGenerator::CreateVehicle(eModelID model, eVehicleCreatedBy createdBy) {
    const auto* mi = CModelInfo::GetModelInfo(model)->AsVehicleModelInfoPtr();
    switch (const auto vt = mi->m_nVehicleType) {
    case VEHICLE_TYPE_AUTOMOBILE:
        return new CAutomobile{model, createdBy, true};
    case VEHICLE_TYPE_MTRUCK:
        return new CMonsterTruck{model, createdBy};
    case VEHICLE_TYPE_QUAD:
        return new CQuadBike{model, createdBy};
    case VEHICLE_TYPE_HELI:
        return new CHeli{model, createdBy};
    case VEHICLE_TYPE_PLANE:
        return new CPlane{model, createdBy};
    case VEHICLE_TYPE_BOAT:
        return new CBoat{model, createdBy};
    case VEHICLE_TYPE_BIKE: {
        auto* bike = new CBike{model, createdBy};
        bike->bikeFlags.bOnSideStand = true;
        return bike;
    }
    case VEHICLE_TYPE_BMX: {
        auto* const bmx = new CBmx{model, createdBy};
        bmx->bikeFlags.bOnSideStand = true;
        return bmx;
    }
    case VEHICLE_TYPE_TRAILER:
        return new CTrailer{model, createdBy};
    case VEHICLE_TYPE_TRAIN:
        return new CTrain{model, createdBy};
    default:
        NOTSA_LOG_WARN("Invalid vehicle type ({})", (int32)(vt));
    }
    return nullptr;
}
