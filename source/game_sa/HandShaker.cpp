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
    // 0x50D936-0x50D97D: scale[i] = |ang / lim| * (max - min) + min (every op rounds to a float, PC=24)
    const float span = m_scaleReactionMax - m_scaleReactionMin;
    float scale[3];
    for (auto i = 0; i < 3; i++) {
        scale[i] = std::abs(m_ang[i] / m_lim[i]) * span + m_scaleReactionMin;
    }
    // 0x50D97F-0x50DA53: scale *= slow only when ang and vel are BOTH strictly positive or BOTH strictly negative
    // (the port used signbit equality, which multiplied for a zero angle / zero velocity)
    for (auto i = 0; i < 3; i++) {
        if ((m_ang[i] > 0.f && m_vel[i] > 0.f) || (m_ang[i] < 0.f && m_vel[i] < 0.f)) {
            scale[i] *= m_slow[i];
        }
    }

    //> 0x50DA61-0x50DB29: three draws in the order z, y, x (the port drew x, y, z); motion = r * m_motion * scale, negated while ang > 0; then vel += motion
    float motion[3];
    for (auto i = 2; i >= 0; i--) {
        const float r = static_cast<float>(CGeneral::GetRandomNumber()) * RAND_MAX_FLOAT_RECIPROCAL;
        motion[i]     = r * m_motion[i] * scale[i];
    }
    for (auto i = 0; i < 3; i++) {
        if (m_ang[i] > 0.f) {
            motion[i] = -motion[i];
        }
        m_vel[i] += motion[i];
    }

    const auto twitchT = (float)NOTSA_FTOL(size_t, CTimer::GetTimeStepNonClippedInMS()) * std::bit_cast<float>(0x3CF5C290u); // 0x85A6EC (one ulp above the literal 0.03f)
    // 0x50DB65-0x50DB9D: twitch fires when GetRandomNumberInRange(1, ftol(twitchFreq * twitchT)) == 2, i.e. 1 + (int)(r / 32768 * (N - 1)) == 2
    // (the port tested a one-argument range == 1, i.e. (int)(r / 32768 * N))
    if (CGeneral::GetRandomNumberInRange(1, NOTSA_FTOL(int32, (float)m_twitchFreq * twitchT)) == 2) {
        // 0x50DBA7-0x50DC71: CVector::Random(-v, v) drawn z, y, x (CVector::Random draws x, y, z); each (v - -v) * r + -v
        const float lo = -m_twitchVel;
        float c[3];
        for (auto i = 2; i >= 0; i--) {
            const float r = static_cast<float>(CGeneral::GetRandomNumber()) * RAND_MAX_FLOAT_RECIPROCAL;
            c[i]          = (m_twitchVel - lo) * r + lo;
        }
        m_vel.x += c[0];
        m_vel.y += c[1];
        m_vel.z += c[2];
    }
    m_ang += twitchT * m_vel; //> 0x50DC74
    m_ang  = Clamp(m_ang, -m_lim, m_lim); //> 0x50DCAF

    m_resultMat.SetRotate(m_ang * degree); //> 0x50DD40
}
