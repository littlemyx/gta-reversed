#include "StdInc.h"

#include <extensions/utility.hpp>
#include <reversiblebugfixes/Bugs.hpp>

#include "AECollisionAudioEntity.h"
#include "AEAudioHardware.h"
#include "AEAudioUtility.h"
#include "AEAudioStats.h"

#include "AECollisionAudioEntity.CollisionLookup.h"

void CAECollisionAudioEntity::InjectHooks() {
    RH_ScopedVirtualClass(CAECollisionAudioEntity, 0x862E64, 1);
    RH_ScopedCategory("Audio/Entities");

    RH_ScopedVMTInstall(UpdateParameters, 0x4DA370);

    RH_ScopedInstall(Initialise, 0x5B9BD0);
    RH_ScopedInstall(InitialisePostLoading, 0x4DA050);
    RH_ScopedInstall(AddCollisionSoundToList, 0x4DAAC0);
    RH_ScopedInstall(Reset, 0x4DA320);
    RH_ScopedInstall(ReportGlassCollisionEvent, 0x4DA070);
    RH_ScopedInstall(UpdateLoopingCollisionSound, 0x4DA540);
    RH_ScopedInstall(GetCollisionSoundStatus, 0x4DA830);
    RH_ScopedInstall(ReportObjectDestruction, 0x4DAB60);
    RH_ScopedInstall(PlayOneShotCollisionSound, 0x4DB150);
    RH_ScopedInstall(PlayLoopingCollisionSound, 0x4DB450);
    RH_ScopedInstall(PlayBulletHitCollisionSound, 0x4DB7C0);
    RH_ScopedInstall(ReportCollision, 0x4DBA10);
    RH_ScopedInstall(ReportBulletHit, 0x4DBDF0);
    RH_ScopedInstall(Service, 0x4DA2C0);
    RH_ScopedInstall(ChooseCollisionSoundID, 0x4DAA50);
    RH_ScopedOverloadedInstall(ReportWaterSplash, "at-position", 0x4DA190, void(CAECollisionAudioEntity::*)(CVector, float));
    RH_ScopedOverloadedInstall(ReportWaterSplash, "for-physical", 0x4DAE40, void(CAECollisionAudioEntity::*)(CPhysical*, float, bool));
}

namespace {
//! The exe releases an entry by resetting everything except `LoopStopTimeMs` (0x4DA2FB.., 0x4DA345.., 0x4DA440..).
void ReleaseEntry(tCollisionSound& entry) {
    entry.EntityA  = nullptr;
    entry.EntityB  = nullptr;
    entry.Sound    = nullptr;
    entry.Status   = COLLISION_SOUND_INACTIVE;
    entry.SurfaceA = (eSurfaceType)TOTAL_NUM_COLLISION_SURFACE_TYPES;
    entry.SurfaceB = (eSurfaceType)TOTAL_NUM_COLLISION_SURFACE_TYPES;
}
}; // namespace

// 0x4DA370
// BUG (port): this virtual override was never ported (the DLL build kept the exe's through the vtable), so in the standalone build the base `CAEAudioEntity::UpdateParameters`
//             (empty) ran: one-shot entries of `m_CollisionSoundList` were never released -> "Collision sound list is full" after 300 collisions, no collision sound afterwards.
void CAECollisionAudioEntity::UpdateParameters(CAESound* sound, int16 curPlayPos) {
    // Jump table 0x4DA47C (byte table 0x4DA490, index = event - 1): events 2 / 3 / 128 retarget the speed of their sound once its delay has elapsed, event 1 does nothing,
    // every other event (and anything outside 1..128) only handles the end of the sound.
    const auto SetSpeedAfterDelay = [&](float speed) {
        if (curPlayPos < 0) {
            return;
        }
        if (CTimer::GetTimeInMS() < (uint32)NOTSA_FTOL(int32, sound->m_ClientVariable)) { // 0x4DA39F: `_ftol`, then unsigned `jb`
            return;
        }
        sound->m_Speed = speed;
    };
    switch (sound->m_Event) {
    case 1:   return;
    case 2:   return SetSpeedAfterDelay(1.26f);           // 0x4DA3BB: 0x3FA147AE
    case 3:   return SetSpeedAfterDelay(0.94f);           // 0x4DA3E6: 0x3F70A3D7
    case 128: return SetSpeedAfterDelay(0.56f);           // 0x4DA390: 0x3F0F5C29 (AE_GLASS_HIT_GROUND_SLOW)
    default:  break;
    }

    // 0x4DA40D - The sound ended: release its list entry. The exe does not touch `LoopStopTimeMs` (the other fields are reset).
    if (curPlayPos != -1) {
        return;
    }
    for (auto& entry : m_CollisionSoundList) {
        if (entry.Sound != sound) {
            continue;
        }
        ReleaseEntry(entry);
        m_NumActiveCollisionSounds--;
        return;
    }
}

