/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "PopCycle.h"
#include "Core/X87Intrinsics.h"
#include <CustomBuildingDNPipeline.h>

void CPopCycle::InjectHooks() {
    RH_ScopedClass(CPopCycle);
    RH_ScopedCategoryGlobal();

    RH_ScopedGlobalInstall(Initialise, 0x5BC090);
    RH_ScopedGlobalInstall(PickGangToCreateMembersOf, 0x60F8D0);
    RH_ScopedGlobalInstall(FindNewPedType, 0x60FBD0);
    RH_ScopedGlobalInstall(PickPedMIToStreamInForCurrentZone, 0x60FFD0);
    RH_ScopedGlobalInstall(IsPedAppropriateForCurrentZone, 0x610150);
    RH_ScopedGlobalInstall(IsPedInGroup, 0x610210);
    RH_ScopedGlobalInstall(PickARandomGroupOfOtherPeds, 0x610420);
    RH_ScopedGlobalInstall(PlayerKilledADealer, 0x610490);
    RH_ScopedGlobalInstall(UpdateDealerStrengths, 0x6104B0);
    RH_ScopedGlobalInstall(UpdateAreaDodgyness, 0x610560);
    RH_ScopedGlobalInstall(UpdateIsGangArea, 0x6106D0);
    RH_ScopedGlobalInstall(PedIsAcceptableInCurrentZone, 0x610720);
    RH_ScopedGlobalInstall(UpdatePercentages, 0x610770);
    RH_ScopedGlobalInstall(Update, 0x610BF0);
    RH_ScopedGlobalInstall(GetCurrentPercOther_Peds, 0x610310);
}

// 0x5BC090
void CPopCycle::Initialise() {
    CFileMgr::SetDir("DATA");
    const auto file = CFileMgr::OpenFile("POPCYCLE.DAT", "r");
    CFileMgr::SetDir("");

    const notsa::ScopeGuard autoCloser{ [&] { CFileMgr::CloseFile(file); } };

    auto nline{ 1u };
    for (auto zone = 0u; zone < +eZonePopulationType::COUNT; zone++) {
        for (auto wktime = 0; wktime < 2; wktime++) { // weekday (0) / weekend(1)
            for (auto daytime = 0; daytime < 24 / PERC_DATA_TIME_RESOLUTION_HR; daytime++) {
                // Find next suitable data line
                const char* l{};
                while (true) {
                    l = CFileLoader::LoadLine(file);
                    if (!l) {
                        NOTSA_UNREACHABLE("Expected more data, got EOF!");
                    }
                    nline++;
                    if (l[0] != '/' && l[0]) {
                        break;
                    }
                }

                // Ideally we could/would use a file pointer here (instead of `LoadLine`)
                // and read each number one-by-one.
                // But until then we're stuck with this hardcoded version.

                auto& percs = m_nPercTypeGroup[daytime][wktime][zone];
                const auto nread = sscanf_s(
                    l,
                    "%hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu %hhu",

                    &m_nMaxNumPeds[daytime][wktime][zone],
                    &m_nMaxNumCars[daytime][wktime][zone],

                    &m_nPercDealers[daytime][wktime][zone],
                    &m_nPercGang[daytime][wktime][zone],
                    &m_nPercCops[daytime][wktime][zone],
                    &m_nPercOther[daytime][wktime][zone],

                    &percs[0], &percs[1], &percs[2], &percs[3], &percs[4], &percs[5],
                    &percs[6], &percs[7], &percs[8], &percs[9], &percs[10], &percs[11],
                    &percs[12], &percs[13], &percs[14], &percs[15], &percs[16], &percs[17]
                );

                if (nread != 6 + 18) {
                    NOTSA_UNREACHABLE("Failed reading all data!");
                }

                // In the vanilla game the %'s in this array add up to 100% (The original code rescales the values in order to make sure this is the case...but fails sometimes, see below)
                // But we take another route and don't normalize to 100%. This way there's no rounding error involved and everything works perfectly. 
#ifndef FIX_BUGS
                // The percs should always be >= 100 in total (otherwise `PickARandomGroupOfOtherPeds` will fail)
                if (const auto percsSum = notsa::accumulate(percs, (size_t)0); percsSum < 100) {
                    for (auto& p : percs) {
                        p = (size_t)p * 100u / percsSum; // fp math unnecessary here - Rescale to 102% here to make sure we're at 100%
                    }

                    // At this point this must hold - If the value is over 100 that's fine, but it may not be less!
                    // assert(notsa::accumulate(percs, (size_t)0) >= 100u); // In vanilla game this always triggers because of rounding errors... Not much to do.
                }
#endif

            }
        }
    }
    NOTSA_LOG_DEBUG("POPCYCLE.DAT has been loaded successfully!");
}

