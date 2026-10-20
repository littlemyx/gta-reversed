#include <StdInc.h>
#include <Stats.h>

#include <ZoneInfo.h>
#include "Commands.hpp"
#include <CommandParser/Parser.hpp>

namespace {
CZone* GetZoneByLabel(const char* label) {
    const auto zoneID = CTheZones::FindZoneByLabel(label, eZoneType::ZONE_TYPE_INFO);
    if (zoneID == -1) {
        return nullptr;
    }
    return &CTheZones::GetNavigationZones()[zoneID];
}

// The exe handlers (0x46B4DC, 0x46ECC0, 0x46ED23, 0x4718CD, 0x47962B: the SET_ commands) skip a missing label (`test ax, ax; jl`) and otherwise use &ZoneInfoArray[navZone.m_ZoneInfoIndex]
// with no null-for-index-0 special case (CTheZones::GetZoneInfo(const CZone*) returns nullptr for index 0, which made the port ignore a script writing to zone info 0)
CZoneInfo* GetZoneInfoByLabel(const char* label) {
    if (auto* const zone = GetZoneByLabel(label)) {
        return CTheZones::ZoneInfoArray.data() + zone->m_ZoneInfoIndex;
    }
    return nullptr;
}

// The exe handlers that do not check FindZoneByLabel's result (0x46B535 GET_ZONE_GANG_STRENGTH, 0x46ED64 GET_ZONE_DEALER_STRENGTH, 0x475222 SET_ZONE_FOR_GANG_WARS_TRAINING) index the
// navigation zones with the movsx'ed -1: the word at +0x1C of "zone -1" is TotalNumberOfNavigationZones (0xBA3794), so a missing label selects ZoneInfoArray[TotalNumberOfNavigationZones]
uint16 GetZoneInfoIndexByLabelUnchecked(const char* label) {
    if (auto* const zone = GetZoneByLabel(label)) {
        return zone->m_ZoneInfoIndex;
    }
    return (uint16)CTheZones::TotalNumberOfNavigationZones;
}

// SET_ZONE_POPULATION_TYPE (1895)
void SetZonePopulationType(const char* label, eZonePopulationType popType) {
    if (auto* const zi = GetZoneInfoByLabel(label)) {
        zi->PopType = +popType;
    }
}

// SET_ZONE_DEALER_STRENGTH (1898)
void SetZoneDealerStrength(const char* label, uint8 strength) {
    if (auto* const zi = GetZoneInfoByLabel(label)) {
        zi->DealerStrength = strength;
    }
}

// GET_ZONE_DEALER_STRENGTH (1899)
auto GetZoneDealerStrength(const char* label) {
    return CTheZones::ZoneInfoArray[GetZoneInfoIndexByLabelUnchecked(label)].DealerStrength;
}

// SET_ZONE_GANG_STRENGTH (1900)
void SetZoneGangStrength(const char* label, eGangID gang, uint8 strength) {
    if (auto* const zi = GetZoneInfoByLabel(label)) {
        zi->GangStrength[gang] = strength;
    }
}

// GET_ZONE_GANG_STRENGTH (1901)
auto GetZoneGangStrength(const char* label, eGangID gang) {
    return CTheZones::ZoneInfoArray[GetZoneInfoIndexByLabelUnchecked(label)].GangStrength[gang];
}

// todo
// GET_NAME_OF_ZONE (2115)
//void GetNameOfZone() {
//    
//}

// SET_ZONE_POPULATION_RACE (2164)
void SetZonePopulationRace(const char* label, uint8 races) {
    if (auto* const zi = GetZoneInfoByLabel(label)) {
        zi->PopRaces = races;
    }
}

// SET_ZONE_FOR_GANG_WARS_TRAINING (2227)
void SetZoneForGangWarsTraining(const char* label) {
    CGangWars::ZoneInfoForTraining = GetZoneInfoIndexByLabelUnchecked(label);
}

// INIT_ZONE_POPULATION_SETTINGS (2250)
void InitZonePopulationSettings() {
    CTheZones::InitZonesPopulationSettings();
}

// GET_CURRENT_POPULATION_ZONE_TYPE (2259)
auto GetCurrentPopulationZoneType() {
    return CPopCycle::m_nCurrentZoneType;
}

// TODO
// GET_NAME_OF_INFO_ZONE (2289)
//void GetNameOfInfoZone() {
//    NOTSA_UNREACHABLE("COMMAND_GET_NAME_OF_INFO_ZONE is not implemented");
//}

// SET_SPECIFIC_ZONE_TO_TRIGGER_GANG_WAR (2316)
void SetSpecificZoneToTriggerGangWar(const char* label) {
    CGangWars::SetSpecificZoneToTriggerGangWar(CTheZones::FindZoneByLabel(label, eZoneType::ZONE_TYPE_INFO));
}

// CLEAR_SPECIFIC_ZONES_TO_TRIGGER_GANG_WAR (2317)
void ClearSpecificZonesToTriggerGangWar() {
    CGangWars::ClearSpecificZonesToTriggerGangWar();
}

// SET_ZONE_NO_COPS (2487)
void SetZoneNoCops(const char* label, bool isNoCops) {
    if (auto* const zi = GetZoneInfoByLabel(label)) {
        zi->IsNoCops = isNoCops;
    }
}

// DISPLAY_ZONE_NAMES (2490)
void DisplayZoneNames(bool enabled) {
    CHud::bScriptDontDisplayAreaName = !enabled;
}
};

void notsa::script::commands::zone::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Zone");

    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_GET_RANDOM_COP_IN_ZONE);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_IS_PICKUP_IN_ZONE);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_SET_ZONE_CIVILIAN_CAR_INFO);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_GET_RANDOM_ICE_CREAM_CUSTOMER_IN_ZONE);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_SET_ZONE_RADAR_COLOURS);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ZONE_POPULATION_TYPE, SetZonePopulationType);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_SET_ZONE_GANG_CAP);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_GET_ZONE_GANG_CAP);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ZONE_DEALER_STRENGTH, SetZoneDealerStrength);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_ZONE_DEALER_STRENGTH, GetZoneDealerStrength);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ZONE_GANG_STRENGTH, SetZoneGangStrength);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_ZONE_GANG_STRENGTH, GetZoneGangStrength);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_SET_BEAT_ZONE_SIZE);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_IS_3D_COORD_IN_ZONE);
    //REGISTER_COMMAND_HANDLER(COMMAND_GET_NAME_OF_ZONE, GetNameOfZone);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ZONE_POPULATION_RACE, SetZonePopulationRace);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ZONE_FOR_GANG_WARS_TRAINING, SetZoneForGangWarsTraining);
    REGISTER_COMMAND_HANDLER(COMMAND_INIT_ZONE_POPULATION_SETTINGS, InitZonePopulationSettings);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CURRENT_POPULATION_ZONE_TYPE, GetCurrentPopulationZoneType);
    //REGISTER_COMMAND_HANDLER(COMMAND_GET_NAME_OF_INFO_ZONE, GetNameOfInfoZone);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_SPECIFIC_ZONE_TO_TRIGGER_GANG_WAR, SetSpecificZoneToTriggerGangWar);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_SPECIFIC_ZONES_TO_TRIGGER_GANG_WAR, ClearSpecificZonesToTriggerGangWar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ZONE_NO_COPS, SetZoneNoCops);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_ZONE_NAMES, DisplayZoneNames);
}
