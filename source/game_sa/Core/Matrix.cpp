/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Matrix.h"

uint8* CMatrix::EulerIndices1 = (uint8*)0x866D9C;
uint8* CMatrix::EulerIndices2 = (uint8*)0x866D94;
auto& numMatrices = StaticRef<int32>(0xB74238);
auto& gDummyMatrix = StaticRef<CMatrix>(0xB74240);


void CMatrix::InjectHooks()
{
    RH_ScopedClass(CMatrix);
    RH_ScopedCategory("Core");

    RH_ScopedInstall(Attach, 0x59BD10);
    RH_ScopedInstall(Detach, 0x59ACF0);
    RH_ScopedInstall(CopyOnlyMatrix, 0x59ADD0);
    RH_ScopedInstall(Update, 0x59BB60);
    RH_ScopedInstall(UpdateMatrix, 0x59AD20);
    RH_ScopedInstall(UpdateRW, 0x59BBB0);
    RH_ScopedInstall(UpdateRwMatrix, 0x59AD70);
    RH_ScopedInstall(SetUnity, 0x59AE70);
    RH_ScopedInstall(ResetOrientation, 0x59AEA0);
    RH_ScopedOverloadedInstall(SetScale, "f", 0x59AED0, void(CMatrix::*)(float));
    RH_ScopedOverloadedInstall(SetScale, "fff", 0x59AF00, void(CMatrix::*)(float, float, float));
    RH_ScopedInstall(SetTranslateOnly, 0x59AF80);
    RH_ScopedInstall(SetTranslate, 0x59AF40);
    RH_ScopedInstall(SetRotateXOnly, 0x59AFA0);
    RH_ScopedInstall(SetRotateYOnly, 0x59AFE0);
    RH_ScopedInstall(SetRotateZOnly, 0x59B020);
    RH_ScopedOverloadedInstall(SetRotate, "xyz", 0x59B120, void(CMatrix::*)(float, float, float));
    RH_ScopedInstall(SetRotateX, 0x59B060);
    RH_ScopedInstall(SetRotateY, 0x59B0A0);
    RH_ScopedInstall(SetRotateZ, 0x59B0E0);
    //RH_ScopedInstall(RotateX, 0x59B1E0, {.enabled }); // has NOTSA args, cant hook
    //RH_ScopedInstall(RotateY, 0x59B2C0, {.enabled }); // has NOTSA args, cant hook
    //RH_ScopedInstall(RotateZ, 0x59B390, {.enabled }); // has NOTSA args, cant hook
    RH_ScopedInstall(Rotate, 0x59B460);
    RH_ScopedInstall(Reorthogonalise, 0x59B6A0);
    RH_ScopedInstall(CopyToRwMatrix, 0x59B8B0);
    RH_ScopedOverloadedInstall(SetRotate, "quat", 0x59BBF0, void(CMatrix::*)(const CQuaternion&));
    RH_ScopedInstall(Scale, 0x459350);
    RH_ScopedInstall(ForceUpVector, 0x59B7E0);
    RH_ScopedInstall(ConvertToEulerAngles, 0x59A840);
    RH_ScopedInstall(ConvertFromEulerAngles, 0x59AA40);
    RH_ScopedInstall(operator=, 0x59BBC0);
    RH_ScopedInstall(operator+=, 0x59ADF0);
    RH_ScopedInstall(operator*=, 0x411A80);
    RH_ScopedGlobalOverloadedInstall(operator*, "Mat", 0x59BE30, CMatrix(*)(const CMatrix&, const CMatrix&));
    //RH_ScopedGlobalOverloadedInstall(operator*, "Vec", 0x59C890, CVector(*)(const CMatrix&, const CVector&));
    RH_ScopedGlobalOverloadedInstall(operator+, "", 0x59BFA0, CMatrix(*)(const CMatrix&, const CMatrix&));
    RH_ScopedGlobalOverloadedInstall(Invert, "1", 0x59B920, CMatrix&(*)(CMatrix&, CMatrix&));
    RH_ScopedGlobalOverloadedInstall(Invert, "2", 0x59BDD0, CMatrix(*)(const CMatrix&));
}

CMatrix::CMatrix(const CMatrix& matrix) {
    CMatrix::CopyOnlyMatrix(matrix);
}

// like previous + attach
CMatrix::CMatrix(RwMatrix* matrix, bool temporary) {
    CMatrix::Attach(matrix, temporary);
}

// destructor detaches matrix if attached 
CMatrix::~CMatrix()
{
    CMatrix::Detach();
}

void CMatrix::Attach(RwMatrix* matrix, bool bOwnsMatrix)
{
    CMatrix::Detach();

    m_pAttachMatrix = matrix;
    m_bOwnsAttachedMatrix = bOwnsMatrix;
    CMatrix::Update();
}