// 0x60FBD0
bool CPopCycle::FindNewPedType(ePedType& outPedType, eModelID& outPedMI, bool noGangs, bool noCops) {
    // NOTSA: Bug prevention
    outPedMI = MODEL_INVALID;

    if (!m_pCurrZoneInfo) {
        return false;
    }

    if (CPopulation::bInPoliceStation && CGeneral::RandomBool(70.f)) {
        outPedType = PED_TYPE_COP;
        outPedMI = CPopulation::ChoosePolicePedOccupation();
        return true;
    }

    auto dealersChance = m_NumDealers_Peds - (float)CPopulation::ms_nNumDealers;

    auto gangChance = m_NumGangs_Peds - (float)CPopulation::CalculateTotalNumGangPeds();
    if (CPopulation::m_bOnlyCreateRandomGangMembers) {
        gangChance = 50.f;
    }
    if (CPopulation::m_bDontCreateRandomGangMembers || noGangs) {
        gangChance = -10.f;
    }

    auto copChance = m_NumCops_Peds - (float)CPopulation::ms_nNumCop;
    if (CGangWars::GangWarFightingGoingOn() || CPopulation::m_bDontCreateRandomCops || noCops || m_pCurrZoneInfo->IsNoCops) {
        copChance = -10.f;
    }

    auto civPedsChance = CPopCycle::m_NumOther_Peds - (float)(CPopulation::ms_nNumCivMale + CPopulation::ms_nNumCivFemale);

    for (auto chance : { &civPedsChance, &copChance, &dealersChance, &gangChance }) {
        if (*chance < 2.f) {
            *chance *= CGeneral::GetRandomNumberInRange(0.f, 1.f);
        }
    }

    if (!CGame::CanSeeOutSideFromCurrArea()) {
        dealersChance = -10.f;
    }

    // Pirulax: I had to refactor the code to be acceptable and bugless - sorry
    while (true) {
        const auto highestChance = std::max({ civPedsChance, copChance, dealersChance, gangChance });

        if (highestChance <= 0.f) {
            return false;
        }

        if (highestChance == dealersChance) {
            // 0x60FEF2:
            // Because originally there was no `return` inside the loop itself it always returned at the last viable ID.
            // we reverse the loop and just return at the first viable ID.
            // It seems intentional, but I'm unsure what the purpose was.
            for (auto modelId : CPopulation::GetModelsInPedGroup(CPopulation::GetPedGroupId(POPCYCLE_GROUP_DEALERS)) | rng::views::reverse) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    outPedMI = modelId;
                    assert(outPedMI != MODEL_PLAYER);
                    outPedType = PED_TYPE_DEALER;
                    return true;
                }
            }
            dealersChance = 0.f;
            continue;
        } else if (highestChance == gangChance) { // 0x60FF13
            outPedType = PickGangToCreateMembersOf();
            assert(IsPedTypeGang(outPedType));
            outPedMI = CPopulation::ChooseGangOccupation((eGangID)(outPedType - ePedType::PED_TYPE_GANG1));
            assert(outPedMI != MODEL_PLAYER);
            if (outPedMI >= 0) {
                return true;
            }
            if (CPopulation::m_bOnlyCreateRandomGangMembers) {
                return false;
            }
            gangChance = 0.f;
            continue;
        } else if (highestChance == copChance) { // 0x60FF62
            outPedMI = CPopulation::ChoosePolicePedOccupation();
            outPedType = PED_TYPE_COP;
            return true;
        } else if (highestChance == civPedsChance) { // 0x60FF8F
            outPedMI = CPopulation::ChooseCivilianOccupation();
            switch (outPedMI) {
            case MODEL_INVALID:
            case MODEL_MALE01:
                return false;
            }
            assert(outPedMI != MODEL_PLAYER);
            outPedType = CModelInfo::GetPedModelInfo(outPedMI)->m_nPedType;
            return true;
        } else {
            NOTSA_UNREACHABLE();
        }
    }
}

