/*
    P2B-01: RwMatrix* / RwV2d* / RwV3d* of the RW C API on top of librw (rw::Matrix, rw::V3d).
    Semantics checked against the original exe (gta_sa_compact.exe, RW 3.6 matrix.c / vector.c):
      * RwMatrixUpdate        0x7F18A0: flags &= ~(IDENTITY | TYPEMASK)                                        == Matrix::update
      * RwMatrixMultiply      0x7F18B0: identity shortcuts (copy the other operand), else out = a*b, flags = a.flags & b.flags      == Matrix::mult
      * RwMatrixInvert        0x7F2070: identity -> copy; ORTHONORMAL -> transpose + flags 3; otherwise general, flags = 0
                              (librw's invertGeneral leaves dst->flags alone, so it is cleared here)
      * RwMatrixRotate        0x7F1FD0: degrees, axis normalised, REPLACE flags 3, PRE/POST = rot*m / m*rot with identity shortcut == Matrix::rotate
      * RwMatrixTranslate     0x7F2450: PRE/POST only add to pos (type flags kept, IDENTITY cleared) -> written out here (no multiply)
      * RwMatrixScale         0x7F22C0: result flags &= ~(IDENTITY | TYPEMASK) (a scaled matrix is no longer orthonormal; librw keeps the type bits)
      * RwMatrixTransform     0x7F25A0: REPLACE copy; PRE/POST like Multiply                                    == Matrix::transform
    Differences kept on purpose: RwV3dNormalize/Length use exact sqrtf (RW 3.6 used a table-based approximation), and Multiply/Invert are safe when
    the output aliases an input (the original generic code was not, callers do not rely on that).
*/
#ifdef NOTSA_RW_UNIT_TEST
#include <rwcore.h>
#else
#include "StdInc.h"
#endif

#include <cmath>
#ifdef NOTSA_RW_LIBRW

namespace {
constexpr uint32_t kIdentity = rw::Matrix::IDENTITY;
constexpr uint32_t kTypeMask = rw::Matrix::TYPEMASK;
inline rw::CombineOp ToOp(RwOpCombineType op) { return static_cast<rw::CombineOp>(op); }
} // namespace

RwMatrix* RwMatrixCreate() {
    return rw::Matrix::create();
}

RwBool RwMatrixDestroy(RwMatrix* mpMat) {
    if (mpMat) {
        mpMat->destroy();
    }
    return TRUE;
}

RwMatrix* RwMatrixUpdate(RwMatrix* matrix) {
    matrix->update();
    return matrix;
}

RwMatrix* RwMatrixMultiply(RwMatrix* matrixOut, const RwMatrix* MatrixIn1, const RwMatrix* matrixIn2) {
    if (matrixOut == MatrixIn1 || matrixOut == matrixIn2) {
        RwMatrix tmp;
        rw::Matrix::mult(&tmp, MatrixIn1, matrixIn2);
        *matrixOut = tmp;
    } else {
        rw::Matrix::mult(matrixOut, MatrixIn1, matrixIn2);
    }
    return matrixOut;
}

RwMatrix* RwMatrixInvert(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    if (matrixIn->flags & kIdentity) {
        *matrixOut = *matrixIn;
    } else if ((matrixIn->flags & kTypeMask) == rw::Matrix::TYPEORTHONORMAL) {
        if (matrixOut == matrixIn) {
            RwMatrix tmp;
            rw::Matrix::invertOrthonormal(&tmp, matrixIn);
            *matrixOut = tmp;
        } else {
            rw::Matrix::invertOrthonormal(matrixOut, matrixIn);
        }
    } else {
        if (matrixOut == matrixIn) {
            RwMatrix tmp;
            rw::Matrix::invertGeneral(&tmp, matrixIn);
            tmp.flags = 0;
            *matrixOut = tmp;
        } else {
            rw::Matrix::invertGeneral(matrixOut, matrixIn);
            matrixOut->flags = 0;
        }
    }
    return matrixOut;
}

RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    return matrix->rotate(axis, angle, ToOp(combineOp));
}

RwMatrix* RwMatrixScale(RwMatrix* matrix, const RwV3d* scale, RwOpCombineType combineOp) {
    matrix->scale(scale, ToOp(combineOp));
    matrix->flags &= ~(kIdentity | kTypeMask);
    return matrix;
}

RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp) {
    return matrix->transform(transform, ToOp(combineOp));
}

RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp) {
    switch (combineOp) {
    case rwCOMBINEREPLACE:
        matrix->right = { 1.0f, 0.0f, 0.0f };
        matrix->up    = { 0.0f, 1.0f, 0.0f };
        matrix->at    = { 0.0f, 0.0f, 1.0f };
        matrix->pos   = *translation;
        matrix->flags = (matrix->flags | kTypeMask) & ~kIdentity;
        break;
    case rwCOMBINEPRECONCAT: // pos += translation * (3x3 part of the matrix)
        matrix->pos.x += translation->x * matrix->right.x + translation->y * matrix->up.x + translation->z * matrix->at.x;
        matrix->pos.y += translation->x * matrix->right.y + translation->y * matrix->up.y + translation->z * matrix->at.y;
        matrix->pos.z += translation->x * matrix->right.z + translation->y * matrix->up.z + translation->z * matrix->at.z;
        matrix->flags &= ~kIdentity;
        break;
    case rwCOMBINEPOSTCONCAT:
        matrix->pos.x += translation->x;
        matrix->pos.y += translation->y;
        matrix->pos.z += translation->z;
        matrix->flags &= ~kIdentity;
        break;
    default: // the original raises an RW error and returns NULL
        return nullptr;
    }
    return matrix;
}

//--------------------------------------------------------------------------------------------------
// Vectors
//--------------------------------------------------------------------------------------------------
RwReal RwV2dLength(const RwV2d* in) {
    return std::sqrt(in->x * in->x + in->y * in->y);
}

RwReal RwV3dLength(const RwV3d* in) {
    return std::sqrt(in->x * in->x + in->y * in->y + in->z * in->z);
}

// Returns the length of `in`; a zero-length input gives a zero vector and 0 (no NaN), like the original
RwReal RwV3dNormalize(RwV3d* out, const RwV3d* in) {
    const RwReal lengthSq = in->x * in->x + in->y * in->y + in->z * in->z;
    const RwReal recip    = lengthSq != 0.0f ? 1.0f / std::sqrt(lengthSq) : 0.0f;
    out->x = in->x * recip;
    out->y = in->y * recip;
    out->z = in->z * recip;
    return lengthSq * recip;
}

RwV3d* RwV3dTransformPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix) {
    rw::V3d::transformPoints(pointOut, pointIn, 1, matrix);
    return pointOut;
}

RwV3d* RwV3dTransformPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    rw::V3d::transformPoints(pointsOut, pointsIn, numPoints, matrix);
    return pointsOut;
}

RwV3d* RwV3dTransformVector(RwV3d* vectorOut, const RwV3d* vectorIn, const RwMatrix* matrix) {
    rw::V3d::transformVectors(vectorOut, vectorIn, 1, matrix);
    return vectorOut;
}

RwV3d* RwV3dTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    rw::V3d::transformVectors(vectorsOut, vectorsIn, numPoints, matrix);
    return vectorsOut;
}

#endif // NOTSA_RW_LIBRW
