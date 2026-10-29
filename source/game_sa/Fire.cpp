#include "StdInc.h"

#include "Fire.h"
#include "FireManager.h"
#include "CreepingFire.h"

// The exe's distance chain (0x53A941 / 0x53AA58): sqrt((dz*dz + dy*dy) + dx*dx) kept on the x87 stack
static x87d DistZYX(const CVector& a, const CVector& b) {
    const x87d dx = (x87d)a.x - (x87d)b.x;
    const x87d dy = (x87d)a.y - (x87d)b.y;
    const x87d dz = (x87d)a.z - (x87d)b.z;
    return std::sqrt((dz * dz + dy * dy) + dx * dx);
}

void CFire::InjectHooks() {
    RH_ScopedClass(CFire);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x539D90);
    RH_ScopedInstall(Initialise, 0x538B30);
    RH_ScopedInstall(CreateFxSysForStrength, 0x539360);
    RH_ScopedInstall(Extinguish, 0x5393F0);
    RH_ScopedInstall(ProcessFire, 0x53A570);
}

// 0x539D90
CFire::CFire() {
    Initialise();
}

CFire* CFire::Constructor() {
    this->CFire::CFire();
    return this;
}

// 0x538B30
void CFire::Initialise() {
    // Originally m_nFlags = (m_nFlags & 0xF4) | 0x14; - Clear 1st, 2nd, 4th and set 3rd, 5th bits (1-based numbering)
    m_IsActive = false;
    m_IsCreatedByScript = false;
    m_MakesNoise = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration = true;
    m_Position = CVector{};
    m_ScriptReferenceIndex = 1;
    m_TimeToBurn = 0;
    m_EntityOnFire = nullptr;
    m_EntityStartedFire = nullptr;
    m_Strength = 1.0f;
    m_FxSystem = nullptr;
    m_NumGenerationsAllowed = 100;
    m_RemovalDist = 60;
}

void CFire::ExtinguishWithWater(float fWaterStrength) {
    const float fOriginalStrength = m_Strength;
    m_Strength -= fWaterStrength * CTimer::GetTimeStepInSeconds();

    /* Create particles */
    CVector particlePos = m_Position + CVector{
        CGeneral::GetRandomNumberInRange(-1.28f, 1.28f),
        CGeneral::GetRandomNumberInRange(-1.28f, 1.28f),
        CGeneral::GetRandomNumberInRange(-0.64f, 0.64f)
        /* Original code:
        (float)((CGeneral::GetRandomNumber() % 256) - 128) / 100.0f,
        (float)((CGeneral::GetRandomNumber() % 256) - 128) / 100.0f,
        (float)((CGeneral::GetRandomNumber() % 256) - 128) / 200.0f
        */
    };
    FxPrtMult_c prtMult{ 1.0f, 1.0f, 1.0f, 0.6f, 0.75f, 0.0f, 0.4f };
    const auto AddParticle = [&](CVector velocity) {
        g_fx.m_SmokeII3expand->AddParticle(particlePos, velocity, 0.0f, prtMult);
    };
    /* The two particles only differ in velocity */
    AddParticle({ 0.0f, 0.0f, 0.8f });
    AddParticle({ 0.0f, 0.0f, 1.4f });

    /* Re-create fx / extinguish */
    m_IsBeingExtinguished = true;
    if (m_Strength >= 0.0f) {
        if (NOTSA_FTOL(intmax_t, fOriginalStrength) != NOTSA_FTOL(intmax_t, m_Strength)) { /* Check if integer part has changed */
            CreateFxSysForStrength(m_Position, nullptr); /* Yes, so needs a new fx */
        }
    } else {
        Extinguish();
    }
}

