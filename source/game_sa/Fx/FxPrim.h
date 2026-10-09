#pragma once

class FxPrimBP_c;
class FxSystem_c;

class NOTSA_EXPORT_VTABLE FxPrim_c {
public:
    FxPrimBP_c* m_PrimBP;
    FxSystem_c* m_System;
    bool        m_bEnabled;

public:
    FxPrim_c();
    virtual ~FxPrim_c() = 0;

    virtual bool Init(FxPrimBP_c* prim, FxSystem_c* system) = 0;
    virtual void Update(float a2, float a3) = 0;
    virtual void Reset() = 0;
    // NOTE: MSVC lays out overloaded virtuals in reverse declaration order; the exe vtable is [Mat (0x4A4050), Vec (0x4A3EA0)], so Vec is declared first
    virtual void AddParticle(const CVector& pos, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ, float brightness, bool createLocal) = 0;
    virtual void AddParticle(const RwMatrix& mat, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults, float rotZ, float brightness, bool createLocal) = 0;

    void Enable(bool enabled);
};
