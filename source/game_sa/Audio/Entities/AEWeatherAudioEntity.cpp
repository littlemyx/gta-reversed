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


// NOTE: (x, y, z) = (-0.906, 0.423, 0), as built on the stack at 0x505A00 (compared against `sound->m_CurrPos`); `Service` (0x5052F0) builds its own 4 variants
constexpr CVector DEFAULT_POS = { -0.906f, 0.423f, 0.f };

NOTSA_GLOBAL(m_snLastRainDropSoundID, 0x8CC310, (int16), { 2 }); // TODO: Use `eSoundID`. NOTE: The original loads 32 bits (high half is always 0) but stores only 16 (`mov word ptr [0x8CC310], di`)
NOTSA_GLOBAL(m_sRainSoundL, 0xB6BB18, (CAETwinLoopSoundEntity), {}); VALIDATE_SIZE(m_sRainSoundL, 168);  // dunno about names
NOTSA_GLOBAL(m_sRainSoundR, 0xB6BBC0, (CAETwinLoopSoundEntity), {}); VALIDATE_SIZE(m_sRainSoundR, 168);

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

    // 0x5068BA: the log10 of `duration * 0.0375f + 0.25` (extended precision), * 20, + the default volume (int) and ONE rounding to float (same expression as `UpdateParameters` 0x505A85)
    const auto volume = (float)(X87R(std::log10(X87W((x87d)CWeather::LightningDuration * (x87d)0.0375f + (x87d)0.25)) * (x87d)20.0) + (x87d)GetDefaultVolume(AE_THUNDER));
    m_nThunderFrequencyVariationCounter = (uint8)((m_nThunderFrequencyVariationCounter + 1) % std::size(THUNDER_FREQUENCIES));
    const auto freq = THUNDER_FREQUENCIES[m_nThunderFrequencyVariationCounter];

    const float clampedVolume = volume < 0.f ? volume : 0.f; // 0x5069E0: `fcomp 0.0; test ah, 5; jp` - NaN and >= 0 give 0 (std::min(NaN, 0) is NaN)

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
    PlayThunderSound(SND_GENRL_EXPLOSIONS_DISTANT_L, -0.906f, clampedVolume, freq * 0.35636002f, SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_ROLLED_OFF, eWeatherEvent::THUNDER_DIRECT);
    PlayThunderSound(SND_GENRL_EXPLOSIONS_DISTANT_L, 0.906f, clampedVolume, freq * 0.4f, SOUND_FRONT_END | SOUND_IS_CANCELLABLE | SOUND_REQUEST_UPDATES | SOUND_ROLLED_OFF, eWeatherEvent::THUNDER_DIRECT);
}

namespace {
//! 0x59C910 - `CVector::Normalise`, the squared length is accumulated at extended precision, in this exact term order
void NormaliseX87(CVector& v) {
    const x87d len2 = ((x87d)v.x * (x87d)v.x + (x87d)v.y * (x87d)v.y) + (x87d)v.z * (x87d)v.z;
    if (len2 <= 0.0) { // `!(len2 > 0)` but without NaN
        v.x = 1.0f; // NOTE: Only X is set by the original
        return;
    }
    const x87d inv = (x87d)1.0 / std::sqrt(len2);
    v.x = (float)(inv * (x87d)v.x);
    v.y = (float)(inv * (x87d)v.y);
    v.z = (float)(inv * (x87d)v.z);
}

//! 0x4082C0 - `CVector::Magnitude`, the result is returned at extended precision (never rounded to float)
double MagnitudeX87(const CVector& v) {
    return std::sqrt(((x87d)v.x * (x87d)v.x + (x87d)v.y * (x87d)v.y) + (x87d)v.z * (x87d)v.z);
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
        const x87d v = (x87d)vol - (x87d)0.6f;
        sound->SetVolume(v > -50.0 ? (float)v : -50.f);
    };

