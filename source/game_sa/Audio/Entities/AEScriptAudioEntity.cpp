#include "StdInc.h"

#include "AEScriptAudioEntity.h"
#include "AESoundManager.h"
#include "AEAudioUtility.h"
#include "AEAudioHardware.h"
#include "AEAmbienceTrackManager.h"

// 0x5074D0
CAEScriptAudioEntity::CAEScriptAudioEntity() : CAEAudioEntity() {
    m_nLastTimeHornPlayed = 0;
    field_7E = 0;
    field_7C = 0;
    m_Volume = 0.0f;
    m_Speed = 1.0f;
    field_7D = 0;
    field_8C = 0.0f;
}

// 0x5B9B60
void CAEScriptAudioEntity::Initialise() {
    for (auto& link : wavLinks) {
        link.Init();
    }
}

// 0x4EC150
void CAEScriptAudioEntity::Reset() {
    for (auto i = 0; i < MISSION_AUDIO_COUNT; i++) {
        ClearMissionAudio(i);
    }
    field_7C = 0;
    m_Entity = nullptr;
    field_7D = 0;
    field_8C = 2.0f;
}

// 0x0
void CAEScriptAudioEntity::AddAudioEvent(int32) {
    /* Android NOP */
}

// 0x4EC100
CVector* CAEScriptAudioEntity::AttachMissionAudioToPhysical(uint8 sampleId, CPhysical* physical) {
    auto& link = wavLinks[sampleId];
    link.m_pEntity   = physical;
    link.m_vPosition = CVector{-1000.0f, -1000.0f, -1000.0f};
    return &link.m_vPosition; // NOTSA: The original returns this in EAX (even though it's a `void` function)
}

// 0x4EC040
void CAEScriptAudioEntity::ClearMissionAudio(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return;
    }
    AESoundManager.CancelSoundsInBankSlot((int16)(SND_BANK_SLOT_MISSION1 + sampleId), true);

    auto& link = wavLinks[sampleId];
    link.m_pEntity   = nullptr;
    link.m_vPosition = CVector{-1000.0f, -1000.0f, -1000.0f};
    link.m_Sound     = nullptr;
}

// 0x4EBFE0
bool CAEScriptAudioEntity::IsMissionAudioSampleFinished(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return true;
    }
    if (sampleId >= 2) {
        return AESoundManager.AreSoundsPlayingInBankSlot((int16)(SND_BANK_SLOT_MISSION1 + sampleId)) == 0;
    }
    return wavLinks[sampleId].m_Sound == nullptr;
}

// 0x4EBF60
int8 CAEScriptAudioEntity::GetMissionAudioLoadingStatus(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return 1;
    }
    const auto& link = wavLinks[sampleId];
    if (link.m_nBankId < 0) {
        return 1;
    }
    const auto slot = (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId);
    if (link.m_nBankSlotId < 0) { // NOTSA: `m_nBankSlotId` is actually the sound ID in the bank (-1 = whole bank)
        return AEAudioHardware.GetSoundBankLoadingStatus((eSoundBank)(uint16)link.m_nBankId, slot);
    }
    return AEAudioHardware.GetSoundLoadingStatus((eSoundBank)(uint16)link.m_nBankId, (eSoundID)(uint16)link.m_nBankSlotId, slot);
}

// 0x4EC020
int32 CAEScriptAudioEntity::GetMissionAudioEvent(uint8 sampleId) {
    return wavLinks[sampleId].m_nAudioEvent;
}

// 0x4EC0C0
void CAEScriptAudioEntity::SetMissionAudioPosition(uint8 sampleId, CVector& posn) {
    auto& link = wavLinks[sampleId];
    link.m_vPosition = posn;
    link.m_pEntity   = nullptr;
}

// 0x4EC4D0
CVector* CAEScriptAudioEntity::GetMissionAudioPosition(uint8 sampleId) {
    auto& link = wavLinks[sampleId];
    if (link.m_pEntity) {
        return &link.m_pEntity->GetPosition();
    }
    if (link.m_vPosition == CVector{-1000.0f, -1000.0f, -1000.0f}) {
        return nullptr;
    }
    if (link.m_vPosition == CVector{0.0f, 0.0f, 0.0f}) {
        return nullptr;
    }
    return &link.m_vPosition;
}

// 0x4EC6D0
void CAEScriptAudioEntity::PlayMissionBankSound(eAudioEvents event, CVector& posn, CPhysical* physical, int16 sfxId, uint8 linkId, uint8 dontPlayIfAlreadyPlaying, float volume, float maxDistance, float speed) {
    if (linkId < 2 || linkId >= MISSION_AUDIO_COUNT) {
        return;
    }
    if (dontPlayIfAlreadyPlaying && AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(event, this)) {
        return;
    }

    const auto slot = (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + linkId);
    if (!AEAudioHardware.IsSoundBankLoaded((eSoundBank)(uint16)wavLinks[linkId].m_nBankId, slot)) {
        return;
    }

    const auto totalVolume = GetDefaultVolume((eAudioEvents)(uint16)event) + volume;

    bool   isFrontEnd = false;
    CVector pos;
    if (physical) {
        pos = physical->GetPosition();
    } else if (posn == CVector{-1000.0f, -1000.0f, -1000.0f} || posn == CVector{0.0f, 0.0f, 0.0f}) {
        pos        = CVector{0.0f, 1.0f, 0.0f};
        isFrontEnd = true;
    } else if (posn == CVector{-1.0f, 0.0f, 0.0f} || posn == CVector{1.0f, 0.0f, 0.0f}) {
        pos        = posn;
        isFrontEnd = true;
    } else {
        pos = posn;
    }

    m_tempSound.Initialise(slot, (eSoundID)sfxId, this, pos, totalVolume, maxDistance, speed, 1.0f, 0, 0, 0.0f, 0);
    m_tempSound.m_Flags = SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES;
    m_tempSound.SetFlags(SOUND_FRONT_END, isFrontEnd);
    if (physical) {
        m_tempSound.SetFlags(SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY, true);
        m_tempSound.RegisterWithPhysicalEntity(physical);
    }
    m_tempSound.m_Event = (uint16)event;
    AESoundManager.RequestNewSound(&m_tempSound);
}

