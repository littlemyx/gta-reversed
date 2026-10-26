/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Matrix.h"

class CPlaceable;

class CMatrixLink : public CMatrix {
public:
    CMatrixLink() : CMatrix() {}
    CMatrixLink(float fScale) : CMatrix() { SetScale(fScale); }
#ifdef NOTSA_PORTABLE
    // STAB2: the implicit copy assignment also copies m_pOwner / m_pPrev / m_pNext. `*car->m_matrix = *vehicle->m_matrix` (CCarCtrl, police / convoy cars: 0x59BBC0 is CMatrix::operator=,
    // the exe copies the 3x4 matrix only) therefore made the car's link a clone of the donor's: owner = donor, prev / next = the donor's neighbours. The lists were spliced wrongly and
    // the clone stayed behind as an ownerless link (owner dead / its m_matrix null) in list 1 for ever -> the "oldest link has no owner" null deref in CPlaceable::AllocateMatrix.
    CMatrixLink& operator=(const CMatrixLink& o) { CMatrix::operator=(o); return *this; }
    CMatrixLink(const CMatrixLink&) = default;
#endif

public:
    CPlaceable*  m_pOwner;
    CMatrixLink* m_pPrev;
    CMatrixLink* m_pNext;

public:
    static void InjectHooks();

    NOTSA_INLINE_IN_CPP void Insert(CMatrixLink* pWhere);
    NOTSA_INLINE_IN_CPP void Remove();
};

VALIDATE_SIZE(CMatrixLink, 0x54);
