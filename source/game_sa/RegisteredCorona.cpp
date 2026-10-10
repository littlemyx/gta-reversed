#include "StdInc.h"

#include "RegisteredCorona.h"
#include "Fx/FxFtol.h"

void CRegisteredCorona::InjectHooks() {
    RH_ScopedClass(CRegisteredCorona);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Update, 0x6FABF0);
}

// 0x6FABF0
void CRegisteredCorona::Update() {
    if (!m_bRegisteredThisFrame) {
        m_Color.a = 0;
    }

    if (m_bAttached && !m_pAttachedTo) {
        m_dwId = 0;
        --CCoronas::NumCoronas;
        return;
    }

    const auto fadeStep = CTimer::GetTimeStep() * m_fFadeSpeed;
    const auto& camPos = TheCamera.GetPosition();
    if (!m_bCheckObstacles || ((!CCoronas::SunBlockedByClouds || m_dwId != 2)
        && CWorld::GetIsLineOfSightClear(m_vPosn, camPos, true, false, false, false, false, false, false))) {
        if (m_bOffScreen || (m_bOnlyFromBelow && camPos.z > m_vPosn.z)) {
            m_FadedIntensity = (uint8)(std::max(0.f, (float)(m_FadedIntensity)-fadeStep));
        } else {
            // 0x6FACC1: step towards the target intensity; NaN takes the stepped value (std::min / max would differ); equal values change nothing (not even the immediate update)
            const uint8 faded = m_FadedIntensity, target = m_Color.a;
            if (target > faded) {
                const float next = (float)((double)faded + (double)CTimer::GetTimeStep() * m_fFadeSpeed);
                m_FadedIntensity = (uint8)notsa::detail::Ftol((double)target < (double)next ? (double)target : (double)next);
                if (CCoronas::bChangeBrightnessImmediately) {
                    m_FadedIntensity = target;
                }
            } else if (target < faded) {
                const float next = (float)((double)faded - (double)CTimer::GetTimeStep() * m_fFadeSpeed);
                m_FadedIntensity = (uint8)notsa::detail::Ftol((double)target > (double)next ? (double)target : (double)next);
                if (CCoronas::bChangeBrightnessImmediately) {
                    m_FadedIntensity = target;
                }
            }

            if (m_dwId == 2) {
                const float v = (float)((double)CCoronas::LightsMult - (double)CTimer::GetTimeStep() * (double)std::bit_cast<float>(0x3D75C28Fu)); // 0x859934: 0.06
                CCoronas::LightsMult = (0.6f <= v || v != v) ? v : 0.6f;
            }
        }
    } else {
        m_FadedIntensity = (uint8)(std::max(0.f, (float)(m_FadedIntensity)-fadeStep));
    }

    if (!m_FadedIntensity && !m_bJustCreated) {
        m_dwId = 0;
        --CCoronas::NumCoronas;
    }

    m_bJustCreated = false;
    m_bRegisteredThisFrame = false;
}

//! Calculate the position to use for rendering
auto CRegisteredCorona::GetPosition() const -> CVector {
    if (!m_pAttachedTo) {
        return m_vPosn;
    }
    if (m_pAttachedTo->GetIsTypeVehicle() && m_pAttachedTo->AsVehicle()->IsSubBike()) {
        return m_pAttachedTo->AsBike()->m_mLeanMatrix.TransformPoint(m_vPosn);
    }
    return m_pAttachedTo->GetMatrix().TransformPoint(m_vPosn);
}

auto CRegisteredCorona::CalculateIntensity(float scrZ, float farClip) const -> float {
    return std::clamp(invLerp(farClip, farClip / 2.f, scrZ), 0.f, 1.f);
}