// event eAudioEvents
// 0x4EC550
void CAEScriptAudioEntity::PlayResidentSoundEvent(eSoundBankSlot slot, eSoundBank bank, eSoundID sfx, eAudioEvents event, CVector& posn, CPhysical* physical, float vol, float speed, int16 playPosn, float maxDistance) {
    if (!AEAudioHardware.IsSoundBankLoaded(bank, slot)) {
        return;
    }

    bool bFrontend = false;
    const auto volume = GetDefaultVolume(static_cast<eAudioEvents>(event)) + vol;
    CVector pos = [&] {
        if (physical) {
            return physical->GetPosition();
        } else if (posn == -1000.0f || posn.IsZero()) {
            bFrontend = true;
            return CVector{0.0f, 1.0f, 0.0f};
        } else {
            return posn;
        }
    }();

    AESoundManager.PlaySound({
        .BankSlotID         = slot,
        .SoundID            = sfx,
        .AudioEntity        = this,
        .Pos                = pos,
        .Volume             = volume,
        .RollOffFactor      = maxDistance,
        .Speed              = speed,
        .Doppler            = 1.0f,
        .FrameDelay         = 0,
        .Flags              = SOUND_START_PERCENTAGE | SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE | (bFrontend ? SOUND_FRONT_END : 0u),
        .FrequencyVariance  = 0.0f,
        .PlayTime           = playPosn,
        .RegisterWithEntity = physical,
        .EventID            = (eAudioEvents)(event),
    });
}

// 0x4EC270
void CAEScriptAudioEntity::PlayLoadedMissionAudio(uint8 sampleId) {
    if (sampleId >= MISSION_AUDIO_COUNT) {
        return;
    }
    auto& link = wavLinks[sampleId];
    if (link.m_nBankId < 0 || link.m_nBankSlotId < 0) {
        return;
    }
    if (GetMissionAudioLoadingStatus(sampleId) != 1) {
        return;
    }

    bool isFrontEnd  = false; // 0x12
    bool isChannel01 = false; // 0x13

    float volume;
    if (link.m_nAudioEvent == 0xFFFF) {
        volume = -100.0f;
    } else {
        volume = GetDefaultVolume((eAudioEvents)link.m_nAudioEvent);
        if (sampleId < 2) {
            isChannel01 = true;
            if (volume == -128.0f) {
                volume = 6.0f;
            }
        }
    }

    CVector pos;
    if (link.m_pEntity) {
        pos = link.m_pEntity->GetPosition();
    } else if (link.m_vPosition == CVector{-1000.0f, -1000.0f, -1000.0f} || link.m_vPosition == CVector{0.0f, 0.0f, 0.0f}) {
        pos        = CVector{0.0f, 1.0f, 0.0f};
        isFrontEnd = true;
    } else {
        pos = link.m_vPosition;
    }

    CAESound sound;
    sound.Initialise((eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + sampleId), (eSoundID)(uint16)link.m_nBankSlotId, this, pos, volume, 2.0f, 1.0f, 1.0f, 0, 0, 0.0f, 0);
    sound.m_Flags = SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_PLAY_PHYSICALLY;
    sound.SetFlags(SOUND_FRONT_END, isFrontEnd);
    sound.SetFlags(SOUND_IS_DUCKABLE, isChannel01);
    sound.SetFlags(SOUND_IS_COMPRESSABLE, isChannel01);
    sound.SetFlags(SOUND_SMOOTH_DUCKING, isChannel01);
    link.m_Sound = AESoundManager.RequestNewSound(&sound);
}

// 0x4EC190
void CAEScriptAudioEntity::PreloadMissionAudio(uint8 slotId, int32 sampleId) {
    if (slotId >= MISSION_AUDIO_COUNT) {
        return;
    }
    if (!IsMissionAudioSampleFinished(slotId)) {
        return;
    }

    auto&         link  = wavLinks[slotId];
    auto          event = (eAudioEvents)sampleId;
    eSoundBankS32 bankId{};
    int32         soundId{};
    if (!CAEAudioUtility::GetBankAndSoundFromScriptSlotAudioEvent(event, bankId, soundId, slotId)) {
        return;
    }
    link.m_nBankId     = bankId;
    link.m_nBankSlotId = soundId;

    const auto slot = (eSoundBankSlot)(SND_BANK_SLOT_MISSION1 + slotId);
    if (link.m_nBankSlotId < 0) {
        AEAudioHardware.LoadSoundBank((eSoundBank)(uint16)link.m_nBankId, slot);
    } else {
        AEAudioHardware.LoadSound((eSoundBank)(uint16)link.m_nBankId, (eSoundID)(uint16)link.m_nBankSlotId, slot);
    }

    link.m_nAudioEvent = sampleId;
    link.m_pEntity     = nullptr;
    link.m_vPosition   = CVector{-1000.0f, -1000.0f, -1000.0f};
}

