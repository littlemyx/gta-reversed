/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Placeable.h"

void CPlaceable::InjectHooks() {
    RH_ScopedVirtualClass(CPlaceable, 0x863C40, 1);
    RH_ScopedCategory("Entity");

    RH_ScopedOverloadedInstall(SetPosn, "xyz", 0x420B80, void(CPlaceable::*)(float, float, float));
    RH_ScopedOverloadedInstall(SetPosn, "vector", 0x4241C0, void(CPlaceable::*)(const CVector&));
    RH_ScopedOverloadedInstall(SetOrientation, "xyz", 0x439A80, void(CPlaceable::*)(float, float, float));
    RH_ScopedInstall(SetHeading, 0x43E0C0);
    RH_ScopedInstall(GetHeading, 0x441DB0);
    RH_ScopedInstall(GetRoll, 0x420B30);
    RH_ScopedOverloadedInstall(IsWithinArea, "xy", 0x54F200, bool(CPlaceable::*)(float, float, float, float) const);
    RH_ScopedOverloadedInstall(IsWithinArea, "xyz", 0x54F2B0, bool(CPlaceable::*)(float, float, float, float, float, float) const);
    RH_ScopedInstall(RemoveMatrix, 0x54F3B0);
    RH_ScopedInstall(AllocateStaticMatrix, 0x54F4C0);
    RH_ScopedInstall(AllocateMatrix, 0x54F560);
    RH_ScopedInstall(SetMatrix, 0x54F610);
    RH_ScopedInstall(GetMatrix, 0x411990);
    RH_ScopedInstall(ShutdownMatrixArray, 0x54EFD0);
    RH_ScopedInstall(InitMatrixArray, 0x54F3A0);
    RH_ScopedInstall(FreeStaticMatrix, 0x54F010);
}

CPlaceable::CPlaceable() : m_placement() {
    m_matrix = nullptr;
}

CPlaceable::~CPlaceable() {
    if (m_matrix) {
        CPlaceable::RemoveMatrix();
        --numMatrices;
    }

    m_matrix = reinterpret_cast<CMatrixLink*>(&gDummyMatrix);
}

CVector CPlaceable::GetRightVector() {
    if (m_matrix)
        return m_matrix->GetRight();
    return { (float)(x87::cos(m_placement.m_fHeading)), (float)(x87::sin(m_placement.m_fHeading)), 0.0f };
}

CVector CPlaceable::GetForwardVector() {
    if (m_matrix)
        return m_matrix->GetForward();
    return { (float)(-x87::sin(m_placement.m_fHeading)), (float)(x87::cos(m_placement.m_fHeading)), 0.0f };
}

CVector CPlaceable::GetUpVector() {
    if (m_matrix)
        return m_matrix->GetUp();
    return {0.0f, 0.0f, 1.0f};
}

void CPlaceable::SetPosn(float x, float y, float z) {
    auto& pos = GetPosition();
    pos.Set(x, y, z);
}

void CPlaceable::SetPosn(const CVector& posn) {
    auto& pos = GetPosition();
    pos = posn;
}

void CPlaceable::SetOrientation(float x, float y, float z) {
    if (!m_matrix) {
        m_placement.m_fHeading = z;
        return;
    }

    CVector vecPos = m_matrix->GetPosition();
    m_matrix->SetRotate(x, y, z);
    m_matrix->GetPosition() += vecPos;
}

void CPlaceable::SetHeading(float heading) {
    if (m_matrix)
        m_matrix->SetRotateZOnly(heading);
    else
        m_placement.m_fHeading = heading;
}

float CPlaceable::GetHeading() const {
    return m_matrix
        ? m_matrix->GetForward().Heading()
        : m_placement.m_fHeading;
}

// 0x420B30
float CPlaceable::GetRoll() const {
    // 0x420B30: // The exe's x87 code VERBATIM (note: atan2(right.z, +-sqrt(right.x^2 + right.y^2)), the old port used the SQUARED magnitude) (generated from the asm, called inside the __asm block with the original stack frame): every product / sum is rounded at the current
    // precision control and the sine / cosine / arctangent results stay unrounded on the FPU stack exactly like the original.
    static const uint32 K858B50 = 0x00000000u;
    static const uint32 K858C1C = 0xBF800000u;
    float res;
    const CPlaceable* self_ = this;
    __asm {
        mov ecx, self_
        call L_BODY
        fstp dword ptr [res]
        jmp L_END
    L_BODY:
        mov ecx, dword ptr [ecx + 0x14]
        test ecx, ecx
        je L_420B6A
        fld dword ptr [ecx + 4]
        fld dword ptr [ecx]
        fld st(0)
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fsqrt
        fstp st(2)
        fstp st(0)
        fld dword ptr [ecx + 0x28]
        fcomp dword ptr [K858B50]
        fnstsw ax
        test ah, 5
        jp L_420B62
        fmul dword ptr [K858C1C]
    L_420B62:
        fld dword ptr [ecx + 8]
        fxch st(1)
        fpatan
        ret
    L_420B6A:
        fld dword ptr [K858B50]
        ret
    L_END:
    }
    return res;
}

