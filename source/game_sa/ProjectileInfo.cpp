#include "StdInc.h"

#include "ProjectileInfo.h"

#include "Entity/Object/Projectile.h"
#include "Radar.h"
#include "World.h"
#include "Pools/Pools.h"
#include "Collision/Box.h"
#include "Collision/ColModel.h"

namespace {
// Used by heat seeking missiles (WEAPON_ROCKET_HS) that are homing in on a plane (by the player)
inline auto& HOMING_PLANE_DAMPING = StaticRef<float>(0x8D6104); // 0.95 - velocity multiplier (per time step, exponent)
inline auto& HOMING_PLANE_ACCEL   = StaticRef<float>(0x8D6108); // 0.15 - acceleration towards the target
// Not exactly `0.0117f` (it's 1 ULP lower), so read it from the exe
inline auto& HOMING_PLAYER_ACCEL  = StaticRef<float>(0x872BFC); // Acceleration towards the target of the missiles fired by the player
}

void CProjectileInfo::InjectHooks() {
    RH_ScopedClass(CProjectileInfo);
    RH_ScopedCategoryGlobal();

    // Install("CProjectileInfo", "", , &CProjectileInfo::);
    RH_ScopedInstall(Initialise, 0x737B40);
    RH_ScopedInstall(Shutdown, 0x737BC0);
    RH_ScopedInstall(GetProjectileInfo, 0x737BF0);
    RH_ScopedInstall(RemoveNotAdd, 0x737C00);
    RH_ScopedInstall(AddProjectile, 0x737C80);
    RH_ScopedInstall(RemoveDetonatorProjectiles, 0x738860);
    RH_ScopedInstall(RemoveProjectile, 0x7388F0);
    RH_ScopedInstall(Update, 0x738B20);
    RH_ScopedInstall(IsProjectileInRange, 0x739860);
    RH_ScopedInstall(RemoveAllProjectiles, 0x7399B0);
    RH_ScopedInstall(RemoveIfThisIsAProjectile, 0x739A40);
    RH_ScopedInstall(RemoveFXSystem, 0x737B80);
}

// 0x737B40
void CProjectileInfo::Initialise() {
    ms_apProjectile.fill(nullptr);
    for (auto& info : gaProjectileInfo) {
        info.m_nWeaponType = WEAPON_GRENADE;
        info.m_pCreator    = nullptr;
        info.m_nDestroyTime = 0;
        info.m_bActive     = false;
        info.m_pFxSystem   = nullptr;
    }
}

// 0x737BC0
void CProjectileInfo::Shutdown() {
    for (auto& info : gaProjectileInfo) {
        info.RemoveFXSystem(true);
    }
}

// 0x737BF0
CProjectileInfo* CProjectileInfo::GetProjectileInfo(int32 infoId) {
    return &gaProjectileInfo[infoId];
}

// 0x737C00
void CProjectileInfo::RemoveNotAdd(CEntity* creator, eWeaponType weaponType, CVector pos) {
    eExplosionType explosionType;
    switch (weaponType) {
    case WEAPON_GRENADE:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
        explosionType = EXPLOSION_GRENADE;
        break;
    case WEAPON_MOLOTOV:
        explosionType = EXPLOSION_MOLOTOV;
        break;
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS:
        explosionType = EXPLOSION_ROCKET;
        break;
    default:
        return;
    }
    CExplosion::AddExplosion(nullptr, creator, explosionType, pos, 0, true, -1.0f, false);
}