// 0x4ECCF0
void CAEScriptAudioEntity::ProcessMissionAudioEvent(eAudioEvents eventId, CVector& posn, CPhysical* physical, float volume, float speed) {
    // Plays a sound of the mission bank (link) for this event
    const auto PlayBank = [&](int16 sfxId, uint8 linkId, bool dontPlayIfAlreadyPlaying = false, float vol = 0.0f, float maxDistance = 2.0f, float spd = 1.0f) {
        PlayMissionBankSound(eventId, posn, physical, sfxId, linkId, dontPlayIfAlreadyPlaying, vol, maxDistance, spd);
    };

    // Plays a sound of a (resident) sound bank for this event
    const auto PlayResident = [&](eSoundBankSlot slot, eSoundBank bank, int16 sfxId, float vol = 0.0f, float spd = 1.0f, int16 playPosn = 0, float maxDistance = 1.0f) {
        PlayResidentSoundEvent(slot, bank, (eSoundID)sfxId, eventId, posn, physical, vol, spd, playPosn, maxDistance);
    };

    // Cancels all sounds of the (other) event played by this entity
    const auto CancelOwnEvent = [&](eAudioEvents event) {
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity((int16)event, this);
    };

    // Same as above, but only for the sounds attached to `physical` (if there is one)
    const auto CancelOwnEventForPhysical = [&](eAudioEvents event) {
        if (physical) {
            AESoundManager.CancelSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)event, this, physical);
        } else {
            CancelOwnEvent(event);
        }
    };

    switch ((uint16)eventId) {
        case AE_CRANE_WINCH_MOVE: // 0x68
            if (!field_7D) {
                break;
            }
            if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)eventId, this, physical)) {
                PlayResident(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 1, volume, speed, 0, 2.5f);
            }
            m_Speed  = speed;
            m_Volume = GetDefaultVolume(AE_CRANE_WINCH_MOVE) + volume;
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
            break;
        case AE_SCRIPT_DISABLE_HELI_AUDIO: // 0x3E8
            CAEVehicleAudioEntity::DisableHelicoptors(); // 0x4F4EE0
            break;
        case AE_SCRIPT_ENABLE_HELI_AUDIO: // 0x3E9
            CAEVehicleAudioEntity::EnableHelicoptors(); // 0x4F4EF0
            break;
        case AE_SCRIPT_CEILING_VENT_LAND: // 0x3EA
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x40, 0.0f, 0.79f, 0x23, 1.0f);
            break;
        case AE_SCRIPT_CLAXON_START: // 0x3ED
            PlayBank(1, 2);
            break;
        case AE_SCRIPT_CLAXON_STOP: // 0x3EE
            CancelOwnEvent(AE_SCRIPT_CLAXON_START);
            break;
        case AE_SCRIPT_BLAST_DOOR_SLIDE_START: // 0x3EF
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_BLAST_DOOR_SLIDE_STOP: // 0x3F0
            CancelOwnEvent(AE_SCRIPT_BLAST_DOOR_SLIDE_START);
            break;
        case AE_SCRIPT_BONNET_DENT: // 0x3F1
        case AE_SCRIPT_CAR_SMASH_CAR: // 0x474
        case AE_SCRIPT_MAGNET_VEHICLE_COLLISION: // 0x47C
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28), 0.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_BASKETBALL_BOUNCE: // 0x3F2
            PlayBank((int16)CAEAudioUtility::GetRandomNumberInRange(0, 2), 3);
            break;
        case AE_SCRIPT_BASKETBALL_HIT_HOOP: // 0x3F3
        case AE_SCRIPT_GOGO_EXPLOSION: // 0x425
        case AE_SCRIPT_BANDIT_INSERT_COIN: // 0x43F
        case AE_SCRIPT_OTB_NO_CASH: // 0x486
            PlayBank(3, 3);
            break;
        case AE_SCRIPT_BASKETBALL_SCORE: // 0x3F4
            if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity((int16)eventId, this)) {
                break;
            }
            PlayBank(4, 3);
            break;
        case AE_SCRIPT_POOL_BREAK: // 0x3F5
        case AE_SCRIPT_POOL_BALL_HIT_BALL: // 0x3F7
        case AE_SCRIPT_CRANE_SMASH_PORTACABIN: // 0x3FF
        case AE_SCRIPT_CONTAINER_COLLISION: // 0x400
        case AE_SCRIPT_FREEFALL_START: // 0x40D
        case AE_SCRIPT_FREEFALL_STOP: // 0x40E
        case AE_SCRIPT_PARACHUTE_COLLAPSE: // 0x410
        case AE_SCRIPT_OFFICE_FIRE_ALARM_START: // 0x443
        case AE_SCRIPT_OFFICE_FIRE_ALARM_STOP: // 0x444
        case AE_SCRIPT_OFFICE_FIRE_COUGHING_START: // 0x445
        case AE_SCRIPT_OFFICE_FIRE_COUGHING_STOP: // 0x446
        case AE_SCRIPT_BIKE_GANG_WHEEL_SPIN: // 0x448
        case AE_SCRIPT_HEAVY_DOOR_STOP: // 0x452
        case AE_SCRIPT_DA_NANG_MUFFLED_REFUGEES: // 0x458
        case AE_SCRIPT_PICKUP_CRATE: // 0x47A
        case AE_SCRIPT_ROULETTE_BALL_BOUNCING: // 0x488
            break;
        case AE_SCRIPT_POOL_HIT_WHITE: // 0x3F6
            PlayBank(10, 3);
            break;
        case AE_SCRIPT_POOL_HIT_CUSHION: // 0x3F8
        case AE_SCRIPT_DUAL_SHOOT: // 0x411
            PlayBank(8, 3);
            break;
        case AE_SCRIPT_POOL_BALL_POT: // 0x3F9
            PlayBank((int16)CAEAudioUtility::GetRandomNumberInRange(3, 5), 3);
            break;
        case AE_SCRIPT_POOL_CHALK_CUE: // 0x3FA
        case AE_SCRIPT_TEMPEST_SELECT: // 0x49C
            PlayBank(7, 3);
            break;
        case AE_SCRIPT_CRANE_ENTER: // 0x3FB
            if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_CRANE_P, SND_BANK_SLOT_PLAYER_ENGINE_P)) {
                if (AESoundManager.AreSoundsPlayingInBankSlot(SND_BANK_SLOT_PLAYER_ENGINE_P)) {
                    AESoundManager.CancelSoundsInBankSlot(SND_BANK_SLOT_PLAYER_ENGINE_P, false);
                }
                AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_CRANE_P, SND_BANK_SLOT_PLAYER_ENGINE_P);
            }
            m_Physical = physical;
            field_7D   = 1;
            break;
        case AE_SCRIPT_CRANE_MOVE_START: // 0x3FC
            if (!field_7D) {
                break;
            }
            if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical((int16)eventId, this, physical)) {
                PlayResident(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 1, 0.0f, 1.0f, 0, 2.5f);
            }
            break;
        case AE_SCRIPT_CRANE_MOVE_STOP: // 0x3FD
            if (!field_7D) {
                break;
            }
            CancelOwnEventForPhysical(AE_SCRIPT_CRANE_MOVE_START);
            PlayResident(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 2, 0.0f, 1.0f, 0, 2.5f);
            break;
        case AE_SCRIPT_CRANE_EXIT: // 0x3FE
            if (!field_7D) {
                break;
            }
            AESoundManager.CancelSoundsInBankSlot(SND_BANK_SLOT_PLAYER_ENGINE_P, true);
            PlayResident(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 3, 0.0f, 1.0f, 0, 2.5f);
            m_Physical = nullptr;
            field_7D   = 0;
            break;
        case AE_SCRIPT_VIDEO_POKER_PAYOUT: // 0x401
            PlayBank(1, 3);
            break;
        case AE_SCRIPT_VIDEO_POKER_BUTTON: // 0x402
            PlayBank(0, 3);
            break;
        case AE_SCRIPT_WHEEL_OF_FORTUNE_CLACKER: // 0x403
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x13, 0.0f, 1.0f, 0x46, 1.0f);
            break;
        case AE_SCRIPT_KEYPAD_BEEP: // 0x404
            if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_KEYPAD, SND_BANK_SLOT_MISSION3)) {
                PlayBank(0, 2);
            } else if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_UNCLE_SAM, SND_BANK_SLOT_MISSION3)) {
                PlayBank(3, 2);
            }
            break;
        case AE_SCRIPT_KEYPAD_PASS: // 0x405
            PlayBank(2, 2);
            break;
        case AE_SCRIPT_KEYPAD_FAIL: // 0x406
        case AE_SCRIPT_DA_NANG_HEAVY_DOOR_OPEN: // 0x457
        case AE_SCRIPT_CAT2_WOODEN_DOOR_BREACH: // 0x484
            PlayBank(1, 2);
            break;
        case AE_SCRIPT_SHOOTING_RANGE_TARGET_SHATTER: // 0x407
            PlayBank((int16)CAEAudioUtility::GetRandomNumberInRange(2, 6), 3);
            break;
        case AE_SCRIPT_SHOOTING_RANGE_TARGET_DROP: // 0x408
            PlayBank(1, 3);
            break;
        case AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_START: // 0x409
            PlayBank(0, 3);
            break;
        case AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_STOP: // 0x40A
            CancelOwnEventForPhysical(AE_SCRIPT_SHOOTING_RANGE_TARGET_MOVE_START);
            break;
        case AE_SCRIPT_SHUTTER_DOOR_START: // 0x40B
        case AE_SCRIPT_GARAGE_DOOR_START: // 0x481
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_SHUTTER_DOOR_STOP: // 0x40C
        case AE_SCRIPT_GARAGE_DOOR_STOP: // 0x482
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_PARACHUTE_OPEN: // 0x40F
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x41, 0.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_DUAL_THRUST: // 0x412
            PlayBank(0, 3, true);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
            break;
        case AE_SCRIPT_DUAL_EXPLOSION_SHORT: // 0x413
        case AE_SCRIPT_OTB_LOSE: // 0x462
            PlayBank(2, 3, true);
            break;
        case AE_SCRIPT_DUAL_EXPLOSION_LONG: // 0x414
            PlayBank(1, 3, true);
            break;
        case AE_SCRIPT_DUAL_MENU_SELECT: // 0x415
        case AE_SCRIPT_BEE_SELECT: // 0x431
            PlayBank(5, 3);
            break;
        case AE_SCRIPT_DUAL_MENU_DESELECT: // 0x416
        case AE_SCRIPT_BEE_PICKUP: // 0x42F
        case AE_SCRIPT_TEMPEST_HIGHLIGHT: // 0x49B
            PlayBank(4, 3);
            break;
        case AE_SCRIPT_DUAL_GAME_OVER: // 0x417
            PlayBank(3, 3, true);
            break;
        case AE_SCRIPT_DUAL_PICKUP_LIGHT: // 0x418
            PlayBank(7, 3, true);
            break;
        case AE_SCRIPT_DUAL_PICKUP_DARK: // 0x419
            PlayBank(6, 3, true);
            break;
        case AE_SCRIPT_DUAL_TOUCH_DARK: // 0x41A
            PlayBank(9, 3, true);
            break;
        case AE_SCRIPT_DUAL_TOUCH_LIGHT: // 0x41B
            PlayBank(10, 3, true);
            break;
        case AE_SCRIPT_AMMUNATION_BUY_WEAPON: // 0x41C
        case AE_SCRIPT_SHOP_BUY: // 0x41E
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PURCHASE_WEAPON, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_AMMUNATION_BUY_WEAPON_DENIED: // 0x41D
        case AE_SCRIPT_SHOP_BUY_DENIED: // 0x41F
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_NO_CASH, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_RACE_321: // 0x420
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RACE_321, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_RACE_GO: // 0x421
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_RACE_GO, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_PART_MISSION_COMPLETE: // 0x422
        case AE_SCRIPT_CHECKPOINT_GREEN: // 0x472
        case AE_SCRIPT_PROPERTY_PURCHASED: // 0x47D
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_GOGO_PLAYER_FIRE: // 0x423
            PlayBank(5, 3);
            break;
        case AE_SCRIPT_GOGO_ENEMY_FIRE: // 0x424
            PlayBank(2, 3);
            break;
        case AE_SCRIPT_GOGO_TRACK_START: // 0x426
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0x99); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_GOGO_TRACK_STOP: // 0x427
        case AE_SCRIPT_DUAL_TRACK_STOP: // 0x42D
        case AE_SCRIPT_BEE_TRACK_STOP: // 0x435
        case AE_SCRIPT_AWARD_TRACK_STOP: // 0x44A
        case AE_SCRIPT_OTB_TRACK_STOP: // 0x477
        case AE_SCRIPT_TEMPEST_TRACK_STOP: // 0x49E
        case AE_SCRIPT_DRIVING_AWARD_TRACK_STOP: // 0x4A0
        case AE_SCRIPT_BIKE_AWARD_TRACK_STOP: // 0x4A2
        case AE_SCRIPT_PILOT_AWARD_TRACK_STOP: // 0x4A4
            AEAmbienceTrackManager.StopSpecialMissionAmbienceTrack();
            break;
        case AE_SCRIPT_GOGO_SELECT: // 0x428
        case AE_SCRIPT_TEMPEST_PLAYER_SHOOT: // 0x492
            PlayBank(6, 3);
            break;
        case AE_SCRIPT_GOGO_ACCEPT: // 0x429
            PlayBank(0, 3);
            break;
        case AE_SCRIPT_GOGO_DECLINE: // 0x42A
        case AE_SCRIPT_BEE_ACCEPT: // 0x432
            PlayBank(1, 3);
            break;
        case AE_SCRIPT_GOGO_GAME_OVER: // 0x42B
            PlayBank(4, 3, true);
            break;
        case AE_SCRIPT_DUAL_TRACK_START: // 0x42C
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0x96); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_BEE_ZAP: // 0x42E
            PlayBank(6, 3);
            break;
        case AE_SCRIPT_BEE_DROP: // 0x430
            PlayBank(2, 3);
            break;
        case AE_SCRIPT_BEE_DECLINE: // 0x433
            PlayBank(1, 3, false, 0.0f, 2.0f, 0.79f);
            break;
        case AE_SCRIPT_BEE_TRACK_START: // 0x434
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0x8d); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_BEE_GAME_OVER: // 0x436
        case AE_SCRIPT_TEMPEST_GAME_OVER: // 0x49A
            PlayBank(3, 3, true);
            break;
        case AE_SCRIPT_FREEZER_OPEN: // 0x437
            PlayBank(2, 2, true, 0.0f, 3.0f);
            break;
        case AE_SCRIPT_FREEZER_CLOSE: // 0x438
            PlayBank(1, 2, true, 0.0f, 3.0f);
            break;
        case AE_SCRIPT_MEAT_TRACK_START: // 0x439
            PlayBank(3, 2, true, 0.0f, 3.0f);
            PlayBank(0, 2, false, 0.0f, 3.0f);
            break;
        case AE_SCRIPT_MEAT_TRACK_STOP: // 0x43A
            CancelOwnEvent(AE_SCRIPT_MEAT_TRACK_START);
            PlayBank(4, 2, true, 0.0f, 3.0f);
            break;
        case AE_SCRIPT_ROULETTE_ADD_CASH: // 0x43B
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SELECT, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_ROULETTE_REMOVE_CASH: // 0x43C
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BACK, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_ROULETTE_NO_CASH: // 0x43D
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_ERROR, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_ROULETTE_SPIN: // 0x43E
            PlayBank(0, 3);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
            field_7C = 0;
            break;
        case AE_SCRIPT_BANDIT_WHEEL_STOP: // 0x440
            CancelOwnEventForPhysical(AE_SCRIPT_BANDIT_WHEEL_START);
            PlayBank(2, 3);
            break;
        case AE_SCRIPT_BANDIT_WHEEL_START: // 0x441
            PlayBank(0, 3);
            break;
        case AE_SCRIPT_BANDIT_PAYOUT: // 0x442
            PlayBank(1, 3);
            break;
        case AE_SCRIPT_BIKE_PACKER_CLUNK: // 0x447
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x41, 0.0f, 1.0f, 0x10, 1.0f);
            break;
        case AE_SCRIPT_AWARD_TRACK_START: // 0x449
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0x91); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_MESH_GATE_OPEN_START: // 0x44C
            if (field_7E) {
                PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x45, 0.0f, 1.0f, 0x3f, 1.0f);
            } else {
                PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x46, 0.0f, 1.0f, 0x3e, 1.0f);
            }
            field_7E = (uint8)(((int8)field_7E + 1) % 2);
            break;
        case AE_SCRIPT_MESH_GATE_OPEN_STOP: // 0x44D
            if (field_7E) {
                PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x45, 0.0f, 0.94f, 0x21, 1.0f);
            } else {
                PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x46, 0.0f, 1.0f, 0x19, 1.0f);
            }
            field_7E = (uint8)(((int8)field_7E + 1) % 2);
            break;
        case AE_SCRIPT_OGLOC_DOORBELL: // 0x44E
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_OGLOC_WINDOW_RATTLE_BANG: // 0x44F
            PlayBank(1, 2);
            break;
        case AE_SCRIPT_STINGER_RELOAD: // 0x450
        case AE_SCRIPT_MECHANIC_SLIDE_OUT: // 0x47F
            PlayBank(1, 2);
            break;
        case AE_SCRIPT_HEAVY_DOOR_START: // 0x451
            PlayBank(2, 2);
            break;
        case AE_SCRIPT_SHOOT_CONTROLS: // 0x453
            if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_BLACK_PROJECT, SND_BANK_SLOT_MISSION3)) {
                PlayBank(3, 2);
            } else if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_UNCLE_SAM, SND_BANK_SLOT_MISSION3)) {
                PlayBank(4, 2);
            }
            break;
        case AE_SCRIPT_CARGO_PLANE_DOOR_START: // 0x454
            PlayBank(1, 2);
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_CARGO_PLANE_DOOR_STOP: // 0x455
            CancelOwnEvent(AE_SCRIPT_CARGO_PLANE_DOOR_START);
            PlayBank(2, 2);
            break;
        case AE_SCRIPT_DA_NANG_CONTAINER_OPEN: // 0x456
        case AE_SCRIPT_MECHANIC_ATTACH_CAR_BOMB: // 0x480
        case AE_SCRIPT_CAT2_SECURITY_ALARM: // 0x483
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_GYM_BIKE_START: // 0x459
            PlayBank(0, 3, true, -18.0f);
            field_8C = 1.0f;
            break;
        case AE_SCRIPT_GYM_BIKE_STOP: // 0x45A
        case AE_SCRIPT_GYM_RUNNING_MACHINE_STOP: // 0x45F
            field_8C = 2.0f;
            break;
        case AE_SCRIPT_GYM_BOXING_BELL: // 0x45B
        case AE_SCRIPT_TEMPEST_EXPLOSION: // 0x494
            PlayBank(2, 3);
            break;
        case AE_SCRIPT_GYM_INCREASE_DIFFICULTY: // 0x45C
            PlayBank(3, 3);
            break;
        case AE_SCRIPT_GYM_REST_WEIGHTS: // 0x45D
            PlayBank(7, 3);
            break;
        case AE_SCRIPT_GYM_RUNNING_MACHINE_START: // 0x45E
            PlayBank(1, 3, true, -18.0f);
            field_8C = 1.0f;
            break;
        case AE_SCRIPT_OTB_BET_ZERO: // 0x460
            PlayBank(0, 3);
            break;
        case AE_SCRIPT_OTB_INCREASE_BET: // 0x461
        case AE_SCRIPT_TEMPEST_ENEMY_SHOOT: // 0x493
            PlayBank(1, 3);
            break;
        case AE_SCRIPT_OTB_PLACE_BET: // 0x463
            PlayBank(4, 3);
            break;
        case AE_SCRIPT_OTB_WIN: // 0x464
            PlayBank(5, 3, true);
            break;
        case AE_SCRIPT_STINGER_FIRE: // 0x465
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_HEAVY_GATE_START: // 0x466
            PlayBank(1, 2);
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_HEAVY_GATE_STOP: // 0x467
            CancelOwnEvent(AE_SCRIPT_HEAVY_GATE_START);
            // NOTE: The original falls through into the body of the next event (0x405) here
            PlayBank(2, 2);
            break;
        case AE_SCRIPT_VERTICAL_BIRD_LIFT_START: // 0x468
            PlayBank(2, 2);
            PlayBank(0, 2);
            break;
        case AE_SCRIPT_VERTICAL_BIRD_LIFT_STOP: // 0x469
            CancelOwnEventForPhysical(AE_SCRIPT_VERTICAL_BIRD_LIFT_START);
            PlayBank(3, 2);
            break;
        case AE_SCRIPT_PUNCH_PED: // 0x46A
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x3a, 0.0f, 1.0f, 0, 1.0f);
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x28, 0.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_AMMUNATION_GUN_COLLISION: // 0x46B
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x21, 0.0f, 1.0f, 0, 1.0f);
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x32, 0.0f, 0.79f, 0x16, 1.0f);
            break;
        case AE_SCRIPT_CAMERA_SHOT: // 0x46C
            if (physical) {
                AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_CAMERA, physical);
            } else {
                PlayResidentSoundEvent(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x2d, eventId, posn, nullptr, 0.0f, 1.0f, 0, 1.0f);
            }
            break;
        case AE_SCRIPT_BUY_CAR_MOD: // 0x46D
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BUY_CAR_MOD, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_BUY_CAR_RESPRAY: // 0x46E
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_CAR_RESPRAY, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_BASEBALL_BAT_HIT_PED: // 0x46F
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x22, 5.0f, 1.0f, 0, 1.0f);
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x28, 0.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_STAMP_PED: // 0x470
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, 0x52, -3.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_CHECKPOINT_AMBER: // 0x471
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.12f);
            break;
        case AE_SCRIPT_CHECKPOINT_RED: // 0x473
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PART_MISSION_COMPLETE, 0.0f, 1.26f);
            break;
        case AE_SCRIPT_CAR_SMASH_GATE: // 0x475
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, (int16)CAEAudioUtility::GetRandomNumberInRange(20, 28), 0.0f, 1.0f, 0, 1.0f);
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x41, 0.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_OTB_TRACK_START: // 0x476
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0xa0); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_PED_HIT_WATER_SPLASH: // 0x478
            if (physical) {
                AudioEngine.ReportWaterSplash(physical, -6.0f, false);
            } else {
                AudioEngine.ReportWaterSplash(posn, -6.0f);
            }
            break;
        case AE_SCRIPT_RESTAURANT_TRAY_COLLISION: // 0x479
            PlayResident(SND_BANK_SLOT_COLLISIONS, SND_BANK_GENRL_COLLISIONS, 0x13, 0.0f, 1.0f, 0x41, 1.0f);
            break;
        case AE_SCRIPT_SWEETS_HORN: // 0x47B
            if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity((int16)eventId, this)) {
                break;
            }
            PlayResident(SND_BANK_SLOT_HORN_AND_SIREN, SND_BANK_GENRL_HORN, 7, 0.0f, 1.0f, 0, 1.0f);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
            break;
        case AE_SCRIPT_PICKUP_STANDARD: // 0x47E
            AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_PICKUP_MONEY, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_MINITANK_FIRE: // 0x485
            if (physical) {
                AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_M4, physical);
            }
            break;
        case AE_SCRIPT_EXPLOSION: // 0x487
            m_ExplosionAudio.AddAudioEvent(AE_EXPLOSION, physical ? physical->GetPosition() : posn, 0.0f);
            break;
        case AE_SCRIPT_VERTICAL_BIRD_ALARM_START: // 0x489
            PlayBank(1, 2);
            break;
        case AE_SCRIPT_VERTICAL_BIRD_ALARM_STOP: // 0x48A
            CancelOwnEventForPhysical(AE_SCRIPT_VERTICAL_BIRD_ALARM_START);
            break;
        case AE_SCRIPT_PED_COLLAPSE: // 0x48B
            PlayResident(SND_BANK_SLOT_FOOTSTEPS_GENERIC, SND_BANK_FEET_GENERIC, 0, 0.0f, 1.0f, 0, 1.0f);
            break;
        case AE_SCRIPT_AIR_HORN: // 0x48C
            {
                // NOTSA: The original ignores `posn` and `physical` here and plays the sounds from the left and the right of the listener
                CVector left{-1.0f, 0.0f, 0.0f};
                PlayMissionBankSound(eventId, left, nullptr, 0, 3, false, 0.0f, 2.0f, 1.0f);
                CVector right{1.0f, 0.0f, 0.0f};
                PlayMissionBankSound(eventId, right, nullptr, 1, 3, false, 0.0f, 2.0f, 1.0f);
            }
            break;
        case AE_SCRIPT_SHUTTER_DOOR_SLOW_START: // 0x48D
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENING, physical ? physical->GetPosition() : posn, 0.0f, 0.79f);
            break;
        case AE_SCRIPT_SHUTTER_DOOR_SLOW_STOP: // 0x48E
            m_GarageAudio.AddAudioEvent(AE_GARAGE_DOOR_OPENED, physical ? physical->GetPosition() : posn, 0.0f, 1.0f);
            break;
        case AE_SCRIPT_BEE_BUZZ: // 0x48F
            PlayBank(0, 3, true);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
            break;
        case AE_SCRIPT_RESTAURANT_CJ_EAT: // 0x490
            PlayBank(0, 3);
            break;
        case AE_SCRIPT_RESTAURANT_CJ_PUKE: // 0x491
            PlayBank(1, 3);
            break;
        case AE_SCRIPT_TEMPEST_PICKUP1: // 0x495
            PlayBank(5, 3, false, 0.0f, 2.0f, 0.67f);
            break;
        case AE_SCRIPT_TEMPEST_PICKUP2: // 0x496
            PlayBank(5, 3, false, 0.0f, 2.0f, 0.79f);
            break;
        case AE_SCRIPT_TEMPEST_PICKUP3: // 0x497
            PlayBank(5, 3);
            break;
        case AE_SCRIPT_TEMPEST_WARP: // 0x498
            PlayBank(8, 3, true, 0.0f, 2.0f, 0.38f);
            break;
        case AE_SCRIPT_TEMPEST_SHIELD_GLOW: // 0x499
            PlayBank(0, 3, true);
            m_nLastTimeHornPlayed = CTimer::GetTimeInMS();
            break;
        case AE_SCRIPT_TEMPEST_TRACK_START: // 0x49D
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0xac); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_DRIVING_AWARD_TRACK_START: // 0x49F
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0x95); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_BIKE_AWARD_TRACK_START: // 0x4A1
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0x8e); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_PILOT_AWARD_TRACK_START: // 0x4A3
            AEAmbienceTrackManager.PlaySpecialMissionAmbienceTrack((eAudioEvents)0xa1); // NOTSA: Not an audio event, but a special mission ambience track ID
            break;
        case AE_SCRIPT_PED_DEATH_CRUNCH: // 0x4A5
            if (CLocalisation::Blood() && physical && physical->GetIsTypePed()) {
                static_cast<CPed*>(physical)->GetAE().AddAudioEvent(AE_PED_CRUNCH, 0.0f, 1.0f, physical, SURFACE_DEFAULT, 0, 0);
            }
            break;
        case AE_SCRIPT_SPANK: // 0x4A6
            PlayResident(SND_BANK_SLOT_WEAPON_GEN, SND_BANK_GENRL_WEAPONS, (int16)CAEAudioUtility::GetRandomNumberInRange(78, 80), 0.0f, 1.0f, 0, 1.0f);
            break;
        default:
            break;
    }
}

