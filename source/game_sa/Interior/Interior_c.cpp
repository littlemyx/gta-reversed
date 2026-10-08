#include "StdInc.h"
#include "Interior_c.h"

#include "InteriorGroup_c.h"
#include "InteriorManager_c.h"
#include "FurnitureManager_c.h"
#include "Furniture_c.h"
#include "ModelIndices.h"

void Interior_c::InjectHooks() {
    RH_ScopedClass(Interior_c);
    RH_ScopedCategory("Interior");

    //RH_ScopedInstall(Constructor, 0x5921D0, { .Reversed = false });
    //RH_ScopedInstall(Destructor, 0x591360, { .Reversed = false });

    RH_ScopedInstall(Bedroom_AddTableItem, 0x593F10);
    RH_ScopedInstall(FurnishBedroom, 0x593FC0);
    RH_ScopedInstall(Kitchen_FurnishEdges, 0x596930);
    RH_ScopedInstall(FurnishKitchen, 0x5970B0);
    RH_ScopedInstall(Lounge_AddTV, 0x597240);
    RH_ScopedInstall(Lounge_AddHifi, 0x597430);
    RH_ScopedInstall(Lounge_AddChairInfo, 0x5974E0);
    RH_ScopedInstall(Lounge_AddSofaInfo, 0x5975C0);
    RH_ScopedInstall(FurnishLounge, 0x597740);
    RH_ScopedInstall(Office_PlaceEdgeFillers, 0x599210);
    RH_ScopedInstall(Office_PlaceDesk, 0x5993E0);
    RH_ScopedInstall(Office_PlaceEdgeDesks, 0x5995B0);
    RH_ScopedInstall(Office_FurnishEdges, 0x599770);
    RH_ScopedInstall(Office_PlaceDeskQuad, 0x599960);
    RH_ScopedInstall(Office_FurnishCenter, 0x599A30);
    RH_ScopedInstall(FurnishOffice, 0x599AF0);
    RH_ScopedInstall(Shop_Place3PieceUnit, 0x599BB0);
    RH_ScopedInstall(Shop_PlaceEdgeUnits, 0x599DC0);
    RH_ScopedInstall(Shop_PlaceCounter, 0x599EF0);
    RH_ScopedInstall(Shop_PlaceFixedUnits, 0x59A030);
    RH_ScopedInstall(Shop_FurnishCeiling, 0x59A130);
    RH_ScopedInstall(Shop_AddShelfInfo, 0x59A140);
    RH_ScopedInstall(Shop_FurnishEdges, 0x59A1B0);
    RH_ScopedInstall(GetBoundingBox, 0x593DB0);
    RH_ScopedInstall(Init, 0x593BF0);
    RH_ScopedInstall(ResetTiles, 0x593910);
    RH_ScopedInstall(PlaceObject, 0x5934E0);
    RH_ScopedInstall(GetFurnitureEntity, 0x5913B0);
    RH_ScopedInstall(IsPtInside, 0x5913E0);
    RH_ScopedInstall(CalcMatrix, 0x5914D0);
    RH_ScopedInstall(Furnish, 0x591590);
    RH_ScopedInstall(Unfurnish, 0x5915D0);
    RH_ScopedInstall(CheckTilesEmpty, 0x591680);
    RH_ScopedInstall(SetTilesStatus, 0x591700);
    RH_ScopedInstall(SetCornerTiles, 0x5917C0);
    RH_ScopedInstall(GetTileStatus, 0x5918E0);
    RH_ScopedInstall(GetNumEmptyTiles, 0x591920);
    RH_ScopedInstall(GetRandomTile, 0x591B20);
    RH_ScopedInstall(Shop_FurnishAisles, 0x59A590);
    RH_ScopedInstall(GetTileCentre, 0x591BD0);
    RH_ScopedInstall(AddGotoPt, 0x591D20);
    RH_ScopedInstall(AddInteriorInfo, 0x591E40);
    RH_ScopedInstall(AddPickups, 0x591F90);
    RH_ScopedInstall(Exit, 0x592230);
    RH_ScopedInstall(FindBoundingBox, 0x5922C0);
    RH_ScopedInstall(CalcExitPts, 0x5924A0);
    RH_ScopedInstall(IsVisible, 0x5929F0);
    RH_ScopedInstall(PlaceFurniture, 0x592AA0);
    RH_ScopedInstall(PlaceFurnitureOnWall, 0x593120);
    RH_ScopedInstall(PlaceFurnitureInCorner, 0x593340);
    RH_ScopedInstall(FindEmptyTiles, 0x591C50);
    RH_ScopedInstall(FurnishShop, 0x59A790);
}

namespace {
// Pool of free `FurnitureEntity_c` items (see `Unfurnish`)
auto& s_FurnitureEntityPool = StaticRef<TList_c<FurnitureEntity_c>>(0xBAD3EC);

// Counter used to space out the shelf `InteriorInfo`s (see `Shop_AddShelfInfo`) - Initially 2
auto& s_ShelfInfoCounter = StaticRef<int32>(0x8D0948);

// Chance (0-100) used by `Shop_PlaceEdgeUnits` to decide what kind of unit to place - Not written by any function reversed here
auto& s_ShopUnitChance = StaticRef<int32>(0xBB3DE4);

// Set once the office had its (single) special `InteriorInfo` (type 7) placed (see `Office_PlaceEdgeFillers`)
auto& s_OfficeSpecialInfoPlaced = StaticRef<bool>(0xBB3DC8);

// Members of `g_interiorMan` (private) - Objects that can be stolen (see `PlaceObject`)
auto& s_StealableObjectCount = StaticRef<int32>(0xBB3A18);                         // InteriorManager_c::m_ObjectCount
auto& s_StealableObjects     = StaticRef<std::array<InteriorObject, 32>>(0xBB3A1C); // InteriorManager_c::m_Objects

// The following 4 are members of `g_interiorMan`, but they are private, so we can't access them
auto& s_TimeLastPickupsGenerated = StaticRef<uint32>(0xBB3DC4);     // InteriorManager_c::m_TimeLastPickupsGenerated
auto& s_InteriorCount            = StaticRef<size_t>(0xBB3914);     // InteriorManager_c::m_InteriorCount
auto& s_InteriorIds              = StaticRef<std::array<int32, 64>>(0xBB3918); // InteriorManager_c::m_InteriorIds
auto& s_EnEx                     = StaticRef<CEntryExit*>(0xBB3DAC); // InteriorManager_c::m_EnEx

// The original code inlined this everywhere: `(int)(rand() * (1 / 32768.f) * n)`
// Note: Unlike `CGeneral::GetRandomNumberInRange`, the result is uniformly distributed in [0, n)
int32 RandBelow(int32 n) {
    return static_cast<int32>(static_cast<float>(CGeneral::GetRandomNumber()) * (1.f / 32768.f) * static_cast<float>(n));
}
} // namespace

// 0x593BF0
int32 Interior_c::Init(const CVector& pos) {
    CalcMatrix(&pos);
    ResetTiles();

    auto* const entity = m_pGroup->GetEntity();
    if (!entity->m_matrix) {
        entity->AllocateMatrix();
        entity->m_placement.UpdateMatrix(entity->m_matrix);
    }
    const auto entityPos = entity->GetPosition(); // Original made a copy of the entire matrix to get this

    if (m_box->m_type != 99) { // 'c'
        int32 seed;
        if (s_EnEx) {
            // NOTE: The multiplications might overflow (but wrap around in the original code)
            const auto enexHash = static_cast<uint32>(static_cast<int32>(s_EnEx->m_fEntranceZ))
                                * static_cast<uint32>(static_cast<int32>(s_EnEx->m_recEntrance.bottom))
                                * static_cast<uint32>(static_cast<int32>(s_EnEx->m_recEntrance.left));
            const auto posHash = static_cast<uint32>(static_cast<int32>(entityPos.z))
                               * static_cast<uint32>(static_cast<int32>(entityPos.y))
                               * static_cast<uint32>(static_cast<int32>(entityPos.x));
            seed = static_cast<int32>(posHash + enexHash + m_box->m_seed);
        } else {
            seed = static_cast<int32>(entityPos.z * entityPos.y * entityPos.x + static_cast<float>(m_box->m_seed));
        }
        srand(static_cast<uint32>(seed)); // Makes the furniture layout deterministic
    }

    m_gotoPtsCount       = 0;
    m_interiorInfosCount = 0;

    switch (m_box->m_type) {
    case 0: FurnishShop(0);   break;
    case 1: FurnishOffice();  break;
    case 2: FurnishLounge();  break;
    case 3: FurnishBedroom(); break;
    case 4: FurnishKitchen(); break;
    }

    CalcExitPts();

    if (!g_interiorMan.HasInteriorHadStealDataSetup(this) && s_InteriorCount < s_InteriorIds.size()) {
        s_InteriorIds[s_InteriorCount++] = m_interiorId;
    }

    if (m_box->m_type == 2 || m_box->m_type == 3) {
        AddPickups();
    }

    return 1;
}

// 0x592230
void Interior_c::Exit() {
    constexpr float RADIUS = 50.f;
    CPickups::RemovePickUpsInArea(
        m_matrix.pos.x - RADIUS, m_matrix.pos.x + RADIUS,
        m_matrix.pos.y - RADIUS, m_matrix.pos.y + RADIUS,
        m_matrix.pos.z - RADIUS, m_matrix.pos.z + RADIUS
    );
    Unfurnish();
}

// 0x593F10
CObject* Interior_c::Bedroom_AddTableItem(int32 groupId, int32 subGroupId, int32 rotation, int32 tileX, int32 tileY, int32 rotationIdx) {
    float x = static_cast<float>(tileX);
    float y = static_cast<float>(tileY);
    if (rotation == 0 || rotation == 2) {
        x += 0.5f;
    } else if (rotation == 1 || rotation == 3) {
        y += 0.5f;
    }
    auto* const furniture = g_furnitureMan.GetFurniture(groupId, subGroupId, -1, m_box->m_status);
    return PlaceObject(true, furniture, x + 0.5f, y + 0.5f, 0.5f, static_cast<float>(rotationIdx) * 90.f);
}

