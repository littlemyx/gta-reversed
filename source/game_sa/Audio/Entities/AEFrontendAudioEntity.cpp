#include "StdInc.h"

#include "AEFrontendAudioEntity.h"

#include "AETwinLoopSoundEntity.h"
#include "AEAudioHardware.h"
#include "AESoundManager.h"
#include "AEAudioUtility.h"
#include "AESound.h"

// 0x5B9AB0
void CAEFrontendAudioEntity::Initialise() {
    m_nLastFrameGeneral_or_nFrameCount = 0;
    m_nLastFrameMissionComplete        = 0;
    m_nLastFrameBulletPass             = 0;
    m_BulletPassCount                  = 0;
    m_BulletPassBank                   = SND_BANK_GENRL_BULLET_PASS_FIRST;
    m_f7E                              = -1;
    m_nLastTimeCarRespray              = 0;
    m_nLatestTimerCount                = 0;
    m_bAmplifierWakeUp                 = false;
    m_pAmplifierWakeUp                 = nullptr;

    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_FRONTEND_MENU, SND_BANK_SLOT_FRONTEND_MENU);
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME);
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_BULLET_PASS_1, SND_BANK_SLOT_BULLET_PASS);

    m_nbLoadingTuneSeed = CAEAudioUtility::GetRandomNumberInRange(0, 3);
    AEAudioHardware.LoadSound(SND_BANK_GENRL_LOADING, 2 * m_nbLoadingTuneSeed + 0, SND_BANK_SLOT_COLLISIONS);
    AEAudioHardware.LoadSound(SND_BANK_GENRL_LOADING, 2 * m_nbLoadingTuneSeed + 1, SND_BANK_SLOT_WEAPON_GEN);
}

// 0x4DD440
void CAEFrontendAudioEntity::Reset() {
    m_nLastFrameGeneral_or_nFrameCount = 0;
    m_nLastFrameMissionComplete        = 0;
    m_nLastFrameBulletPass             = 0;
    m_BulletPassCount                  = 0;
    m_f7E                              = -1;
    AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_SCANNER_NOISE_START, this);
}

