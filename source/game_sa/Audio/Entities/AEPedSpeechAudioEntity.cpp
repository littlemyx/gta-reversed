#include "StdInc.h"
#include <bitset>
#include "AEPedSpeechAudioEntity.h"
#include "AEPedlessSpeechAudioEntity.h"
#include "AEAudioUtility.h"
#include "PedClothesDesc.h"
#include <AEAudioHardware.h>
#include <Audio/Enums/eSoundBankSlot.h>

#include "PedSpeechAudioEntityLUTs.inc.h"

void CAEPedSpeechAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEPedSpeechAudioEntity, 0x85F310, 8);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Constructor, 0x4E4F10);
    RH_ScopedInstall(IsGlobalContextImportantForInterupting, 0x4E4600);
    RH_ScopedInstall(IsGlobalContextUberImportant, 0x4E46F0);
    RH_ScopedInstall(GetNextMoodToUse, 0x4E4700);
    RH_ScopedInstall(GetVoiceForMood, 0x4E4760);
    RH_ScopedInstall(CanWePlayScriptedSpeech, 0x4E4950);
    RH_ScopedInstall(GetSpeechContextVolumeOffset, 0x4E4AE0);
    RH_ScopedInstall(RequestPedConversation, 0x4E50E0);
    RH_ScopedInstall(ReleasePedConversation, 0x4E52A0);
    RH_ScopedInstall(GetCurrentCJMood, 0x4E53B0);
    RH_ScopedInstall(StaticInitialise, 0x5B98C0);
    RH_ScopedInstall(GetSpecificSpeechContext, 0x4E4470);
    RH_ScopedInstall(Service, 0x4E3710);
    RH_ScopedInstall(Reset, 0x4E37B0);
    RH_ScopedInstall(ReservePedConversationSpeechSlots, 0x4E37F0);
    RH_ScopedInstall(ReservePlayerConversationSpeechSlot, 0x4E3870);
    RH_ScopedInstall(RequestPlayerConversation, 0x4E38C0);
    RH_ScopedInstall(ReleasePlayerConversation, 0x4E3960);
    RH_ScopedInstall(SetUpConversation, 0x4E3A00);
    RH_ScopedInstall(GetAudioPedType, 0x4E3C60);
    RH_ScopedInstall(GetVoice, 0x4E3CD0);
    RH_ScopedInstall(DisableAllPedSpeech, 0x4E3EB0);
    RH_ScopedInstall(IsGlobalContextPain, 0x4E44F0);
    RH_ScopedInstall(SetCJMood, 0x4E3ED0);
    RH_ScopedInstall(EnableAllPedSpeech, 0x4E3EC0);
    RH_ScopedInstall(IsCJDressedInForGangSpeech, 0x4E4270);
    RH_ScopedInstall(GetSexForSpecialPed, 0x4E4260);
    RH_ScopedInstall(IsGlobalContextImportantForWidescreen, 0x4E46B0);
    RH_ScopedInstall(GetRepeatTime, 0x4E47E0);
    RH_ScopedInstall(LoadAndPlaySpeech, 0x4E4840);
    RH_ScopedInstall(GetNumSlotsPlayingContext, 0x4E49B0);
    RH_ScopedInstall(GetNextPlayTime, 0x4E49E0);
    RH_ScopedInstall(SetNextPlayTime, 0x4E4A20);
    RH_ScopedInstall(DisablePedSpeech, 0x4E56D0);
    RH_ScopedInstall(DisablePedSpeechForScriptSpeech, 0x4E5700);
    RH_ScopedInstall(CanPedSayGlobalContext, 0x4E5730);
    RH_ScopedInstall(GetVoiceAndTypeFromModel, 0x4E58C0);
    RH_ScopedInstall(GetSoundAndBankIDs, 0x4E5920);
    RH_ScopedInstall(CanWePlayGlobalSpeechContext, 0x4E5F10);
    RH_ScopedInstall(AddSayEvent, 0x4E6550);
    RH_ScopedInstall(Initialise, 0x4E68D0);
    RH_ScopedInstall(CanPedHoldConversation, 0x4E69E0);
    RH_ScopedInstall(IsGlobalContextImportantForStreaming, 0x4E4510);
    RH_ScopedInstall(EnablePedSpeech, 0x4E3F70);
    RH_ScopedInstall(EnablePedSpeechForScriptSpeech, 0x4E3F90);
    RH_ScopedInstall(StopCurrentSpeech, 0x4E3FB0);
    RH_ScopedInstall(GetSoundAndBankIDsForScriptedSpeech, 0x4E4400);
    RH_ScopedInstall(GetSexFromModel, 0x4E4200);
    RH_ScopedInstall(GetPedTalking, 0x4E3F50);
    RH_ScopedInstall(GetVoiceAndTypeForSpecialPed, 0x4E4170);
    RH_ScopedVMTInstall(UpdateParameters, 0x4E3520);
    RH_ScopedVMTInstall(AddScriptSayEvent, 0x4E4F70);
    RH_ScopedVMTInstall(Terminate, 0x4E5670);
    RH_ScopedVMTInstall(PlayLoadedSound, 0x4E5CD0);
    RH_ScopedVMTInstall(GetAllocatedVoice, 0x4E4120);
    RH_ScopedVMTInstall(WillPedChatAboutTopic, 0x4E5800);
    RH_ScopedVMTInstall(GetPedType, 0x4E4130);
    RH_ScopedVMTInstall(IsPedFemaleForAudio, 0x4E4150);
}

// 0x4E4F10
CAEPedSpeechAudioEntity::CAEPedSpeechAudioEntity() noexcept :
    m_PedAudioType{PED_TYPE_GEN} // The exe's ctor stores type 0 (and leaves the voice alone); only `Terminate` resets it to -1
{
    m_NextTimeCanSayPain.fill(0);
}

// 0x4E4950
tPedSpeechSlotID CAEPedSpeechAudioEntity::CanWePlayScriptedSpeech() {
    return GetFreeSpeechSlot();
}

// 0x4E4AE0
float CAEPedSpeechAudioEntity::GetSpeechContextVolumeOffset(eGlobalSpeechContextS16 gctx) {
    // exe 0x4E4B08 / 0x4E4BE2: t = clamp((fat - muscle) * 0.001f, -1, 1) kept in an x87 register; result = (t + 1.0) * 0.5 * (G - base) + base,
    // where G is a float at 0xB61C14 / 0xB61C18 that nothing ever writes (.bss => 0.0f)
    const auto CalculateStrainVolumeOffset = [](float base) -> float {
        x87d t = (x87d)CStats::GetStatValue(STAT_FAT);
        t = t - (x87d)CStats::GetStatValue(STAT_MUSCLE);
        t = t * (x87d)0.001f;
        if (t > (x87d)1.0f) {
            t = (x87d)1.0f;
        } else if (t < (x87d)-1.0f) {
            t = (x87d)-1.0f;
        }
        return (float)((t + (x87d)1.0) * (x87d)0.5 * ((x87d)0.0f - (x87d)base) + (x87d)base);
    };
    switch (gctx) {
    case CTX_GLOBAL_STOMACH_RUMBLE:        return -6.f; // 0x4E4CBB
    case CTX_GLOBAL_BREATHING:             return -12.f; // 0x4E4CC7
    case CTX_GLOBAL_PAIN_CJ_STRAIN:        return CalculateStrainVolumeOffset(-18.f); // 0x4E4B08
    case CTX_GLOBAL_PAIN_CJ_STRAIN_EXHALE: return CalculateStrainVolumeOffset(-21.f); // 0x4E4BE2
    case CTX_GLOBAL_PAIN_CJ_DROWNING:      return 3.f; // 0x4E4CBE
    default:                               return 0.f; // 0x4E4CD0
    }
}

// 0x4E50E0
bool CAEPedSpeechAudioEntity::RequestPedConversation(CPed* pedA, CPed* pedB) {
    if (s_bAllSpeechDisabled || s_bPedConversationHappening || s_bPlayerConversationHappening) {
        return false;
    }
    if (pedA->GetSpeechAE().GetAllocatedVoice() == pedB->GetSpeechAE().GetAllocatedVoice()) {
        return false;
    }
    const auto CheckCanPedTalk = [](CPed* p) {
        return !p->GetSpeechAE().IsAllSpeechDisabled()
            && p->CanPedHoldConversation()
            && !p->GetPedTalking()
            && CVector::DistSqr(TheCamera.GetPosition(), p->GetPosition()) <= sq(40.f);
    };
    if (!CheckCanPedTalk(pedA) || !CheckCanPedTalk(pedB)) {
        return false;
    }
    if (!ReservePedConversationSpeechSlots()) {
        return false;
    }

    s_bPedConversationHappening = true;
    s_pConversationPed1         = pedA;
    s_pConversationPed2         = pedB;

    SetUpConversation();

    return true;
}

// 0x4E52A0
void CAEPedSpeechAudioEntity::ReleasePedConversation() {
    if (!s_bPedConversationHappening) {
        return;
    }
    if (s_pConversationPedSlot1 < 0 || s_pConversationPedSlot2 < 0) { // 0x4E52B7: jumps to the flag clear
        s_bPedConversationHappening = false;
        return;
    }

    const auto ReleaseConversationFromSlot = [](CPed*& convoPed, auto& slotID) {
        auto& ss = s_PedSpeechSlots[slotID];
        switch (ss.Status) {
        case CAEPedSpeechSlot::eStatus::FREE:
        case CAEPedSpeechSlot::eStatus::RESERVED:
            break;
        default:
            ss.AudioEntity->StopCurrentSpeech();
        }
        ss       = {}; // Reset slot
        slotID   = -1;
        convoPed = nullptr;
    };
    ReleaseConversationFromSlot(s_pConversationPed1, s_pConversationPedSlot1);
    ReleaseConversationFromSlot(s_pConversationPed2, s_pConversationPedSlot2);

    s_bPedConversationHappening = false;
}

