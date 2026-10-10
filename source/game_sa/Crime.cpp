#include "StdInc.h"

#include "Crime.h"
#include "PedType.h"
#include "eWantedLevel.h"

void CCrime::InjectHooks() {
    RH_ScopedClass(CCrime);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(FindImmediateDetectionRange, 0x531FC0);
    RH_ScopedInstall(ReportCrime, 0x532010);
}

// 0x531FC0
float CCrime::FindImmediateDetectionRange(eCrimeType CrimeType) {
    switch (CrimeType) {
    case CRIME_DESTROY_HELI:
    case CRIME_DESTROY_PLANE:
    case CRIME_EXPLOSION:
        return 60.0f;
    case CRIME_DESTROY_VEHICLE:
        return 30.0f;
    default:
        return 14.0f;
    }
}

// 0x532010
void CCrime::ReportCrime(eCrimeType crimeType, CEntity* pVictim, CPed* pCommitedby) {
    if (!pCommitedby || !pCommitedby->IsPlayer()) {
        return;
    }

    const bool isPedCriminal = pVictim && pVictim->GetIsTypePed() && CPedType::PoliceDontCareAboutCrimesAgainstPedType(pVictim->AsPed()->m_nPedType);

    if (crimeType == CRIME_DAMAGED_PED) {
        // NOTE: the exe dereferences the victim here without a null check
        if (pVictim->GetIsTypePed()
            && IsPedPointerValid(pVictim->AsPed())
            && pCommitedby->AsPlayer()->GetWantedLevel() == eWantedLevel::WANTED_CLEAN
            && pVictim->AsPed()->bBeingChasedByPolice // Vanilla bug here
        ) {
            if (!pVictim->AsPed()->IsStateDying()) {
                CMessages::AddBigMessage(TheText.Get("GOODBOY"), 5'000, eMessageStyle::STYLE_MIDDLE); // Good Citizen Bonus! +$50
                CWorld::Players[CWorld::PlayerInFocus].m_nMoney += 50; // unconditional, of the player in focus (0x5320DF)
            }
            return;
        }
    } else if (crimeType == CRIME_NONE) {
        return;
    }

    // 0x532102: the registration does NOT depend on the victim (null for e.g. weapon fire / explosions); the ped of the player in focus owns the wanted data
    const auto plyrWanted = FindPlayerPed()->GetPlayerWanted();
    if (plyrWanted->m_Multiplier >= 0.0f) {
        const auto  comittedByPos = pCommitedby->GetPosition();
        const auto  victimId      = (uint32)(uintptr_t)pVictim;
        if (CWanted::WorkOutPolicePresence(comittedByPos, FindImmediateDetectionRange(crimeType))
            || (notsa::contains({CRIME_DAMAGE_CAR, CRIME_DAMAGE_COP_CAR, CRIME_SET_PED_ON_FIRE, CRIME_SET_COP_PED_ON_FIRE}, crimeType) && CLocalisation::GermanGame())
        ) {
            plyrWanted->RegisterCrime_Immediately(crimeType, comittedByPos, victimId, isPedCriminal);
            FindPlayerPed()->GetPlayerWanted()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1); // We will never know if this is a bug or not.
        } else {
            plyrWanted->RegisterCrime(crimeType, comittedByPos, victimId, isPedCriminal);
        }
    }

    switch (crimeType) {
    case CRIME_DAMAGED_COP:   FindPlayerPed()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_1); break;
    case CRIME_DAMAGE_COP_CAR:
    case CRIME_STAB_COP:      FindPlayerPed()->SetWantedLevelNoDrop(eWantedLevel::WANTED_LEVEL_2); break;
    }
}
