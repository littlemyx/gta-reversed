#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group09_12.hpp"

#include "World.h"
#include "Camera.h"
#include "Credits.h"
#include "Localisation.h"
#include "Pad.h"
#include "Stats.h"
#include "Audio/AudioEngine.h"
#include "Fx/FxFtol.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;

/*!
* Script commands ported from the exe's group processor for ids 1000..1099 (`CRunningScript::ProcessCommands1000To1099` @0x489500)
* for the vanilla commands that had no handler of their own (S6-B, g10). Written from the asm of each `case`, see `.notes/S6B_TABLE.md`.
*/

namespace {
//! 1005 SET_UPSIDEDOWN_CAR_NOT_DAMAGED (case @0x48954A): car, flag -- the exe writes the CAutomobile flag byte (+0x868 bit 8) for ANY vehicle type
void SetUpsidedownCarNotDamaged(CVehicle& veh, int32 flag) {
    static_cast<CAutomobile&>(veh).autoFlags.bDoesNotGetDamagedUpsideDown = flag != 0;
}

//! 1011 GET_CAR_COLOURS (case @0x48963E): car => primary, secondary (2 values, zero-extended bytes at +0x434 / +0x435)
MultiRet<int32, int32> GetCarColours(CVehicle& veh) {
    return { (int32)veh.m_nPrimaryColor, (int32)veh.m_nSecondaryColor };
}

//! 1012 SET_ALL_CARS_CAN_BE_DAMAGED (case @0x489683): flag
void SetAllCarsCanBeDamaged(int32 flag) {
    if (flag) {
        CWorld::SetAllCarsCanBeDamaged(true);
    } else {
        CWorld::SetAllCarsCanBeDamaged(false);
        CWorld::ExtinguishAllCarFiresInArea(FindPlayerCoors(-1), 4000.0f); // 0x457A0000
    }
}

//! 1013 SET_CAR_CAN_BE_DAMAGED (case @0x4896E8): car, flag  -- +0x42A bit 0x20
void SetCarCanBeDamaged(CVehicle& veh, int32 flag) {
    if (flag) {
        veh.vehicleFlags.bCanBeDamaged = true;
    } else {
        veh.ExtinguishCarFire();
        veh.vehicleFlags.bCanBeDamaged = false;
    }
}

//! 1021 SET_DRUNK_INPUT_DELAY (case @0x489731): pad, delay  -- `CPad::GetPad(a)->SetDrunkInputDelay(b)` (0x53FB70, 0x53F910)
void SetDrunkInputDelay(int32 pad, int32 delay) {
    CPad::GetPad(pad)->SetDrunkInputDelay(delay);
}

//! 1024 GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS (case @0x48979F): object, x, y, z => x, y, z  (no null matrix check in the exe)
CVector GetOffsetFromObjectInWorldCoords(CObject& obj, CVector offset) {
    CVector res = TransformVectorOriginal(*obj.m_matrix, offset); // 0x59C790
    res += obj.GetPosition();                                     // 0x411A00 (matrix position if it has a matrix, else the placement one)
    return res;
}

//! 1031 GET_OFFSET_FROM_CAR_IN_WORLD_COORDS (case @0x489831): car, x, y, z => x, y, z
CVector GetOffsetFromCarInWorldCoords(CVehicle& veh, CVector offset) {
    CVector res = TransformVectorOriginal(*veh.m_matrix, offset);
    res += veh.GetPosition();
    return res;
}

//! 1036 IS_GERMAN_GAME (case @0x4898B4) => compare flag
bool IsGermanGame() {
    return CLocalisation::GermanGame();
}

//! 1037 CLEAR_MISSION_AUDIO (case @0x4898CB): slot (1-based)
void ClearMissionAudio(int32 slot) {
    AudioEngine.ClearMissionAudio((uint8)(slot - 1));
}

//! 1044 SET_FREE_HEALTH_CARE (case @0x4898F0): player, flag  -- `CWorld::Players[player].m_bFreeHealthCare` (+0x152)
void SetFreeHealthCare(int32 player, int32 flag) {
    CWorld::Players[player].m_bFreeHealthCare = flag != 0;
}

//! 1048 SET_OBJECT_DRAW_LAST (case @0x489AAE): object, flag  -- entity flags bit 0x4000 (m_bDrawLast)
void SetObjectDrawLast(CObject& obj, int32 flag) {
    obj.m_bDrawLast = flag != 0;
}

//! 1053 SET_NEAR_CLIP (case @0x489B53): nearClip
void SetNearClip(float nearClip) {
    TheCamera.SetNearClipScript(nearClip);
}

//! 1054 SET_RADIO_CHANNEL (case @0x489B74): channel (-1 = nothing; 11 is bumped to 12; then `inc al` -> id + 1 in the low byte only)
void SetRadioChannel(int32 channel) {
    if (channel == -1) {
        return;
    }
    if (channel == 11) {
        channel = 12;
    }
    const int32 id = (channel & ~0xFF) | ((channel + 1) & 0xFF); // `inc al` on eax
    AudioEngine.RetuneRadio((eRadioID)id);
}

//! 1059 SET_CAR_TRACTION (case @0x489BAE): car, traction -- automobiles: +0x8A0 (m_fCarTraction), ANY other type: +0x794 (bike traction)
void SetCarTraction(CVehicle& veh, float traction) {
    if (veh.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        static_cast<CAutomobile&>(veh).m_fCarTraction = traction;
    } else {
        static_cast<CBike&>(veh).m_fExtraTractionMult = traction;
    }
}

//! 1060 ARE_MEASUREMENTS_IN_METRES (case @0x489BF3) => compare flag
bool AreMeasurementsInMetres() {
    return CLocalisation::Metric();
}

//! 1061 CONVERT_METRES_TO_FEET (case @0x489C1A): metres => feet (float); 0x859F60 = 3.3333333f
float ConvertMetresToFeet(float metres) {
    return metres * 3.3333333f;
}

//! 1064 SET_CAR_AVOID_LEVEL_TRANSITIONS (case @0x489C48): car, flag  -- autopilot flags byte +0x3DB bit 4
void SetCarAvoidLevelTransitions(CVehicle& veh, int32 flag) {
    veh.m_autoPilot.carCtrlFlags.bAvoidLevelTransitions = flag != 0;
}

//! 1067 CLEAR_AREA_OF_CHARS (case @0x489C92): x1, y1, z1, x2, y2, z2 (corners are sorted)
void ClearAreaOfChars(float x1, float y1, float z1, float x2, float y2, float z2) {
    SortPair(x1, x2);
    SortPair(y1, y2);
    SortPair(z1, z2);
    CWorld::ClearPedsFromArea(x1, y1, z1, x2, y2, z2);
}

//! 1068 SET_TOTAL_NUMBER_OF_MISSIONS (case @0x489D5B): count  -- `CStats::SetStatValue(STAT_TOTAL_NUMBER_OF_MISSIONS_IN_GAME (0x94), (float)count)`
void SetTotalNumberOfMissions(int32 count) {
    CStats::SetStatValue(STAT_TOTAL_NUMBER_OF_MISSIONS_IN_GAME, (float)count);
}

//! 1069 CONVERT_METRES_TO_FEET_INT (case @0x489D82): metres => feet (int, truncated by `_ftol`)
int32 ConvertMetresToFeetInt(int32 metres) {
    return notsa::detail::Ftol((double)metres * (double)3.3333333f); // `fild`, `fmul` 0x859F60, `call _ftol`
}

//! 1070 REGISTER_FASTEST_TIME (case @0x489DB4): stat, time
void RegisterFastestTime(int32 stat, int32 time) {
    CStats::RegisterFastestTime((eStats)stat, time);
}

//! 1073 IS_CAR_PASSENGER_SEAT_FREE (case @0x489E73): car, seat => compare flag  (signed seat compare and unchecked array index, as in the exe)
bool IsCarPassengerSeatFree(CVehicle& veh, int32 seat) {
    if (seat >= (int32)veh.m_nMaxPassengers) {
        return false;
    }
    return veh.m_apPassengers.data()[seat] == nullptr;
}

//! 1076 START_CREDITS (case @0x489F56)
void StartCredits() {
    CCredits::Start();
}

//! 1077 STOP_CREDITS (case @0x489F62)
void StopCredits() {
    CCredits::Stop();
}

//! 1078 ARE_CREDITS_FINISHED (case @0x489F6E) => compare flag
bool AreCreditsFinished() {
    return !CCredits::bCreditsGoing; // 0xC6E97C
}

//! 1084 SET_MUSIC_DOES_FADE (case @0x489F94): flag  -- `TheCamera.m_bIgnoreFadingStuffForMusic` (+0x25) = !flag
void SetMusicDoesFade(int32 flag) {
    TheCamera.m_bIgnoreFadingStuffForMusic = flag == 0;
}

//! 1089 GET_CAR_MODEL (case @0x489FB3): car => model index (`movsx`: sign-extended word at +0x22)
int32 GetCarModel(CVehicle& veh) {
    return (int32)(int16)veh.m_nModelIndex;
}
}; // namespace

