#include "StdInc.h"

#include "FxInfoGroundCollide.h"

void FxInfoGroundCollide_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoGroundCollide_c, 0x85A884, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A7090);
    RH_ScopedVMTInstall(Load, 0x4A59F0);
    RH_ScopedVMTInstall(GetValue, 0x4A7100);
}

// 0x4A5980
FxInfoGroundCollide_c::FxInfoGroundCollide_c() : FxInfo_c() {
    m_nType = FX_INFO_GROUNDCOLLIDE_DATA;
    m_InterpInfo.Allocate(3);
}

// 0x4A59F0
void FxInfoGroundCollide_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A7100
void FxInfoGroundCollide_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    if (!m_bTimeModeParticle) {
        mult = currentTime / len;
    }

    auto& movement = *static_cast<MovementInfo_t*>(info);

    CVector origin = movement.m_Pos;
    auto diff = movement.m_Pos.z - movement.m_Vel.z;
    CColPoint colPoint;
    CEntity* colEntity;
    if (!CWorld::ProcessVerticalLine(origin, diff, colPoint, colEntity, true))
        return;

    // 0x4A71A9: return when the hit is above the origin (NaN continues); 0x4A71BF `test ah,5; jp`: return when pos.z >= hit.z or unordered
    if (colPoint.m_vecPoint.z > origin.z || !(movement.m_Pos.z < colPoint.m_vecPoint.z))
        return;

    float values[16];
    m_InterpInfo.GetVal(values, mult);

    // 0x4A71DA-0x4A7239: reflect the velocity: v = 2 * values[0] * ((n.y*v.y + n.x*v.x) + n.z*v.z); out = vel - n * v
    const auto& n = colPoint.m_vecNormal;
    const float dot = (n.y * movement.m_Vel.y + n.x * movement.m_Vel.x) + n.z * movement.m_Vel.z;
    const float v13 = dot * values[0];
    const float refl = v13 + v13;
    CVector out;
    out.x = movement.m_Vel.x - n.x * refl;
    out.y = movement.m_Vel.y - n.y * refl;
    out.z = movement.m_Vel.z - n.z * refl;

    // 0x4A7241-0x4A72BA: three rand() calls (x, y, z in this order), then RW's table-based RwV3dNormalize (0x7ED9B0)
    CVector in;
    in.x = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    in.y = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    in.z = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    RwV3dNormalize(&in, &in);
    const float scatter = values[2] * totalTime * 5.0f;
    in.x *= scatter;
    in.y *= scatter;
    in.z *= scatter;

    // 0x4A72F6: the speed is RwV3dLength (0x7EDAC0) of the reflected velocity BEFORE the scatter is added (not the length of the normalised result)
    const float speed = RwV3dLength(&out);
    out.x += in.x;
    out.y += in.y;
    out.z += in.z;
    RwV3dNormalize(&out, &out);

    // 0x4A7347-0x4A7378: pos.z = hit.z, vel = values[1] * (speed * dir)
    movement.m_Pos.z = colPoint.m_vecPoint.z;
    movement.m_Vel.x = values[1] * (speed * out.x);
    movement.m_Vel.y = values[1] * (speed * out.y);
    movement.m_Vel.z = values[1] * (speed * out.z);
}
