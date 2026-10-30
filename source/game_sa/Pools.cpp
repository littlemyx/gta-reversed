#include "StdInc.h"
#ifdef NOTSA_STANDALONE_RUN
#include "InterestingEvents.h"
#endif

#include <Pools/Pools.h>
#include "CarCtrl.h"

#include <Pools/IplDefPool.h>
#include <Pools/PedPool.h>
#include <Pools/VehiclePool.h>
#include <Pools/BuildingPool.h>
#include <Pools/ObjectPool.h>
#include <Pools/DummyPool.h>
#include <Pools/ColModelPool.h>
#include <Pools/TaskPool.h>
#include <Pools/PedIntelligencePool.h>
#include <Pools/PtrNodeSingleLinkPool.h>
#include <Pools/PtrNodeDoubleLinkPool.h>
#include <Pools/EntryInfoNodePool.h>
#include <Pools/PointRoutePool.h>
#include <Pools/PatrolRoutePool.h>
#include <Pools/EventPool.h>
#include <Pools/NodeRoutePool.h>
#include <Pools/TaskAllocatorPool.h>
#include <Pools/PedAttractorPool.h>

NOTSA_GLOBAL(ms_pPedPool, 0xB74490, (CPedPool*), {});
NOTSA_GLOBAL(ms_pVehiclePool, 0xB74494, (CVehiclePool*), {});
NOTSA_GLOBAL(ms_pBuildingPool, 0xB74498, (CBuildingPool*), {});
NOTSA_GLOBAL(ms_pObjectPool, 0xB7449C, (CObjectPool*), {});
NOTSA_GLOBAL(ms_pDummyPool, 0xB744A0, (CDummyPool*), {});
NOTSA_GLOBAL(ms_pColModelPool, 0xB744A4, (CColModelPool*), {});
NOTSA_GLOBAL(ms_pTaskPool, 0xB744A8, (CTaskPool*), {});
NOTSA_GLOBAL(ms_pPedIntelligencePool, 0xB744C0, (CPedIntelligencePool*), {});
NOTSA_GLOBAL(ms_pPtrNodeSingleLinkPool, 0xB74484, (CPtrNodeSingleLinkPool*), {});
NOTSA_GLOBAL(ms_pPtrNodeDoubleLinkPool, 0xB74488, (CPtrNodeDoubleLinkPool*), {});
NOTSA_GLOBAL(ms_pEntryInfoNodePool, 0xB7448C, (CEntryInfoNodePool*), {});
NOTSA_GLOBAL(ms_pPointRoutePool, 0xB744B0, (CPointRoutePool*), {});
NOTSA_GLOBAL(ms_pPatrolRoutePool, 0xB744B4, (CPatrolRoutePool*), {});
NOTSA_GLOBAL(ms_pEventPool, 0xB744AC, (CEventPool*), {});
NOTSA_GLOBAL(ms_pNodeRoutePool, 0xB744B8, (CNodeRoutePool*), {});
NOTSA_GLOBAL(ms_pTaskAllocatorPool, 0xB744BC, (CTaskAllocatorPool*), {});
NOTSA_GLOBAL(ms_pPedAttractorPool, 0xB744C4, (CPedAttractorPool*), {});

void CPools::InjectHooks() {
    RH_ScopedClass(CPools);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x550F10);
    RH_ScopedInstall(ShutDown, 0x5519F0);
    RH_ScopedInstall(CheckBuildingAtomics, 0x550170);
    RH_ScopedInstall(CheckPoolsEmpty, 0x551950);
    RH_ScopedInstall(GetObject, 0x550050);
    RH_ScopedInstall(GetObjectRef, 0x550020);
    RH_ScopedInstall(GetPed, 0x54FF90);
    RH_ScopedInstall(GetPedRef, 0x54FF60);
    RH_ScopedInstall(GetVehicle, 0x54FFF0);
    RH_ScopedInstall(GetVehicleRef, 0x54FFC0);
    RH_ScopedInstall(Load, 0x5D0890);
    RH_ScopedInstall(LoadObjectPool, 0x5D4A40);
    RH_ScopedInstall(LoadPedPool, 0x5D2D70);
    RH_ScopedInstall(LoadVehiclePool, 0x5D2A20);
    RH_ScopedInstall(MakeSureSlotInObjectPoolIsEmpty, 0x550080);
    RH_ScopedInstall(Save, 0x5D0880);
    RH_ScopedInstall(SaveObjectPool, 0x5D4940);
    RH_ScopedInstall(SavePedPool, 0x5D4B40);
    RH_ScopedInstall(SaveVehiclePool, 0x5D4800);
}