// 0x5B9BD0
void CAECollisionAudioEntity::Initialise() {
    // The exe only touches these fields (the loop over the list stores 0 / 0 / 0 / 0 / 0 / 0xC3 / 0xC3, the history is filled with -1 (0xFF bytes), the base class is left alone). `*this = {}`
    // used the repo's member defaults instead (history = {255, 0, 0 ...}) and reset the base class members.
    m_NumActiveCollisionSounds = 0;
    for (auto& entry : m_CollisionSoundList) {
        ReleaseEntry(entry);
        entry.LoopStopTimeMs = 0;
    }
    m_CollisionSoundIDHistory.fill(-1);
    m_LastBulletHitSoundID = -1;
}

// 0x4DA050
void CAECollisionAudioEntity::InitialisePostLoading() {
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS);
    AEAudioHardware.LoadSoundBank(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_BULLET_HITS);
}

// 0x4DA320
void CAECollisionAudioEntity::Reset() {
    for (auto& entry : m_CollisionSoundList) {
        if (entry.Status != COLLISION_SOUND_LOOPING) {
            continue;
        }

        if (entry.Sound) {
            entry.Sound->StopSoundAndForget();
        }

        ReleaseEntry(entry);
        --m_NumActiveCollisionSounds; // 0x4DA358
    }
}

// 0x4DAAC0
void CAECollisionAudioEntity::AddCollisionSoundToList(
    CEntity* entityA,
    CEntity* entityB,
    eSurfaceType surfaceA,
    eSurfaceType surfaceB,
    CAESound* sound,
    eCollisionSoundStatus status
) {
    // Find an entry with no sound.
    const auto e = rng::find_if_not(m_CollisionSoundList, &tCollisionSound::Sound);
    if (e == m_CollisionSoundList.end()) {
        NOTSA_AUDIO_STAT(collFull); // NOTSA: NOTSA_AUDIO_STATS
        NOTSA_LOG_WARN("Collision sound list is full");
        return;
    }

    NOTSA_AUDIO_STAT(collAdd); // NOTSA: NOTSA_AUDIO_STATS
    e->EntityA  = entityA;
    e->EntityB  = entityB;

    e->SurfaceA = surfaceA;
    e->SurfaceB = surfaceB;

    e->Sound          = sound;
    e->Status         = status;
    e->LoopStopTimeMs = status == COLLISION_SOUND_LOOPING
        ? CTimer::GetTimeInMS() + 100
        : 0;

    m_NumActiveCollisionSounds++;
}

// 0x4DA830
eCollisionSoundStatus CAECollisionAudioEntity::GetCollisionSoundStatus(CEntity* entityA, CEntity* entityB, eSurfaceType surfaceA, eSurfaceType surfaceB, int32& outIndex) {
    auto status = COLLISION_SOUND_INACTIVE;
    for (auto&& [i, v] : rngv::enumerate(m_CollisionSoundList)) {
        if (v.EntityA == entityA && v.EntityB == entityB || v.EntityA == entityB && v.EntityB == entityA) {
            if ((status = v.Status) == COLLISION_SOUND_LOOPING) {
                outIndex = (int32)(i);
                return v.Status;
            }
        }
    }
    outIndex = (int32)(m_CollisionSoundList.size()); // 0x4DA9BF: the exe's (unrolled) loop counter, 300 - not -1 - when no entry is looping
    return status;
}