// 0x5B98C0
void CAEPedSpeechAudioEntity::StaticInitialise() {
    rng::fill(s_PedSpeechSlots, CAEPedSpeechSlot{});
    rng::fill(s_PhraseMemory, tPhraseMemory{});

    Reset();
    // exe 0x5B992B: the table is filled a second time right after `Reset` (the first fill is dead, but its 360 rand() calls are not)
    rng::generate(gGlobalSpeechContextNextPlayTime, []{
        return CTimer::GetTimeInMS() + CAEAudioUtility::GetRandomNumberInRange(3000, 7000);
    });

    s_pConversationPed1            = nullptr;
    s_pConversationPed2            = nullptr;
    s_pConversationPedSlot1        = -1; // exe 0x5B9977 / 0x5B997E: -1, not 0
    s_pConversationPedSlot2        = -1;
    s_pPlayerConversationPed       = nullptr;
    s_bPedConversationHappening    = false;
    s_bPlayerConversationHappening = false;
    rng::fill(s_Conversation, CTX_GLOBAL_UNK);

    s_nCJBasicMood        = MOOD_UNK;
    s_nCJGangBanging      = -1;
    s_nCJFat              = -1;
    s_nCJWellDressed      = -1;
    s_nCJMoodOverrideTime = 0;

    s_NextSpeechSlot     = 0;
    s_bAllSpeechDisabled = false;
    s_bAPlayerSpeaking   = false;
    s_bForceAudible      = false;
}

// 0x4E3710
void CAEPedSpeechAudioEntity::Service() { // static
    s_bForceAudible = false;
    for (auto&& [i, speech] : rngv::enumerate(s_PedSpeechSlots)) {
        // Waiting for sound to load, and has loaded?
        if (speech.Status == CAEPedSpeechSlot::eStatus::LOADING) {
            if (AEAudioHardware.IsSoundLoaded(speech.SoundBankID, speech.SoundID, (eSoundBankSlot)(SND_BANK_SLOT_SPEECH1 + i))) {
                speech.Status = CAEPedSpeechSlot::eStatus::WAITING;
            }
        }

        // Sound is now loaded, waiting to be played
        if (speech.Status == CAEPedSpeechSlot::eStatus::WAITING) {
            if (CTimer::GetTimeInMS() >= speech.StartPlaybackTime) {
                if (const auto ae = speech.AudioEntity) {
                    ae->PlayLoadedSound();
                } else {
                    speech.Status = CAEPedSpeechSlot::eStatus::FREE;
                }
            }
        }

        // `PlayLoadedSound` above might've modified the status, must check it again
        switch (speech.Status) {
        case CAEPedSpeechSlot::eStatus::REQUESTED:
        case CAEPedSpeechSlot::eStatus::PLAYING: {
            s_bForceAudible |= speech.ForceAudible;
        }
        }
    }
}

// 0x4E37B0
void CAEPedSpeechAudioEntity::Reset() {
    rng::generate(gGlobalSpeechContextNextPlayTime, []{
        return CTimer::GetTimeInMS() + CAEAudioUtility::GetRandomNumberInRange(3000, 7000);
    }); 
    s_nCJMoodOverrideTime = 0;
}

// 0x4E37F0
bool CAEPedSpeechAudioEntity::ReservePedConversationSpeechSlots() {
    // exe 0x4E37F0: the first two free slots in index order (0..4, the player's slot 5 is never taken); no round robin, `s_NextSpeechSlot` is not touched
    tPedSpeechSlotID slotA = -1, slotB = -1;
    for (tPedSpeechSlotID i = 0; i < (tPedSpeechSlotID)s_PedSpeechSlots.size() - 1; i++) {
        if (s_PedSpeechSlots[i].Status != CAEPedSpeechSlot::eStatus::FREE) {
            continue;
        }
        if (slotA == -1) {
            slotA = i;
        } else {
            slotB = i;
            break;
        }
    }
    if (slotB == -1) {
        return false;
    }

    const auto SetupSlot = [](int32 slot) {
        auto* const ss                   = &s_PedSpeechSlots[slot];
        ss->IsReservedForPedConversation = true;
        ss->Status                       = CAEPedSpeechSlot::eStatus::RESERVED;
    };
    SetupSlot(s_pConversationPedSlot1 = slotA);
    SetupSlot(s_pConversationPedSlot2 = slotB);

    return true;
}

// 0x4E3870
bool CAEPedSpeechAudioEntity::ReservePlayerConversationSpeechSlot() {
    // exe 0x4E3870: the first free slot of 0..4 in index order (no round robin, `s_NextSpeechSlot` is not touched)
    tPedSpeechSlotID slot = -1;
    for (tPedSpeechSlotID i = 0; i < (tPedSpeechSlotID)s_PedSpeechSlots.size() - 1; i++) {
        if (s_PedSpeechSlots[i].Status == CAEPedSpeechSlot::eStatus::FREE) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        return false;
    }
    auto* const ss                      = &s_PedSpeechSlots[slot];
    ss->Status                          = CAEPedSpeechSlot::eStatus::RESERVED;
    ss->IsReservedForPlayerConversation = true;
    s_pConversationPedSlot1             = slot;
    return true;
}

// 0x4E38C0
bool CAEPedSpeechAudioEntity::RequestPlayerConversation(CPed* ped) {
    if (s_bAllSpeechDisabled) {
        return false;
    }

    if (ped->GetSpeechAE().m_IsSpeechForScriptsDisabled || ped->GetSpeechAE().m_IsSpeechDisabled) {
        return false;
    }

    const auto player = FindPlayerPed();
    if (!player || player->GetSpeechAE().m_IsSpeechDisabled || player->GetSpeechAE().m_IsSpeechForScriptsDisabled) {
        return false;
    }

    if (   s_bPedConversationHappening
        || s_bPlayerConversationHappening
        || ped->GetPedTalking()
        || !ReservePlayerConversationSpeechSlot()
    ) {
        return false;
    }

    s_pPlayerConversationPed       = ped;
    s_bPlayerConversationHappening = true;

    return true;
}

// 0x4E3960
void CAEPedSpeechAudioEntity::ReleasePlayerConversation() {
    if (!s_bPlayerConversationHappening) {
        return;
    }
    s_bPlayerConversationHappening = false;
    if (s_pConversationPedSlot1 < 0) {
        return;
    }
    auto* const ss = &s_PedSpeechSlots[s_pConversationPedSlot1];
    switch (ss->Status) {
    case CAEPedSpeechSlot::eStatus::FREE:
    case CAEPedSpeechSlot::eStatus::RESERVED: {
        *ss = {};
    }
    }
    // exe 0x4E39DB: a slot that is playing / loading keeps everything but its player-conversation reservation (the port reset it whole)
    ss->IsReservedForPlayerConversation = false;
    s_pConversationPedSlot1             = -1;
    s_pPlayerConversationPed            = nullptr;
}

// 0x4E3A00
void CAEPedSpeechAudioEntity::SetUpConversation() {
    rng::fill(s_Conversation, CTX_GLOBAL_NO_SPEECH);

    auto PushConvo = [i = 0](eGlobalSpeechContext gctx) mutable {
        s_Conversation[i++] = gctx;
    };

    const auto PushConvoForPeds = [&](eGlobalSpeechContext ifFemale, eGlobalSpeechContext ifMale) {
        const auto PushConvoForPed = [&](const CPed* p) {
            switch (p->m_nPedType) {
            case PED_TYPE_PROSTITUTE:
            case PED_TYPE_CIVFEMALE: PushConvo(ifFemale); break;
            default:                 PushConvo(ifMale);   break;
            }
        };
        PushConvoForPed(s_pConversationPed2); // yes, ped 2, then ped 1
        PushConvoForPed(s_pConversationPed1);
    };

    // Greeting
    PushConvoForPeds(CTX_GLOBAL_PCONV_GREET_FEM, CTX_GLOBAL_PCONV_GREET_MALE);

    // Other stuff
    const auto r = CAEAudioUtility::GetRandomNumberInRange(1, 10);
    if (r <= 8) { // Combined cases 1, -> 4 + 5, -> 8
        const auto hasInitialState = r <= 4;

        s_ConversationLength = hasInitialState ? 3 : 2;

        // exe 0x4E3AA9 (cases 1 -> 4) / 0x4E3B79 (cases 5 -> 8, result unused): CAEAudioUtility::GetRandomNumberInRange(0, 1) is drawn in both; non-zero = good
        const auto isGood = CAEAudioUtility::GetRandomNumberInRange(0, 1) != 0;
        if (hasInitialState) { // For cases 1 -> 4 only
            PushConvo(isGood ?  CTX_GLOBAL_PCONV_STATE_GOOD :  CTX_GLOBAL_PCONV_STATE_BAD);
            PushConvo(isGood ?  CTX_GLOBAL_PCONV_AGREE_GOOD :  CTX_GLOBAL_PCONV_AGREE_BAD);
        }

        // A question
        PushConvo(CTX_GLOBAL_PCONV_QUESTION);

        // Whenever they agree with the answer (?)
        switch (CAEAudioUtility::GetRandomNumberInRange(0, 2)) {
        case 0: PushConvo(CTX_GLOBAL_PCONV_AGREE_GOOD); break;
        case 1: PushConvo(CTX_GLOBAL_PCONV_AGREE_BAD);  break;
        case 2: PushConvo(CTX_GLOBAL_PCONV_ANS_NO);     break;
        }

        // Part away (?)
        PushConvoForPeds(CTX_GLOBAL_PCONV_PART_FEM, CTX_GLOBAL_PCONV_PART_MALE);
    } else if (r <= 10) { // cases 9, 10
        s_ConversationLength = 1;

        PushConvo(CAEAudioUtility::GetRandomNumberInRange(0, 1) != 0 ?  CTX_GLOBAL_PCONV_STATE_GOOD :  CTX_GLOBAL_PCONV_STATE_BAD); // exe 0x4E3C1C
        PushConvo(CTX_GLOBAL_PCONV_DISMISS);
    } else {
        NOTSA_UNREACHABLE();
    }
}

