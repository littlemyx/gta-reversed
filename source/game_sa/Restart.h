#pragma once

#include "Vector.h"

class CRestart {
public:
    static constexpr uint32 MAX_RESTART_POINTS{ 10u };

    static inline NOTSA_GLOBAL(bOverrideRespawnBasePointForMission, 0xA43248, (bool), {});
    static inline NOTSA_GLOBAL(OverrideRespawnBasePointForMission, 0xA4342C, (CVector), {});
    static inline NOTSA_GLOBAL(OverrideHeading, 0xA43260, (float), {});
    static inline NOTSA_GLOBAL(bOverrideRestart, 0xA43264, (bool), {});
    static inline NOTSA_GLOBAL(OverridePosition, 0xA43408, (CVector), {});

    static inline NOTSA_GLOBAL(bFadeInAfterNextArrest, 0xA4325C, (bool), {});
    static inline NOTSA_GLOBAL(bFadeInAfterNextDeath, 0xA4325D, (bool), {});

    static inline NOTSA_GLOBAL(NumberOfPoliceRestarts, 0xA43268, (uint16), {});
    static inline NOTSA_GLOBAL(PoliceRestartWhenToUse, 0xA43270, (int32[MAX_RESTART_POINTS]), {});
    static inline NOTSA_GLOBAL(PoliceRestartHeadings, 0xA43298, (float[MAX_RESTART_POINTS]), {});
    static inline NOTSA_GLOBAL(PoliceRestartPoints, 0xA43390, (CVector[MAX_RESTART_POINTS]), {});

    static inline NOTSA_GLOBAL(NumberOfHospitalRestarts, 0xA4326C, (uint16), {});
    static inline NOTSA_GLOBAL(HospitalRestartWhenToUse, 0xA432C0, (int32[MAX_RESTART_POINTS]), {});
    static inline NOTSA_GLOBAL(HospitalRestartHeadings, 0xA432E8, (float[MAX_RESTART_POINTS]), {});
    static inline NOTSA_GLOBAL(HospitalRestartPoints, 0xA43318, (CVector[MAX_RESTART_POINTS]), {});

    // Script command 2271 (COMMAND_SET_EXTRA_HOSPITAL_RESTART_POINT) arguments
    static inline NOTSA_GLOBAL(ExtraHospitalRestartCoors, 0xA43414, (CVector), {});
    static inline NOTSA_GLOBAL(ExtraHospitalRestartRadius, 0xA43258, (float), {});
    static inline NOTSA_GLOBAL(ExtraHospitalRestartHeading, 0xA43254, (float), {});

    // Script command 2272 (COMMAND_SET_EXTRA_POLICE_RESTART_POINT) arguments
    static inline NOTSA_GLOBAL(ExtraPoliceStationRestartCoors, 0xA43420, (CVector), {});
    static inline NOTSA_GLOBAL(ExtraPoliceStationRestartRadius, 0xA43250, (float), {});
    static inline NOTSA_GLOBAL(ExtraPoliceStationRestartHeading, 0xA4324C, (float), {});

public:
    static void InjectHooks();

    static void Initialise();
    static void AddHospitalRestartPoint(const CVector& point, float angle, int32 townId);
    static void AddPoliceRestartPoint(const CVector& point, float angle, int32 townId);
    static void OverrideNextRestart(CVector const& point, float angle);
    static void CancelOverrideRestart();
    static void SetRespawnPointForDurationOfMission(CVector point);
    static void ClearRespawnPointForDurationOfMission();
    static void FindClosestHospitalRestartPoint(CVector point, CVector& outPos, float& outAngle);
    static void FindClosestPoliceRestartPoint(CVector point, CVector& storedPoint, float& outAngle);
    static bool Load();
    static bool Save();
};
