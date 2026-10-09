/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "TaskTimer.h"

class CEntity;
class CPed;
struct C2dEffect;

class CAttractorScanner {
public:
    char       field_0;
    char       _pad[3];
    CTaskTimer field_4;
    C2dEffect* m_pEffectInUse;              // 0x10
    CEntity*   m_pEntityInUse;              // 0x14
    int32      field_18[10];
    int32      field_40[10];
    float      field_68[10];

public:
    static void InjectHooks();

    void Clear();
    void ScanForAttractors(CPed& ped); // 0x6060A0

private:
    // Not reversed yet:
    void ScanForAttractorsInPtrList(const void* ptrList, const CPed& ped); // 0x6034B0 - `ptrList` is a CPtrList (the head node pointer is at +0)
    void AddEffect(C2dEffect* effect, CEntity* entity, const CPed& ped);   // 0x5FFFD0
    void GetBestEffect(C2dEffect*& outEffect, CEntity*& outEntity);  // 0x600180 - slot 4 (if used), otherwise the closest one
    static CPed* GetClosestPedToEffect(C2dEffect* effect);               // 0x603570
};

VALIDATE_SIZE(CAttractorScanner, 0x90);
