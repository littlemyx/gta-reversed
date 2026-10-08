#pragma once

#include "Base.h"

#include "Vector.h"
#include "InteriorInfo_t.h"

class CEntity;
class CPed;
class Interior_c;
struct InteriorInfo_t;

enum class eInteriorGroupType : int8 {
    HOUSE = 0,
    SHOP = 1,
    OFFICE = 2,
};

class InteriorGroup_c : public ListItem_c<InteriorGroup_c> {
public:
    CEntity*    m_pEntity;             // 0x8
    uint8       m_id;                  // 0xC
    uint8       m_groupId;             // 0xD
    uint8       m_groupType;           // 0xE - TODO: eInteriorGroupType
    uint8       m_numInteriors;        // 0xF
    Interior_c* m_interiors[8];        // 0x10
    CEntryExit* m_EnEx;                // 0x30
    bool        m_isVisible;           // 0x34
    bool        m_lastIsVisible;       // 0x35
    int8        m_numPeds;             // 0x36
    CPed*       m_peds[16];            // 0x38
    CPed*       m_pedsToRemove[16];    // 0x78
    int8        m_pathSetupComplete;   // 0xB8
    int8        m_updatePeds;          // 0xB9
    int8        m_animBlockReferenced; // 0xBA

public:
    static void InjectHooks();

    InteriorGroup_c() = default;  // 0x597FE0
    ~InteriorGroup_c() = default; // 0x597FF0

    auto GetInteriors() { return m_interiors | rng::views::take(m_numInteriors); }
    auto GetPeds() { return m_peds | rng::views::take(m_numPeds); }

    void Init(CEntity* entity, int32 id);
    void Update();
    void AddInterior(Interior_c* interior);
    void SetupPeds();
    void UpdatePeds();
    void SetupHousePeds();
    void SetupPaths();
    bool ArePathsLoaded();
    void Setup();
    void Exit();
    bool ContainsInteriorType(int32 type);
    void CalcIsVisible();

    void DereferenceAnims();
    void ReferenceAnims();

    void UpdateOfficePeds();
    void RemovePed(CPed* ped);
    void SetupShopPeds();
    void SetupOfficePeds();
    CEntity* GetEntity();
    CPed* GetPed(int32);
    bool FindClosestInteriorInfo(int32 type, CVector point, float radius, InteriorInfo_t** outInfo, Interior_c** outInterior, float* outDistSq);
    bool FindInteriorInfo(eInteriorInfoType infoType, InteriorInfo_t** outInfo, Interior_c** outInterior);
    int32 GetNumInteriorInfos(int32 type);
    Interior_c* GetRandomInterior();
    auto GetId() const { return m_id; }

    auto GetInteriors() const { return m_interiors | std::views::take(m_numInteriors); }
private:

    //! @notsa
    const char* GetAnimBlockName();
};
VALIDATE_SIZE(InteriorGroup_c, 0xBC);
