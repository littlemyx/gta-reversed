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

    float level = 1000.0f;
    if (GetEventType() == EVENT_SOUND_QUIET) {
        CVector& sourcePos = static_cast<CEventSoundQuiet*>(this)->m_position;
        level = std::min((position - sourcePos).Magnitude(), 1.0f);
    } else if (GetSourceEntity()) {
        CVector& sourcePos = GetSourceEntity()->GetPosition();
        level = std::min((position - sourcePos).Magnitude(), 1.0f);
    }
    return (GetLocalSoundLevel() - (std::log10f(level * level) * 10.0f)) - (level * ExeRecip(100.0f));
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