// 0x4DB150
void CAECollisionAudioEntity::PlayOneShotCollisionSound(CEntity* entityA, CEntity* entityB, eSurfaceType surfaceA, eSurfaceType surfaceB, float impulseMagnitude, const CVector& posn) {
    // The exe loops twice (0x4DB190): pass 0 plays the sound of `surfaceA` (the other surface being `surfaceB`), pass 1 the sound of `surfaceB`. Every pass registers
    // (surfaceA, surfaceB) in the list; the entities are swapped in pass 1. `impulseMagnitude` is modified in place (pool ball x5, 0x4DB224) and carried over to pass 1.
    for (int32 pass = 0; pass < 2; pass++) {
        const auto  sound      = pass == 0 ? surfaceA : surfaceB; // 0x4DB1A7 / 0x4DB1D1: `bl`
        const auto  other      = pass == 0 ? surfaceB : surfaceA; // 0x4DB1B2 / 0x4DB1CA: `[esp+0x13]`
        CEntity* const entity1 = pass == 0 ? entityA : entityB;   // 0x4DB1AE / 0x4DB1DC..: `[esp+0x20]`
        CEntity* const entity2 = pass == 0 ? entityB : entityA;   // `[esp+0x1c]`

        if (sound >= TOTAL_NUM_COLLISION_SURFACE_TYPES) { // 0x4DB1E6: `jae` -> next pass
            continue;
        }
        auto slot = SND_BANK_SLOT_COLLISIONS;
        if (sound == AE_SURFACE_TYPE_POOL_BALL) { // 0x4DB1F3
            if (other != sound) { // 0x4DB1F8: leaves the loop
                return;
            }
            slot = SND_BANK_SLOT_MISSION4;
            if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_SCRIPT_POOL_MINIGAME, slot)) { // 0x4DB210
                return;
            }
            impulseMagnitude *= 5.f; // 0x858C80
        } else {
            if (other == AE_SURFACE_TYPE_POOL_BALL) { // 0x4DB233: leaves the loop
                return;
            }
            if (sound == AE_SURFACE_TYPE_BASKETBALL || sound == AE_SURFACE_TYPE_PUNCHBAG) { // 0x4DB23B / 0x4DB249
                slot = SND_BANK_SLOT_MISSION4;
                if (!AEAudioHardware.IsSoundBankLoaded(sound == AE_SURFACE_TYPE_BASKETBALL ? SND_BANK_SCRIPT_BASKETBALL : SND_BANK_SCRIPT_GYM, slot)) {
                    return;
                }
            }
        }

        const auto soundID = ChooseCollisionSoundID(sound); // 0x4DB275
        if ((int16)soundID == -1) {
            continue;
        }

        // 0x4DB28A: `ParamD` of the OTHER surface; x 0.01f (0x858C58, no division), x10 for a BMX hitting a ped, x500 (0x858B58)
        auto offset = (float)(gCollisionLookup[other].ParamD) * impulseMagnitude * ExeRecip(100.f);
        if (sound == AE_SURFACE_TYPE_BMX && other == SURFACE_PED) {
            offset *= 10.f;
        }
        offset *= 500.f;
        // 0x4DB2C2: the exe keeps the converted value in 16 bits (`cmp ax, 100` / `movsx eax, ax` / `cmp cx, 100`): an impulse above 65 wraps (a big crash is silent, like in the exe)
        int16 progress = (int16)NOTSA_FTOL(int32, std::floor(offset));
        if (progress > 100) {
            progress = 100;
        }
        const auto startAt = (int16)(100 - progress); // 0x4DB2F6
        if (startAt >= 100) {
            continue;
        }

        auto volume = GetDefaultVolume(AE_GENERAL_COLLISION) - 3.f; // 0x4DB30D: `fild; fsub 3.0`
        const auto maxStartAt = gCollisionLookup[sound].MaxStartAtPercentage;
        auto playTime = startAt;
        if ((int32)startAt > maxStartAt) { // 0x4DB336
            // 0x4DB349: `fild (100 - startAt); fidiv (100 - max)`, raw `fyl2x` (no floor), x20 (0x858BA4), added to the volume
            volume += CAEAudioUtility::AudioLog10((float)(100 - (int32)startAt) / (float)(100 - maxStartAt)) * 20.f;
            playTime = (int16)(uint16)maxStartAt;
        }
        if (volume <= -100.f) { // 0x4DB373: `fcomp -100.0; jnp` (unordered plays)
            continue;
        }
        auto* const snd = AESoundManager.PlaySound({
            .BankSlotID        = slot,
            .SoundID           = soundID,
            .AudioEntity       = this,
            .Pos               = posn,
            .Volume            = volume,
            .RollOffFactor     = 2.f,
            .Flags             = SOUND_ROLLED_OFF | SOUND_START_PERCENTAGE | SOUND_REQUEST_UPDATES, // 0x424
            .FrequencyVariance = 0.02f,
            .PlayTime          = playTime, // 0x4DB388: `push ecx`
        });
        m_CollisionSoundIDHistory[sound] = soundID; // 0x4DB3E2
        if (snd) {
            AddCollisionSoundToList(entity1, entity2, surfaceA, surfaceB, snd, COLLISION_SOUND_ONE_SHOT);
        }
    }
}

