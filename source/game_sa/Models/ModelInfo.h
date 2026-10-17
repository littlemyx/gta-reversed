/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "BaseModelInfo.h"
#include "AtomicModelInfo.h"
#include "DamageAtomicModelInfo.h"
#include "LodAtomicModelInfo.h"
#include "VehicleModelInfo.h"
#include "WeaponModelInfo.h"
#include "TimeModelInfo.h"
#include "LodTimeModelInfo.h"
#include "PedModelInfo.h"


class CModelInfo {
public:
    // static CBaseModelInfo *ms_modelInfoPtrs[20000]; Use GetModelInfo(int32 index) to get model info by id
    static constexpr int32 NUM_MODEL_INFOS = 20000;
    static inline NOTSA_GLOBAL(ms_modelInfoPtrs, 0xA9B0C8, (CBaseModelInfo*[NUM_MODEL_INFOS]), {});
    static inline NOTSA_GLOBAL(ms_lastPositionSearched, 0xAAE948, (int32), {});

    static constexpr int32 NUM_ATOMIC_MODEL_INFOS = 14000;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_atomicModelInfoStore, 0xAAE950, (CStore<CAtomicModelInfo, NUM_ATOMIC_MODEL_INFOS>));

    static constexpr int32 NUM_DAMAGE_ATOMIC_MODEL_INFOS = 70;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_damageAtomicModelInfoStore, 0xB1BF58, (CStore<CDamageAtomicModelInfo, NUM_DAMAGE_ATOMIC_MODEL_INFOS>));

    static constexpr int32 NUM_LOD_ATOMIC_MODEL_INFOS = 1;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_lodAtomicModelInfoStore, 0xB1C934, (CStore<CLodAtomicModelInfo, NUM_LOD_ATOMIC_MODEL_INFOS>));

    static constexpr int32 NUM_TIME_MODEL_INFOS = 169;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_timeModelInfoStore, 0xB1C960, (CStore<CTimeModelInfo, NUM_TIME_MODEL_INFOS>));

    static constexpr int32 NUM_LOD_TIME_MODEL_INFOS = 1;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_lodTimeModelInfoStore, 0xB1E128, (CStore<CLodTimeModelInfo, NUM_LOD_TIME_MODEL_INFOS>));

    static constexpr int32 NUM_WEAPON_MODEL_INFOS = 51;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_weaponModelInfoStore, 0xB1E158, (CStore<CWeaponModelInfo, NUM_WEAPON_MODEL_INFOS>));

    static constexpr int32 NUM_CLUMP_MODEL_INFOS = 92;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_clumpModelInfoStore, 0xB1E958, (CStore<CClumpModelInfo, NUM_CLUMP_MODEL_INFOS>));

    static constexpr int32 NUM_VEHICLE_MODEL_INFOS = 212;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_vehicleModelInfoStore, 0xB1F650, (CStore<CVehicleModelInfo, NUM_VEHICLE_MODEL_INFOS>));

    static constexpr int32 NUM_PED_MODEL_INFOS = 278;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_pedModelInfoStore, 0xB478F8, (CStore<CPedModelInfo, NUM_PED_MODEL_INFOS>));

    static constexpr int32 NUM_2DFX_INFOS = 100;
    static NOTSA_GLOBAL_DECL(CModelInfo, ms_2dFXInfoStore, 0xB4C2D8, (CStore<C2dEffect, NUM_2DFX_INFOS>));

public:
    static void InjectHooks();

    static void ReInit2dEffects();
    static void ShutDown();
    static CAtomicModelInfo* AddAtomicModel(int32 index);
    static CDamageAtomicModelInfo* AddDamageAtomicModel(int32 index);
    static CLodAtomicModelInfo* AddLodAtomicModel(int32 index);
    static CTimeModelInfo* AddTimeModel(int32 index);
    static CLodTimeModelInfo* AddLodTimeModel(int32 index);
    static CWeaponModelInfo* AddWeaponModel(int32 index);
    static CClumpModelInfo* AddClumpModel(int32 index);
    static CVehicleModelInfo* AddVehicleModel(int32 index);
    static class CPedModelInfo* AddPedModel(int32 index);
    static void Initialise();
    static CBaseModelInfo* GetModelInfo(const char* name, int32* index = nullptr);
    static CBaseModelInfo* GetModelInfoFromHashKey(uint32 uiHash, int32* index = nullptr);
    static CBaseModelInfo* GetModelInfoUInt16(const char* name, uint16* pOutIndex = nullptr);
    // get model in range (search for model only in range (min;max))
    static CBaseModelInfo* GetModelInfo(const char* name, int32 minIndex, int32 maxIndex);
    static CStore<C2dEffect, NUM_2DFX_INFOS>* Get2dEffectStore();
    static bool IsBoatModel(int32 index);
    static bool IsCarModel(int32 index);
    static bool IsTrainModel(int32 index);
    static bool IsHeliModel(int32 index);
    static bool IsPlaneModel(int32 index);
    static bool IsBikeModel(int32 index);
    static bool IsFakePlaneModel(int32 index);
    static bool IsMonsterTruckModel(int32 index);
    static bool IsQuadBikeModel(int32 index);
    static bool IsBmxModel(int32 index);
    static bool IsTrailerModel(int32 index);
    // return -1 if model is not a vehicle model otherwise returns vehicle model type
    static int32 IsVehicleModelType(int32 index);

    static CBaseModelInfo* GetModelInfo(int32 index) { return ms_modelInfoPtrs[index]; }
    static auto GetPedModelInfo(int32 index) { return GetModelInfo(index)->AsPedModelInfoPtr(); }
    static auto GetVehicleModelInfo(int32 index) { return GetModelInfo(index)->AsVehicleModelInfoPtr(); }
    static void SetModelInfo(int32 index, CBaseModelInfo* pInfo) { ms_modelInfoPtrs[index] = pInfo; }
};