void CMatrix::Detach()
{
    if (m_bOwnsAttachedMatrix && m_pAttachMatrix)
        RwMatrixDestroy(m_pAttachMatrix);

    m_pAttachMatrix = nullptr;
}

// copy base RwMatrix to another matrix
void CMatrix::CopyOnlyMatrix(const CMatrix& matrix)
{
    memcpy(this, &matrix, sizeof(RwMatrix));
}

// update RwMatrix with attaching matrix. This doesn't check if attaching matrix is present, so use it only if you know it is present.
// Using UpdateRW() is more safe since it perform this check.
void CMatrix::Update()
{
    CMatrix::UpdateMatrix(m_pAttachMatrix);
}

// update RwMatrix with attaching matrix.
void CMatrix::UpdateRW()
{
    if (!m_pAttachMatrix)
        return;

    CMatrix::UpdateRwMatrix(m_pAttachMatrix);
}

// update RwMatrix with this matrix
void CMatrix::UpdateRwMatrix(RwMatrix* matrix) const
{
    *RwMatrixGetRight(matrix) = m_right;
    *RwMatrixGetUp(matrix) = m_forward;
    *RwMatrixGetAt(matrix) = m_up;
    *RwMatrixGetPos(matrix) = m_pos;

    RwMatrixUpdate(matrix);
}

void CMatrix::UpdateMatrix(RwMatrix* rwMatrix)
{
    m_right = *RwMatrixGetRight(rwMatrix);
    m_forward = *RwMatrixGetUp(rwMatrix);
    m_up = *RwMatrixGetAt(rwMatrix);
    m_pos = *RwMatrixGetPos(rwMatrix);
}

void CMatrix::SetUnity()
{
    CMatrix::ResetOrientation();
    m_pos.Set(0.0F, 0.0F, 0.0F);
}

void CMatrix::ResetOrientation()
{
    m_right.Set  (1.0F, 0.0F, 0.0F);
    m_forward.Set(0.0F, 1.0F, 0.0F);
    m_up.Set     (0.0F, 0.0F, 1.0F);
}

void CMatrix::SetScale(float scale)
{
    m_right.Set  (scale, 0.0F,  0.0F);
    m_forward.Set(0.0F,  scale, 0.0F);
    m_up.Set     (0.0F,  0.0F,  scale);
    m_pos.Set    (0.0F,  0.0F,  0.0F);
}

// scale on three axes
void CMatrix::SetScale(float x, float y, float z)
{
    m_right.Set  (x,     0.0F,  0.0F);
    m_forward.Set(0.0F,  y,     0.0F);
    m_up.Set     (0.0F,  0.0F,  z   );
    m_pos.Set    (0.0F,  0.0F,  0.0F);
}

void CMatrix::SetTranslateOnly(CVector translation)
{
    m_pos = translation;
}

// like previous + reset orientation
void CMatrix::SetTranslate(CVector translation)
{
    CMatrix::ResetOrientation();
    CMatrix::SetTranslateOnly(translation);
}

void CMatrix::SetRotateXOnly(float angle)
{
    auto fSin = sin(angle);
    auto fCos = cos(angle);

    m_right.Set  (1.0F,  0.0F,  0.0F);
    m_forward.Set(0.0F,  fCos,  fSin);
    m_up.Set     (0.0F, -fSin,  fCos);
}

void CMatrix::SetRotateYOnly(float angle)
{
    auto fSin = sin(angle);
    auto fCos = cos(angle);

    m_right.Set  (fCos,  0.0F, -fSin);
    m_forward.Set(0.0F,  1.0F,  0.0F);
    m_up.Set     (fSin,  0.0F,  fCos);
}

void CMatrix::SetRotateZOnly(float angle)
{
    auto fSin = sin(angle);
    auto fCos = cos(angle);

    m_right.Set  ( fCos, fSin, 0.0F);
    m_forward.Set(-fSin, fCos, 0.0F);
    m_up.Set     ( 0.0F, 0.0F, 1.0F);
}

void CMatrix::SetRotateX(float angle)
{
    CMatrix::SetRotateXOnly(angle);
    m_pos.Set(0.0F, 0.0F, 0.0F);
}

void CMatrix::SetRotateY(float angle)
{
    CMatrix::SetRotateYOnly(angle);
    m_pos.Set(0.0F, 0.0F, 0.0F);
}

void CMatrix::SetRotateZ(float angle)
{
    CMatrix::SetRotateZOnly(angle);
    m_pos.Set(0.0F, 0.0F, 0.0F);
}