// 0x550F10
void CPools::Initialise() {
    ZoneScoped;

    ms_pPtrNodeSingleLinkPool = new CPtrNodeSingleLinkPool(70000, "PtrNode Single");
    ms_pPtrNodeDoubleLinkPool = new CPtrNodeDoubleLinkPool(3200, "PtrNode Double");
    ms_pEntryInfoNodePool     = new CEntryInfoNodePool(500, "EntryInfoNode");
    ms_pPedPool               = new CPedPool(140, "Peds");
    ms_pVehiclePool           = new CVehiclePool(110, "Vehicles");
    ms_pBuildingPool          = new CBuildingPool(13000, "Buildings");
    ms_pObjectPool            = new CObjectPool(350, "Objects");
    ms_pDummyPool             = new CDummyPool(2500, "Dummys");
    ms_pColModelPool          = new CColModelPool(10150, "ColModel");
    ms_pTaskPool              = new CTaskPool(500, "Task");
    ms_pEventPool             = new CEventPool(200, "Event");
    ms_pPointRoutePool        = new CPointRoutePool(64, "PointRoute");
    ms_pPatrolRoutePool       = new CPatrolRoutePool(32, "PatrolRoute");
    ms_pNodeRoutePool         = new CNodeRoutePool(64, "NodeRoute");;
    ms_pTaskAllocatorPool     = new CTaskAllocatorPool(16, "TaskAllocator");
    ms_pPedIntelligencePool   = new CPedIntelligencePool(140, "PedIntelligence");
    ms_pPedAttractorPool      = new CPedAttractorPool(64, "PedAttractors");
}

// 0x5519F0
void CPools::ShutDown() {
    NOTSA_LOG_DEBUG("Shutdown pool started");
#ifdef NOTSA_STANDALONE_RUN
    // Like the exe's CPools::ShutDown (17 x free(storage) + delete, no element destructor; notsa::detail::PoolDestructOnClear() is false in the run build) the pool storage
    // is freed without running the entity destructors (no ResolveReferences: the dummies still held references into the already freed object pool, BUG-REFS), and unlike the
    // exe's heap the freed memory is not left intact: forget the entities of CInterestingEvents (the idle camera read a 0xDDDD-filled ped from the previous session)
    for (auto& event : g_InterestingEvents.m_Events) {
        event.entity = nullptr;
        event.type   = 0;
        event.time   = 0;
    }
    g_InterestingEvents.m_nInterestingEvent = -1;
#endif
    delete std::exchange(ms_pPtrNodeSingleLinkPool, nullptr);
    delete std::exchange(ms_pPtrNodeDoubleLinkPool, nullptr);
    delete std::exchange(ms_pEntryInfoNodePool, nullptr);
    delete std::exchange(ms_pPedPool, nullptr);
    delete std::exchange(ms_pVehiclePool, nullptr);
    delete std::exchange(ms_pBuildingPool, nullptr);
    delete std::exchange(ms_pObjectPool, nullptr);
    delete std::exchange(ms_pDummyPool, nullptr);
    delete std::exchange(ms_pColModelPool, nullptr);
    delete std::exchange(ms_pTaskPool, nullptr);
    delete std::exchange(ms_pEventPool, nullptr);
    delete std::exchange(ms_pPointRoutePool, nullptr);
    delete std::exchange(ms_pPatrolRoutePool, nullptr);
    delete std::exchange(ms_pNodeRoutePool, nullptr);
    delete std::exchange(ms_pTaskAllocatorPool, nullptr);
    delete std::exchange(ms_pPedIntelligencePool, nullptr);
    delete std::exchange(ms_pPedAttractorPool, nullptr);
    NOTSA_LOG_DEBUG("Shutdown pool done");
}

