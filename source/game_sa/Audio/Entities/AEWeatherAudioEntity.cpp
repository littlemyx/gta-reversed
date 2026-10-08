#include "StdInc.h"

#include <common.h>

#include "AEWeatherAudioEntity.h"
#include "AEAudioHardware.h"
#include "AEAudioUtility.h"

enum class eWeatherEvent {
    THUNDER        = 1,
    THUNDER_DIRECT = 2,
    CITY_NOISE     = 3,
    UNK_4          = 4,
    UNK_5          = 5,
};


constexpr CVector DEFAULT_POS = { -0.906f, 0.f, 0.423f };

auto& m_snLastRainDropSoundID = StaticRef<int32>(0x8CC310); // TODO: Use `eSoundID`
auto& m_sRainSoundL = StaticRef<CAETwinLoopSoundEntity>(0xB6BB18);  // dunno about names
auto& m_sRainSoundR = StaticRef<CAETwinLoopSoundEntity>(0xB6BBC0);

// 0x72A620
CAEWeatherAudioEntity::CAEWeatherAudioEntity() : CAEAudioEntity() {
    m_nThunderFrequencyVariationCounter = 0;
}

// 0x5B9A70
void CAEWeatherAudioEntity::StaticInitialise() {
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_RAIN, SND_BANK_SLOT_WEATHER);
}

// 0x5052B0
void CAEWeatherAudioEntity::StaticReset() {
    m_sfRainVolume = -100.0f;

    if (m_sRainSoundL.IsActive()) {
        m_sRainSoundL.StopSoundAndForget();
    }

    if (m_sRainSoundR.IsActive()) {
        m_sRainSoundR.StopSoundAndForget();
    }
}

// 0x506800
void CAEWeatherAudioEntity::AddAudioEvent(eAudioEvents event) {
    constexpr float THUNDER_FREQUENCIES[] = { 1.15f, 1.f, 0.85f }; // 0x8CC300

    if (event != AE_THUNDER || !CGame::CanSeeOutSideFromCurrArea() || CCullZones::PlayerNoRain() || CCullZones::CamNoRain()) {
        return;
    }
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_EXPLOSIONS, SND_BANK_SLOT_EXPLOSIONS)) {
        return;
    }

    const auto volume = CAEAudioUtility::AudioLog10(CWeather::LightningDuration * 0.0375f + 0.25f) * 20.f + GetDefaultVolume(AE_THUNDER);
    m_nThunderFrequencyVariationCounter = (uint8)((m_nThunderFrequencyVariationCounter + 1) % std::size(THUNDER_FREQUENCIES));
    const auto freq = THUNDER_FREQUENCIES[m_nThunderFrequencyVariationCounter];

    const auto PlayThunderSound = [&](eSoundID sfx, float posX, float soundVolume, float speed, uint32 flags, eWeatherEvent soundEvent) {
        AESoundManager.PlaySound({
            .BankSlotID  = SND_BANK_SLOT_EXPLOSIONS,
            .SoundID     = sfx,
            .AudioEntity = this,
            .Pos         = CVector{ posX, 0.423f, 0.f },
            .Volume      = soundVolume,
            .Speed       = speed,
            .Flags       = flags,
            .EventID     = +soundEvent,
        });
    };
    PlayThunderSound(SND_GENRL_EXPLOSIONS_NEAR_L, -0.906f, -100.f, freq * 0.35636002f, SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES, eWeatherEvent::THUNDER);
    PlayThunderSound(SND_GENRL_EXPLOSIONS_NEAR_L, 0.906f, -100.f, freq * 0.4f, SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES, eWeatherEvent::THUNDER);
    PlayThunderSound(SND_GENRL_EXPLOSIONS_DISTANT_L, -0.906f, std::min(volume, 0.f), freq * 0.35636002f, SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_ROLLED_OFF, eWeatherEvent::THUNDER_DIRECT);
    PlayThunderSound(SND_GENRL_EXPLOSIONS_DISTANT_L, 0.906f, std::min(volume, 0.f), freq * 0.4f, SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_ROLLED_OFF, eWeatherEvent::THUNDER_DIRECT);
}

