#include "StdInc.h"

#include "RoadBlocks.h"
#include "PedPlacement.h"
#include "TaskComplexWanderCop.h"
#include "TaskSimpleStandStill.h"
#include <extensions/File.hpp>
#include <numbers>
#include "ModelIndices.h"
#include "Entity/Object/Object.h"
#include "Entity/Vehicle/Automobile.h"
#include "CarCtrl.h"
#include "FireManager.h"
#include "VisibilityPlugins.h"
#include "DamageManager.h"

void CRoadBlocks::InjectHooks() {
    RH_ScopedClass(CRoadBlocks);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x461100);
    RH_ScopedInstall(ClearScriptRoadBlocks, 0x460EC0);
    RH_ScopedInstall(ClearSpaceForRoadBlockObject, 0x461020);
    RH_ScopedInstall(CreateRoadBlockBetween2Points, 0x4619C0);
    RH_ScopedInstall(GenerateRoadBlockPedsForCar, 0x461170);
    RH_ScopedInstall(GenerateRoadBlocks, 0x4629E0);
    RH_ScopedInstall(GetRoadBlockNodeInfo, 0x460EE0);
    RH_ScopedInstall(RegisterScriptRoadBlock, 0x460DF0);
}

// 0x461100
void CRoadBlocks::Init() {
    rng::fill(InOrOut, true);
    GenerateDynamicRoadBlocks = false;

    if (notsa::File rbx("data\\paths\\roadblox.dat", "rb"); rbx) {
        rbx.Read(&NumRoadBlocks, sizeof(int32));
        assert(NumRoadBlocks <= MAX_ROADBLOCKS);
        rbx.Read(RoadBlockNodes.data(), RoadBlockNodes.size() * sizeof(CNodeAddress));
    } else {
        NOTSA_UNREACHABLE("roadblox.dat couldn't be opened!");
    }
    ClearScriptRoadBlocks();
}

// 0x460EC0
void CRoadBlocks::ClearScriptRoadBlocks() {
    for (auto& srb : aScriptRoadBlocks) {
        srb.IsActive = false;
    }
}

// 0x461020
// Returns true if cleared successfully.
bool CRoadBlocks::ClearSpaceForRoadBlockObject(CVector cornerA, CVector cornerB){
    int16 numEntities{};
    CEntity* entities[2]{};
    CWorld::FindObjectsIntersectingCube(
        cornerA,
        cornerB,
        &numEntities,
        std::size(entities),
        entities,
        false,
        true,
        true,
        true,
        false
    );

    if (numEntities > std::size(entities) || numEntities <= 0) {
        return numEntities <= 0;
    }

    const auto Remove = [](CEntity* e) {
        CWorld::Remove(e);
        delete e;
    };

    for (auto* entity : entities | rngv::take(numEntities)) {
        switch (entity->GetType()) {
        case ENTITY_TYPE_VEHICLE:
            if (auto* v = entity->AsVehicle(); !v->CanBeDeleted()) {
                return false;
            } else if (!v->vehicleFlags.bCreateRoadBlockPeds) {
                Remove(v);
            }
            break;
        case ENTITY_TYPE_PED:
            if (auto* p = entity->AsPed(); p->CanBeDeleted()) {
                Remove(p);
            } else {
                return false;
            }
            break;
        case ENTITY_TYPE_OBJECT:
            if (auto* o = entity->AsObject(); o->CanBeDeleted() && o->m_nObjectType != OBJECT_GAME) {
                Remove(o);
            } else {
                return false;
            }
            break;
        default:
            NOTSA_UNREACHABLE();
        }
    }

    return true;
}