// see 0x539F00 CFireManager::StartFire
void CFire::Start(CEntity* creator, CVector pos, uint32 nTimeToBurn, uint8 nGens) {
    m_IsActive = true;
    m_IsCreatedByScript = false;
    m_MakesNoise = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration = true;

    // 0x539F87-0x539FD2: rand() * (1/32767) * 0.299999952 + 1.0 stays in the x87 register (no float rounding), * nTimeToBurn + the time, then _ftol
    {
        const x87d factor = (x87d)CGeneral::GetRandomNumber() * (x87d)RAND_MAX_FLOAT_RECIPROCAL * (x87d)0.299999952f + (x87d)1.0f;
        m_TimeToBurn = NOTSA_FTOL(uint32_t, factor * (x87d)nTimeToBurn + (x87d)CTimer::GetTimeInMS());
    }

    SetEntityOnFire(nullptr);
    SetEntityStartedFire(creator);

    m_NumGenerationsAllowed = nGens;
    m_Strength = 1.0f;
    m_Position = pos;

    CreateFxSysForStrength(m_Position, nullptr);
}

// see 0x53A050 CFireManager::StartFire
void CFire::Start(CEntity* creator, CEntity* target, uint32 nTimeToBurn, uint8 nGens) {
    switch (target->GetType()) {
    case ENTITY_TYPE_PED: {
        auto targetPed = target->AsPed();
        targetPed->m_pFire = this;
        if (creator) { // 0x53A114: no crime without a creator
            CCrime::ReportCrime(
                targetPed->m_nPedType == PED_TYPE_COP ? eCrimeType::CRIME_SET_COP_PED_ON_FIRE : eCrimeType::CRIME_SET_PED_ON_FIRE,
                targetPed,
                creator->AsPed()
            );
        }
        break;
    }
    case ENTITY_TYPE_VEHICLE: {
        auto targetVehicle = target->AsVehicle();
        targetVehicle->m_pFire = this;
        if (creator) { // 0x53A149
            CCrime::ReportCrime(
                eCrimeType::CRIME_SET_CAR_ON_FIRE,
                targetVehicle,
                creator->AsPed()
            );
        }
        break;
    }
    case ENTITY_TYPE_OBJECT: {
        target->AsObject()->m_pFire = this;
        break;
    }
    }

    m_IsActive = true;
    m_IsCreatedByScript = false;
    m_MakesNoise = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration = true;

    m_NumGenerationsAllowed = nGens;
    m_Strength = 1.0f;
    m_Position = target->GetPosition();

    if (target->GetIsTypePed() && target->AsPed()->IsPlayer())
        m_TimeToBurn = CTimer::GetTimeInMS() + 2333;
    else if (target->GetIsTypeVehicle())
        m_TimeToBurn = CTimer::GetTimeInMS() + CGeneral::GetRandomNumberInRange(0, 1000) + 3000;
    else
        m_TimeToBurn = CTimer::GetTimeInMS() + CGeneral::GetRandomNumberInRange(0, 1000) + nTimeToBurn;

    SetEntityOnFire(target);
    SetEntityStartedFire(creator);

    CreateFxSysForStrength(m_Position, nullptr);
}

// see 0x53A270 CFireManager::StartScriptFire
void CFire::Start(CVector pos, float fStrength, CEntity* target, uint8 nGens) {
    SetEntityOnFire(target);
    SetEntityStartedFire(nullptr);

    m_IsActive = true;
    m_IsCreatedByScript = true;
    m_MakesNoise = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration = true;

    m_NumGenerationsAllowed = nGens;
    m_Strength = fStrength;
    m_Position = pos;

    if (target) {
        switch (target->GetType()) { /* Set target's `m_pFire` to `this` */
        case ENTITY_TYPE_PED:
            target->AsPed()->m_pFire = this;
            break;
        case ENTITY_TYPE_VEHICLE:
            target->AsVehicle()->m_pFire = this;
            break;
        }
    }

    // 0x53A3A7-0x53A416: the fx is created only without a target (at pos) or for a ped / vehicle target (at its position), not for other entity types
    if (!target) {
        CreateFxSysForStrength(pos, nullptr);
    } else if (target->GetIsTypePed() || target->GetIsTypeVehicle()) {
        CVector targetPos = target->GetPosition();
        CreateFxSysForStrength(targetPos, nullptr);
    }
}

