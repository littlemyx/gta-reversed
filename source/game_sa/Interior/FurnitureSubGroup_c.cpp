#include "StdInc.h"
#include "FurnitureSubGroup_c.h"
#include "Furniture_c.h"

namespace {
// FurnitureManager_c::g_currFurnitureId (only the low word is used here)
auto& s_FurnSubGrpFurnitureCount = StaticRef<uint16>(0xBAB378);
// Storage for all furniture items (256 * 0x14)
auto& s_FurnSubGrpFurnitureStore = StaticRef<std::array<Furniture_c, 256>>(0xBAE1F8);
} // namespace

void FurnitureSubGroup_c::InjectHooks() {
    RH_ScopedClass(FurnitureSubGroup_c);
    RH_ScopedCategory("Interior");

    RH_ScopedInstall(GetFurniture, 0x590EE0);
    RH_ScopedInstall(GetRandomId, 0x590FD0);
    RH_ScopedInstall(AddFurniture, 0x5C00C0);
}

// 0x5C00C0
bool FurnitureSubGroup_c::AddFurniture(uint16 modelId, int16 id, uint8 wealthMin, uint8 wealthMax, uint8 maxAng) {
    if (s_FurnSubGrpFurnitureCount >= s_FurnSubGrpFurnitureStore.size()) {
        return false;
    }

    auto& f = s_FurnSubGrpFurnitureStore[s_FurnSubGrpFurnitureCount++];
    f.m_nModelId = modelId;
    f.m_nId      = id;

    // Original computes the (x87, extended precision) width/depth and rounds them up if the fraction is >= 0.02
    const auto& bb = CModelInfo::GetModelInfo(modelId)->GetColModel()->GetBoundingBox();
    double sizeX;
    float  sizeY;
    if (m_bCanSteal) {
        sizeX = (double)bb.m_vecMax.x - (double)bb.m_vecMin.x;
        sizeY = (float)((double)bb.m_vecMax.y - (double)bb.m_vecMin.y);
    } else {
        sizeX = (double)bb.m_vecMax.x + 0.5;
        sizeY = (float)((double)bb.m_vecMax.y + 0.5);
    }

    auto wx = (uint8)(int32)sizeX;
    if (0.02f <= sizeX - (double)(int32)sizeX) {
        wx++;
    }
    f.m_nWidthX = wx;

    auto wy = (uint8)(int32)sizeY;
    if (0.02f <= (double)sizeY - (double)(int32)sizeY) {
        wy++;
    }
    f.m_nWidthY = wy;

    f.m_nWealthMin = wealthMin;
    f.m_nWealthMax = wealthMax;
    f.m_nMaxAng    = maxAng;
    f.m_bCanPlaceInFrontOfWindow = m_bCanPlaceInFrontOfWindow;
    f.m_bIsTall                  = m_bIsTall;
    f.m_bCanSteal                = m_bCanSteal;

    m_Furnitures.AddItem(&f);
    return true;
}

// 0x590EE0
Furniture_c* FurnitureSubGroup_c::GetFurniture(int16 id, uint8 wealth) {
    if (id >= 0) {
        for (auto* f = m_Furnitures.GetHead(); f; f = f->m_pNext) {
            if (f->m_nId == id) {
                return f;
            }
        }
        return nullptr;
    }

    // NOTE: Original has a `wealth == -1` branch that picks a random item from the whole list,
    // but `wealth` is an `uint8` compared with `-1` as an `int` => it's dead code.

    uint32 numMatching = 0;
    for (auto* f = m_Furnitures.GetHead(); f; f = f->m_pNext) {
        if ((uint8)f->m_nWealthMin <= wealth && wealth <= (uint8)f->m_nWealthMax) {
            numMatching++;
        }
    }
    const auto pick = (int32)((float)CGeneral::GetRandomNumber() * (1.f / 32768.f) * (float)numMatching);

    int32 i = 0;
    for (auto* f = m_Furnitures.GetHead(); f; f = f->m_pNext) {
        if ((uint8)f->m_nWealthMin <= wealth && wealth <= (uint8)f->m_nWealthMax) {
            if (i == pick) {
                return f;
            }
            i++;
        }
    }
    return nullptr;
}

// 0x590FD0
int32 FurnitureSubGroup_c::GetRandomId(uint8 wealth) {
    // NOTE: Same dead `wealth == -1` branch as in `GetFurniture`

    uint32 numMatching = 0;
    for (auto* f = m_Furnitures.GetHead(); f; f = f->m_pNext) {
        if ((uint8)f->m_nWealthMin <= wealth && wealth <= (uint8)f->m_nWealthMax) {
            numMatching++;
        }
    }
    const auto pick = (int32)((float)CGeneral::GetRandomNumber() * (1.f / 32768.f) * (float)numMatching);

    int32 i = 0;
    for (auto* f = m_Furnitures.GetHead(); f; f = f->m_pNext) {
        if ((uint8)f->m_nWealthMin <= wealth && wealth <= (uint8)f->m_nWealthMax) {
            if (i == pick) {
                return f->m_nId;
            }
            i++;
        }
    }
    return -1;
}

void FurnitureSubGroup_c::Exit() {
    m_Furnitures.RemoveAll();
}
