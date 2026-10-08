#include "PedToPlayerConversations.h"

#include "Ragdoll/IKChainManager.h"

#include "TaskComplexSequence.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexKillPedOnFoot.h"
#include "EventScriptCommand.h"
#include "Pools/Pools.h"
#include "Clock.h"
#include "Stats.h"

// 0x43A6A0 - Used to select the (female/male?) reply of the player
static bool IsPedTypeForPositiveReply(ePedType type) {
    return type == PED_TYPE_PROSTITUTE || type == PED_TYPE_CIVFEMALE;
}

// Make `attacker` fight `victim` (inlined multiple times in the original)
static void MakePedAttackPed(CPed* attacker, CPed* victim) {
    auto* const seq = new CTaskComplexSequence();
    seq->AddTask(new CTaskSimpleStandStill(2000, false, false, 8.0f));
    seq->AddTask(new CTaskComplexKillPedOnFoot(victim, -1, 0, 0, 0, 0));

    CEventScriptCommand event{3, seq, false};
    attacker->GetEventGroup().Add(&event, false);
}

void CPedToPlayerConversations::InjectHooks() {
    RH_ScopedClass(CPedToPlayerConversations);
    RH_ScopedCategory("Conversations");
    RH_ScopedInstall(Clear, 0x43AAE0);
    RH_ScopedInstall(Update, 0x43B0F0);
    RH_ScopedInstall(EndConversation, 0x43AB10);
}

// 0x43AAE0
void CPedToPlayerConversations::Clear() {
    ZoneScoped;

    if (m_State != eP2pState::INACTIVE) {
        CAEPedSpeechAudioEntity::ReleasePlayerConversation();
        m_State = eP2pState::INACTIVE;
    }

    m_pPed                         = nullptr;
    m_TimeOfLastPlayerConversation = 0;
}

