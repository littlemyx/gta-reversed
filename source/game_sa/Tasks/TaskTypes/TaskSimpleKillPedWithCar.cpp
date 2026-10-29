#include "StdInc.h"
#include "TaskSimpleKillPedWithCar.h"
#include "Enums/eDirection.h"

void CTaskSimpleKillPedWithCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleKillPedWithCar, 0x86f2c0, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x653C10);
    RH_ScopedInstall(Destructor, 0x653C90);

    RH_ScopedInstall(DamageCarBonnet, 0x653CF0);
    RH_ScopedVMTInstall(Clone, 0x655A20);
    RH_ScopedVMTInstall(GetTaskType, 0x653C70);
    RH_ScopedVMTInstall(MakeAbortable, 0x653C80);
    RH_ScopedVMTInstall(ProcessPed, 0x6564A0);
}

// 0x653C10
CTaskSimpleKillPedWithCar::CTaskSimpleKillPedWithCar(CVehicle* car, float impulseMag) :
    m_Car{car},
    m_ImpulseMag{impulseMag}
{
}

// 0x655A20
CTaskSimpleKillPedWithCar::CTaskSimpleKillPedWithCar(const CTaskSimpleKillPedWithCar& o) :
    CTaskSimpleKillPedWithCar{o.m_Car, o.m_ImpulseMag}
{
}

// 0x653CF0
void CTaskSimpleKillPedWithCar::DamageCarBonnet(CPed const* ped) {
    if (!m_Car->IsSubAutomobile()) {
        return;
    }

    const auto flyingObj = m_Car->AsAutomobile()->RemoveBonnetInPedCollision();
    if (!flyingObj) {
        return;
    }

    // 0x653D3B-0x653E2A: the bonnet takes the *ped's* move speed (not its own) +/- (right * 0.1) + (up * 0.5);
    // the coin flip only flips the right-vector term: the up term is added in both cases. Operation order as in the exe.
    const auto& right = m_Car->GetRight();
    const auto& up    = m_Car->GetUp();
    const auto& pv    = ped->m_vecMoveSpeed;
    if (CGeneral::RandomOverHalf()) { // exe 0x653D17: rand() / 32767.0f > 0.5f
        flyingObj->m_vecMoveSpeed = CVector{
            (pv.x + right.x * 0.1f) + up.x * 0.5f,
            (pv.y + right.y * 0.1f) + up.y * 0.5f,
            (pv.z + right.z * 0.1f) + up.z * 0.5f,
        };
    } else {
        flyingObj->m_vecMoveSpeed = CVector{
            (pv.x - right.x * 0.1f) + up.x * 0.5f,
            (pv.y - right.y * 0.1f) + up.y * 0.5f,
            (pv.z - right.z * 0.1f) + up.z * 0.5f,
        };
    }

    flyingObj->ApplyTurnForce(m_Car->GetUp() * 10.f, m_Car->GetForward());
}