namespace {
//! 0x59C910 - `CVector::Normalise`, the squared length is accumulated at extended precision, in this exact term order
void NormaliseX87(CVector& v) {
    const double len2 = ((double)v.x * (double)v.x + (double)v.y * (double)v.y) + (double)v.z * (double)v.z;
    if (len2 <= 0.0) { // `!(len2 > 0)` but without NaN
        v.x = 1.0f; // NOTE: Only X is set by the original
        return;
    }
    const double inv = 1.0 / std::sqrt(len2);
    v.x = (float)(inv * (double)v.x);
    v.y = (float)(inv * (double)v.y);
    v.z = (float)(inv * (double)v.z);
}

//! 0x4082C0 - `CVector::Magnitude`, the result is returned at extended precision (never rounded to float)
double MagnitudeX87(const CVector& v) {
    return std::sqrt(((double)v.x * (double)v.x + (double)v.y * (double)v.y) + (double)v.z * (double)v.z);
}
}

// 0x505A00
void CAEWeatherAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    if (curPlayPos <= 0) {
        return;
    }

    // Shared by the "outside" check failing in `CITY_NOISE` and `UNK_4/5` (0x5067B0)
    const auto FadeSoundOut = [sound] {
        const float vol = sound->GetVolume();
        if (!(vol > -50.f)) {
            sound->StopSoundAndForget();
            return;
        }
        const double v = (double)vol - (double)0.6f;
        sound->SetVolume(v > -50.0 ? (float)v : -50.f);
    };

    switch (sound->m_Event) {
    case +eWeatherEvent::THUNDER: { // 0x505A85
        const uint32 duration = CWeather::LightningDuration;
        const float  reqVolume = (float)(std::log10((double)duration * (double)0.0375f + 0.25) * 20.0 + (double)GetDefaultVolume(AE_THUNDER));

        const uint32 pos       = (uint32)(int32)curPlayPos; // Sign extended
        const uint32 reqPlayPos = 600u - 500u * (duration / 20u);
        if (pos <= reqPlayPos) { // 0x505AED
            sound->SetVolume((float)(std::log10((double)(int32)pos / (double)reqPlayPos) * 20.0 + (double)reqVolume));
        } else if (pos <= reqPlayPos + 200u && reqVolume > 0.f) { // 0x505B2E
            const double vol = std::pow(10.0, (double)reqVolume * (double)0.05f);
            const double t   = (double)(pos - reqPlayPos);
            sound->SetVolume((float)(std::log10((t * (1.0 - vol)) * (double)0.005f + vol) * 20.0));
        } else { // 0x505B97
            sound->SetVolume(reqVolume < 0.f ? reqVolume : 0.f);
        }
        break;
    }
    case +eWeatherEvent::CITY_NOISE: { // 0x505BE5
        if (CGame::currArea != AREA_CODE_NORMAL_WORLD) { // NOTE: Inlined `CGame::CanSeeOutSideFromCurrArea`
            FadeSoundOut();
            return;
        }

        const float baseVol = GetDefaultVolume(AE_CITY_NOISE);

        const CVector camPos = TheCamera.GetPosition();
        auto* const   player = FindPlayerPed();

        CVector pov = camPos;
        if (player) {
            pov.z = player->GetPosition().z;
        }

        CVector right = TheCamera.GetRightVector();
        NormaliseX87(right);
        const CVector right4{ right.x * 4.f, right.y * 4.f, right.z * 4.f };

        // The only difference between the sides is the direction, the static state and the line's end points
        const auto Process = [&](const CVector& origin, const CVector& lineEnd, float& residue, CEntity*& lastEntity) {
            CEntity*  hitEntity{};
            CColPoint hitCP;
            CWorld::ProcessLineOfSight(origin, lineEnd, hitCP, hitEntity, true, true, false, true, true, false, false, false);

            float  dopplerVol = 0.f;
            float  speed      = 1.f;
            double outSpeed   = 1.0; // Kept at extended precision until it's stored in the sound

            if (hitEntity && hitEntity != lastEntity && std::abs((double)TheCamera.GetPosition().z - (double)pov.z) < 6.0) { // 0x505D4A
                CVector playerVel{};
                if (player && player->bInVehicle && player->m_pVehicle) {
                    playerVel = player->m_pVehicle->GetMoveSpeed();
                }
                CVector hitVel{};
                if (hitEntity->GetIsTypePhysical()) {
                    hitVel = hitEntity->AsPhysical()->GetMoveSpeed();
                }

                CVector fwd = TheCamera.GetForwardVector();
                NormaliseX87(fwd);

                const auto dotPlayer = (float)(((double)fwd.z * (double)playerVel.z + (double)fwd.y * (double)playerVel.y) + (double)fwd.x * (double)playerVel.x);
                const auto dotHit    = (float)(((double)fwd.z * (double)hitVel.z    + (double)fwd.y * (double)hitVel.y)    + (double)fwd.x * (double)hitVel.x);

                // 0x40FE90: `scalar * vec`, 0x40FE60: `a - b`
                const CVector hitProj{ dotHit * fwd.x, dotHit * fwd.y, dotHit * fwd.z };
                const CVector playerProj{ dotPlayer * fwd.x, dotPlayer * fwd.y, dotPlayer * fwd.z };
                const CVector relVel{ playerProj.x - hitProj.x, playerProj.y - hitProj.y, playerProj.z - hitProj.z };

                const double relSpeed = MagnitudeX87(relVel);
                if (relSpeed > (double)0.35f) { // 0x505E82
                    speed = (float)((relSpeed - (double)0.35f) * (double)1.0526316f);

                    const CVector toHit{
                        (float)(((double)right.x + (double)pov.x) - (double)hitCP.m_vecPoint.x),
                        (float)(((double)pov.y + (double)right.y) - (double)hitCP.m_vecPoint.y),
                        (float)((double)(float)((double)right.z + (double)pov.z) - (double)hitCP.m_vecPoint.z) // The Z sum is rounded to float first
                    };
                    const double dist = MagnitudeX87(toHit);
                    dopplerVol = (float)(std::log10((1.0 - dist * (double)(1.f / 6.f)) * (double)speed) * 20.0 + 30.0);
                    outSpeed   = (double)speed * (double)1.75f + (double)1.75f;
                }
            }

            // 0x505F2C
            const float vol    = sound->GetVolume();
            const float target = (float)((double)baseVol + (double)dopplerVol);
            if (vol < target) {
                const double v = ((double)vol + (double)dopplerVol) + (double)0.3f;
                sound->SetVolume(v < (double)target ? (float)v : target);
                sound->SetSpeed((float)outSpeed);
                lastEntity = hitEntity;
                residue    = dopplerVol;
                return;
            }
            if (vol > target) { // 0x505F8F
                if (!(residue > 0.f)) { // 0x5060A5
                    const double v = (double)vol - (double)0.3f;
                    sound->SetVolume(v > (double)baseVol ? (float)v : baseVol);
                } else { // 0x505FB5
                    const float  limit = 1.3f < residue ? 1.3f : residue;
                    const double vmExt = (double)vol - (double)0.3f;
                    const float  vm    = (float)vmExt;
                    if ((double)baseVol - (double)limit < vmExt - (double)limit) {
                        sound->SetVolume((float)((double)vm - (double)limit));
                    } else {
                        sound->SetVolume((float)((double)baseVol - (double)limit));
                    }
                    residue = (float)((double)residue - (double)limit);
                }
            }
            sound->SetSpeed((float)outSpeed);
            lastEntity = hitEntity;
        };

        // NOTE: The original has 2 copies of this code (right: 0x505CC1, left: 0x5060D8)
        if (sound->m_CurrPos == DEFAULT_POS) {
            static auto& s_ResidueRight   = StaticRef<float>(0xB6BC70);
            static auto& s_LastEntityRight = StaticRef<CEntity*>(0xB6BC74);
            Process(
                CVector{ right.x + pov.x, pov.y + right.y, right.z + pov.z },
                CVector{ right4.x + pov.x, pov.y + right4.y, right4.z + pov.z },
                s_ResidueRight,
                s_LastEntityRight
            );
        } else {
            static auto& s_ResidueLeft    = StaticRef<float>(0xB6BC68);
            static auto& s_LastEntityLeft = StaticRef<CEntity*>(0xB6BC6C);
            Process(
                CVector{ -right.x + pov.x, pov.y + -right.y, -right.z + pov.z },
                CVector{ -right4.x + pov.x, pov.y + -right4.y, -right4.z + pov.z },
                s_ResidueLeft,
                s_LastEntityLeft
            );
        }
        break;
    }
    case +eWeatherEvent::UNK_4:
    case +eWeatherEvent::UNK_5: { // 0x506527
        static auto& sbWindOffset  = StaticRef<bool>(0x8CC2C0);
        static auto& sfWindOffset  = StaticRef<float>(0xB6BAFC);
        static auto& sfWindFreq    = StaticRef<float>(0xB6BAF8);
        static auto& sfOldFreqLeft = StaticRef<float>(0x8CC2C4);
        static auto& sWindTableA   = StaticRef<float[3][2]>(0x8CC2D0);
        static auto& sWindTableB   = StaticRef<float[3][2]>(0x8CC2E8);

        const double zFactor = (double)TheCamera.GetPosition().z * (double)0.002f;
        const float  t       = zFactor > 1.0 ? 1.f : (zFactor < 0.0 ? 0.f : (float)zFactor);

        const float a = CAEAudioUtility::GetPiecewiseLinear(t, 3, sWindTableA);
        const float b = CAEAudioUtility::GetPiecewiseLinear(t, 3, sWindTableB);

        const float wind = CWeather::WindClipped > 1.f ? 1.f : (CWeather::WindClipped < 0.f ? 0.f : CWeather::WindClipped);
        const auto ratio = (float)(((double)b - (double)a) * (double)wind + (double)a);

        if (CAEAudioUtility::ResolveProbability(0.07f)) { // 0x506642
            sbWindOffset = !sbWindOffset;
        }
        sfWindOffset = (float)((21.0 * (double)sbWindOffset) * (double)ratio);          // 0x506678
        sfWindFreq   = (float)(((double)sbWindOffset * (double)1.2f) * (double)ratio); // 0x506684

        if (CGame::currArea != AREA_CODE_NORMAL_WORLD) { // 0x506694
            FadeSoundOut();
            return;
        }

        // 0x50669A - Volume
        const float target = (float)(-33.0 + (double)sfWindOffset);
        const float vol    = sound->GetVolume();
        if (vol < target) {
            const double v = (double)vol + (double)0.6f;
            sound->SetVolume(v < (double)target ? (float)v : target);
        } else if (vol > target) {
            const double v = (double)vol - (double)0.6f;
            sound->SetVolume(v > (double)target ? (float)v : target);
        }

        // 0x5066FD - Frequency
        const float freq = sfWindFreq;
        if (sfOldFreqLeft < freq) {
            const double v = (double)sfOldFreqLeft + (double)0.1f;
            sfOldFreqLeft  = v < (double)freq ? (float)v : freq;
        } else if (sfOldFreqLeft > freq) {
            const double v = (double)sfOldFreqLeft - (double)0.1f;
            sfOldFreqLeft  = v > 1.0 ? (float)v : 1.f;
        }
        sound->SetSpeed(sfOldFreqLeft);
        break;
    }
    default: // `THUNDER_DIRECT` and anything else is ignored
        break;
    }
}