// 0x593FC0
void Interior_c::FurnishBedroom() {
    const auto wealth = m_box->m_status;
    const auto width  = static_cast<int32>(m_box->m_width);
    const auto depth  = static_cast<int32>(m_box->m_depth);

    m_furnitureId = static_cast<int8>(g_furnitureMan.GetRandomId(3, 1, wealth));
    SetTilesStatus(m_box->m_door - 1, 0, 2, 2, 7, 0);

    // NOTSA: In the original these are uninitialized if the bed couldn't be placed
    int32 bedRot = 0, bedPos = 0;
    auto* const bed = PlaceFurnitureOnWall(3, 0, -1, 0.f, 1, -1, -1, 0, &bedRot, &bedPos, nullptr, nullptr, nullptr, nullptr);

    // Bedside tables - One before the bed...
    if (bedPos > 0 && PlaceFurnitureOnWall(3, 1, m_furnitureId, 0.f, 1, bedRot, bedPos - 1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
        int32 action, dir, tx, ty, usedX, usedY;
        switch (bedRot) {
        case 1: action = 3; dir = 2; tx = 1;         ty = bedPos - 1; usedX = 2;         usedY = bedPos - 1; break;
        case 3: action = 4; dir = 2; tx = width - 2; ty = bedPos - 1; usedX = width - 3; usedY = bedPos - 1; break;
        case 0: action = 3; dir = 1; tx = bedPos - 1; ty = depth - 2; usedX = bedPos - 1; usedY = depth - 3; break;
        case 2: action = 4; dir = 1; tx = bedPos - 1; ty = 1;         usedX = bedPos - 1; usedY = 2;         break;
        default: action = dir = tx = ty = usedX = usedY = 0; break; // Can't happen
        }
        AddInteriorInfo(action, static_cast<float>(tx), static_cast<float>(ty), dir, bed);
        SetTilesStatus(usedX, usedY, 1, 1, 2, 0);
    }

    // ...and one after
    if (PlaceFurnitureOnWall(3, 1, m_furnitureId, 0.f, 1, bedRot, bedPos + 2, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
        int32 action, dir, tx, ty, usedX, usedY;
        switch (bedRot) {
        case 1: action = 4; dir = 0; tx = 1;         ty = bedPos + 2; usedX = 2;         usedY = bedPos + 2; break;
        case 3: action = 3; dir = 0; tx = width - 2; ty = bedPos + 2; usedX = width - 3; usedY = bedPos + 2; break;
        case 0: action = 4; dir = 3; tx = bedPos + 2; ty = depth - 2; usedX = bedPos + 2; usedY = depth - 3; break;
        case 2: action = 3; dir = 3; tx = bedPos + 2; ty = 1;         usedX = bedPos + 2; usedY = 2;         break;
        default: action = dir = tx = ty = usedX = usedY = 0; break; // Can't happen
        }
        AddInteriorInfo(action, static_cast<float>(tx), static_cast<float>(ty), dir, bed);
        SetTilesStatus(usedX, usedY, 1, 1, 2, 0);
    }

    PlaceFurnitureOnWall(3, 3, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(3, 2, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Table with an item on it
    {
        const auto AddTable = [&](int32 subGroupId) {
            int32 rot = 0, tileX = 0, tileY = 0;
            if (PlaceFurnitureOnWall(2, 6, -1, 0.f, 1, -1, -1, 0, &rot, nullptr, &tileX, &tileY, nullptr, nullptr)) {
                Bedroom_AddTableItem(2, subGroupId, rot, tileX, tileY, rot);
            }
        };

        const auto roll = RandBelow(100);
        if (roll < 25) {
            AddTable(8);
        } else if (roll >= 50) {
            if (roll < 75) {
                AddTable(8);
                AddTable(3);
            }
        } else {
            AddTable(3);
        }
    }

    // Small items - How many to place depends on the wealth
    int32 chance;
    if (wealth >= 75) {
        chance = RandBelow(20);
    } else if (wealth >= 50) {
        chance = 20 - RandBelow(-30);
    } else {
        chance = CGeneral::GetRandomNumberInRange(50, 100);
    }

    const auto PlaceItem = [&](int32 roll, int32 w, int32 d, int32 subGroupId, int32 usedW, int32 usedD) {
        int32 x, y;
        if (roll < chance && FindEmptyTiles(w, d, &x, &y)) {
            auto* const furniture = g_furnitureMan.GetFurniture(8, subGroupId, -1, wealth);
            PlaceObject(false, furniture, static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, 0.05f, 0.f);
            SetTilesStatus(x, y, usedW, usedD, 2, 0);
        }
    };
    PlaceItem(RandBelow(60),  2, 2, 2, 2, 2);
    PlaceItem(RandBelow(100), 1, 1, 5, 1, 1);
    PlaceItem(RandBelow(100), 1, 1, 4, 1, 1);
    PlaceItem(RandBelow(100), 1, 1, 3, 1, 1);
    PlaceItem(RandBelow(100), 2, 2, 6, 1, 1); // NOTE: Only marks 1x1 tiles as used

    // Rug in the middle
    auto* const rug   = g_furnitureMan.GetFurniture(8, 1, -1, wealth);
    const auto  tileY = static_cast<int32>(static_cast<float>(depth) * 0.5f - static_cast<float>(rug->m_nWidthY) * 0.5f);
    const auto  tileX = static_cast<int32>(static_cast<float>(width) * 0.5f - static_cast<float>(rug->m_nWidthX) * 0.5f);
    int32 outW, outD;
    PlaceFurniture(rug, tileX, tileY, 0.f, 0, 0, &outW, &outD, 0);
}

// 0x596930
void Interior_c::Kitchen_FurnishEdges() {
    const auto wealth = m_box->m_status;
    const int32 maxX = m_box->m_width - 1;
    const int32 maxY = m_box->m_depth - 1;
    int32 outW, outD; // Unused

    // Corner units (top left, top right)
    auto* const cornerUnit = g_furnitureMan.GetFurniture(4, 7, m_furnitureId, wealth);
    const bool  hasTopLeft  = PlaceFurniture(cornerUnit, 0, maxY, 0.f, 1, 1, &outW, &outD, 0) != nullptr;
    const bool  hasTopRight = PlaceFurniture(cornerUnit, maxX, maxY, 0.f, 1, 0, &outW, &outD, 0) != nullptr;

    // Length of the left and right side's "doors"
    const auto leftRunEnd  = std::max<int32>(m_box->m_lDoorEnd, m_box->m_lWindowEnd);
    const auto rightRunEnd = std::max<int32>(m_box->m_rDoorEnd, m_box->m_rWindowEnd);
    int32 leftRun  = std::max(leftRunEnd, 0);
    int32 rightRun = std::max(rightRunEnd, 0);
    if (rightRun <= 0) {
        if (leftRun <= 0) {
            leftRun  = CGeneral::GetRandomNumberInRange(0, maxY);
            rightRun = CGeneral::GetRandomNumberInRange(0, maxY);
        } else {
            rightRun = 0;
        }
    } else if (leftRun <= 0) {
        leftRun = 0;
    }
    if (leftRun > 0) {
        SetTilesStatus(0, 0, 1, leftRun, 2, 0);
    }
    if (rightRun > 0) {
        SetTilesStatus(maxX, 0, 1, rightRun, 2, 0);
    }

    // Units at the end of the left and right wall
    PlaceFurniture(g_furnitureMan.GetFurniture(4, 0, m_furnitureId, wealth), 0, leftRun, 0.f, 1, 1, &outW, &outD, 0);
    PlaceFurniture(g_furnitureMan.GetFurniture(4, 2, m_furnitureId, wealth), maxX, rightRun, 0.f, 1, 3, &outW, &outD, 0);

    // Fridge? (on the top wall)
    int32 along = 0;
    if (PlaceFurnitureOnWall(4, 3, m_furnitureId, 0.f, 1, 0, m_box->m_tWindowStart == -1 ? -1 : m_box->m_tWindowStart, 0, nullptr, &along, nullptr, nullptr, nullptr, nullptr)) {
        along++;
        AddInteriorInfo(5, static_cast<float>(along), static_cast<float>(m_box->m_depth) - 1.5f, 2, nullptr);
    }
    PlaceFurnitureOnWall(4, 5, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(4, 4, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(4, 6, m_furnitureId, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Counters along the walls. For each one, remember where items could be placed on top of it
    constexpr size_t MAX_SPOTS = 32;
    float            spotX[MAX_SPOTS]{}, spotY[MAX_SPOTS]{}, spotRot[MAX_SPOTS]{}, spotTaken[MAX_SPOTS]{};
    int32            numSpots = 0;
    const auto       AddSpot  = [&](float x, float y, float rot) {
        // NOTSA: The original overflows the arrays here
        if (numSpots < static_cast<int32>(MAX_SPOTS)) {
            spotX[numSpots]   = x;
            spotY[numSpots]   = y;
            spotRot[numSpots] = rot;
            numSpots++;
        }
    };

    auto* const counter = g_furnitureMan.GetFurniture(4, 1, m_furnitureId, wealth);
    for (int32 y = leftRun + 1; y < maxY; y++) {
        if (PlaceFurniture(counter, 0, y, 0.f, 1, 1, &outW, &outD, 0)) {
            AddSpot(0.5f, static_cast<float>(y) + 0.5f, 90.f);
        }
    }
    for (int32 x = 1; x < maxX; x++) {
        if (PlaceFurniture(counter, x, maxY, 0.f, 1, 0, &outW, &outD, 0)) {
            AddSpot(static_cast<float>(x) + 0.5f, static_cast<float>(maxY) + 0.5f, 0.f);
        }
    }
    for (int32 y = rightRun + 1; y <= maxY - 1; y++) {
        if (PlaceFurniture(counter, maxX, y, 0.f, 1, 3, &outW, &outD, 0)) {
            AddSpot(static_cast<float>(maxX) + 0.5f, static_cast<float>(y) + 0.5f, 270.f);
        }
    }

    // Something in one of the top corners
    auto* const cornerItem = g_furnitureMan.GetFurniture(4, 10, -1, wealth);
    if (CGeneral::GetRandomNumber() < 0x3FFF && hasTopLeft) {
        PlaceObject(true, cornerItem, 0.5f, static_cast<float>(m_box->m_depth) - 0.5f, 1.05f, 45.f);
    } else if (hasTopRight) {
        PlaceObject(true, cornerItem, static_cast<float>(m_box->m_width) - 0.5f, static_cast<float>(m_box->m_depth) - 0.5f, 1.05f, 315.f);
    }

    if (numSpots <= 0) {
        return;
    }

    // Pick 2 (different) spots for stealable items
    const auto RollSpot = [&] { return RandBelow(numSpots); };
    const int32 spotA = RollSpot();
    spotTaken[spotA] = 1.f;
    int32 spotB = RollSpot();
    if (spotTaken[spotB] == 1.f) {
        int32 tries = 0;
        do {
            if (tries > 29) {
                break;
            }
            spotB = RollSpot();
            tries++;
        } while (spotTaken[spotB] == 1.f);
        // BUG: Even if a free spot was found at the very last try, it's thrown away
        if (tries == 30) {
            spotB = -1;
        }
    }

    // Chance of placing an item on the remaining spots
    int32 chance;
    if (wealth >= 75) {
        chance = RandBelow(20);
    } else if (wealth >= 50) {
        chance = 20 - RandBelow(-30);
    } else {
        chance = 50 - RandBelow(-50);
    }

    for (int32 i = 0; i < numSpots; i++) {
        if (spotTaken[i] != 0.f) {
            continue;
        }
        if (RandBelow(100) >= chance) {
            continue;
        }
        const auto subGroupId = CGeneral::GetRandomNumber() < 0x3FFF ? 3 : 4;
        if (auto* const furniture = g_furnitureMan.GetFurniture(8, subGroupId, -1, wealth)) {
            PlaceObject(false, furniture, spotX[i], spotY[i], 1.05f, spotRot[i]);
        }
    }

    if (spotA != -1) {
        PlaceObject(true, g_furnitureMan.GetFurniture(4, 8, -1, wealth), spotX[spotA], spotY[spotA], 1.05f, spotRot[spotA]);
    }
    if (spotB != -1) {
        PlaceObject(true, g_furnitureMan.GetFurniture(4, 9, -1, wealth), spotX[spotB], spotY[spotB], 1.05f, spotRot[spotB]);
    }
}

// 0x5970B0
void Interior_c::FurnishKitchen() {
    SetTilesStatus(m_box->m_door - 1, 0, 2, 1, 7, 0);

    const int32 maxX = m_box->m_width - 2;
    const int32 maxY = m_box->m_depth - 2;
    for (int32 x = 1; x <= maxX; x++) {
        SetTilesStatus(x, maxY, 1, 1, 3, 0);
        SetTilesStatus(x, 0, 1, 1, 3, 0);
    }
    for (int32 y = 0; y <= maxY; y++) {
        SetTilesStatus(1, y, 1, 1, 3, 0);
        SetTilesStatus(maxX, y, 1, 1, 3, 0);
    }

    AddGotoPt(1, 1, 0.f, 0.f);
    AddGotoPt(1, maxY, 0.f, 0.f);
    AddGotoPt(maxX, 1, 0.f, 0.f);
    AddGotoPt(maxX, maxY, 0.f, 0.f);

    m_furnitureId = static_cast<int8>(g_furnitureMan.GetRandomId(4, 0, m_box->m_status));
    Kitchen_FurnishEdges();

    // Place something in the middle of the room
    auto* const furniture = g_furnitureMan.GetFurniture(8, 1, -1, m_box->m_status);
    const auto  tileY     = static_cast<int32>(static_cast<float>(m_box->m_depth) * 0.5f - static_cast<float>(furniture->m_nWidthY) * 0.5f);
    const auto  tileX     = static_cast<int32>(static_cast<float>(m_box->m_width) * 0.5f - static_cast<float>(furniture->m_nWidthX) * 0.5f);
    int32 width, depth;
    PlaceFurniture(furniture, tileX, tileY, 0.f, 0, 0, &width, &depth, 0);
}

// 0x597240
CObject* Interior_c::Lounge_AddTV(int32 rotation, int32, int32, int32) {
    const auto width = static_cast<float>(m_box->m_width);
    const auto depth = static_cast<float>(m_box->m_depth);

    // NOTSA: In the original these are uninitialized if `rotation` isn't in [0, 3]
    float tvX = 0.f, tvY = 0.f;
    float secondX = 0.f, secondY = 0.f; // The second item (hifi/etc)

    switch (rotation) {
    case 0:
        tvX     = 0.5f;
        tvY     = depth - 0.5f;
        secondX = 1.5f;
        secondY = tvY;
        AddInteriorInfo(0, 1.f, depth - 2.f, -1, nullptr);
        break;
    case 1:
        tvX     = 0.5f;
        tvY     = 0.5f;
        secondX = 0.5f;
        secondY = 1.5f;
        AddInteriorInfo(0, 1.f, 1.f, -1, nullptr);
        break;
    case 2:
        tvX     = width - 0.5f;
        tvY     = 0.5f;
        secondX = tvX - 1.f;
        secondY = 0.5f;
        AddInteriorInfo(0, width - 2.f, 1.f, -1, nullptr);
        break;
    case 3:
        tvX     = width - 0.5f;
        tvY     = depth - 0.5f;
        secondX = tvX;
        secondY = tvY - 1.f;
        AddInteriorInfo(0, width - 2.f, depth - 2.f, -1, nullptr);
        break;
    }

    const auto rotationDeg = static_cast<float>(rotation & 3) * 90.f;

    auto* const tv = g_furnitureMan.GetFurniture(2, 3, -1, m_box->m_status);
    PlaceObject(true, tv, tvX, tvY, 0.5f, rotationDeg + 45.f);

    const auto subGroupId = CGeneral::GetRandomNumber() < 0x3FFF ? 7 : 9;
    auto* const second = g_furnitureMan.GetFurniture(2, subGroupId, -1, m_box->m_status);
    return PlaceObject(true, second, secondX, secondY, 0.5f, rotationDeg);
}

// 0x597430
CObject* Interior_c::Lounge_AddHifi(int32 rotation, int32 tileX, int32 tileY, int32) {
    float x = static_cast<float>(tileX);
    float y = static_cast<float>(tileY);
    if (rotation == 0 || rotation == 2) {
        x += 0.5f;
    } else if (rotation == 1 || rotation == 3) {
        y += 0.5f;
    }
    auto* const furniture = g_furnitureMan.GetFurniture(2, 8, -1, m_box->m_status);
    return PlaceObject(true, furniture, x + 0.5f, y + 0.5f, 0.5f, static_cast<float>(rotation & 3) * 90.f);
}

// 0x5974E0
void Interior_c::Lounge_AddChairInfo(int32 rotation, int32 offset, CEntity* entityIgnoredCollision) {
    switch (rotation) {
    case 0:
        AddInteriorInfo(1, static_cast<float>(offset) + 0.5f, static_cast<float>(m_box->m_depth - 1) - 1.f, 2, entityIgnoredCollision);
        break;
    case 2:
        AddInteriorInfo(1, static_cast<float>(offset) + 0.5f, 1.f, 0, entityIgnoredCollision);
        break;
    case 1:
        AddInteriorInfo(1, 1.f, static_cast<float>(offset) + 0.5f, 3, entityIgnoredCollision);
        break;
    case 3:
        AddInteriorInfo(1, static_cast<float>(m_box->m_width - 1) - 1.f, static_cast<float>(offset) + 0.5f, 1, entityIgnoredCollision);
        break;
    }
}

// 0x5975C0
void Interior_c::Lounge_AddSofaInfo(int32 rotation, int32 offset, CEntity* entityIgnoredCollision) {
    switch (rotation) {
    case 0: {
        const auto x = static_cast<float>(offset) + 0.5f;
        const auto y = static_cast<float>(m_box->m_depth - 1) - 1.f;
        AddInteriorInfo(1, x, y, 2, entityIgnoredCollision);
        AddInteriorInfo(1, x + 1.f, y, 2, entityIgnoredCollision);
        break;
    }
    case 2: {
        const auto x = static_cast<float>(offset) + 0.5f;
        AddInteriorInfo(1, x, 1.f, 0, entityIgnoredCollision);
        AddInteriorInfo(1, x + 1.f, 1.f, 0, entityIgnoredCollision);
        break;
    }
    case 1: {
        const auto y = static_cast<float>(offset) + 0.5f;
        AddInteriorInfo(1, 1.f, y, 3, entityIgnoredCollision);
        AddInteriorInfo(1, 1.f, y + 1.f, 3, entityIgnoredCollision);
        break;
    }
    case 3: {
        const auto x = static_cast<float>(m_box->m_width - 1) - 1.f;
        const auto y = static_cast<float>(offset) + 0.5f;
        AddInteriorInfo(1, x, y, 1, entityIgnoredCollision);
        AddInteriorInfo(1, x, y + 1.f, 1, entityIgnoredCollision);
        break;
    }
    }
}

// 0x597740
void Interior_c::FurnishLounge() {
    const auto wealth = m_box->m_status;
    const auto width  = static_cast<int32>(m_box->m_width);
    const auto depth  = static_cast<int32>(m_box->m_depth);
    const auto perimeter = (depth + width) * 2;

    // Door
    SetTilesStatus(m_box->m_door - 1, 0, 2, 1, 7, 0);
    SetTilesStatus(m_box->m_door - 2, 0, 1, 1, 2, 0);
    SetTilesStatus(m_box->m_door + 1, 0, 1, 1, 2, 0);

    // TV in a corner
    // NOTSA: `cornerRot` is uninitialized in the original if the TV couldn't be placed
    int32 cornerRot = -1, cornerX = 0, cornerY = 0, tmpW, tmpD;
    int32 usedCorner = -1;
    if (PlaceFurnitureInCorner(2, 2, -1, 0.f, 1, -1, 0, &cornerRot, &cornerX, &cornerY, &tmpW, &tmpD)) {
        Lounge_AddTV(cornerRot, cornerX, cornerY, 0);
        usedCorner = cornerRot;
    }
    SetCornerTiles(cornerRot, 2, 2, 1);

    // Mark the tiles along the walls (and add goto points to the corners)
    const int32 maxX = width - 2;
    const int32 maxY = depth - 2;
    for (int32 x = 1; x <= maxX; x++) {
        SetTilesStatus(x, maxY, 1, 1, 3, 0);
        SetTilesStatus(x, 1, 1, 1, 3, 0);
    }
    for (int32 y = 1; y <= maxY; y++) {
        SetTilesStatus(1, y, 1, 1, 3, 0);
        SetTilesStatus(maxX, y, 1, 1, 3, 0);
    }
    AddGotoPt(1, 1, 0.f, 0.f);
    AddGotoPt(1, maxY, 0.f, 0.f);
    AddGotoPt(maxX, 1, 0.f, 0.f);
    AddGotoPt(maxX, maxY, 0.f, 0.f);

    SetCornerTiles(0, 2, 2, 0);
    SetCornerTiles(2, 2, 2, 0);
    SetCornerTiles(1, 2, 2, 0);
    SetCornerTiles(3, 2, 2, 0);

    m_furnitureId = static_cast<int8>(g_furnitureMan.GetRandomId(2, 0, wealth));

    // Sofa (and a table in front of it)
    int32 rot = 0, pos = 0; // NOTSA: Uninitialized in the original if nothing was placed
    if (auto* const sofa = PlaceFurnitureOnWall(2, 0, m_furnitureId, 0.f, 1, -1, -1, 0, &rot, &pos, nullptr, nullptr, nullptr, nullptr)) {
        Lounge_AddSofaInfo(rot, pos, sofa);
        if (!PlaceFurnitureOnWall(2, 4, -1, 0.f, 1, rot, pos, 2, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
            PlaceFurnitureOnWall(2, 4, -1, 0.f, 1, rot, pos, 3, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
    }

    // Chairs
    const auto AddChair = [&] {
        if (auto* const chair = PlaceFurnitureOnWall(2, 1, m_furnitureId, 0.f, 1, -1, -1, 0, &rot, &pos, nullptr, nullptr, nullptr, nullptr)) {
            Lounge_AddChairInfo(rot, pos, chair);
        }
    };
    AddChair();
    AddChair();
    if (perimeter > 28) {
        AddChair();
    }

    // Free the corner tiles (except for the one with the TV)
    if (usedCorner != 0) { SetCornerTiles(0, 2, 0, 0); }
    if (usedCorner != 2) { SetCornerTiles(2, 2, 0, 0); }
    if (usedCorner != 1) { SetCornerTiles(1, 2, 0, 0); }
    if (usedCorner != 3) { SetCornerTiles(3, 2, 0, 0); }

    // Hifi
    {
        int32 hifiRot = 0, hifiX = 0, hifiY = 0;
        if (PlaceFurnitureOnWall(2, 6, -1, 0.f, 1, -1, -1, 0, &hifiRot, nullptr, &hifiX, &hifiY, nullptr, nullptr)) {
            Lounge_AddHifi(hifiRot, hifiX, hifiY, 0);
        }
    }
    PlaceFurnitureOnWall(2, 5, -1, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(8, 0, -1, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PlaceFurnitureOnWall(8, 0, -1, 0.f, 1, -1, -1, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Small items - How many to place depends on the wealth
    int32 chance;
    if (wealth >= 75) {
        chance = RandBelow(20);
    } else if (wealth >= 50) {
        chance = 20 - RandBelow(-30);
    } else {
        chance = 50 - RandBelow(-50);
    }

    const auto PlaceItem = [&](int32 roll, int32 w, int32 d, int32 subGroupId, int32 usedW, int32 usedD) {
        int32 x, y;
        if (roll < chance && FindEmptyTiles(w, d, &x, &y)) {
            auto* const furniture = g_furnitureMan.GetFurniture(8, subGroupId, -1, wealth);
            PlaceObject(false, furniture, static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, 0.05f, 0.f);
            SetTilesStatus(x, y, usedW, usedD, 2, 0);
        }
    };
    PlaceItem(RandBelow(60),  2, 2, 2, 2, 2);
    PlaceItem(RandBelow(100), 1, 1, 5, 1, 1);
    PlaceItem(RandBelow(100), 1, 1, 4, 1, 1);

    // Rug in the middle
    {
        auto* const rug   = g_furnitureMan.GetFurniture(8, 1, -1, wealth);
        const auto  tileY = static_cast<int32>(static_cast<float>(depth) * 0.5f - static_cast<float>(rug->m_nWidthY) * 0.5f);
        const auto  tileX = static_cast<int32>(static_cast<float>(width) * 0.5f - static_cast<float>(rug->m_nWidthX) * 0.5f);
        int32 outW, outD;
        PlaceFurniture(rug, tileX, tileY, 0.f, 0, 0, &outW, &outD, 0);
    }

    // Interior infos for the free tiles along the walls
    for (int32 x = 0; width - 1 > 0 && x < width - 1; x++) {
        const auto bottom = GetTileStatus(x, 0);
        const auto top    = GetTileStatus(x, depth - 1);
        if (bottom == 0 || bottom == 2) {
            AddInteriorInfo(2, static_cast<float>(x), 0.f, 2, nullptr);
        }
        if (top == 0 || top == 2) {
            AddInteriorInfo(2, static_cast<float>(x), static_cast<float>(depth - 1), 0, nullptr);
        }
    }
    for (int32 y = 1; y < depth - 2; y++) {
        const auto left  = GetTileStatus(0, y);
        const auto right = GetTileStatus(width - 1, y);
        if (left == 0 || left == 2) {
            AddInteriorInfo(2, 0.f, static_cast<float>(y), 1, nullptr);
        }
        if (right == 0 || right == 2) {
            AddInteriorInfo(2, static_cast<float>(width - 1), static_cast<float>(y), 3, nullptr);
        }
    }
}

// 0x599210
int32 Interior_c::Office_PlaceEdgeFillers(int32 subGroupId, int32 tileX, int32 tileY, int32 direction, int32) {
    const auto roll = RandBelow(100);

    const auto numEmpty = GetNumEmptyTiles(tileX, tileY, (direction == 2 || direction == 0) ? 1 : 2, 1);
    if (numEmpty < 1) {
        return 1;
    }

    // Tile in front of the filler (for the interior info)
    int32 infoX = tileX;
    int32 infoY = tileY;
    switch (direction) {
    case 2: infoY = tileY + 1; break;
    case 0: infoY = tileY - 1; break;
    case 3: infoX = tileX - 1; break;
    case 1: infoX = tileX + 1; break;
    }

    int32        rotation = direction;
    Furniture_c* furniture;
    if (subGroupId == -1) {
        if (roll > 90 && !s_OfficeSpecialInfoPlaced) {
            furniture = g_furnitureMan.GetFurniture(1, 2, -1, m_box->m_status);
            if (furniture) {
                AddInteriorInfo(7, static_cast<float>(infoX), static_cast<float>(infoY), (direction - 2) & 3, nullptr);
            }
            s_OfficeSpecialInfoPlaced = true;
        } else if (roll > 75) {
            // BUG: The original code reuses the variable holding the rotation here, so the
            // furniture is placed using a random rotation in [0, 4]. (It probably should've been a random subgroup id)
            rotation  = CGeneral::GetRandomNumberInRange(0, 5);
            furniture = g_furnitureMan.GetFurniture(8, 0, -1, m_box->m_status);
        } else if (roll > 25) {
            furniture = g_furnitureMan.GetFurniture(1, 3, -1, m_box->m_status);
            if (furniture) {
                AddInteriorInfo(7, static_cast<float>(infoX), static_cast<float>(infoY), (direction - 2) & 3, nullptr);
            }
        } else {
            return 1;
        }
    } else {
        furniture = g_furnitureMan.GetFurniture(1, subGroupId, -1, m_box->m_status);
    }

    int32 width = 0, depth = 0;
    PlaceFurniture(furniture, tileX, tileY, 0.f, 1, rotation, &width, &depth, 0);
    return width;
}

// 0x5993E0
int32 Interior_c::Office_PlaceDesk(int32 tileX, int32 tileY, int32 direction, int32, uint8, int32 furnitureId) {
    const auto rotation = (direction - 2) & 3;

    // The desk itself
    int32 deskX = tileX;
    int32 deskY = tileY;
    if (direction == 2) {
        deskY = tileY + 1;
    } else if (direction == 1) {
        deskX = tileX + 1;
    }
    auto* const desk = g_furnitureMan.GetFurniture(1, 0, static_cast<int16>(furnitureId), m_box->m_status);
    int32 width, depth;
    if (!PlaceFurniture(desk, deskX, deskY, 0.f, 1, rotation, &width, &depth, 0)) {
        return 1;
    }

    // The chair [In front of the desk]
    int32 usedTileX = tileX; // Tile that will be marked as used
    int32 usedTileY = tileY;
    int32 chairX    = tileX;
    int32 chairY    = tileY;
    if (direction == 2) {
        chairX = tileX + 1;
    } else if (direction == 0) {
        chairY    = tileY + 1;
        usedTileX = tileX + 1;
        usedTileY = chairY;
    } else if (direction == 3) {
        chairY    = tileY + 1;
        usedTileX = tileX + 1;
        chairX    = usedTileX;
    } else if (direction == 1) {
        usedTileY = tileY + 1;
    }
    auto* const chair = g_furnitureMan.GetFurniture(1, 1, static_cast<int16>(m_furnitureId2), m_box->m_status);
    auto* const chairEntity = PlaceFurniture(chair, chairX, chairY, 0.f, 1, rotation, &width, &depth, 1);

    // Interior info [for peds to use the desk]
    auto infoX = static_cast<float>(chairX);
    auto infoY = static_cast<float>(chairY);
    switch (rotation) {
    case 2: infoY += 0.5f; break;
    case 0: infoY -= 0.5f; break;
    case 3: infoX -= 0.5f; break;
    case 1: infoX += 0.5f; break;
    }
    AddInteriorInfo(6, infoX, infoY, (rotation - 2) & 3, chairEntity);
    SetTilesStatus(usedTileX, usedTileY, 1, 1, 2, 1);

    return 2;
}

// 0x5995B0
int32 Interior_c::Office_PlaceEdgeDesks(int32, int32 tileX, int32 tileY, int32 direction, int32 side) {
    const auto roll1 = RandBelow(100);
    const auto roll2 = RandBelow(40); // Original: `-RandBelow(40)`, see below

    const auto numEmpty = GetNumEmptyTiles(tileX, tileY, (direction == 2 || direction == 0) ? 1 : 2, 1);
    if (numEmpty < 2) {
        return 1;
    }

    auto numDesks = numEmpty / 2;
    if (2 + RandBelow(2) <= numDesks) {
        numDesks = 2 + RandBelow(2);
    }

    if (30 + roll2 < roll1) {
        return 1;
    }

    int32 placed = 0;
    for (; numDesks > 0; numDesks--) {
        int32 deskX, deskY;
        switch (side) {
        case 0:
            deskX = tileX + placed;
            deskY = tileY - 1;
            break;
        case 3:
            deskX = tileX - 1;
            deskY = tileY + placed;
            break;
        case 1:
            deskX = tileX;
            deskY = tileY + placed;
            break;
        case 2:
            deskX = tileX + placed;
            deskY = tileY;
            break;
        default:
            continue;
        }
        placed += Office_PlaceDesk(deskX, deskY, (direction - 2) & 3, 70, 0, m_furnitureId);
    }

    return placed + 1;
}

// 0x599770
void Interior_c::Office_FurnishEdges() {
    const int32 maxX = m_box->m_width - 3;
    const int32 maxY = m_box->m_depth - 3;
    for (int32 x = 2; x <= maxX; x++) {
        SetTilesStatus(x, maxY, 1, 1, 3, 0);
        SetTilesStatus(x, 2, 1, 1, 3, 0);
    }
    for (int32 y = 2; y <= maxY; y++) {
        SetTilesStatus(2, y, 1, 1, 3, 0);
        SetTilesStatus(maxX, y, 1, 1, 3, 0);
    }

    AddGotoPt(2, 2, 0.5f, 0.5f);
    AddGotoPt(2, maxY, 0.5f, -0.5f);
    AddGotoPt(maxX, 2, -0.5f, 0.5f);
    AddGotoPt(maxX, maxY, -0.5f, -0.5f);

    const int32 depth = m_box->m_depth;
    const int32 right = m_box->m_width - 1; // x of the right wall
    SetTilesStatus(m_box->m_door - 2, 0, 4, 2, 7, 0);

    // Desks
    for (int32 x = 1; x < right;) {
        x += Office_PlaceEdgeDesks(-1, x, 0, 2, 2);
    }
    for (int32 x = 1; x < right;) {
        x += Office_PlaceEdgeDesks(-1, x, depth - 1, 0, 0);
    }
    for (int32 y = 1; y <= depth - 2;) {
        y += Office_PlaceEdgeDesks(-1, 0, y, 1, 1);
    }
    for (int32 y = 1; y <= depth - 2;) {
        y += Office_PlaceEdgeDesks(-1, right, y, 3, 3);
    }

    // Fillers
    for (int32 x = 1; x < right;) {
        x += Office_PlaceEdgeFillers(-1, x, 0, 2, 2);
    }
    for (int32 x = 1; x < right;) {
        x += Office_PlaceEdgeFillers(-1, x, depth - 1, 0, 0);
    }
    for (int32 y = 1; y <= depth - 2;) {
        y += Office_PlaceEdgeFillers(-1, 0, y, 1, 1);
    }
    for (int32 y = 1; y <= depth - 2;) {
        y += Office_PlaceEdgeFillers(-1, right, y, 3, 3);
    }
}

// 0x599960
int32 Interior_c::Office_PlaceDeskQuad(int32, int32 tileX, int32 tileY, int32 furnitureId) {
    const auto upperY = tileY - 2;
    Office_PlaceDesk(tileX, upperY, 2, 70, 0, furnitureId);
    Office_PlaceDesk(tileX, tileY, 0, 70, 0, furnitureId);
    Office_PlaceDesk(tileX - 2, tileY, 0, 70, 0, furnitureId);
    Office_PlaceDesk(tileX - 2, upperY, 2, 70, 0, furnitureId);

    SetTilesStatus(tileX - 3, tileY - 3, 6, 1, 3, 0);
    SetTilesStatus(tileX - 3, tileY + 2, 6, 1, 3, 0);
    SetTilesStatus(tileX - 3, upperY, 1, 4, 3, 0);
    SetTilesStatus(tileX + 2, upperY, 1, 4, 3, 0);
    return 6;
}

// 0x599A30
void Interior_c::Office_FurnishCenter() {
    const int32 availX = m_box->m_width - 6;
    const int32 availY = m_box->m_depth - 6;
    if (availX <= 0 || availY <= 0 || availX / 6 <= 0) {
        return;
    }

    int32 x = (availX % 6) / 2;
    for (int32 i = 0; i < availX / 6; i++) {
        x += 6;
        int32 y = (availY % 6) / 2;
        for (int32 j = 0; j < availY / 6; j++) {
            Office_PlaceDeskQuad(-1, x, y + 6, m_furnitureId);
            y += 6;
        }
    }
}

// 0x599AF0
void Interior_c::FurnishOffice() {
    SetTilesStatus(0, 0, 2, 2, 2, 0);
    SetTilesStatus(0, m_box->m_depth - 2, 2, 2, 2, 0);
    SetTilesStatus(m_box->m_width - 2, 0, 2, 2, 2, 0);
    SetTilesStatus(m_box->m_width - 2, m_box->m_depth - 2, 2, 2, 2, 0);

    m_furnitureId  = static_cast<int8>(g_furnitureMan.GetRandomId(1, 0, m_box->m_status));
    m_furnitureId2 = static_cast<int8>(g_furnitureMan.GetRandomId(1, 1, m_box->m_status));

    Office_FurnishEdges();
    Office_FurnishCenter();
}

// 0x599BB0
int8 Interior_c::Shop_Place3PieceUnit(int32 subGroupId, int32 tileX, int32 tileY, int32 direction, int32 length) {
    const auto wealth = m_box->m_status;

    // Which piece is placed first/last depends on the direction
    Furniture_c* first;
    int32        lastSubGroupId;
    if (direction == 2 || direction == 3) {
        first          = g_furnitureMan.GetFurniture(m_furnitureGroupId, subGroupId + 1, -1, wealth);
        lastSubGroupId = subGroupId;
    } else {
        first          = g_furnitureMan.GetFurniture(m_furnitureGroupId, subGroupId, -1, wealth);
        lastSubGroupId = subGroupId + 1;
    }
    auto* const last   = g_furnitureMan.GetFurniture(m_furnitureGroupId, lastSubGroupId, first->m_nId, wealth);
    auto* const middle = g_furnitureMan.GetFurniture(m_furnitureGroupId, subGroupId + 2, first->m_nId, wealth);

    int32 width, depth;
    if (direction != 2 && direction != 0) { // Along the Y axis
        PlaceFurniture(first, tileX, tileY, 0.f, 1, direction, &width, &depth, 0);
        int32 y = tileY + depth;
        for (int32 i = 0; i < length - 2; i++) {
            PlaceFurniture(middle, tileX, y, 0.f, 1, direction, &width, &depth, 0);
            y += depth;
        }
        PlaceFurniture(last, tileX, y, 0.f, 1, direction, &width, &depth, 0);
    } else { // Along the X axis
        PlaceFurniture(first, tileX, tileY, 0.f, 1, direction, &width, &depth, 0);
        int32 x = tileX + width;
        for (int32 i = 0; i < length - 2; i++) {
            PlaceFurniture(middle, x, tileY, 0.f, 1, direction, &width, &depth, 0);
            x += width;
        }
        PlaceFurniture(last, x, tileY, 0.f, 1, direction, &width, &depth, 0);
    }
    return 1;
}

// 0x599DC0
int32 Interior_c::Shop_PlaceEdgeUnits(int32 subGroupId, int32 tileX, int32 tileY, int32 direction) {
    const auto numEmpty = GetNumEmptyTiles(tileX, tileY, (direction == 2 || direction == 0) ? 1 : 2, 1);
    if (numEmpty < 2) {
        return 1;
    }

    // Length of the unit (in tiles)
    const auto wanted = 2 + RandBelow(3);
    int32      length;
    if (numEmpty == 3) {
        length = 3;
    } else if (numEmpty - wanted < 0) {
        length = numEmpty;
    } else if (numEmpty - wanted == 1) {
        length = wanted - 1;
    } else {
        length = wanted;
    }

    if (subGroupId != -1) {
        Shop_Place3PieceUnit(subGroupId, tileX, tileY, direction, length);
    } else if (s_ShopUnitChance > 50) {
        Shop_Place3PieceUnit(0, tileX, tileY, direction, length);
    } else if (s_ShopUnitChance >= 26) {
        Shop_Place3PieceUnit(3, tileX, tileY, direction, length);
    } else if (s_ShopUnitChance >= 11) {
        Shop_Place3PieceUnit(6, tileX, tileY, direction, length);
    } else {
        Shop_Place3PieceUnit(9, tileX, tileY, direction, length);
    }
    return length;
}

// 0x599EF0
int32 Interior_c::Shop_PlaceCounter(uint8 isOnRightSide) {
    auto* const counter = g_furnitureMan.GetFurniture(0, 12, -1, m_box->m_status);
    auto* const till    = g_furnitureMan.GetFurniture(0, 13, -1, m_box->m_status);
    const auto  rotation = RandBelow(4);

    int32 width, depth;
    int32 counterX, tillX;
    if (!isOnRightSide) {
        counterX = m_box->m_door + 2;
        PlaceFurniture(counter, counterX, 1, 0.f, 1, 0, &width, &depth, 0);
        SetTilesStatus(counterX, 0, width + 1, 1, 2, 0);
        tillX = m_box->m_door - 2;
    } else {
        const int32 door = m_box->m_door;
        counterX = door - 5;
        PlaceFurniture(counter, counterX, 1, 0.f, 1, 0, &width, &depth, 0);
        SetTilesStatus(door - 6, 0, width + 1, 1, 2, 0);
        tillX = door + 1;
    }
    PlaceFurniture(till, tillX, 0, 0.f, 1, rotation, &width, &depth, 1);
    return counterX + 2;
}

// 0x59A030
void Interior_c::Shop_PlaceFixedUnits() {
    if (m_box->m_door == -1) {
        return;
    }

    SetTilesStatus(m_box->m_door - 1, 0, 2, 1, 7, 0);

    const int32 spaceLeft  = m_box->m_door - 2;
    const int32 spaceRight = m_box->m_width - m_box->m_door - 2;

    // BUG: If there's no space on either side, the original code doesn't place the counter, and
    // uses a stack variable that wasn't initialized (it happens to contain the `this` pointer) as the position.
    int32 counterEndX = static_cast<int32>(reinterpret_cast<uintptr_t>(this));
    if (spaceRight < 6) {
        if (spaceLeft >= 6) {
            counterEndX = Shop_PlaceCounter(true);
        }
    } else if (spaceLeft < 6) {
        counterEndX = Shop_PlaceCounter(false);
    } else {
        counterEndX = Shop_PlaceCounter(CGeneral::GetRandomNumber() < 0x3FFF);
    }

    AddInteriorInfo(9, static_cast<float>(counterEndX), 2.f, 0, nullptr);
    AddInteriorInfo(10, static_cast<float>(counterEndX), 0.f, 2, nullptr);
}

// 0x59A130
void Interior_c::Shop_FurnishCeiling() {
    // NOP
}

// 0x59A140
void Interior_c::Shop_AddShelfInfo(int32 tileX, int32 tileY, int32 direction) {
    if (s_ShelfInfoCounter > 1) {
        if (RandBelow(100) > 60) {
            AddInteriorInfo(8, static_cast<float>(tileX), static_cast<float>(tileY), direction, nullptr);
            s_ShelfInfoCounter = 1;
            return;
        }
    }
    s_ShelfInfoCounter++;
}

// 0x59A1B0
void Interior_c::Shop_FurnishEdges() {
    const auto& box   = *m_box;
    const int32 width = box.m_width;
    const int32 depth = box.m_depth;

    // Chooses the type of unit for a whole wall
    const auto RollSubGroupId = [] {
        const auto roll = RandBelow(100);
        if (roll > 50) {
            return 0;
        }
        if (roll > 25) {
            return 3;
        }
        return roll > 10 ? 6 : 9;
    };

    // Units along the walls
    const int32 maxX = width - 1;
    const int32 maxY = depth - 2;
    {
        const auto subGroupId = RollSubGroupId();
        for (int32 x = 1; maxX > 1 && x < maxX;) {
            x += Shop_PlaceEdgeUnits(subGroupId, x, depth - 1, 0);
        }
    }
    {
        const auto subGroupId = RollSubGroupId();
        for (int32 y = 1; maxY >= 1 && y <= maxY;) {
            y += Shop_PlaceEdgeUnits(subGroupId, 0, y, 1);
        }
    }
    {
        const auto subGroupId = RollSubGroupId();
        for (int32 y = 1; maxY >= 1 && y <= maxY;) {
            y += Shop_PlaceEdgeUnits(subGroupId, maxX, y, 3);
        }
    }

    // Is `v` outside of the [start, end] range (-1 as start means the range doesn't exist)
    const auto IsOutside = [](int32 v, int8 start, int8 end) {
        return start == -1 || v < start || v > end;
    };

    // Adds a shelf info with some probability (same as in `Shop_AddShelfInfo`, but with a different position)
    const auto TryAddShelfInfo = [&](float x, float y, int32 direction) {
        if (s_ShelfInfoCounter > 1) {
            if (RandBelow(100) > 60) {
                AddInteriorInfo(8, x, y, direction, nullptr);
                s_ShelfInfoCounter = 0;
            }
        }
        s_ShelfInfoCounter++;
    };

    // Shelf infos (and walkable tiles) along the walls
    const int32 lastX = width - 2;
    for (int32 x = 1; lastX >= 1 && x <= lastX; x++) {
        if (IsOutside(x, box.m_tDoorStart, box.m_tDoorEnd)) {
            TryAddShelfInfo(static_cast<float>(x), static_cast<float>(maxY), 2);
        }
        if (GetTileStatus(x, 1) == 0) {
            SetTilesStatus(x, 1, 1, 1, 2, 0);
        }
    }

    const int32 lastY = depth - 3;
    for (int32 y = 2; lastY >= 2 && y <= lastY; y++) {
        if (IsOutside(y, box.m_lDoorStart, box.m_lDoorEnd)) {
            TryAddShelfInfo(1.f, static_cast<float>(y), 3);
        }
        if (IsOutside(y, box.m_rDoorStart, box.m_rDoorEnd)) {
            TryAddShelfInfo(static_cast<float>(lastX), static_cast<float>(y), 1);
        }
    }

    // Mark the aisle tiles
    const int32 innerX = width - 3;
    for (int32 x = 2; innerX >= 2 && x <= innerX; x++) {
        if (GetTileStatus(x, 2) == 0) {
            SetTilesStatus(x, 2, 1, 1, 2, 0);
        }
        SetTilesStatus(x, lastY, 1, 1, 3, 0);
    }
    const int32 innerY = depth - 4;
    for (int32 y = 3; innerY >= 3 && y <= innerY; y++) {
        SetTilesStatus(2, y, 1, 1, 3, 0);
        SetTilesStatus(innerX, y, 1, 1, 3, 0);
    }
    SetTilesStatus(3, 3, width - 6, 1, 3, 0);
}

// 0x593DB0
bool Interior_c::GetBoundingBox(FurnitureEntity_c* entity, CVector* outPoints) {
    switch (m_box->m_type) {
    case 0:
    case 1:
    case 6:
        break;
    default:
        return false;
    }

    const int32 tileX = entity->m_tileX;
    const int32 tileY = entity->m_tileY;

    int32 visited[30 * 30]{};
    visited[tileX * 30 + tileY] = 1;

    int32 minX = tileX, maxX = tileX;
    int32 minY = tileY, maxY = tileY;
    FindBoundingBox(tileX, tileY, &minX, &maxX, &minY, &maxY, visited);

    constexpr float MARGIN = 0.35f; // 0x8D22B0
    GetTileCentre(static_cast<float>(minX) - 0.5f - MARGIN, static_cast<float>(maxY) + MARGIN + 0.5f, &outPoints[0]);
    GetTileCentre(static_cast<float>(minX) - 0.5f - MARGIN, static_cast<float>(minY) - 0.5f - MARGIN, &outPoints[1]);
    GetTileCentre(static_cast<float>(maxX) + MARGIN + 0.5f, static_cast<float>(minY) - 0.5f - MARGIN, &outPoints[2]);
    GetTileCentre(static_cast<float>(maxX) + MARGIN + 0.5f, static_cast<float>(maxY) + MARGIN + 0.5f, &outPoints[3]);
    return true;
}

// 0x593910
void Interior_c::ResetTiles() {
    std::memset(m_tiles, 0, sizeof(m_tiles));

    const auto& box = *m_box;

    // Marks all empty (status 0) tiles in the rectangle as `status`
    const auto MarkEmpty = [&](int32 x, int32 y, int32 w, int32 d, uint8 status) {
        for (int32 ix = 0; ix < w; ix++) {
            for (int32 iy = 0; iy < d; iy++) {
                auto& tile = m_tiles[x + ix][y + iy];
                if (tile == 0) {
                    tile = status;
                }
            }
        }
    };

    // Doors (status 8)
    if (box.m_lDoorStart != -1) {
        const int32 start = box.m_lDoorStart, len = box.m_lDoorEnd - start;
        if (len > 0 && start >= 0 && box.m_width != 0 && start + len <= box.m_depth) {
            MarkEmpty(0, start, 1, len, 8);
        }
    }
    if (box.m_rDoorStart != -1) {
        const int32 start = box.m_rDoorStart, len = box.m_rDoorEnd - start;
        const int32 x = box.m_width - 1;
        if (len > 0 && x >= 0 && start >= 0 && start + len <= box.m_depth) {
            MarkEmpty(x, start, 1, len, 8);
        }
    }
    if (box.m_tDoorStart != -1) {
        const int32 start = box.m_tDoorStart, len = box.m_tDoorEnd - start;
        const int32 y = box.m_depth - 1;
        if (len > 0 && y >= 0 && start >= 0 && start + len <= box.m_width) {
            MarkEmpty(start, y, len, 1, 8);
        }
    }

    // Windows (status 9)
    if (box.m_lWindowStart != -1) {
        const int32 start = box.m_lWindowStart, len = box.m_lWindowEnd - start;
        if (len > 0 && start >= 0 && box.m_width != 0 && start + len <= box.m_depth) {
            MarkEmpty(0, start, 1, len, 9);
        }
    }
    if (box.m_rWindowStart != -1) {
        const int32 start = box.m_rWindowStart, len = box.m_rWindowEnd - start;
        const int32 x = box.m_width - 1;
        if (len > 0 && x >= 0 && start >= 0 && start + len <= box.m_depth) {
            MarkEmpty(x, start, 1, len, 9);
        }
    }
    if (box.m_tWindowStart != -1) {
        const int32 start = box.m_tWindowStart, len = box.m_tWindowEnd - start;
        const int32 y = box.m_depth - 1;
        if (len > 0 && y >= 0 && start >= 0 && start + len <= box.m_width) {
            MarkEmpty(start, y, len, 1, 9);
        }
    }

    // No-go areas (status 11)
    for (int32 i = 0; i < 3; i++) {
        const int32 x = box.m_noGoLeft[i];
        const int32 y = box.m_noGoBottom[i];
        if (x == -1 || y == -1) {
            continue;
        }
        const int32 w = box.m_noGoWidth[i];
        const int32 d = box.m_noGoDepth[i];
        if (x >= 0 && y >= 0 && x + w <= box.m_width && y + d <= box.m_depth && w > 0) {
            MarkEmpty(x, y, w, d, 11);
        }
    }
}

// 0x5934E0
CObject* Interior_c::PlaceObject(uint8 isStealable, Furniture_c* furniture, float offsetX, float offsetY, float offsetZ, float rotationZ) {
    const float x = static_cast<float>(-static_cast<int32>(m_box->m_width)) * 0.5f + offsetX;
    const float y = static_cast<float>(-static_cast<int32>(m_box->m_depth)) * 0.5f + offsetY;
    const float z = static_cast<float>(-static_cast<int32>(m_box->m_height)) * 0.5f + offsetZ
                  - CModelInfo::GetModelInfo(furniture->m_nModelId)->GetColModel()->GetBoundingBox().m_vecMin.z;

    if (s_FurnitureEntityPool.GetNumItems() <= 0) {
        return nullptr;
    }

    CMatrix interiorMat{ &m_matrix, false };
    auto    localMat = CMatrix::Unity();
    localMat.RotateZ(rotationZ * 0.017453292f);
    localMat.GetPosition() += CVector{ x, y, z };
    CMatrix worldMat = interiorMat * localMat;

    auto* const item = s_FurnitureEntityPool.RemoveHead();
    if (!item) {
        return nullptr;
    }

    auto* const obj = new CObject(furniture->m_nModelId, false);
    item->m_entity = obj;
    obj->SetMatrix(worldMat);
    obj->SetAreaCode(static_cast<eAreaCodes>(m_areaCode));
    obj->m_bDontCastShadowsOn = true; // 0x10000
    obj->m_nObjectType = OBJECT_TYPE_DECORATION;
    obj->SetIsStatic(true);
    CWorld::Add(obj);
    item->m_tileX = static_cast<uint16>(static_cast<int32>(offsetX));
    item->m_tileY = static_cast<uint16>(static_cast<int32>(offsetY));
    m_furnitureList.AddItem(item);

    if (isStealable) {
        obj->objectFlags.bIsLiftable = true; // 0x2000

        if (!g_interiorMan.HasInteriorHadStealDataSetup(this)) {
            // First time this interior is set up, remember the object so it can be stolen
            auto& rec     = s_StealableObjects[s_StealableObjectCount];
            rec.entity     = obj;
            rec.modelId    = furniture->m_nModelId;
            rec.interiorId = m_interiorId;
            rec.pos        = CVector{ x, y, z };
            rec.wasStolen  = false;
            s_StealableObjectCount++;
        } else {
            const auto idx = g_interiorMan.FindStealableObjectId(m_interiorId, furniture->m_nModelId, CVector{ x, y, z });
            if (idx >= 0 && s_StealableObjects[idx].wasStolen) {
                // Object was stolen previously, so don't recreate it
                CWorld::Remove(obj);
                delete obj;
                item->m_entity = nullptr;
                m_furnitureList.RemoveItem(item);
                s_FurnitureEntityPool.AddItem(item);
                return nullptr;
            }
            const auto idx2 = g_interiorMan.FindStealableObjectId(m_interiorId, furniture->m_nModelId, CVector{ x, y, z });
            if (idx2 >= 0) {
                s_StealableObjects[idx2].entity = obj;
            }
        }
    }
    return obj;
}

// 0x5913B0
FurnitureEntity_c* Interior_c::GetFurnitureEntity(CEntity* entity) {
    for (auto& item : m_furnitureList) {
        if (item.m_entity == entity) {
            return &item;
        }
    }
    return nullptr;
}

// 0x5913E0
bool Interior_c::IsPtInside(const CVector& pt, CVector bias) {
    const auto dx = pt.x - m_matrix.pos.x;
    const auto dy = pt.y - m_matrix.pos.y;
    const auto dz = pt.z - m_matrix.pos.z;

    // Distance along the right axis
    if (!(std::abs(m_matrix.right.x * dx + m_matrix.right.y * dy + m_matrix.right.z * dz) <= static_cast<float>(m_box->m_width) * 0.5f + bias.x)) {
        return false;
    }
    // Distance along the up axis
    if (!(std::abs(m_matrix.up.x * dx + m_matrix.up.y * dy + m_matrix.up.z * dz) <= static_cast<float>(m_box->m_depth) * 0.5f + bias.y)) {
        return false;
    }
    // Distance along the at axis
    if (!(std::abs(m_matrix.at.x * dx + m_matrix.at.y * dy + m_matrix.at.z * dz) <= static_cast<float>(m_box->m_height) * 0.5f + bias.z)) {
        return false;
    }
    return true;
}

// 0x5914D0
void Interior_c::CalcMatrix(const CVector* translation) {
    m_matrix.right = { 1.f, 0.f, 0.f };
    m_matrix.up    = { 0.f, 1.f, 0.f };
    m_matrix.at    = { 0.f, 0.f, 1.f };
    m_matrix.pos   = { 0.f, 0.f, 0.f };
    m_matrix.flags |= rwMATRIXTYPEORTHONORMAL | rwMATRIXINTERNALIDENTITY;

    const RwV3d axisZ{ 0.f, 0.f, 1.f };
    RwMatrixRotate(&m_matrix, &axisZ, m_box->m_rot, rwCOMBINEREPLACE);
    RwMatrixTranslate(&m_matrix, translation, rwCOMBINEPOSTCONCAT);

    // Make it relative to the entity the interior belongs to
    auto* const entity = m_pGroup->GetEntity();
    if (entity->GetRwObject()) {
        RwMatrixMultiply(&m_matrix, &m_matrix, RwFrameGetMatrix(RwFrameGetParent(entity->GetRwObject())));
    } else {
        // BUG: The original code passes a null matrix here (which would crash)
        RwMatrixMultiply(&m_matrix, &m_matrix, nullptr);
    }
}

// 0x591590
void Interior_c::Furnish() {
    switch (m_box->m_type) {
    case 0: FurnishShop(0);   break;
    case 1: FurnishOffice();  break;
    case 2: FurnishLounge();  break;
    case 3: FurnishBedroom(); break;
    case 4: FurnishKitchen(); break;
    }
}

// 0x5915D0
void Interior_c::Unfurnish() {
    for (auto* item = m_furnitureList.GetHead(); item;) {
        auto* const next   = item->m_pNext;
        auto* const entity = item->m_entity;

        // If the player is holding this entity, don't delete it, but make it a temporary object instead
        auto* const player = FindPlayerPed(-1);
        auto* const held   = player ? player->GetEntityThatThisPedIsHolding() : nullptr;
        if (held && held == entity && held->GetIsTypeObject() && held->AsObject()->objectFlags.bIsLiftable) {
            auto* const obj = held->AsObject();
            CObject::nNoTempObjects++;
            obj->m_nObjectType  = OBJECT_TEMPORARY;
            obj->m_nRemovalTime = CTimer::GetTimeInMS() + 99'999'999;
        } else {
            CWorld::Remove(entity);
            delete entity;
        }
        item->m_entity = nullptr;

        m_furnitureList.RemoveItem(item);
        s_FurnitureEntityPool.AddItem(item);

        item = next;
    }
}

// 0x591680
int8 Interior_c::CheckTilesEmpty(int32 tileX, int32 tileY, int32 width, int32 depth, uint8 allowStatus9) {
    if (tileX < 0 || tileY < 0 || tileX + width > m_box->m_width || tileY + depth > m_box->m_depth) {
        return false;
    }
    for (int32 x = 0; x < width; x++) {
        for (int32 y = 0; y < depth; y++) {
            const auto status = m_tiles[tileX + x][tileY + y];
            if (status != 0 && (!allowStatus9 || status != 9)) {
                return false;
            }
        }
    }
    return true;
}

// 0x591700
void Interior_c::SetTilesStatus(int32 tileX, int32 tileY, int32 width, int32 depth, int32 status, int8 overwrite) {
    if (tileX < 0 || tileY < 0 || tileX + width > m_box->m_width || tileY + depth > m_box->m_depth) {
        return;
    }
    for (int32 x = 0; x < width; x++) {
        for (int32 y = 0; y < depth; y++) {
            auto& tile = m_tiles[tileX + x][tileY + y];
            if (tile == 9 && status == 5) {
                tile = 10;
            } else if (!overwrite) {
                if (tile == 3) {
                    if (status == 3) {
                        return; // NOTE: Returns from the whole function, not just this iteration!
                    }
                    if (status == 4) {
                        tile = 4;
                    }
                } else if (tile == 0) {
                    tile = static_cast<uint8>(status);
                }
            } else if (tile != 5 && tile != 7 && tile != 8) {
                tile = static_cast<uint8>(status);
            }
        }
    }
}

// 0x5917C0
void Interior_c::SetCornerTiles(int32 corner, int32 size, int32 status, uint8 overwrite) {
    switch (corner) {
    case 1:
        SetTilesStatus(0, 0, size, 1, status, overwrite);
        SetTilesStatus(0, 0, 1, size, status, overwrite);
        break;
    case 0:
        SetTilesStatus(0, m_box->m_depth - 1, size, 1, status, overwrite);
        SetTilesStatus(0, m_box->m_depth - size, 1, size, status, overwrite);
        break;
    case 2:
        SetTilesStatus(m_box->m_width - size, 0, size, 1, status, overwrite);
        SetTilesStatus(m_box->m_width - 1, 0, 1, size, status, overwrite);
        break;
    case 3:
        SetTilesStatus(m_box->m_width - size, m_box->m_depth - 1, size, 1, status, overwrite);
        SetTilesStatus(m_box->m_width - 1, m_box->m_depth - size, 1, size, status, overwrite);
        break;
    }
}

// 0x5918E0
int32 Interior_c::GetTileStatus(int32 x, int32 y) {
    if (x < m_box->m_width && y < m_box->m_depth && x >= 0 && y >= 0) {
        return m_tiles[x][y];
    }
    return 1;
}

// 0x591920
int32 Interior_c::GetNumEmptyTiles(int32 tileX, int32 tileY, int32 direction, int32 length) {
    const int32 step = (direction == 3 || direction == 0) ? -1 : 1;

    int32 numEmpty = 0;
    if (direction == 3 || direction == 1) { // Moving along the X axis, `length` tiles wide along Y
        for (;; tileX += step) {
            for (int32 i = 0; i < length; i++) {
                const int32 y = tileY + i;
                if (tileX >= m_box->m_width || y >= m_box->m_depth || tileX < 0 || y < 0 || m_tiles[tileX][y] != 0) {
                    return numEmpty;
                }
            }
            numEmpty++;
        }
    } else { // Moving along the Y axis, `length` tiles wide along X
        for (;; tileY += step) {
            for (int32 i = 0; i < length; i++) {
                const int32 x = tileX + i;
                if (x >= m_box->m_width || tileY >= m_box->m_depth || x < 0 || tileY < 0 || m_tiles[x][tileY] != 0) {
                    return numEmpty;
                }
            }
            numEmpty++;
        }
    }
}

// 0x591B20
void Interior_c::GetRandomTile(int32 status, int32* outTileX, int32* outTileY) {
    int32 x, y;
    do {
        x = RandBelow(m_box->m_width);
        y = RandBelow(m_box->m_depth);
    } while (GetTileStatus(x, y) != status);
    *outTileX = x;
    *outTileY = y;
}

// 0x59A590
void Interior_c::Shop_FurnishAisles() {
    const int32 width      = m_box->m_width;
    const int32 depth      = m_box->m_depth;
    const int32 numAisles  = width - 6;
    const int32 aisleLen   = depth - 7;
    if (numAisles <= 0 || aisleLen <= 0) {
        return;
    }

    AddGotoPt(2, 3, -0.5f, -0.5f);
    const int32 lastY = depth - 3;
    AddGotoPt(2, lastY, -0.5f, 0.5f);

    int32 i = 0;
    for (; i < numAisles; i++) {
        int32 y = 4;

        int32 subGroupId;
        const auto roll = RandBelow(100);
        if (roll > 50) {
            subGroupId = 0;
        } else if (roll > 25) {
            subGroupId = 3;
        } else {
            subGroupId = roll > 10 ? 6 : 9;
        }

        switch (i & 3) {
        case 0:
            if (i != width - 7) {
                for (int32 n = 0; n < aisleLen; n++) {
                    y += Shop_PlaceEdgeUnits(subGroupId, i + 3, y, 3);
                }
            }
            break;
        case 1:
            for (int32 n = 0; n < aisleLen; n++) {
                y += Shop_PlaceEdgeUnits(subGroupId, i + 3, y, 1);
            }
            break;
        case 2:
            for (int32 n = 0; n < aisleLen; n++) {
                Shop_AddShelfInfo(i + 3, n + 4, 3);
            }
            break;
        case 3: {
            const int32 x = i + 3;
            SetTilesStatus(x, 4, 1, aisleLen, 3, 0);
            AddGotoPt(x, 3, -0.5f, -0.5f);
            AddGotoPt(x, lastY, -0.5f, 0.5f);
            break;
        }
        }
    }

    AddGotoPt(i + 3, 3, 0.5f, -0.5f);
    AddGotoPt(i + 3, lastY, 0.5f, 0.5f);
}

// 0x591BD0
CVector* Interior_c::GetTileCentre(float offsetX, float offsetY, CVector* pointsIn) {
    pointsIn->x = -static_cast<float>(m_box->m_width) * 0.5f + offsetX + 0.5f;
    pointsIn->y = -static_cast<float>(m_box->m_depth) * 0.5f + offsetY + 0.5f;
    pointsIn->z = -static_cast<float>(m_box->m_height) * 0.5f;
    RwV3dTransformPoints(pointsIn, pointsIn, 1, &m_matrix);
    return pointsIn;
}

// 0x591D20
void Interior_c::AddGotoPt(int32 tileX, int32 tileY, float offsetX, float offsetY) {
    if (m_gotoPtsCount >= (int8)std::size(m_gotoPts)) {
        return;
    }

    const bool isOnWallTile = tileX < m_box->m_width && tileY < m_box->m_depth && tileX >= 0 && tileY >= 0 && m_tiles[tileX][tileY] == 3;
    if (!isOnWallTile && GetTileStatus(tileX, tileY) != 7) {
        return;
    }

    auto& pt = m_gotoPts[m_gotoPtsCount];
    GetTileCentre(static_cast<float>(tileX) + offsetX, static_cast<float>(tileY) + offsetY, &pt.Pos);
    pt.TileX = static_cast<uint8>(tileX);
    pt.TileY = static_cast<uint8>(tileY);

    if (tileX >= 0 && tileY >= 0 && tileX + 1 <= m_box->m_width && tileY + 1 <= m_box->m_depth) {
        auto& tile = m_tiles[tileX][tileY];
        if (tile == 3 || tile == 0) {
            tile = 4;
        }
    }

    m_gotoPtsCount++;
}

// 0x591E40
bool Interior_c::AddInteriorInfo(int32 actionType, float offsetX, float offsetY, int32 direction, CEntity* entityIgnoredCollision) {
    if (m_interiorInfosCount >= (int8)std::size(m_interiorInfos)) {
        return false;
    }

    CVector pos;
    GetTileCentre(offsetX, offsetY, &pos);
    pos.z += 0.8f;

    CVector dir{};
    if (direction != -1) {
        switch (direction) {
        case 3: dir.x = -1.f; break;
        case 1: dir.x = 1.f;  break;
        case 2: dir.y = 1.f;  break;
        case 0: dir.y = -1.f; break;
        }
        RwV3dTransformVectors(&dir, &dir, 1, &m_matrix);
    }

    auto& info                  = m_interiorInfos[m_interiorInfosCount];
    info.Type                   = static_cast<eInteriorInfoType>(actionType);
    info.Pos                    = pos;
    info.Dir                    = dir;
    info.IsInUse                = false;
    info.EntityIgnoredCollision = entityIgnoredCollision;

    m_interiorInfosCount++;
    return true;
}

// 0x591F90
void Interior_c::AddPickups() {
    if (CTimer::GetTimeInMS() - s_TimeLastPickupsGenerated <= 179'999u) {
        return;
    }

    for (int32 i = 0; i < 100; i++) {
        const int32 tileX = RandBelow(m_box->m_width - 1);
        const int32 tileY = RandBelow(m_box->m_depth - 1);

        if (tileX >= m_box->m_width || tileY >= m_box->m_depth || tileX < 0 || tileY < 0) {
            continue;
        }
        const auto status = m_tiles[tileX][tileY];
        if (status != 0 && status != 3 && status != 4) {
            continue;
        }

        CVector pos;
        GetTileCentre(static_cast<float>(tileX), static_cast<float>(tileY), &pos);

        if (RandBelow(100) < 75) {
            CPickups::GenerateNewOne(pos, ModelIndices::MI_MONEY, PICKUP_MONEY, 10 + RandBelow(40));
        } else {
            pos.z += 0.5f;

            const auto roll = RandBelow(100);
            eWeaponType weaponType;
            if (roll < 40) {
                weaponType = WEAPON_BASEBALLBAT;
            } else if (roll < 80) {
                weaponType = WEAPON_PISTOL;
            } else if (roll >= 90) {
                weaponType = WEAPON_SHOTGUN;
            } else {
                weaponType = WEAPON_KNIFE;
            }
            CPickups::GenerateNewOne_WeaponType(pos, weaponType, PICKUP_ONCE, 3 + RandBelow(15), false, nullptr);
        }
        break; // Only place a single pickup
    }
}

// 0x5922C0
void Interior_c::FindBoundingBox(int32 tileX, int32 tileY, int32* minX, int32* maxX, int32* minY, int32* maxY, int32* visited) {
    const int32 width = m_box->m_width;
    const int32 depth = m_box->m_depth;

    // NOTE: The original turned the recursion towards `tileY - 1` into a loop
    for (;;) {
        // Towards x - 1
        if (tileX > 0) {
            const int32 x = tileX - 1;
            if (x < width && tileY < depth && x >= 0 && tileY >= 0 && m_tiles[x][tileY] == 5 && !visited[x * 30 + tileY]) {
                visited[x * 30 + tileY] = 1;
                if (x < *minX) {
                    *minX = x;
                }
                FindBoundingBox(x, tileY, minX, maxX, minY, maxY, visited);
            }
        }

        // Towards y + 1
        {
            const int32 y = tileY + 1;
            if (tileY < 29 && tileX < width && y < depth && tileX >= 0 && y >= 0 && m_tiles[tileX][y] == 5 && !visited[tileX * 30 + y]) {
                visited[tileX * 30 + y] = 1;
                if (*maxY < y) {
                    *maxY = y;
                }
                FindBoundingBox(tileX, y, minX, maxX, minY, maxY, visited);
            }
        }

        // Towards x + 1
        if (tileX < 29) {
            const int32 x = tileX + 1;
            if (x < width && tileY < depth && x >= 0) {
                if (tileY < 0) {
                    return;
                }
                if (m_tiles[x][tileY] == 5 && !visited[x * 30 + tileY]) {
                    visited[x * 30 + tileY] = 1;
                    if (*maxX < x) {
                        *maxX = x;
                    }
                    FindBoundingBox(x, tileY, minX, maxX, minY, maxY, visited);
                }
            }
        }

        // Towards y - 1
        if (tileY < 1 || tileX >= width) {
            return;
        }
        const int32 y = tileY - 1;
        if (y >= depth || tileX < 0 || y < 0) {
            return;
        }
        if (m_tiles[tileX][y] != 5 || visited[tileX * 30 + y]) {
            return;
        }
        visited[tileX * 30 + y] = 1;
        if (y < *minY) {
            *minY = y;
        }
        tileY = y;
    }
}

// 0x5924A0
void Interior_c::CalcExitPts() {
    const auto& box   = *m_box;
    const int32 count = m_gotoPtsCount;

    // Bottom (door side)
    if (box.m_door >= 0) {
        auto&       exitPt = m_exitPts[0];
        const float x      = static_cast<float>(box.m_door) - 0.5f;
        float       y;
        if (count <= 2) {
            exitPt.GotoPtIdx[0] = exitPt.GotoPtIdx[1] = -1;
            y = 0.f;
        } else {
            int32 i = 0;
            do {
                if (static_cast<float>(static_cast<int8>(m_gotoPts[i].TileX)) > x) {
                    break;
                }
                i += 2;
            } while (i < count);
            if (i == 0) {
                exitPt.GotoPtIdx[0] = 0;
                exitPt.GotoPtIdx[1] = -1;
            } else if (i == count) {
                exitPt.GotoPtIdx[0] = static_cast<int8>(count - 2);
                exitPt.GotoPtIdx[1] = -1;
            } else {
                exitPt.GotoPtIdx[0] = static_cast<int8>(i);
                exitPt.GotoPtIdx[1] = static_cast<int8>(i - 2);
            }
            y = static_cast<float>(static_cast<int8>(m_gotoPts[0].TileY)) - 0.25f;
        }
        GetTileCentre(x, y, &exitPt.PosInside);
        GetTileCentre(x, -0.25f, &exitPt.PosOutside);
    }

    // Left
    if (box.m_lDoorStart >= 0) {
        auto&       exitPt = m_exitPts[1];
        const float y      = static_cast<float>(box.m_lDoorEnd - box.m_lDoorStart) * 0.5f + static_cast<float>(box.m_lDoorStart) - 0.5f;
        float       x;
        if (count <= 2) {
            exitPt.GotoPtIdx[0] = exitPt.GotoPtIdx[1] = -1;
            x = 0.f;
        } else {
            if (static_cast<float>(static_cast<int8>(m_gotoPts[0].TileY)) > y) {
                exitPt.GotoPtIdx[0] = 0;
                exitPt.GotoPtIdx[1] = -1;
            } else if (!(static_cast<float>(static_cast<int8>(m_gotoPts[1].TileY)) < y)) {
                exitPt.GotoPtIdx[0] = 0;
                exitPt.GotoPtIdx[1] = 1;
            } else {
                exitPt.GotoPtIdx[0] = 1;
                exitPt.GotoPtIdx[1] = -1;
            }
            x = static_cast<float>(static_cast<int8>(m_gotoPts[0].TileX)) - 0.25f;
        }
        GetTileCentre(x, y, &exitPt.PosInside);
        GetTileCentre(-0.25f, y, &exitPt.PosOutside);
    }

    // Top
    if (box.m_tDoorStart >= 0) {
        auto&       exitPt = m_exitPts[2];
        const float x      = static_cast<float>(box.m_tDoorEnd - box.m_tDoorStart) * 0.5f + static_cast<float>(box.m_tDoorStart) - 0.5f;
        float       y;
        if (count <= 2) {
            exitPt.GotoPtIdx[0] = exitPt.GotoPtIdx[1] = -1;
            y = static_cast<float>(box.m_depth - 1);
        } else {
            int32 i = 1;
            do {
                if (static_cast<float>(static_cast<int8>(m_gotoPts[i].TileX)) > x) {
                    break;
                }
                i += 2;
            } while (i < count);
            if (i == count) {
                exitPt.GotoPtIdx[0] = static_cast<int8>(count - 1);
                exitPt.GotoPtIdx[1] = -1;
            } else {
                exitPt.GotoPtIdx[0] = static_cast<int8>(i);
                exitPt.GotoPtIdx[1] = static_cast<int8>(i - 2);
            }
            y = static_cast<float>(static_cast<int8>(m_gotoPts[1].TileY)) + 0.25f;
        }
        GetTileCentre(x, y, &exitPt.PosInside);
        GetTileCentre(x, static_cast<float>(box.m_depth - 1) + 0.25f, &exitPt.PosOutside);
    }

    // Right
    if (box.m_rDoorStart >= 0) {
        auto&       exitPt = m_exitPts[3];
        const float y      = static_cast<float>(box.m_rDoorEnd - box.m_rDoorStart) * 0.5f + static_cast<float>(box.m_rDoorStart) - 0.5f;
        float       x;
        if (count <= 2) {
            exitPt.GotoPtIdx[0] = exitPt.GotoPtIdx[1] = -1;
            x = static_cast<float>(box.m_width - 1);
        } else {
            if (static_cast<float>(static_cast<int8>(m_gotoPts[count - 2].TileY)) > y) {
                exitPt.GotoPtIdx[0] = static_cast<int8>(count - 2);
                exitPt.GotoPtIdx[1] = -1;
            } else if (!(static_cast<float>(static_cast<int8>(m_gotoPts[count - 1].TileY)) < y)) {
                exitPt.GotoPtIdx[0] = static_cast<int8>(count - 1);
                exitPt.GotoPtIdx[1] = static_cast<int8>(count - 2);
            } else {
                exitPt.GotoPtIdx[0] = static_cast<int8>(count - 1);
                exitPt.GotoPtIdx[1] = -1;
            }
            // NOTE: Uses the TileX of the 2nd last point (the original does that too)
            x = static_cast<float>(static_cast<int8>(m_gotoPts[count - 2].TileX)) + 0.25f;
        }
        GetTileCentre(x, y, &exitPt.PosInside);
        GetTileCentre(static_cast<float>(box.m_width - 1) + 0.25f, y, &exitPt.PosOutside);
    }
}

// 0x5929F0
bool Interior_c::IsVisible() {
    const CVector camPos = TheCamera.GetPosition();
    if (IsPtInside(camPos, { 5.f, 5.f, 0.f })) {
        return true;
    }
    if (m_box->m_door > 0) {
        const auto dx = camPos.x - m_position.x;
        const auto dy = camPos.y - m_position.y;
        if (dx * dx + dy * dy < 100.f) {
            return true;
        }
    }
    return false;
}

// 0x592AA0
CEntity* Interior_c::PlaceFurniture(Furniture_c* furniture, int32 tileX, int32 tileY, float offsetZ, int32 checkTiles, int32 rotation, int32* outWidth, int32* outDepth, uint8 a9) {
    const auto Fail = [&]() -> CEntity* {
        *outWidth = 0;
        *outDepth = 0;
        return nullptr;
    };

    if (s_FurnitureEntityPool.GetNumItems() <= 0) {
        return Fail();
    }

    // Size of the furniture after rotation
    const bool  isSideways = rotation == 1 || rotation == 3;
    const int32 width      = isSideways ? furniture->m_nWidthY : furniture->m_nWidthX;
    const int32 depth      = isSideways ? furniture->m_nWidthX : furniture->m_nWidthY;
    const int32 baseWidthX = furniture->m_nWidthX;
    const int32 baseWidthY = furniture->m_nWidthY;

    if (checkTiles == 1 && !a9 && !CheckTilesEmpty(tileX, tileY, width, depth, furniture->m_bCanPlaceInFrontOfWindow)) {
        return Fail();
    }

    // Random rotation around the center of the furniture
    CMatrix jitterMat;
    if (furniture->m_nMaxAng) {
        const float cy = 0.5f - static_cast<float>(baseWidthY) * 0.5f;
        const float cx = 0.5f - static_cast<float>(baseWidthX) * 0.5f;

        auto toOrigin = CMatrix::Unity();
        toOrigin.SetTranslate({ cx, cy, 0.f });

        auto rot = CMatrix::Unity();
        rot.RotateZ(static_cast<float>(CGeneral::GetRandomNumberInRange(-static_cast<int32>(furniture->m_nMaxAng), static_cast<int32>(furniture->m_nMaxAng))) * 0.017453292f);

        auto fromOrigin = CMatrix::Unity();
        fromOrigin.SetTranslate({ -cx, -cy, 0.f });

        jitterMat = (fromOrigin * rot) * toOrigin;
    } else {
        jitterMat.SetUnity();
    }

    CMatrix interiorMat{ &m_matrix, false };
    auto    placeMat = CMatrix::Unity();
    placeMat.RotateZ(static_cast<float>(rotation) * 1.5707964f);

    int32 xOff = 0, yOff = 0;
    switch (rotation) {
    case 1: xOff = width - 1; break;
    case 2: xOff = width - 1; yOff = depth - 1; break;
    case 3: yOff = depth - 1; break;
    }

    if (checkTiles == 1 || checkTiles == 2) {
        const float z = static_cast<float>(-static_cast<int32>(m_box->m_height)) * 0.5f + offsetZ;
        const float y = static_cast<float>(-static_cast<int32>(m_box->m_depth)) * 0.5f + static_cast<float>(yOff) + static_cast<float>(tileY) + 0.5f;
        const float x = static_cast<float>(-static_cast<int32>(m_box->m_width)) * 0.5f + static_cast<float>(xOff) + static_cast<float>(tileX) + 0.5f;
        placeMat.GetPosition() += CVector{ x, y, z };
    } else if (checkTiles == 0) {
        const float z = static_cast<float>(m_box->m_height) * 0.5f - offsetZ;
        const float y = static_cast<float>(-static_cast<int32>(m_box->m_depth)) * 0.5f + static_cast<float>(yOff) + static_cast<float>(tileY) + 0.5f;
        const float x = static_cast<float>(-static_cast<int32>(m_box->m_width)) * 0.5f + static_cast<float>(xOff) + static_cast<float>(tileX) + 0.5f;
        placeMat.GetPosition() += CVector{ x, y, z };
    }

    CMatrix worldMat = (interiorMat * placeMat) * jitterMat;

    auto* const item = s_FurnitureEntityPool.RemoveHead();
    if (!item) {
        return Fail();
    }

    auto* const building = new CBuilding();
    item->m_entity = building;
    building->SetModelIndexNoCreate(furniture->m_nModelId); // vtable slot 6 (0x18)
    building->SetMatrix(worldMat);
    building->SetAreaCode(static_cast<eAreaCodes>(m_areaCode));
    building->m_bDontCastShadowsOn = true; // 0x10000
    building->m_bIsTempBuilding    = true; // 0x400000
    CWorld::Add(building);
    item->m_tileX = static_cast<uint16>(tileX);
    item->m_tileY = static_cast<uint16>(tileY);
    m_furnitureList.AddItem(item);

    if (checkTiles == 1) {
        SetTilesStatus(tileX, tileY, width, depth, furniture->m_bIsTall ? 6 : 5, 1);
    }

    *outWidth = width;
    *outDepth = depth;
    return item->m_entity;
}

// 0x593120
CEntity* Interior_c::PlaceFurnitureOnWall(int32 furnitureGroupId, int32 furnitureSubgroupId, int32 furnitureId, float offsetZ, int32 checkTiles, int32 rotation, int32 posAlongWall,
                                          int32 distFromWall, int32* outRotation, int32* outPosAlongWall, int32* outTileX, int32* outTileY, int32* outWidth, int32* outDepth) {
    auto* const furniture = g_furnitureMan.GetFurniture(furnitureGroupId, furnitureSubgroupId, static_cast<int16>(furnitureId), m_box->m_status);
    if (!furniture) {
        return nullptr;
    }

    // Only try once if everything is fixed, otherwise try (with random values) a bunch of times
    const int32 numTries = (rotation != -1 && posAlongWall != -1) ? 1 : 100;

    int32 rot   = rotation;
    int32 along = posAlongWall;
    for (int32 i = 0; i < numTries; i++) {
        if (rotation == -1) {
            rot = RandBelow(4);
        }

        int32 tileX, tileY;
        if (rot == 1 || rot == 3) { // Along the Y axis
            if (posAlongWall == -1) {
                along = RandBelow(m_box->m_depth - furniture->m_nWidthX);
            }
            tileY = along;
            tileX = rot == 1 ? distFromWall : m_box->m_width - furniture->m_nWidthY - distFromWall;
        } else { // Along the X axis
            if (posAlongWall == -1) {
                along = RandBelow(m_box->m_width - furniture->m_nWidthX);
            }
            tileX = along;
            tileY = rot == 2 ? distFromWall : m_box->m_depth - furniture->m_nWidthY - distFromWall;
        }

        int32 width, depth;
        if (auto* const placed = PlaceFurniture(furniture, tileX, tileY, offsetZ, checkTiles, rot, &width, &depth, 0)) {
            if (outRotation)     { *outRotation     = rot; }
            if (outPosAlongWall) { *outPosAlongWall = along; }
            if (outTileX)        { *outTileX        = tileX; }
            if (outTileY)        { *outTileY        = tileY; }
            if (outWidth)        { *outWidth        = width; }
            if (outDepth)        { *outDepth        = depth; }
            return placed;
        }
    }
    return nullptr;
}

// 0x593340
CEntity* Interior_c::PlaceFurnitureInCorner(int32 furnitureGroupId, int32 furnitureSubgroupId, int32 id, float offsetZ, int32 checkTiles, int32 rotation, int32 distFromWall,
                                            int32* outRotation, int32* outTileX, int32* outTileY, int32* outWidth, int32* outDepth) {
    auto* const furniture = g_furnitureMan.GetFurniture(furnitureGroupId, furnitureSubgroupId, static_cast<int16>(id), m_box->m_status);
    if (!furniture) {
        return nullptr;
    }

    // Only try once if the rotation is fixed
    const int32 numTries = rotation != -1 ? 1 : 20;

    int32 rot   = rotation;
    int32 tileX = rotation; // NOTE: Uninitialized if the rotation isn't in [0, 3] (original initializes them with the rotation argument)
    int32 tileY = rotation;
    for (int32 i = 0; i < numTries; i++) {
        if (rotation == -1) {
            rot = RandBelow(4);
        }

        switch (rot) {
        case 1:
            tileX = distFromWall;
            tileY = 0;
            break;
        case 3:
            tileX = m_box->m_width - furniture->m_nWidthY - distFromWall;
            tileY = m_box->m_depth - furniture->m_nWidthX;
            break;
        case 2:
            tileX = m_box->m_width - furniture->m_nWidthX;
            tileY = distFromWall;
            break;
        case 0:
            tileX = 0;
            tileY = m_box->m_depth - furniture->m_nWidthY - distFromWall;
            break;
        }

        int32 width, depth;
        if (auto* const placed = PlaceFurniture(furniture, tileX, tileY, offsetZ, checkTiles, rot, &width, &depth, 0)) {
            if (outRotation) { *outRotation = rot; }
            if (outTileX)    { *outTileX    = tileX; }
            if (outTileY)    { *outTileY    = tileY; }
            if (outWidth)    { *outWidth    = width; }
            if (outDepth)    { *outDepth    = depth; }
            return placed;
        }
    }
    return nullptr;
}

// 0x591C50
bool Interior_c::FindEmptyTiles(int32 width, int32 depth, int32* outTileX, int32* outTileY) {
    for (int32 i = 0; i < 100; i++) {
        const int32 x = RandBelow(m_box->m_width - width);
        const int32 y = RandBelow(m_box->m_depth - depth);
        if (CheckTilesEmpty(x, y, width, depth, 1)) {
            *outTileX = x;
            *outTileY = y;
            return true;
        }
    }
    return false;
}

// 0x59A790
void Interior_c::FurnishShop(int32 furnitureGroupId) {
    m_furnitureGroupId = static_cast<int8>(furnitureGroupId);

    const int32 door = m_box->m_door;
    if (door - 1 > 5 || m_box->m_width - door > 5) {
        SetTilesStatus(0, 0, 1, 1, 2, 0);
        SetTilesStatus(0, m_box->m_depth - 1, 1, 1, 2, 0);
        SetTilesStatus(m_box->m_width - 1, 0, 1, 1, 2, 0);
        SetTilesStatus(m_box->m_width - 1, m_box->m_depth - 1, 1, 1, 2, 0);

        Shop_PlaceFixedUnits();
        Shop_FurnishEdges();
        Shop_FurnishAisles();
    }
}
