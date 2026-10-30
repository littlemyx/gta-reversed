#include "StdInc.h"

#include "WindModifiers.h"

void CWindModifiers::InjectHooks() {
    RH_ScopedClass(CWindModifiers);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(RegisterOne, 0x72C8B0);
    RH_ScopedInstall(FindWindModifier, 0x72C950);
}

// 0x72C8B0
void CWindModifiers::RegisterOne(CVector vecPos, int32 iActive, float fPower) {
    if (Number < MAX_NUM_MODIFIERS) {
        // exe 0x72C8D2-0x72C909: x87 `sqrt((dz*dz + dy*dy) + dx*dx) < 200` (z, y, x term order; the compare is against the unrounded root; NaN -> not registered)
        const auto& camPos = TheCamera.GetPosition();
        const float dx = vecPos.x - camPos.x, dy = vecPos.y - camPos.y, dz = vecPos.z - camPos.z;
        if ((float)std::sqrt(((x87d)dz * dz + (x87d)dy * dy) + (x87d)dx * dx) < 200.0f) {
            Array[Number] = {
                .m_vecPos = vecPos,
                .m_iActive = iActive,
                .m_fPower = fPower
            };
            Number++;
        }
    }
}

// 0x72C950
bool CWindModifiers::FindWindModifier(CVector vecPos, float* outX, float* outY) {
    if (Number <= 0)
        return false;

    float posX{}, posY{};
    bool appliedAny{};
    for (auto i = 0; i < Number; i++) {
        const auto& wm = Array[i];
        if (wm.m_iActive != 1) // 0x72C984: `dec eax; jne` - only exactly 1 is active
            continue;

        float absZDist = vecPos.z + 15.f - wm.m_vecPos.z;
        if (absZDist < 0.0f) // fcom 0 / fchs
            absZDist = -absZDist;
        if (!(absZDist < 40.0f)) // 0x72C9A5: jp (>=40 or NaN) leaves
            continue;
        const float distZProg = 1.0f - absZDist * 0.025f; // 0x859038: multiply by the constant, not invLerp's divide

        // 0x72C9C2-0x72C9F6: dist = sqrt((dz*dz + dy*dy) + dx*dx), x/y stored as floats first
        const float dx = vecPos.x - wm.m_vecPos.x;
        const float dy = vecPos.y - wm.m_vecPos.y;
        const float dz = vecPos.z - wm.m_vecPos.z;
        const float dist = (float)std::sqrt(((x87d)dz * dz + (x87d)dy * dy) + (x87d)dx * dx);
        if (!(dist < 50.f))
            continue;

        const float distProg = dist < 20.f ? 1.f : 1.0f - (dist - 20.f) * 0.033333335f; // 0x858F10: (dist - 20) * const
        const float power = distZProg * (distProg * wm.m_fPower) * 0.2f; // 0x858CC4

        // NOTE: Possible bug? They don't seem to re-use the last value, but rather overwrite it..
        //       shouldn't they add up all the values or something?
        const float inv = 1.0f / dist; // 0x72CA3D: fld 1.0 / fdiv dist, then multiplied (no per-axis divide)
        posX = dx * inv * power;
        posY = inv * dy * power;

        appliedAny = true;
    }

    if (!appliedAny)
        return false;

    const float rnd = 1.0f + (float)(CGeneral::GetRandomNumber() % 32 - 16) * 0.0005f; // 0x859978
    *outX += posX * rnd;
    *outY += posY * rnd;

    return true;
}