// 0x5052F0
void CAEWeatherAudioEntity::Service() {
    if (CGame::CanSeeOutSideFromCurrArea()) { // 0x50538C
        const auto EnsureSoundForEventIsPlaying = [this](eSoundID sfx, eWeatherEvent event, int16 startPlayPercentage) {
            if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
                return;
            }
            if (AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(+event, this)) {
                return;
            }
            AESoundManager.PlaySound({
                .BankSlotID  = SND_BANK_SLOT_FRONTEND_GAME,
                .SoundID     = sfx,
                .AudioEntity = this,
                .Pos         = DEFAULT_POS,
                .Volume      = -50.f,
                .Flags       = SOUND_FORCED_FRONT | SOUND_START_PERCENTAGE | SOUND_REQUEST_UPDATES | SOUND_FRONT_END,
                .PlayTime    = startPlayPercentage,
                .EventID     = +event,
            });
        };

        // 0x50539E
        EnsureSoundForEventIsPlaying(0, eWeatherEvent::CITY_NOISE, 0); 
        EnsureSoundForEventIsPlaying(0, eWeatherEvent::CITY_NOISE, 50);

        // 0x505482
        EnsureSoundForEventIsPlaying(29, eWeatherEvent::UNK_4, 0);

        // 0x505509
        EnsureSoundForEventIsPlaying(30, eWeatherEvent::UNK_5, 50);
    }

    const auto UpdateRainSounds = [this](float targetVolume) {
        m_sfRainVolume = notsa::step_to(m_sfRainVolume, targetVolume, 0.5f);

        if (targetVolume == -100.f && m_sfRainVolume <= -50.f) { // 0x50566D
            m_sfRainVolume = -100.f;
            if (m_sRainSoundL.IsActive()) {
                m_sRainSoundL.StopSoundAndForget();
            }
            if (m_sRainSoundR.IsActive()) {
                m_sRainSoundR.StopSoundAndForget();
            }
        } else {
            const auto UpdateRainTwinLoopSound = [this](CAETwinLoopSoundEntity& sound, float posX) {
                if (sound.IsActive()) {
                    sound.UpdateTwinLoopSound({posX, 0.f, 0.423f}, m_sfRainVolume, 1.f);
                    if (sound.DoSoundsSwitchThisFrame()) {
                        m_snLastRainDropSoundID = m_snLastRainDropSoundID + 1 <= 11
                            ? m_snLastRainDropSoundID + 1
                            : 2;
                        AESoundManager.PlaySound({
                            .BankSlotID = SND_BANK_SLOT_WEATHER,
                            .SoundID    = (eSoundID)(m_snLastRainDropSoundID),
                            .Pos        = CVector{ posX, CAEAudioUtility::ResolveProbability(0.5f) ? 0.423f : -0.423f, 0.f },
                            .Volume     = CAEAudioUtility::GetRandomNumberInRange(-6.f, 6.f) + m_sfRainVolume - 15.f,
                            .Flags      = SOUND_IS_CANCELLABLE | SOUND_FRONT_END
                        });
                    }
                } else if (AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_RAIN, SND_BANK_SLOT_WEATHER)) {
                    sound.Initialise(
                        SND_BANK_SLOT_WEATHER,
                        1,
                        0,
                        this,
                        65u,
                        350u
                    );
                    sound.PlayTwinLoopSound(
                        { posX, 0.423f, 0.f },
                        m_sfRainVolume,
                        1.f,
                        1.f,
                        1.f,
                        (eSoundEnvironment)(SOUND_START_PERCENTAGE | SOUND_IS_CANCELLABLE | SOUND_FRONT_END)
                    );
                }
            };
            UpdateRainTwinLoopSound(m_sRainSoundL, -0.906f);
            UpdateRainTwinLoopSound(m_sRainSoundR, 0.906f);
        }
    };
    if (CWeather::Rain <= 0.f || CCullZones::PlayerNoRain() || CCullZones::CamNoRain() || !CGame::CanSeeOutSideFromCurrArea()) { // 0x5055A3
        UpdateRainSounds(-100.0f);
    } else { // 0x5055D5
        m_sfRainVolume = std::max(m_sfRainVolume, -50.f);
        UpdateRainSounds(GetDefaultVolume(AE_RAIN) + CAEAudioUtility::AudioLog10(std::max(CWeather::Rain - 0.2f, 0.f) / 0.8f) * 20.f);
    }
}

void CAEWeatherAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAEWeatherAudioEntity, 0x872A74, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedInstall(StaticInitialise, 0x5B9A70);
    RH_ScopedInstall(StaticReset, 0x5052B0);
    RH_ScopedInstall(AddAudioEvent, 0x506800);
    RH_ScopedVMTInstall(UpdateParameters, 0x505A00);
    RH_ScopedInstall(Service, 0x5052F0);
}