// 0x4E3C60
eAudioPedType CAEPedSpeechAudioEntity::GetAudioPedType(const char* name) {
    static const auto mapping = notsa::make_mapping<std::string_view, eAudioPedType>({
        {"PED_TYPE_GEN",    PED_TYPE_GEN   },
        {"PED_TYPE_EMG",    PED_TYPE_EMG   },
        {"PED_TYPE_PLAYER", PED_TYPE_PLAYER},
        {"PED_TYPE_GANG",   PED_TYPE_GANG  },
        {"PED_TYPE_GFD",    PED_TYPE_GFD   },
        {"PED_TYPE_SPC",    PED_TYPE_SPC   },
    });
    return notsa::find_value_or(mapping, name, PED_TYPE_UNK);
}

// 0x4E3CD0
ePedSpeechVoiceS16 CAEPedSpeechAudioEntity::GetVoice(const char* name, eAudioPedTypeS16 type) {
    const auto DoLookUpIn = [name](auto&& mapping) {
        return notsa::find_value_or(mapping, name, VOICE_UNK);
    };
    switch (type) {
    case PED_TYPE_GEN:   return DoLookUpIn(gGenSpeechVoiceLookup);
    case PED_TYPE_EMG:   return DoLookUpIn(gEmgSpeechVoiceLookup);
    case PED_TYPE_PLAYER:return DoLookUpIn(gPlySpeechVoiceLookup);
    case PED_TYPE_GANG:  return DoLookUpIn(gGngSpeechVoiceLookup);
    case PED_TYPE_GFD:   return DoLookUpIn(gGfdSpeechVoiceLookup);
    case PED_TYPE_SPC:   return VOICE_UNK;
    default:             NOTSA_UNREACHABLE(); return VOICE_UNK; // 0x4E3E92: types above 4 => -1
    }
}

// 0x4E3EB0
void CAEPedSpeechAudioEntity::DisableAllPedSpeech() {
    s_bAllSpeechDisabled = true;
}

// 0x4E3ED0
void CAEPedSpeechAudioEntity::SetCJMood(eCJMood basicMood, uint32 overrideTimeMS, int16 isGangBanging, int16 isFat, int16 isWellDressed) {
    s_nCJMoodOverrideTime = CTimer::GetTimeInMS() + overrideTimeMS;
    s_nCJBasicMood  = [=]{
        switch (basicMood) {
        case MOOD_AR:
        case MOOD_CR:
        case MOOD_PR:
        case MOOD_WR: return basicMood;
        default:      return MOOD_UNK;
        }
    }();
    s_nCJGangBanging = isGangBanging;
    s_nCJFat         = isFat;
    s_nCJWellDressed = isWellDressed;
}


// 0x4E53B0
eCJMood CAEPedSpeechAudioEntity::GetCurrentCJMood() {
    const auto* const plyr = FindPlayerPed();
    if (!plyr) {
        return MOOD_CR;
    }

    // exe 0x4E557E: every path (wanted > 3, wanted > 1, mission-passed window, fall-through) ends here: the script override
    // (`SetCJMood`) is applied to the basic mood and to the gang/fat/well-dressed flags while `override_time > now` (unsigned, `jbe` skips it).
    // The flags themselves are always computed from the game state first (0x4E53DD..0x4E551F), so the override can never leave one stale.
    const bool isMoodOverrideActive = s_nCJMoodOverrideTime > CTimer::GetTimeInMS();

    const auto CheckCJIsWellDressed = [&] { // 0x4E53DF
        if (isMoodOverrideActive && s_nCJWellDressed != -1) {
            return (uint8)s_nCJWellDressed != 0; // 0x4E559B: only the low byte (cl) is kept
        }
        return CStats::GetStatValue(STAT_CLOTHES_RESPECT) >= 650.f
            && CStats::GetStatValue(STAT_FASHION_BUDGET) >= 10'000.f;
    };

    const auto CheckCJIsGangBanging = [&] { // 0x4E5440
        if (isMoodOverrideActive && s_nCJGangBanging != -1) {
            return (uint8)s_nCJGangBanging != 0; // 0x4E5598: low byte (bl)
        }
        if (IsCJDressedInForGangSpeech()) {
            return true;
        }
        auto& plyrGrp = plyr->GetPlayerGroup();
        switch (plyrGrp.GetMembership().CountMembersExcludingLeader()) {
        case 0: // No other members, just the player alone
            return false;
        case 1: { // Exactly one: 0x4E544F loops over all 8 membership slots (the leader included) and any match is enough
            for (auto* const mem : plyrGrp.GetMembership().GetMembers()) {
                if (mem->m_nPedType == PED_TYPE_GANG2) {
                    return true;
                }
                auto& memSpeech = mem->GetSpeechAE();
                if (memSpeech.m_PedAudioType == PED_TYPE_GANG
                    && notsa::contains({ VOICE_GNG_RYDER, VOICE_GNG_SWEET, VOICE_GNG_SMOKE }, (eGngSpeechVoices)memSpeech.m_VoiceID)) {
                    return true;
                }
            }
            return false;
        }
        default: // 2 or more
            return true;
        }
    };

    const auto CheckCJIsFat = [&] { // 0x4E54E0
        if (isMoodOverrideActive && s_nCJFat != -1) {
            return (uint8)s_nCJFat != 0; // 0x4E55A7: low byte (dl)
        }
        return CStats::GetStatValue(STAT_FAT) >= 600.f
            && (x87d)CStats::GetStatValue(STAT_FAT) - (x87d)200.f > (x87d)CStats::GetStatValue(STAT_MUSCLE); // exe 0x4E54E0: fsub keeps the x87 precision
    };

    const auto DeriveMood = [&](eCJMood basicMood) { // 0x4E55CC
        if (isMoodOverrideActive && s_nCJBasicMood != MOOD_UNK) { // 0x4E5592
            basicMood = s_nCJBasicMood;
        }
        switch (basicMood) {
        case MOOD_AR:
            return CheckCJIsGangBanging()
                ? MOOD_AG
                : MOOD_AR;
        case MOOD_CR: {
            if (CheckCJIsGangBanging()) {
                return MOOD_CG;
            }
            if (CheckCJIsFat()) {
                return MOOD_CF;
            }
            return CheckCJIsWellDressed()
                ? MOOD_CD
                : MOOD_CR;
        }
        case MOOD_PR:
            return CheckCJIsGangBanging()
                ? MOOD_PG
                : MOOD_PR;
        case MOOD_WR:
            return CheckCJIsGangBanging()
                ? MOOD_WG
                : MOOD_WR;
        default:
            return MOOD_CR;
        }
    };

    if (FindPlayerWanted()->GetWantedLevel() > eWantedLevel::WANTED_LEVEL_3) { // 0x4E5537
        return DeriveMood(MOOD_PR);
    }

    if (FindPlayerWanted()->GetWantedLevel() > eWantedLevel::WANTED_LEVEL_1) { // 0x4E554B
        return DeriveMood(MOOD_AR);
    }

    if (CTheScripts::LastMissionPassedTime != -1) { // 0x4E555C
        if (CTimer::GetTimeInMS() >= CTheScripts::LastMissionPassedTime) {
            if (CTimer::GetTimeInMS() < CTheScripts::LastMissionPassedTime + 180'000) {
                return DeriveMood(MOOD_WR);
            }
            return DeriveMood(MOOD_CR);
        } else {
            CTheScripts::LastMissionPassedTime = CTimer::GetTimeInMS();
        }
    }

    return DeriveMood(MOOD_CR); // 0x4E5579
}

// 0x4E4700
eCJMood __stdcall CAEPedSpeechAudioEntity::GetNextMoodToUse(eCJMood currMood) {
    switch (currMood) {
    case MOOD_AG:
    case MOOD_PR: return MOOD_AR;
    case MOOD_PG: return MOOD_AG;
    case MOOD_WG: return MOOD_CG;
    default:      return MOOD_CR;
    }
}