// 0x4DD4A0
void CAEFrontendAudioEntity::AddAudioEvent(eAudioEvents event, float fVolumeBoost, float fSpeed) {
    // Sound environment flags that are used by (almost) all of the events below (Names are from the VC, SA has them only as hex values)
    constexpr uint16 FLAGS_MENU_ROLLED_OFF = SOUND_FORCED_FRONT | SOUND_ROLLED_OFF | SOUND_IS_DUCKABLE | SOUND_IS_PAUSABLE | SOUND_PLAY_PHYSICALLY | SOUND_IS_CANCELLABLE | SOUND_FRONT_END; // 0x151B
    constexpr uint16 FLAGS_MENU            = SOUND_FORCED_FRONT | SOUND_IS_DUCKABLE | SOUND_IS_PAUSABLE | SOUND_PLAY_PHYSICALLY | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                      // 0x111B
    constexpr uint16 FLAGS_CENTER          = SOUND_IS_DUCKABLE | SOUND_IS_PAUSABLE | SOUND_PLAY_PHYSICALLY | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                                             // 0x11B
    constexpr uint16 FLAGS_TIMER           = SOUND_FORCED_FRONT | SOUND_IS_DUCKABLE | SOUND_PLAY_PHYSICALLY | SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                  // 0x110F
    constexpr uint16 FLAGS_UPDATEABLE      = SOUND_IS_DUCKABLE | SOUND_IS_PAUSABLE | SOUND_PLAY_PHYSICALLY | SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                    // 0x11F
    constexpr uint16 FLAGS_SCANNER         = SOUND_IS_DUCKABLE | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                                                                                       // 0x103
    constexpr uint16 FLAGS_DUCKABLE        = SOUND_IS_DUCKABLE | SOUND_FRONT_END;                                                                                                              // 0x101
    constexpr uint16 FLAGS_BULLET_PASS     = SOUND_REQUEST_UPDATES | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                                                                                   // 0x7
    constexpr uint16 FLAGS_FIRE_FAIL       = SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                                                                                                           // 0x3
    constexpr uint16 FLAGS_RETUNE          = SOUND_IS_DUCKABLE | SOUND_MUSIC_MASTERED | SOUND_PLAY_PHYSICALLY | SOUND_IS_CANCELLABLE | SOUND_FRONT_END;                                         // 0x14B
    constexpr uint16 FLAGS_RETUNE_PAUSED   = FLAGS_RETUNE | SOUND_IS_PAUSABLE;                                                                                                                 // 0x15B

    const CVector XVECP{ +1.0f, 0.0f, 0.0f };
    const CVector XVECM{ -1.0f, 0.0f, 0.0f };
    const CVector YVECP{ +0.0f, 1.0f, 0.0f };

    const float volume = GetDefaultVolume(event) + fVolumeBoost;

    // Plays a sound with the properties that are common to all sounds in here
    const auto Play = [&](eSoundBankSlot slot, eSoundID sfx, const CVector& pos, float speed, uint16 flags, int32 evt = AE_UNDEFINED, float speedVariance = 0.0f) {
        CAESound sound;
        sound.Initialise(slot, sfx, this, pos, volume, 1.0f, speed, 1.0f, 0, flags, speedVariance, 0);
        sound.m_Event = evt;
        return AESoundManager.RequestNewSound(&sound);
    };

    // Plays 2 sounds, one on the left, and one on the right
    const auto PlayStereo = [&](eSoundBankSlot slot, eSoundID sfxL, eSoundID sfxR, float speed, uint16 flags, int32 evt = AE_UNDEFINED) {
        Play(slot, sfxL, XVECM, speed, flags, evt);
        Play(slot, sfxR, XVECP, speed, flags, evt);
    };

    const auto PlayBulletPass = [&](const CVector& pos) {
        if (!AEAudioHardware.IsSoundBankLoaded(m_BulletPassBank, SND_BANK_SLOT_BULLET_PASS) || CTimer::GetFrameCounter() < m_nLastFrameBulletPass + 5) {
            return;
        }
        if (m_BulletPassCount > 10) {
            if (AESoundManager.AreSoundsPlayingInBankSlot(SND_BANK_SLOT_BULLET_PASS) != 0) {
                return;
            }
            m_BulletPassBank = (eSoundBank)(m_BulletPassBank + 1);
            if (m_BulletPassBank > SND_BANK_GENRL_BULLET_PASS_LAST) {
                m_BulletPassBank = SND_BANK_GENRL_BULLET_PASS_FIRST;
            }
            AEAudioHardware.LoadSoundBank(m_BulletPassBank, SND_BANK_SLOT_BULLET_PASS);
            m_BulletPassCount = 0;
            return;
        }
        m_nLastFrameBulletPass = CTimer::GetFrameCounter();
        Play(SND_BANK_SLOT_BULLET_PASS, (eSoundID)CAEAudioUtility::GetRandomNumberInRange(0, 2), pos, 1.0f, FLAGS_BULLET_PASS, event, 0.03125f);
        m_BulletPassCount++;
    };

    switch (event) {
    case AE_FRONTEND_START: { // 0x4DD5DB
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 25, 26, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_SELECT: { // 0x4DD712
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_MENU, SND_BANK_SLOT_FRONTEND_MENU)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_MENU, 6, 7, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_BACK: { // 0x4DD7F0
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_MENU, SND_BANK_SLOT_FRONTEND_MENU)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_MENU, 0, 1, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_HIGHLIGHT: { // 0x4DD88E
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_MENU, SND_BANK_SLOT_FRONTEND_MENU)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_MENU, 4, 5, fSpeed, FLAGS_MENU);
        break;
    }
    case AE_FRONTEND_ERROR: { // 0x4DD9EA
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_MENU, SND_BANK_SLOT_FRONTEND_MENU)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_MENU, 2, 3, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_NOISE_TEST: { // 0x4DD96C
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_MENU, SND_BANK_SLOT_FRONTEND_MENU)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_MENU, 8, YVECP, fSpeed, FLAGS_CENTER);
        break;
    }
    case AE_FRONTEND_PICKUP_WEAPON:
    case AE_FRONTEND_CAR_FIT_BOMB_TIMED:
    case AE_FRONTEND_CAR_FIT_BOMB_BOOBY_TRAPPED:
    case AE_FRONTEND_CAR_FIT_BOMB_REMOTE_CONTROLLED:
    case AE_FRONTEND_PURCHASE_WEAPON: { // 0x4DDA89
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || CTimer::GetFrameCounter() < m_nLastFrameGeneral_or_nFrameCount + 5) {
            break;
        }
        m_nLastFrameGeneral_or_nFrameCount = CTimer::GetFrameCounter();
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 27, 28, fSpeed, FLAGS_MENU);
        break;
    }
    case AE_FRONTEND_PICKUP_MONEY:
    case AE_FRONTEND_PICKUP_HEALTH:
    case AE_FRONTEND_PICKUP_ADRENALINE:
    case AE_FRONTEND_PICKUP_BODY_ARMOUR: { // 0x4DDB40
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || CTimer::GetFrameCounter() < m_nLastFrameGeneral_or_nFrameCount + 5) {
            break;
        }
        m_nLastFrameGeneral_or_nFrameCount = CTimer::GetFrameCounter();
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 16, 17, fSpeed, FLAGS_MENU);
        break;
    }
    case AE_FRONTEND_PICKUP_INFO:
    case AE_FRONTEND_DISPLAY_INFO: { // 0x4DD676
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 14, 15, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_PICKUP_DRUGS:
    case AE_FRONTEND_PICKUP_COLLECTABLE1:
    case AE_FRONTEND_PART_MISSION_COMPLETE: { // 0x4DE6A6
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || CTimer::GetFrameCounter() < m_nLastFrameMissionComplete + 5) {
            break;
        }
        m_nLastFrameMissionComplete = CTimer::GetFrameCounter();
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 18, 19, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_CAR_NO_CASH:
    case AE_FRONTEND_CAR_IS_HOT:
    case AE_FRONTEND_CAR_ALREADY_RIGGED: { // 0x4DDD27
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || CTimer::GetFrameCounter() < m_nLastFrameGeneral_or_nFrameCount + 5) {
            break;
        }
        m_nLastFrameGeneral_or_nFrameCount = CTimer::GetFrameCounter();
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 27, 28, 0.841f, FLAGS_MENU); // NOTE: `fSpeed` isn't used here
        break;
    }
    case AE_FRONTEND_CAR_RESPRAY: { // 0x4DDBF7
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
            break;
        }
        constexpr float SPRAY_SPEED_HIGH = 1.18921006f; // 0x3F983809
        float speedL, speedR;
        if (CAEAudioUtility::ResolveProbability(0.5f)) {
            speedL = SPRAY_SPEED_HIGH;
            speedR = 1.0f;
        } else {
            speedL = 1.0f;
            speedR = SPRAY_SPEED_HIGH;
        }
        Play(SND_BANK_SLOT_WEAPON_GEN, SND_GENRL_WEAPONS_SPRAY_PAINT, XVECM, speedL, FLAGS_TIMER, event);
        Play(SND_BANK_SLOT_WEAPON_GEN, SND_GENRL_WEAPONS_SPRAY_PAINT, XVECP, speedR, FLAGS_TIMER, event);
        m_nLastTimeCarRespray = CTimer::GetTimeInMS();
        break;
    }
    case AE_FRONTEND_BULLET_PASS_LEFT_REAR: { // 0x4DDE81
        PlayBulletPass({ -0.1f, -1.0f, 0.0f });
        break;
    }
    case AE_FRONTEND_BULLET_PASS_LEFT_FRONT: { // 0x4DDF69
        PlayBulletPass({ -0.1f, +1.0f, 0.0f });
        break;
    }
    case AE_FRONTEND_BULLET_PASS_RIGHT_REAR: { // 0x4DE010
        PlayBulletPass({ +0.1f, -1.0f, 0.0f });
        break;
    }
    case AE_FRONTEND_BULLET_PASS_RIGHT_FRONT: { // 0x4DE0BC
        PlayBulletPass({ +0.1f, +1.0f, 0.0f });
        break;
    }
    case AE_FRONTEND_WAKEUP_AMPLIFIER: { // 0x4DE1D8
        if (m_bAmplifierWakeUp || !AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        m_pAmplifierWakeUp = Play(SND_BANK_SLOT_FRONTEND_GAME, 8, YVECP, fSpeed, FLAGS_UPDATEABLE);
        m_bAmplifierWakeUp = m_pAmplifierWakeUp != nullptr;
        break;
    }
    case AE_FRONTEND_TIMER_COUNT: { // 0x4DE26D
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_TIMER_COUNT, this)) {
            if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
                break;
            }
            PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 4, 5, fSpeed, FLAGS_TIMER, AE_FRONTEND_TIMER_COUNT);
        }
        m_nLatestTimerCount = CTimer::GetTimeInMS();
        break;
    }
    case AE_FRONTEND_RADIO_RETUNE_START: { // 0x4DE381
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        auto& retune = CTimer::GetIsPaused() ? m_objRetunePaused : m_objRetune;
        if (retune.IsActive()) {
            break;
        }
        retune.Initialise(SND_BANK_SLOT_FRONTEND_GAME, 2, 1, this, 200, 650, -1, -1);
        retune.PlayTwinLoopSound(
            YVECP,
            volume,
            1.0f,
            1.0f,
            1.0f,
            (eSoundEnvironment)(CTimer::GetIsPaused() ? FLAGS_RETUNE_PAUSED : FLAGS_RETUNE)
        );
        break;
    }
    case AE_FRONTEND_RADIO_RETUNE_STOP: // 0x4DE480
        if (!CTimer::GetIsPaused()) {
            if (m_objRetune.IsActive()) {
                m_objRetune.StopSoundAndForget();
            }
            break;
        }
        [[fallthrough]];
    case AE_FRONTEND_RADIO_RETUNE_STOP_PAUSED: // 0x4DE4AF
        if (m_objRetunePaused.IsActive()) {
            m_objRetunePaused.StopSoundAndForget();
        }
        break;
    case AE_FRONTEND_RADIO_CLICK_ON: { // 0x4DE4CC
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 23, YVECP, 1.0f, FLAGS_CENTER);
        break;
    }
    case AE_FRONTEND_RADIO_CLICK_OFF: { // 0x4DE53D
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 23, YVECP, 0.8909f, FLAGS_CENTER);
        break;
    }
    case AE_FRONTEND_FIRE_FAIL_SNIPERRIFFLE: { // 0x4DE5AE
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 10, YVECP, fSpeed, FLAGS_FIRE_FAIL);
        break;
    }
    case AE_FRONTEND_FIRE_FAIL_ROCKET: { // 0x4DE62A
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 11, YVECP, fSpeed, FLAGS_FIRE_FAIL);
        break;
    }
    case AE_FRONTEND_RACE_321: { // 0x4DE79D
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 6, 7, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_RACE_GO: { // 0x4DE878
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        PlayStereo(SND_BANK_SLOT_FRONTEND_GAME, 12, 13, fSpeed, FLAGS_MENU_ROLLED_OFF);
        break;
    }
    case AE_FRONTEND_BUY_CAR_MOD: { // 0x4DDE08
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 9, YVECP, 1.0f, FLAGS_CENTER, AE_UNDEFINED, 0.0588f);
        break;
    }
    case AE_FRONTEND_SCANNER_NOISE_START: { // 0x4DE9ED
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_SCANNER_NOISE_START, this)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 3, YVECP, fSpeed, FLAGS_SCANNER, AE_FRONTEND_SCANNER_NOISE_START);
        break;
    }
    case AE_FRONTEND_SCANNER_NOISE_STOP: { // 0x4DEA87
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_SCANNER_NOISE_START, this);
        break;
    }
    case AE_FRONTEND_SCANNER_CLICK: { // 0x4DEA8F
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_SCANNER_CLICK, this)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 24, YVECP, fSpeed, FLAGS_SCANNER, AE_FRONTEND_SCANNER_CLICK);
        break;
    }
    case AE_FRONTEND_LOADING_TUNE_START: { // 0x4DEB29
        const auto sfxL = (eSoundID)(2 * m_nbLoadingTuneSeed);
        const auto sfxR = (eSoundID)(sfxL + 1);
        if (!AEAudioHardware.IsSoundLoaded(SND_BANK_GENRL_LOADING, sfxL, SND_BANK_SLOT_COLLISIONS)
            || !AEAudioHardware.IsSoundLoaded(SND_BANK_GENRL_LOADING, sfxR, SND_BANK_SLOT_WEAPON_GEN)
            || AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_LOADING_TUNE_START, this)
        ) {
            break;
        }
        Play(SND_BANK_SLOT_COLLISIONS, sfxL, XVECM, fSpeed, FLAGS_MENU, AE_FRONTEND_LOADING_TUNE_START);
        Play(SND_BANK_SLOT_WEAPON_GEN, sfxR, XVECP, fSpeed, FLAGS_MENU, AE_FRONTEND_LOADING_TUNE_START);
        break;
    }
    case AE_FRONTEND_LOADING_TUNE_STOP: { // 0x4DEC64
        AESoundManager.CancelSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_LOADING_TUNE_START, this);
        break;
    }
    case AE_MISSILE_LOCK: { // 0x4DE953
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME) || AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_MISSILE_LOCK, this)) {
            break;
        }
        Play(SND_BANK_SLOT_FRONTEND_GAME, 13, YVECP, fSpeed, FLAGS_DUCKABLE, AE_MISSILE_LOCK);
        break;
    }
    default:
        break;
    }
}

