/*
    P2B-01 / 01r: RwMatrix* / RwV2d* / RwV3d* of the RW C API. The numerics are the exe's (RW 3.6 matrix.c / vector.c), ported from the asm in
    rwmath_exact.h (table-based sqrt/inverse sqrt, the x87 evaluation order and float spill points of each function); librw's Matrix:: functions are
    NOT used because their summation order and sqrt differ. Exe addresses:
      RwMatrixUpdate 0x7F18A0, Multiply 0x7F18B0 (kernel 0x7F12F0), OrthoNormalize 0x7F1920, Rotate 0x7F1FD0 (+ OneMinusCosineSine 0x7F1D00),
      Invert 0x7F2070 (general 0x7F2160), Scale 0x7F22C0, Translate 0x7F2450, Transform 0x7F25A0, RwV3dNormalize 0x7ED9B0, RwV3dLength 0x7EDAC0,
      RwV2dLength 0x7EDBF0, RwV3dTransformPoint 0x7EDD60 (kernel 0x7ED730) / Points 0x7EDD90 (0x7ED670) / Vector 0x7EDDC0 (0x7ED880) / Vectors (0x7ED7D0).
    Flags: identity shortcuts copy the other operand with its flags; Multiply/Transform flags = a & b; Invert: identity -> copy, orthonormal -> transpose
    with flags 3, else general with flags 0; Scale clears IDENTITY|TYPEMASK; Translate keeps the type bits and clears IDENTITY; Rotate/Update as RW.
    Deliberate differences: Multiply/Invert are safe when the output aliases an input (the exe's generic code was not), invalid combine ops return NULL
    without raising an RW error, a zero vector is not reported through RwError.
*/
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "rwmath_exact.h"

RwMatrix* RwMatrixCreate() {
    return rw::Matrix::create();
}

RwBool RwMatrixDestroy(RwMatrix* mpMat) {
    if (mpMat) {
        mpMat->destroy();
    }
    return TRUE;
}

// 0x7F18A0: flags &= ~(IDENTITY | TYPEMASK)
RwMatrix* RwMatrixUpdate(RwMatrix* matrix) {
    matrix->flags &= ~(rwx::kIdentity | rwx::kTypeMask);
    return matrix;
}

RwMatrix* RwMatrixMultiply(RwMatrix* matrixOut, const RwMatrix* MatrixIn1, const RwMatrix* matrixIn2) {
    return rwx::Multiply(matrixOut, MatrixIn1, matrixIn2);
}

RwMatrix* RwMatrixInvert(RwMatrix* matrixOut, const RwMatrix* matrixIn) {
    return rwx::Invert(matrixOut, matrixIn);
}

RwMatrix* RwMatrixRotate(RwMatrix* matrix, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    return rwx::Rotate(matrix, axis, angle, static_cast<int>(combineOp));
}

RwMatrix* RwMatrixScale(RwMatrix* matrix, const RwV3d* scale, RwOpCombineType combineOp) {
    return rwx::Scale(matrix, scale, static_cast<int>(combineOp));
}

RwMatrix* RwMatrixTransform(RwMatrix* matrix, const RwMatrix* transform, RwOpCombineType combineOp) {
    return rwx::Transform(matrix, transform, static_cast<int>(combineOp));
}

RwMatrix* RwMatrixTranslate(RwMatrix* matrix, const RwV3d* translation, RwOpCombineType combineOp) {
    return rwx::Translate(matrix, translation, static_cast<int>(combineOp));
}

RwMatrix* RwMatrixOrthoNormalize(RwMatrix* dst, const RwMatrix* src) {
    return rwx::OrthoNormalize(dst, src);
}

//--------------------------------------------------------------------------------------------------
// Vectors
//--------------------------------------------------------------------------------------------------
RwReal RwV2dLength(const RwV2d* in) {
    return rwx::V2dLength(in);
}

RwReal RwV3dLength(const RwV3d* in) {
    return rwx::V3dLength(in);
}

// Returns the (approximate, table based) length of `in`; a zero-length input gives a zero vector and 0, like the exe
RwReal RwV3dNormalize(RwV3d* out, const RwV3d* in) {
    return rwx::V3dNormalize(out, in);
}

RwV3d* RwV3dTransformPoint(RwV3d* pointOut, const RwV3d* pointIn, const RwMatrix* matrix) {
    rwx::TransformPoint(pointOut, pointIn, matrix);
    return pointOut;
}

RwV3d* RwV3dTransformPoints(RwV3d* pointsOut, const RwV3d* pointsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    for (RwInt32 i = 0; i < numPoints; i++) {
        rwx::TransformPoint(&pointsOut[i], &pointsIn[i], matrix);
    }
    return pointsOut;
}

RwV3d* RwV3dTransformVector(RwV3d* vectorOut, const RwV3d* vectorIn, const RwMatrix* matrix) {
    rwx::TransformVector(vectorOut, vectorIn, matrix);
    return vectorOut;
}

RwV3d* RwV3dTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, RwInt32 numPoints, const RwMatrix* matrix) {
    for (RwInt32 i = 0; i < numPoints; i++) {
        rwx::TransformVector(&vectorsOut[i], &vectorsIn[i], matrix);
    }
    return vectorsOut;
}

#endif // NOTSA_RW_LIBRW