// 0x4E4760
ePedSpeechVoiceS16 __stdcall CAEPedSpeechAudioEntity::GetVoiceForMood(eCJMood mood) {
    // 0x4E4760: result = rand(0,1) + 2 * mood (voices are laid out pairwise: X, X2)
    const auto b = CAEAudioUtility::GetRandomNumberInRange(0, 1) != 0;
    switch (mood) {
    case MOOD_AG: return b ? VOICE_PLY_AG2 : VOICE_PLY_AG;
    case MOOD_AR: return b ? VOICE_PLY_AR2 : VOICE_PLY_AR;
    case MOOD_CD: return b ? VOICE_PLY_CD2 : VOICE_PLY_CD;
    case MOOD_CF: return b ? VOICE_PLY_CF2 : VOICE_PLY_CF;
    case MOOD_CG: return b ? VOICE_PLY_CG2 : VOICE_PLY_CG;
    case MOOD_CR: return b ? VOICE_PLY_CR2 : VOICE_PLY_CR;
    case MOOD_PG: return b ? VOICE_PLY_PG2 : VOICE_PLY_PG;
    case MOOD_PR: return b ? VOICE_PLY_PR2 : VOICE_PLY_PR;
    case MOOD_WG: return b ? VOICE_PLY_WG2 : VOICE_PLY_WG;
    case MOOD_WR: return b ? VOICE_PLY_WR2 : VOICE_PLY_WR;
    default:      NOTSA_UNREACHABLE(); return b ? VOICE_PLY_CR2 : VOICE_PLY_CR; // 0x4E47AD: default shares the MOOD_CR entry
    }
}

// 0x4E3EC0
void CAEPedSpeechAudioEntity::EnableAllPedSpeech() {
    s_bAllSpeechDisabled = false;
}

// 0x4E4270
bool CAEPedSpeechAudioEntity::IsCJDressedInForGangSpeech() {
    static constexpr struct {
        eClothesTexturePart ClothesPart;
        const char*         TexName;
    } GANG_SPEECH_CLOTHES[] = {
        { CLOTHES_TEXTURE_TORSO,   "hoodyagreen" },
        { CLOTHES_TEXTURE_TORSO,   "shirtbgang"  },
        { CLOTHES_TEXTURE_LEGS,    "tracktrgang" },
        { CLOTHES_TEXTURE_LEGS,    "denimsgang"  },
        { CLOTHES_TEXTURE_HATS,    "bandgang"    },
        { CLOTHES_TEXTURE_HATS,    "hockey"      },
        { CLOTHES_TEXTURE_HATS,    "capgang"     },
        { CLOTHES_TEXTURE_HATS,    "capgangback" },
        { CLOTHES_TEXTURE_HATS,    "capgangside" },
        { CLOTHES_TEXTURE_HATS,    "capgangover" },
        { CLOTHES_TEXTURE_HATS,    "capgangup"   },
        { CLOTHES_TEXTURE_GLASSES, "bandred3"    },
        { CLOTHES_TEXTURE_GLASSES, "bandblue3"   },
        { CLOTHES_TEXTURE_GLASSES, "bandgang3"   },
        { CLOTHES_TEXTURE_GLASSES, "bandblack3"  },
    };

    if (!FindPlayerPed(PED_TYPE_PLAYER1)) {
        return false;
    }

    if (CGameLogic::FindCityClosestToPoint(FindPlayerCoors()) != LEVEL_NAME_LOS_SANTOS) {
        return false;
    }

    return rng::any_of(GANG_SPEECH_CLOTHES, [pcd = FindPlayerPed()->GetClothesDesc()](auto& gc) {
        return pcd->m_anTextureKeys[gc.ClothesPart] == CKeyGen::GetUppercaseKey(gc.TexName);
    });
}

// 0x4E4600
bool __stdcall CAEPedSpeechAudioEntity::IsGlobalContextImportantForInterupting(eGlobalSpeechContext gCtx) {
    const auto ctxi = GetGlobalSpeechContextInfo(gCtx);
    return ctxi && ctxi->IsImportantForInterrupting; // NOTSA: Use context info lookup instead of static switch
    /*
    switch (gCtx) {
    case CTX_GLOBAL_ARREST:
    case CTX_GLOBAL_ARRESTED:
    case CTX_GLOBAL_JOIN_GANG_NO:
    case CTX_GLOBAL_JOIN_GANG_YES:
    case CTX_GLOBAL_JOIN_ME_ASK:
        return true;
    }
    return false;
    */
}

// 0x4E46F0 - unused
bool CAEPedSpeechAudioEntity::IsGlobalContextUberImportant(int16 gCtx) {
    return false;
}

// 0x4E44F0
bool __stdcall CAEPedSpeechAudioEntity::IsGlobalContextPain(eGlobalSpeechContext gCtx) {
    const auto ctxi = GetGlobalSpeechContextInfo(gCtx);
    return ctxi && ctxi->IsPain;
}

// 0x4E46B0
bool CAEPedSpeechAudioEntity::IsGlobalContextImportantForWidescreen(eGlobalSpeechContext gCtx) {
    switch (m_PedAudioType) {
    case PED_TYPE_GFD:
    case PED_TYPE_PLAYER:
        return true;
    }
    const auto ctxi = GetGlobalSpeechContextInfo(gCtx);
    return ctxi && ctxi->IsImportantForWidescreen; // NOTSA: Use context info lookup instead of static switch
    /*
    switch (gCtx) {
    case CTX_GLOBAL_ARREST:
    case CTX_GLOBAL_ARRESTED:
    case CTX_GLOBAL_HAVING_SEX:
        return true;
    }
    return false;
    */
}

// 0x4E4510
bool CAEPedSpeechAudioEntity::IsGlobalContextImportantForStreaming(eGlobalSpeechContext gCtx) {
    switch (m_PedAudioType) {
    case PED_TYPE_GFD:
    case PED_TYPE_PLAYER:
        return true;
    }
    const auto ctxi = GetGlobalSpeechContextInfo(gCtx);
    return ctxi && ctxi->IsImportantForStreaming; // NOTSA: Use context info lookup instead of static switch
    /*
    switch (gCtx) {
    case CTX_GLOBAL_ARREST:
    case CTX_GLOBAL_ARRESTED:
    case CTX_GLOBAL_CONV_DISL_CAR:
    case CTX_GLOBAL_CONV_DISL_CLOTHES:
    case CTX_GLOBAL_CONV_DISL_HAIR:
    case CTX_GLOBAL_CONV_DISL_PHYS:
    case CTX_GLOBAL_CONV_DISL_SHOES:
    case CTX_GLOBAL_CONV_DISL_SMELL:
    case CTX_GLOBAL_CONV_DISL_TATTOO:
    case CTX_GLOBAL_CONV_DISL_WEATHER:
    case CTX_GLOBAL_CONV_IGNORED:
    case CTX_GLOBAL_CONV_LIKE_CAR:
    case CTX_GLOBAL_CONV_LIKE_CLOTHES:
    case CTX_GLOBAL_CONV_LIKE_HAIR:
    case CTX_GLOBAL_CONV_LIKE_PHYS:
    case CTX_GLOBAL_CONV_LIKE_SHOES:
    case CTX_GLOBAL_CONV_LIKE_SMELL:
    case CTX_GLOBAL_CONV_LIKE_TATTOO:
    case CTX_GLOBAL_CONV_LIKE_WEATHER:
    case CTX_GLOBAL_JOIN_GANG_NO:
    case CTX_GLOBAL_JOIN_GANG_YES:
    case CTX_GLOBAL_JOIN_ME_ASK:
    case CTX_GLOBAL_PCONV_AGREE_BAD:
    case CTX_GLOBAL_PCONV_AGREE_GOOD:
    case CTX_GLOBAL_PCONV_ANS_NO:
    case CTX_GLOBAL_PCONV_DISMISS:
    case CTX_GLOBAL_PCONV_GREET_FEM:
    case CTX_GLOBAL_PCONV_GREET_MALE:
    case CTX_GLOBAL_PCONV_PART_FEM:
    case CTX_GLOBAL_PCONV_PART_MALE:
    case CTX_GLOBAL_PCONV_QUESTION:
    case CTX_GLOBAL_PCONV_STATE_BAD:
    case CTX_GLOBAL_PCONV_STATE_GOOD:
    return true;
    }
    return false;
    */
}

// 0x4E47E0
int16 CAEPedSpeechAudioEntity::GetRepeatTime(eGlobalSpeechContext gCtx) const {
    // exe 0x4E47E8: `test dx, dx; jl` / `cmp dx, 0x168; jge` - anything outside 0..0x167 is 0 (the exe table ends with a -1 terminator entry and has a 0x168 one)
    if (gCtx < 0 || gCtx >= CTX_GLOBAL_NUM) {
        return 0;
    }
    if (const auto* const ctxi = GetGlobalSpeechContextInfo(gCtx)) {
        return ctxi->RepeatTime;
    }
    return 0;
}

// 0x4E4840
void CAEPedSpeechAudioEntity::LoadAndPlaySpeech(uint32 playbackTimeOffsetMS) {
    auto* const speech = GetCurrentSpeech();
    switch (speech->Status) {
    case CAEPedSpeechSlot::eStatus::FREE:
    case CAEPedSpeechSlot::eStatus::RESERVED:
        break;
    default:
        return;
    }

    AEAudioHardware.LoadSound(m_BankID, m_SoundID, (eSoundBankSlot)(SND_BANK_SLOT_SPEECH1 + m_PedSpeechSlotID));
    // exe 0x4E4840: only these fields are written - `Sound` and the two conversation-reservation flags are kept
    // (a reserved conversation slot must go back to RESERVED, not FREE, when its speech stops)
    speech->Status            = CAEPedSpeechSlot::eStatus::LOADING;
    speech->SoundBankID       = m_BankID;
    speech->SoundID           = m_SoundID;
    speech->AudioEntity       = this;
    speech->StartPlaybackTime = CTimer::GetTimeInMS() + playbackTimeOffsetMS;
    speech->PedAudioType      = m_PedAudioType;
    speech->GCtx              = m_LastGCtx;
    speech->ForceAudible      = m_IsForcedAudible;
}