// set rotate on 3 axes
void CMatrix::SetRotate(float x, float y, float z)
{
    auto fSinX = sin(x);
    auto fCosX = cos(x);
    auto fSinY = sin(y);
    auto fCosY = cos(y);
    auto fSinZ = sin(z);
    auto fCosZ = cos(z);

    m_right.Set  ( fCosZ*fCosY-(fSinZ*fSinX)*fSinY,  fCosZ*fSinX*fSinY+fSinZ*fCosY,  -(fSinY*fCosX));
    m_forward.Set(-(fSinZ*fCosX),                    fCosZ*fCosX,                     fSinX);
    m_up.Set     ( fCosZ*fSinY+fSinZ*fSinX*fCosY,    fSinZ*fSinY-fCosZ*fSinX*fCosY,   fCosY*fCosX);
    m_pos.Set(0.0F, 0.0F, 0.0F);
}

// 0x59B1E0: the exe's x87 code verbatim (the sin is spilled to a float, the cos stays unrounded on the FPU stack, every row is mixed with the
// extended-precision products in the exe's term order and spilled to float temporaries). The exe takes only the angle and ALWAYS rotates m_pos too;
// NOTSA: bKeepPos restores the position afterwards.
void CMatrix::RotateX(float angle, bool bKeepPos)
{
    const CVector savedPos = m_pos;
    float         A[1] = { angle };
    float         L[8];
    CMatrix*      self = this;
    __asm {
        mov ecx, self
        fld dword ptr [A + 0]
        fcos
        fld dword ptr [A + 0]
        fsin
        fstp dword ptr [A + 0]
        fld st(0)
        fmul dword ptr [ecx + 4]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 8]
        fsubp st(1), st(0)
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 4]
        fld st(2)
        fmul dword ptr [ecx + 8]
        faddp st(1), st(0)
        fld st(2)
        fmul dword ptr [ecx + 0x14]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x18]
        fsubp st(1), st(0)
        fstp dword ptr [L + 8]
        fld dword ptr [A + 0]
        mov eax, dword ptr [L + 8]
        fmul dword ptr [ecx + 0x14]
        mov dword ptr [ecx + 0x14], eax
        fld st(3)
        fmul dword ptr [ecx + 0x18]
        faddp st(1), st(0)
        fstp dword ptr [L + 12]
        fld st(2)
        mov edx, dword ptr [L + 12]
        fmul dword ptr [ecx + 0x24]
        mov dword ptr [ecx + 0x18], edx
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x28]
        fsubp st(1), st(0)
        fstp dword ptr [L + 16]
        fld dword ptr [A + 0]
        mov eax, dword ptr [L + 16]
        fmul dword ptr [ecx + 0x24]
        mov dword ptr [ecx + 0x24], eax
        fld st(3)
        fmul dword ptr [ecx + 0x28]
        faddp st(1), st(0)
        fstp dword ptr [L + 20]
        mov edx, dword ptr [L + 20]
        fld st(2)
        fmul dword ptr [ecx + 0x34]
        mov dword ptr [ecx + 0x28], edx
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x38]
        fsubp st(1), st(0)
        fstp dword ptr [L + 24]
        fld dword ptr [A + 0]
        mov eax, dword ptr [L + 24]
        fmul dword ptr [ecx + 0x34]
        mov dword ptr [ecx + 0x34], eax
        fxch st(3)
        fmul dword ptr [ecx + 0x38]
        faddp st(3), st(0)
        fxch st(2)
        fstp dword ptr [L + 28]
        mov edx, dword ptr [L + 28]
        fstp dword ptr [ecx + 4]
        mov dword ptr [ecx + 0x38], edx
        fstp dword ptr [ecx + 8]
    }
    if (bKeepPos) {
        m_pos = savedPos;
    }
}

