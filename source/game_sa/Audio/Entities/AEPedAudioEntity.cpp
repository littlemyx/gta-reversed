#include "StdInc.h"

#include "AEPedAudioEntity.h"
#include "AEAudioHardware.h"
#include "AEAudioUtility.h"

// Surface table (TOTAL_NUM_COLLISION_SURFACE_TYPES = 195 records of 16 B, read with `id << 4`); the exe's copy is at 0x8AD908 (read-only), `percent` is the field at +0xC.
// The first three fields are used by code that is not reversed yet.
struct tSurfaceHitEntry { int32 f0, f1, f2, percent; };
constexpr tSurfaceHitEntry kSurfaceHitTable[195] = { // detached mode: THE table; address mode: unused (the exe's table at 0x8AD908 is read)
    {34,34,70,100}, {34,34,70,100}, {34,34,70,100}, {34,34,70,100},
    {34,34,70,100}, {34,34,70,100}, {34,34,70,100}, {34,34,70,100},
    {34,34,70,100}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {34,34,70,100}, {31,31,0,20},
    {31,31,0,20}, {34,34,70,100}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {34,34,70,100}, {34,34,70,100},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {34,34,70,100}, {34,34,70,100},
    {34,34,70,100}, {34,34,70,100}, {-1,-1,0,20}, {-1,-1,0,20},
    {31,31,0,20}, {31,31,0,20}, {35,37,90,50}, {33,33,0,50},
    {40,40,60,40}, {-1,-1,0,30}, {-1,-1,0,30}, {-1,-1,0,30},
    {-1,-1,0,0}, {-1,-1,0,0}, {46,46,90,80}, {65,65,60,100},
    {62,62,80,100}, {32,32,40,100}, {69,71,80,40}, {69,71,80,40},
    {65,65,60,100}, {54,54,50,100}, {30,30,60,100}, {38,38,70,60},
    {60,60,0,30}, {12,14,0,40}, {-1,-1,0,20}, {20,28,90,100},
    {20,28,90,100}, {20,28,90,100}, {-1,-1,1,20}, {60,60,0,30},
    {4,4,60,30}, {34,34,70,100}, {19,19,80,50}, {31,31,0,20},
    {33,33,0,50}, {33,33,0,50}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {34,34,70,100}, {34,34,70,100}, {31,31,0,20},
    {34,34,70,100}, {34,34,70,100}, {33,33,0,50}, {33,33,0,50},
    {33,33,0,50}, {33,33,0,50}, {33,33,0,50}, {33,33,0,50},
    {-1,-1,0,20}, {-1,-1,0,20}, {-1,-1,0,20}, {-1,-1,0,20},
    {-1,-1,0,20}, {34,34,70,100}, {33,33,0,50}, {34,34,70,100},
    {33,33,0,50}, {33,33,0,50}, {34,34,70,100}, {34,34,70,100},
    {34,34,70,100}, {34,34,70,100}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {34,34,70,100},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {34,34,70,100},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {34,34,70,100},
    {31,31,0,20}, {31,31,0,20}, {34,34,70,100}, {34,34,70,100},
    {34,34,70,100}, {33,33,0,50}, {34,34,70,100}, {34,34,70,100},
    {34,34,70,100}, {34,34,70,100}, {34,34,70,100}, {31,31,0,20},
    {34,34,70,100}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {31,31,0,20}, {31,31,0,20},
    {31,31,0,20}, {31,31,0,20}, {-1,-1,0,20}, {-1,-1,0,20},
    {-1,-1,0,20}, {31,31,0,20}, {33,33,0,70}, {4,4,60,30},
    {31,31,0,20}, {34,34,70,100}, {65,65,60,100}, {31,31,0,20},
    {65,65,60,100}, {34,34,70,100}, {10,10,40,10}, {63,64,50,80},
    {55,55,60,80}, {59,59,30,20}, {4,4,60,30}, {39,39,50,80},
    {45,45,70,40}, {40,40,70,50}, {40,40,70,50}, {51,51,30,50},
    {52,52,50,20}, {47,49,30,20}, {65,65,60,100}, {-1,-1,0,0},
    {-1,-1,0,0}, {-1,-1,0,0}, {-1,-1,0,0}, {-1,-1,0,0},
    {-1,-1,0,0}, {-1,-1,0,0}, {-1,-1,0,0}, {-1,-1,0,0},
    {5,9,90,60}, {11,11,0,30}, {61,61,0,40}, {50,50,0,30},
    {0,2,0,70}, {0,2,0,30}, {4,6,0,40},
};
NOTSA_GLOBAL_VERIFY(0x8AD908, kSurfaceHitTable);
#line 7
//! Volume offset of a collision with the given surface (dB)
//! NOTE: The table at 0x8AD908 (195 x 16 B, `percent` at +0xC) isn't named yet
// 0x4DA510
static float GetSurfaceHitVolume(uint8 surfaceId) {
    NOTSA_GLOBAL_LOCAL_REF(table, 0x8AD908, (const tSurfaceHitEntry[195]), kSurfaceHitTable); const auto percent = table[surfaceId].percent;
    return (float)(X87R(std::log10(X87W((x87d)percent * (x87d)0.01f)) * (x87d)20.0));
}

void CAEPedAudioEntity::InjectHooks() {
    RH_ScopedClass(CAEPedAudioEntity);
    RH_ScopedCategory("Audio/Entities");

    //RH_ScopedInstall(Constructor, 0x5DE8D0, { .Reversed = false });

    RH_ScopedInstall(Initialise, 0x4E0E80);
    RH_ScopedInstall(StaticInitialise, 0x5B98A0);
    RH_ScopedInstall(Terminate, 0x4E1360);
    RH_ScopedInstall(AddAudioEvent, 0x4E2BB0);
    RH_ScopedInstall(TurnOnJetPack, 0x4E28A0);
    RH_ScopedInstall(TurnOffJetPack, 0x4E2A70);
    RH_ScopedInstall(StopJetPackSound, 0x4E1120);
    RH_ScopedInstall(UpdateJetPack, 0x4E0EE0);
    RH_ScopedInstall(PlayWindRush, 0x4E1170);
    RH_ScopedInstall(UpdateParameters, 0x4E1180);
    RH_ScopedInstall(HandleFootstepEvent, 0x4E13A0);
    RH_ScopedInstall(HandleSkateEvent, 0x4E17E0);
    RH_ScopedInstall(HandleLandingEvent, 0x4E18E0);
    RH_ScopedInstall(HandlePedSwing, 0x4E1A40);
    RH_ScopedInstall(HandlePedHit, 0x4E1CC0);
    RH_ScopedInstall(HandlePedJacked, 0x4E2350);
    RH_ScopedInstall(HandleSwimSplash, 0x4E26A0);
    RH_ScopedInstall(HandleSwimWake, 0x4E2790);
    RH_ScopedInstall(PlayShirtFlap, 0x4E2A90);
    RH_ScopedInstall(Service, 0x4E2EE0);
}

