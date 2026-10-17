/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Garage.h"

class CVehicle;
class CObject;
class CVector;

class CGarages {
public:
    static constexpr auto MAX_NUM_SAFEHOUSES{ 20 };
    static constexpr auto MAX_CARS_IN_SAFEHOUSE{ 4 };
    static constexpr auto MAX_NUM_GARAGES{ 50 };

    static inline NOTSA_GLOBAL(aCarsInSafeHouse, 0x96ABD8, (CStoredCar[MAX_NUM_SAFEHOUSES][MAX_CARS_IN_SAFEHOUSE]), {});
    static inline NOTSA_GLOBAL(aGarages, 0x96C048, (CGarage[MAX_NUM_GARAGES]), {}); static_assert(sizeof(aGarages) == 10800);
    static inline NOTSA_GLOBAL(MessageIDString, 0x96C014, (char[8]), {});
    static inline NOTSA_GLOBAL(LastGaragePlayerWasIn, 0x96BFDC, (int32), {});
    static inline NOTSA_GLOBAL(LastTimeHelpMessage, 0x96BFE0, (int32), {});
    static inline NOTSA_GLOBAL(bCamShouldBeOutside, 0x96BFE4, (bool), {});
    static inline NOTSA_GLOBAL(CrushedCarId, 0x96BFE8, (int32), {});
    static inline NOTSA_GLOBAL(CarTypesCollected, 0x96BFEC, (int32[MAX_CARS_IN_SAFEHOUSE]), {});
    static inline NOTSA_GLOBAL(PoliceCarsCollected, 0x96BFFC, (int32), {});
    static inline NOTSA_GLOBAL(BankVansCollected, 0x96C000, (int32), {});
    static inline NOTSA_GLOBAL(CarsCollected, 0x96C004, (int32), {});
    static inline NOTSA_GLOBAL(NoResprays, 0x96C008, (bool), {});
    static inline NOTSA_GLOBAL(RespraysAreFree, 0x96C009, (int8), {});
    static inline NOTSA_GLOBAL(BombsAreFree, 0x96C00A, (int8), {});
    static inline NOTSA_GLOBAL(PlayerInGarage, 0x96C00B, (int8), {});
    static inline NOTSA_GLOBAL(MessageNumberInString, 0x96C00C, (int32), {});
    static inline NOTSA_GLOBAL(MessageNumberInString2, 0x96C010, (int32), {});
    static inline NOTSA_GLOBAL(MessageEndTime, 0x96C01C, (uint32), {});
    static inline NOTSA_GLOBAL(MessageStartTime, 0x96C020, (uint32), {});
    static inline NOTSA_GLOBAL(NumGarages, 0x96C024, (int32), {});
    static inline NOTSA_GLOBAL(pOldToGarageWeAreIn, 0x96BFD8, (CGarage*), {});

public:
    static void InjectHooks();

    static void Init();
    static void Init_AfterRestart();
    static void Shutdown();
    static void Update();

    static int32 AddOne(float x1, float y1, float z1, float frontX, float frontY, float x2, float y2, float z2, uint8 type, uint32 a10, char* name, uint32 argFlags);
    static void CloseHideOutGaragesBeforeSave();
    static void PlayerArrestedOrDied();
    static void AllRespraysCloseOrOpen(bool state);

    static int32 FindSafeHouseIndexForGarageType(eGarageType type);
    static int16 FindGarageForObject(CObject*);
    static int16 FindGarageIndex(char* name);
    static float FindDoorHeightForMI(uint32 modelIndex);

    static bool IsModelIndexADoor(int32 model);
    static bool IsPointWithinHideOutGarage(Const CVector& point);
    static bool IsGarageOpen(int16 garageId);
    static bool IsGarageClosed(int16 garageId);
    static bool IsCarSprayable(CVehicle* vehicle);
    static bool IsPointInAGarageCameraZone(CVector point);
    static bool IsThisCarWithinGarageArea(int16 garageId, CEntity* entity);
    static bool IsPointWithinAnyGarage(CVector& point);

    static void ActivateGarage(int16 a1);
    static void DeActivateGarage(int16 garageId);
    static void SetTargetCarForMissionGarage(int16 garageId, CVehicle* vehicle);

    static int16 GetGarageNumberByName(const char* name);
    static void StoreCarInNearestImpoundingGarage(CVehicle* vehicle);

    static void ChangeGarageType(int16 garageId, eGarageType type, uint32 unused);
    static bool HasCarBeenDroppedOffYet(int16 garageId) { return GetGarage(garageId).m_nDoorState == GARAGE_DOOR_CLOSED_DROPPED_CAR; } // 0x447C90
    static bool HasResprayHappened(int16 garageId);
    static bool CameraShouldBeOutside() { return bCamShouldBeOutside; } // 0x448650
    static void GivePlayerDetonator();
    static void StopCarFromBlowingUp(CAutomobile* vehicle);
    static int32 CountCarsInHideoutGarage(eGarageType type);

    static void TriggerMessage(const char* tagMsg, int16 msgMin = -1, uint16 time = 5000, int16 msgMax = -1);
    static void PrintMessages();

    static bool Load();
    static bool Save();

    static CGarage&    GetGarage(int32 iGarageInd) { return aGarages[iGarageInd]; }
    static CStoredCar* GetStoredCarsInSafehouse(int32 iSafehouseInd) { return aCarsInSafeHouse[iSafehouseInd]; }
    static CStoredCar& GetStoredCar(int32 iSafehouseInd, int32 iCarInd) { return aCarsInSafeHouse[iSafehouseInd][iCarInd]; }
};