// 0x59B2C0: the exe's x87 code verbatim (the sin is spilled to a float, the cos stays unrounded on the FPU stack, every row is mixed with the
// extended-precision products in the exe's term order and spilled to float temporaries). The exe takes only the angle and ALWAYS rotates m_pos too;
// NOTSA: bKeepPos restores the position afterwards.
void CMatrix::RotateY(float angle, bool bKeepPos)
{
    const CVector savedPos = m_pos;
    float         A[1] = { angle };
    float         L[8];
    CMatrix*      self = this;
    __asm {
        mov ecx, self
        fld dword ptr [A + 0]
        fcos
        fld dword ptr [A + 0]
        fsin
        fst dword ptr [A + 0]
        fmul dword ptr [ecx + 8]
        fld st(1)
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fld st(1)
        fmul dword ptr [ecx + 8]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx]
        fsubp st(1), st(0)
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x18]
        fld st(3)
        fmul dword ptr [ecx + 0x10]
        faddp st(1), st(0)
        fstp dword ptr [L + 8]
        fld st(2)
        mov eax, dword ptr [L + 8]
        fmul dword ptr [ecx + 0x18]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x10]
        mov dword ptr [ecx + 0x10], eax
        fsubp st(1), st(0)
        fstp dword ptr [L + 12]
        fld dword ptr [A + 0]
        mov edx, dword ptr [L + 12]
        fmul dword ptr [ecx + 0x28]
        mov dword ptr [ecx + 0x18], edx
        fld st(3)
        fmul dword ptr [ecx + 0x20]
        faddp st(1), st(0)
        fstp dword ptr [L + 16]
        fld st(2)
        mov eax, dword ptr [L + 16]
        fmul dword ptr [ecx + 0x28]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x20]
        mov dword ptr [ecx + 0x20], eax
        fsubp st(1), st(0)
        fstp dword ptr [L + 20]
        fld dword ptr [A + 0]
        mov edx, dword ptr [L + 20]
        fmul dword ptr [ecx + 0x38]
        mov dword ptr [ecx + 0x28], edx
        fld st(3)
        fmul dword ptr [ecx + 0x30]
        faddp st(1), st(0)
        fstp dword ptr [L + 24]
        mov eax, dword ptr [L + 24]
        fxch st(2)
        fmul dword ptr [ecx + 0x38]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x30]
        mov dword ptr [ecx + 0x30], eax
        fsubp st(1), st(0)
        fstp dword ptr [L + 28]
        mov edx, dword ptr [L + 28]
        fstp dword ptr [ecx]
        mov dword ptr [ecx + 0x38], edx
        fstp dword ptr [ecx + 8]
    }
    if (bKeepPos) {
        m_pos = savedPos;
    }
}

// 0x59B390: the exe's x87 code verbatim (the sin is spilled to a float, the cos stays unrounded on the FPU stack, every row is mixed with the
// extended-precision products in the exe's term order and spilled to float temporaries). The exe takes only the angle and ALWAYS rotates m_pos too;
// NOTSA: bKeepPos restores the position afterwards.
void CMatrix::RotateZ(float angle, bool bKeepPos)
{
    const CVector savedPos = m_pos;
    float         A[1] = { angle };
    float         L[8];
    CMatrix*      self = this;
    __asm {
        mov ecx, self
        fld dword ptr [A + 0]
        fcos
        fld dword ptr [A + 0]
        fsin
        fstp dword ptr [A + 0]
        fld st(0)
        fmul dword ptr [ecx]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 4]
        fsubp st(1), st(0)
        fld st(1)
        fmul dword ptr [ecx + 4]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fld st(2)
        fmul dword ptr [ecx + 0x10]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x14]
        fsubp st(1), st(0)
        fstp dword ptr [L + 8]
        fld dword ptr [A + 0]
        mov eax, dword ptr [L + 8]
        fmul dword ptr [ecx + 0x10]
        mov dword ptr [ecx + 0x10], eax
        fld st(3)
        fmul dword ptr [ecx + 0x14]
        faddp st(1), st(0)
        fstp dword ptr [L + 12]
        fld st(2)
        mov edx, dword ptr [L + 12]
        fmul dword ptr [ecx + 0x20]
        mov dword ptr [ecx + 0x14], edx
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x24]
        fsubp st(1), st(0)
        fstp dword ptr [L + 16]
        fld dword ptr [A + 0]
        mov eax, dword ptr [L + 16]
        fmul dword ptr [ecx + 0x20]
        mov dword ptr [ecx + 0x20], eax
        fld st(3)
        fmul dword ptr [ecx + 0x24]
        faddp st(1), st(0)
        fstp dword ptr [L + 20]
        mov edx, dword ptr [L + 20]
        fld st(2)
        fmul dword ptr [ecx + 0x30]
        mov dword ptr [ecx + 0x24], edx
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x34]
        fsubp st(1), st(0)
        fstp dword ptr [L + 24]
        fld dword ptr [A + 0]
        mov eax, dword ptr [L + 24]
        fmul dword ptr [ecx + 0x30]
        mov dword ptr [ecx + 0x30], eax
        fxch st(3)
        fmul dword ptr [ecx + 0x34]
        faddp st(3), st(0)
        fxch st(2)
        fstp dword ptr [L + 28]
        mov edx, dword ptr [L + 28]
        fstp dword ptr [ecx]
        mov dword ptr [ecx + 0x34], edx
        fstp dword ptr [ecx + 4]
    }
    if (bKeepPos) {
        m_pos = savedPos;
    }
}