// 0x4619C0
void CRoadBlocks::CreateRoadBlockBetween2Points(CVector a, CVector b, bool isGangRoadBlock) {
    // Allocates an object from the pool, and only calls the constructor if that succeeded
    const auto CreateTempObject = [](ModelIndex model) -> CObject* {
        void* const mem = CObject::operator new(sizeof(CObject));
        return mem ? ::new (mem) CObject((eModelID)model, true) : nullptr;
    };

    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    const float roadLen = std::sqrt((dx * dx + dy * dy) + dz * dz);
    const CVector mid{
        (a.x + b.x) * 0.5f,
        (a.y + b.y) * 0.5f,
        (a.z + b.z) * 0.5f
    };

    // Direction of the road block
    CVector dir{ dx, dy, dz };
    dir.Normalise();

    // Perpendicular of the direction, pointing towards the player
    CVector perp{ dir.y, -dir.x, 0.f };
    perp.Normalise();
    {
        const CVector toPlayer = FindPlayerCoors(-1) - mid;
        if ((toPlayer.z * perp.z + toPlayer.x * perp.x) + toPlayer.y * perp.y < 0.f) {
            perp.x *= -1.f;
            perp.y *= -1.f;
            perp.z *= -1.f;
        }
    }

    // Pick the model
    eModelID model;
    if (!isGangRoadBlock) {
        const auto wanted = FindPlayerWanted(-1);
        if (wanted->AreArmyRequired()) {
            model = (eModelID)433; // 0x1B1
        } else if (wanted->AreFbiRequired()) {
            model = (eModelID)490; // 0x1EA
        } else if (wanted->AreSwatRequired()) {
            model = (eModelID)427; // 0x1AB
        } else {
            model = CStreaming::GetDefaultCopCarModel(false);
        }
        if (!CStreaming::IsModelLoaded(model)) {
            model = CStreaming::GetDefaultCopCarModel(false);
        }
        if (model == (eModelID)523) { // 0x20B - Cop bike
            return;
        }
    } else {
        model = CPopulation::PickRiotRoadBlockCar();
        if (model == MODEL_INVALID) {
            return;
        }
    }

    const auto& carBB  = CModelInfo::GetModelInfo(model)->GetColModel()->GetBoundingBox();
    const CVector carMin = carBB.m_vecMin;
    const CVector carMax = carBB.m_vecMax;

    const float carWidth  = (carMax.x - carMin.x) + 2.0f;
    float       carLength = (carMax.y - carMin.y) + 0.2f;
    if (isGangRoadBlock) {
        carLength += 0.5f;
    }

    // Figure out how many cars (and in which orientation) fit in
    std::array<bool, 5> isSideways{};
    int32 numCars       = 0;
    float remainingLen  = roadLen;
    for (size_t i = 0; i < isSideways.size();) {
        if (model == 433 || isGangRoadBlock) {
            isSideways[i] = false;
        } else {
            isSideways[i] = (CGeneral::GetRandomNumber() & 1) != 0;
        }
        const float needed = isSideways[i] ? carWidth : carLength;
        if (remainingLen < needed) {
            break;
        }
        remainingLen -= needed;
        numCars++;
        if (numCars == 5 || ++i >= isSideways.size()) {
            break;
        }
    }

    const float spacing = remainingLen / (float)(numCars + 1);

    CMatrix M{};

    // Sets the matrix' orientation (not the position)
    const auto SetMatrixAxes = [&](const CVector& right, const CVector& fwd, const CVector& up) {
        M.GetRight()   = right;
        M.GetForward() = fwd;
        M.GetUp()      = up;
    };

    // Calculates the 2 corners (in world space) used to clear the space around the object
    const auto ClearSpaceAround = [&]() {
        const auto& pos = M.GetPosition();
        const CVector cornerA = M.InverseTransformVector(carMin) + pos;
        const CVector cornerB = M.InverseTransformVector(carMax) + pos;
        return ClearSpaceForRoadBlockObject(cornerA, cornerB);
    };

    if (numCars > 0) {
        const float halfLength = carLength * 0.5f;
        float       offset     = spacing; // Offset from `a` along the road
        for (int32 i = 0; i < numCars; i++) {
            bool isOnItsSide = false; // Only used for gang road blocks
            float angle;
            if (!isSideways[i]) { // Car is aligned along the road
                offset += halfLength;
                if (isGangRoadBlock) {
                    isOnItsSide = (CGeneral::GetRandomNumber() & 0xFF) < 0x40;
                }
                if (isOnItsSide) {
                    SetMatrixAxes(CVector{ 0.f, 0.f, 1.f }, dir, CVector{ -dir.y, dir.x, 0.f });
                } else {
                    SetMatrixAxes(CVector{ dir.y, -dir.x, 0.f }, dir, CVector{ 0.f, 0.f, 1.f });
                }
                if (CGeneral::GetRandomNumber() & 1) { // Flip it
                    if (isOnItsSide) {
                        M.RotateY(std::numbers::pi_v<float>);
                    } else {
                        M.RotateZ(std::numbers::pi_v<float>);
                    }
                }
                angle = ((float)(CGeneral::GetRandomNumber() & 0xFF) - 128.f) * (isGangRoadBlock ? 0.006f : 0.003f);
            } else { // Car is perpendicular to the road
                offset += carWidth * 0.5f;
                SetMatrixAxes(CVector{ perp.y, -perp.x, 0.f }, perp, CVector{ 0.f, 0.f, 1.f });
                angle = ((float)(CGeneral::GetRandomNumber() & 0xFF) - 128.f) * (isGangRoadBlock ? 0.004f : 0.002f);
            }
            M.RotateZ(angle);

            const float offX = dir.x * offset;
            const float offY = dir.y * offset;
            const float offZ = dir.z * offset;
            M.SetTranslateOnly(CVector{ offX + a.x, offY + a.y, offZ + a.z });

            // Lift it, so it's on the ground
            M.GetPosition().z = (M.GetPosition().z - (isOnItsSide ? carMin.x : carMin.z)) + 0.3f;

            // NOTE: Even for sideways cars the half of the car's *length* is used here (and not the width)
            offset += halfLength + spacing;

            if (!ClearSpaceAround()) {
                continue;
            }

            if (!isGangRoadBlock || (!isOnItsSide && (CGeneral::GetRandomNumber() & 0xFF) >= 0x40)) {
                // Create a car
                void* const mem = CVehicle::operator new(sizeof(CAutomobile));
                if (!mem) {
                    continue; // NOTSA: Original would crash here
                }
                auto* const veh = ::new (mem) CAutomobile(model, RANDOM_VEHICLE, true);
                veh->SetStatus(STATUS_ABANDONED);
                M.GetPosition().z = (veh->GetHeightAboveRoad() - 0.6f) + M.GetPosition().z;
                veh->GetMatrix() = M;
                veh->PlaceOnRoadProperly();
                veh->SetIsStatic(false);
                if (veh->GetRwObject()) {
                    veh->UpdateRwMatrix();
                }
                veh->m_nDoorLock = CARLOCK_UNLOCKED;
                CCarCtrl::JoinCarWithRoadSystem(veh);

                auto& ap = veh->m_autoPilot;
                ap.m_nCurrentLane  = 0;
                ap.m_nNextLane     = 0;
                ap.m_nCarMission   = MISSION_NONE;
                ap.m_nTempAction   = TEMPACT_NONE;
                ap.m_speed         = 0.f;
                ap.m_nCruiseSpeed  = 0;
                veh->vehicleFlags.bNeverUseSmallerRemovalRange = true;
                veh->vehicleFlags.bIsLocked   = false;
                veh->vehicleFlags.bEngineOn   = false;

                bool isBurning = false;
                if (isGangRoadBlock) {
                    if (((CGeneral::GetRandomNumber() & 0xFFFF) * (1.f / 32768.f) * 4.f) < 1.f) { // rand in [0, 4) truncated => 0
                        veh->SetTotalDamage(true);
                    } else {
                        veh->BlowUpCarCutSceneNoExtras(true, true, true, true);
                        veh->m_nTimeWhenBlowedUp += 1000000;
                        isBurning = true;
                    }
                } else if (veh->UsesSiren() && (CGeneral::GetRandomNumber() & 1)) {
                    veh->vehicleFlags.bSirenOrAlarm = true;
                }

                if (veh->GetMatrix().GetUp().z <= 0.94f) { // Flipped too much
                    delete veh;
                } else {
                    CVisibilityPlugins::SetClumpAlpha(veh->GetRpClump(), 0);
                    CWorld::Add(veh);
                    veh->vehicleFlags.bCreateRoadBlockPeds = true;
                    veh->m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 7000;
                    veh->m_nNumPedsForRoadBlock = numCars < 4 ? 2 : 1;

                    if (isSideways[i]) {
                        veh->m_nPedsPositionForRoadBlock = 2;
                        auto* const autoMobile = veh;
                        auto node = CDamageManager::GetCarNodeIndexFromDoor((eDoors)2);
                        if (autoMobile->m_aCarNodes[node]) {
                            autoMobile->OpenDoor(nullptr, node, (eDoors)2, 1.f, true);
                        }
                        if (veh->m_nNumPedsForRoadBlock > 1) {
                            node = CDamageManager::GetCarNodeIndexFromDoor((eDoors)3);
                            if (autoMobile->m_aCarNodes[node]) {
                                autoMobile->OpenDoor(nullptr, node, (eDoors)3, 1.f, true);
                            }
                        }
                    } else {
                        const CVector vehPos = veh->GetPosition();
                        const CVector toVeh  = vehPos - FindPlayerCoors(-1);
                        const auto&   vehM   = veh->GetMatrix();
                        // Is the vehicle facing away from the player?
                        const float   dot    = (toVeh.y * vehM.GetRight().y + toVeh.z * vehM.GetRight().z) + toVeh.x * vehM.GetRight().x;
                        veh->m_nPedsPositionForRoadBlock = dot >= 0.f ? 1 : 0;
                    }

                    if (isBurning) {
                        gFireManager.StartFire(veh, nullptr, 2.8f, true, 60000, 2);
                        if (veh->m_pFire) {
                            veh->m_pFire->SetRemovalDist(92);
                        }
                    }
                }
            } else {
                // Create a burning wreck
                auto* const obj = CreateTempObject(ModelIndices::MI_ROADBLOCKFUCKEDCAR1);
                if (!obj) {
                    continue; // NOTSA: Original would crash here
                }
                obj->GetMatrix() = M;
                obj->SetPosn(M.GetPosition());
                obj->SetIsStatic(false);
                CObject::nNoTempObjects++;
                obj->m_nObjectType  = OBJECT_TEMPORARY;
                obj->m_nRemovalTime = CTimer::GetTimeInMS() + 600000;
                CWorld::Add(obj);
                gFireManager.StartFire(obj, nullptr, 2.8f, true, 60000, 2);
                if (obj->m_pFire) {
                    obj->m_pFire->SetRemovalDist(92);
                }
            }
        }
    }

    // Place some barriers around the road block
    if (!isGangRoadBlock) {
        const ModelIndex barrierModel = ModelIndices::MI_ROADWORKBARRIER1;
        const auto& barrierBB   = CModelInfo::GetModelInfo((eModelID)barrierModel)->GetColModel()->GetBoundingBox();
        const CVector barMin = barrierBB.m_vecMin;
        const CVector barMax = barrierBB.m_vecMax;

        const float barSize   = (barMax.x - barMin.x) + 0.5f;
        const int32 numBars   = std::min((int32)(roadLen / barSize), 8);
        const float barSpacing = (roadLen - (float)numBars * barSize) / (float)(numBars + 1);

        CObject::DeleteAllTempObjectsInArea(mid, roadLen * 0.5f);

        for (int32 i = 0; i < numBars; i++) {
            const float offset = ((float)i + 0.5f) * barSize + (float)(i + 1) * barSpacing;

            M.SetUnity();
            M.SetTranslate(CVector{ 0.f, 0.f, 0.f });
            SetMatrixAxes(dir, CVector{ dir.y, -dir.x, 0.f }, CVector{ 0.f, 0.f, 1.f });
            M.RotateZ(((float)(CGeneral::GetRandomNumber() & 0xFF) - 128.f) * 0.003f);

            M.SetTranslateOnly(CVector{
                (dir.x * offset + a.x) + perp.x * 5.f,
                (dir.y * offset + a.y) + perp.y * 5.f,
                (dir.z * offset + a.z) + perp.z * 5.f
            });
            M.GetPosition().x += (float)(CGeneral::GetRandomNumber() & 0xF) * 0.05f;
            M.GetPosition().y += (float)(CGeneral::GetRandomNumber() & 0xF) * 0.05f;

            bool found{};
            M.GetPosition().z = CWorld::FindGroundZFor3DCoord(CVector{ M.GetPosition().x, M.GetPosition().y, M.GetPosition().z + 2.f }, &found, nullptr);
            if (!found) {
                continue;
            }
            M.GetPosition().z -= barMin.z;

            const CVector cornerA = M.InverseTransformVector(barMin) + M.GetPosition();
            const CVector cornerB = M.InverseTransformVector(barMax) + M.GetPosition();
            if (!ClearSpaceForRoadBlockObject(cornerA, cornerB)) {
                continue;
            }

            auto* const obj = CreateTempObject(barrierModel);
            if (!obj) {
                continue; // NOTSA: Original would crash here
            }
            obj->GetMatrix() = M;
            obj->SetPosn(M.GetPosition());
            CObject::nNoTempObjects++;
            obj->m_nObjectType  = OBJECT_TEMPORARY;
            obj->m_nRemovalTime = CTimer::GetTimeInMS() + 600000;
            CWorld::Add(obj);
        }
    }
}

