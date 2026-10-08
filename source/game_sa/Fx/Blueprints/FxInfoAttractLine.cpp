#include "StdInc.h"

#include "FxInfoAttractLine.h"

void FindClosestPtOnLine(CVector& out, CVector& lineA, CVector& lineB, CVector& point);

void FxInfoAttractLine_c::InjectHooks() {
    RH_ScopedClass(FxInfoAttractLine_c);
    RH_ScopedCategory("Fx");

    RH_ScopedGlobalInstall(FindClosestPtOnLine, 0x4A4880);
}

// 0x4A57C0
FxInfoAttractLine_c::FxInfoAttractLine_c() : FxInfo_c() {
    m_nType = FX_INFO_ATTRACTLINE_DATA;
    m_InterpInfo.Allocate(7);
}

// 0x4A5830
void FxInfoAttractLine_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A4880
void FindClosestPtOnLine(CVector& out, CVector& lineA, CVector& lineB, CVector& point) {
    // The original keeps everything on the x87 stack (extended precision); only the X delta of the
    // line is spilled to a float temporary, and it is used for the X term of the dot product only.
    const double dx = (double)lineB.x - lineA.x;
    const double dy = (double)lineB.y - lineA.y;
    const double dz = (double)lineB.z - lineA.z;
    const float  dxf = (float)dx;

    const double dot    = ((double)point.y - lineA.y) * dy + ((double)point.z - lineA.z) * dz + ((double)point.x - lineA.x) * dxf;
    const double sqMag  = dy * dy + dz * dz + dx * dx;
    double       t      = dot / sqMag;

    // NaN passes through unchanged
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }

    out.x = (float)(dxf * t + lineA.x);
    out.y = (float)(dy * t + lineA.y);
    out.z = (float)(dz * t + lineA.z);
}

// 0x4A5850
void FxInfoAttractLine_c::GetValue(float currentTime, float mult, float totalTime, float length, bool useConst, void* info) {
    float values[7];
    if (m_bTimeModeParticle) {
        m_InterpInfo.GetVal(values, mult);
    } else {
        m_InterpInfo.GetVal(values, currentTime / length);
    }

    CVector out;
    auto& lineA = *(CVector*)&values[0];
    auto& lineB = *(CVector*)&values[3];

    auto& movement = *static_cast<MovementInfo_t*>(info);
    FindClosestPtOnLine(out, lineA, lineB, movement.m_Pos);
    CVector in = out - movement.m_Pos;
    auto dist = in.Magnitude();

    if (dist > 0.0f) {
        in.Normalise();
    }

    if (dist > 0.1f) {
        movement.m_Vel += in * values[6] * totalTime;
    }
}