void CFire::SetEntityOnFire(CEntity* target) {
    CEntity::SafeCleanUpRef(m_EntityOnFire); /* Assume old target's m_pFire is not pointing to `*this` */

    m_EntityOnFire = target; /* assign, even if its null, to clear it */
    CEntity::SafeRegisterRef(m_EntityOnFire); /* Assume caller set target->m_pFire */
}

void CFire::SetEntityStartedFire(CEntity* creator) {
    CEntity::SafeCleanUpRef(m_EntityStartedFire);

    m_EntityStartedFire = creator; /* assign, even if its null, to clear it */
    CEntity::SafeRegisterRef(m_EntityStartedFire);
}

void CFire::DestroyFxNow() {
    if (m_FxSystem) {
        g_fxMan.DestroyFxSystem(m_FxSystem);
        m_FxSystem = nullptr;
    }
}

void CFire::DestroyFx() {
    if (m_FxSystem) {
        m_FxSystem->Kill();
        m_FxSystem = nullptr;
    }
}

auto CFire::GetFireParticleNameForStrength() const {
    if (m_Strength > 1.0f)
        return (m_Strength > 2.0f) ? "fire_large" : "fire_med";
    else
        return "fire";
};

// 0x539360
void CFire::CreateFxSysForStrength(const CVector& point, RwMatrix* matrix) {
    DestroyFx();
    m_FxSystem = g_fxMan.CreateFxSystem(GetFireParticleNameForStrength(), point, matrix, true);
    if (m_FxSystem)
        m_FxSystem->Play();
}

// 0x5393F0
void CFire::Extinguish() {
    if (!m_IsActive)
        return;

    m_TimeToBurn = 0;

    // Originally m_nFlags = (m_nFlags & 0xF6) | 0x10; - Clear 1st and 4th, set 5th
    m_IsActive = false;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration = true;

    DestroyFx();

    if (m_EntityOnFire) {
        switch (m_EntityOnFire->GetType()) {
        case ENTITY_TYPE_PED: {
            m_EntityOnFire->AsPed()->m_pFire = nullptr;
            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            m_EntityOnFire->AsVehicle()->m_pFire = nullptr;
            break;
        }
        }
        CEntity::ClearReference(m_EntityOnFire);
    }
}