// 0x4DD480
bool CAEFrontendAudioEntity::IsRadioTuneSoundActive() {
    return CTimer::GetIsPaused()
        ? m_objRetunePaused.IsActive()
        : m_objRetune.IsActive();
}

// 0x4DD470
bool CAEFrontendAudioEntity::IsLoadingTuneActive() {
    return AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_FRONTEND_LOADING_TUNE_START, this);
}

// 0x4DEDA0
void CAEFrontendAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (!sound) {
        return;
    }

    // Moves the bullet pass sound from the front to the back (or vice versa) as it plays
    // BUG: The original code sets the Z coordinate, and not Y. Behaviour kept.
    const auto UpdateBulletPassPosition = [&](float x, bool isRear) {
        if (curPlayPos < 0 || curPlayPos > 350) {
            return;
        }
        const float t = (float)curPlayPos * (1.0f / 350.0f);
        const float z = isRear
            ? (t + t) - 1.0f
            : 1.0f - (t + t);
        sound->SetPosition(CVector{ x, 0.0f, z });
    };

    switch (sound->m_Event) {
    case AE_FRONTEND_CAR_RESPRAY: // 0x4DEED3
        if (curPlayPos > 0 && CTimer::GetTimeInMS() > m_nLastTimeCarRespray + 1900) {
            sound->StopSoundAndForget();
        }
        break;
    case AE_FRONTEND_BULLET_PASS_LEFT_REAR: // 0x4DEDD6
        UpdateBulletPassPosition(-0.1f, true);
        break;
    case AE_FRONTEND_BULLET_PASS_LEFT_FRONT: // 0x4DEE10
        UpdateBulletPassPosition(-0.1f, false);
        break;
    case AE_FRONTEND_BULLET_PASS_RIGHT_REAR: // 0x4DEE39
        UpdateBulletPassPosition(+0.1f, true);
        break;
    case AE_FRONTEND_BULLET_PASS_RIGHT_FRONT: // 0x4DEE70
        UpdateBulletPassPosition(+0.1f, false);
        break;
    case AE_FRONTEND_TIMER_COUNT: // 0x4DEEED
        if (curPlayPos > 0 && CTimer::GetTimeInMS() > m_nLatestTimerCount + 100) {
            sound->StopSoundAndForget();
        }
        break;
    default:
        break;
    }

    if (sound == m_pAmplifierWakeUp && curPlayPos == -1) {
        m_pAmplifierWakeUp = nullptr;
        m_bAmplifierWakeUp = false;
    }
}

void CAEFrontendAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEFrontendAudioEntity, 0x862E54, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(Initialise, 0x5B9AB0);
    RH_ScopedInstall(Reset, 0x4DD440);
    RH_ScopedInstall(AddAudioEvent, 0x4DD4A0);
    RH_ScopedInstall(IsRadioTuneSoundActive, 0x4DD480);
    RH_ScopedInstall(IsLoadingTuneActive, 0x4DD470);
    RH_ScopedVMTInstall(UpdateParameters, 0x4DEDA0);
}