// 0x4E49B0
int32 CAEPedSpeechAudioEntity::GetNumSlotsPlayingContext(eGlobalSpeechContext gCtx) {
    // 0x4E49B8: `mov edx, 5` - the exe counts the 5 regular slots, not the player's (last) one
    return rng::count_if(s_PedSpeechSlots | rngv::take(s_PedSpeechSlots.size() - 1), [&](const CAEPedSpeechSlot& speech) {
        return speech.Status != CAEPedSpeechSlot::eStatus::FREE && speech.GCtx == gCtx;
    });
}

// 0x4E49E0
uint32 CAEPedSpeechAudioEntity::GetNextPlayTime(eGlobalSpeechContext gCtx) const {
    return GetNextPlayTimeRef(gCtx);
}

// 0x4E4A20
void CAEPedSpeechAudioEntity::SetNextPlayTime(eGlobalSpeechContext gCtx) {
    assert(gCtx < CTX_GLOBAL_NUM); // OG: `return;`

    if (const auto* const ctxi = GetGlobalSpeechContextInfo(gCtx)) {
        GetNextPlayTimeRef(gCtx) = CTimer::GetTimeInMS() + ctxi->RepeatTime + CAEAudioUtility::GetRandomNumberInRange(1, 1000);
    }
}

// 0x4E56D0
void CAEPedSpeechAudioEntity::DisablePedSpeech(int16 a1) {
    if (m_IsInitialized) {
        m_IsSpeechDisabled = true;
        if (a1)
            StopCurrentSpeech();
    }
}

// 0x4E5700
void CAEPedSpeechAudioEntity::DisablePedSpeechForScriptSpeech(int16 a1) {
    if (m_IsInitialized) {
        m_IsSpeechForScriptsDisabled = true;
        if (a1)
            StopCurrentSpeech();
    }
}

// 0x4E5730
bool CAEPedSpeechAudioEntity::CanPedSayGlobalContext(eGlobalSpeechContext gCtx) const {
    if (!m_IsInitialized) {
        return false;
    }
    const auto sCtx = GetSpecificSpeechContext(gCtx, m_PedAudioType);
    if (sCtx < 0) {
        return false;
    }
    // exe 0x4E5767 (jump table at 0x4E57E8, 5 entries): the ped types GEN, EMG, PLAYER and GANG look the context up in their own table;
    //       GFD (4) and every other type (the table's `ja`) return false. There is NO pain special case here (`GetSpecificSpeechContextInfo` switches to the
    //       pain table for pain contexts: the exe reads [sCtx][voice] of the ped type's table, e.g. GEN [15..21] for the pain contexts, and the player table).
    const auto voice = m_VoiceID;
    switch (m_PedAudioType) {
    case PED_TYPE_GEN:    return gGenSpeechLookup[sCtx][voice].IsUseable();
    case PED_TYPE_EMG:    return gEmgSpeechLookup[sCtx][voice].IsUseable();
    case PED_TYPE_PLAYER: return gPlySpeechLookup[sCtx][voice].IsUseable();
    case PED_TYPE_GANG:   return gGngSpeechLookup[sCtx][voice].IsUseable();
    default:              return false;
    }
}

// 0x4E58C0
bool CAEPedSpeechAudioEntity::GetVoiceAndTypeFromModel(eModelID modelId) {
    auto* const mi = CModelInfo::GetModelInfo(modelId)->AsPedModelInfoPtr();

    m_PedAudioType = mi->m_nPedAudioType; // 0x4E58D2: stored before the range check (the port never stored it)
    if ((int16)m_PedAudioType < 0 || (int16)m_PedAudioType >= PED_TYPE_NUM) {
        return false;
    }
    if (m_PedAudioType == PED_TYPE_SPC) {
        return GetVoiceAndTypeForSpecialPed(mi->m_nKey);
    }

    m_VoiceID = mi->m_nVoiceId; // 0x4E58FA: stored before the sign check
    if ((int16)m_VoiceID < 0) {
        return false;
    }

    mi->IncrementVoice();

    return true;
}

// 0x4E5920
int16 CAEPedSpeechAudioEntity::GetSoundAndBankIDs(eGlobalSpeechContext gCtx, eSpecificSpeechContext& outSpecificSpeechContext) {
    // Left out some ifs error checking ifs as they aren't necessary (They're gonna get caught in debug anyways)

    const auto sCtx = GetSpecificSpeechContext(gCtx, m_PedAudioType);
    if (sCtx == -1) {
        return -1;
    }
    outSpecificSpeechContext = sCtx;

    const auto voiceID = [&]() -> ePedSpeechVoiceS32 {
        if (IsGlobalContextPain(gCtx)) {
            return GetPainVoice();
        } else if (m_PedAudioType == PED_TYPE_PLAYER) {
            eCJMood mood = GetCurrentCJMood();
            NOTSA_LOG_DEBUG("GetCurrentCJMood: 0x{:x}", (int)mood);
            for (; ;mood = GetNextMoodToUse(mood)) {
                const auto voiceID = GetVoiceForMood(mood);
                if (mood == MOOD_CR || GetSpecificSpeechContextInfo(sCtx, gCtx, m_PedAudioType, voiceID)->IsUseable()) {
                    return voiceID;
                }
            }
        } else {
            return m_VoiceID;
        }
    }();
    if (voiceID == -1) {
        return -1;
    }
    m_BankID = GetVoiceSoundBank(gCtx, m_PedAudioType, voiceID);

    const auto* const ctx = GetSpecificSpeechContextInfo(sCtx, gCtx, m_PedAudioType, voiceID);
    if (!ctx || !ctx->IsUseable()) {
        return -1;
    }
    assert(ctx->FirstSoundID <= ctx->LastSoundID);

    const size_t numSounds = ctx->GetNumSounds();
    assert(numSounds > 0);

    // Find sound ID to use
    // If all possible sounds are in the PhraseMemory,
    // the least recently used one will be picked
    // NOTE: Below is a better version of what they did (without using an intermediary array)

    const auto GetPhraseIndexInMemory = [this](int32 soundID) -> int16 {
        for (auto&& [i, p] : rngv::enumerate(s_PhraseMemory)) {
            if (p.SoundID == soundID && p.BankID == m_BankID) {
                return i;
            }
        }
        return -1;
    };

    int16      maxPhraseIMemdx = -1;
    const auto rndOffset    = CAEAudioUtility::GetRandomNumberInRange<size_t>(0u, numSounds - 1); // exe 0x4E5B6F: GetRandomNumberInRange(0, last - first) is inclusive => [0, numSounds - 1]
    for (size_t i = 0; i < std::min<size_t>(20u, numSounds); i++) { // NOTE: Here you can tune the maximum tries (Default: 20)
        const auto soundIDCurr  = ctx->FirstSoundID + (int16)((rndOffset + i) % numSounds);
        const auto phraseMemIdx = GetPhraseIndexInMemory(soundIDCurr);

        // Not in memory at all? Good, pick this!
        if (phraseMemIdx == -1) {
            m_SoundID = soundIDCurr;
            break;
        }

        // Maybe this was used a longer time ago than the previous one?
        if (maxPhraseIMemdx < phraseMemIdx) {
            m_SoundID = soundIDCurr;
            maxPhraseIMemdx = phraseMemIdx;
        }
    }
    assert(m_SoundID != -1);

    // Insert into front of `s_PhraseMemory`
    rng::shift_right(s_PhraseMemory, 1);
    s_PhraseMemory[0] = {m_SoundID, m_BankID};

    return m_SoundID - ctx->FirstSoundID;
}

// 0x4E5F10
int16 CAEPedSpeechAudioEntity::CanWePlayGlobalSpeechContext(eGlobalSpeechContext gCtx) {
    // exe 0x4E5F10: a conversation slot only counts when it is not -1 - otherwise the next check (player conversation, then a free slot) runs; the free slot is taken
    // (s_NextSpeechSlot advanced) BEFORE the next-play-time test
    const auto IsConversationContext = [&] { return !IsGlobalContextImportantForInterupting(gCtx) && !IsGlobalContextPain(gCtx); };
    const auto CheckSlot = [](tPedSpeechSlotID slot) -> int16 {
        return s_PedSpeechSlots[slot].Status == CAEPedSpeechSlot::eStatus::RESERVED
            ? slot
            : -1;
    };
    if (s_bPedConversationHappening && IsConversationContext()) {
        tPedSpeechSlotID slot = -1;
        if (this == &s_pConversationPed1->GetSpeechAE()) {
            slot = s_pConversationPedSlot1;
        } else if (this == &s_pConversationPed2->GetSpeechAE()) {
            slot = s_pConversationPedSlot2;
        }
        if (slot != -1) {
            return CheckSlot(slot);
        }
    }
    if (s_bPlayerConversationHappening && IsConversationContext()) {
        if (this == &s_pPlayerConversationPed->GetSpeechAE() && s_pConversationPedSlot1 != -1) {
            return CheckSlot(s_pConversationPedSlot1);
        }
    }
    const auto slot = GetFreeSpeechSlot(); // s_NextSpeechSlot is set in `GetFreeSpeechSlot`
    if (slot == -1) {
        return -1;
    }
    if ((int16)gCtx >= 0 && (int16)gCtx < 0x168 && CTimer::GetTimeInMS() < GetNextPlayTime(gCtx)) {
        return -1;
    }
    return slot;
}

