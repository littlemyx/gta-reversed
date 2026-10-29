#include "StdInc.h"
#include "Event.h"


void CEvent::InjectHooks() {
    RH_ScopedVirtualClass(CEvent, 0x85AB38, 16);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x4ABFC0);
    RH_ScopedInstall(CalcSoundLevelIncrement, 0x4AC050);
    RH_ScopedInstall(GetSoundLevel, 0x4B2850);
}

// 0x4B5620
#ifdef NOTSA_PORTABLE
void* CEvent::operator new(size_t size) {
    GetEventPool()->NotsaCheckAllocSize(size, "event");
    return GetEventPool()->New();
}
#else
void* CEvent::operator new(size_t) {
    return GetEventPool()->New();
}
#endif

// 0x4B5630
void CEvent::operator delete(void* object) {
    GetEventPool()->Delete(static_cast<CEvent*>(object));
}

// 0x4ABFC0
CEvent::CEvent() {
    m_nTimeActive = 0;
    m_bValid = false;
}

CEvent::~CEvent() {
    // nothing here
}

// 0x4ABFC0
CEvent* CEvent::Constructor() {
    this->CEvent::CEvent();
    return this;
}

// 0x4B2850
float CEvent::GetSoundLevel(const CEntity* entity, CVector& position) {
    if ((entity && entity != GetSourceEntity()) || GetLocalSoundLevel() == 0.0f)
        return 0.0f;

    // 0x4B2884-0x4B293A: the distance is raised TO AT LEAST 1.0 (fcomp 1.0, jp keeps it when >= 1; was std::min = at most 1, which cancelled the falloff)
    // the distance is sqrt((dz^2 + dy^2) + dx^2) (0x4B28E4: the differences are pushed x, y, z, so z is squared first)
    const auto DistanceZYX = [&](const CVector& to) {
        const float dx = position.x - to.x, dy = position.y - to.y, dz = position.z - to.z;
        return (float)std::sqrt(((x87d)dz * dz + (x87d)dy * dy) + (x87d)dx * dx);
    };
    float level = 1000.0f;
    if (GetEventType() == EVENT_SOUND_QUIET) {
        CVector& sourcePos = static_cast<CEventSoundQuiet*>(this)->m_position;
        level = DistanceZYX(sourcePos);
        if (level < 1.0f) {
            level = 1.0f;
        }
    } else if (GetSourceEntity()) {
        CVector& sourcePos = GetSourceEntity()->GetPosition();
        level = DistanceZYX(sourcePos);
        if (level < 1.0f) {
            level = 1.0f;
        }
    }
    // fldlg2; fyl2x on the 24-bit level * level (0x4B2920), the log10 stays unrounded into `* 10.0f`; then (local - that) - level * 0x858C58 (0.01f), each op rounded to 24 bits
    const auto lg10 = X87R(x87::log10(X87W((x87d)(level * level))) * (x87d)10.0);
    return (float)X87R(X87R((x87d)GetLocalSoundLevel() - lg10) - (x87d)(level * ExeRecip(100.0f)));
}

// 0x4AC050
float CEvent::CalcSoundLevelIncrement(float level1, float level2) {
    if (level2 == 0.0f)
        return 0.0f;

    if (level1 == 0.0f)
        return level2;

    // exe 0x4AC08E/0x4AC0A7: pow #1 is stored as a float (fstp dword), pow #2 stays unrounded in the sum; fldlg2/fyl2x on the 24-bit quotient
    level1 = (float)x87::pow(10.0, X87W((x87d)(level1 * 0.1f)));
    const auto pow2 = x87::pow(10.0, X87W((x87d)(level2 * 0.1f)));
    return (float)(X87R(x87::log10(X87W(X87R(X87R(pow2 + (x87d)level1) / (x87d)level1))) * (x87d)10.0));
}