// 0x550170
int32 CPools::CheckBuildingAtomics() {
    return -1;
}

// 0x551950
void CPools::CheckPoolsEmpty() {
    for (auto& obj : ms_pObjectPool->GetAllValid()) {
        if (obj.m_nObjectType != OBJECT_TYPE_DECORATION)
            continue;

        const auto& objPos = obj.GetPosition();
        NOTSA_LOG_DEBUG("Offending object: MI: {} Coors:{} {} {}", obj.m_nModelIndex, objPos.x, objPos.y, objPos.z);
    }
    NOTSA_LOG_DEBUG("Pools have been cleared!");
}

// 0x550050
CObject* CPools::GetObject(int32 handle) {
    return ms_pObjectPool->GetAtRef(handle);
}

// 0x550020
int32 CPools::GetObjectRef(CObject* object) {
    return ms_pObjectPool->GetRef(object);
}

// 0x54FF90
CPed* CPools::GetPed(int32 handle) {
    return ms_pPedPool->GetAtRef(handle);
}

// 0x54FF60
int32 CPools::GetPedRef(CPed* ped) {
    return ms_pPedPool->GetRef(ped);
}

// 0x54FFF0
CVehicle* CPools::GetVehicle(int32 handle) {
    return ms_pVehiclePool->GetAtRef(handle);
}

// 0x54FFC0
int32 CPools::GetVehicleRef(CVehicle* vehicle) {
    return ms_pVehiclePool->GetRef(vehicle);
}

// 0x5D0890
bool CPools::Load() {
    bool loaded = LoadPedPool();
    if (loaded)
        loaded = LoadObjectPool();

    return loaded;
}

// 0x5D4A40
bool CPools::LoadObjectPool() {
    auto iNumObjects = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();
    for (int32 i = 0; i < iNumObjects; ++i)
    {
        auto iPoolRef = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();
        auto iModelId = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();

        auto* objInPool = GetObjectPool()->GetAtRefNoChecks(iPoolRef);
        if (objInPool)
            CPopulation::ConvertToDummyObject(objInPool);

        auto* newObj = new(iPoolRef) CObject(iModelId, false);
        newObj->Load();
        CWorld::Add(newObj);
    }

    return true;
}

// 0x5D2D70
bool CPools::LoadPedPool() {
    const auto pedCount = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();
    for (auto i = 0; i < pedCount; i++) {
        const auto poolRef = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();
        const auto model   = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();
        const auto pedType = CGenericGameStorage::LoadDataFromWorkBuffer<int32>();

        CPlayerPed* playerPed = nullptr;
        if (pedType == PED_TYPE_PLAYER1) {
            playerPed = new(poolRef) CPlayerPed(0, false);
            playerPed->SetWeaponAccuracy(100);
            CWorld::Players[0].m_pPed = playerPed;
        }
        assert(playerPed != nullptr);

        playerPed->Load();
        CWorld::Add(playerPed);
    }
    return true;
}