// 0x59B460 (RET 0xC: 3 float args). The exe's x87 code verbatim: sin/cos of x, y, z (fsin/fcos), the 3x3 product built from float spills, then
// every row of this matrix (including the position) is transformed with the extended-precision accumulations of the exe.
void CMatrix::Rotate(CVector rotation)
{
    float    A[3] = { rotation.x, rotation.y, rotation.z };
    float    L[21];
    CMatrix* self = this;
    __asm {
        mov ecx, self
        fld dword ptr [A + 0]
        fcos
        fld dword ptr [A + 0]
        fsin
        fstp dword ptr [A + 0]
        fld dword ptr [A + 4]
        fcos
        fld dword ptr [A + 4]
        fsin
        fld dword ptr [A + 8]
        fcos
        fld dword ptr [A + 8]
        fsin
        fst dword ptr [A + 4]
        fmul dword ptr [A + 0]
        fld st(1)
        fmul dword ptr [A + 0]
        fstp dword ptr [A + 8]
        fld st(1)
        fmul st(0), st(4)
        fld st(1)
        fmul st(0), st(4)
        fsubp st(1), st(0)
        fstp dword ptr [L + 0]
        fld dword ptr [A + 8]
        fmul st(0), st(3)
        fld dword ptr [A + 4]
        fmul st(0), st(5)
        faddp st(1), st(0)
        fstp dword ptr [L + 4]
        fld st(2)
        fmul st(0), st(5)
        fchs
        fstp dword ptr [L + 8]
        fld dword ptr [A + 4]
        fmul st(0), st(5)
        fchs
        fstp dword ptr [L + 12]
        fld st(1)
        fmul st(0), st(5)
        fstp dword ptr [L + 16]
        fxch st(1)
        fmul st(0), st(2)
        fxch st(1)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fstp dword ptr [L + 24]
        fld dword ptr [A + 4]
        fmul st(0), st(1)
        fld dword ptr [A + 8]
        fmul st(0), st(3)
        fsubp st(1), st(0)
        fstp dword ptr [L + 28]
        fstp st(0)
        fmulp st(1), st(0)
        fld dword ptr [L + 24]
        fmul dword ptr [ecx + 8]
        fld dword ptr [L + 12]
        fmul dword ptr [ecx + 4]
        faddp st(1), st(0)
        fld dword ptr [L + 0]
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fld dword ptr [L + 28]
        fmul dword ptr [ecx + 8]
        fld dword ptr [L + 16]
        fmul dword ptr [ecx + 4]
        faddp st(1), st(0)
        fld dword ptr [L + 4]
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fld st(2)
        fmul dword ptr [ecx + 8]
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 4]
        faddp st(1), st(0)
        fld dword ptr [L + 8]
        fmul dword ptr [ecx]
        faddp st(1), st(0)
        fstp dword ptr [L + 44]
        fld dword ptr [L + 0]
        fmul dword ptr [ecx + 0x10]
        fld dword ptr [L + 24]
        fmul dword ptr [ecx + 0x18]
        faddp st(1), st(0)
        fld dword ptr [L + 12]
        fmul dword ptr [ecx + 0x14]
        faddp st(1), st(0)
        fstp dword ptr [L + 48]
        fld dword ptr [L + 4]
        fmul dword ptr [ecx + 0x10]
        fld dword ptr [L + 28]
        fmul dword ptr [ecx + 0x18]
        faddp st(1), st(0)
        fld dword ptr [L + 16]
        fmul dword ptr [ecx + 0x14]
        faddp st(1), st(0)
        fstp dword ptr [L + 52]
        fld dword ptr [L + 8]
        fmul dword ptr [ecx + 0x10]
        fld st(3)
        fmul dword ptr [ecx + 0x18]
        faddp st(1), st(0)
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x14]
        faddp st(1), st(0)
        fstp dword ptr [L + 56]
        fld dword ptr [L + 0]
        fmul dword ptr [ecx + 0x20]
        fld dword ptr [L + 24]
        fmul dword ptr [ecx + 0x28]
        faddp st(1), st(0)
        fld dword ptr [L + 12]
        fmul dword ptr [ecx + 0x24]
        faddp st(1), st(0)
        fstp dword ptr [L + 60]
        fld dword ptr [L + 4]
        fmul dword ptr [ecx + 0x20]
        fld dword ptr [L + 28]
        fmul dword ptr [ecx + 0x28]
        faddp st(1), st(0)
        fld dword ptr [L + 16]
        fmul dword ptr [ecx + 0x24]
        faddp st(1), st(0)
        fstp dword ptr [L + 64]
        fld dword ptr [L + 8]
        fmul dword ptr [ecx + 0x20]
        fld st(3)
        fmul dword ptr [ecx + 0x28]
        faddp st(1), st(0)
        fld dword ptr [A + 0]
        fmul dword ptr [ecx + 0x24]
        faddp st(1), st(0)
        fstp dword ptr [L + 68]
        fld dword ptr [L + 0]
        fmul dword ptr [ecx + 0x30]
        fld dword ptr [L + 24]
        fmul dword ptr [ecx + 0x38]
        faddp st(1), st(0)
        fld dword ptr [L + 12]
        fmul dword ptr [ecx + 0x34]
        faddp st(1), st(0)
        fstp dword ptr [L + 72]
        fld dword ptr [L + 4]
        fmul dword ptr [ecx + 0x30]
        fld dword ptr [L + 28]
        fmul dword ptr [ecx + 0x38]
        faddp st(1), st(0)
        fld dword ptr [L + 16]
        fmul dword ptr [ecx + 0x34]
        faddp st(1), st(0)
        fstp dword ptr [L + 76]
        fld dword ptr [L + 8]
        fmul dword ptr [ecx + 0x30]
        fxch st(3)
        fmul dword ptr [ecx + 0x38]
        mov eax, dword ptr [L + 44]
        faddp st(3), st(0)
        fld dword ptr [A + 0]
        mov edx, dword ptr [L + 48]
        fmul dword ptr [ecx + 0x34]
        mov dword ptr [ecx + 8], eax
        mov eax, dword ptr [L + 52]
        mov dword ptr [ecx + 0x10], edx
        mov edx, dword ptr [L + 56]
        faddp st(3), st(0)
        mov dword ptr [ecx + 0x14], eax
        fxch st(2)
        mov eax, dword ptr [L + 60]
        mov dword ptr [ecx + 0x18], edx
        fstp dword ptr [L + 80]
        mov edx, dword ptr [L + 64]
        mov dword ptr [ecx + 0x20], eax
        fstp dword ptr [ecx]
        mov eax, dword ptr [L + 68]
        mov dword ptr [ecx + 0x24], edx
        fstp dword ptr [ecx + 4]
        mov edx, dword ptr [L + 72]
        mov dword ptr [ecx + 0x28], eax
        mov eax, dword ptr [L + 76]
        mov dword ptr [ecx + 0x30], edx
        mov edx, dword ptr [L + 80]
        mov dword ptr [ecx + 0x34], eax
        mov dword ptr [ecx + 0x38], edx
    }
}