// 0x737C80
bool CProjectileInfo::AddProjectile(CEntity* creator, eWeaponType projectileType, CVector origin, float force, const CVector* dir, CEntity* target) {
    CMatrix  matrix;
    CVector  velocity{};
    float    elasticity   = 0.75f;
    bool     applyGravity = true;
    uint8    objInfoFlag  = 0; // Set to 5 for grenade-like projectiles (means "use object info 4")
    uint32   destroyTime{};

    // Heading of the creator, used by thrown projectiles
    const auto GetCreatorHeading = [&] {
        if (creator->m_matrix) {
            return std::atan2(-creator->m_matrix->GetForward().x, creator->m_matrix->GetForward().y);
        }
        return creator->m_placement.m_fHeading;
    };

    switch (projectileType) {
    case WEAPON_GRENADE:
    case WEAPON_REMOTE_SATCHEL_CHARGE: {
        destroyTime = CTimer::GetTimeInMS() + 2000;

        float speed = force == 0.0f ? 0.0f : force * 0.22f + 0.15f;
        if (projectileType == WEAPON_REMOTE_SATCHEL_CHARGE) {
            speed *= 0.5f;
        }

        float angle = GetCreatorHeading();
        if (creator->GetIsTypeVehicle()) {
            angle = CGeneral::LimitRadianAngle(angle + 3.1415927f);
        }

        matrix.SetTranslate(CVector{ 0.0f, 0.0f, 0.0f });
        matrix.RotateZ(angle);
        matrix.m_pos.x += origin.x;
        matrix.m_pos.y += origin.y;
        matrix.m_pos.z += origin.z;

        velocity.x = std::sin(angle) * speed * -1.0f;
        velocity.y = std::cos(angle) * speed;
        velocity.z = (force + 1.0f) * 0.4f * speed;
        if (creator->m_nModelIndex == MODEL_SENTINEL) { // Weird, but the original code checks for this model specifically
            velocity += creator->AsPhysical()->m_vecMoveSpeed;
        }

        objInfoFlag = 5;
        elasticity  = projectileType == WEAPON_REMOTE_SATCHEL_CHARGE ? 0.03f : 0.5f;
        break;
    }
    case WEAPON_TEARGAS: {
        destroyTime = CTimer::GetTimeInMS() + 20000;

        const float speed = force == 0.0f ? 0.0f : force * 0.22f + 0.15f;
        const float angle = GetCreatorHeading();

        matrix.SetTranslate(CVector{ 0.0f, 0.0f, 0.0f });
        matrix.RotateZ(angle);
        matrix.m_pos.x += origin.x;
        matrix.m_pos.y += origin.y;
        matrix.m_pos.z += origin.z;

        objInfoFlag = 5;
        elasticity  = 0.5f;

        velocity.x = std::sin(angle) * speed * -1.0f;
        velocity.y = std::cos(angle) * speed;
        velocity.z = (force + 1.0f) * 0.4f * speed;
        break;
    }
    case WEAPON_MOLOTOV: {
        destroyTime = CTimer::GetTimeInMS() + 2000;

        float speed = force * 0.22f + 0.15f;
        if (speed < 0.2f) {
            speed = 0.2f;
        }
        const float angle = GetCreatorHeading();

        matrix.SetTranslate(CVector{ 0.0f, 0.0f, 0.0f });
        matrix.RotateZ(angle);
        matrix.m_pos.x += origin.x;
        matrix.m_pos.y += origin.y;
        matrix.m_pos.z += origin.z;

        velocity.x = std::sin(angle) * speed * -1.0f;
        velocity.y = std::cos(angle) * speed;
        velocity.z = (force * 0.2f + 0.4f) * speed;
        break;
    }
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS: {
        float speed;
        if (projectileType == WEAPON_ROCKET) {
            destroyTime = CTimer::GetTimeInMS() + 3000;
            speed       = 0.4f;
        } else {
            destroyTime = CTimer::GetTimeInMS() + 10000;
            speed       = 0.2f;
        }

        if (creator->GetIsTypeVehicle()) {
            matrix       = creator->GetMatrix();
            matrix.m_pos = origin;
            speed        = creator->AsPhysical()->m_vecMoveSpeed.Magnitude() + speed;
        } else if (creator->GetIsTypePed() && creator->AsPed()->IsPlayer()) {
            const auto& cam = TheCamera.GetActiveCam();
            matrix.m_forward = cam.m_vecFront;
            matrix.m_up      = cam.m_vecUp;
            matrix.m_right   = CrossProduct(cam.m_vecUp, cam.m_vecFront);
            matrix.m_pos     = origin;
        } else if (dir) {
            matrix.m_forward = *dir;
            matrix.m_right   = creator->GetMatrix().m_right;
            matrix.m_up      = CrossProduct(matrix.m_right, matrix.m_forward);
            matrix.m_pos     = origin;
        } else {
            matrix = creator->GetMatrix(); // BUG: `origin` isn't used in this case, the creator's position is
        }

        velocity     = matrix.TransformVector(CVector{ 0.0f, speed, 0.0f });
        applyGravity = false;
        break;
    }
    case WEAPON_FREEFALL_BOMB:
    case WEAPON_FLARE: {
        if (projectileType == WEAPON_FREEFALL_BOMB) {
            destroyTime = CTimer::GetTimeInMS() + 2000000;
        } else {
            CStreaming::RequestModel(ModelIndices::MI_FLARE, 0);
            destroyTime = CTimer::GetTimeInMS() + 10000;
        }

        if (creator->GetType() >= ENTITY_TYPE_VEHICLE && creator->GetType() <= ENTITY_TYPE_OBJECT) {
            velocity = creator->AsPhysical()->m_vecMoveSpeed;
        } else {
            velocity = CVector{ 0.0f, 0.0f, 0.0f };
        }
        matrix       = creator->GetMatrix();
        matrix.m_pos = origin;
        break;
    }
    default:
        break;
    }

    // Find a free slot
    size_t slot = 0;
    while (slot < gaProjectileInfo.size() && gaProjectileInfo[slot].m_bActive) {
        slot++;
    }
    if (slot == gaProjectileInfo.size()) {
        return false;
    }

    // Create the object
    switch (projectileType) {
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_REMOTE_SATCHEL_CHARGE: {
        auto* const proj = new CProjectile(CWeaponInfo::GetWeaponInfo(projectileType, eWeaponSkill::STD)->m_nModelId1);
        ms_apProjectile[slot] = proj;
        if (proj) {
            // Make sure the model has a collision sphere
            auto* const cm = proj->GetModelInfo()->GetColModel();
            if (!cm->m_pColData) {
                cm->AllocateData(1, 0, 0, 0, 0, false);
                cm->m_pColData->m_pSpheres[0].Set(cm->GetBoundRadius() * 0.75f, cm->GetBoundCenter(), SURFACE_GIRDER);
            } else if (cm->m_pColData->m_nNumSpheres == 0 && !cm->m_pColData->m_pSpheres) {
                cm->m_pColData->m_nNumSpheres = 1;
                cm->m_pColData->m_pSpheres    = static_cast<CColSphere*>(CMemoryMgr::Malloc(sizeof(CColSphere)));
                cm->m_pColData->m_pSpheres->Set(cm->GetBoundRadius() * 0.75f, cm->GetBoundCenter(), SURFACE_GIRDER);
            }
        }
        break;
    }
    case WEAPON_ROCKET:
    case WEAPON_ROCKET_HS:
    case WEAPON_FREEFALL_BOMB:
        ms_apProjectile[slot] = new CProjectile(CWeaponInfo::GetWeaponInfo(projectileType, eWeaponSkill::STD)->m_nModelId1);
        break;
    case WEAPON_FLARE: {
        auto* const proj = new CProjectile(ModelIndices::MI_FLARE);
        ms_apProjectile[slot] = proj;
        if (proj) {
            proj->m_fAirResistance = 0.9f;
        }
        break;
    }
    default: // BUG: Slot isn't touched, so a stale (possibly already deleted) pointer might be used below
        break;
    }

    auto* const proj = ms_apProjectile[slot];
    if (!proj) {
        return false;
    }

    auto& info = gaProjectileInfo[slot];
    info.m_nWeaponType = projectileType;
    info.m_pCreator    = creator;
    creator->RegisterReference(&info.m_pCreator);

    proj->SetMatrix(matrix);
    proj->m_vecMoveSpeed = velocity;
    proj->physicalFlags.bApplyGravity = applyGravity;
    info.m_nDestroyTime = destroyTime;
    proj->m_fElasticity = elasticity;
    if (objInfoFlag == 5) {
        proj->m_pObjectInfo = &CObjectData::GetAtIndex(4);
    }

    info.m_pVictim = target;
    if (target) {
        target->RegisterReference(&info.m_pVictim);
    }

    info.m_bActive = true;
    CWorld::Add(proj);
    proj->RegisterReference(reinterpret_cast<CEntity**>(&ms_apProjectile[slot]));
    info.m_vecLastPosn = proj->GetPosition();

    if (projectileType == WEAPON_TEARGAS) {
        if (auto* const mat = proj->GetModellingMatrix()) {
            info.m_pFxSystem = g_fxMan.CreateFxSystem("teargasAD", CVector{ 0.0f, 0.0f, 0.0f }, mat, false);
            if (info.m_pFxSystem) {
                info.m_pFxSystem->Play();
            }
        }
    }

    proj->m_pEntityIgnoredCollision = creator;
    proj->physicalFlags.bCanBeCollidedWith = true;
    if (creator->GetType() >= ENTITY_TYPE_VEHICLE && creator->GetType() <= ENTITY_TYPE_OBJECT) {
        if (!creator->AsPhysical()->m_pEntityIgnoredCollision) {
            creator->AsPhysical()->m_pEntityIgnoredCollision = creator;
        }
    }

    if (projectileType == WEAPON_ROCKET_HS) {
        const auto blip = CRadar::SetEntityBlip(BLIP_OBJECT, GetObjectPool()->GetRef(proj), 0xFF0000FF, BLIP_DISPLAY_BLIPONLY);
        CRadar::ChangeBlipScale(blip, 1);
        CRadar::ChangeBlipColour(blip, creator == FindPlayerPed(-1) || creator == FindPlayerVehicle(-1, false)
            ? static_cast<eBlipColour>(0xFFFFFFFF)
            : static_cast<eBlipColour>(0xFF0000FF)
        );
    }

    AudioEngine.ReportWeaponEvent(AE_PROJECTILE_FIRE, projectileType, proj);

    return true;
}