static_assert(offsetof(CPlayerInfo, m_bFreeHealthCare) == 0x152);
static_assert(offsetof(CVehicle, m_nMaxPassengers) == 0x488);
static_assert(offsetof(CBike, nBikeFlags) == 0x614 && offsetof(CAutomobile, autoFlags) == 0x868);
static_assert(offsetof(CCamera, m_bIgnoreFadingStuffForMusic) == 0x25);
static_assert(offsetof(CAutomobile, m_fCarTraction) == 0x8A0);
static_assert(offsetof(CBike, m_fExtraTractionMult) == 0x794);

void notsa::script::commands::ported::g09_12::RegisterG10() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g10");

    REGISTER_COMMAND_HANDLER(COMMAND_SET_UPSIDEDOWN_CAR_NOT_DAMAGED, SetUpsidedownCarNotDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_COLOURS, GetCarColours);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ALL_CARS_CAN_BE_DAMAGED, SetAllCarsCanBeDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_CAN_BE_DAMAGED, SetCarCanBeDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_DRUNK_INPUT_DELAY, SetDrunkInputDelay);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS, GetOffsetFromObjectInWorldCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OFFSET_FROM_CAR_IN_WORLD_COORDS, GetOffsetFromCarInWorldCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GERMAN_GAME, IsGermanGame);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_MISSION_AUDIO, ClearMissionAudio);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_FREE_HEALTH_CARE, SetFreeHealthCare);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_DRAW_LAST, SetObjectDrawLast);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_NEAR_CLIP, SetNearClip);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_RADIO_CHANNEL, SetRadioChannel);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_TRACTION, SetCarTraction);
    REGISTER_COMMAND_HANDLER(COMMAND_ARE_MEASUREMENTS_IN_METRES, AreMeasurementsInMetres);
    REGISTER_COMMAND_HANDLER(COMMAND_CONVERT_METRES_TO_FEET, ConvertMetresToFeet);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_AVOID_LEVEL_TRANSITIONS, SetCarAvoidLevelTransitions);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_AREA_OF_CHARS, ClearAreaOfChars);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TOTAL_NUMBER_OF_MISSIONS, SetTotalNumberOfMissions);
    REGISTER_COMMAND_HANDLER(COMMAND_CONVERT_METRES_TO_FEET_INT, ConvertMetresToFeetInt);
    REGISTER_COMMAND_HANDLER(COMMAND_REGISTER_FASTEST_TIME, RegisterFastestTime);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_PASSENGER_SEAT_FREE, IsCarPassengerSeatFree);
    REGISTER_COMMAND_HANDLER(COMMAND_START_CREDITS, StartCredits);
    REGISTER_COMMAND_HANDLER(COMMAND_STOP_CREDITS, StopCredits);
    REGISTER_COMMAND_HANDLER(COMMAND_ARE_CREDITS_FINISHED, AreCreditsFinished);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MUSIC_DOES_FADE, SetMusicDoesFade);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_MODEL, GetCarModel);
}