void CMatrix::Reorthogonalise()
{
    auto vecCross = CrossProduct(m_right, m_forward);
    vecCross.Normalise();

    auto vecCross2 = CrossProduct(m_forward, vecCross);
    vecCross2.Normalise();

    auto vecCross3 = CrossProduct(vecCross, vecCross2);

    m_right = vecCross2;
    m_forward = vecCross3;
    m_up = vecCross;
}

// similar to UpdateRW(RwMatrixTag *)
void CMatrix::CopyToRwMatrix(RwMatrix* matrix) const
{
    UpdateRwMatrix(matrix);
    RwMatrixUpdate(matrix);
}

void CMatrix::SetRotate(const CQuaternion& quat)
{
    auto vecImag2 = quat.imag + quat.imag;
    auto x2x = vecImag2.x * quat.imag.x;
    auto y2x = vecImag2.y * quat.imag.x;
    auto z2x = vecImag2.z * quat.imag.x;

    auto y2y = vecImag2.y * quat.imag.y;
    auto z2y = vecImag2.z * quat.imag.y;
    auto z2z = vecImag2.z * quat.imag.z;

    auto x2r = vecImag2.x * quat.real;
    auto y2r = vecImag2.y * quat.real;
    auto z2r = vecImag2.z * quat.real;

    m_right.Set  (1.0F-(z2z+y2y),   z2r+y2x,        z2x-y2r);
    m_forward.Set(y2x-z2r,          1.0F-(z2z+x2x), x2r+z2y);
    m_up.Set     (y2r+z2x,          z2y-x2r,        1.0F-(y2y+x2x));
}

void CMatrix::Scale(float scale) {
    ScaleXYZ(scale, scale, scale);
}

void CMatrix::ScaleXYZ(float x, float y, float z) {
    m_right   *= x;
    m_forward *= y;
    m_up      *= z;
}

void CMatrix::ForceUpVector(CVector vecUp) {
    m_right   = CrossProduct(m_forward, vecUp);
    m_forward = CrossProduct(vecUp, m_right);
    m_up      = vecUp;
}