// 0x5DE8D0
CAEPedAudioEntity::CAEPedAudioEntity() : CAEAudioEntity() {
    m_pPed = nullptr;
    m_bCanAddEvent = false;

    m_JetPackSound0 = nullptr;
    m_JetPackSound1 = nullptr;
    m_JetPackSound2 = nullptr;
}

// (CEntity* entity)
// 0x4E0E80
void CAEPedAudioEntity::Initialise(CPed* ped) {
    m_pPed = ped;
    m_nSfxId = 0;
    m_LastSwimWakeTriggerTimeMs = 0;

    m_bJetPackPlaying = false;
    m_JetPackSound0 = nullptr;
    m_JetPackSound1 = nullptr;
    m_fVolume1 = -100.0f;
    m_fVolume2 = -100.0f;

    field_150 = nullptr;
    field_154 = -100.0f;
    field_158 = -100.0f;
    m_bCanAddEvent = true;
}

// 0x5B98A0
void CAEPedAudioEntity::StaticInitialise() {
    AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING);
}

// 0x4E1360
void CAEPedAudioEntity::Terminate() {
    m_bCanAddEvent = false;
    m_pPed   = nullptr;
    StopJetPackSound();
    AESoundManager.CancelSoundsOwnedByAudioEntity(this, true);
    if (m_sTwinLoopSoundEntity.IsActive()) {
        m_sTwinLoopSoundEntity.StopSoundAndForget();
    }
}

// 0x4E2BB0
void CAEPedAudioEntity::AddAudioEvent(eAudioEvents event, float volume, float speed, CPhysical* ped, eSurfaceType surfaceId, int32 a7, uint32 maxVol) {
    if (!m_bCanAddEvent)
        return;

    if (!m_pPed)
        return;

    switch (event) {
    case AE_PED_FOOTSTEP_LEFT:
    case AE_PED_FOOTSTEP_RIGHT:
        HandleFootstepEvent(event, volume, speed, surfaceId);
        break;
    case AE_PED_SKATE_LEFT:
    case AE_PED_SKATE_RIGHT:
        HandleSkateEvent(event, volume, speed);
        break;
    case AE_PED_LAND_ON_FEET_AFTER_FALL:
    case AE_PED_COLLAPSE_AFTER_FALL:
        HandleLandingEvent(event);
        break;
    case AE_PED_SWING:
        HandlePedSwing(event, a7, maxVol);
        break;
    case AE_PED_HIT_HIGH:
    case AE_PED_HIT_LOW:
    case AE_PED_HIT_GROUND:
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
        HandlePedHit(event, ped, surfaceId, volume, maxVol);
        break;
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_BIKE:
    case AE_PED_JACKED_DOZER:
        HandlePedJacked(event);
        break;
    case AE_PED_SWIM_STROKE_SPLASH:
    case AE_PED_SWIM_DIVE_SPLASH:
        HandleSwimSplash(event);
        break;
    case AE_PED_SWIM_WAKE:
        HandleSwimWake(event);
        break;
    case AE_PED_CRUNCH: {
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS)) {
            break;
        }

        volume += GetDefaultVolume(AE_PED_CRUNCH);

        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_PED_CRUNCH, this)) {
            break;
        }
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
            .SoundID           = 29,
            .AudioEntity       = this,
            .Pos               = ped->GetPosition(),
            .Volume            = volume,
            .RollOffFactor     = 1.5f,
            .Speed             = speed,
            .Doppler           = 1.0f,
            .FrameDelay        = 0,
            .Flags             = SOUND_DEFAULT,
            .FrequencyVariance = std::bit_cast<float>(0x3D75C290u), // 0x4E2D8C / 0x4E2E29: the exe stores 0x3D75C290, 0.06f is 0x3D75C28F
            .PlayTime          = 0,
            .EventID           = AE_PED_CRUNCH
        });

        if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(AE_PED_KNOCK_DOWN, this) != 0) {
            break;
        }
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
            .SoundID           = (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(47, 49)),
            .AudioEntity       = this,
            .Pos               = ped->GetPosition(),
            .Volume            = volume,
            .RollOffFactor     = 1.5f,
            .Speed             = speed,
            .Doppler           = 1.0f,
            .FrameDelay        = 0,
            .Flags             = SOUND_DEFAULT,
            .FrequencyVariance = std::bit_cast<float>(0x3D75C290u), // 0x4E2D8C / 0x4E2E29: the exe stores 0x3D75C290, 0.06f is 0x3D75C28F
            .PlayTime          = 0,
            .EventID           = AE_PED_KNOCK_DOWN
        });
        break;
    }
    default:
        break;
    }
}

// 0x4E28A0
void CAEPedAudioEntity::TurnOnJetPack() {
    if (!m_pPed)
        return;

    if (m_bJetPackPlaying || m_JetPackSound1 || m_JetPackSound0 || m_JetPackSound2)
        return;

    m_fVolume1 = -100.0f;
    m_fVolume2 = -100.0f;
    m_fVolume3 = +2.000f;
    m_JetPackSoundSpeedMult = 0.400f;

    m_bJetPackPlaying = true;

    const auto PlayJetPackSound = [&](eSoundBankSlot slot, eSoundID soundID) {
        return AESoundManager.PlaySound({
            .BankSlotID  = slot,
            .SoundID     = soundID,
            .AudioEntity = this,
            .Pos         = m_pPed->GetPosition(),
            .Volume      = -100.f,
            .Flags       = SOUND_REQUEST_UPDATES,
        });
    };
    m_JetPackSound0 = PlayJetPackSound(SND_BANK_SLOT_VEHICLE_GEN, 26);
    m_JetPackSound1 = PlayJetPackSound(SND_BANK_SLOT_WEAPON_GEN, 10);
    m_JetPackSound2 = PlayJetPackSound(SND_BANK_SLOT_FRONTEND_GAME, 0);
}

