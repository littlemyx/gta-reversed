#include "StdInc.h"

#include "EntitySeekPosCalculatorRadiusAngleOffset.h"
#include "PedGeometryAnalyser.h"

void CEntitySeekPosCalculatorRadiusAngleOffset::InjectHooks() {
    RH_ScopedVirtualClass(CEntitySeekPosCalculatorRadiusAngleOffset, 0x85A384, 2);
    RH_ScopedCategory("Tasks/TaskTypes/SeekPosCalculators");

    RH_ScopedVMTInstall(ComputeEntitySeekPos, 0x6946f0);
}

CEntitySeekPosCalculatorRadiusAngleOffset::CEntitySeekPosCalculatorRadiusAngleOffset(float radius, float angle) :
    m_radius{ radius },
    m_angle{ angle }
{
}

void CEntitySeekPosCalculatorRadiusAngleOffset::ComputeEntitySeekPos(const CPed& seeker, const CEntity& target, CVector& outPos) {
    // exe 0x6946F8..0x694731: a target with a matrix gives its forward vector (matrix + 0x10); without one the placement heading (+0x10) gives (-sin h, cos h)
    CVector2D forward;
    if (target.m_matrix) {
        forward = CVector2D{ target.m_matrix->GetForward() };
    } else {
        const auto h = target.m_placement.m_fHeading;
        forward = { (float)(-x87::sin(h)), (float)x87::cos(h) };
    }
    const auto heading = CGeneral::LimitRadianAngle(
        CGeneral::GetRadianAngleBetweenPoints(forward, CVector2D{ 0.0f, 0.0f })
    );
    const auto angle = CGeneral::LimitRadianAngle(heading + m_angle);
    CPedGeometryAnalyser::ComputeClearTarget(
        seeker, 
        target.GetPosition() + CVector{
            (float)(X87R(-x87::sin(angle) * m_radius)),
            (float)x87::cos(angle) * m_radius, // 0x69477E: cos is stored as a float (fstp) before the radius multiply, unlike sin (kept extended)
            0.0f
        }, 
        outPos
    );
}
