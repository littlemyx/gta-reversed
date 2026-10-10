#pragma once

#include "Vector.h"

class CVehicle;
#ifndef NOTSA_RW_LIBRW // fakerw: the RW names are aliases of rw::* types, they cannot be forward-declared as structs
struct RwFrame;
#endif

class  CBouncingPanel {
    static inline NOTSA_GLOBAL(BOUNCE_SPRING_DAMP_MULT, 0x8D3954, (float), { 0.95f }); // 0.95
    static inline NOTSA_GLOBAL(BOUNCE_SPRING_RETURN_MULT, 0x8D3958, (float), { 0.1f }); // 0.1
    static inline NOTSA_GLOBAL(BOUNCE_VEL_CHANGE_LIMIT, 0x8D395C, (float), { 0.1f }); // 0.1
    static inline NOTSA_GLOBAL(BOUNCE_HANGING_DAMP_MULT, 0x8D3960, (float), { 0.98f }); // 0.98
    static inline NOTSA_GLOBAL(BOUNCE_HANGING_RETURN_MULT, 0x8D3964, (float), { 0.02f }); // 0.02

public:
    uint16 m_nFrameId{(uint16)-1};
    uint16 m_nAxis{};
    float    m_fAngleLimit{};
    CVector  m_vecRotation{};
    CVector  m_vecPos{};

public:
    CBouncingPanel() = default;

    static void InjectHooks();

    void ResetPanel();
    void SetPanel(int16 frameId, int16 axis, float angleLimit);
    float GetAngleChange(float velocity) const;
    void ProcessPanel(CVehicle* vehicle, RwFrame* frame, CVector arg2, CVector arg3, float arg4, float arg5);
};
VALIDATE_SIZE(CBouncingPanel, 0x20);