// 0x4E2A70
void CAEPedAudioEntity::TurnOffJetPack() {
    StopJetPackSound();
    m_bJetPackPlaying = false;
}

// 0x4E1120
void CAEPedAudioEntity::StopJetPackSound() {
    if (m_JetPackSound0) {
        m_JetPackSound0->StopSoundAndForget();
        m_JetPackSound0 = nullptr;
    }

    if (m_JetPackSound1) {
        m_JetPackSound1->StopSoundAndForget();
        m_JetPackSound1 = nullptr;
    }

    if (m_JetPackSound2) {
        m_JetPackSound2->StopSoundAndForget();
        m_JetPackSound2 = nullptr;
    }
}

// exe 0x4E1068..0x4E10A4: `fld angle; fsin; fcom 0; fst dword [tmp]; (fchs if < 0); fmul [0xB613B0]; fadd 1.0` - the fsin result is a 64-bit-mantissa value that is NOT rounded to float before
// the multiply (fsin ignores the precision control; only the `fst dword` copy is a float), so |sin| * factor rounds once at the precision control, not after a float rounding of |sin|.
// The second use (sound 2) reloads the float copy: `fld [tmp]`, abs.
static void JetPackSinSpeeds(float angle, float factor, float& speed0, float& speed) {
#if NOTSA_X87_ASM
    const float zero = 0.0f, one = 1.0f;
    float tmp, s0, s1;
    __asm {
        fld   dword ptr [angle]
        fsin
        fcom  dword ptr [zero]
        fst   dword ptr [tmp]
        fnstsw ax
        test  ah, 5
        jp    L_abs0
        fchs
    L_abs0:
        fmul  dword ptr [factor]
        fadd  dword ptr [one]
        fstp  dword ptr [s0]
        fld   dword ptr [tmp]
        fcomp dword ptr [zero]
        fld   dword ptr [tmp]
        fnstsw ax
        test  ah, 5
        jp    L_abs1
        fchs
    L_abs1:
        fstp  dword ptr [s1]
    }
    speed0 = s0;
    speed = s1;
#elif defined(NOTSA_PORTABLE)
    notsa::fp::sites::JetPackSinSpeeds(angle, factor, speed0, speed);
#else
    const float sn = (float)x87::sin(angle);
    speed = sn < 0.0f ? -sn : sn;
    speed0 = speed * factor + 1.0f;
#endif
}

// 0x4E0EE0
void CAEPedAudioEntity::UpdateJetPack(float thrustFwd, float thrustAngle) {
    if (!m_bJetPackPlaying || !m_JetPackSound1 || !m_JetPackSound0 || !m_JetPackSound2) {
        return;
    }

    // The exe clamps with `fcom` + a flag test, not with min / max: the limit wins unless the value is strictly inside (a NaN becomes the limit).
    //   0x4E0F39 / 0x4E0F87 / 0x4E0FAE / ...:  `fcom lim; test ah, 5; jnp keep`   -> v <  lim ? v : lim
    //   0x4E0FD8 / 0x4E1026 / 0x4E1047 / ...:  `fcom lim; test ah, 0x41; je keep`  -> v >  lim ? v : lim
    const auto ClampBelow = [](float v, float lim) { return v < lim ? v : lim; };
    const auto ClampAbove = [](float v, float lim) { return v > lim ? v : lim; };

    // 0x4E0F1C: `fcomp [0.5]; test ah, 0x41; jne`: <, == and unordered (NaN) take the "flying" branch
    if (!(thrustFwd > 0.5f)) { // flying
        m_fVolume1 = ClampAbove(m_fVolume1 - 5.0f, -100.0f);
        m_fVolume2 = ClampBelow(6.0f + m_fVolume2, -17.0f);
        m_fVolume3 = ClampAbove(m_fVolume3 - 0.3f, 2.0f);
        m_JetPackSoundSpeedMult = ClampAbove(m_JetPackSoundSpeedMult - 0.031f, 0.4f);
    } else { // idle
        m_fVolume1 = ClampBelow(15.0f + m_fVolume1, -15.0f);
        m_fVolume2 = ClampAbove(m_fVolume2 - 7.1f, -100.0f);
        m_fVolume3 = ClampBelow(0.3f + m_fVolume3, 11.0f);
        m_JetPackSoundSpeedMult = ClampBelow(0.031f + m_JetPackSoundSpeedMult, 0.71f);
    }

    // 0xB613B0 is computed by the CRT start-up (0x84C7E0): `0.93f - 1.0f` (-0.06999999285, NOT the literal -0.07f)
    constexpr float kPitchFactor = 0.93f - 1.0f;
    float speed0, speed; // speed0 = |sin| * kPitchFactor + 1.0, speed = (float)|sin|
    JetPackSinSpeeds(thrustAngle, kPitchFactor, speed0, speed);

    // 0.0f == 0xB61384 (uninitialized)
    m_JetPackSound0->m_Volume = m_fVolume1 + 0.0f;
    m_JetPackSound0->m_Speed = speed0;

    m_JetPackSound1->m_Volume = m_fVolume2 + 0.0f;
    m_JetPackSound1->m_Speed = 0.56f;

    m_JetPackSound2->m_Volume = m_fVolume3 + 0.0f;
    m_JetPackSound2->m_Speed = (speed * 0.2f + 1.0f) * m_JetPackSoundSpeedMult; // exe 0x4E10F7: `fmul [0x8AE620]` (0.2f), not a division by 5
}

// 0x4E1170
void CAEPedAudioEntity::PlayWindRush(float, float) {
    // NOP
}

