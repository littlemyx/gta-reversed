#include "StdInc.h"

#include "PlaneTrail.h"

constexpr auto POINT_EXPIRATION_TIME = 30'000u;

void CPlaneTrail::InjectHooks() {
    RH_ScopedClass(CPlaneTrail);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x717160);
    RH_ScopedInstall(Render, 0x717180);
    RH_ScopedInstall(RegisterPoint, 0x7172D0);
}

// 0x717160
void CPlaneTrail::Init() {
    std::ranges::fill(m_Timepoints, 0);
}

// 0x717180
void CPlaneTrail::Render(float intensity) {
    const auto maxAlpha = NOTSA_FTOL(int32, intensity * 110.0f);
    if (!maxAlpha) {
        return;
    }

    auto nvert{0u}; // num of vertices stored
    static RxObjSpace3DVertex vertBuff[PLANE_TRAIL_BUF_SIZE]; // 0xC4D958 (a global in the exe)
    for (auto i = 0u; i < PLANE_TRAIL_BUF_SIZE; i++) {
        const uint32 timeDelta = CTimer::GetTimeInMS() - m_Timepoints[i];
        if (IsPointInUse(i) && timeDelta <= POINT_EXPIRATION_TIME) {
            RxObjSpace3DVertex& vert = vertBuff[nvert]; // 0x717180: the used vertices are packed (the port indexed by the point, leaving holes)

            const float fAlphaMult = std::min(1.0f, ((float)POINT_EXPIRATION_TIME - (float)timeDelta) * ExeRecip(10'000.0f)); // Clamped to 1.0f; exe 0x858FC4 = float(1/10000), a multiply
            RwIm3DVertexSetRGBA(&vert, 255, 255, 255, NOTSA_FTOL(uint8, (float)maxAlpha * fAlphaMult));

            RwV3dAssign(RwIm3DVertexGetPos(&vert), &m_Positions[i]);

            nvert++;
        } else { // This one has expired, set to 0
            m_Timepoints[i] = 0;
        }
    }

    if (nvert > 1) { // 0x7171F9: `cmp ebp, 1; jle` - one vertex draws nothing and leaves the render states alone
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
        RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
        if (RwIm3DTransform(vertBuff, nvert, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
            static RwImVertexIndex indices[] = { // From 0x8D5B98, size 32
                0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16
            };
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, indices, 2 * nvert - 2);
            RwIm3DEnd();
        }
    }
}

// 0x7172D0
void CPlaneTrail::RegisterPoint(CVector pos) {
    const auto& lastUpdate = m_Timepoints[0];
    const bool bDoShift = lastUpdate && CTimer::GetTimeInMS() - lastUpdate > 2000;
    if (bDoShift) {
        // Shift right
#if __cpp_lib_shift >= 202202L 
        rng::shift_right(m_Timepoints, 1);
        rng::shift_right(m_Positions, 1);
#else
        for (auto i = PLANE_TRAIL_BUF_SIZE - 1; i; i--) {
            m_Timepoints[i] = m_Timepoints[i - 1];
            m_Positions[i] = m_Positions[i - 1];
        }
#endif
    }

    m_Positions[0] = pos;
    if (bDoShift || !IsPointInUse(0)) {
        m_Timepoints[0] = CTimer::GetTimeInMS();
    }
}

// From `CPlaneTrails::Update`
void CPlaneTrail::Update(CVector pos, const CRGBA& color, uint32 coronaIdx, uint32 timeModifierMs, uint8 afterHour, uint8 beforeHour, bool cosFirst) {
    const float fTimeProg = (float)(CTimer::GetTimeInMS() % 131072) * 4.7936901e-05f; // exe constant 0x8631C4 (2*pi/131072), not / 20860.0f
    // exe 0x7174F0: the 3rd trail puts cos on x and sin on y, the others sin on x and cos on y
    // 0x7174F0: `fsin; fmul pos.x; fstp` - the trig result is multiplied unrounded (the port rounded sin/cos to float first)
    const CVector currPos = cosFirst
        ? CVector{ (float)X87R(x87::cos(fTimeProg) * pos.x), (float)X87R(x87::sin(fTimeProg) * pos.y), pos.z }
        : CVector{ (float)X87R(x87::sin(fTimeProg) * pos.x), (float)X87R(x87::cos(fTimeProg) * pos.y), pos.z };

    RegisterPoint(currPos);

    // exe: the corona is processed when hours > afterHour || hours < beforeHour (a wrapped night range; ClockHoursInRange is `&&`, always false here)
    if (!(CClock::ms_nGameClockHours > afterHour || CClock::ms_nGameClockHours < beforeHour)) {
        return;
    }

    if ((CTimer::GetTimeInMS() + timeModifierMs) & 512) {
        CCoronas::RegisterCorona(
            coronaIdx,
            nullptr,
            color.r, color.g, color.b, color.a,
            currPos,
            5.0f,
            2000.0f,
            eCoronaType::CORONATYPE_HEADLIGHT,
            eCoronaFlareType::FLARETYPE_NONE,
            eCoronaReflType::CORREFL_NONE,
            eCoronaLOSCheck::LOSCHECK_OFF,
            eCoronaTrail::TRAIL_OFF,
            0.0f,
            false,
            1.5f,
            0,
            15.0f,
            false,
            false
        );
    } else {
        CCoronas::UpdateCoronaCoors(coronaIdx, currPos, 2000.0f, 0.0f);
    }
}

bool CPlaneTrail::IsPointInUse(size_t pt) {
    return m_Timepoints[pt] != 0;
}