// based on `PlayLoopingCollisionSound` (0x4DB450) and `UpdateLoopingCollisionSound` (0x4DA540). The two exe functions share the "which velocity" part but NOT the speed formula:
//   Play  (0x4DB5D7-0x4DB68A): mag == 0 -> volume -100, speed 0.75 * 0.8; else s = min(sqrt(mag * (A.ParamD * B.ParamD) * 1e-4f) * 3, 0.3)
//   Update(0x4DA6A5-0x4DA747): no mag == 0 test, no sqrt:      s = min(mag * (A.ParamD * B.ParamD) * 1e-4f * 3, 1.0)
// both: force-looping s *= 1/6; volume = default + 20 * log10(min(4/3 * s, 1)) (raw `fyl2x`, no `AudioLog10` floor); speed = max(s, 0.75) * 0.8.
std::pair<float, float> CAECollisionAudioEntity::GetLoopingCollisionSoundVolumeAndSpeed(CEntity* entityA, CEntity* entityB, eSurfaceType surfaceA, eSurfaceType surfaceB, bool isForceLooping, bool isPlay) {
    // The exe sums the squares z, y, x (0x4DA58F..: z*z + y*y + x*x)
    const auto SqZYX = [](const CVector& v) { return v.z * v.z + v.y * v.y + v.x * v.x; };
    const auto MagnitudeOfBigger = [&](const CVector& velocity, const CVector& turn) { // 0x4DA5BF: `jp` = turn when turn >= velocity or unordered
        return std::sqrt(!(SqZYX(turn) < SqZYX(velocity)) ? SqZYX(turn) : SqZYX(velocity));
    };

    float mag; // 0x4DA69F: `fld 0.0` for anything but a car colliding with a non-ped
    if (surfaceA == SURFACE_CAR && surfaceB == SURFACE_CAR) {
        const auto* const vehA = entityA->AsVehicle();
        const auto* const vehB = entityB->AsVehicle();
        mag = MagnitudeOfBigger(vehA->GetMoveSpeed() - vehB->GetMoveSpeed(), vehA->GetTurnSpeed() - vehB->GetTurnSpeed());
    } else if ((surfaceB != SURFACE_PED && surfaceA == SURFACE_CAR) || (surfaceA != SURFACE_PED && surfaceB == SURFACE_CAR)) { // 0x4DA620
        const auto* const physicalA = entityA->AsPhysical();
        mag = MagnitudeOfBigger(physicalA->GetMoveSpeed(), physicalA->GetTurnSpeed());
    } else {
        mag = 0.f;
    }

    float s;
    if (isPlay) {
        if (mag == 0.f) { // 0x4DB5D7
            return { -100.f, 0.75f * 0.8f };
        }
        s = std::sqrt(mag * (float)(gCollisionLookup[surfaceA].ParamD * gCollisionLookup[surfaceB].ParamD) * 1e-4f) * 3.f; // 0x4DB5F1: 0x858FC4 = 1e-4f, no division
        if (!(s < 0.3f)) { // 0x4DB610: 0x858C24
            s = 0.3f;
        }
    } else {
        s = mag * (float)(gCollisionLookup[surfaceA].ParamD * gCollisionLookup[surfaceB].ParamD) * 1e-4f * 3.f; // 0x4DA6B7
        if (!(s < 1.f)) { // 0x4DA6CB
            s = 1.f;
        }
    }
    if (isForceLooping) {
        s *= 0.16666667f; // 0x85F0A0
    }
    auto w = 1.3333334f * s; // 0x85F09C
    if (!(w < 1.f)) {
        w = 1.f;
    }
    const auto volume = (float)(X87R(std::log10(X87W((x87d)w)) * (x87d)20.0) + (x87d)GetDefaultVolume(AE_GENERAL_COLLISION)); // raw fyl2x: log10(0) = -inf
    return { volume, (s > 0.75f ? s : 0.75f) * 0.8f }; // 0x4DA741: `fcom 0.75; je` keeps s only when s > 0.75
}

// 0x4DB450
void CAECollisionAudioEntity::PlayLoopingCollisionSound(CEntity* entityA, CEntity* entityB, eSurfaceType surfaceA, eSurfaceType surfaceB, float force, const CVector& pos, bool isForceLooping) {
    const auto [volume, speed] = GetLoopingCollisionSoundVolumeAndSpeed(entityA, entityB, surfaceA, surfaceB, isForceLooping, true);

    const auto PlaySound = [&](eSoundID soundID) {
        if (auto* const sound = AESoundManager.PlaySound({
            .BankSlotID    = SND_BANK_SLOT_COLLISIONS,
            .SoundID       = soundID,
            .AudioEntity   = this,
            .Pos           = pos,
            .Volume        = volume,
            .RollOffFactor = 2.f,
            .Speed         = speed,
            .Flags         = SOUND_REQUEST_UPDATES,
            })
        ) {
            AddCollisionSoundToList(entityA, entityB, surfaceA, surfaceB, sound, COLLISION_SOUND_LOOPING);
        }
    };

    // FIX(#1406):
    // This fixes an OOB access in the original code,
    // Namely, originally for BMX `AE_SURFACE_TYPE_BMX` was passed in,
    // but it's not an actual surface type, but a special case for sound selection.

    // If both surfaces are valid we preserve the original logic...
    if (!notsa::bugfixes::CAECollisionAudioEntity_PlayLoopingCollisionSound_InvalidSurfaceType || (surfaceA < TOTAL_NUM_SURFACE_TYPES && surfaceB < TOTAL_NUM_SURFACE_TYPES)) {
        if (g_surfaceInfos.IsAudioGrass(surfaceA) || g_surfaceInfos.IsAudioGrass(surfaceB)) {
            PlaySound(SND_GENRL_COLLISIONS_GRASS_SKID);
        } else if (g_surfaceInfos.IsAudioWater(surfaceA) || g_surfaceInfos.IsAudioWater(surfaceB)) {
            PlaySound(SND_GENRL_COLLISIONS_WATER_LOOP);
        } else if (g_surfaceInfos.IsAudioMetal(surfaceA) || g_surfaceInfos.IsAudioMetal(surfaceB)) {
            PlaySound(SND_GENRL_COLLISIONS_METAL_SCRAPE);
        } else {
            PlaySound(SND_GENRL_COLLISIONS_GRAVEL_SKID);
        }
    } else {
        const auto GetSoundIDForSurface = [&](eSurfaceType surface) -> std::optional<eSoundID> { // 0x4DB6AF
            assert(surface < TOTAL_NUM_COLLISION_SURFACE_TYPES && "Expected `AE_SURFACE_TYPE_*` - Any other values are a bug");
            if (surface < TOTAL_NUM_SURFACE_TYPES) {
                if (g_surfaceInfos.IsAudioGrass(surface)) {
                    return SND_GENRL_COLLISIONS_GRASS_SKID;
                } else if (g_surfaceInfos.IsAudioWater(surface)) {
                    return SND_GENRL_COLLISIONS_WATER_LOOP;
                } else if (g_surfaceInfos.IsAudioMetal(surface)) {
                    return SND_GENRL_COLLISIONS_METAL_SCRAPE;
                }
            }
            return std::nullopt;
        };
        if (const auto soundOfSurfaceA = GetSoundIDForSurface(surfaceA)) {
            PlaySound(*soundOfSurfaceA);
        } else if (const auto soundOfSurfaceB = GetSoundIDForSurface(surfaceB)) {
            PlaySound(*soundOfSurfaceB);
        } else {
            PlaySound(SND_GENRL_COLLISIONS_GRAVEL_SKID);
        }
    }
}