// 0x4E1180
void CAEPedAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (sound == m_JetPackSound1) {
        if (curPlayPos == -1) {
            m_JetPackSound1 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    if (sound == m_JetPackSound0) {
        if (curPlayPos == -1) {
            m_JetPackSound0 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    if (sound == m_JetPackSound2) {
        if (curPlayPos == -1) {
            m_JetPackSound2 = nullptr;
            return;
        }
        if (m_pPed) {
            sound->SetPosition(m_pPed->GetPosition());
        }
        return;
    }

    // shit
    if (sound == field_150) {
        if (curPlayPos == -1) {
            field_150 = nullptr;
        }
        return;
    }

    if (curPlayPos < 0)
        return;

    switch (sound->m_Event) {
    case AE_PED_SWING:
    case AE_PED_HIT_HIGH:
    case AE_PED_HIT_LOW:
    case AE_PED_HIT_GROUND:
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_BIKE:
    case AE_PED_JACKED_DOZER:
        if (CTimer::GetTimeInMS() < NOTSA_FTOL(uint32, sound->m_ClientVariable))
            return;
        sound->m_Speed = 1.0f;
        return;
    case AE_PED_SWIM_WAKE: {
        const auto volume = GetDefaultVolume(AE_PED_SWIM_WAKE);

        if (CTimer::GetTimeInMS() <= m_LastSwimWakeTriggerTimeMs + 100) { // 0x4E130C: fade in by 0.6 dB per call, up to the default volume
            if (!(sound->m_Volume < volume)) { // `fcom; test ah,5; jp` (NaN returns)
                return;
            }
            const float up = sound->m_Volume + 0.6f;
            sound->m_Volume = up < volume ? up : volume; // 0x4E1320..0x4E12EC
        } else { // 0x4E12B9: fade out by 0.6 dB per call, stop 20 dB below the default volume
            const float lo = volume - 20.0f;
            if (!(sound->m_Volume > lo)) { // `fcom; test ah,0x41; jne` (NaN stops)
                sound->StopSoundAndForget();
                m_LastSwimWakeTriggerTimeMs = 0;
                return;
            }
            const float down = sound->m_Volume - 0.6f;
            sound->m_Volume = down > lo ? down : lo; // 0x4E12DB
        }

        return;
    }
    default:
        return;
    }
}

// 0x4E13A0
void CAEPedAudioEntity::HandleFootstepEvent(eAudioEvents event, float volume, float speed, eSurfaceType forcedSurfaceType) {
    if (m_pPed->bIsInTheAir) { // 0x4E13B3
        return;
    }

    // 0x4E13C7..0x4E13F2: (default + volume) + rand(-3, 3), the random value is drawn for the player and for every other ped
    volume = GetDefaultVolume(event) + volume;
    volume = (float)CAEAudioUtility::GetRandomNumberInRange(-3, 3) + volume;

    // 0x4E1777..0x4E17CE: every footstep sound is played with these parameters
    const auto PlayFootstepSound = [&](eSoundBankSlot slot, eSoundID sfx, int16 playTime) {
        AESoundManager.PlaySound({
            .BankSlotID         = slot,
            .SoundID            = sfx,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = volume,
            .Speed              = speed,
            .Flags              = SOUND_START_PERCENTAGE,
            .FrequencyVariance  = 0.0588f,
            .PlayTime           = playTime,
            .RegisterWithEntity = m_pPed,
        });
    };

    // 0x4E1686 (the player's fallback) / 0x4E172D (every other ped): the generic bank, sound 1..5
    const auto PlayGenericFootstepSound = [&]() {
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
            AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
            return;
        }
        PlayFootstepSound(SND_BANK_SLOT_FOOTSTEPS_GENERIC, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(1, 5)), 0);
    };

    if (FindPlayerPed(-1) != m_pPed) { // 0x4E172D
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
            AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
            return;
        }
        volume -= 3.0f; // 0x4E1743
        PlayFootstepSound(SND_BANK_SLOT_FOOTSTEPS_GENERIC, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(1, 5)), 0);
        return;
    }

    // The player: one bank per surface, all in the player's footstep slot. A bank that isn't there yet is requested (the sounds
    // that play in the slot at that moment are cancelled) and the generic footstep is played instead.
    const auto PlayPlayerSurfaceFootstepSound = [&](eSoundBank bank, int32 maxSfx) {
        if (AEAudioHardware.IsSoundBankLoaded(bank, SND_BANK_SLOT_FOOTSTEPS_PLAYER)) {
            PlayFootstepSound(SND_BANK_SLOT_FOOTSTEPS_PLAYER, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, maxSfx)), 0);
            return;
        }
        const auto status = AESoundManager.AreSoundsPlayingInBankSlot(SND_BANK_SLOT_FOOTSTEPS_PLAYER);
        if (status == 1 || status == 2) {
            AESoundManager.CancelSoundsInBankSlot(SND_BANK_SLOT_FOOTSTEPS_PLAYER, false);
        }
        AEAudioHardware.LoadSoundBank(bank, SND_BANK_SLOT_FOOTSTEPS_PLAYER);
        PlayGenericFootstepSound();
    };

    const auto contactSurface = m_pPed->m_nContactSurface;
    if (g_surfaceInfos.IsAudioWater((uint8)forcedSurfaceType) || g_surfaceInfos.IsAudioWater(contactSurface)) { // 0x4E1703
        if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
            PlayFootstepSound(SND_BANK_SLOT_SWIMMING, (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(0, 4)), 50);
        } else {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING);
            PlayGenericFootstepSound();
        }
    } else if (g_surfaceInfos.IsAudioConcrete(contactSurface)) { // 0x4E145E
        PlayGenericFootstepSound();
    } else if (g_surfaceInfos.IsAudioGrass(contactSurface) || g_surfaceInfos.IsAudioLongGrass(contactSurface)) { // 0x4E16BA
        PlayPlayerSurfaceFootstepSound(SND_BANK_FEET_GRASS, 4);
    } else if (g_surfaceInfos.IsAudioSand(contactSurface)) { // 0x4E14B7
        PlayPlayerSurfaceFootstepSound(SND_BANK_FEET_SAND, 3);
    } else if (g_surfaceInfos.IsAudioGravel(contactSurface)) { // 0x4E1531
        PlayPlayerSurfaceFootstepSound(SND_BANK_FEET_GRAVEL, 4);
    } else if (g_surfaceInfos.IsAudioWood(contactSurface)) { // 0x4E157D
        PlayPlayerSurfaceFootstepSound(SND_BANK_FEET_WOOD, 4);
    } else if (g_surfaceInfos.IsAudioTile(contactSurface)) { // 0x4E15DB
        PlayPlayerSurfaceFootstepSound(SND_BANK_FEET_TILE, 4);
    } else if (g_surfaceInfos.IsAudioMetal(contactSurface)) { // 0x4E1636
        PlayPlayerSurfaceFootstepSound(SND_BANK_FEET_METAL, 4);
    } else { // 0x4E168F
        PlayGenericFootstepSound();
    }
}