// 0x5D2A20
// Used in CPools::Load (Android 1.0)
bool CPools::LoadVehiclePool() {
    int32 count;
    CGenericGameStorage::LoadDataFromWorkBuffer(&count, 4);
    for (auto i = 0; i < count; i++) {
        int32 modelId, poolRef;

        CGenericGameStorage::LoadDataFromWorkBuffer(&poolRef, 4); // exe 0x5D2A60-0x5D2D2F: first value = saved pool handle (SaveVehiclePool writes GetVehicleRef); the exe allocates with CVehicle::operator new(size, poolRef) (0x6E2D60) so the vehicle keeps its handle
        CGenericGameStorage::LoadDataFromWorkBuffer(&modelId, 4);

        CStreaming::RequestModel(modelId, STREAMING_KEEP_IN_MEMORY);
        CStreaming::LoadAllRequestedModels(false);

        CVehicle* vehicle = nullptr;
        switch (CModelInfo::GetModelInfo(modelId)->AsVehicleModelInfoPtr()->m_nVehicleType) {
        case VEHICLE_TYPE_MTRUCK:
            vehicle = new(poolRef) CMonsterTruck(modelId, RANDOM_VEHICLE);
            break;
        case VEHICLE_TYPE_QUAD:
            vehicle = new(poolRef) CQuadBike(modelId, RANDOM_VEHICLE);
            break;
        case VEHICLE_TYPE_HELI:
            vehicle = new(poolRef) CHeli(modelId, RANDOM_VEHICLE);
            break;
        case VEHICLE_TYPE_PLANE:
            vehicle = new(poolRef) CPlane(modelId, RANDOM_VEHICLE);
            break;
        case VEHICLE_TYPE_BOAT:
            vehicle = new(poolRef) CBoat(modelId, RANDOM_VEHICLE);
            break;
        case VEHICLE_TYPE_TRAIN:
            vehicle = new(poolRef) CTrain(modelId, RANDOM_VEHICLE);
            break;
        case VEHICLE_TYPE_BIKE:
            vehicle = new(poolRef) CBike(modelId, RANDOM_VEHICLE);
            vehicle->AsBike()->bikeFlags.bOnSideStand = true;
            break;
        case VEHICLE_TYPE_BMX:
            vehicle = new(poolRef) CBmx(modelId, RANDOM_VEHICLE);
            vehicle->AsBike()->bikeFlags.bOnSideStand = true;
            break;
        case VEHICLE_TYPE_TRAILER:
            vehicle = new(poolRef) CTrailer(modelId, RANDOM_VEHICLE);
            break;
        default:
            vehicle = new(poolRef) CAutomobile(modelId, RANDOM_VEHICLE, true);
            break;
        }
        assert(vehicle);
        vehicle->Load();
        --CCarCtrl::NumRandomCars;
        CCarCtrl::UpdateCarCount(vehicle, false);
        CWorld::Add(vehicle);
    }
    return true;
}

// 0x550080
void CPools::MakeSureSlotInObjectPoolIsEmpty(int32 slot) {
    // All code paths to this function are unreachable
    // The only caller is `CPickup::GiveUsAPickUpObject`, 
    // but all calls of it pass `-1` as the slot, so this function is never actually used.
    // The implementation is messy, as it acceses `m_pRwObject` of the Entity, which is no good.
    NOTSA_UNREACHABLE();
    //if (GetObjectPool()->IsFreeSlotAtIndex(slot))
    //    return;
    //
    //auto* obj = GetObjectPool()->GetAt(slot);
    //if (obj->IsTemporary()) {
    //    CWorld::Remove(obj);
    //    delete obj;
    //} else if (CProjectileInfo::RemoveIfThisIsAProjectile(obj)) {
    //    auto newObj = new CObject(obj->m_nModelIndex, false);
    //    CWorld::Remove(obj);
    //    *newObj = *obj;
    //    CWorld::Add(newObj);
    //
    //    obj->m_pRwObject = nullptr;
    //    delete obj;
    //
    //    newObj->m_pReferences = nullptr;
    //}
}

// 0x5D0880
bool CPools::Save() {
    bool saved = SavePedPool();
    if (saved)
        saved = SaveObjectPool();

    return saved;
}

// 0x5D4940
bool CPools::SaveObjectPool() {
    const auto IsSavable = [](CObject& obj) { return obj.m_nObjectType == OBJECT_MISSION; };

    int32 count = 0;
    for (auto& obj : GetObjectPool()->GetAllValid()) {
        if (IsSavable(obj)) {
            count++;
        }
    }
    CGenericGameStorage::SaveDataToWorkBuffer(count);

    for (auto& obj : GetObjectPool()->GetAllValid()) {
        if (!IsSavable(obj)) {
            continue;
        }
        CGenericGameStorage::SaveDataToWorkBuffer(GetObjectRef(&obj));
        CGenericGameStorage::SaveDataToWorkBuffer((int32)obj.m_nModelIndex);
        obj.Save(); // Saves the size (0x30) and the `CObjectSaveStructure`
    }
    return true;
}