// 0x4DA540
void CAECollisionAudioEntity::UpdateLoopingCollisionSound(
    CAESound*      sound,
    CEntity*       entityA,
    CEntity*       entityB,
    eSurfaceType   surfaceA,
    eSurfaceType   surfaceB,
    float          impulseForce,
    const CVector& pos,
    bool           isForceLooping
) {
    const auto [volume, speed] = GetLoopingCollisionSoundVolumeAndSpeed(entityA, entityB, surfaceA, surfaceB, isForceLooping, false);
    // 0x4DA74F: the CURRENT volume (+0x14) steps towards the target by 1.0, then the current speed (+0x1C) by 0.1 (0x858B1C). `StepTowards` = the exe's fcom ladders (unordered: unchanged)
    const auto StepTowards = [](float cur, float target, float step) {
        if (target > cur) {
            const auto up = cur + step;
            return up < target ? up : target;
        }
        if (target < cur) {
            const auto down = cur - step;
            return down > target ? down : target;
        }
        return cur;
    };
    sound->SetVolume(StepTowards(sound->GetVolume(), volume, 1.0f));
    sound->SetSpeed(StepTowards(sound->GetSpeed(), speed, 0.1f));
    sound->SetPosition(pos);
}

// 0x4DB7C0
void CAECollisionAudioEntity::PlayBulletHitCollisionSound(eSurfaceType surface, const CVector& posn, float angleWithColPointNorm) {
    if (surface >= TOTAL_NUM_COLLISION_SURFACE_TYPES) {
        return;
    }
    const auto PlayRandomSound = [&](int32 minID, int32 maxID, float volumeOffset = 0.f, float rollOff = 1.5f) { // 0x4DB9BF
        //! Find a new random sound ID that is not the same as the last one.
        const auto GetNewRandomSoundID = [&]{
            while (true) {
                const auto id = CAEAudioUtility::GetRandomNumberInRange(minID, maxID);
                if (id != m_LastBulletHitSoundID) {
                    return id;
                }
            }
        };
        AESoundManager.PlaySound({
            .BankSlotID        = SND_BANK_SLOT_BULLET_HITS,
            .SoundID           = m_LastBulletHitSoundID = GetNewRandomSoundID(),
            .AudioEntity       = this,
            .Pos               = posn,
            .Volume            = GetDefaultVolume(AE_BULLET_HIT) + volumeOffset,
            .RollOffFactor     = rollOff,
            .FrequencyVariance = 0.02f
        });
    };
    if (surface == SURFACE_PED) {
        PlayRandomSound(7, 9);
    } else if (g_surfaceInfos.IsAudioWater(surface)) {
        PlayRandomSound(16, 18, 6.f, 2.f);
    } else if (g_surfaceInfos.IsAudioWood(surface)) {
        PlayRandomSound(19, 21);
    } else if (g_surfaceInfos.IsAudioMetal(surface)) {
        if (CAEAudioUtility::ResolveProbability((90.0f - angleWithColPointNorm) * ExeRecip(180.0f))) { // see BoneNode_c::EulerToQuat
            PlayRandomSound(10, 12);
        } else {
            PlayRandomSound(4, 6);
        }
    } else if (g_surfaceInfos.IsAudioGravelConcreteOrTile(surface)) {
        PlayRandomSound(13, 15);
    } else {
        PlayRandomSound(1, 3);
    }
}

// 0x4DA070
void CAECollisionAudioEntity::ReportGlassCollisionEvent(eAudioEvents event, const CVector& posn, uint32 time) {
    const auto PlayCollisionSound = [&](eSoundID soundID, float speed) {
        if (time) {
            AESoundManager.PlaySound({
                .BankSlotID     = SND_BANK_SLOT_COLLISIONS,
                .SoundID        = soundID,
                .AudioEntity    = this,
                .Pos            = posn,
                .Volume         = GetDefaultVolume(event),
                .RollOffFactor  = 1.5f,
                .Speed          = 0.f,
                .Flags          = SOUND_REQUEST_UPDATES,
                .EventID        = event,
                .ClientVariable = (float)(time + CTimer::GetTimeInMS()),
            });
        } else {
            AESoundManager.PlaySound({
                .BankSlotID     = SND_BANK_SLOT_COLLISIONS,
                .SoundID        = soundID,
                .AudioEntity    = this,
                .Pos            = posn,
                .Volume         = GetDefaultVolume(event),
                .RollOffFactor  = 1.5f,
                .Speed          = speed,
            });
        }
    };
    switch (event) {
    case AE_GLASS_HIT:             PlayCollisionSound(51, 1.f);                                                break;
    case AE_GLASS_CRACK:           PlayCollisionSound(68, 1.f);                                                break;
    case AE_GLASS_BREAK_SLOW:      PlayCollisionSound(56, 0.75f);                                              break;
    case AE_GLASS_BREAK_FAST:      PlayCollisionSound(56, 1.f);                                                break;
    case AE_GLASS_HIT_GROUND:      PlayCollisionSound(CAEAudioUtility::GetRandomNumberInRange(15, 18), 1.f);   break;
    case AE_GLASS_HIT_GROUND_SLOW: PlayCollisionSound(CAEAudioUtility::GetRandomNumberInRange(15, 18), 0.56f); break;
    default:                       break; // 0x4DA089: `ja` to the epilogue for anything outside 123..128
    }
}