// 0x610310
int32 CPopCycle::GetCurrentPercOther_Peds() {
    // Exe: the answer is an INT (`_ftol` of `percOther * factor`, eax is returned and st0 popped), not a float
    const auto percOther = (int32)m_nPercOther[CPopCycle::m_nCurrentTimeIndex][CPopCycle::m_nCurrentTimeOfWeek][CPopCycle::m_nCurrentZoneType];

    // 0x610339: `1.0f - sqrt(Rain) * 0.8f`, spilled to a float
    float factor = 1.f - x87::sqrt(CWeather::Rain) * 0.8f;

    if (CTheScripts::IsPlayerOnAMission()) {
        if (const auto plyr = FindPlayerPed()) {
            if (plyr->IsInVehicle()) {
                switch (plyr->m_pVehicle->m_nModelIndex) {
                case MODEL_TAXI:
                case MODEL_CABBIE:
                    factor = 1.f;
                    break;
                }
            }
        }
    }

    if (CDarkel::FrenzyOnGoing()) { // 0x610398: `fld 1.0f` instead of the factor
        factor = 1.f;
    }

    return (int32)((double)percOther * (double)factor);
}

// 0x610150
bool CPopCycle::IsPedAppropriateForCurrentZone(int32 modelIndex) {
    if (!IsRaceAllowedInCurrentZone((eModelID)modelIndex)) {
        return false;
    }

    // Check if any group active in this zone contains the given model
    for (auto grpId = 0; grpId < POPCYCLE_TOTAL_GROUP_PERCS; grpId++) { // 0x6101F6: the 18 civilian groups (the gang / dealer groups after them are not scanned)
        // Check if this group is active now (0x6101C0: the percentage OF THIS GROUP in the current zone's row must be non-zero)
        if (!m_nPercTypeGroup[m_nCurrentTimeIndex][m_nCurrentTimeOfWeek][m_pCurrZoneInfo->PopType][grpId]) {
            continue;
        }

        // Check if group contains this model
        if (notsa::contains(CPopulation::GetModelsInPedGroup(CPopulation::GetPedGroupId((ePopcycleGroup)(grpId), CPopulation::CurrentWorldZone)), modelIndex)) {
            return true;
        }
    }

    // No group currently active contains the given model
    return false;
}

// 0x610210
bool CPopCycle::IsPedInGroup(int32 modelIndex, ePopcycleGroup group) {
    for (auto pedGroup : CPopulation::GetPedGroupsOfGroup(group)) {
        for (auto mdlId : CPopulation::GetModelsInPedGroup(pedGroup)) {
            if (mdlId == modelIndex) {
                return true;
            }
        }
    }
    return false;
}

// 0x610720
bool CPopCycle::PedIsAcceptableInCurrentZone(int32 modelIndex) {
    if (!m_pCurrZoneInfo) {
        return false;
    }

    if (CCheat::IsZoneStreamingAllowed()) {
        return true;
    }

    if (IsRaceAllowedInCurrentZone((eModelID)modelIndex)) {
        return true;
    }

    return false;
}

// 0x610420
ePopcycleGroup CPopCycle::PickARandomGroupOfOtherPeds() {
    const auto& percs = m_nPercTypeGroup[m_nCurrentTimeIndex][m_nCurrentTimeOfWeek][m_pCurrZoneInfo->PopType];
    auto rndPerc = CGeneral::GetRandomNumberInRange(
        0,
#ifdef FIX_BUGS // See `Initialise` for an explanation
        (int32)notsa::accumulate(percs, (size_t)0)
#else
        100
#endif
    );
    for (auto [grpIdx, grpPerc] : rngv::enumerate(percs)) {
        if ((int32)(grpPerc) >= rndPerc) {
            return (ePopcycleGroup)grpIdx;
        }
        rndPerc -= (int32)(grpPerc);
    }
    NOTSA_UNREACHABLE();
    // 0x610480: the exe's search loop has no bound (known vanilla bug, the row can sum to < 100): it keeps going over the bytes that follow the row
    for (auto grpIdx = std::size(percs);; grpIdx++) {
        const auto grpPerc = reinterpret_cast<const uint8*>(&percs[0])[grpIdx];
        if ((int32)(grpPerc) >= rndPerc) {
            return (ePopcycleGroup)grpIdx;
        }
        rndPerc -= (int32)(grpPerc);
    }
}

