#include "StdInc.h"
#include "EventGunShotWhizzedBy.h"


void CEventGunShotWhizzedBy::InjectHooks()
{
    RH_ScopedVirtualClass(CEventGunShotWhizzedBy, 0x85B730, 17);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x4B1FD0);
    RH_ScopedVMTInstall(AffectsPed, 0x4B5120);
}

// 0x4B1FD0
CEventGunShotWhizzedBy::CEventGunShotWhizzedBy(CEntity* entity, const CVector& startPoint, const CVector& endPoint, bool bHasNoSound) :
    CEventGunShot(entity, startPoint, endPoint, bHasNoSound)
{
}

// 0x4B1FD0
CEventGunShotWhizzedBy* CEventGunShotWhizzedBy::Constructor(CEntity* entity, const CVector& startPoint, const CVector& endPoint, bool bHasNoSound)
{
    this->CEventGunShotWhizzedBy::CEventGunShotWhizzedBy(entity, startPoint, endPoint, bHasNoSound);
    return this;
}

// 0x4B5120
bool CEventGunShotWhizzedBy::AffectsPed(CPed* ped)
{
    if (!ped->IsPlayer() && ped->IsAlive()) {
        CVector direction = m_endPoint - m_startPoint;
        direction.Normalise();
        CVector distance = ped->GetPosition() - m_startPoint;
        // exe 0x4B5149-0x4B51A2: the projection length is the dot product (z,y,x order); the port squared the component-wise product
        const float proj = distance.z * direction.z + distance.y * direction.y + distance.x * direction.x;
        if (proj > 0.0f) {
            // exe 0x4B51D4-0x4B5224: only x/y of the closest point are used (the port's CVector2D -> CVector conversion added a z term)
            const float dx = ped->GetPosition().x - (direction.x * proj + m_startPoint.x);
            const float dy = ped->GetPosition().y - (direction.y * proj + m_startPoint.y);
            return 2.0f * 2.0f > dy * dy + dx * dx;
        }
    }
    return false;
}