// 0x4E6550
int16 CAEPedSpeechAudioEntity::AddSayEvent(eAudioEvents audioEvent, eGlobalSpeechContext gCtx, uint32 startTimeDelayMs, float probability, bool overrideSilence, bool isForceAudible, bool isFrontEnd) {
    return I_AddSayEvent<false>(m_Entity, audioEvent, gCtx, startTimeDelayMs, probability, overrideSilence, isForceAudible, isFrontEnd);
}

// 0x4E68D0
void CAEPedSpeechAudioEntity::Initialise(CEntity* ped) {
    // Field by field like the exe: `m_IsInitialized`, `m_VoiceID`, `m_SoundID`, `m_BankID`, `m_PedSpeechSlotID` and the audio type
    // keep their previous values unless set below (the old whole-object assignment reset them all)
    m_Entity = ped;
    m_Sound = nullptr;
    m_EventVolume = -100.f;
    m_LastGCtx = CTX_GLOBAL_UNK;
    m_IsFemale = 0;
    m_NextTimeCanSayPain.fill(0);
    m_IsPlayingSpeech = m_IsSpeechDisabled = m_IsSpeechForScriptsDisabled = m_IsFrontend = m_IsForcedAudible = false;

    const auto modelId = (int16)ped->m_nModelIndex;
    if (modelId < 0) { // 0x4E6934
        return;
    }
    auto* const mi = CModelInfo::GetModelInfo(modelId)->AsPedModelInfoPtr();
    m_PedAudioType = mi->m_nPedAudioType; // 0x4E6948: stored before the range check
    if ((int16)m_PedAudioType < 0 || (int16)m_PedAudioType >= PED_TYPE_NUM) {
        return;
    }
    if (m_PedAudioType == PED_TYPE_SPC) { // 0x4E695B
        if (!GetVoiceAndTypeForSpecialPed(mi->m_nKey)) {
            return;
        }
        // 0x4E697B: the model's own type is checked again, then the speech is initialised without touching `m_IsFemale` (set from the LUT)
        m_IsInitialized = true;
        return;
    }
    m_VoiceID = mi->m_nVoiceId; // 0x4E69B1: stored before the sign check
    if ((int16)m_VoiceID < 0) {
        return;
    }
    mi->IncrementVoice();
    VERIFY(GetSexFromModel((eModelID)modelId)); // 0x4E6991
    m_IsInitialized = true;
}

// 0x4E69E0
bool CAEPedSpeechAudioEntity::CanPedHoldConversation() const {
    return CanPedSayGlobalContext(CTX_GLOBAL_PCONV_QUESTION);
}

// 0x4E3F70
void CAEPedSpeechAudioEntity::EnablePedSpeech() {
    if (m_IsInitialized) {
        m_IsSpeechDisabled = false;
    }
}

// 0x4E3F90
void CAEPedSpeechAudioEntity::EnablePedSpeechForScriptSpeech() {
    if (m_IsInitialized) {
        m_IsSpeechForScriptsDisabled = false;
    }
}

// 0x4E3FB0
void CAEPedSpeechAudioEntity::StopCurrentSpeech() {
    if (!m_IsInitialized || !m_IsPlayingSpeech) {
        return;
    }

    if (auto* const tFacial = notsa::cast<CTaskComplexFacial>(m_Entity->AsPed()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_FACIAL_COMPLEX))) {
        tFacial->StopAll();
    }

    auto* const speech = GetCurrentSpeech();

    if (speech->Status == CAEPedSpeechSlot::eStatus::PLAYING) {
        if (auto* const s = std::exchange(m_Sound, nullptr)) {
            s->StopSoundAndForget();
        }
    }

    *speech = CAEPedSpeechSlot{
        .Status = speech->IsReservedForPedConversation || speech->IsReservedForPlayerConversation
            ? CAEPedSpeechSlot::eStatus::RESERVED
            : CAEPedSpeechSlot::eStatus::FREE,
        .IsReservedForPedConversation    = speech->IsReservedForPedConversation,
        .IsReservedForPlayerConversation = speech->IsReservedForPlayerConversation,
    };

    m_IsPlayingSpeech = false;
    if (m_PedAudioType == PED_TYPE_PLAYER) {
        s_bAPlayerSpeaking = false;
    }
    m_PedSpeechSlotID = -1;
}

// 0x4E4400
bool CAEPedSpeechAudioEntity::GetSoundAndBankIDsForScriptedSpeech(eAudioEvents scriptID) {
    int32         soundID = -1;
    eSoundBankS32 bankID  = SND_BANK_UNK;
    m_BankID  = SND_BANK_UNK; // 0x4E440A: reset before the lookup
    m_SoundID = -1;
    CAEAudioUtility::GetBankAndSoundFromScriptSlotAudioEvent(scriptID, bankID, soundID, -1);
    if (soundID < 0 || bankID < 0) {
        m_BankID  = SND_BANK_UNK;
        m_SoundID = -1;
        return false;
    } else {
        m_BankID  = bankID;
        m_SoundID = soundID;
        return true;
    }
}

// 0x4E4200
bool CAEPedSpeechAudioEntity::GetSexFromModel(eModelID model) {
    const auto* const mi = CModelInfo::GetModelInfo(model)->AsPedModelInfoPtr();
    if ((int16)mi->m_nPedAudioType < 0 || (int16)mi->m_nPedAudioType >= PED_TYPE_NUM) { // 0x4E420F
        return false;
    }
    if (mi->m_nPedAudioType == PED_TYPE_SPC) {
        return true;
    }
    m_IsFemale = notsa::contains({PED_TYPE_CIVFEMALE, PED_TYPE_PROSTITUTE}, mi->GetPedType());
    return true;
}

// 0x4E3F50
bool CAEPedSpeechAudioEntity::GetPedTalking() const {
    return m_IsInitialized && m_IsPlayingSpeech;
}

// 0x4E4170
bool CAEPedSpeechAudioEntity::GetVoiceAndTypeForSpecialPed(uint32 modelNameHash) {
    const auto idx = notsa::indexof(gSpecialPedVoiceNameLookup, modelNameHash);
    if (idx == -1) {
        return false;
    }
    const auto [voice, type, isFemale] = gSpecialPedVoiceLookup[idx];
    if (voice == VOICE_UNK || type == PED_TYPE_UNK) { // 0x4E41B6 / 0x4E41C9: entries without a voice or type are rejected
        return false;
    }
    m_VoiceID      = voice;
    m_PedAudioType = type;
    m_IsFemale     = isFemale; // 0x4E41D6: zero-extended byte
    return true;
}

// notsa
ePainSpeechVoices CAEPedSpeechAudioEntity::GetPainVoice() const {
    if (m_PedAudioType == PED_TYPE_PLAYER) {
        return VOICE_PAIN_CARL;
    }
    return m_IsFemale
        ? VOICE_PAIN_FEMALE
        : VOICE_PAIN_MALE;
}

// 0x4E4F70
void CAEPedSpeechAudioEntity::AddScriptSayEvent(eAudioEvents audioEvent, eAudioEvents scriptID, bool overrideSilence, bool isForceAudible, bool isFrontEnd) {
    if (audioEvent != AE_SCRIPT_SPEECH_PED) {
        return;
    }
    if (!m_IsInitialized || s_bAllSpeechDisabled) {
        return;
    }
    if (TheCamera.m_bWideScreenOn) {
        return;
    }
    if (m_IsSpeechDisabled && !overrideSilence) {
        return;
    }
    if (m_IsPlayingSpeech) {
        return;
    }
    if (CStreaming::IsVeryBusy()) {
        return;
    }
    const auto speechSlotID = m_PedAudioType == PED_TYPE_PLAYER
        ? !CAEPedSpeechAudioEntity::s_bAPlayerSpeaking || m_IsPlayingSpeech
            ? PLAYER_SPEECH_SLOT
            : -1
        : CanWePlayScriptedSpeech();
    if (speechSlotID < 0 || (m_PedAudioType != PED_TYPE_PLAYER && speechSlotID >= PLAYER_SPEECH_SLOT)) { // 0x4E500C: a non-player slot must be in [0, 5)
        return;
    }
    if (!GetSoundAndBankIDsForScriptedSpeech(scriptID)) {
        return;
    }

    m_LastGCtx        = CTX_GLOBAL_UNK;
    m_PedSpeechSlotID = speechSlotID;
    m_IsFrontend      = isFrontEnd;

    m_IsForcedAudible = isForceAudible;
    if (const auto veh = m_Entity->AsPed()->GetVehicleIfInOne()) { // 0x4E505F: sets the front-end flag (+0x9b), not the forced-audible one (+0x9c)
        if (veh == CAEAudioUtility::FindVehicleOfPlayer()) {
            m_IsFrontend = true;
        }
    }

    m_EventVolume = GetDefaultVolume(AE_SCRIPT_SPEECH_PED); // 0x4E5030: event 0x35, not AE_SPEECH_PED (0x34)
    if (m_PedAudioType == PED_TYPE_PLAYER) {
        m_EventVolume += 3.f;
    }

    m_IsPlayingSpeech = true;
    if (m_PedAudioType == PED_TYPE_PLAYER) {
        s_bAPlayerSpeaking = true;
    }

    LoadAndPlaySpeech();
}