// 0x60FFD0
eModelID CPopCycle::PickPedMIToStreamInForCurrentZone() {
    for (auto tr = 0; tr < 10; tr++) { // 10 tries
        // NOTE: This is `PickARandomGroupOfOtherPeds` inlined in the original code, but it differs from the
        // non-inlined version (random number distribution and `<` instead of `<=` when picking the group)
        const auto grpId = [] {
            const auto& percs = m_nPercTypeGroup[m_nCurrentTimeIndex][m_nCurrentTimeOfWeek][m_pCurrZoneInfo->PopType];
            auto        rnd   = (int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.f / 32768.f) * 100.f);
            size_t      grp   = 0;
            while (rnd >= (int32)percs[grp]) {
                rnd -= (int32)percs[grp];
                if (++grp >= std::size(percs)) { // BUG: Original code reads out of bounds here
                    grp = std::size(percs) - 1;
                    break;
                }
            }
            return (ePopcycleGroup)grp;
        }();
        const auto pedGrpId     = CPopulation::GetPedGroupId(grpId, CPopulation::CurrentWorldZone);
        const auto npeds        = CPopulation::GetNumPedsInGroup(pedGrpId);
        auto& nextPedToLoadSlot = CStreaming::ms_NextPedToLoadFromGroup[grpId];
        for (auto p = 0; p < npeds; p++) {
            nextPedToLoadSlot  = (nextPedToLoadSlot + 1) % npeds;
            const auto modelId = (eModelID)CPopulation::GetPedGroupModelId(pedGrpId, nextPedToLoadSlot);
            if (!notsa::contains(CStreaming::ms_pedsLoaded, modelId) && IsRaceAllowedInCurrentZone(modelId)) {
                return modelId;
            }
        }
    }
    return MODEL_INVALID;
}

// 0x610490
void CPopCycle::PlayerKilledADealer() {
    if (m_pCurrZoneInfo && m_pCurrZoneInfo->DealerStrength) {
        m_pCurrZoneInfo->DealerStrength--;
    }
}

// 0x610BF0
void CPopCycle::Update() {
    ZoneScoped;

    m_nCurrentTimeOfWeek = []() -> int32 {
        switch (CClock::GetGameWeekDay()) {
        case 0: // Not sure (Maybe Sunday)
        case 7: // Sunday
            return 1;
        case 1:  // Monday
            return CClock::GetGameClockHours() >= 20 ? 0 : 1;
        case 2:
        case 3:
        case 4:
        case 5:
            return 0;
        case 6: // Saturday
            return CClock::GetGameClockHours() >= 20 ? 1 : 0;
        }
        NOTSA_UNREACHABLE();
        return m_nCurrentTimeOfWeek; // 0x610C3E: weekdays above 7 leave the value untouched
    }();

    m_nCurrentTimeIndex = CClock::GetGameClockHours() / 2;

    if (const auto& pos = FindPlayerCentreOfWorld(); pos.z < 950.f || !m_pCurrZoneInfo) {
        m_pCurrZoneInfo = CTheZones::GetZoneInfo(pos, &m_pCurrZone);
        m_nCurrentZoneType = m_pCurrZoneInfo->PopType;
    }

    UpdatePercentages();
    UpdateDealerStrengths();
    UpdateAreaDodgyness();
    UpdateIsGangArea();
}

// 0x610560
void CPopCycle::UpdateAreaDodgyness() {
    m_fCurrentZoneDodgyness = std::min((float)m_pCurrZoneInfo->DealerStrength * 0.07f + (float)m_pCurrZoneInfo->GetSumOfGangDensity() * ExeRecip(100.f), 1.0f);
}