// 0x43B0F0
void CPedToPlayerConversations::Update() {
    ZoneScoped;

    auto* const player = FindPlayerPed();

    if (FindPlayerVehicle(-1, false)) {
        m_pPlayerVehicle = FindPlayerVehicle(-1, false);
        CEntity::RegisterReference(m_pPlayerVehicle);
    }

    const auto now = CTimer::GetTimeInMS();

    switch (m_State) {
    case eP2pState::INACTIVE: { // 0x43B15E
        if (now <= m_TimeOfLastPlayerConversation + 30'000u) {
            return;
        }
        if (FindPlayerVehicle(-1, false)) {
            return;
        }
        if (FindPlayerPed()->GetWantedLevel() != eWantedLevel::WANTED_CLEAN) {
            return;
        }
        if (CConversations::IsConversationGoingOn()) {
            return;
        }
        if (!player->PedIsReadyForConversation(true)) {
            return;
        }

        auto* const pedPool = GetPedPool();
        for (auto i = 0; i < 8; i++) {
            if (++m_NextPedIndexToCheck >= (int32)pedPool->GetSize()) {
                m_NextPedIndexToCheck = 0;
            }

            auto* const ped = pedPool->GetAt(m_NextPedIndexToCheck);
            if (!ped || !ped->IsCreatedBy(PED_GAME)) {
                continue;
            }
            if (!ped->PedIsReadyForConversation(true)) {
                continue;
            }
            if (ped->bHasAScriptBrain) {
                continue;
            }
            if (!ped->GetIsOnScreen()) {
                continue;
            }

            // Ped and player have to face each other
            if (DotProduct(ped->GetForwardVector(), player->GetPosition() - ped->GetPosition()) <= 0.f) {
                continue;
            }
            if (DotProduct(player->GetForwardVector(), ped->GetPosition() - player->GetPosition()) <= 0.f) {
                continue;
            }

            if (!((ped->GetPosition() - FindPlayerCoors()).Magnitude() < 7.f)) {
                continue;
            }

            // Pick a topic (some are for specific ped types only)
            m_Topic = (ped->m_nPedType == PED_TYPE_GANG1 || (ped->m_nPedType >= PED_TYPE_GANG3 && ped->m_nPedType <= PED_TYPE_GANG10)) // All gang peds, except Grove Street Families
                ? CGeneral::GetRandomNumberInRange(8, 10)
                : CGeneral::GetRandomNumberInRange(0, 7);
            if (!ped->m_pedSpeech.WillPedChatAboutTopic((int16)m_Topic)) {
                continue;
            }

            // Tries to start the conversation, returns whenever it was started
            const auto TryStart = [&](bool positiveOpening, eGlobalSpeechContext ctx) {
                if (!CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                    return false;
                }
                m_bPositiveOpening = positiveOpening;
                ped->Say(ctx, 0, 1.f, true, false, false);
                return true;
            };

            // Scales the stat with the random seed of the ped (so it varies from ped to ped)
            const auto StatWithSeed = [&](eStats stat, uint16 seedMask, float offset) {
                return (int32)((double)CStats::GetStatValue(stat) + (double)(ped->m_nRandomSeed & seedMask) - (double)offset);
            };

            const auto Success = [&] {
                m_State = eP2pState::PEDHASOPENED;
                m_pPed  = ped;
                CEntity::RegisterReference(m_pPed);
                m_TimeOfLastPlayerConversation = CTimer::GetTimeInMS();
                m_StartTime                    = CTimer::GetTimeInMS();
                ped->DisablePedSpeech(false);
                g_ikChainMan.LookAt("Ped2Pl_Conversation", ped, player, 100'000, (eBoneTag32)5, nullptr, false, 0.25f, 500, 8, false);
                g_ikChainMan.LookAt("Ped2Pl_ConversationP", player, ped, 100'000, (eBoneTag32)5, nullptr, false, 0.25f, 500, 8, false);
            };

            switch (m_Topic) {
            case 0: { // 0x43B380
                if (!m_pPlayerVehicle) {
                    break;
                }
                const auto delta = m_pPlayerVehicle->GetPosition() - ped->GetPosition();
                if (!(std::sqrt((double)delta.y * (double)delta.y + (double)delta.x * (double)delta.x) < 20.0)) {
                    break;
                }
                switch (CModelInfo::GetVehicleModelInfo(m_pPlayerVehicle->m_nModelIndex)->m_nVehicleClass) {
                case VEHICLE_CLASS_POORFAMILY:
                case VEHICLE_CLASS_WORKER:
                case VEHICLE_CLASS_BIG:
                case VEHICLE_CLASS_TAXI:
                    if (TryStart(false, (eGlobalSpeechContext)0x30)) {
                        return Success();
                    }
                    break;
                case VEHICLE_CLASS_RICHFAMILY:
                case VEHICLE_CLASS_EXECUTIVE:
                case VEHICLE_CLASS_LEISUREBOAT:
                    if (m_pPlayerVehicle->m_fHealth > 800.f && TryStart(true, (eGlobalSpeechContext)0x39)) {
                        return Success();
                    }
                    break;
                default:
                    break;
                }
                break;
            }
            case 1: { // 0x43B47C
                const auto v = StatWithSeed(STAT_CLOTHES_RESPECT, 0x1FF, 256.f);
                if (v > 500) {
                    if (TryStart(true, (eGlobalSpeechContext)0x3A)) {
                        return Success();
                    }
                } else if (v < 150) {
                    if (TryStart(false, (eGlobalSpeechContext)0x31)) {
                        return Success();
                    }
                }
                break;
            }
            case 2: { // 0x43B516
                const auto v = StatWithSeed(STAT_CLOTHES_RESPECT, 0x1FF, 256.f);
                if (!FindPlayerPed()->GetClothesDesc()->HasVisibleNewHairCut(0)) {
                    break;
                }
                if (v > 500) {
                    if (TryStart(true, (eGlobalSpeechContext)0x3B)) {
                        return Success();
                    }
                } else if (v < 150 || CStats::GetStatValue(STAT_HAIRDRESSING_BUDGET) < 5.f) {
                    if (TryStart(false, (eGlobalSpeechContext)0x32)) {
                        return Success();
                    }
                }
                break;
            }
            case 3: { // 0x43B5E9
                const auto muscle = StatWithSeed(STAT_MUSCLE, 0xFF, 128.f);
                const auto fat    = StatWithSeed(STAT_FAT, 0xFF, 128.f);
                if (muscle > 400 && fat < 250) {
                    if (TryStart(true, (eGlobalSpeechContext)0x3C)) {
                        return Success();
                    }
                } else if (muscle - fat < 200 && fat > 500) {
                    if (TryStart(false, (eGlobalSpeechContext)0x33)) {
                        return Success();
                    }
                }
                break;
            }
            case 4: { // 0x43B6BE
                const auto v = StatWithSeed(STAT_CLOTHES_RESPECT, 0x3FF, 512.f);
                if (v > 700) {
                    if (TryStart(true, (eGlobalSpeechContext)0x3D)) {
                        return Success();
                    }
                } else if (v < 300) {
                    if (TryStart(false, (eGlobalSpeechContext)0x34)) {
                        return Success();
                    }
                }
                break;
            }
            case 5: { // 0x43B758
                const auto v = StatWithSeed(STAT_SEX_APPEAL, 0x1FF, 256.f);
                if (v > 700) {
                    if (TryStart(true, (eGlobalSpeechContext)0x3E)) {
                        return Success();
                    }
                } else if (v < 300) {
                    if (TryStart(false, (eGlobalSpeechContext)0x35)) {
                        return Success();
                    }
                }
                break;
            }
            case 6: { // 0x43B7F1
                if (!FindPlayerPed()->GetClothesDesc()->HasVisibleTattoo()) {
                    break;
                }
                const auto r = CGeneral::GetRandomNumberInRange(0, 1000);
                if (r > 700) {
                    if (TryStart(true, (eGlobalSpeechContext)0x3F)) {
                        return Success();
                    }
                } else if (r < 300) {
                    if (TryStart(false, (eGlobalSpeechContext)0x36)) {
                        return Success();
                    }
                }
                break;
            }
            case 7: { // 0x43B890
                if (CWeather::Wind >= 0.1f) {
                    if (CWeather::Wind > 0.5f && TryStart(false, (eGlobalSpeechContext)0x37)) {
                        return Success();
                    }
                } else if (CClock::ClockHoursInRange(6, 20) && TryStart(true, (eGlobalSpeechContext)0x40)) {
                    return Success();
                }
                break;
            }
            case 8:   // 0x43B923
            case 9: { // 0x43B941
                if (CAEPedSpeechAudioEntity::RequestPlayerConversation(ped)) {
                    ped->Say((eGlobalSpeechContext)(m_Topic == 8 ? 0xFB : 0xFC), 0, 1.f, true, false, false);
                    m_bPositiveOpening = true;
                    return Success();
                }
                break;
            }
            default:
                break;
            }

            // 0x43B94E - Nothing started, small chance for the ped to say something anyway
            if ((rand() & 0xFFF) == 3) {
                ped->Say((eGlobalSpeechContext)0x58, 0, 1.f, false, false, false);
                return;
            }
        }
        break;
    }
    case eP2pState::PEDHASOPENED: { // 0x43BA38
        if (!m_pPed) {
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation();
            if (g_ikChainMan.IsLooking(player)) {
                g_ikChainMan.AbortLookAt(player, 250);
            }
            return;
        }

        if (now > m_StartTime + 4000u) { // The player didn't react in time
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation();
            m_pPed->EnablePedSpeech();
            if (g_ikChainMan.IsLooking(player)) {
                g_ikChainMan.AbortLookAt(player, 250);
            }
            if (g_ikChainMan.IsLooking(m_pPed)) {
                g_ikChainMan.AbortLookAt(m_pPed, 250);
                if (m_bPositiveOpening) {
                    m_pPed->Say((eGlobalSpeechContext)0x38, 0, 1.f, false, false, false);
                }
            }
            return;
        }

        const auto PlayerSays = [&](int32 ctx) {
            player->Say((eGlobalSpeechContext)ctx, 0, 1.f, true, false, false);
        };

        const auto SetState = [&](eP2pState state) {
            m_State     = state;
            m_StartTime = CTimer::GetTimeInMS();
        };

        // 0x43BB16
        if (CPad::GetPad(0)->ConversationYesJustDown()) {
            if (!m_bPositiveOpening) { // 0x43BE19
                PlayerSays(IsPedTypeForPositiveReply(m_pPed->m_nPedType) ? 0x83 : 0x84);
                return SetState(eP2pState::WAITINGTOFINISH);
            }

            switch (m_Topic) {
            case 0: PlayerSays(0x81); break;
            case 1: PlayerSays(0x82); break;
            case 2: PlayerSays(0x86); break;
            case 3: PlayerSays(0x89); break;
            case 4: PlayerSays(0x8A); break;
            case 5: PlayerSays(0x8B); break;
            case 6: PlayerSays(0x8C); break;
            case 7: PlayerSays(0xEA); break;
            case 8:   // 0x43BBEF
            case 9: { // 0x43BCF8
                PlayerSays(m_Topic == 8 ? 0xEC : 0x6B);
                if (CGeneral::GetRandomNumberInRange(0, 100) < 75) {
                    MakePedAttackPed(m_pPed, player);
                }
                return EndConversation();
            }
            default:
                break;
            }
            return SetState(eP2pState::WAITINGTOFINISH);
        }

        if (!CPad::GetPad(0)->ConversationNoJustDown()) { // 0x43BE66
            return;
        }

        if (!m_bPositiveOpening) { // 0x43BF1B
            PlayerSays(IsPedTypeForPositiveReply(m_pPed->m_nPedType) ? 0x87 : 0x88);

            if (m_pPed->m_nPedType != PED_TYPE_GANG2 && CGeneral::GetRandomNumberInRange(0, 100) < 40) {
                MakePedAttackPed(m_pPed, player);
            }
            return SetState(eP2pState::WAITINGTOFINISH);
        }

        // 0x43BE8A
        if (m_Topic == 8) {
            PlayerSays(0xEB);
            return EndConversation();
        }
        if (m_Topic == 9) {
            PlayerSays(0x6A);
            return EndConversation();
        }
        PlayerSays(IsPedTypeForPositiveReply(m_pPed->m_nPedType) ? 0x83 : 0x84);
        return SetState(eP2pState::WAITINGFORFINALWORD);
    }
    case eP2pState::WAITINGFORFINALWORD: { // 0x43C086
        if (!m_pPed) { // 0x43BA40
            m_State = eP2pState::INACTIVE;
            CAEPedSpeechAudioEntity::ReleasePlayerConversation();
            if (g_ikChainMan.IsLooking(player)) {
                g_ikChainMan.AbortLookAt(player, 250);
            }
            return;
        }
        if (now <= m_StartTime + 3000u) {
            return;
        }
        if (m_pPed->GetPedTalking()) {
            return;
        }
        m_pPed->Say((eGlobalSpeechContext)0x85, 0, 1.f, true, false, false);
        m_State     = eP2pState::WAITINGTOFINISH;
        m_StartTime = CTimer::GetTimeInMS();
        break;
    }
    case eP2pState::WAITINGTOFINISH: { // 0x43C0CE
        if (now > m_StartTime + 2500u || !m_pPed) {
            EndConversation();
        }
        break;
    }
    default:
        break;
    }
}

// 0x43AB10
void CPedToPlayerConversations::EndConversation() {
    m_State           = eP2pState::INACTIVE;

    CAEPedSpeechAudioEntity::ReleasePlayerConversation();

    if (m_pPed) {
        m_pPed->EnablePedSpeech();
    }

    const auto player = FindPlayerPed(-1);
    if (g_ikChainMan.IsLooking(player)) {
        g_ikChainMan.AbortLookAt(player, 250);
    }

    if (m_pPed && g_ikChainMan.IsLooking(m_pPed)) {
        return g_ikChainMan.AbortLookAt(m_pPed, 250);
    }
}