// 0x5D4B40
bool CPools::SavePedPool() {
    // Only the player ped (not in a vehicle) is saved
    const auto IsSavable = [](CPed& ped) { return !ped.bInVehicle && ped.m_nPedType == PED_TYPE_PLAYER1; };

    int32 count = 0;
    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (IsSavable(ped)) {
            count++;
        }
    }
    CGenericGameStorage::SaveDataToWorkBuffer(count);

    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (!IsSavable(ped)) {
            continue;
        }
        CGenericGameStorage::SaveDataToWorkBuffer(GetPedRef(&ped));
        CGenericGameStorage::SaveDataToWorkBuffer((int32)ped.m_nModelIndex);
        CGenericGameStorage::SaveDataToWorkBuffer((int32)ped.m_nPedType);
        ped.Save();
    }
    return true;
}

// 0x5D4800
// Used in CPools::Save (Android 1.0)
bool CPools::SaveVehiclePool() {
    // Only empty (no driver, no passengers) mission vehicles are saved
    const auto IsSavable = [](CVehicle& veh) {
        return veh.m_nCreatedBy == MISSION_VEHICLE
            && !veh.m_pDriver
            && rng::none_of(veh.m_apPassengers, [](CPed* p) { return p != nullptr; });
    };

    int32 count = 0;
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (IsSavable(veh)) {
            count++;
        }
    }
    CGenericGameStorage::SaveDataToWorkBuffer(count);

    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (!IsSavable(veh)) {
            continue;
        }
        CGenericGameStorage::SaveDataToWorkBuffer(GetVehicleRef(&veh));
        CGenericGameStorage::SaveDataToWorkBuffer((int32)veh.m_nModelIndex);
        veh.Save();
    }
    return true;
}

// 0x404550
CPedPool* GetPedPool() {
    return ms_pPedPool;
}

// 0x404560
CVehiclePool* GetVehiclePool() {
    return ms_pVehiclePool;
}

// 0x403DF0
CBuildingPool* GetBuildingPool() {
    return ms_pBuildingPool;
}

// 0x404570
CObjectPool* GetObjectPool() {
    return ms_pObjectPool;
}

// 0x404580
CDummyPool* GetDummyPool() {
    return ms_pDummyPool;
}

// 0x40F000
CColModelPool* GetColModelPool() {
    return ms_pColModelPool;
}

// 0x61A330
CTaskPool* GetTaskPool() {
    return ms_pTaskPool;
}

// 0x5FF930
CPedIntelligencePool* GetPedIntelligencePool() {
    return ms_pPedIntelligencePool;
}

// 0x552190
CPtrNodeSingleLinkPool* GetPtrNodeSingleLinkPool() {
    return ms_pPtrNodeSingleLinkPool;
}

// 0x5521A0
CPtrNodeDoubleLinkPool* GetPtrNodeDoubleLinkPool() {
    return ms_pPtrNodeDoubleLinkPool;
}

// 0x536C80
CEntryInfoNodePool* GetEntryInfoNodePool() {
    return ms_pEntryInfoNodePool;
}

// 0x41B400
CPointRoutePool* GetPointRoutePool() {
    return ms_pPointRoutePool;
}

// 0x41B410
CPatrolRoutePool* GetPatrolRoutePool() {
    return ms_pPatrolRoutePool;
}

// 0x4ABF80
CEventPool* GetEventPool() {
    return ms_pEventPool;
}

// 0x41B420
CNodeRoutePool* GetNodeRoutePool() {
    return ms_pNodeRoutePool;
}

// 0x69BB70
CTaskAllocatorPool* GetTaskAllocatorPool() {
    return ms_pTaskAllocatorPool;
}