// 0x6104B0
void CPopCycle::UpdateDealerStrengths() {
    if (!CGangWars::bGangWarsActive) {
        return;
    }

    // 0x6104BE: only in the frame the game minute changes (the exe returns when the minute of the current and previous time are EQUAL)
    if (CTimer::m_snTimeInMilliseconds / 60000 == CTimer::m_snPreviousTimeInMilliseconds / 60000) {
        return;
    }

    // 0x6104E3: walks the used zone infos (CTheZones::TotalNumberOfZoneInfos), not the map zone count / the whole array
    for (auto& zone : CTheZones::GetZoneInfos()) {
        const auto Chk = [&](eGangID gangId) { return zone.GangStrength[gangId] > 10u; };
        if (!Chk(GANG_BALLAS) && !Chk(GANG_GROVE) && !Chk(GANG_VAGOS)) {
            continue;
        }

        constexpr float chances[]{ 0.05f, 0.2f, 0.3f, 0.35f, 0.4f, 0.5f, 0.55f, 0.6f, 0.65f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f }; // 0x8D24F0

        // 0x610505: the index is clamped to 14 and rand() is consumed whatever the dealer strength is; the chance is compared against `rand() * (1/32767)`
        const auto idx = std::min<size_t>(zone.DealerStrength, 14);
        if (!((float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL < chances[idx])) {
            continue;
        }

        if (zone.DealerStrength < 0xF) {
            zone.DealerStrength++;
        }
    }
}

// 0x610770
void CPopCycle::UpdatePercentages() {
    const auto zone = m_pCurrZoneInfo;
    const auto rcp  = ExeRecip(100.f); // 0x858C58 (0.01f)

    // 0x610781: the dealer share is CAPPED at 0.1 (`fcomp`/`jp`: 0.1f < x ? 0.1f : x), not floored
    {
        const float x = (float)zone->DealerStrength * rcp;
        m_fPercDealers = (0.1f < x) ? 0.1f : x;
    }

    // 0x6107AE: gangs capped at 0.5; cops derived from the (capped) gang share
    {
        const float g = (float)zone->GetSumOfGangDensity() * rcp;
        m_fPercGangs  = (0.5f < g) ? 0.5f : g;
    }
    if (!(m_fPercGangs < 0.15f)) { // 0x61083F (taken when >= or unordered)
        const float v = 0.3f - m_fPercGangs;
        m_fPercCops   = (0.03f > v) ? 0.03f : v;
    } else {
        m_fPercCops = (0.02f > m_fPercGangs) ? 0.02f : m_fPercGangs;
    }

    // 0x610881: jump table over `PopType - 4` (case 0: floor 0.1, case 1: floor 0.05, case 2: none at all)
    switch (zone->PopType) {
    case +eZonePopulationType::RESIDENTIAL_RICH:
    case +eZonePopulationType::SHOPPING_POSH:
    case +eZonePopulationType::AIRPORT:
        if (!(m_fPercCops > 0.1f)) {
            m_fPercCops = 0.1f;
        }
        break;
    case +eZonePopulationType::RESIDENTIAL_AVERAGE:
        if (!(m_fPercCops > 0.05f)) {
            m_fPercCops = 0.05f;
        }
        break;
    case +eZonePopulationType::BEACH:
    case +eZonePopulationType::GOLF_CLUB:
        m_fPercCops = 0.f;
        break;
    }

    // 0x610905: `(cops + gangs) + dealers`; <= 1 => other = ((1 - dealers) - gangs) - cops, else every share is multiplied by 1 / sum (NOT divided)
    const float sum = (m_fPercCops + m_fPercGangs) + m_fPercDealers;
    if (!(sum > 1.f)) {
        m_fPercOther = ((1.f - m_fPercDealers) - m_fPercGangs) - m_fPercCops;
    } else {
        const float inv = 1.f / sum;
        m_fPercOther    = 0.f;
        m_fPercDealers  = m_fPercDealers * inv;
        m_fPercGangs    = m_fPercGangs * inv;
        m_fPercCops     = inv * m_fPercCops;
    }

    // 0x61097E: share of the max count per group: `(table% * share) * 0.01f`, others' peds use the int from `GetCurrentPercOther_Peds`
    const auto ti = m_nCurrentTimeIndex, tw = m_nCurrentTimeOfWeek, zt = m_nCurrentZoneType;
    const float shareDealers = ((float)m_nPercDealers[ti][tw][zt] * m_fPercDealers) * rcp;
    const float shareGangs   = ((float)m_nPercGang[ti][tw][zt] * m_fPercGangs) * rcp;
    const float shareCops    = ((float)m_nPercCops[ti][tw][zt] * m_fPercCops) * rcp;
    const float shareOtherCars = ((float)m_nPercOther[ti][tw][zt] * m_fPercOther) * rcp;
    const float shareOtherPeds = ((float)GetCurrentPercOther_Peds() * m_fPercOther) * rcp; // 0x610A16

    // 0x610A49 + 0x610A57: in an LA riot area the ped limit is at least 20
    int32 maxPeds = GetMaxPedsCurrently();
    if (CGameLogic::LaRiotsActiveHere() && maxPeds <= 20) {
        maxPeds = 20;
    }

    // 0x610A6D + 0x610A87: riot light multiplier (-0.01 down to 0.6 while a riot is active and the DN balance is < 0.5, else +0.01 up to 1)
    const float dnInv = 1.f - CCustomBuildingDNPipeline::m_fDNBalanceParam;
    if (CGameLogic::LaRiotsActiveHere() && dnInv > 0.5f) {
        gfLaRiotsLightMult = gfLaRiotsLightMult - rcp;
        if (gfLaRiotsLightMult < 0.6f) {
            gfLaRiotsLightMult = 0.6f;
        }
    } else {
        gfLaRiotsLightMult = gfLaRiotsLightMult + rcp;
        if (gfLaRiotsLightMult > 1.f) {
            gfLaRiotsLightMult = 1.f;
        }
    }

    // 0x610AEC: peds
    const float fMaxPeds = (float)maxPeds;
    m_NumDealers_Peds = shareDealers * fMaxPeds;
    m_NumGangs_Peds   = shareGangs * fMaxPeds;
    m_NumCops_Peds    = shareCops * fMaxPeds;
    m_NumOther_Peds   = fMaxPeds * shareOtherPeds;

    // 0x610B41: cars
    const float fMaxCars = (float)GetMaxCarsCurrently();
    m_NumDealers_Cars = shareDealers * fMaxCars;
    m_NumGangs_Cars   = shareGangs * fMaxCars;
    m_NumCops_Cars    = shareCops * fMaxCars;
    m_NumOther_Cars   = fMaxCars * shareOtherCars;

    // 0x610B73
    if (CGameLogic::LaRiotsActiveHere()) {
        m_NumDealers_Cars *= 0.75f;
        m_NumGangs_Cars   *= 0.75f;
        m_NumCops_Cars    *= 0.75f;
        m_NumOther_Cars   *= 0.75f;
    }
}

// 0x60F8D0
ePedType CPopCycle::PickGangToCreateMembersOf() {
    if (CCheat::IsActive(CHEAT_GANGS_CONTROLS_THE_STREETS)) {
        // 0x60F8DC: `7 - (int)(rand01 * -8.0f)`: only GANG1..GANG8 (the last two gangs never appear with this cheat)
        return (ePedType)(PED_TYPE_GANG1 + CGeneral::GetRandomNumberInRange(0, 8));
    }

    // 0x60F90C: sum of the gang densities as float; nothing to pick from with a zero sum => type 0
    const float sumGangDensity = (float)m_pCurrZoneInfo->GetSumOfGangDensity();
    if (sumGangDensity <= 0.f) {
        return (ePedType)0;
    }

    // Both reciprocals are real divisions (spilled floats), the per-gang value is `strength * (1/sum) - numGang * (1/numGangPeds)`;
    // the best starts at 0.0 (gang 0), another gang must be strictly greater
    const float invSum = 1.f / sumGangDensity;
    const float invNum = 1.f / m_NumGangs_Peds;
    const auto  Value  = [&](size_t gang) { return (float)m_pCurrZoneInfo->GangStrength[gang] * invSum - (float)(int32)CPopulation::ms_nNumGang[gang] * invNum; };

    float  best    = 0.f;
    size_t bestIdx = 0;
    if (const float v = Value(0); v > best) {
        best = v;
    }
    for (size_t gang = 1; gang < TOTAL_GANGS; gang++) {
        if (const float v = Value(gang); v > best) {
            best    = v;
            bestIdx = gang;
        }
    }
    return (ePedType)((size_t)PED_TYPE_GANG1 + bestIdx);
}

// notsa
bool CPopCycle::IsRaceAllowedInCurrentZone(ePedRace race) {
    return race == RACE_DEFAULT || m_pCurrZoneInfo->PopRaces & (1 << (race - 1));
}

// notsa
bool CPopCycle::IsRaceAllowedInCurrentZone(eModelID pedModelId) {
    return IsRaceAllowedInCurrentZone(CModelInfo::GetPedModelInfo((int32)pedModelId)->GetRace());
}

void CPopCycle::UpdateIsGangArea() {
    m_bCurrentZoneIsGangArea = m_pCurrZoneInfo->GetSumOfGangDensity() > 20;
}