// 0x738860
void CProjectileInfo::RemoveDetonatorProjectiles() {
    for (auto&& [info, proj] : rngv::zip(gaProjectileInfo, ms_apProjectile)) {
        if (!info.m_bActive || info.m_nWeaponType != WEAPON_REMOTE_SATCHEL_CHARGE) {
            continue;
        }

        CExplosion::AddExplosion(nullptr, info.m_pCreator, EXPLOSION_GRENADE, proj->GetPosition(), 0, true, -1.0f, false);
        info.m_bActive = false;
        info.RemoveFXSystem(false);
        proj->m_bRemoveFromWorld = true;
    }
}

// 0x7388F0
void CProjectileInfo::RemoveProjectile(CProjectileInfo* info, CProjectile* object) {
    switch (info->m_nWeaponType) {
    case WEAPON_GRENADE:
    case WEAPON_FREEFALL_BOMB:
        CExplosion::AddExplosion(nullptr, info->m_pCreator, EXPLOSION_GRENADE, object->GetPosition(), 0, true, -1.0f, false);
        break;
    case WEAPON_MOLOTOV:
        CExplosion::AddExplosion(nullptr, info->m_pCreator, EXPLOSION_MOLOTOV, object->GetPosition(), 0, true, -1.0f, false);
        AudioEngine.ReportObjectDestruction(object);
        break;
    case WEAPON_ROCKET: {
        // If the rocket was fired from a vehicle the driver is the explosion's creator
        auto* const creator = info->m_pCreator && info->m_pCreator->GetIsTypeVehicle()
            ? info->m_pCreator->AsVehicle()->m_pDriver
            : info->m_pCreator;
        CExplosion::AddExplosion(nullptr, creator, EXPLOSION_ROCKET, object->GetPosition(), 0, true, -1.0f, false);
        break;
    }
    case WEAPON_ROCKET_HS: {
        // Rockets of other peds/vehicles do less damage
        const auto type = info->m_pCreator == FindPlayerPed(-1) ? EXPLOSION_ROCKET : EXPLOSION_WEAK_ROCKET;
        CExplosion::AddExplosion(nullptr, info->m_pCreator, type, object->GetPosition(), 0, true, -1.0f, false);
        break;
    }
    default:
        break;
    }

    info->m_bActive = false;
    if (info->m_pFxSystem) { // Not using `RemoveFXSystem` to keep the same behaviour
        info->m_pFxSystem->Kill();
        info->m_pFxSystem = nullptr;
    }
    CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(object));
    CWorld::Remove(object);
    delete object;
}

