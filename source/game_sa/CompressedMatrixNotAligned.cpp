#include "StdInc.h"

#include "CompressedMatrixNotAligned.h"

void CCompressedMatrixNotAligned::InjectHooks()
{
    RH_ScopedClass(CCompressedMatrixNotAligned);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(DecompressIntoFullMatrix, 0x59B9F0);
    RH_ScopedInstall(CompressFromFullMatrix, 0x59BAD0);
}

void CCompressedMatrixNotAligned::DecompressIntoFullMatrix(CMatrix& matrix) const
{
    // 0x59BA0C: the exe multiplies the signed bytes by the float constant 0x859BCC (= 0.0078740157f), `FixedFloat` divides by 127 (different last bit)
    const auto* const comp = reinterpret_cast<const int8*>(this) + sizeof(CVector);
    const auto Dec = [](int8 v) { return (float)v * std::bit_cast<float>(0x3C010204u); };
    matrix.GetRight()   = CVector{ Dec(comp[0]), Dec(comp[1]), Dec(comp[2]) };
    matrix.GetForward() = CVector{ Dec(comp[3]), Dec(comp[4]), Dec(comp[5]) };
    matrix.GetUp() = CrossProduct(matrix.GetRight(), matrix.GetForward());
    matrix.GetPosition() = m_vecPos;
    matrix.Reorthogonalise();
}

void CCompressedMatrixNotAligned::CompressFromFullMatrix(const CMatrix& matrix)
{
    m_vecRight = matrix.GetRight();
    m_vecForward = matrix.GetForward();
    m_vecPos = matrix.GetPosition();
}

