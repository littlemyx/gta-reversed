#include "StdInc.h"

NOTSA_GLOBAL(g_LoadMonitor, 0xB72978, (CLoadMonitor), {}); static_assert(sizeof(g_LoadMonitor) == 688);

void CLoadMonitor::InjectHooks() {
    RH_ScopedClass(CLoadMonitor);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x53CFA0);
    RH_ScopedInstall(Destructor, 0x856430);

    RH_ScopedInstall(BeginFrame, 0x53D030);
    RH_ScopedInstall(EndFrame, 0x53D0B0);
    RH_ScopedInstall(StartTimer, 0x53D050);
    RH_ScopedInstall(EndTimer, 0x53D070);
    RH_ScopedInstall(Render, 0x53D480);
}

// 0x53CFA0
CLoadMonitor::CLoadMonitor() {
    m_bInFrame            = 0;
    m_bUseLoadMonitor     = 1;
    m_eProcLevel          = eProcessingLevel::OK;
    m_LastTime            = CTimer::GetTimeInMS();
    m_NumFramesThisSec    = 0;
    m_FPS                 = 0;
    m_bEnableAmbientCrime = true;

    rng::fill(m_iMaxCycles, 0);

    rng::fill(m_fAveragedCyclesThisSecond, 0.f);

    m_fPeakLevels[+eLoadType::PED_AI]                 = 15.0f;
    m_fPeakLevels[+eLoadType::COLLISION]              = 20.0f;
    m_fPeakLevels[+eLoadType::NUM_STREAMING_REQUESTS] = 10.0f;
    m_fPeakLevels[+eLoadType::MOVE_SPEED]             = 50.0f;

    m_DisplayType = eLoadMonitorDisplay::NONE;
    m_VarConsoleDisplayType = eLoadMonitorDisplay::NONE;
}

// 0x53D030
void CLoadMonitor::BeginFrame() {
    rng::fill(m_iCyclesThisFrame, 0);
    m_bInFrame = 1;
}

// 0x53D0B0
void CLoadMonitor::EndFrame() {
    if (auto* ped = FindPlayerPed()) {
        const auto& movevec = ped->IsInVehicle()
            ? ped->GetVehicleIfInOne()->GetMoveSpeed()
            : ped->GetMoveSpeed();

        // 0x53D0FD-0x53D13B: each component * 50 first, then sqrt((z^2 + y^2) + x^2)
        const x87d sx = (x87d)movevec.x * (x87d)50.0f;
        const x87d sy = (x87d)movevec.y * (x87d)50.0f;
        const x87d sz = (x87d)movevec.z * (x87d)50.0f;
        m_iCyclesThisFrame[+eLoadType::MOVE_SPEED] = NOTSA_FTOL(uint32, std::sqrt((sz * sz + sy * sy) + sx * sx));
    }

    // 0x53D143 - 0x53D1D3
    for (auto t = 0u; t < +eLoadType::NUM_LOAD_TYPES; t++) {
        m_iMaxCycles[t] = std::max(m_iMaxCycles[t], m_iCyclesThisFrame[t]);
        // 0x53D154-0x53D166: fild of the integer, fadd of the float sum, one rounding (the port rounded the cycles to float first)
        m_fAveragedCyclesThisSecond[t] = (float)((x87d)m_iCyclesThisFrame[t] + (x87d)m_fAveragedCyclesThisSecond[t]);
    }

    auto mstime = CTimer::GetTimeInMS();
    auto delta  = mstime - m_LastTime;

    if (delta > 2'000) {
        m_LastTime = mstime;
        m_NumFramesThisSec = 0;
        return;
    }

    if (delta >= 1'000) {
        m_FPS = m_NumFramesThisSec;
        m_LastTime = mstime;

        if (m_NumFramesThisSec == 0) {
            m_NumFramesThisSec = 1;
        }

        const float recip = 1.f / (float)m_NumFramesThisSec;
        for (auto& avg : m_fAveragedCyclesThisSecond) {
            avg *= recip;
        }

        m_NumFramesThisSec = 0;

        for (auto&& [ch, acts] : rngv::zip(m_iCyclesHistory, m_fAveragedCyclesThisSecond)) {
            rng::shift_right(ch, 1);
            ch[0] = NOTSA_FTOL(uint32, acts);
            acts = 0.0f;
        }

        m_eProcLevel = eProcessingLevel::OK;

        const auto needsCyclesConversion = [](uint32 t) {
            return t != +eLoadType::NUM_STREAMING_REQUESTS
                && t != +eLoadType::MOVE_SPEED;
        };

        for (auto t = 0u; t < +eLoadType::NUM_LOAD_TYPES; t++) {
            float smoothed{};
            if (m_iMaxCycles[t] != 0) {
                // 0x53D2CC-0x53D3D2: history[0] + history[1] + ... + history[7] (each fild'ed unrounded, one rounding per add), * 0.125, capped by (float)max
                float sum = (float)((x87d)m_iCyclesHistory[t][0] + (x87d)m_iCyclesHistory[t][1]);
                for (auto a = 2u; a < 8u; a++) {
                    sum = (float)((x87d)sum + (x87d)m_iCyclesHistory[t][a]);
                }

                const float avg = sum * 0.125f;
                const float cap = (float)m_iMaxCycles[t];
                smoothed = avg <= cap ? avg : cap;
            }
            m_fSmoothedValues[t] = smoothed;

            const float val = needsCyclesConversion(t) ? smoothed / (float)CTimer::GetCyclesPerMillisecond() : smoothed;
            const float normalized = val / m_fPeakLevels[t];

            // 0x53D40E-0x53D441: the thresholds are the float constants 0x863AD8 = 0.66666f and 0x863AD4 = 0.33333f (not 2/3, 1/3)
            if (normalized > 0.66666f && m_eProcLevel < eProcessingLevel::HIGH) {
                m_eProcLevel = eProcessingLevel::HIGH;
            } else if (normalized > 0.33333f && m_eProcLevel < eProcessingLevel::MED) {
                m_eProcLevel = eProcessingLevel::MED;
            }

            m_fNormalizedPeakRangeValues[t] = normalized;
            m_fSmoothedValues[t] = smoothed / (float)m_iMaxCycles[t];
        }
    } else {
        m_NumFramesThisSec++;
    }

    if (IsForcingProcLevel()) {
        m_eProcLevel = m_eProcLevelToForce;
    }
}

// 0x53D050
void CLoadMonitor::StartTimer(eLoadType timerIndex) {
    m_iStartTimes[+timerIndex] = CTimer::GetCurrentTimeInCycles();
}

// 0x53D070
void CLoadMonitor::EndTimer(eLoadType timerIndex) {
    m_iCyclesThisFrame[+timerIndex] += CTimer::GetCurrentTimeInCycles() - m_iStartTimes[+timerIndex];
}

// debug function?
// See LoadMonitorDebugModule
// 0x53D480
void CLoadMonitor::Render() {
    // NOP
}