// 0x738B20
void CProjectileInfo::Update() {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    for (size_t i = 0; i < gaProjectileInfo.size(); i++) {
        auto&       info = gaProjectileInfo[i];
        auto* const proj = ms_apProjectile[i];
        if (!info.m_bActive) {
            continue;
        }

        if (proj->physicalFlags.bSubmergedInWater) {
            info.RemoveFXSystem(false);
        }

        if (info.m_pCreator && info.m_pCreator->GetIsTypePed() && !info.m_pCreator->AsPed()->IsPointerValid()) {
            info.m_pCreator = nullptr;
        }

        const auto type = static_cast<eWeaponType>(info.m_nWeaponType);

        if (type == WEAPON_REMOTE_SATCHEL_CHARGE || type == WEAPON_GRENADE || type == WEAPON_TEARGAS) {
            // Make the projectile less bouncy once it has (almost) stopped
            if (proj->m_fElasticity > 0.1f
                && std::abs(proj->m_vecMoveSpeed.x) < 0.05f
                && std::abs(proj->m_vecMoveSpeed.y) < 0.05f
                && std::abs(proj->m_vecMoveSpeed.z) < 0.05f
            ) {
                proj->m_fElasticity = 0.03f;
            }

            if (type == WEAPON_TEARGAS
                && info.m_nDestroyTime - 17500u < CTimer::GetTimeInMS()
                && CGeneral::GetRandomNumberInRange(0, 100) < 10
            ) {
                const auto& pos = proj->GetPosition();
                CWorld::SetPedsChoking(pos.x, pos.y, pos.z, 6.0f, info.m_pCreator);
            }
        }

        if (type == WEAPON_ROCKET || type == WEAPON_ROCKET_HS) {
            // Smoke trail
            const auto Rand01 = [] { return static_cast<float>(CGeneral::GetRandomNumber()) * RAND_MAX_FLOAT_RECIPROCAL; }; // Same as `rand() / 32767.f`
            FxPrtMult_c prtMult{ 0.3f, 0.3f, 0.3f, 0.3f, 0.5f, 1.0f, 0.08f };

            const CVector moved{
                CTimer::ms_fTimeStep * proj->m_vecMoveSpeed.x,
                CTimer::ms_fTimeStep * proj->m_vecMoveSpeed.y,
                CTimer::ms_fTimeStep * proj->m_vecMoveSpeed.z,
            };
            const auto numParticles = std::max(1, static_cast<int32>(std::sqrt((moved.x * moved.x + moved.z * moved.z) + moved.y * moved.y)));
            for (int32 p = 0; p < numParticles; p++) {
                const auto shade = Rand01() * 0.25f + 0.25f;
                prtMult.m_Color.red   = shade;
                prtMult.m_Color.green = shade;
                prtMult.m_Color.blue  = shade;
                prtMult.m_fLife       = Rand01() * 0.04f + 0.08f;

                const float t = 1.0f - static_cast<float>(p) / static_cast<float>(numParticles);
                const auto& projPos = proj->GetPosition();
                const CVector particlePos{
                    projPos.x - t * moved.x,
                    projPos.y - t * moved.y,
                    projPos.z - t * moved.z,
                };

                CVector randomDir{
                    Rand01() * 2.0f - 1.0f,
                    Rand01() * 2.0f - 1.0f,
                    Rand01() * 2.0f - 1.0f,
                };
                randomDir.Normalise();

                CVector moveDir = proj->m_vecMoveSpeed;
                moveDir.Normalise();

                const CVector particleVel = CrossProduct(moveDir, randomDir) * 1.5f;
                g_fx.m_SmokeHuge->AddParticle(particlePos, particleVel, 0.0f, prtMult, -1.0f, 1.2f, 0.6f, false);
            }
        }

        // Rockets: check for collisions (0x7396BB)
        const auto CheckRocketCollision = [&] {
            if (!proj->physicalFlags.bOnSolidSurface) {
                CWorld::pIgnoreEntity = info.m_pCreator;
                proj->SetUsesCollision(false);
                const bool isClear = CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, proj->GetPosition(), true, true, true, true, false, false, false);
                CWorld::pIgnoreEntity = nullptr;
                proj->SetUsesCollision(true);
                proj->m_pEntityIgnoredCollision = info.m_pCreator;
                if (isClear) {
                    return;
                }
            }

            // 0x739090
            if (proj->m_nNumEntitiesCollided == 0
                || !proj->m_apCollidedEntities[0]
                || (proj->m_apCollidedEntities[0] != info.m_pCreator && proj->m_apCollidedEntities[0]->m_nModelIndex != MODEL_MISSILE)
            ) {
                RemoveProjectile(&info, proj);
            }
        };

        if (static_cast<uint32>(info.m_nDestroyTime) < CTimer::GetTimeInMS() && info.m_nDestroyTime != 0) { // Note: the time is unsigned
            if (type == WEAPON_REMOTE_SATCHEL_CHARGE) {
                // BUG: `m_pCreator` isn't null-checked
                if ((info.m_pCreator || !notsa::IsFixBugs()) && info.m_pCreator->GetIsTypePed() && info.m_pCreator->AsPed()->IsPlayer()) {
                    auto* const ped = info.m_pCreator->AsPed();
                    // Player has no more detonator (or ammo for it) => detonate
                    if (ped->GetWeapon(WEAPON_DETONATOR).m_Type != WEAPON_DETONATOR || ped->GetWeapon(WEAPON_DETONATOR).m_TotalAmmo == 0) {
                        info.m_nDestroyTime = 0;
                    }
                }
            } else {
                RemoveProjectile(&info, proj);
            }
        } else {
            switch (type) {
            case WEAPON_ROCKET: {
                // Accelerate along the forward vector
                const auto ts      = CTimer::ms_fTimeStep * 0.008f;
                const auto& fwd    = proj->GetForward();
                const CVector accel{ ts * fwd.x, ts * fwd.y, ts * fwd.z };
                proj->m_vecMoveSpeed += accel;

                auto& vel = proj->m_vecMoveSpeed;
                const auto speed = std::sqrt((vel.x * vel.x + vel.y * vel.y) + vel.z * vel.z);
                if (speed > 9.9f) {
                    const auto scale = 9.9f / speed;
                    vel.x = scale * vel.x;
                    vel.y = scale * vel.y;
                    vel.z = scale * vel.z;
                }

                CheckRocketCollision();
                break;
            }
            case WEAPON_FLARE: {
                proj->SetUsesCollision(false);
                CWorld::pIgnoreEntity = info.m_pCreator;
                const bool isClear = CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, proj->GetPosition(), true, true, true, true, false, false, false);
                proj->SetUsesCollision(true);
                CWorld::pIgnoreEntity = nullptr;
                if (!isClear) {
                    proj->m_vecMoveSpeed = CVector{ 0.0f, 0.0f, 0.0f };
                    proj->GetPosition()  = info.m_vecLastPosn;
                }
                break;
            }
            case WEAPON_MOLOTOV:
            case WEAPON_FREEFALL_BOMB: {
                const CVector projPos = proj->GetPosition();
                CWorld::pIgnoreEntity = info.m_pCreator;
                proj->SetUsesCollision(false);

                bool doChecks = true;
                if (const auto* const creator = info.m_pCreator) {
                    const auto& creatorPos = creator->GetPosition();
                    const auto dx = info.m_vecLastPosn.x - creatorPos.x;
                    const auto dy = info.m_vecLastPosn.y - creatorPos.y;
                    const auto dz = info.m_vecLastPosn.z - creatorPos.z;
                    if ((dx * dx + dy * dy) + dz * dz < 2.0f) { // Close to the creator => don't check
                        doChecks = false;
                    }
                }
                if (doChecks) {
                    if (proj->physicalFlags.bOnSolidSurface
                        || !CWorld::GetIsLineOfSightClear(info.m_vecLastPosn, projPos, true, true, true, true, false, false, false)
                    ) {
                        RemoveProjectile(&info, proj);
                    }
                }

                CWorld::pIgnoreEntity = nullptr;
                proj->SetUsesCollision(true);
                break;
            }
            case WEAPON_ROCKET_HS: {
                if (!info.m_pVictim) {
                    CheckRocketCollision();
                    break;
                }

                if (info.m_pVictim == FindPlayerVehicle(-1, false)) {
                    AudioEngine.ReportFrontendAudioEvent(AE_MISSILE_LOCK, 0.0f, 1.0f);
                }

                const CVector fwd    = proj->GetForward();
                const CVector origin = proj->GetPosition() + fwd;

                // Find the best target: either the victim, or the best flare
                const float victimScore = CWeapon::EvaluateTargetForHeatSeekingMissile(info.m_pVictim, origin, fwd, 1.2f, true, nullptr);
                float       bestFlareScore = 0.0f;
                CEntity*    bestFlare      = nullptr;
                for (size_t k = 0; k < gaProjectileInfo.size(); k++) {
                    auto* const flare = ms_apProjectile[k];
                    if (gaProjectileInfo[k].m_nWeaponType != WEAPON_FLARE || !gaProjectileInfo[k].m_bActive) {
                        continue;
                    }
                    const float score = CWeapon::EvaluateTargetForHeatSeekingMissile(flare, origin, fwd, 1.2f, true, nullptr);
                    if (score >= bestFlareScore) {
                        bestFlareScore = score;
                        bestFlare      = flare;
                    }
                }
                CEntity* tgt = info.m_pVictim;
                if (bestFlare && bestFlareScore > victimScore) {
                    tgt = bestFlare;
                }

                // Is the player homing in on a plane?
                bool isHomingOnPlane = false;
                if (tgt->GetIsTypeVehicle()
                    && (info.m_pCreator == FindPlayerPed(-1) || info.m_pCreator == FindPlayerVehicle(-1, false))
                    && tgt->AsVehicle()->IsSubPlane()
                ) {
                    isHomingOnPlane = true;
                }

                auto& vel = proj->m_vecMoveSpeed;

                // Point the missile is aiming at: where it'll be in 100 frames
                const auto& projPos = proj->GetPosition();
                CVector aimAt{
                    vel.x * 100.0f + projPos.x,
                    vel.y * 100.0f + projPos.y,
                    vel.z * 100.0f + projPos.z,
                };
                if (isHomingOnPlane) {
                    aimAt = projPos;
                }

                // Distance to the target
                const auto& tgtPos = tgt->GetPosition();
                const auto dx = projPos.x - tgtPos.x;
                const auto dy = projPos.y - tgtPos.y;
                const auto dz = projPos.z - tgtPos.z;
                const auto dist = std::sqrt((dx * dx + dz * dz) + dy * dy);

                float lead = dist < 50.0f ? dist : 50.0f;
                if (isHomingOnPlane) {
                    lead = dist < 1.5f ? dist : 1.5f;
                }

                // Predicted position of the target
                const auto& tgtVel = tgt->AsPhysical()->m_vecMoveSpeed;
                const CVector predicted{
                    (lead * tgtVel.x) + tgtPos.x,
                    (lead * tgtVel.y) + tgtPos.y,
                    (lead * tgtVel.z) + tgtPos.z,
                };

                CVector toTarget = predicted - aimAt;

                CVector velDir = vel;
                velDir.Normalise();

                const float dot = toTarget.z * velDir.z + toTarget.y * velDir.y + toTarget.x * velDir.x;
                if (dot < 0.0f) { // Target is behind => remove the backwards component
                    toTarget.x = toTarget.x - velDir.x * dot;
                    toTarget.y = toTarget.y - velDir.y * dot;
                    toTarget.z = toTarget.z - velDir.z * dot;
                }
                toTarget.Normalise();

                float turnAccel = (info.m_pCreator == FindPlayerPed(-1) || info.m_pCreator == FindPlayerVehicle(-1, false))
                    ? HOMING_PLAYER_ACCEL
                    : 0.009f;
                if (tgt->AsPhysical()->m_vecMoveSpeed.Magnitude() > 0.8f) {
                    turnAccel *= 1.2f;
                }

                float damping = 1.0f;
                if (isHomingOnPlane) {
                    turnAccel = HOMING_PLANE_ACCEL;
                    damping   = std::pow(HOMING_PLANE_DAMPING, CTimer::ms_fTimeStep);
                }

                vel.x = damping * vel.x;
                vel.y = damping * vel.y;
                vel.z = damping * vel.z;

                turnAccel *= CTimer::ms_fTimeStep;
                vel.x = toTarget.x * turnAccel + vel.x;
                vel.y = toTarget.y * turnAccel + vel.y;
                vel.z = toTarget.z * turnAccel + vel.z;

                const auto speed = std::sqrt((vel.x * vel.x + vel.y * vel.y) + vel.z * vel.z);
                if (speed > 9.9f) {
                    const auto scale = 9.9f / speed;
                    vel.x = scale * vel.x;
                    vel.y = scale * vel.y;
                    vel.z = scale * vel.z;
                }

                proj->GetForward() = velDir; // Note: This is the direction of the velocity *before* it was changed

                CheckRocketCollision();
                break;
            }
            case WEAPON_REMOTE_SATCHEL_CHARGE: {
                // Attach to what we've hit
                if (proj->m_fDamageIntensity > 0.0f && proj->m_pDamageEntity && !proj->m_pAttachedTo) {
                    proj->AttachEntityToEntity(proj->m_pDamageEntity->AsPhysical(), static_cast<CVector*>(nullptr), static_cast<CQuaternion*>(nullptr));
                    proj->SetUsesCollision(false);
                }
                break;
            }
            default:
                break;
            }
        }

        if (notsa::IsFixBugs() && !info.m_bActive) { // The projectile was removed above
            continue;
        }

        info.m_vecLastPosn = proj->GetPosition(); // BUG: `proj` might have been deleted above (but the pool still contains the memory)
    }
}

