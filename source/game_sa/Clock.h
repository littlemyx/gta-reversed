/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "ExeRecip.h"

class CClock {
public:
    static uint8 daysInMonth[12];
    static inline NOTSA_GLOBAL(bClockHasBeenStored, 0xB70144, (bool), {});
    static inline NOTSA_GLOBAL(ms_Stored_nGameClockSeconds, 0xB70148, (uint16), {});
    static inline NOTSA_GLOBAL(ms_Stored_nGameClockMinutes, 0xB7014A, (uint8), {});
    static inline NOTSA_GLOBAL(ms_Stored_nGameClockHours, 0xB7014B, (uint8), {});
    static inline NOTSA_GLOBAL(ms_Stored_nGameClockDays, 0xB7014C, (uint8), {});
    static inline NOTSA_GLOBAL(ms_Stored_nGameClockMonths, 0xB7014D, (uint8), {});
    static inline NOTSA_GLOBAL(CurrentDay, 0xB7014E, (uint8), {});
    static inline NOTSA_GLOBAL(ms_nGameClockSeconds, 0xB70150, (uint16), {});
    static inline NOTSA_GLOBAL(ms_nGameClockMinutes, 0xB70152, (uint8), {});
    static inline NOTSA_GLOBAL(ms_nGameClockHours, 0xB70153, (uint8), {});
    static inline NOTSA_GLOBAL(ms_nGameClockDays, 0xB70154, (uint8), {});
    static inline NOTSA_GLOBAL(ms_nGameClockMonth, 0xB70155, (uint8), {});
    static inline NOTSA_GLOBAL(ms_nLastClockTick, 0xB70158, (uint32), {});
    static inline NOTSA_GLOBAL(ms_nMillisecondsPerGameMinute, 0xB7015C, (uint32), {});

    static inline bool gbFreezeTime;

public:
    static void InjectHooks();

    static void Initialise(uint32 millisecondsPerGameMinute);
    static void Update();

    static uint16 GetGameClockMinutesUntil(uint8 hours, uint8 minutes);
    static bool GetIsTimeInRange(uint8 from, uint8 to);
    static void NormaliseGameClock();
    static void OffsetClockByADay(uint32 timeDirection);
    static void SetGameClock(uint8 hours, uint8 minutes, uint8 day);
    static void StoreClock();
    static void RestoreClock();

    // inlined and may have not original names
    static uint16 GetGameClockSeconds()   { return ms_nGameClockSeconds; } // 0x55F460
    static uint8  GetGameClockMinutes()   { return ms_nGameClockMinutes; } // 0x4410C0
    static uint8  GetGameClockHours()     { return ms_nGameClockHours; }   // 0x43A690
    static uint8  GetGameClockDays()      { return ms_nGameClockDays; }    // 0x4E7EF0
    static uint8  GetGameClockMonth()     { return ms_nGameClockMonth; }   // 0x4E7EE0
    static uint8  GetGameWeekDay()        { return CurrentDay; }           // NOTSA, maybe

    static float GetMinutesToday() { return (float)((int32)ms_nGameClockHours * 60 + (int32)ms_nGameClockMinutes) + (float)ms_nGameClockSeconds * ExeRecip(60.0f); } // 0x55F470: (hours * 60 + minutes) is an int, seconds * (1/60)
    static float GetHoursToday() { return (float)(CClock::GetGameClockMinutes()) / 60.0f + (float)(CClock::GetGameClockSeconds()) / 3600.0f + (float)(CClock::GetGameClockHours()); } // notsa

    static bool ClockHoursInRange(uint8 start, uint8 end) { return ms_nGameClockHours > start && ms_nGameClockHours < end; }
};