// 0x53A570
void CFire::ProcessFire() {
    {
        // 0x53A578-0x53A5BC: the strength only grows while the integer part stays the same (fst new; _ftol(new) == _ftol(old) ? keep new : restore old)
        const float sum         = m_Strength + CTimer::GetTimeStep() * ExeRecip(500.0f);
        const float newStrength = (3.0f < sum) ? 3.0f : sum; // fcomp + jp: a NaN sum stays NaN
        if (NOTSA_FTOL(uint32, m_Strength) == NOTSA_FTOL(uint32, newStrength)) {
            m_Strength = newStrength;
        }
    }

    if (m_EntityOnFire) {
        m_Position = m_EntityOnFire->GetPosition();

        switch (m_EntityOnFire->GetType()) {
        case ENTITY_TYPE_PED: {
            auto targetPed = m_EntityOnFire->AsPed();

            if (targetPed->m_pFire != this) {
                Extinguish();
                return;
            }

            switch (targetPed->m_nPedState) {
            case PEDSTATE_DIE:
            case PEDSTATE_DEAD: {
                m_Position.z -= 1.0f; /* probably because ped is laying on the ground */
                break;
            }
            }

            if (auto vehicle = targetPed->GetVehicleIfInOne()) {
                if (!ModelIndices::IsFireTruck(vehicle->m_nModelIndex) && vehicle->IsAutomobile()) {
                    vehicle->m_fHealth = 75.0f;
                }
            } else if (!targetPed->IsPlayer() && !targetPed->IsAlive()) {
                targetPed->physicalFlags.bRenderScorched = true;
            }

            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            auto targetVehicle = m_EntityOnFire->AsVehicle();

            if (targetVehicle->m_pFire != this) {
                Extinguish();
                return;
            }

            if (!m_IsCreatedByScript) {
                targetVehicle->InflictDamage(m_EntityStartedFire, eWeaponType::WEAPON_FLAMETHROWER, CTimer::GetTimeStep() * 1.2f, CVector{});
            }

            if (targetVehicle->IsAutomobile()) {
                // 0x53A709-0x53A760: the 0.15 is added to the model-space dummy position, the sum is then transformed by the vehicle matrix
                m_Position = targetVehicle->GetMatrix().TransformPoint(targetVehicle->GetDummyPositionObjSpace(eVehicleDummy::DUMMY_LIGHT_FRONT_MAIN) + CVector{0.0f, 0.0f, 0.15f});
            }
            break;
        }
        }

        if (m_FxSystem) {
            auto targetPhysical = m_EntityOnFire->AsPhysical();
            m_FxSystem->SetOffsetPos(m_Position + CTimer::GetTimeStep() * 2.0f * targetPhysical->m_vecMoveSpeed);
        }
    }

    CPlayerPed* player = FindPlayerPed();
    if (!m_EntityOnFire || !m_EntityOnFire->GetIsTypeVehicle()) {
        // Check if we can set player's ped on fire
        if (!FindPlayerVehicle()
         && !player->m_pFire /* not already on fire */
         && !player->physicalFlags.bFireProof
         && !player->m_pAttachedTo
         ) {
            const CVector toPlayer = player->GetPosition() - m_Position;
            if ((toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z) + toPlayer.y * toPlayer.y < 1.2f) { /* Note: Squared distance, 0x53A859: (x*x + z*z) + y*y */
                player->DoStuffToGoOnFire();
                gFireManager.StartFire(player, m_EntityStartedFire, 0.8f, true, 7000, 100);
            }
        }
    }

    if (CGeneral::GetRandomNumber() % 32 == 0) {
        for (auto& veh : NOTSA_EXE_OR(GetVehiclePool()->GetAllValidReverse(), GetVehiclePool()->GetAllValid())) { // NOTSA: Original loop was backwards [not that it matters] (it does: the order decides the fire slots / RNG use; the run build iterates backwards like the exe, 0x53A8D2; NOTSA-AUDIT)
            if (veh.m_pFire || ModelIndices::IsFireTruck(veh.m_nModelIndex)) { // 0x53A918, 0x53A926
                continue;
            }
            if (!(DistZYX(veh.GetPosition(), m_Position) < (x87d)2.0)) // 0x53A941: (dz*dz + dy*dy) + dx*dx, NaN skips
                continue;

            if (veh.IsSubBMX()) {
                player->DoStuffToGoOnFire();
                gFireManager.StartFire(player, m_EntityStartedFire, 0.8f, true, 7000, 100);
                veh.BurstTyre(CAR_PIECE_FIRST_WHEEL + +veh.FindTyreNearestPoint(m_Position), false);
            } else {
                gFireManager.StartFire(&veh, m_EntityStartedFire, 0.8f, true, 7000, 0); // 0x53A9E5: no generations
            }
        }
    }

    if (CGeneral::GetRandomNumber() % 4 == 0) {
        for (auto& obj : NOTSA_EXE_OR(GetObjectPool()->GetAllValidReverse(), GetObjectPool()->GetAllValid())) { // NOTSA: Original loop was backwards [not that it matters] (run build: backwards like the exe, 0x53AA37; NOTSA-AUDIT)
            if (!(DistZYX(obj.GetPosition(), m_Position) < (x87d)3.0)) // 0x53AA58: (dz*dz + dy*dy) + dx*dx, NaN skips
                continue;

            obj.ObjectFireDamage(CTimer::GetTimeStep() * 8.0f, m_EntityStartedFire);
        }
    }

    if ((int8)m_NumGenerationsAllowed > 0 && CGeneral::GetRandomNumber() % 128 == 0) { // 0x53AAB0: signed byte test
        if (gFireManager.GetNumOfFires() < 25) {
            // 0x53AB39-0x53AC1A: the FIRST random number gives dir.y, the second dir.x; the direction is normalised; z gets +2.0; the child has one generation less
            const float rndY = CGeneral::GetRandomNumberInRange(-1.0f, 1.0f);
            const float rndX = CGeneral::GetRandomNumberInRange(-1.0f, 1.0f);
            CVector dir{ rndX, rndY, 0.0f };
            dir.Normalise();
            const float scale = CGeneral::GetRandomNumberInRange(2.0f, 3.0f);
            const CVector pos{ dir.x * scale + m_Position.x, dir.y * scale + m_Position.y, (dir.z * scale + m_Position.z) + 2.0f };
            CCreepingFire::TryToStartFireAtCoors(pos, (uint8)(m_NumGenerationsAllowed - 1), false, IsScript(), 10.0f);
        }
    }

    if (m_Strength <= 2.0f && m_NumGenerationsAllowed && CGeneral::GetRandomNumber() % 16 == 0) {
        CFire& nearby = gFireManager.GetRandomFire();
        if (&nearby != this && nearby.m_IsActive && !nearby.m_IsCreatedByScript && nearby.m_Strength <= 1.0f) {
            if (DistanceBetweenPoints(nearby.m_Position, m_Position) < 3.5f) {
                m_Position = m_Position * 0.7f + nearby.m_Position * 0.3f; // 0x53ACE2-0x53AD2F: moves THIS fire towards the other one
                m_Strength += 1.0f;
                m_TimeToBurn = std::max(m_TimeToBurn, CTimer::GetTimeInMS() + 7000);
                CreateFxSysForStrength(m_Position, nullptr);
                m_NumGenerationsAllowed = (uint8)std::min((int8)m_NumGenerationsAllowed, (int8)nearby.m_NumGenerationsAllowed); // 0x53AD57: signed min
                nearby.Extinguish();
            }
        }
    }

    if (m_FxSystem) {
        // 0x53AD79-0x53ADD5: min(strength - (int)strength, (float)(uint32)(m_TimeToBurn - now) / 3500) (the remaining burn time, not the absolute time)
        const x87d  fract = (x87d)m_Strength - (x87d)NOTSA_FTOL(int32, m_Strength);
        const int32 left  = (int32)(m_TimeToBurn - CTimer::GetTimeInMS());
        x87d        leftD = (x87d)left;
        if (left < 0) {
            leftD += (x87d)4294967296.0;
        }
        const float burn = (float)(leftD * ExeRecip(3500.0f));
        m_FxSystem->SetConstTime(true, (fract < (x87d)burn) ? (float)fract : burn);
    }

    if (m_IsCreatedByScript || (HasTimeToBurn() && IsNotInRemovalDistance())) {
        const float fColorRG = (float)(CGeneral::GetRandomNumber() % 128) / 512.0f; // todo: GetRandomNumberInRange
        CPointLights::AddLight(ePointLightType::PLTYPE_POINTLIGHT, m_Position, CVector{}, 8.0f, fColorRG, fColorRG, 0.0f, 0, false, nullptr);
    } else {
        if (m_Strength <= 1.0f) {
            Extinguish();
        } else {
            m_Strength -= 1.0f;
            m_TimeToBurn = CTimer::GetTimeInMS() + 7000;
            CreateFxSysForStrength(m_Position, nullptr);
        }
    }
}

bool CFire::HasTimeToBurn() const {
    return CTimer::GetTimeInMS() < m_TimeToBurn;
}

bool CFire::IsNotInRemovalDistance() const {
    // 0x53ADE5-0x53AE26: the exe measures the distance in the XY plane only: sqrt(dy*dy + dx*dx)
    const auto& cam = TheCamera.GetPosition();
    const x87d  dx  = (x87d)cam.x - (x87d)m_Position.x;
    const x87d  dy  = (x87d)cam.y - (x87d)m_Position.y;
    return (x87d)m_RemovalDist > std::sqrt(dy * dy + dx * dx);
}