// 0x4DA190
void CAECollisionAudioEntity::ReportWaterSplash(CVector posn, float volume) {
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS, true)) {
        return;
    }
    AESoundManager.PlaySound({                        
        .BankSlotID    = SND_BANK_SLOT_COLLISIONS,
        .SoundID       = 67,
        .AudioEntity   = this,
        .Pos           = posn,
        .Volume        = GetDefaultVolume(AE_WATER_SPLASH) + volume,
        .RollOffFactor = 2.5f,
        .Speed         = 1.26f,
        .Flags         = SOUND_REQUEST_UPDATES,
        .EventID       = 1                               
    });
    AESoundManager.PlaySound({
        .BankSlotID     = SND_BANK_SLOT_COLLISIONS,
        .SoundID        = 66,
        .AudioEntity    = this,
        .Pos            = posn,
        .Volume         = GetDefaultVolume(AE_WATER_SPLASH) + volume,
        .RollOffFactor  = 2.5f,
        .Speed          = 0.f,
        .Flags          = SOUND_REQUEST_UPDATES,
        .EventID        = 2,
        .ClientVariable = (float)(CTimer::GetTimeInMS() + 166)
    });
}

// 0x4DAE40
void CAECollisionAudioEntity::ReportWaterSplash(CPhysical* physical, float volume, bool isForceSplash) {
    assert(physical->GetIsTypePhysical());

    if (!isForceSplash) {
        if (physical->GetMoveSpeed().z > -0.1f && volume == -100.f) {
            return;
        }
        if (rng::any_of(std::to_array({ 1, 2, 3 }), [&](int32 event) {
            return AESoundManager.AreSoundsOfThisEventPlayingForThisEntityAndPhysical(event, this, physical) != 0;
        })) {
            return;
        }
    }
    if (!AEAudioHardware.EnsureSoundBankIsLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS, true)) { // 0x4DAED2
        return;
    }
    const auto PlaySound = [&](int32 eventID, float speed, float volumeOffset, uint32 interval) {
        if (!(volume > -100.f)) { // 0x4DAF98: `fcomp -100.0; jne` (<= or unordered)
            // 0x4DAFB2..0x4DB04D: log10(min(|move speed|, 0.6) * 1/0.6 (0x85F0A8)) * 20 with a raw `fyl2x`, at least -18 (0x85F0A4)
            const auto mag = physical->GetMoveSpeed().Magnitude();
            const auto lg  = (float)X87R(std::log10(X87W((x87d)((mag < 0.6f ? mag : 0.6f) * 1.6666666f))) * (x87d)20.0);
            volume = lg > -18.f ? lg : -18.f;
        }
        volume += GetDefaultVolume(AE_WATER_SPLASH) + volumeOffset;

        AESoundManager.PlaySound({
            .BankSlotID         = SND_BANK_SLOT_COLLISIONS,
            .SoundID            = 67,
            .AudioEntity        = this,
            .Pos                = physical->GetPosition(),
            .Volume             = volume,
            .RollOffFactor      = 2.5f,
            .Speed              = speed,
            .Flags              = SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY, // 0x4DB070: 0x84 + `RegisterWithPhysicalEntity` (0x4DB0AA) on this one too
            .RegisterWithEntity = physical,
            .EventID            = 1,
        });
        AESoundManager.PlaySound({
            .BankSlotID         = SND_BANK_SLOT_COLLISIONS,
            .SoundID            = 66,
            .AudioEntity        = this,
            .Pos                = physical->GetPosition(),
            .Volume             = volume,
            .RollOffFactor      = 2.5f,
            .Speed              = 0.f, // 0x4DB0E2: `push 0` (the delayed sound is retargeted to `speed` by `UpdateParameters` once the delay is over)
            .Flags              = SOUND_REQUEST_UPDATES | SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY,
            .RegisterWithEntity = physical,
            .EventID            = eventID,
            .ClientVariable     = (float)(CTimer::GetTimeInMS() + interval),
        });
    };
    if (physical->GetIsTypePed()) { // 0x4DAF2B - Inverted
        if (isForceSplash && !physical->AsPed()->GetIntelligence()->GetTaskSwim()) {
            PlaySound(2, 1.26f, -6.f, 166);
        }
    } else if (physical->GetIsTypeVehicle()) { // 0x4DAF52
        PlaySound(3, 0.94f, 0.f, 248);
    } else { // 0x4DAF70
        PlaySound(2, 1.26f, -12.f, 166);
    }
}