// 0x5E95A0
CPedAttractorPool* GetPedAttractorPool() {
    return ms_pPedAttractorPool;
}
#ifdef NOTSA_PORTABLE

// STAB2: pool slot = sizeof(S) of CPool<T, S>; on LP64 a derived class can be wider than the exe's chosen widest class (CPlane 3136 > CHeli 3128, CTaskAllocatorAttack 48 > char[32]), and an
// oversized object overwrites the first bytes (the vtable pointer) of the NEXT slot. These fail to compile when a layout change makes a pooled class outgrow its slot.
// (CTask / CEvent have 300 / 80 derived classes: the largest known ones are asserted here, every `new` additionally goes through CPool::NotsaCheckAllocSize at run time.)
#include "Entity/Ped/CopPed.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Object/CutsceneObject.h"
#include "Entity/AnimatedBuilding.h"
#include "Entity/Dummy/DummyPed.h"
#include "Entity/Dummy/DummyObject.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"
#include "Entity/Vehicle/Bmx.h"
#include "Entity/Vehicle/Boat.h"
#include "Entity/Vehicle/QuadBike.h"
#include "Entity/Vehicle/MonsterTruck.h"
#include "Entity/Vehicle/Trailer.h"
#include "Entity/Vehicle/Train.h"
#include "Entity/Vehicle/Heli.h"
#include "Entity/Vehicle/Plane.h"
#include "Tasks/TaskTypes/TaskSimpleSlideToCoord.h"
#include "Tasks/TaskTypes/TaskSimpleJetPack.h"
#include "Tasks/TaskTypes/TaskComplexPartnerGreet.h"
#include "Tasks/TaskTypes/TaskComplexPartnerChat.h"
#include "Tasks/TaskTypes/TaskComplexPartnerShove.h"
#include "Tasks/TaskTypes/TaskSimpleCarDriveTimed.h"
#include "Events/EventDamage.h"
#include "Events/EventKnockOffBike.h"
#include "Tasks/Allocators/TaskAllocatorAttack.h"
#include "Tasks/Allocators/TaskAllocatorPlayerCommandAttack.h"
#include "Attractors/PedStopAttractor.h"
#include "Attractors/PedSeatAttractor.h"

namespace {
template<class Pool, class... Ts> constexpr bool FitsPoolSlot = ((sizeof(Ts) <= Pool::kSlotSize) && ...);
static_assert(FitsPoolSlot<CPedPool, CPed, CPlayerPed, CCivilianPed, CCopPed, CEmergencyPed>, "ped pool slot too small");
static_assert(FitsPoolSlot<CVehiclePool, CVehicle, CAutomobile, CBike, CBmx, CBoat, CQuadBike, CMonsterTruck, CTrailer, CTrain, CHeli, CPlane>, "vehicle pool slot too small");
static_assert(FitsPoolSlot<CObjectPool, CObject, CCutsceneObject, CHandObject, CProjectile>, "object pool slot too small");
static_assert(FitsPoolSlot<CBuildingPool, CBuilding, CAnimatedBuilding>, "building pool slot too small");
static_assert(FitsPoolSlot<CDummyPool, CDummy, CDummyObject, CDummyPed>, "dummy pool slot too small");
static_assert(FitsPoolSlot<CTaskPool, CTask, CTaskSimpleSlideToCoord, CTaskSimpleJetPack, CTaskComplexPartnerGreet, CTaskComplexPartnerChat, CTaskComplexPartnerShove, CTaskSimpleCarDriveTimed>, "task pool slot too small");
static_assert(FitsPoolSlot<CEventPool, CEvent, CEventDamage, CEventKnockOffBike>, "event pool slot too small");
static_assert(FitsPoolSlot<CTaskAllocatorPool, CTaskAllocator, CTaskAllocatorAttack, CTaskAllocatorPlayerCommandAttack>, "task allocator pool slot too small");
static_assert(FitsPoolSlot<CPedAttractorPool, CPedAttractor, CPedStopAttractor, CPedSeatAttractor>, "ped attractor pool slot too small");
}
#endif