// 0x4E17E0
void CAEPedAudioEntity::HandleSkateEvent(eAudioEvents event, float volume, float speed) {
    if (m_pPed->bIsInTheAir) {
        return;
    }
    // 0x4E1808..0x4E1839: (default + volume) + rand(-3, 3); the random value is drawn before the bank is checked
    volume = GetDefaultVolume(event) + volume;
    volume = (float)CAEAudioUtility::GetRandomNumberInRange(-3, 3) + volume;
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
        return;
    }
    AESoundManager.PlaySound({
        .BankSlotID         = SND_BANK_SLOT_FOOTSTEPS_GENERIC,
        .SoundID            = (eSoundID)(event == AE_PED_SKATE_LEFT ? 7 : 8),
        .AudioEntity        = this,
        .Pos                = m_pPed->GetPosition(),
        .Volume             = volume,
        .Speed              = speed,
        .FrequencyVariance  = 0.0588f, // 0x3D70D845
        .RegisterWithEntity = m_pPed,
    });
}

// 0x4E18E0
void CAEPedAudioEntity::HandleLandingEvent(eAudioEvents event) {
    if (m_pPed->bIsInTheAir) {
        return;
    }

    const auto PlayLandingSound = [&](eSoundBankSlot slot, eSoundID soundID, float volume, int16 playPos) {
        AESoundManager.PlaySound({
            .BankSlotID = slot,
            .SoundID            = soundID,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = volume,
            .Flags              = SOUND_START_PERCENTAGE,
            .FrequencyVariance  = 0.0588f,
            .PlayTime           = playPos,
            .RegisterWithEntity = m_pPed
        });
    };
    const auto volume = GetDefaultVolume(event);
    if (g_surfaceInfos.IsAudioWater(m_pPed->m_nContactSurface)) {
        if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
            PlayLandingSound(
                SND_BANK_SLOT_SWIMMING,
                CAEAudioUtility::GetRandomNumberInRange<eSoundID>(SND_GENRL_SWIMMING_SWIM1, SND_GENRL_SWIMMING_SWIM5),
                std::max(0.f, volume),
                50
            );
        }
    } else {
        if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
            PlayLandingSound(
                SND_BANK_SLOT_FOOTSTEPS_GENERIC,
                event == AE_PED_LAND_ON_FEET_AFTER_FALL ? 6 : 0,
                volume,
                0
            );
        }
    }
}

// 0x4E1A40
void CAEPedAudioEntity::HandlePedSwing(eAudioEvents event, int32 a3, uint32 maxVol) {
    auto& weapon = m_pPed->GetActiveWeapon();

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (!AudioEngine.IsLoadingTuneActive()) {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        }
        return;
    }

    int16 sfx         = -1;
    float speed       = 1.0f;
    float extraVolume = 0.0f;

    switch (weapon.m_Type) {
    case WEAPON_UNARMED:
    case WEAPON_BRASSKNUCKLE:
    case WEAPON_CHAINSAW:
    case WEAPON_FLOWERS:
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_PISTOL:
    case WEAPON_PISTOL_SILENCED:
    case WEAPON_DESERT_EAGLE:
    case WEAPON_SHOTGUN:
    case WEAPON_SAWNOFF_SHOTGUN:
    case WEAPON_SPAS12_SHOTGUN:
    case WEAPON_MICRO_UZI:
    case WEAPON_MP5:
    case WEAPON_AK47:
    case WEAPON_M4:
    case WEAPON_TEC9:
    case WEAPON_COUNTRYRIFLE:
    case WEAPON_SNIPERRIFLE:
    case WEAPON_FLAMETHROWER:
    case WEAPON_MINIGUN:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
    case WEAPON_DETONATOR:
    case WEAPON_SPRAYCAN:
    case WEAPON_EXTINGUISHER:
    case WEAPON_CAMERA:
    case WEAPON_NIGHTVISION:
    case WEAPON_INFRARED:
        sfx = 0x57; speed = 0.84f; extraVolume = -6.0f;
        break;
    case WEAPON_GOLFCLUB:
    case WEAPON_CANE:
        sfx = 0x2F;
        break;
    case WEAPON_NIGHTSTICK:
    case WEAPON_POOL_CUE:
        sfx = 0x56; speed = 0.84f;
        break;
    case WEAPON_KNIFE:
    case WEAPON_KATANA:
        sfx = 0x58; extraVolume = -3.0f;
        break;
    case WEAPON_BASEBALLBAT:
    case WEAPON_SHOVEL:
        sfx = 0x56; speed = 0.67f;
        break;
    case WEAPON_DILDO1:
    case WEAPON_DILDO2:
    case WEAPON_VIBE1:
    case WEAPON_VIBE2:
        sfx = 0x56; speed = 0.84f; extraVolume = -6.0f;
        break;
    default: // 19, 20, 21, 35, 36 and everything above 45
        break;
    }

    // NOTE: The following is based on `a3`, which isn't a weapon type but (apparently) an event ID
    switch (a3) {
    case AE_PED_HIT_GROUND_KICK:
    case AE_PED_HIT_HIGH_UNARMED:
    case AE_PED_HIT_LOW_UNARMED:
        sfx         = 0x57;
        speed       = 0.84f;
        extraVolume = -6.0f;
        break;
    case AE_PED_HIT_MARTIAL_PUNCH:
    case AE_PED_HIT_MARTIAL_KICK:
        sfx         = 0x57;
        speed       = 0.84f;
        extraVolume = -2.0f;
        break;
    default:
        if (sfx < 0) {
            return;
        }
        break;
    }

    m_tempSound.Initialise(
        SND_BANK_SLOT_WEAPON_GEN,
        (eSoundID)sfx,
        this,
        m_pPed->GetPosition(),
        extraVolume + GetDefaultVolume(event),
        1.0f,
        speed,
        1.0f,
        0,
        SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
        0.04f,
        0
    );
    m_tempSound.RegisterWithPhysicalEntity(m_pPed);
    if (maxVol > 0u) {
        m_tempSound.m_SpeedVariance  = 0.0f;
        m_tempSound.m_Speed          = 0.0f;
        m_tempSound.m_ClientVariable = (float)(CTimer::GetTimeInMS() + maxVol);
        m_tempSound.m_Event          = event;
        m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, true);
    }
    AESoundManager.RequestNewSound(&m_tempSound);
}