// 0x4DAB60
void CAECollisionAudioEntity::ReportObjectDestruction(CEntity* entity) {
    if (!entity) {
        return;
    }

    if (entity->GetModelId() == ModelIndices::MI_GRASSHOUSE) { // 0x4DAB94 - Moved up here
        AESoundManager.PlaySound({ // 0x4DABEF
            .BankSlotID    = SND_BANK_SLOT_COLLISIONS,
            .SoundID       = (eSoundID)(56),
            .AudioEntity   = this,
            .Pos           = entity->GetPosition(),
            .Volume        = GetDefaultVolume(AE_GLASS_BREAK_SLOW),
            .RollOffFactor = 1.5f,
            .Speed         = 0.75f,
        });
        AESoundManager.PlaySound({ // 0x4DAC65
            .BankSlotID    = SND_BANK_SLOT_COLLISIONS,
            .SoundID       = (eSoundID)(CAEAudioUtility::GetRandomNumberInRange(15, 18)),
            .AudioEntity   = this,
            .Pos           = entity->GetPosition(),
            .Volume        = GetDefaultVolume(AE_GLASS_HIT_GROUND_SLOW),
            .RollOffFactor = 1.5f,
            .Speed         = 0.0f,
            .Flags         = SOUND_REQUEST_UPDATES,
            .EventID        = AE_GLASS_HIT_GROUND_SLOW,
            .ClientVariable = (float)(CTimer::GetTimeInMS() + 600),
        });
        return;
    }

    const auto PlaySoundForSurface = [&](eSurfaceType surface) {
        const auto PlaySound = [&](eSoundID soundID) {
            AESoundManager.PlaySound({
                .BankSlotID        = SND_BANK_SLOT_COLLISIONS,
                .SoundID           = soundID,
                .AudioEntity       = this,
                .Pos               = entity->GetPosition(),
                .Volume            = GetDefaultVolume(AE_GENERAL_COLLISION),
                .RollOffFactor     = 2.f,
                .FrequencyVariance = 0.0588f,
            });
        };
        switch (surface) {
        case SURFACE_WOOD_CRATES:
        case SURFACE_WOOD_BENCH:
        case SURFACE_WOOD_PICKET_FENCE:
        case SURFACE_WOOD_SLATTED_FENCE:
        case SURFACE_WOOD_RANCH_FENCE:   PlaySound(CAEAudioUtility::GetRandomNumberInRange(41, 44)); break; // 0x4DAD14
        case SURFACE_GLASS:              PlaySound(11); break;                                              // 0x4DAD43
        default:                         break;
        }
    };
    if (entity->GetModelId() == MODEL_MOLOTOV) { // 0x4DAB7B
        PlaySoundForSurface(SURFACE_GLASS);
    } else if (const auto cd = entity->GetModelInfo()->GetColModel()->m_pColData) { // 0x4DACAE: the MODEL's collision (`[model + 0x14]`), not `GetColModel()` (vehicle special collisions) // 0x4DAB89
        if (const auto spheres = cd->GetSpheres(); !spheres.empty()) {
            PlaySoundForSurface(spheres.front().GetSurfaceType());
        } else if (const auto boxes = cd->GetBoxes(); !boxes.empty()) {
            PlaySoundForSurface(boxes.front().GetSurfaceType());
        } else if (const auto tris = cd->GetTris(); !tris.empty()) {
            PlaySoundForSurface(tris.front().GetSurfaceType());
        }
    }
}