// 0x4EE960
void CAEScriptAudioEntity::ReportMissionAudioEvent(eAudioEvents eventId, CPhysical* physical, float volume, float speed) {
    CVector posn{ -1000.0f, -1000.0f, -1000.0f };
    ProcessMissionAudioEvent(eventId, posn, physical, volume, speed);
}

// 0x4EE940
void CAEScriptAudioEntity::ReportMissionAudioEvent(eAudioEvents eventId, CVector& posn) {
    ProcessMissionAudioEvent(eventId, posn, nullptr);
}

// 0x4EC970
void CAEScriptAudioEntity::UpdateParameters(CAESound* sound, int16 playTime) {
    CVector unusedPosn{-1000.0f, -1000.0f, -1000.0f};
    if (!sound) {
        return;
    }

    // Gym bike / running machine: fade volume towards the event's default volume (or out, depending on `field_8C`)
    const auto ProcessGymVolume = [&](eAudioEvents event) {
        float targetVol = GetDefaultVolume(event);
        if (field_8C == 1.0f) { // Fade in
            if (sound->m_Volume < targetVol) {
                const auto newVol = sound->m_Volume + 0.1f;
                sound->m_Volume   = newVol < targetVol ? newVol : targetVol;
            }
        } else if (field_8C == 2.0f) { // Fade out
            targetVol -= 18.0f;
            if (!(sound->m_Volume > targetVol)) {
                sound->StopSoundAndForget();
            } else {
                const auto newVol = sound->m_Volume - 0.1f;
                sound->m_Volume   = newVol > targetVol ? newVol : targetVol;
            }
        }
    };

    for (auto i = 0; i < MISSION_AUDIO_COUNT; i++) {
        auto& link = wavLinks[i];

        if (sound == link.m_Sound) {
            if (playTime == -1) {
                link.m_Sound = nullptr;
                return;
            }
            if (link.m_pEntity) {
                sound->SetPosition(link.m_pEntity->GetPosition());
            }
            continue;
        }

        switch (sound->m_Event) {
        case AE_CRANE_WINCH_MOVE: { // 0x68
            if (CTimer::GetTimeInMS() > m_nLastTimeHornPlayed + 300u) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
                CEntity* const physEntity = sound->m_PhysicalEntity;
                PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 2, AE_SCRIPT_CRANE_MOVE_STOP, sound->m_CurrPos, static_cast<CPhysical*>(physEntity), -12.0f, 1.0f, 0, 2.5f);
            } else {
                sound->m_Volume = m_Volume;
                sound->m_Speed  = m_Speed;
            }
            break;
        }
        case AE_SCRIPT_DUAL_THRUST: // 0x412
            if (CTimer::GetTimeInMS() > m_nLastTimeHornPlayed + 300u) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
            }
            break;
        case AE_SCRIPT_ROULETTE_SPIN: { // 0x43E
            if (CTimer::GetTimeInMS() > m_nLastTimeHornPlayed + 4500u) {
                if (!(sound->m_Volume > -40.0f)) {
                    sound->StopSoundAndForget();
                    m_nLastTimeHornPlayed = 0;
                } else {
                    sound->m_Volume -= 0.1f;
                }
                if (sound->m_Speed > 0.0f) {
                    const auto newSpeed = sound->m_Speed - 0.001f;
                    sound->m_Speed      = newSpeed < 0.0f ? 0.0f : newSpeed;
                }
            }
            if (CTimer::GetTimeInMS() > m_nLastTimeHornPlayed + 4800u && !field_7C) {
                const auto sfx = (int16)CAEAudioUtility::GetRandomNumberInRange(1, 3);
                PlayMissionBankSound(AE_SCRIPT_ROULETTE_BALL_BOUNCING, unusedPosn, nullptr, sfx, 3, 0, 0.0f, 2.0f, 1.0f);
                field_7C = 1;
            }
            break;
        }
        case AE_SCRIPT_GYM_BIKE_START: // 0x459
            ProcessGymVolume(AE_SCRIPT_GYM_BIKE_START);
            break;
        case AE_SCRIPT_GYM_RUNNING_MACHINE_START: // 0x45E
            ProcessGymVolume(AE_SCRIPT_GYM_RUNNING_MACHINE_START);
            break;
        case AE_SCRIPT_SWEETS_HORN: // 0x47B
            if (CTimer::GetTimeInMS() > m_nLastTimeHornPlayed + 500u) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
            }
            break;
        case AE_SCRIPT_BEE_BUZZ: // 0x48F
        case AE_SCRIPT_TEMPEST_SHIELD_GLOW: // 0x499
            if (CTimer::GetTimeInMS() > m_nLastTimeHornPlayed + 300u) {
                sound->StopSoundAndForget();
                m_nLastTimeHornPlayed = 0;
            }
            break;
        default:
            break;
        }
    }
}