// 0x461170
void CRoadBlocks::GenerateRoadBlockPedsForCar(CVehicle* vehicle, int32 pedsPositionsType, ePedType pedType) {
    const auto Generate = [&](eModelID pedModel = MODEL_INVALID, eCopType copType = COP_TYPE_CITYCOP, bool isSpecialCop = false) {
        static constexpr auto PLACEMENTS = std::to_array<CVector>({
            { -1.5f, +1.9f, 0.0f },
            { -1.5f, -2.6f, 0.0f },
            { +1.5f, +1.9f, 0.0f },
            { +1.5f, -2.6f, 0.0f },
            { -1.5f,  0.0f, 0.0f },
            { +1.5f,  0.0f, 0.0f },
        });

        static constexpr auto SPECIAL_COP_PLACEMENTS = std::to_array<CVector>({
            {  0.0f, +3.2f, 0.0f },
            { +1.5f, -1.8f, 0.0f },
            {  0.0f, +3.2f, 0.0f },
            { -1.5f, -1.8f, 0.0f },
            { -1.5f,  0.0f, 0.0f },
            { +1.5f,  0.0f, 0.0f },
        });

        const auto placementIdx = 2 * pedsPositionsType;
        const auto radiusRatio  = vehicle->GetColModel()->GetBoundingSphere().m_fRadius
            / CModelInfo::GetModelInfo(CStreaming::GetDefaultCopCarModel(false))->GetColModel()->GetBoundingSphere().m_fRadius;

        for (auto i = 0u; i < vehicle->m_nNumPedsForRoadBlock; i++) {
            const auto offset = (isSpecialCop ? SPECIAL_COP_PLACEMENTS : PLACEMENTS)[placementIdx + i] * radiusRatio;
            const auto pos = vehicle->GetMatrix().TransformPoint(offset);

            auto* ped = [&]() -> CPed* {
                if (pedType != PED_TYPE_COP) { // 0x461560
                    return new CCivilianPed(pedType, pedModel);
                } else {
                    auto* p = new CCopPed(CStreaming::IsModelLoaded(pedModel) ? copType : COP_TYPE_CITYCOP);
                    if (copType == COP_TYPE_CITYCOP) {
                        p->SetCurrentWeapon(WEAPON_PISTOL);
                    }
                    return p;
                }
            }();

            ped->SetPosn(std::get<CVector>(CPedPlacement::FindZCoorForPed(pos)));
            ped->GetMatrix().SetRotateKeepPos({ 0.0f, 0.0f, -HALF_PI });

            if (pedType == PED_TYPE_COP) {
                auto* t = new CTaskComplexWanderCop(PEDMOVE_STILL, CGeneral::GetRandomNumberInRange(8ui8));
                t->m_nSubTaskCreatedTimer = {};
                t->m_nScanForStuffTimer   = {};
                ped->GetTaskManager().SetTask(t, TASK_PRIMARY_PRIMARY);
            }
            ped->GetTaskManager().SetTask(new CTaskSimpleStandStill(0, true), TASK_PRIMARY_DEFAULT);

            ped->bStayInSamePlace         = true;
            ped->bNotAllowedToDuck        = true;
            ped->m_nTimeTillWeNeedThisPed = CTimer::GetTimeInMS() + 10'000;
            ped->bCrouchWhenShooting      = !isSpecialCop || pedsPositionsType != 2;
            ped->bCullExtraFarAway        = true;
            CEntity::RegisterReference(ped->m_pVehicle = vehicle);
            CVisibilityPlugins::SetClumpAlpha(ped->GetRpClump(), 0);

            if (pedType != PED_TYPE_COP) {
                const auto weapon = CGangs::Gang[pedType - PED_TYPE_GANG1].GetRandomWeapon(false);
                if (weapon != WEAPON_UNARMED) {
                    ped->GiveDelayedWeapon(weapon, 25'001);
                    ped->SetCurrentWeapon(weapon);
                }
            }
            CWorld::Add(ped);
            ped->GetEventGroup().Add<CEventScriptCommand>({ TASK_PRIMARY_PRIMARY, new CTaskComplexKillPedOnFoot(FindPlayerPed()) });
        }
    };

    if (pedType == PED_TYPE_COP) {
        switch (vehicle->GetModelId()) {
        case MODEL_ENFORCER: Generate(MODEL_SWAT,    COP_TYPE_SWAT1,   true); break;
        case MODEL_BARRACKS: Generate(MODEL_ARMY,    COP_TYPE_ARMY,    true); break;
        case MODEL_FBIRANCH: Generate(MODEL_FBI,     COP_TYPE_FBI,     true); break;
        case MODEL_COPCARRU: Generate(MODEL_INVALID, COP_TYPE_CITYCOP, true); break;
        default:             Generate(MODEL_INVALID, COP_TYPE_CITYCOP, false); break;
        }
    } else if (IsPedTypeGang(pedType)) {
        for (auto i = 0; i < TOTAL_GANGS; i++) {
            if (!CPopCycle::m_pCurrZoneInfo->GangStrength[i]) {
                continue;
            }
            const auto pedModel = CGangs::ChooseGangPedModel((eGangID)i);
            if (pedModel == MODEL_INVALID) {
                continue;
            }
            Generate(pedModel);
            return;
        }
    } else {
        Generate();
    }
}

// 0x4629E0
void CRoadBlocks::GenerateRoadBlocks() {
    ZoneScoped;

    if (FindPlayerWanted()->m_ChanceOnRoadBlock && FindPlayerVehicle()) {
        if (!GenerateDynamicRoadBlocks) {
            rng::fill(InOrOut, true);
            GenerateDynamicRoadBlocks = true;
        }

        const auto counter1      = MAX_ROADBLOCKS * (CTimer::GetFrameCounter() % 16 + 1);
        const auto rbsToGenerate = std::min((uint32)NumRoadBlocks, ((counter1 % 16) + counter1) / 16);
        auto       counter2      = MAX_ROADBLOCKS * (CTimer::GetFrameCounter() % 16) / 16;

        for (; counter2 < rbsToGenerate; counter2++) {
            const auto& mrbNode = RoadBlockNodes[counter2];
            if (!ThePaths.IsAreaLoaded(mrbNode)) {
                continue;
            }
            const auto& mainNode = ThePaths.GetPathNode(mrbNode);
            const auto  playerPos = FindPlayerCoors();
            if (std::abs(playerPos.x - mainNode->GetPosition().x) >= 90.0f ||
                std::abs(playerPos.y - mainNode->GetPosition().y) >= 90.0f ||
                DistanceBetweenPoints2D(playerPos, mainNode->GetPosition()) >= 90.0f)
            {
                InOrOut[counter2] = false;
                continue;
            }

            if (InOrOut[counter2]) {
                continue;
            }
            InOrOut[counter2] = true;

            if (CGeneral::GetRandomNumberInRange(128u) >= FindPlayerWanted()->m_ChanceOnRoadBlock) {
                continue;
            }

            float mrbWidth{};
            CVector mrbDir{};
            if (!GetRoadBlockNodeInfo(mrbNode, mrbWidth, mrbDir)) {
                continue;
            }

            if (mainNode->m_nPathWidth) {
                const auto width = mainNode->m_nPathWidth / 16.0f;
                CreateRoadBlockBetween2Points(
                    mainNode->GetPosition() + mrbDir * (mrbWidth / 2.f + width),
                    mainNode->GetPosition() + mrbDir * width,
                    false
                );
                CreateRoadBlockBetween2Points(
                    mainNode->GetPosition() - mrbDir * width,
                    mainNode->GetPosition() - mrbDir * (mrbWidth / 2.f + width),
                    false
                );
                continue;
            }

            for (auto&& [i, nodeAddr] : rngv::enumerate(RoadBlockNodes)) {
                if (counter2 == i || InOrOut[i] || !ThePaths.IsAreaLoaded(nodeAddr.m_wAreaId)) {
                    continue;
                }
                const auto& node = ThePaths.GetPathNode(nodeAddr);

                if (std::abs(mainNode->GetPosition().x - node->GetPosition().x) >= 30.0f ||
                    std::abs(mainNode->GetPosition().y - node->GetPosition().y) >= 30.0f)
                {
                    continue;
                }

                float   width{};
                CVector dir{}; 
                if (!GetRoadBlockNodeInfo(nodeAddr, width, dir)) {
                    continue;
                }

                if (mrbWidth != width || dir.Dot(mrbDir) <= 0.7f) {
                    continue;
                }

                [[maybe_unused]] CColPoint col{};
                [[maybe_unused]] CEntity*  colEntity{};
                if (CWorld::ProcessLineOfSight(
                    mainNode->GetPosition() + CVector{0.0f, 0.0f, 1.0f},
                    node->GetPosition() + CVector{0.0f, 0.0f, 1.0f},
                    col,
                    colEntity,
                    true,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false,
                    false
                )) {
                    continue;
                }

                const auto dirFromMain = (node->GetPosition() - mainNode->GetPosition()).Normalized();
                CreateRoadBlockBetween2Points(
                    node->GetPosition()     + dirFromMain * (mrbWidth / 2.0f),
                    mainNode->GetPosition() - dirFromMain * (mrbWidth / 2.0f),
                    false
                );
                InOrOut[i] = true;

                if (i == NumRoadBlocks) {
                    CreateRoadBlockBetween2Points(
                        mainNode->GetPosition() - dirFromMain * (mrbWidth / 2.0f),
                        mainNode->GetPosition() + dirFromMain * (mrbWidth / 2.0f),
                        false
                    );
                    break;
                }
            }
        }
    } else {
        GenerateDynamicRoadBlocks = false;
    }

    if (auto& srb = aScriptRoadBlocks[CTimer::GetFrameCounter() % MAX_SCRIPT_ROADBLOCKS]; srb.IsActive) {
        const auto c = CVector::Centroid({ srb.CornerA, srb.CornerB });

        if (DistanceBetweenPoints(FindPlayerCoors(), c) >= 90.0f) {
            srb.IsSafeToCreate = true;
        } else if (srb.IsSafeToCreate) {
            CreateRoadBlockBetween2Points(srb.CornerA, srb.CornerB, srb.IsGangRoadBlock);
            srb.IsSafeToCreate = false;
        }
    }
}

// 0x460EE0
bool CRoadBlocks::GetRoadBlockNodeInfo(CNodeAddress nodeAddress, float& outWidth, CVector& outDir) {
    auto* const node = ThePaths.GetPathNode(nodeAddress);
    assert(node);

    assert(node->m_nNumLinks >= 2);
    const auto naviLinkAddrA = ThePaths.GetNaviLink(nodeAddress.m_wAreaId, node->m_wBaseLinkId + 0),
               naviLinkAddrB = ThePaths.GetNaviLink(nodeAddress.m_wAreaId, node->m_wBaseLinkId + 1);
    if (!ThePaths.IsAreaLoaded(naviLinkAddrA.m_wAreaId) || !ThePaths.IsAreaLoaded(naviLinkAddrB.m_wAreaId)) {
        return false;
    }

    const auto &naviLinkA = ThePaths.GetCarPathLink(naviLinkAddrA),
               &naviLinkB = ThePaths.GetCarPathLink(naviLinkAddrB);

    const auto maxNumLanes = std::max(
        naviLinkA.m_numOppositeDirLanes + naviLinkA.m_numSameDirLanes,
        naviLinkB.m_numOppositeDirLanes + naviLinkB.m_numSameDirLanes
    );

    outWidth = ((float)maxNumLanes + 1.f) * 5.f;
    outDir   = CVector{ (naviLinkB.GetNodeCoors() - naviLinkA.GetNodeCoors()).GetPerpRight(), 0.f }.Normalized();

    return true;
}

// 0x460DF0
void CRoadBlocks::RegisterScriptRoadBlock(CVector cornerA, CVector cornerB, bool isGangRoadBlock) {
    auto free = rng::find_if(aScriptRoadBlocks, [](const auto& srb) { return !srb.IsActive; });
    if (free == aScriptRoadBlocks.end()) {
        // No free script roadblock found
        return;
    }

    free->CornerA         = cornerA;
    free->CornerB         = cornerB;
    free->IsActive        = true;
    free->IsSafeToCreate  = true;
    free->IsGangRoadBlock = isGangRoadBlock;
}