// 0x4DBA10
void CAECollisionAudioEntity::ReportCollision(
    CEntity*       entityA,
    CEntity*       entityB,
    eSurfaceType   surfaceA,
    eSurfaceType   surfaceB,
    const CVector& pos,
    const CVector* normal,
    float          impulseForce,
    float          relVelSq,
    bool           isForceOneShot,
    bool           isForceLooping
) {
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_COLLISIONS, SND_BANK_SLOT_COLLISIONS)) {
        return;
    }
    if (!CanAddNewSound()) {
        return;
    }
    if (relVelSq <= 0.f) {
        return;
    }

    const auto GetSurfaceToUse = [&](
        CEntity* eA, eSurfaceType& sA,
        CEntity* eB, eSurfaceType& sB
    ) {
        if (!eA->GetIsTypeVehicle()) {
            switch (eA->GetModelId()) {
            case MODEL_MOLOTOV:
                return SURFACE_GLASS;
            case MODEL_SATCHEL:
                return AE_SURFACE_TYPE_SATCHEL_CHARGE;
            case MODEL_GRENADE:
            case MODEL_BOMB:
            case MODEL_TEARGAS:
                return AE_SURFACE_TYPE_GRENADE;
            }
            const auto eAModelId = eA->GetModelId();
            if (eAModelId == ModelIndices::MI_BASKETBALL) {
                return AE_SURFACE_TYPE_BASKETBALL;
            }
            if (eAModelId == ModelIndices::MI_PUNCHBAG) {
                return AE_SURFACE_TYPE_PUNCHBAG;
            }
            if (eAModelId == ModelIndices::MI_GRASSHOUSE) {
                return SURFACE_UNBREAKABLE_GLASS;
            }
            if (eAModelId == ModelIndices::MI_IMY_GRAY_CRATE) {
                return SURFACE_WOOD_SOLID;
            }
            if (eA->GetIsTypePhysical() && eA->AsPhysical()->physicalFlags.bDisableZ) { // 0x4DBC11: `test byte [esi+0x40], 0x80` (bit 7, not bit 0)
                return AE_SURFACE_TYPE_POOL_BALL;
            }
            return sA;        
        }
        if (eB && eB->GetIsTypeBuilding() && normal && (eA->GetUp().z * normal->z + eA->GetUp().y * normal->y + eA->GetUp().x * normal->x) > 0.6f) { // 0x4DBA9F: z, y, x
            if (eA->AsVehicle()->IsSubBMX()) {
                return AE_SURFACE_TYPE_BMX;
            }
            if (g_surfaceInfos.GetFrictionEffect(sB) != FRICTION_EFFECT_SPARKS) {
                return SURFACE_RUBBER;
            }
        }
        if (eA->AsVehicle()->IsSubBMX()) {
            return AE_SURFACE_TYPE_BMX;
        }
        return SURFACE_CAR;
    };

    surfaceA = GetSurfaceToUse(entityA, surfaceA, entityB, surfaceB);
    surfaceB = GetSurfaceToUse(entityB, surfaceB, entityA, surfaceA);

    if (isForceOneShot) { // 0x4DBC68
        PlayOneShotCollisionSound(entityA, entityB, surfaceA, surfaceB, impulseForce, pos);
    } else  { // 0x4DBC83
        int32 entryID;
        switch (const auto soundStatus = CAECollisionAudioEntity::GetCollisionSoundStatus(entityA, entityB, surfaceA, surfaceB, entryID)) {
        case COLLISION_SOUND_INACTIVE: { // 0x4DBD1C, 0x4DBD5A
            if (isForceLooping) {
                return PlayLoopingCollisionSound(entityA, entityB, surfaceA, surfaceB, impulseForce, pos, isForceLooping);
            }
            return PlayOneShotCollisionSound(entityA, entityB, surfaceA, surfaceB, impulseForce, pos);
        }
        case COLLISION_SOUND_ONE_SHOT: // 0x4DBD1C, 0x4DBDE0
            return PlayLoopingCollisionSound(entityA, entityB, surfaceA, surfaceB, impulseForce, pos, isForceLooping);
        case COLLISION_SOUND_LOOPING: { // 0x4DBCA3, 0x4DBD44
            const auto e = &m_CollisionSoundList[entryID];
            e->LoopStopTimeMs = CTimer::GetTimeInMS() + 100;
            if (e->Sound) {
                UpdateLoopingCollisionSound(e->Sound, e->EntityA, e->EntityB, e->SurfaceA, e->SurfaceB, impulseForce, pos, isForceLooping);
            }
            break;
        }
        default:
            NOTSA_UNREACHABLE("Invalid soundStatus: {}", (int32)(soundStatus));
        }
    }
}

// 0x4DBDF0
void CAECollisionAudioEntity::ReportBulletHit(CEntity* entity, eSurfaceType surface, const CVector& posn, float angleWithColPointNorm) {
    if (!AEAudioHardware.IsSoundBankLoaded(SND_BANK_GENRL_BULLET_HITS, SND_BANK_SLOT_BULLET_HITS)) {
        return;
    }
    if (entity && entity->GetIsTypeVehicle()) {
        PlayBulletHitCollisionSound(
            entity->AsVehicle()->IsSubBMX()
                ? AE_SURFACE_TYPE_BMX
                : SURFACE_CAR,
            posn,
            angleWithColPointNorm
        );
    } else {
        PlayBulletHitCollisionSound(surface, posn, angleWithColPointNorm);
    }
}

// 0x4DA2C0
void CAECollisionAudioEntity::Service() {
    const auto time = CTimer::GetTimeInMS();
    for (auto& entry : m_CollisionSoundList) {
        if (entry.Status != COLLISION_SOUND_LOOPING || time < entry.LoopStopTimeMs)
            continue;

        if (entry.Sound) {
            entry.Sound->StopSoundAndForget();
        }

        ReleaseEntry(entry);
        --m_NumActiveCollisionSounds;
    }
}

// 0x4DAA50
eSoundID CAECollisionAudioEntity::ChooseCollisionSoundID(eSurfaceType surface) {
    const auto* const l = &gCollisionLookup[surface];
    if (l->MinSoundID == l->MaxSoundID) {
        return l->MinSoundID;
    }
    for (auto retry = 0; retry < 100; retry++) { // NOTSA: Handle very unlikely infinite loop
        const auto soundID = CAEAudioUtility::GetRandomNumberInRange(l->MinSoundID, l->MaxSoundID);
        if (soundID != m_CollisionSoundIDHistory[surface]) {
            return soundID;
        }
    }
    NOTSA_LOG_WARN("Failed to generate collision sound ID not in history!"); // NOTSA
    return l->MinSoundID; // NOTSA
}