// 0x4E1CC0
void CAEPedAudioEntity::HandlePedHit(eAudioEvents event, CPhysical* physical, uint8 surfaceId, float volume, uint32 maxVol) {
    const auto& weapon = m_pPed->GetActiveWeapon();

    int16          sfxA      = -1;                       // Sound played first (weapon dependant)
    int16          sfxB      = -1;                       // Sound played second (victim dependant)
    eSoundBankSlot slotB     = SND_BANK_SLOT_WEAPON_GEN; // Slot of the second sound
    float          volumeA   = 0.0f;                     // Volume offset of the first sound
    float          volumeB   = -3.0f;                    // Volume offset of the second sound
    float          variance  = 0.0588f;                  // Frequency variance of the first sound
    bool           skipA     = false;                    // Skip playing the first sound?

    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (!AudioEngine.IsLoadingTuneActive()) {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        }
        return;
    }

    const auto baseVolume = GetDefaultVolume(event) + volume;

    switch (weapon.m_Type) {
    case WEAPON_UNARMED:
    case WEAPON_NIGHTVISION:
    case WEAPON_INFRARED:
        sfxA = 0x3A;
        break;
    case WEAPON_BRASSKNUCKLE:
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_PISTOL:
    case WEAPON_PISTOL_SILENCED:
    case WEAPON_DESERT_EAGLE:
    case WEAPON_SHOTGUN:
    case WEAPON_SAWNOFF_SHOTGUN:
    case WEAPON_SPAS12_SHOTGUN:
    case WEAPON_MICRO_UZI:
    case WEAPON_MP5:
    case WEAPON_AK47:
    case WEAPON_M4:
    case WEAPON_TEC9:
    case WEAPON_COUNTRYRIFLE:
    case WEAPON_SNIPERRIFLE:
    case WEAPON_FLAMETHROWER:
    case WEAPON_MINIGUN:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
    case WEAPON_DETONATOR:
    case WEAPON_SPRAYCAN:
    case WEAPON_EXTINGUISHER:
    case WEAPON_CAMERA:
        sfxA = 0x2C;
        break;
    case WEAPON_GOLFCLUB:
        sfxA = 0x30;
        break;
    case WEAPON_NIGHTSTICK:
        sfxA = 0x23;
        break;
    case WEAPON_KNIFE:
        sfxA = 0x39; variance = 0.03f;
        break;
    case WEAPON_BASEBALLBAT:
        sfxA = 0x22;
        break;
    case WEAPON_SHOVEL:
        sfxA = 0x4B;
        break;
    case WEAPON_POOL_CUE:
        sfxA = 0x43;
        break;
    case WEAPON_KATANA:
        sfxA = 0x38;
        break;
    case WEAPON_DILDO1:
    case WEAPON_DILDO2:
    case WEAPON_VIBE1:
    case WEAPON_VIBE2:
        sfxA = 0x32;
        break;
    case WEAPON_FLOWERS:
        sfxA = 0x36; variance = 0.07f;
        break;
    case WEAPON_CANE:
        sfxA = 0x2E;
        break;
    case WEAPON_CHAINSAW:
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS:
    case WEAPON_FREEFALL_BOMB:
    case WEAPON_RLAUNCHER:
    case WEAPON_RLAUNCHER_HS:
        sfxA = -1;
        break;
    default:
        sfxA = -1;
        break;
    }

    // 0x4E1E15
    if (physical && physical->GetIsTypePed()) {
        // Common to chainsaw hits - Uses a different bank/slot
        const auto CheckChainsawBank = [&]() {
            if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_BULLET_HITS)) {
                return false;
            }
            slotB = SND_BANK_SLOT_BULLET_HITS;
            sfxB  = (int16)CAEAudioUtility::GetRandomNumberInRange(7, 9);
            skipA = true;
            return true;
        };

        switch (event) {
        case AE_PED_HIT_HIGH: { // 0x4E1E43
            switch (weapon.m_Type) {
            case WEAPON_BRASSKNUCKLE:
                volumeA = 1.0f;
                sfxB    = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
                break;
            case WEAPON_KNIFE:
            case WEAPON_KATANA:
                sfxB    = 0x51;
                volumeB = -9.0f;
                skipA   = true;
                break;
            case WEAPON_BASEBALLBAT:
                volumeA = -10.0f;
                sfxB    = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
                break;
            case WEAPON_CHAINSAW:
                if (!CheckChainsawBank()) {
                    return;
                }
                break;
            case WEAPON_FLOWERS:
                sfxB = -1;
                break;
            default:
                volumeA = -15.0f;
                sfxB    = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
                break;
            }
            break;
        }
        case AE_PED_HIT_LOW: { // 0x4E1E88
            switch (weapon.m_Type) {
            case WEAPON_BRASSKNUCKLE:
                volumeA = 1.0f;
                sfxB    = (int16)CAEAudioUtility::GetRandomNumberInRange(0x24, 0x27);
                break;
            case WEAPON_KNIFE:
            case WEAPON_KATANA:
                sfxB    = 0x51;
                volumeB = -9.0f;
                skipA   = true;
                break;
            case WEAPON_BASEBALLBAT:
                volumeA = -10.0f;
                sfxB    = (int16)CAEAudioUtility::GetRandomNumberInRange(0x24, 0x27);
                break;
            case WEAPON_CHAINSAW:
                if (!CheckChainsawBank()) {
                    return;
                }
                break;
            case WEAPON_FLOWERS:
                sfxB = -1;
                break;
            default:
                volumeA = -15.0f;
                sfxB    = (int16)CAEAudioUtility::GetRandomNumberInRange(0x24, 0x27);
                break;
            }
            break;
        }
        case AE_PED_HIT_GROUND: { // 0x4E1EEF
            switch (weapon.m_Type) {
            case WEAPON_KNIFE:
            case WEAPON_KATANA:
                sfxB    = 0x51;
                volumeB = -9.0f;
                skipA   = true;
                break;
            case WEAPON_BASEBALLBAT:
                volumeA = -10.0f;
                sfxB    = 0x52;
                volumeB = -6.0f;
                break;
            case WEAPON_CHAINSAW:
                if (!CheckChainsawBank()) {
                    return;
                }
                break;
            case WEAPON_FLOWERS:
                sfxB = -1;
                break;
            default:
                volumeA = -15.0f;
                sfxB    = 0x52;
                volumeB = -6.0f;
                break;
            }
            break;
        }
        case AE_PED_HIT_GROUND_KICK: // 0x4E1F77
            sfxB    = 0x52;
            volumeB = -6.0f;
            skipA   = true;
            break;
        case AE_PED_HIT_HIGH_UNARMED:
        case AE_PED_HIT_MARTIAL_PUNCH: // 0x4E1F89
            sfxA = 0x3A;
            sfxB = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
            break;
        case AE_PED_HIT_LOW_UNARMED: // 0x4E1FA0
            sfxA = 0x3A;
            sfxB = (int16)CAEAudioUtility::GetRandomNumberInRange(0x24, 0x27);
            break;
        case AE_PED_HIT_MARTIAL_KICK: // 0x4E1FB7
            sfxB  = (int16)CAEAudioUtility::GetRandomNumberInRange(0x3B, 0x3E);
            skipA = true;
            break;
        default: // 0x4E1E59
            sfxB = -1;
            break;
        }
    } else { // 0x4E1FCA
        sfxB = -1;
        switch (event) {
        case AE_PED_HIT_HIGH:
        case AE_PED_HIT_LOW:
        case AE_PED_HIT_GROUND: // 0x4E1FE0
            volumeA = GetSurfaceHitVolume(surfaceId);
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, (eSurfaceType)surfaceId, m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            break;
        case AE_PED_HIT_GROUND_KICK:
        case AE_PED_HIT_MARTIAL_KICK: // 0x4E202B
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, (eSurfaceType)surfaceId, m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            return;
        case AE_PED_HIT_HIGH_UNARMED:
        case AE_PED_HIT_LOW_UNARMED:
        case AE_PED_HIT_MARTIAL_PUNCH: // 0x4E206E
            sfxA    = 0x3A;
            volumeA = GetSurfaceHitVolume(surfaceId);
            AudioEngine.ReportCollision(m_pPed, physical, SURFACE_PED, (eSurfaceType)surfaceId, m_pPed->GetPosition(), nullptr, 1.0f, 1.0f, true, false);
            break;
        default:
            break;
        }
    }

    // Plays one of the sounds (Both are almost identical)
    const auto PlayHitSound = [&](eSoundBankSlot slot, int16 sfx, float volumeOffset, float freqVariance) {
        m_tempSound.Initialise(
            slot,
            (eSoundID)sfx,
            this,
            m_pPed->GetPosition(),
            baseVolume + volumeOffset,
            1.0f,
            1.0f,
            1.0f,
            0,
            SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            freqVariance,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        if (maxVol > 0u) {
            m_tempSound.m_SpeedVariance  = 0.0f;
            m_tempSound.m_Speed          = 0.0f;
            m_tempSound.m_ClientVariable = (float)(CTimer::GetTimeInMS() + maxVol);
            m_tempSound.m_Event          = event;
            m_tempSound.SetFlags(SOUND_REQUEST_UPDATES, true);
        }
        AESoundManager.RequestNewSound(&m_tempSound);
    };

    // 0x4E1EC0
    if (!skipA && sfxA >= 0) {
        PlayHitSound(SND_BANK_SLOT_WEAPON_GEN, sfxA, volumeA, variance);
    }

    // 0x4E2164
    if (sfxB >= 0) {
        PlayHitSound(slotB, sfxB, volumeB, 0.0588f);
    }
}