// 0x4E5670
void CAEPedSpeechAudioEntity::Terminate() {
    StopCurrentSpeech();
    // exe 0x4E5680: only these fields are reset (`m_IsFemale`, `m_IsFrontend`, `m_IsForcedAudible`, `m_LastGCtx` and the pain timers are kept)
    m_Entity                     = nullptr;
    m_IsInitialized              = false;
    m_PedAudioType               = PED_TYPE_UNK;
    m_VoiceID                    = VOICE_UNK;
    m_IsPlayingSpeech            = false;
    m_IsSpeechDisabled           = false;
    m_IsSpeechForScriptsDisabled = false;
    m_Sound                      = nullptr;
    m_SoundID                    = -1;
    m_BankID                     = SND_BANK_UNK;
    m_PedSpeechSlotID            = -1;
    m_EventVolume                = -100.f;
}

// 0x4E5CD0
void CAEPedSpeechAudioEntity::PlayLoadedSound() {
    return I_PlayLoadedSound<false>(m_Entity);
}

// 0x4E4120
int16 CAEPedSpeechAudioEntity::GetAllocatedVoice() {
    return m_VoiceID;
}

// 0x4E5800
bool CAEPedSpeechAudioEntity::WillPedChatAboutTopic(int16 topic) {
    switch (topic) {
    case 0:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_CAR);
    case 1:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_CLOTHES);
    case 2:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_HAIR);
    case 3:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_PHYS);
    case 4:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_SHOES);
    case 5:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_SMELL);
    case 6:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_TATTOO);
    case 7:  return CanPedSayGlobalContext(CTX_GLOBAL_CONV_DISL_WEATHER);
    case 8:
    case 9:  return true;
    default: return false;
    }
}

// 0x4E4130
eAudioPedType CAEPedSpeechAudioEntity::GetPedType() {
    return m_IsInitialized 
        ? m_PedAudioType
        : PED_TYPE_UNK;
}

// 0x4E4150
bool CAEPedSpeechAudioEntity::IsPedFemaleForAudio() {
    return m_IsInitialized
        ? m_IsFemale
        : false;
}

// notsa
tPedSpeechSlotID CAEPedSpeechAudioEntity::GetFreeSpeechSlot() {
    const auto size = s_PedSpeechSlots.size() - 1; // Last one always reserved for the player
    for (size_t n = 0; n < size; n++) {
        const auto i = (s_NextSpeechSlot + n) % size;
        if (s_PedSpeechSlots[i].Status == CAEPedSpeechSlot::eStatus::FREE) {
            s_NextSpeechSlot = (tPedSpeechSlotID)((i + 1u) % size);
            return (tPedSpeechSlotID)i;
        }
    }
    return -1;
}

// 0x4E4470
eSpecificSpeechContext CAEPedSpeechAudioEntity::GetSpecificSpeechContext(eGlobalSpeechContext gCtx, eAudioPedType pedAudioType) {
    // Omitted useless `if`s
    // exe 0x4E4470: ped types 0..5 are accepted (anything else is -1). `PED_TYPE_SPC` has no table: the exe reads the 6th int16 of the 16-byte entry,
    //       which is the entry's `RepeatTime` (callers then reject type SPC themselves, e.g. CanPedSayGlobalContext compares the type to 4)
    if (pedAudioType < 0 || pedAudioType > PED_TYPE_SPC) {
        return -1;
    }
    // exe 0x4E4475: `test dx, dx; jl` / `cmp dx, 0x168; jge` - the global context must be 0..0x167 (the exe table also has a -1 terminator and a 0x168 entry)
    if (gCtx < 0 || gCtx >= CTX_GLOBAL_NUM) {
        return -1;
    }
    if (const auto* const ctxi = GetGlobalSpeechContextInfo(gCtx)) {
        if (pedAudioType == PED_TYPE_SPC) {
            return ctxi->RepeatTime;
        }
        return ctxi->SpecificSpeechContext[pedAudioType];
    }
    return -1;
}

// notsa
const tGlobalSpeechContextInfo* CAEPedSpeechAudioEntity::GetGlobalSpeechContextInfo(eGlobalSpeechContext gCtx) {
    assert(gCtx > 0 && gCtx < CTX_GLOBAL_NUM);

    // Must use a loop because there are a few skipped values (TODO: Though I guess we could fix this?)
    for (const auto& e : gSpeechContextLookup) {
        if (e.GCtx == gCtx) {
            return &e;
        }
    }
    return nullptr;
}

// notsa
const tSpecificSpeechContextInfo* CAEPedSpeechAudioEntity::GetSpecificSpeechContextInfo(eSpecificSpeechContext sCtx, eGlobalSpeechContext gCtx, eAudioPedType pt, ePedSpeechVoiceS16 voice) {
    if (IsGlobalContextPain(gCtx)) {
        return &gPainSpeechLookup[sCtx][voice];
    }
    switch (pt) {
    case PED_TYPE_GEN:    return &gGenSpeechLookup[sCtx][voice];
    case PED_TYPE_EMG:    return &gEmgSpeechLookup[sCtx][voice];
    case PED_TYPE_PLAYER: return &gPlySpeechLookup[sCtx][voice];
    case PED_TYPE_GANG:   return &gGngSpeechLookup[sCtx][voice];
    case PED_TYPE_GFD:    return &gGfdSpeechLookup[sCtx][voice];
    default:              return nullptr;
    }
}

eSoundBank CAEPedSpeechAudioEntity::GetVoiceSoundBank(eGlobalSpeechContext gCtx, eAudioPedType pt, ePedSpeechVoiceS16 voice) {
    if (IsGlobalContextPain(gCtx)) {
        return gPainVoiceToBankLookup[voice];
    }
    switch (pt) {
    case PED_TYPE_GEN:    return gGenSpeechVoiceToBankLookup[voice];
    case PED_TYPE_EMG:    return gEmgSpeechVoiceToBankLookup[voice];
    case PED_TYPE_PLAYER: return gPlySpeechVoiceToBankLookup[voice];
    case PED_TYPE_GANG:   return gGngSpeechVoiceToBankLookup[voice];
    case PED_TYPE_GFD:    return gGfdSpeechVoiceToBankLookup[voice];
    default:              NOTSA_UNREACHABLE_RET({});
    }
}

// The templated functions below must be here because
// due to a cyclic dependency with `CPed` they can't be defined in the header