    switch (sound->m_Event) {
    case +eWeatherEvent::THUNDER: { // 0x505A85
        const uint32 duration = CWeather::LightningDuration;
        const float  reqVolume = (float)(X87R(std::log10(X87W((x87d)duration * (x87d)0.0375f + (x87d)0.25)) * (x87d)20.0) + (x87d)GetDefaultVolume(AE_THUNDER));

        const uint32 pos       = (uint32)(int32)curPlayPos; // Sign extended
        const uint32 reqPlayPos = 600u - 500u * (duration / 20u);
        if (pos <= reqPlayPos) { // 0x505AED
            sound->SetVolume((float)(X87R(std::log10(X87W((x87d)(int32)pos / (x87d)reqPlayPos)) * (x87d)20.0) + (x87d)reqVolume));
        } else if (pos <= reqPlayPos + 200u && reqVolume > 0.f) { // 0x505B2E
            const x87d vol = x87::pow(10.0, (x87d)reqVolume * (x87d)0.05f);
            const x87d t   = (x87d)(pos - reqPlayPos);
            sound->SetVolume((float)(X87R(std::log10(X87W(X87R((t * (X87R((x87d)1.0 - vol))) * (x87d)0.005f + vol))) * 20.0)));
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
            x87d outSpeed   = (x87d)1.0; // Kept at extended precision until it's stored in the sound

            if (hitEntity && hitEntity != lastEntity && std::abs((x87d)TheCamera.GetPosition().z - (x87d)pov.z) < 6.0) { // 0x505D4A
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

                const auto dotPlayer = (float)(((x87d)fwd.z * (x87d)playerVel.z + (x87d)fwd.y * (x87d)playerVel.y) + (x87d)fwd.x * (x87d)playerVel.x);
                const auto dotHit    = (float)(((x87d)fwd.z * (x87d)hitVel.z    + (x87d)fwd.y * (x87d)hitVel.y)    + (x87d)fwd.x * (x87d)hitVel.x);

                // 0x40FE90: `scalar * vec`, 0x40FE60: `a - b`
                const CVector hitProj{ dotHit * fwd.x, dotHit * fwd.y, dotHit * fwd.z };
                const CVector playerProj{ dotPlayer * fwd.x, dotPlayer * fwd.y, dotPlayer * fwd.z };
                const CVector relVel{ playerProj.x - hitProj.x, playerProj.y - hitProj.y, playerProj.z - hitProj.z };

                const double relSpeed = MagnitudeX87(relVel);
                if (relSpeed > (double)0.35f) { // 0x505E82
                    speed = (float)((X87R(relSpeed - (double)0.35f)) * (x87d)1.0526316f);

                    const CVector toHit{
                        (float)(((x87d)right.x + (x87d)pov.x) - (x87d)hitCP.m_vecPoint.x),
                        (float)(((x87d)pov.y + (x87d)right.y) - (x87d)hitCP.m_vecPoint.y),
                        (float)((x87d)(float)((x87d)right.z + (x87d)pov.z) - (x87d)hitCP.m_vecPoint.z) // The Z sum is rounded to float first
                    };
                    const double dist = MagnitudeX87(toHit);
                    dopplerVol = (float)(X87R(std::log10(X87W(((x87d)1.0 - X87R(dist * (double)ExeRecip(6.f))) * (x87d)speed)) * 20.0) + (x87d)30.0);
                    outSpeed   = (x87d)speed * (x87d)1.75f + (x87d)1.75f;
                }
            }

            // 0x505F2C
            const float vol    = sound->GetVolume();
            const float target = (float)((x87d)baseVol + (x87d)dopplerVol);
            if (vol < target) {
                const x87d v = ((x87d)vol + (x87d)dopplerVol) + (x87d)0.3f;
                sound->SetVolume(v < (double)target ? (float)v : target);
                sound->SetSpeed((float)outSpeed);
                lastEntity = hitEntity;
                residue    = dopplerVol;
                return;
            }
            if (vol > target) { // 0x505F8F
                if (!(residue > 0.f)) { // 0x5060A5
                    const x87d v = (x87d)vol - (x87d)0.3f;
                    sound->SetVolume(v > (double)baseVol ? (float)v : baseVol);
                } else { // 0x505FB5
                    const float  limit = 1.3f < residue ? 1.3f : residue;
                    const x87d vmExt = (x87d)vol - (x87d)0.3f;
                    const float  vm    = (float)vmExt;
                    if ((x87d)baseVol - (x87d)limit < vmExt - (x87d)limit) {
                        sound->SetVolume((float)((x87d)vm - (x87d)limit));
                    } else {
                        sound->SetVolume((float)((x87d)baseVol - (x87d)limit));
                    }
                    residue = (float)((x87d)residue - (x87d)limit);
                }
            }
            sound->SetSpeed((float)outSpeed);
            lastEntity = hitEntity;
        };

        // NOTE: The original has 2 copies of this code (right: 0x505CC1, left: 0x5060D8)
        if (sound->m_CurrPos == DEFAULT_POS) {
            NOTSA_GLOBAL_LOCAL(s_ResidueRight, 0xB6BC70, (float), {});
            NOTSA_GLOBAL_LOCAL(s_LastEntityRight, 0xB6BC74, (CEntity*), {});
            Process(
                CVector{ right.x + pov.x, pov.y + right.y, right.z + pov.z },
                CVector{ right4.x + pov.x, pov.y + right4.y, right4.z + pov.z },
                s_ResidueRight,
                s_LastEntityRight
            );
        } else {
            NOTSA_GLOBAL_LOCAL(s_ResidueLeft, 0xB6BC68, (float), {});
            NOTSA_GLOBAL_LOCAL(s_LastEntityLeft, 0xB6BC6C, (CEntity*), {});
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
        NOTSA_GLOBAL_LOCAL(sbWindOffset, 0x8CC2C0, (bool), { true });
        NOTSA_GLOBAL_LOCAL(sfWindOffset, 0xB6BAFC, (float), {});
        NOTSA_GLOBAL_LOCAL(sfWindFreq, 0xB6BAF8, (float), {});
        NOTSA_GLOBAL_LOCAL(sfOldFreqLeft, 0x8CC2C4, (float), { 1.0f });
        NOTSA_GLOBAL_LOCAL(sWindTableA, 0x8CC2D0, (float[3][2]), { {}, { 0.3f, 0.4f }, { 1.0001f, 0.5f } });
        NOTSA_GLOBAL_LOCAL(sWindTableB, 0x8CC2E8, (float[3][2]), { { 0.0f, 0.5f }, { 0.3f, 1.0f }, { 1.0001f, 1.0f } });

        const x87d zFactor = (x87d)TheCamera.GetPosition().z * (x87d)0.002f;
        const float  t       = zFactor > 1.0 ? 1.f : (zFactor < 0.0 ? 0.f : (float)zFactor);

        const float a = CAEAudioUtility::GetPiecewiseLinear(t, 3, sWindTableA);
        const float b = CAEAudioUtility::GetPiecewiseLinear(t, 3, sWindTableB);

        const float wind = CWeather::WindClipped > 1.f ? 1.f : (CWeather::WindClipped < 0.f ? 0.f : CWeather::WindClipped);
        const auto ratio = (float)(((x87d)b - (x87d)a) * (x87d)wind + (x87d)a);

        if (CAEAudioUtility::ResolveProbability(0.07f)) { // 0x506642
            sbWindOffset = !sbWindOffset;
        }
        sfWindOffset = (float)(((x87d)21.0 * (x87d)sbWindOffset) * (x87d)ratio);          // 0x506678
        sfWindFreq   = (float)(((x87d)sbWindOffset * (x87d)1.2f) * (x87d)ratio); // 0x506684

        if (CGame::currArea != AREA_CODE_NORMAL_WORLD) { // 0x506694
            FadeSoundOut();
            return;
        }

        // 0x50669A - Volume
        const float target = (float)(-(x87d)33.0 + (x87d)sfWindOffset);
        const float vol    = sound->GetVolume();
        if (vol < target) {
            const x87d v = (x87d)vol + (x87d)0.6f;
            sound->SetVolume(v < (double)target ? (float)v : target);
        } else if (vol > target) {
            const x87d v = (x87d)vol - (x87d)0.6f;
            sound->SetVolume(v > (double)target ? (float)v : target);
        }

        // 0x5066FD - Frequency
        const float freq = sfWindFreq;
        if (sfOldFreqLeft < freq) {
            const x87d v = (x87d)sfOldFreqLeft + (x87d)0.1f;
            sfOldFreqLeft  = v < (double)freq ? (float)v : freq;
        } else if (sfOldFreqLeft > freq) {
            const x87d v = (x87d)sfOldFreqLeft - (x87d)0.1f;
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
    // The 4 positions the original builds on its stack: (+-0.906, +-0.423, 0)
    static_assert(std::bit_cast<uint32>(0.906f) == 0x3F67EF9E && std::bit_cast<uint32>(0.423f) == 0x3ED89375);
    constexpr CVector POS_L_FRONT{ -0.906f,  0.423f, 0.f }; // [esp+0x20]
    constexpr CVector POS_R_FRONT{  0.906f,  0.423f, 0.f }; // [esp+0x14]
    constexpr CVector POS_L_BACK { -0.906f, -0.423f, 0.f }; // [esp+0x38]
    constexpr CVector POS_R_BACK {  0.906f, -0.423f, 0.f }; // [esp+0x2C]

    if (CGame::CanSeeOutSideFromCurrArea()) { // 0x50538C
        // Each block: [gate on event] -> [bank loaded ? play sound(s) : request bank load]
        const auto PlayEventSound = [this](eSoundID sfx, const CVector& pos, uint16 flags, int16 playTime, eWeatherEvent event) {
            AESoundManager.PlaySound({
                .BankSlotID  = SND_BANK_SLOT_FRONTEND_GAME,
                .SoundID     = sfx,
                .AudioEntity = this,
                .Pos         = pos,
                .Volume      = -50.f,
                .Flags       = flags,
                .PlayTime    = playTime,
                .EventID     = +event,
            });
        };

        // 0x50539E: City noise - a pair of sounds under a single gate
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(+eWeatherEvent::CITY_NOISE, this)) {
            if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
                PlayEventSound(0, POS_L_FRONT, SOUND_FORCED_FRONT | SOUND_REQUEST_UPDATES | SOUND_FRONT_END, 0, eWeatherEvent::CITY_NOISE);
                PlayEventSound(0, POS_R_FRONT, SOUND_FORCED_FRONT | SOUND_START_PERCENTAGE | SOUND_REQUEST_UPDATES | SOUND_FRONT_END, 50, eWeatherEvent::CITY_NOISE);
            } else {
                AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME);
            }
        }

        // 0x505482: Wind (left)
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(+eWeatherEvent::UNK_4, this)) {
            if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
                PlayEventSound(29, POS_L_FRONT, SOUND_FORCED_FRONT | SOUND_REQUEST_UPDATES | SOUND_FRONT_END, 0, eWeatherEvent::UNK_4);
            } else {
                AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME);
            }
        }

        // 0x505509: Wind (right)
        if (!AESoundManager.AreSoundsOfThisEventPlayingForThisEntity(+eWeatherEvent::UNK_5, this)) {
            if (AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME)) {
                PlayEventSound(30, POS_R_FRONT, SOUND_FORCED_FRONT | SOUND_START_PERCENTAGE | SOUND_REQUEST_UPDATES | SOUND_FRONT_END, 50, eWeatherEvent::UNK_5);
            } else {
                AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FRONTEND_GAME);
            }
        }
    }

    // 0x505592: Rain
    // Returns false if the rest of the function must be skipped (original jumps to the epilogue)
    const auto UpdateRainTwinLoopSound = [&](CAETwinLoopSoundEntity& sound, bool isLeft) -> bool {
        const auto& posFront = isLeft ? POS_L_FRONT : POS_R_FRONT;
        const auto& posBack  = isLeft ? POS_L_BACK  : POS_R_BACK;
        if (sound.IsActive()) {
            sound.UpdateTwinLoopSound(posFront, m_sfRainVolume, 1.f);
            if (sound.DoSoundsSwitchThisFrame()) {
                m_snLastRainDropSoundID = (int16)(m_snLastRainDropSoundID + 1) <= 11
                    ? (int16)(m_snLastRainDropSoundID + 1)
                    : (int16)2;
                // NOTE: The original calls the random number generator BEFORE `ResolveProbability`
                // and computes `(vol - 15) + rand` with `vol - 15` in extended precision.
                const auto rnd = CAEAudioUtility::GetRandomNumberInRange(-6.f, 6.f);
                const auto vol = (float)(((x87d)m_sfRainVolume - (x87d)15.0) + (x87d)rnd);
                AESoundManager.PlaySound({
                    .BankSlotID = SND_BANK_SLOT_WEATHER,
                    .SoundID    = (eSoundID)(m_snLastRainDropSoundID),
                    .Pos        = CAEAudioUtility::ResolveProbability(0.5f) ? posFront : posBack,
                    .Volume     = vol,
                    .Flags      = SOUND_IS_CANCELLABLE | SOUND_FRONT_END, // Original: init with 0, then `m_Flags = 3`
                });
            }
            return true;
        }
        if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_RAIN, SND_BANK_SLOT_WEATHER)) { // 0x505701 / 0x505862
            AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_RAIN, SND_BANK_SLOT_WEATHER);
            return false; // Jumps to the function end (the right sound isn't handled this frame if the left one fails)
        }
        if (isLeft) { // 0x50571E
            sound.Initialise(SND_BANK_SLOT_WEATHER, 1, 0, this, 65u, 350u, 0, 0);
        } else { // 0x50588E
            sound.Initialise(SND_BANK_SLOT_WEATHER, 0, 1, this, 65u, 350u, 50, 50);
        }
        sound.PlayTwinLoopSound(
            posFront,
            m_sfRainVolume,
            1.f,
            1.f,
            1.f,
            (eSoundEnvironment)(SOUND_START_PERCENTAGE | SOUND_IS_CANCELLABLE | SOUND_FRONT_END)
        );
        return true;
    };

    // The target volume is kept in extended precision by the original until it's compared / stored, so use doubles
    x87d targetVolume;
    if (CWeather::Rain <= 0.f || CCullZones::PlayerNoRain() || CCullZones::CamNoRain() || !CGame::CanSeeOutSideFromCurrArea()) { // 0x5055A3
        targetVolume = -(x87d)100.0;
    } else { // 0x5055D5
        if (!(m_sfRainVolume > -50.f)) {
            m_sfRainVolume = -50.f;
        }
        x87d x = (x87d)CWeather::Rain - (x87d)0.2f;
        if (!(x > 0.0)) {
            x = (x87d)0.0;
        }
        x *= (x87d)1.25; // 0x8595F0 (NOT a division by 0.8)
        // NOTE: Raw `fyl2x`, NOT `AudioLog10` (no clamp, log10(0) = -inf)
        targetVolume = X87R(std::log10(X87W(x)) * (x87d)20.0) + (x87d)GetDefaultVolume(AE_RAIN); // 0x505608
    }

    // 0x505634: step `m_sfRainVolume` towards the target by 0.5
    const x87d cur = m_sfRainVolume;
    if (targetVolume < cur) {
        const x87d dec = cur - (x87d)0.5;
        m_sfRainVolume = dec > targetVolume ? (float)dec : (float)targetVolume;
    } else if (targetVolume > cur) {
        const x87d inc = cur + (x87d)0.5;
        m_sfRainVolume = inc < targetVolume ? (float)inc : (float)targetVolume;
    } // else: equal or unordered => unchanged

    if (targetVolume == -100.0 && m_sfRainVolume <= -50.f) { // 0x50566D
        m_sfRainVolume = -100.f;
        if (m_sRainSoundL.IsActive()) {
            m_sRainSoundL.StopSoundAndForget();
        }
        if (m_sRainSoundR.IsActive()) {
            m_sRainSoundR.StopSoundAndForget();
        }
        return;
    }
    if (UpdateRainTwinLoopSound(m_sRainSoundL, true)) {
        UpdateRainTwinLoopSound(m_sRainSoundR, false);
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
