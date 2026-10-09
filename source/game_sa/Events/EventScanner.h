/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "TaskTimer.h"
#include "AttractorScanner.h"

class CPed;
class CObject;

class CPedAcquaintanceScanner {
public:
    static inline auto& ms_fThresholdDotProduct = StaticRef<float>(0xC0B034);
    static inline auto& ms_nScanInterval        = StaticRef<int32>(0x8D2358); // 500
    static inline auto& ms_nLongInterval        = StaticRef<int32>(0x8D235C); // 3000
    static inline auto& ms_nShortInterval       = StaticRef<int32>(0x8D2360); // 200

    CTaskTimer m_timer;
    bool m_bScanAllowedScriptPed;
    bool m_bScanAllowedInVehicle;
    bool m_bScanAllowedScriptedTask;

    static void InjectHooks();

    void ScanForPedAcquaintanceEvents(CPed& ped, CEntity** entities, int32 count); // 0x607D80
    bool IsScanAllowed(CPed& ped);                                                  // 0x603A30
    static bool WantsToRiotAgainst(CPed* ped, CPed* other);                         // 0x603AF0
    int32 ScanCandidateForAcquaintance(CPed& ped, int32 acquaintanceId, int32 curIdx, CPed* candidate, CPed*& outPed, int32& outIdx); // 0x607560
    bool CreateAcquaintanceEvent(CPed& ped, int32 acquaintanceType, CPed* other);   // 0x606BA0

    void ScanForPedAcquaintances(CPed& ped, int32 acquaintanceId, CEntity** entities, int32 count, CPed*& outPed, int32& outIdx); // 0x607A90 - `acquaintanceId` is -1 for "any"

    void SetOnlyScriptPedAllowed() {
        m_bScanAllowedScriptPed    = true;
        m_bScanAllowedInVehicle    = false;
        m_bScanAllowedScriptedTask = false;
    }

    void TurnOffAllScanners() {
        m_bScanAllowedScriptPed    = false;
        m_bScanAllowedInVehicle    = false;
        m_bScanAllowedScriptedTask = false;
    }
};

class CVehiclePotentialCollisionScanner {
public:
    CTaskTimer m_timer;
    static void InjectHooks();
    void ScanForVehiclePotentialCollisionEvents(const CPed& ped, CEntity** entities, int32 count);
};

class CObjectPotentialCollisionScanner {
public:
    CTaskTimer m_timer;

    static void InjectHooks();
    void ScanForObjectPotentialCollisionEvents(CPed& ped); // 0x606890
};

class CSexyPedScanner {
public:
    CTaskTimer m_timer;

    static void InjectHooks();
    void ScanForSexyPedEvents(CPed& ped, CEntity** entities, int32 count); // 0x603BF0
};

class CNearbyFireScanner {
public:
    CTaskTimer m_timer;

    static void InjectHooks();
    void ScanForNearbyFireEvents(CPed& ped); // 0x603E70
};

VALIDATE_SIZE(CPedAcquaintanceScanner, 0x10);
VALIDATE_SIZE(CVehiclePotentialCollisionScanner, 0xC);
VALIDATE_SIZE(CObjectPotentialCollisionScanner, 0xC);
VALIDATE_SIZE(CSexyPedScanner, 0xC);
VALIDATE_SIZE(CNearbyFireScanner, 0xC);

class CEventScanner {
public:
    uint32                            m_nNextScanTime;
    CVehiclePotentialCollisionScanner m_vehiclePotentialCollisionScanner;
    CObjectPotentialCollisionScanner  m_objectPotentialCollisionScanner;
    CAttractorScanner                 m_attractorScanner;
    CPedAcquaintanceScanner           m_pedAcquaintanceScanner;
    CSexyPedScanner                   m_sexyPedScanner;
    CNearbyFireScanner                m_nearbyFireScanner;

    static inline auto& m_sDeadPedWalkingTimer = StaticRef<uint32>(0xC0B038);

public:
    static void InjectHooks();

    CEventScanner();
    ~CEventScanner() = default;

    void Clear();
    void ScanForEvents(CPed& ped);
    static void __stdcall ScanForPedPotentialCollisionEvents(CPed* ped, CPed* closestPed); // 0x606580
    void ScanForEventsNow(const CPed& ped, bool bDontScan);

    auto& GetAcquaintanceScanner() {
        return m_pedAcquaintanceScanner;
    }
};

VALIDATE_SIZE(CEventScanner, 0xD4);