void CMatrix::ConvertToEulerAngles(float* pX, float* pY, float* pZ, uint32 uiFlags)
{
    float fArr[3][3];

    fArr[0][0] = m_right.x;
    fArr[0][1] = m_right.y;
    fArr[0][2] = m_right.z;

    fArr[1][0] = m_forward.x;
    fArr[1][1] = m_forward.y;
    fArr[1][2] = m_forward.z;

    fArr[2][0] = m_up.x;
    fArr[2][1] = m_up.y;
    fArr[2][2] = m_up.z;

    /* Original indices deciding logic, i replaced it with clearer one
    auto iInd1 = CMatrix::EulerIndices1[(uiFlags >> 3) & 0x3];
    auto iInd2 = CMatrix::EulerIndices2[iInd1 + ((uiFlags & 0x4) != 0)];
    auto iInd3 = CMatrix::EulerIndices2[iInd1 - ((uiFlags & 0x4) != 0) + 1]; */
    int8 iInd1 = 0, iInd2 = 1, iInd3 = 2;
    switch (uiFlags & eMatrixEulerFlags::_ORDER_MASK) {
    case ORDER_XYZ:
        iInd1 = 0, iInd2 = 1, iInd3 = 2;
        break;
    case ORDER_XZY:
        iInd1 = 0, iInd2 = 2, iInd3 = 1;
        break;
    case ORDER_YZX:
        iInd1 = 1, iInd2 = 2, iInd3 = 0;
        break;
    case ORDER_YXZ:
        iInd1 = 1, iInd2 = 0, iInd3 = 2;
        break;
    case ORDER_ZXY:
        iInd1 = 2, iInd2 = 0, iInd3 = 1;
        break;
    case ORDER_ZYX:
        iInd1 = 2, iInd2 = 1, iInd3 = 0;
        break;
    }

    if (uiFlags & eMatrixEulerFlags::EULER_ANGLES) {
        auto r13 = fArr[iInd1][iInd3];
        auto r12 = fArr[iInd1][iInd2];
        auto cy = sqrt(r12 * r12 + r13 * r13);
        if (cy > 0.0000019073486) { // Some epsilon?
            *pX = atan2(r12, r13);
            *pY = atan2(cy, fArr[iInd1][iInd1]);
            *pZ = atan2(fArr[iInd2][iInd3], -fArr[iInd3][iInd1]);
        }
        else {
            *pX = atan2(-fArr[iInd2][iInd3], fArr[iInd2][iInd2]);
            *pY = atan2(cy, fArr[iInd1][iInd1]);
            *pZ = 0.0F;
        }
    }
    else {
        auto r21 = fArr[iInd2][iInd1];
        auto r11 = fArr[iInd1][iInd1];
        auto cy = sqrt(r11 * r11 + r21 * r21);
        if (cy > 0.0000019073486) { // Some epsilon?
            *pX = atan2(fArr[iInd3][iInd2], fArr[iInd3][iInd3]);
            *pY = atan2(-fArr[iInd3][iInd1], cy);
            *pZ = atan2(r21, r11);
        }
        else {
            *pX = atan2(-fArr[iInd2][iInd3], fArr[iInd2][iInd2]);
            *pY = atan2(-fArr[iInd3][iInd1], cy);
            *pZ = 0.0F;
        }
    }

    if (uiFlags & eMatrixEulerFlags::SWAP_XZ)
        std::swap(*pX, *pZ);

    if (uiFlags & eMatrixEulerFlags::_ORDER_NEEDS_SWAP) {
        *pX = -*pX;
        *pY = -*pY;
        *pZ = -*pZ;
    }
}