// 0x739860
bool CProjectileInfo::IsProjectileInRange(float x1, float x2, float y1, float y2, float z1, float z2, bool bDestroy) {
    const CBox bb{
        CVector{ x1, y1, z1 },
        CVector{ x2, y2, z2 }
    };
    bool found = false;
    for (auto&& [info, proj] : rngv::zip(gaProjectileInfo, ms_apProjectile)) {
        if (!info.m_bActive) {
            continue;
        }

        if (!IsWeaponTypeProjectile(static_cast<eWeaponType>(info.m_nWeaponType))) {
            continue;
        }

        if (!bb.IsPointInside(proj->GetPosition())) {
            continue;
        }

        found = true;
        if (bDestroy) {
            info.m_bActive = false;
            info.RemoveFXSystem(false);
            CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(proj));
            CWorld::Remove(proj);
            delete proj;
        }
    }
    return found;
}

// 0x7399B0
void CProjectileInfo::RemoveAllProjectiles() {
    for (auto&& [info, proj] : rngv::zip(gaProjectileInfo, ms_apProjectile)) {
        if (!info.m_bActive) {
            continue;
        }

        info.m_bActive = false;
        info.RemoveFXSystem(true);
        CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(proj));
        CWorld::Remove(proj);
        delete proj;
    }
}

// 0x739A40
bool CProjectileInfo::RemoveIfThisIsAProjectile(CObject* object) {
    for (auto&& [info, proj] : rngv::zip(gaProjectileInfo, ms_apProjectile)) {
        if (proj != object || !info.m_bActive) {
            continue;
        }

        info.m_bActive = false;
        info.RemoveFXSystem(false);
        CRadar::ClearBlipForEntity(BLIP_OBJECT, GetObjectPool()->GetRef(proj));
        CWorld::Remove(proj);
        delete proj;
        proj = nullptr;
        return true;
    }
    return false;
}

// 0x737B80
void CProjectileInfo::RemoveFXSystem(bool bInstantly) {
    if (!m_pFxSystem) {
        return;
    }

    if (bInstantly) {
        g_fxMan.DestroyFxSystem(m_pFxSystem);
    } else {
        m_pFxSystem->Kill();
    }
    m_pFxSystem = nullptr;
}
