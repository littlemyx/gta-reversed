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
NOTSA_GLOBAL(numMatrices, 0xB74238, (int32), {});
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
    auto fSin = x87::sin(angle);
    auto fCos = x87::cos(angle);

    m_right.Set  (1.0F,  0.0F,  0.0F);
    m_forward.Set(0.0F,  fCos,  fSin);
    m_up.Set     (0.0F, -fSin,  fCos);
}

void CMatrix::SetRotateYOnly(float angle)
{
    auto fSin = x87::sin(angle);
    auto fCos = x87::cos(angle);

    m_right.Set  (fCos,  0.0F, -fSin);
    m_forward.Set(0.0F,  1.0F,  0.0F);
    m_up.Set     (fSin,  0.0F,  fCos);
}

void CMatrix::SetRotateZOnly(float angle)
{
    auto fSin = x87::sin(angle);
    auto fCos = x87::cos(angle);

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
void CMatrix::SetRotate(float x, float y, float z) {
    // 0x59B120: // The exe's x87 code VERBATIM (generated from the asm, called inside the __asm block with the original stack frame): every product / sum is rounded at the current
    // precision control and the sine / cosine / arctangent results stay unrounded on the FPU stack exactly like the original.
    const float a_x = x;
    const float a_y = y;
    const float a_z = z;
    CMatrix* self_ = this;
    __asm {
        push dword ptr [a_z]
        push dword ptr [a_y]
        push dword ptr [a_x]
        mov ecx, self_
        call L_BODY
        jmp L_END
    L_BODY:
        fld dword ptr [esp + 4]
        xor eax, eax
        fcos
        mov dword ptr [ecx + 0x30], eax
        mov dword ptr [ecx + 0x34], eax
        mov dword ptr [ecx + 0x38], eax
        fld dword ptr [esp + 4]
        fsin
        fld dword ptr [esp + 8]
        fcos
        fstp dword ptr [esp + 4]
        fld dword ptr [esp + 8]
        fsin
        fld dword ptr [esp + 0xc]
        fcos
        fld dword ptr [esp + 0xc]
        fsin
        fst dword ptr [esp + 8]
        fmul st(0), st(3)
        fld st(1)
        fmul st(0), st(4)
        fstp dword ptr [esp + 0xc]
        fld st(1)
        fmul dword ptr [esp + 4]
        fld st(1)
        fmul st(0), st(4)
        fsubp st(1), st(0)
        fstp dword ptr [ecx]
        fld dword ptr [esp + 0xc]
        fmul st(0), st(3)
        fld dword ptr [esp + 8]
        fmul dword ptr [esp + 4]
        faddp st(1), st(0)
        fstp dword ptr [ecx + 4]
        fld st(2)
        fmul st(0), st(5)
        fchs
        fstp dword ptr [ecx + 8]
        fld dword ptr [esp + 8]
        fmul st(0), st(5)
        fchs
        fstp dword ptr [ecx + 0x10]
        fld st(1)
        fmul st(0), st(5)
        fstp dword ptr [ecx + 0x14]
        fxch st(3)
        fstp dword ptr [ecx + 0x18]
        fmul st(0), st(1)
        fxch st(2)
        fmul dword ptr [esp + 4]
        faddp st(2), st(0)
        fxch st(1)
        fstp dword ptr [ecx + 0x20]
        fld dword ptr [esp + 8]
        fmul st(0), st(1)
        fld dword ptr [esp + 0xc]
        fmul dword ptr [esp + 4]
        fsubp st(1), st(0)
        fstp dword ptr [ecx + 0x24]
        fstp st(0)
        fld dword ptr [esp + 4]
        fmul st(0), st(1)
        fstp dword ptr [ecx + 0x28]
        fstp st(0)
        ret 12
    L_END:
    }
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
    // 0x59B6A0: fully inlined x87 code. Squared lengths are summed z, y, x on the x87 stack; the reciprocal 1 / sqrt(..) is stored as a float
    // (the x component is scaled with the unrounded one), cy / cz / nz / my / mz / the cross products are spilled to floats.
    const auto NormaliseInline = [](float x, float y, float z) {
        const double recip = 1.0 / std::sqrt((double)z * z + (double)y * y + (double)x * x);
        const float  recipF = (float)recip;
        return CVector{ (float)(x * recip), (float)(recipF * y), (float)(recipF * z) };
    };

    // vecCross = right x forward
    const float cx = m_right.y * m_forward.z - m_forward.y * m_right.z;
    const float cy = m_forward.x * m_right.z - m_right.x * m_forward.z;
    const float cz = m_forward.y * m_right.x - m_forward.x * m_right.y;
    const CVector n1 = NormaliseInline(cx, cy, cz);

    // vecCross2 = forward x vecCross
    const float mx = n1.z * m_forward.y - n1.y * m_forward.z;
    const float my = n1.x * m_forward.z - n1.z * m_forward.x;
    const float mz = n1.y * m_forward.x - n1.x * m_forward.y;
    const CVector n2 = NormaliseInline(mx, my, mz);

    // vecCross3 = vecCross x vecCross2
    const CVector n3{
        n2.z * n1.y - n2.y * n1.z,
        n2.x * n1.z - n2.z * n1.x,
        n2.y * n1.x - n2.x * n1.y,
    };

    m_right   = n2;
    m_forward = n3;
    m_up      = n1;
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

void CMatrix::ConvertToEulerAngles(float* pX, float* pY, float* pZ, uint32 uiFlags) {
    // 0x59A840: // The exe's x87 code VERBATIM (generated from the asm, called inside the __asm block with the original stack frame): every product / sum is rounded at the current
    // precision control and the sine / cosine / arctangent results stay unrounded on the FPU stack exactly like the original.
    alignas(16) static const unsigned char T866D9C[96] = {0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x50,0xCD,0x59,0x00,0x70,0xCC,0x59,0x00,0xC0,0xCC,0x59,0x00,0xE0,0xCC,0x59,0x00,0x10,0xCD,0x59,0x00,0x57,0x49,0x4E,0x53,0x4F,0x43,0x4B,0x2E,0x44,0x4C,0x4C,0x20,0x64,0x6F,0x65,0x73,0x20,0x6E,0x6F,0x74};
    alignas(16) static const unsigned char T866D95[96] = {0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x50,0xCD,0x59,0x00,0x70,0xCC,0x59,0x00,0xC0,0xCC,0x59,0x00,0xE0,0xCC,0x59,0x00,0x10,0xCD,0x59,0x00,0x57,0x49,0x4E,0x53,0x4F,0x43,0x4B,0x2E,0x44,0x4C,0x4C,0x20,0x64};
    alignas(16) static const unsigned char T866D94[96] = {0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x50,0xCD,0x59,0x00,0x70,0xCC,0x59,0x00,0xC0,0xCC,0x59,0x00,0xE0,0xCC,0x59,0x00,0x10,0xCD,0x59,0x00,0x57,0x49,0x4E,0x53,0x4F,0x43,0x4B,0x2E,0x44,0x4C,0x4C,0x20};
    static const uint32 K866D90 = 0x36000000u;
    const float* a_pX = pX;
    const float* a_pY = pY;
    const float* a_pZ = pZ;
    const uint32 a_uiFlags = uiFlags;
    CMatrix* self_ = this;
    __asm {
        push dword ptr [a_uiFlags]
        push dword ptr [a_pZ]
        push dword ptr [a_pY]
        push dword ptr [a_pX]
        mov ecx, self_
        call L_BODY
        jmp L_END
    L_BODY:
        sub esp, 0x28
        mov eax, dword ptr [ecx]
        mov edx, dword ptr [ecx + 4]
        mov dword ptr [esp + 4], eax
        mov eax, dword ptr [ecx + 8]
        mov dword ptr [esp + 0xc], eax
        mov eax, dword ptr [ecx + 0x14]
        mov dword ptr [esp + 0x14], eax
        mov eax, dword ptr [ecx + 0x20]
        mov dword ptr [esp + 8], edx
        mov edx, dword ptr [ecx + 0x10]
        mov dword ptr [esp + 0x1c], eax
        mov eax, dword ptr [ecx + 0x28]
        push ebx
        mov dword ptr [esp + 0x14], edx
        mov edx, dword ptr [ecx + 0x18]
        mov dword ptr [esp + 0x28], eax
        mov eax, dword ptr [esp + 0x3c]
        push ebp
        mov dword ptr [esp + 0x20], edx
        mov edx, dword ptr [ecx + 0x24]
        mov ecx, eax
        shr eax, 1
        push esi
        push edi
        mov edi, eax
        shr eax, 1
        mov ebx, eax
        shr eax, 1
        and ecx, 1
        and eax, 3
        movzx esi, byte ptr [eax + T866D9C + 16]
        mov dword ptr [esp + 0x10], ecx
        and ebx, 1
        mov ecx, esi
        and edi, 1
        sub ecx, ebx
        cmp edi, 1
        movzx ecx, byte ptr [ecx + T866D95 + 16]
        mov dword ptr [esp + 0x30], edx
        movzx edx, byte ptr [esi + ebx + T866D94 + 16]
        jne L_59A965
        lea eax, [esi + esi*2]
        lea edi, [eax + ecx]
        fld dword ptr [esp + edi*4 + 0x14]
        lea ebp, [eax + edx]
        fld dword ptr [esp + ebp*4 + 0x14]
        lea edi, [esp + edi*4 + 0x14]
        fld st(0)
        lea ebp, [esp + ebp*4 + 0x14]
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fsqrt
        fstp st(2)
        fstp st(0)
        fcom dword ptr [K866D90]
        fnstsw ax
        test ah, 0x41
        jne L_59A93D
        fld dword ptr [ebp]
        mov eax, dword ptr [esp + 0x3c]
        fld dword ptr [edi]
        mov edi, esi
        fpatan
        shl edi, 4
        lea ebp, [esi + edx*2]
        add edx, ebp
        fstp dword ptr [eax]
        fld dword ptr [esp + edi + 0x14]
        mov edi, dword ptr [esp + 0x40]
        fpatan
        fstp dword ptr [edi]
        fld dword ptr [esp + edx*4 + 0x14]
        lea edx, [esi + ecx*2]
        add ecx, edx
        fld dword ptr [esp + ecx*4 + 0x14]
        fchs
        mov ecx, dword ptr [esp + 0x44]
        fpatan
        fstp dword ptr [ecx]
        jmp L_59AA0E
    L_59A93D:
        lea eax, [ecx + edx*2]
        mov ecx, edx
        add ecx, eax
        fld dword ptr [esp + ecx*4 + 0x14]
        shl edx, 4
        fchs
        mov eax, dword ptr [esp + 0x3c]
        fld dword ptr [esp + edx + 0x14]
        fpatan
        shl esi, 4
        fstp dword ptr [eax]
        fld dword ptr [esp + esi + 0x14]
        jmp L_59A9FC
    L_59A965:
        lea edi, [edx + edx*2]
        mov eax, esi
        shl eax, 4
        lea ebp, [edi + esi]
        fld dword ptr [esp + ebp*4 + 0x14]
        lea eax, [esp + eax + 0x14]
        fld dword ptr [eax]
        lea ebp, [esp + ebp*4 + 0x14]
        fld st(1)
        mov dword ptr [esp + 0x48], eax
        fmulp st(2), st(0)
        fld st(0)
        fmul st(0), st(1)
        faddp st(2), st(0)
        fxch st(1)
        fsqrt
        fstp st(1)
        fcom dword ptr [K866D90]
        fnstsw ax
        test ah, 0x41
        mov eax, dword ptr [esp + 0x3c]
        jne L_59A9DC
        lea edi, [ecx + ecx*2]
        add edx, edi
        fld dword ptr [esp + edx*4 + 0x14]
        shl ecx, 4
        fld dword ptr [esp + ecx + 0x14]
        fpatan
        add edi, esi
        mov ecx, dword ptr [esp + 0x48]
        fstp dword ptr [eax]
        fld dword ptr [esp + edi*4 + 0x14]
        mov edi, dword ptr [esp + 0x40]
        fchs
        fxch st(1)
        fpatan
        fstp dword ptr [edi]
        fld dword ptr [ebp]
        fld dword ptr [ecx]
        mov ecx, dword ptr [esp + 0x44]
        fpatan
        fstp dword ptr [ecx]
        jmp L_59AA0E
    L_59A9DC:
        add edi, ecx
        fld dword ptr [esp + edi*4 + 0x14]
        shl edx, 4
        fchs
        fld dword ptr [esp + edx + 0x14]
        fpatan
        lea edx, [esi + ecx*2]
        add ecx, edx
        fstp dword ptr [eax]
        fld dword ptr [esp + ecx*4 + 0x14]
        fchs
        fxch st(1)
    L_59A9FC:
        fpatan
        mov edi, dword ptr [esp + 0x40]
        mov ecx, dword ptr [esp + 0x44]
        fstp dword ptr [edi]
        mov dword ptr [ecx], 0
    L_59AA0E:
        cmp ebx, 1
        jne L_59AA25
        fld dword ptr [eax]
        fchs
        fstp dword ptr [eax]
        fld dword ptr [edi]
        fchs
        fstp dword ptr [edi]
        fld dword ptr [ecx]
        fchs
        fstp dword ptr [ecx]
    L_59AA25:
        cmp dword ptr [esp + 0x10], 1
        pop edi
        pop esi
        pop ebp
        pop ebx
        jne L_59AA38
        mov edx, dword ptr [ecx]
        fld dword ptr [eax]
        mov dword ptr [eax], edx
        fstp dword ptr [ecx]
    L_59AA38:
        add esp, 0x28
        ret 16
    L_END:
    }
}

void CMatrix::ConvertFromEulerAngles(float x, float y, float z, uint32 uiFlags) {
    // 0x59AA40: // The exe's x87 code VERBATIM (generated from the asm, called inside the __asm block with the original stack frame): every product / sum is rounded at the current
    // precision control and the sine / cosine / arctangent results stay unrounded on the FPU stack exactly like the original.
    alignas(16) static const unsigned char T866D9C[96] = {0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x50,0xCD,0x59,0x00,0x70,0xCC,0x59,0x00,0xC0,0xCC,0x59,0x00,0xE0,0xCC,0x59,0x00,0x10,0xCD,0x59,0x00,0x57,0x49,0x4E,0x53,0x4F,0x43,0x4B,0x2E,0x44,0x4C,0x4C,0x20,0x64,0x6F,0x65,0x73,0x20,0x6E,0x6F,0x74};
    alignas(16) static const unsigned char T866D94[96] = {0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x50,0xCD,0x59,0x00,0x70,0xCC,0x59,0x00,0xC0,0xCC,0x59,0x00,0xE0,0xCC,0x59,0x00,0x10,0xCD,0x59,0x00,0x57,0x49,0x4E,0x53,0x4F,0x43,0x4B,0x2E,0x44,0x4C,0x4C,0x20};
    alignas(16) static const unsigned char T866D95[96] = {0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x00,0x00,0x00,0x36,0x01,0x02,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x0A,0xD7,0xA3,0x3B,0x66,0x66,0x66,0x40,0x00,0x00,0x48,0x42,0x70,0xCB,0x59,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x3A,0x26,0x82,0x00,0x50,0xCD,0x59,0x00,0x70,0xCC,0x59,0x00,0xC0,0xCC,0x59,0x00,0xE0,0xCC,0x59,0x00,0x10,0xCD,0x59,0x00,0x57,0x49,0x4E,0x53,0x4F,0x43,0x4B,0x2E,0x44,0x4C,0x4C,0x20,0x64};
    const float a_x = x;
    const float a_y = y;
    const float a_z = z;
    const uint32 a_uiFlags = uiFlags;
    CMatrix* self_ = this;
    __asm {
        push dword ptr [a_uiFlags]
        push dword ptr [a_z]
        push dword ptr [a_y]
        push dword ptr [a_x]
        mov ecx, self_
        call L_BODY
        jmp L_END
    L_BODY:
        mov eax, dword ptr [esp + 0x10]
        sub esp, 0x38
        push ebx
        mov ebx, eax
        shr eax, 1
        push ebp
        mov ebp, eax
        shr eax, 1
        push esi
        push edi
        mov edi, eax
        shr eax, 1
        and eax, 3
        movzx edx, byte ptr [eax + T866D9C + 16]
        and edi, 1
        movzx esi, byte ptr [edx + edi + T866D94 + 16]
        mov eax, edx
        and ebx, 1
        sub eax, edi
        movzx eax, byte ptr [eax + T866D95 + 16]
        and ebp, 1
        cmp ebx, 1
        jne L_59AA94
        mov ebx, dword ptr [esp + 0x4c]
        fld dword ptr [esp + 0x54]
        mov dword ptr [esp + 0x4c], ebx
        mov dword ptr [esp + 0x54], ebx
        jmp L_59AA98
    L_59AA94:
        fld dword ptr [esp + 0x4c]
    L_59AA98:
        cmp edi, 1
        jne L_59AAB1
        fchs
        fld dword ptr [esp + 0x50]
        fchs
        fld dword ptr [esp + 0x54]
        fchs
        fstp dword ptr [esp + 0x54]
        jmp L_59AAB5
    L_59AAB1:
        fld dword ptr [esp + 0x50]
    L_59AAB5:
        fld st(1)
        mov edi, edx
        fcos
        shl edi, 4
        cmp ebp, 1
        fstp dword ptr [esp + 0x20]
        fld st(0)
        fcos
        fstp dword ptr [esp + 0x50]
        fld dword ptr [esp + 0x54]
        fcos
        fstp dword ptr [esp + 0x58]
        fxch st(1)
        fsin
        fstp dword ptr [esp + 0x1c]
        fsin
        fstp dword ptr [esp + 0x4c]
        fld dword ptr [esp + 0x54]
        fsin
        fld dword ptr [esp + 0x58]
        fmul dword ptr [esp + 0x20]
        fstp dword ptr [esp + 0x10]
        fld dword ptr [esp + 0x20]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x14]
        fld dword ptr [esp + 0x1c]
        fmul dword ptr [esp + 0x58]
        fstp dword ptr [esp + 0x18]
        fld dword ptr [esp + 0x1c]
        fmul st(0), st(1)
        fstp dword ptr [esp + 0x54]
        jne L_59ABB2
        fld dword ptr [esp + 0x50]
        fstp dword ptr [esp + edi + 0x24]
        lea edi, [edx + edx*2]
        fld dword ptr [esp + 0x4c]
        lea ebx, [edi + esi]
        fmul dword ptr [esp + 0x1c]
        add edi, eax
        fstp dword ptr [esp + ebx*4 + 0x24]
        fld dword ptr [esp + 0x4c]
        fmul dword ptr [esp + 0x20]
        fstp dword ptr [esp + edi*4 + 0x24]
        lea edi, [esi + esi*2]
        lea ebx, [edi + edx]
        fmul dword ptr [esp + 0x4c]
        add edi, eax
        fstp dword ptr [esp + ebx*4 + 0x24]
        mov ebx, esi
        fld dword ptr [esp + 0x54]
        shl ebx, 4
        fmul dword ptr [esp + 0x50]
        fsubr dword ptr [esp + 0x10]
        fstp dword ptr [esp + ebx + 0x24]
        fld dword ptr [esp + 0x14]
        fmul dword ptr [esp + 0x50]
        fchs
        fsub dword ptr [esp + 0x18]
        fstp dword ptr [esp + edi*4 + 0x24]
        lea edi, [eax + eax*2]
        fld dword ptr [esp + 0x4c]
        add edx, edi
        fmul dword ptr [esp + 0x58]
        add edi, esi
        fchs
        fstp dword ptr [esp + edx*4 + 0x24]
        fld dword ptr [esp + 0x18]
        fmul dword ptr [esp + 0x50]
        fadd dword ptr [esp + 0x14]
        fstp dword ptr [esp + edi*4 + 0x24]
        fld dword ptr [esp + 0x10]
        fmul dword ptr [esp + 0x50]
        fsub dword ptr [esp + 0x54]
        jmp L_59AC40
    L_59ABB2:
        fld dword ptr [esp + 0x58]
        fmul dword ptr [esp + 0x50]
        fstp dword ptr [esp + edi + 0x24]
        lea edi, [edx + edx*2]
        fld dword ptr [esp + 0x18]
        lea ebx, [edi + esi]
        fmul dword ptr [esp + 0x4c]
        add edi, eax
        fsub dword ptr [esp + 0x14]
        fstp dword ptr [esp + ebx*4 + 0x24]
        fld dword ptr [esp + 0x10]
        fmul dword ptr [esp + 0x4c]
        fadd dword ptr [esp + 0x54]
        fstp dword ptr [esp + edi*4 + 0x24]
        lea edi, [esi + esi*2]
        lea ebx, [edi + edx]
        fmul dword ptr [esp + 0x50]
        add edi, eax
        fstp dword ptr [esp + ebx*4 + 0x24]
        mov ebx, esi
        fld dword ptr [esp + 0x54]
        shl ebx, 4
        fmul dword ptr [esp + 0x4c]
        fadd dword ptr [esp + 0x10]
        fstp dword ptr [esp + ebx + 0x24]
        fld dword ptr [esp + 0x14]
        fmul dword ptr [esp + 0x4c]
        fsub dword ptr [esp + 0x18]
        fstp dword ptr [esp + edi*4 + 0x24]
        lea edi, [eax + eax*2]
        fld dword ptr [esp + 0x4c]
        add edx, edi
        fchs
        add edi, esi
        fstp dword ptr [esp + edx*4 + 0x24]
        fld dword ptr [esp + 0x1c]
        fmul dword ptr [esp + 0x50]
        fstp dword ptr [esp + edi*4 + 0x24]
        fld dword ptr [esp + 0x50]
        fmul dword ptr [esp + 0x20]
    L_59AC40:
        shl eax, 4
        fstp dword ptr [esp + eax + 0x24]
        mov eax, dword ptr [esp + 0x24]
        mov edx, dword ptr [esp + 0x28]
        mov dword ptr [ecx], eax
        mov eax, dword ptr [esp + 0x2c]
        mov dword ptr [ecx + 4], edx
        mov edx, dword ptr [esp + 0x30]
        mov dword ptr [ecx + 8], eax
        mov eax, dword ptr [esp + 0x34]
        pop edi
        mov dword ptr [ecx + 0x10], edx
        mov edx, dword ptr [esp + 0x34]
        mov dword ptr [ecx + 0x14], eax
        mov eax, dword ptr [esp + 0x38]
        pop esi
        mov dword ptr [ecx + 0x18], edx
        mov edx, dword ptr [esp + 0x38]
        mov dword ptr [ecx + 0x20], eax
        mov eax, dword ptr [esp + 0x3c]
        pop ebp
        mov dword ptr [ecx + 0x24], edx
        mov dword ptr [ecx + 0x28], eax
        pop ebx
        add esp, 0x38
        ret 16
    L_END:
    }
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
