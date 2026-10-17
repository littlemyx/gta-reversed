#pragma once

#include "Base.h"
#include "FurnitureGroup_c.h"
#include "FurnitureSubGroup_c.h"
#include "FurnitureItem.h"
#include "List_c.h"

class Furniture_c;

class FurnitureManager_c {
    friend struct FurnitureManagerMembers; // typed access for the exe-address alias of m_FurnitureList (see below)
private:
    // Both are 16 bit in the exe (e.g. `mov word [0xBAB37C], ax` at 0x5C04A8); the repo had them as uint32 (the upper halves 0xBAB37A/0xBAB37E are padding in front of the manager at 0xBAB380)
    static inline NOTSA_GLOBAL(g_currSubGroupId, 0xBAB37C, (uint16), {});
    static inline NOTSA_GLOBAL(g_currFurnitureId, 0xBAB378, (uint16), {});
    friend class FurnitureSubGroup_c; // FurnitureSubGroup_c::AddFurniture reads/increments g_currFurnitureId (exe 0x5C00C0)
    static inline NOTSA_GLOBAL(g_subGroupStore, 0xBAD3F8, (std::array<FurnitureSubGroup_c, 128>), {}); static_assert(sizeof(std::array<FurnitureSubGroup_c, 128>) == 3584);

public:
    static void InjectHooks();

    FurnitureManager_c() = default;  // 0x591260
    ~FurnitureManager_c() = default; // 0x5912E0

    static int32 GetGroupId(const char* name);
    static int32 GetSubGroupId(const char* name);
    static FurnitureSubGroup_c* NewSubGroup();

    bool Init();
    void Exit();
    void LoadFurniture();
    Furniture_c* GetFurniture(int32 furnitureGroupId, int32 furnitureSubgroupId, int16 id, uint8 wealth);
    int32 GetRandomId(int32 groupId, int32 a3, uint8 a4);

private:
    std::array<FurnitureGroup_c, 9> m_Groups;
    std::array<FurnitureItem, 512>  m_FurnitureItem;
    TList_c<FurnitureItem>          m_FurnitureList;
};
VALIDATE_SIZE(FurnitureManager_c, 0x2078);

NOTSA_GLOBAL_HDR(g_furnitureMan, 0xBAB380, (FurnitureManager_c), {}); static_assert(sizeof(FurnitureManager_c) == 8312);

#ifdef NOTSA_GLOBALS_DETACHED
// Interior_c.cpp reaches `m_FurnitureList` (12 bytes at 0x206C = 0xBAD3EC) as the pool of `FurnitureEntity_c`: both item types are 16 bytes behind the same ListItem_c, the exe uses one list for both.
#include "FurnitureEntity_c.h"
struct FurnitureManagerMembers {
    static auto& FurnitureEntityPool(FurnitureManager_c& m) { return reinterpret_cast<TList_c<FurnitureEntity_c>&>(m.m_FurnitureList); }
};
#endif