// 0x4EC900
void CAEScriptAudioEntity::Service() {
    CVector posn = {-1000.0f, -1000.0f, -1000.0f};
    if (!m_Physical)
        return;
    if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_SCRIPT_CRANE_ENTER, this) != 0)
        return;

    PlayResidentSoundEvent(SND_BANK_SLOT_PLAYER_ENGINE_P, SND_BANK_GENRL_CRANE_P, 0, AE_SCRIPT_CRANE_ENTER, posn, m_Physical, 0.0f, 1.0f, 0, 2.5f);
}

void CAEScriptAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEScriptAudioEntity, 0x862E58, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Constructor, 0x5074D0);
    RH_ScopedInstall(Initialise, 0x5B9B60);
    RH_ScopedInstall(Service, 0x4EC900);
    RH_ScopedInstall(Reset, 0x4EC150);
    RH_ScopedInstall(GetMissionAudioLoadingStatus, 0x4EBF60);
    RH_ScopedInstall(IsMissionAudioSampleFinished, 0x4EBFE0);
    RH_ScopedInstall(GetMissionAudioEvent, 0x4EC020);
    RH_ScopedInstall(ClearMissionAudio, 0x4EC040);
    RH_ScopedInstall(SetMissionAudioPosition, 0x4EC0C0);
    RH_ScopedInstall(AttachMissionAudioToPhysical, 0x4EC100);
    RH_ScopedInstall(PreloadMissionAudio, 0x4EC190);
    RH_ScopedInstall(PlayLoadedMissionAudio, 0x4EC270);
    RH_ScopedInstall(GetMissionAudioPosition, 0x4EC4D0);
    RH_ScopedInstall(PlayResidentSoundEvent, 0x4EC550);
    RH_ScopedInstall(PlayMissionBankSound, 0x4EC6D0);
    RH_ScopedInstall(ProcessMissionAudioEvent, 0x4ECCF0);
    RH_ScopedOverloadedInstall(ReportMissionAudioEvent, "1", 0x4EE960, void (CAEScriptAudioEntity::*)(eAudioEvents, CPhysical*, float, float));
    RH_ScopedOverloadedInstall(ReportMissionAudioEvent, "2", 0x4EE940, void (CAEScriptAudioEntity::*)(eAudioEvents, CVector&));
    RH_ScopedVMTInstall(UpdateParameters, 0x4EC970);
}

CAEScriptAudioEntity* CAEScriptAudioEntity::Constructor() {
    this->CAEScriptAudioEntity::CAEScriptAudioEntity();
    return this;
}