bool CPlaceable::IsWithinArea(float x1, float y1, float x2, float y2) const {
    const auto& vecPos = GetPosition();
    if (x1 > x2)
        std::swap(x1, x2);

    if (y1 > y2)
        std::swap(y1, y2);

    return vecPos.x >= x1 && vecPos.x <= x2 && vecPos.y >= y1 && vecPos.y <= y2;
}


bool CPlaceable::IsWithinArea(float x1, float y1, float z1, float x2, float y2, float z2) const {
    const auto& vecPos = GetPosition();
    if (x1 > x2)
        std::swap(x1, x2);

    if (y1 > y2)
        std::swap(y1, y2);

    if (z1 > z2)
        std::swap(z1, z2);

    return vecPos.x >= x1
        && vecPos.x <= x2
        && vecPos.y >= y1
        && vecPos.y <= y2
        && vecPos.z >= z1
        && vecPos.z <= z2;
}

void CPlaceable::RemoveMatrix() {
    const auto& vecForward = m_matrix->GetForward();
    auto fHeading = x87::atan2(-vecForward.x, vecForward.y);

    m_placement.m_vPosn = m_matrix->GetPosition();
    m_placement.m_fHeading = fHeading;

    m_matrix->m_pOwner = nullptr;
    gMatrixList.MoveToFreeList(m_matrix);
    m_matrix = nullptr;
}

void CPlaceable::AllocateStaticMatrix() {
    if (m_matrix)
        return gMatrixList.MoveToList2(m_matrix);

    if (gMatrixList.IsFull())
        gMatrixList.GetOldestLink()->m_pOwner->RemoveMatrix();

    m_matrix = gMatrixList.AddToList2();
    m_matrix->m_pOwner = this;
}

void CPlaceable::AllocateMatrix() {
    if (m_matrix)
        return;

    if (gMatrixList.IsFull())
        gMatrixList.GetOldestLink()->m_pOwner->RemoveMatrix();

    m_matrix = gMatrixList.AddToList1();
    m_matrix->m_pOwner = this;
}

void CPlaceable::SetMatrix(CMatrix& matrix) {
    if (!m_matrix) {
        if (matrix.GetUp().z == 1.0F) {
            auto& vecForward = matrix.GetForward();
            auto fHeading = x87::atan2(-vecForward.x, vecForward.y);

            m_placement.m_vPosn = matrix.GetPosition();
            m_placement.m_fHeading = fHeading;
            return;
        }
        CPlaceable::AllocateMatrix();
    }

    *static_cast<CMatrix*>(m_matrix) = matrix;
}

// NOTSA
bool CPlaceable::IsPointInRange(const CVector& point, float range) {
    return DistanceBetweenPointsSquared(point, GetPosition()) <= sq(range);
}

CMatrix& CPlaceable::GetMatrix() {
    if (!m_matrix) {
        CPlaceable::AllocateMatrix();
        m_placement.UpdateMatrix(m_matrix);
    }

    return *m_matrix;
}

void CPlaceable::ShutdownMatrixArray() {
    gMatrixList.Shutdown();
}

void CPlaceable::InitMatrixArray() {
    ZoneScoped;

    gMatrixList.Init(CPlaceable::NUM_MATRICES_TO_CREATE);
}

void CPlaceable::FreeStaticMatrix() {
    gMatrixList.MoveToList1(m_matrix);
}

void CPlaceable::GetOrientation(float& x, float& y, float& z) {
    if (!m_matrix) {
        z = m_placement.m_fHeading;
        return;
    }

    x = x87::asin(GetForward().z);

    float cosx = x87::cos(x);
    float cosy = GetUp().z / cosx;
    y = x87::acos(cosy);

    float cosz = GetForward().y / cosx;
    z = x87::acos(cosz);
}
