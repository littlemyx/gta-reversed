#pragma once

#include "List_c.h"
#include "Interior_c.h"
#include "InteriorGroup_c.h"
#include "InteriorObject.h"
#include "InteriorEffectInfo_t.h"

class InteriorManager_c {
    friend struct InteriorManagerMembers; // typed access for the exe-address aliases of the private members (see below)
public:
    static void InjectHooks();

    InteriorManager_c() = default;  // 0x5984C0
    ~InteriorManager_c() = default; // 0x598580

    void Init();
    bool Update();
    void Exit();

    static void AddSameGroupEffectInfos(InteriorEffectInfo_t* intFxInfo, int32 a2);
    static bool AreAnimsLoaded(int32 animBlock);

    void PruneVisibleEffects(InteriorEffectInfo_t* pInteriorEffectInfos, size_t numInfos, size_t reqdNumInfos, float maxDist);
    size_t GetVisibleEffects(InteriorEffectInfo_t* effectInfo, uint32 totalEffects);
    bool IsInteriorEffectVisible(C2dEffectInterior* effect, CEntity* entity);

    /*!
    * @addr 0x5984B0
    * 
    * Add an interior to the free list
    */
    void ReturnInteriorToPool(Interior_c* interior);

    /*!
    * @addr 0x5984A0
    * @returns An interior from the free list
    */
    Interior_c* GetInteriorFromPool();

    /*!
    * @addr 0x5983D0
    *
    * @returns The interior the point `pt` lies within or `null`
    */
    Interior_c* GetVectorsInterior(const CVector& pt);

    /*!
    * @addr 0x598620
    *
    * @returns The interior the ped (it's position) is within, or null
    */
    Interior_c* GetPedsInterior(const CPed* ped);

    void SetStealableObjectStolen(CEntity* entity, uint8 a3);
    int32 FindStealableObjectId(CEntity* entity) const;
    int32 FindStealableObjectId(int32 interiorId, int32 modelId, CVector point) const;
    bool HasInteriorHadStealDataSetup(Interior_c* interior) const;
    int8 IsGroupActive(int32 group) const;
    InteriorGroup_c* GetPedsInteriorGroup(const CPed* ped);
    void SetEntryExitPtr(CEntryExit* exit);
    bool GetBoundingBox(CEntity* entity, CVector* pos);
    void ActivatePeds(bool enable);

    auto GetInteriorIds() const { return m_InteriorIds | rng::views::take(m_InteriorCount); }
    auto GetObjects() const { return m_Objects | rng::views::take(m_ObjectCount); }

private:
    Interior_c               m_Interiors[8]{};               // 0x0
    TList_c<Interior_c>      m_InteriorPool{};               // 0x3CA0
    InteriorGroup_c          m_InteriorGroups[8]{};          // 0x3CAC
    TList_c<InteriorGroup_c> m_InteriorGroupList{};          // 0x428C
    TList_c<InteriorGroup_c> m_InteriorGroupPool{};          // 0x4298
    size_t                   m_InteriorCount{};              // 0x42A4
    int32                    m_InteriorIds[64]{};            // 0x42A8
    int32                    m_ObjectCount{};                // 0x43A8
    InteriorObject           m_Objects[32]{};                // 0x43AC
    bool                     m_InteriorPedsAliveState[16]{}; // 0x472C
    CEntryExit*              m_EnEx{};                       // 0x473C
    CRect                    m_EnExRect{};                   // 0x4740
    bool                     m_IsFrozen{};                   // 0x4750
    bool                     m_IsActive{};                   // 0x4751
    bool                     m_ArePedsEnabled{};             // 0x4752
    int32                    m_TimeLastPickupsGenerated{};   // 0x4754
};
VALIDATE_SIZE(InteriorManager_c, 0x4758);

NOTSA_GLOBAL_HDR(g_interiorMan, 0xBAF670, (InteriorManager_c), {}); VALIDATE_SIZE(InteriorManager_c, 18264);

#ifdef NOTSA_GLOBALS_DETACHED
// Interior_c.cpp / InteriorGroup_c.cpp name some private members of `g_interiorMan` by their exe address (`NOTSA_GLOBAL_ALIAS(s_X, 0xADDR, (T), InteriorManagerMembers::X(g_interiorMan))`);
// in the detached build the alias must be the member itself, so these (same bytes, the exe's accesses are 4/1 byte wide) are viewed with the type the .cpp uses.
struct InteriorManagerMembers {
    static auto& InteriorCount(InteriorManager_c& m)            { return m.m_InteriorCount; }                                                       // 0x42A4
    static auto& InteriorIds(InteriorManager_c& m)              { return reinterpret_cast<std::array<int32, 64>&>(m.m_InteriorIds); }              // 0x42A8
    static auto& ObjectCount(InteriorManager_c& m)              { return m.m_ObjectCount; }                                                         // 0x43A8
    static auto& Objects(InteriorManager_c& m)                  { return reinterpret_cast<std::array<InteriorObject, 32>&>(m.m_Objects); }         // 0x43AC
    static auto& InteriorPedsAliveState(InteriorManager_c& m)   { return reinterpret_cast<std::array<bool, 16>&>(m.m_InteriorPedsAliveState); }    // 0x472C
    static auto& EnEx(InteriorManager_c& m)                     { return m.m_EnEx; }                                                                // 0x473C
    static auto& ArePedsEnabled(InteriorManager_c& m)           { return m.m_ArePedsEnabled; }                                                      // 0x4752
    static auto& TimeLastPickupsGenerated(InteriorManager_c& m) { return reinterpret_cast<uint32&>(m.m_TimeLastPickupsGenerated); }                 // 0x4754
};
#endif