void CMatrix::ConvertFromEulerAngles(float x, float y, float z, uint32 uiFlags)
{
    /* Original indices deciding logic, i replaced it with clearer one
    auto iInd1 = CMatrix::EulerIndices1[(uiFlags >> 3) & 0x3];
    auto iInd2 = CMatrix::EulerIndices2[iInd1 + ((uiFlags & 0x4) != 0)];
    auto iInd3 = CMatrix::EulerIndices2[iInd1 - ((uiFlags & 0x4) != 0) + 1]; */
    int8 iInd1 = 0, iInd2 = 1, iInd3 = 2;
    switch (uiFlags & eMatrixEulerFlags::_ORDER_MASK) {
    case ORDER_XYZ:
        iInd1 = 0, iInd2 = 1, iInd3 = 2;
        break;
    case ORDER_XZY:
        iInd1 = 0, iInd2 = 2, iInd3 = 1;
        break;
    case ORDER_YZX:
        iInd1 = 1, iInd2 = 2, iInd3 = 0;
        break;
    case ORDER_YXZ:
        iInd1 = 1, iInd2 = 0, iInd3 = 2;
        break;
    case ORDER_ZXY:
        iInd1 = 2, iInd2 = 0, iInd3 = 1;
        break;
    case ORDER_ZYX:
        iInd1 = 2, iInd2 = 1, iInd3 = 0;
        break;
    }

    float fArr[3][3];

    if (uiFlags & eMatrixEulerFlags::SWAP_XZ)
        std::swap(x, z);

    if (uiFlags & eMatrixEulerFlags::_ORDER_NEEDS_SWAP) {
        x = -x;
        y = -y;
        z = -z;
    }

    auto fSinX = sin(x);
    auto fCosX = cos(x);
    auto fSinY = sin(y);
    auto fCosY = cos(y);
    auto fSinZ = sin(z);
    auto fCosZ = cos(z);

    if (uiFlags & eMatrixEulerFlags::EULER_ANGLES) {
        fArr[iInd1][iInd1] = fCosY;
        fArr[iInd1][iInd2] = fSinX*fSinY;
        fArr[iInd1][iInd3] = fCosX*fSinY;

        fArr[iInd2][iInd1] =   fSinY*fSinZ;
        fArr[iInd2][iInd2] =   fCosX*fCosY  - fCosY*fSinX*fSinZ;
        fArr[iInd2][iInd3] = -(fSinX*fCosZ) -(fCosX*fCosY*fSinZ);

        fArr[iInd3][iInd1] = -(fCosZ*fSinY);
        fArr[iInd3][iInd2] =   fCosX*fSinZ  + fCosY*fCosZ*fSinX;
        fArr[iInd3][iInd3] = -(fSinX*fSinZ) + fCosX*fCosY*fCosZ;
    } else { // Use Tait-Bryan angles
        fArr[iInd1][iInd1] =   fCosY*fCosZ;
        fArr[iInd1][iInd2] = -(fCosX*fSinZ) + fCosZ*fSinX*fSinY;
        fArr[iInd1][iInd3] =   fSinX*fSinZ  + fCosX*fCosZ*fSinY;

        fArr[iInd2][iInd1] =   fCosY*fSinZ;
        fArr[iInd2][iInd2] =   fCosX*fCosZ  + fSinX*fSinY*fSinZ;
        fArr[iInd2][iInd3] = -(fCosZ*fSinX) + fCosX*fSinY*fSinZ;

        fArr[iInd3][iInd1] = -fSinY;
        fArr[iInd3][iInd2] =  fCosY*fSinX;
        fArr[iInd3][iInd3] =  fCosX*fCosY;
    }

    m_right.Set  (fArr[0][0], fArr[0][1], fArr[0][2]);
    m_forward.Set(fArr[1][0], fArr[1][1], fArr[1][2]);
    m_up.Set     (fArr[2][0], fArr[2][1], fArr[2][2]);
}

void CMatrix::operator=(const CMatrix& other) {
    CopyOnlyMatrix(other);
    UpdateRW();
}

void CMatrix::operator+=(const CMatrix& rvalue)
{
    m_right += rvalue.m_right;
    m_forward += rvalue.m_forward;
    m_up += rvalue.m_up;
    m_pos += rvalue.m_pos;
}

void CMatrix::operator*=(const CMatrix& rvalue)
{
    *this = (*this * rvalue);
}

CMatrix CMatrix::GetIdentity() {
    CMatrix m;
    m.m_right   = CVector{ 1.f, 0.f, 0.f };
    m.m_forward = CVector{ 0.f, 1.f, 0.f };
    m.m_up      = CVector{ 0.f, 0.f, 1.f };
    m.m_pos     = CVector{ 0.f, 0.f, 0.f };
    return m;
}

CMatrix operator*(const CMatrix& a, const CMatrix& b)
{
    auto result = CMatrix();
    result.m_right =   a.m_right * b.m_right.x   + a.m_forward * b.m_right.y   + a.m_up * b.m_right.z;
    result.m_forward = a.m_right * b.m_forward.x + a.m_forward * b.m_forward.y + a.m_up * b.m_forward.z;
    result.m_up =      a.m_right * b.m_up.x      + a.m_forward * b.m_up.y      + a.m_up * b.m_up.z;
    result.m_pos =     a.m_right * b.m_pos.x     + a.m_forward * b.m_pos.y     + a.m_up * b.m_pos.z + a.m_pos;
    return result;
}

CVector operator*(const CMatrix& a, const CVector& b) {
    return a.TransformPoint(b);
}

CMatrix operator+(const CMatrix& a, const CMatrix& b)
{
    CMatrix result;
    result.m_right =   a.m_right + b.m_right;
    result.m_forward = a.m_forward + b.m_forward;
    result.m_up =      a.m_up + b.m_up;
    result.m_pos =     a.m_pos + b.m_pos;
    return result;
}

CMatrix& Invert(CMatrix& in, CMatrix& out)
{
    out = in.Inverted();
    return out;
}

CMatrix Invert(const CMatrix& in)
{
    return in.Inverted();
}

CMatrix Lerp(CMatrix from, CMatrix to, float t) {
    from.ScaleAll(1.0f - t);
    to.ScaleAll(t);
    return from + to;
}