// 0x6564A0
bool CTaskSimpleKillPedWithCar::ProcessPed(CPed* ped) {
    const auto  carPos        = m_Car->GetPosition();
    const auto  carCM         = m_Car->GetColModel();   
    const auto& carBB         = carCM->GetBoundingBox();
    const auto& carVel        = m_Car->GetMoveSpeed();
    const auto& carFwd        = m_Car->GetForward();
    // 0x65651E-0x656520 (fz*vz + fy*vy) + fx*vx; tested `> 0` at 0x65660B (the exe treats a zero dot as "going backwards")
    const auto  carIsGoingFwd = (carFwd.z * carVel.z + carFwd.y * carVel.y) + carFwd.x * carVel.x > 0.f;

    const auto pedPos     = ped->GetPosition();
    const auto pedToCar   = pedPos - carPos;
    // Ped's position in the Car's object space: 0x6564FE-0x65655A, dot products with the car's axes (right, forward, up), summed z-term first
    const auto& carRight = m_Car->GetRight();
    const auto& carUp    = m_Car->GetUp();
    const CVector pedPosOS{
        (pedToCar.z * carRight.z + pedToCar.y * carRight.y) + pedToCar.x * carRight.x,
        (pedToCar.z * carFwd.z   + pedToCar.y * carFwd.y)   + pedToCar.x * carFwd.x,
        (pedToCar.z * carUp.z    + pedToCar.y * carUp.y)    + pedToCar.x * carUp.x,
    };
    auto       pedHitSide = (eDirection)CPedGeometryAnalyser::ComputePedHitSide(*ped, *m_Car);

    eWeaponType killWT = WEAPON_RAMMEDBYCAR;
    enum {
        FROM_LEFT = 1,
        FROM_RIGHT = 3,
        FROM_BACK = 4,
        FROM_FRONT = 5
    } carDir{};
    if (std::abs(pedPosOS.x) > carBB.m_vecMax.x * 0.99f) { // 0x6565D5 - Ped is on left/right (This assumes abs(bb.max.x) == abs(bb.min.x))
        carDir = pedPosOS.x < 0.f
            ? FROM_LEFT
            : FROM_RIGHT;
        if (std::abs(pedPosOS.y) < std::abs((carIsGoingFwd ? carBB.m_vecMax.y : carBB.m_vecMin.y) * 0.85f)) { // Did we run over the ped?
            killWT = WEAPON_RUNOVERBYCAR;

            ped->m_vecMoveSpeed = CVector{CVector2D{carVel} * 0.9f};
            if (CLocalisation::KnockDownPeds()) {
                m_Car->m_vehicleAudio.AddAudioEvent(AE_PED_CRUNCH, 0.f);
            }

            switch (pedHitSide) { // 0x6566A7
            case eDirection::LEFT:
            case eDirection::RIGHT:
                pedHitSide = eDirection::FORWARD;
            }
        }
    } else if (pedPosOS.z > 0.1f) { // 0x6566CB - Ped is on the bonnet
        carDir = FROM_BACK;

        const auto ApplyMoveSpeed = [&](float x, float w) {
            const auto t = (x - pedPos.z) / (w / m_Car->GetMoveSpeed().Magnitude());
            ped->m_vecMoveSpeed = m_Car->GetMoveSpeed().Normalized() * t;
        };

        if (carFwd.z < -0.2f) { // (Inverted) - Back is lifted (0x6566EC, strict)
            ApplyMoveSpeed(
                carBB.m_vecMin.y * carFwd.z + carBB.m_vecMax.z * m_Car->GetUp().z + carPos.z, // 0x6566EE-0x6566FE: max.z (was max.x)
                carIsGoingFwd
                    ? carBB.m_vecMax.y - carBB.m_vecMin.y
                    : -carBB.m_vecMin.y
            );
        } else if (carFwd.z > 0.1f) { // 0x65671F (strict)
            ApplyMoveSpeed(
                carBB.m_vecMax.y * carFwd.z + carBB.m_vecMax.z * m_Car->GetUp().z + carPos.z, // 0x656730
                carIsGoingFwd // 0x65673F-0x656754: picked by the direction of travel (was `max.y <= 0`)
                    ? carBB.m_vecMax.y
                    : carBB.m_vecMax.y - carBB.m_vecMin.y
            );
        } else {
            ApplyMoveSpeed(
                carBB.m_vecMax.z * m_Car->GetUp().z + carPos.z, // 0x65675E
                carIsGoingFwd
                    ? carBB.m_vecMax.y
                    : -carBB.m_vecMin.y
            );
        }

        DamageCarBonnet(ped);
    } else { // 0x656824 - the exe sets carDir = 5 (FROM_FRONT) and the RUNOVERBYCAR weapon here (was FROM_BACK + RAMMEDBYCAR)
        carDir = FROM_FRONT;
        killWT = WEAPON_RUNOVERBYCAR;

        // Copy paste
        ped->m_vecMoveSpeed = CVector{CVector2D{carVel} * 0.9f};
        if (CLocalisation::KnockDownPeds()) {
            m_Car->m_vehicleAudio.AddAudioEvent(AE_PED_CRUNCH, 0.f);
        }

        switch (pedHitSide) {
        case eDirection::LEFT:
        case eDirection::RIGHT:
            pedHitSide = eDirection::FORWARD;
        }
    }

    GetEventGlobalGroup()->Add(CEventVehicleHitAndRun{ ped, m_Car });

    // 0x6568E2

    CPedDamageResponseCalculator dmgRespCalc{m_Car, 1000.f, killWT, PED_PIECE_HEAD, false};

    const auto ppiece = [&]{
        switch (carDir) {
        case FROM_LEFT:  return PED_PIECE_LEFT_ARM;
        case FROM_RIGHT: return PED_PIECE_RIGHT_ARM;
        case FROM_BACK:  return PED_PIECE_ASS;
        case FROM_FRONT: return PED_PIECE_HEAD;
        default:         NOTSA_UNREACHABLE_RET(PED_PIECE_ASS);
        }
    }();
    CEventDamage eventDmg{m_Car, CTimer::GetTimeInMS(), killWT, ppiece, (uint8)pedHitSide, false, ped->bInVehicle};
    eventDmg.ComputeDamageResponseIfAffectsPed(ped, dmgRespCalc, true);
    ped->GetEventGroup().Add(&eventDmg);

    if (FindPlayerVehicle() == m_Car) {
        // 0x6563F8-0x656424: speed * (1/mass) (not speed / mass) * 200000 + 80
        const auto invMass = 1.f / m_Car->GetMass();
        const auto f = NOTSA_FTOL(uint32, std::min(250.f, m_Car->GetMoveSpeed().Magnitude() * invMass * 200'000.f + 80.f));
        CPad::GetPad()->StartShake(40'000 / (f % 256), f, 0);
    }

    ped->bIsStanding       = false;
    ped->bKnockedUpIntoAir = carDir == FROM_BACK;

    m_Car->ApplyMoveForce(pedToCar.Normalized() * (std::min(1.f, m_Car->GetMass() * ExeRecip(1400.f)) * -100.f)); // 0x656A29: scalar (min * -100) first, then each component

    return true;
}