// 0x4E2350
void CAEPedAudioEntity::HandlePedJacked(eAudioEvents event) {
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN)) {
        if (!AudioEngine.IsLoadingTuneActive()) {
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_WEAPONS, SND_BANK_SLOT_WEAPON_GEN);
        }
        return;
    }

    const auto volume = GetDefaultVolume(event);
    const auto now    = CTimer::GetTimeInMS();

    int16  sfxA   = 0; // Not set for some events!
    int16  sfxB   = -1;
    uint32 delayA = 0; // Delay of `sfxA` / `sfxB`
    uint32 delayC = 0; // Delay of the last sound (bank `0`, slot `SND_BANK_SLOT_FOOTSTEPS_GENERIC`)

    switch (event) {
    case AE_PED_JACKED_CAR_PUNCH:
        sfxA   = 0x3A;
        sfxB   = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
        delayA = 500;
        delayC = 0x855;
        break;
    case AE_PED_JACKED_CAR_HEAD_BANG:
        sfxA   = 0x32;
        sfxB   = (int16)CAEAudioUtility::GetRandomNumberInRange(0x24, 0x27);
        delayA = 0x3A5;
        delayC = 0xA49;
        break;
    case AE_PED_JACKED_CAR_KICK:
        sfxB   = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
        delayA = 0x384;
        delayC = 0xAF0;
        break;
    case AE_PED_JACKED_BIKE:
        sfxA   = 0x3A;
        sfxB   = (int16)CAEAudioUtility::GetRandomNumberInRange(0x28, 0x2B);
        delayA = 0x21;
        delayC = 0x16E;
        break;
    case AE_PED_JACKED_DOZER:
        sfxB   = (int16)CAEAudioUtility::GetRandomNumberInRange(0x24, 0x27);
        delayA = 0x10A;
        delayC = 0x341;
        break;
    default:
        delayA = 0;
        delayC = 0;
        break;
    }

    // Plays a delayed sound (Speed is set to 0 until the time has passed, see `UpdateParameters`)
    const auto PlayDelayedSound = [&](eSoundBankSlot slot, int16 sfx, uint32 delay) {
        m_tempSound.Initialise(
            slot,
            (eSoundID)sfx,
            this,
            m_pPed->GetPosition(),
            volume,
            2.0f,
            0.0f,
            1.0f,
            0,
            SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            0.0f,
            0
        );
        m_tempSound.RegisterWithPhysicalEntity(m_pPed);
        m_tempSound.m_Event          = event;
        m_tempSound.m_ClientVariable = (float)(now + delay);
        AESoundManager.RequestNewSound(&m_tempSound);
    };

    switch (event) {
    case AE_PED_JACKED_CAR_PUNCH:
    case AE_PED_JACKED_CAR_HEAD_BANG:
    case AE_PED_JACKED_BIKE:
        PlayDelayedSound(SND_BANK_SLOT_WEAPON_GEN, sfxA, delayA); // 0x4E24B7
        [[fallthrough]];
    case AE_PED_JACKED_CAR_KICK:
    case AE_PED_JACKED_DOZER: // 0x4E252B
        if (sfxB >= 0) {
            PlayDelayedSound(SND_BANK_SLOT_WEAPON_GEN, sfxB, delayA);
        }
        break;
    default:
        break;
    }

    // 0x4E25C0
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC)) {
        AEAudioHardware.LoadSoundBank(SND_BANK_FEET_GENERIC, SND_BANK_SLOT_FOOTSTEPS_GENERIC);
        return;
    }
    PlayDelayedSound(SND_BANK_SLOT_FOOTSTEPS_GENERIC, 0, delayC);
}

