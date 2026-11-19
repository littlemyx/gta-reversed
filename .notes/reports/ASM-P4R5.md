# ASM-P4R5: game-data loaders

Every listed loader was already MATCH/FIXED in earlier tsvs (handling, ped stats/type, weapon data, popcycle, ped/car groups, colours, upgrades, CFileLoader sections, cd directory, decision makers, anim files). New this slice: handling unit conversions (World/Game/Bike), GetHandlingId, FindPedType, CTheZones Init/CreateZone/AssignZoneInfo/InitZonesPopulationSettings, CClock::Initialise, CWeather::Init: 12 functions compared, 12 MATCH, 0 fixes, no source changed (no build). Open: none; CTimeCycle render fields not re-walked (already MATCH).