// 0x4E6550 / 0x4E60D0
template<bool IsPedless>
int16 CAEPedSpeechAudioEntity::I_AddSayEvent(const CEntity* posEntity, eAudioEvents audioEvent, eGlobalSpeechContext gCtx, uint32 startTimeDelayMs, float probability, bool overrideSilence, bool isForceAudible, bool isFrontEnd) {
    if (!m_IsInitialized || s_bAllSpeechDisabled || m_IsSpeechForScriptsDisabled) {
        return -1;
    }

    if (TheCamera.m_bWideScreenOn && !IsGlobalContextImportantForWidescreen(gCtx)) {
        return -1;
    }

    if constexpr (!IsPedless) {
        if (gCtx == CTX_GLOBAL_CHAT) { // 0x4E65BA
            if (CTimer::GetFrameCounter() % 64 || s_bPedConversationHappening || s_bPlayerConversationHappening) {
                return -1;
            }
        } else if (gCtx == CTX_GLOBAL_PAIN_CJ_DROWNING && CWeather::UnderWaterness < 0.5f)  {
            return -1;
        }
    }

    if (CGameLogic::GameState == GAMELOGIC_STATE_BUSTED) { // 0x4E6139, 0x4E65AD: `cmp byte [0x96A8B0], 2` (was compared to eGameState::GAME_STATE_PLAYING_LOGO, which is also 2)
        return -1;
    }

    if (!CAEAudioUtility::ResolveProbability(probability) || audioEvent != AE_SPEECH_PED) {
        return -1;
    }

    if (m_IsSpeechDisabled && !overrideSilence) {
        return -1;
    }

    if constexpr (IsPedless) {
        if (m_IsPlayingSpeech) { // 0x4E616C
            return -1;
        }
        if (gCtx < CTX_GLOBAL_POLICE_BOAT || gCtx > CTX_GLOBAL_POLICE_OVERBOARD) { // 0x4E6176
            return -1;
        }
        // exe 0x4E6188: set after the context range test and before every other check
        m_PedAudioType = PED_TYPE_GFD;
        m_VoiceID      = (ePedSpeechVoiceS16)10;
    } else { // 0x4E6638
        const auto IsContextImportant = [](eGlobalSpeechContext c) {
            return IsGlobalContextPain(c) || IsGlobalContextImportantForInterupting(c);
            };
        if (IsContextImportant(gCtx)) {
            if (const auto* const speech = GetCurrentSpeech()) {
                if (speech->GCtx != CTX_GLOBAL_PAIN_CJ_HIGH_FALL && IsContextImportant(speech->GCtx)) {
                    return -1;
                }
            }
            StopCurrentSpeech();
        } else if (m_IsPlayingSpeech) { // 0x4E664D
            return -1;
        }

        if (!m_Entity->AsPed()->IsAlive() && !IsGlobalContextPain(gCtx)) { // 0x4E66A7
            return -1;
        }
    }

    if (IsGlobalContextImportantForStreaming(gCtx) || isForceAudible || isFrontEnd) {
        if (CStreaming::ms_numModelsRequested > 15 && CAEVehicleAudioEntity::s_pPlayerAttachedForRadio) { // 0x4E61D5, 0x4E66F0: the exe tests the driver at 0xB6B98C
            return -1;
        }
    } else if (CStreaming::IsVeryBusy()) {
        return -1;
    }

    if constexpr (!IsPedless) {
        if (m_PedAudioType == PED_TYPE_PLAYER) {
            if (FindPlayerPed() != m_Entity || s_bAPlayerSpeaking && !m_IsPlayingSpeech) {
                return -1;
            }
        }
    }

    // 0x4E6750: the exe reads the position of the entity here only (after every early out); the port read it on entry (a ped without a matrix faulted)
    // exe 0x4E6786 / 0x4E6225: fcomp 1600.0 / 3600.0 + je => rejected only when the squared distance is strictly greater
    if (CVector::DistSqr(TheCamera.GetPosition(), posEntity->GetPosition()) > sq(IsPedless ? 60.f : 40.f)) {
        return -1;
    }

    // exe 0x4E67AB..0x4E67EA: the speech slot is chosen BEFORE the sound is picked (GetSoundAndBankIDs consumes rand and shifts the phrase memory); non-player slots must be in [0, 5)
    const bool isPlayer = !IsPedless && m_PedAudioType == PED_TYPE_PLAYER;
    tPedSpeechSlotID speechSlotID;
    if (isPlayer) {
        if (CTimer::GetTimeInMS() < GetNextPlayTime(gCtx)) {
            return -1;
        }
        speechSlotID = PLAYER_SPEECH_SLOT;
    } else {
        speechSlotID = CanWePlayGlobalSpeechContext(gCtx);
        if (speechSlotID < 0 || speechSlotID >= PLAYER_SPEECH_SLOT) {
            return -1;
        }
    }

    eSpecificSpeechContext sCtx;
    const auto soundID = CAEPedSpeechAudioEntity::GetSoundAndBankIDs(gCtx, sCtx);
    if (soundID < 0) {
        return -1;
    }

    if constexpr (IsPedless) {
        // exe 0x4E6263: the attached entity is registered (CleanUpOldReference + RegisterReference) only once everything else passed
        static_cast<CAEPedlessSpeechAudioEntity*>(this)->m_AttachedTo = const_cast<CEntity*>(posEntity);
    }

    SetNextPlayTime(gCtx);
    m_IsForcedAudible = isForceAudible;

    m_IsFrontend = isFrontEnd;
    if constexpr (!IsPedless) {
        if (const auto veh = m_Entity->AsPed()->GetVehicleIfInOne()) {
            if (veh == CAEAudioUtility::FindVehicleOfPlayer() && gCtx != CTX_GLOBAL_STOMACH_RUMBLE) { // exe 0x4E683B: CAEAudioUtility::FindVehicleOfPlayer (0x4D9E10)
                m_IsFrontend = true;
            }
        }
    }

    m_EventVolume = GetDefaultVolume(AE_SPEECH_PED);
    if constexpr (!IsPedless) {
        if (m_PedAudioType == PED_TYPE_PLAYER) {
            m_EventVolume += 3.f;
        }
        m_EventVolume += GetSpeechContextVolumeOffset(gCtx);
    }

    m_IsPlayingSpeech = true;
    if constexpr (!IsPedless) {
        if (m_PedAudioType == PED_TYPE_PLAYER) {
            s_bAPlayerSpeaking = true;
        }
    }

    m_LastGCtx        = gCtx;
    m_PedSpeechSlotID = speechSlotID;
    LoadAndPlaySpeech(startTimeDelayMs);

    return soundID;
}

template
int16 CAEPedSpeechAudioEntity::I_AddSayEvent<true>(const CEntity* posEntity, eAudioEvents audioEvent, eGlobalSpeechContext gCtx, uint32 startTimeDelayMs, float probability, bool overrideSilence, bool isForceAudible, bool isFrontEnd);

// 0x4E3520 / 0x4E4D10
template<bool IsPedless>
void CAEPedSpeechAudioEntity::I_UpdateParameters(CAESound* sound, int16 playTime) {
    auto* const ped = IsPedless ? nullptr : m_Entity->AsPed();
    auto* const speech = GetCurrentSpeech();
    if (playTime == -1) { // Sound has finished?
        *speech = {
            .Status = CAEPedSpeechSlot::eStatus::FREE
        };

        m_Sound           = nullptr;
        m_SoundID         = -1;
        m_BankID          = SND_BANK_UNK;
        m_IsForcedAudible = false;
        m_IsPlayingSpeech = false;
        m_PedSpeechSlotID = -1;

        if constexpr (!IsPedless) {
            if (m_PedAudioType == PED_TYPE_PLAYER) {
                s_bAPlayerSpeaking = false;
            }

            if (s_bPedConversationHappening) {
                if (   this == &s_pConversationPed1->GetSpeechAE() && m_PedSpeechSlotID == s_pConversationPedSlot1
                    || this == &s_pConversationPed2->GetSpeechAE() && m_PedSpeechSlotID == s_pConversationPedSlot2
                ) {
                    speech->Status = CAEPedSpeechSlot::eStatus::RESERVED;
                }
            } else if (s_bPlayerConversationHappening) {
                if (this == &s_pPlayerConversationPed->GetSpeechAE() && m_PedSpeechSlotID == s_pConversationPedSlot1) {
                    speech->Status = CAEPedSpeechSlot::eStatus::RESERVED;
                }
            }

            if (auto* const tFacial = notsa::cast<CTaskComplexFacial>(m_Entity->AsPed()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_FACIAL_COMPLEX))) {
                tFacial->StopAll();
            }
        }
    } else { // Update sound
        speech->Status = CAEPedSpeechSlot::eStatus::PLAYING;
        if constexpr (IsPedless) {
            if (!m_IsFrontend || sound->m_Event == AE_SPEECH_PED) {
                sound->SetPosition(m_Entity->GetPosition());
            }
        } else {
            if (ped->bIsTalking && ped->m_nBodypartToRemove == PED_NODE_HEAD) {
                sound->StopSound();
            } else if (!m_IsFrontend) {
                sound->SetPosition(m_Entity->GetPosition());
            }
        }
    }
}

template
void CAEPedSpeechAudioEntity::I_UpdateParameters<true>(CAESound* sound, int16 playTime);

// 0x4E5CD0 / 0x4E6380
template<bool IsPedless>
void CAEPedSpeechAudioEntity::I_PlayLoadedSound(CEntity* attachTo) {
    if (!attachTo) {
        // Done by the caller (No other solution)
        // if constexpr (IsPedless) {
        //     StopCurrentSpeech()
        // }
        return;
    }

    const auto DoPlaySound = [&](CVector pos, uint32 flags, float volume, float maxDist) {
        m_Sound = AESoundManager.PlaySound({
            .BankSlotID         = (eSoundBankSlot)(SND_BANK_SLOT_SPEECH_FIRST + m_PedSpeechSlotID),
            .SoundID            = m_SoundID,
            .AudioEntity        = this,
            .Pos                = pos,
            .Volume             = m_IsForcedAudible ? 3.f : volume,
            .RollOffFactor      = maxDist,
            .Speed              = 1.f,
            .Doppler            = 1.f,
            .FrameDelay         = 0,
            .Flags              = flags | (IsPedless ? SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY : 0), // exe 0x4E63FA / 0x4E642B: pedless flags are 0x387 / 0x86
            .RegisterWithEntity = IsPedless ? attachTo : nullptr
        });
    };

    if (m_IsFrontend) {
        DoPlaySound(
            {0.f, 1.f, 0.f},
            SOUND_IS_COMPRESSABLE | SOUND_IS_DUCKABLE | SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE | SOUND_FRONT_END,
            3.f,
            1.f
        );
    } else {
        DoPlaySound(
            attachTo->GetPosition(),
            SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE,
            IsPedless ? 12.f : m_EventVolume,
            IsPedless ? 3.5f : CAEVehicleAudioEntity::s_pPlayerAttachedForRadio ? 3.f : 2.f // 0x4E5DD2: tests the driver at 0xB6B98C
        );
    }

    auto* const speech = GetCurrentSpeech();
    if (m_Sound) {
        speech->Sound = m_Sound;
        speech->Status = CAEPedSpeechSlot::eStatus::PLAYING;
        if constexpr (!IsPedless) {
            switch (m_LastGCtx) {
            case CTX_GLOBAL_BREATHING:
            case CTX_GLOBAL_STOMACH_RUMBLE:
                break;
            default: {
                notsa::cast<CTaskComplexFacial>(m_Entity->AsPed()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_FACIAL_COMPLEX))->SetRequest(
                    eFacialExpression::TALKING,
                    2800,
                    eFacialExpression::NONE,
                    0 // exe 0x4E5E5F pushes (7, 0xAF0, -1, 0)
                );
            }
            }
        }
    } else {
        *speech = CAEPedSpeechSlot{};
        if constexpr (!IsPedless) {
            if (m_PedAudioType == PED_TYPE_PLAYER) {
                s_bAPlayerSpeaking = false;
            }
        }
        m_IsPlayingSpeech = false;
        m_PedSpeechSlotID = -1;
    }
}

template
void CAEPedSpeechAudioEntity::I_PlayLoadedSound<true>(CEntity* attachTo);