// 0x4E26A0
void CAEPedAudioEntity::HandleSwimSplash(eAudioEvents event) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_SWIMMING, SND_BANK_SLOT_SWIMMING)) {
        return;
    }
    m_nSfxId = std::max(0, m_nSfxId + 1);
    AESoundManager.PlaySound({
        .BankSlotID         = SND_BANK_SLOT_SWIMMING,
        .SoundID            = (eSoundID)(m_nSfxId),
        .AudioEntity        = this,
        .Pos                = m_pPed->GetPosition(),
        .Volume             = GetDefaultVolume(event),
        .Flags              = SOUND_PLAY_PHYSICALLY | SOUND_START_PERCENTAGE | SOUND_IS_DUCKABLE,
        .FrequencyVariance  = 0.0588f,
        .RegisterWithEntity = m_pPed,
    });
}

// 0x4E2790
void CAEPedAudioEntity::HandleSwimWake(eAudioEvents event) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS, true)) {
        return;
    }
    if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical(event, this, m_pPed)) {
        AESoundManager.PlaySound({
            .BankSlotID         = SND_BANK_SLOT_COLLISIONS,
            .SoundID            = 3,
            .AudioEntity        = this,
            .Pos                = m_pPed->GetPosition(),
            .Volume             = GetDefaultVolume(event) - 20.0f,
            .RollOffFactor      = 1.f,
            .Speed              = 0.75f,
            .Doppler            = 1.f,
            .FrameDelay         = 0,
            .Flags              = SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            .RegisterWithEntity = m_pPed,
            .EventID            = event,
        });
    }
    m_LastSwimWakeTriggerTimeMs = CTimer::GetTimeInMS();
}

// 0x4E2A90
void CAEPedAudioEntity::PlayShirtFlap(float volume, float speed) {
    if (m_sTwinLoopSoundEntity.IsActive()) {
        m_sTwinLoopSoundEntity.UpdateTwinLoopSound(m_pPed->GetPosition(), volume, speed);
    } else {
        m_sTwinLoopSoundEntity.Initialise(SND_BANK_SLOT_WEAPON_GEN, 19, 20, this, 200, 1000, -1, -1);
        m_sTwinLoopSoundEntity.PlayTwinLoopSound(m_pPed->GetPosition(), volume, speed, 2.0f, 1.0f, SOUND_DEFAULT);
    }
}

// 0x4E2EE0
void CAEPedAudioEntity::Service() {
    constexpr float WIND_VOL_SCALE_VEHICLE = 1.2f - 0.8f; // 0xB613BC (Initialised at runtime by 0x84C840)
    constexpr float WIND_VOL_SCALE_PED     = 1.6f - 0.8f; // 0xB613C0 (Initialised at runtime by 0x84C860)
    constexpr float WIND_SPEED_RANGE       = 1.0f - 0.5f; // 0xB613CC (Initialised at runtime by 0x84C8C0)

    float volume                = -100.0f;
    float speed                 = 1.0f;
    bool  isPedWindEnabled      = true;
    bool  isUsingParachute      = false;
    float skydiveAccelBlend     = 0.0f;
    float paraDecelBlend        = 0.0f;

    if (!m_pPed->IsPlayer()) {
        return;
    }

    if ((m_pPed->m_pAttachedTo && m_pPed->m_pAttachedTo->GetIsTypeVehicle()) || m_pPed->bInVehicle) {
        isPedWindEnabled = false;
    }

    // Wind noise while driving an open-top car/bike
    if (m_pPed->m_nPedState == PEDSTATE_DRIVING) {
        if (const auto veh = m_pPed->m_pVehicle) {
            if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE || (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && veh->IsOpenTopCar())) {
                float vehSpeed = veh->m_vecMoveSpeed.Magnitude();
                if (vehSpeed < 0.0f) { // NOTSA: Can't ever happen
                    vehSpeed = -vehSpeed;
                }
                if (vehSpeed > 0.0f) {
                    const auto baseVolume = veh->m_vehicleAudio.m_AuSettings.IsBMX()
                        ? -17.0f
                        : -10.0f;
                    speed  = std::clamp(vehSpeed, 0.0f, 1.0f);
                    volume = CAEAudioUtility::AudioLog10(speed) * 20.0f + baseVolume;
                    speed  = WIND_VOL_SCALE_VEHICLE * speed + 0.8f;
                }
            }
        }
    }

    if (m_pPed->GetIntelligence()) {
        isUsingParachute = m_pPed->GetIntelligence()->GetUsingParachute();
    }

    if (isPedWindEnabled) {
        speed = m_pPed->m_vecMoveSpeed.Magnitude() / 0.7f;

        float baseVolume;
        if (!isUsingParachute) {
            baseVolume = -16.0f;
            if (speed >= 0.5f) {
                speed = std::clamp((speed - 0.5f) / WIND_SPEED_RANGE, 0.0f, 1.0f);
            } else {
                speed = 0.0f;
            }
        } else {
            baseVolume = -7.0f;
            if (const auto anim = RpAnimBlendClumpGetAssociation(m_pPed->GetRpClump(), "FALL_SkyDive_accel")) {
                skydiveAccelBlend = std::clamp(anim->m_BlendAmount, 0.0f, 1.0f);
            }
            if (const auto anim = RpAnimBlendClumpGetAssociation(m_pPed->GetRpClump(), "Para_decel")) {
                paraDecelBlend = std::clamp(anim->m_BlendAmount, 0.0f, 1.0f);
            }
            speed = std::clamp(speed, 0.0f, 1.0f);
        }
        volume = CAEAudioUtility::AudioLog10(speed) * 20.0f + baseVolume;
        speed  = WIND_VOL_SCALE_PED * speed + 0.8f;
    }

    speed = (0.5f * skydiveAccelBlend + 1.0f) * speed;

    const float targetVolume = (4.0f * skydiveAccelBlend + volume) + (-6.0f * paraDecelBlend);

    // Smoothly move towards the target volume
    if (targetVolume > field_158) {
        field_158 = field_158 + 5.0f < targetVolume
            ? field_158 + 5.0f
            : targetVolume;
    } else {
        field_158 = field_158 - 2.0f > targetVolume
            ? field_158 - 2.0f
            : targetVolume;
    }

    // Jump straight to the target if it's very quiet
    if (targetVolume < -20.0f && field_158 < -20.0f) {
        field_158 = targetVolume;
    }

    if (field_158 > -100.0f) {
        PlayShirtFlap(field_158, speed);
    } else {
        m_sTwinLoopSoundEntity.StopSoundAndForget();
    }
}
