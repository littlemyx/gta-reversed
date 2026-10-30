#include "StdInc.h"

#include "HandShaker.h"

void CHandShaker::InjectHooks() {
    RH_ScopedClass(CHandShaker);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(SetDefaults, 0x517330);
    RH_ScopedInstall(Reset, 0x50D860);
    RH_ScopedInstall(Process, 0x50D930);
}

// 0x517330
void CHandShaker::SetDefaults() {
    m_lim.Set(0.02f, 0.02f, 0.01f);
    m_motion.Set(0.0002f, 0.0002f, 0.0001f);
    m_slow.Set(1.3f, 1.3f, 1.4f);

    m_scaleReactionMin = 0.3f;
    m_scaleReactionMax = 1.f;
    m_twitchFreq       = 20;
    m_twitchVel        = 0.001f;

    Reset();
}

// 0x50D860
void CHandShaker::Reset() {
    m_ang = CVector();
    // 0x50D860: three rand() draws in the order z, y, x (CVector::Random draws x, y, z). The z and y draws are rounded to a float (spilled) before the multiply
    // by the motion limit, the x draw stays in extended precision (rand * 1/32767 * motion.x)
    const auto k  = (x87d)RAND_MAX_FLOAT_RECIPROCAL;
    const auto rz = (float)((x87d)CGeneral::GetRandomNumber() * k);
    const auto ry = (float)((x87d)CGeneral::GetRandomNumber() * k);
    const auto rx = (x87d)CGeneral::GetRandomNumber() * k;
    m_vel.x = (float)(rx * (x87d)m_motion.x);
    m_vel.y = (float)((x87d)ry * (x87d)m_motion.y);
    m_vel.z = (float)((x87d)rz * (x87d)m_motion.z);
}

// 0x50D930
void CHandShaker::Process(float degree) {
    // Process velocity on each axis
    for (auto i = 0; i < 3; i++) {
        //> 0x50D951 - Calculate scale [of motion]
        auto scale = std::lerp(m_scaleReactionMin, m_scaleReactionMax, std::abs(m_ang[i] / m_lim[i]));
        if (std::signbit(m_ang[i]) == std::signbit(m_vel[i])) {
            scale *= m_slow[i];
        }

        //> 0x50DA61 - Calculate random motion using it
        float motion = CGeneral::GetRandomNumberInRange(0.f, 1.f) * m_motion[i] * scale;
        if (m_ang[i] > 0.f) {
            motion = -motion;
        }

        //> 0x50DB29
        m_vel[i] += motion;
    }

    const auto twitchT = (float)NOTSA_FTOL(size_t, CTimer::GetTimeStepNonClippedInMS()) * std::bit_cast<float>(0x3CF5C290u); // 0x85A6EC (one ulp above the literal 0.03f)
    if (CGeneral::GetRandomNumberInRange(NOTSA_FTOL(size_t, (float)m_twitchFreq * twitchT)) == 1) { //> 0x50DB97
        m_vel += CVector::Random(-m_twitchVel, m_twitchVel);
    }
    m_ang += twitchT * m_vel; //> 0x50DC74
    m_ang  = Clamp(m_ang, -m_lim, m_lim); //> 0x50DCAF

    m_resultMat.SetRotate(m_ang * degree); //> 0x50DD40
}
