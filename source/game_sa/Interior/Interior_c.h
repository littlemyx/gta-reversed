#pragma once

#include "Base.h"

#include "rwplcore.h" // RwMatrix

#include "Vector.h"
#include "NodeAddress.h"
#include "InteriorInfo_t.h"
#include "List_c.h"
#include "ListItem_c.h"
#include "FurnitureEntity_c.h"

class CEntity;
class CObject;
class Furniture_c;
class InteriorGroup_c;

class Interior_c : public ListItem_c<Interior_c> {
public:
    //! A point peds can walk to inside of the interior (see `AddGotoPt`)
    struct GotoPt_t {
        uint8   TileX;
        uint8   TileY;
        // 2 bytes of padding
        CVector Pos; //!< World position
    };
    VALIDATE_SIZE(GotoPt_t, 0x10);

    //! A point peds can use to leave the interior (see `CalcExitPts`)
    struct ExitPt_t {
        int8    field_0[2];
        int8    GotoPtIdx[2]; //!< Indices of the `m_gotoPts` to go through (-1 = none)
        CVector PosInside;
        float   field_14;
        CVector PosOutside;
    };
    VALIDATE_SIZE(ExitPt_t, 0x20);

    int32             m_interiorId;         // 0x8
    InteriorGroup_c*  m_pGroup;             // 0xC
    int32             m_areaCode;           // 0x10
    tEffectInterior*  m_box;                // 0x14
    RwMatrix          m_matrix;             // 0x18
    int32             field_58;             // 0x58
    TList_c<FurnitureEntity_c> m_furnitureList; // 0x5C
    uint8             m_tiles[30][30];      // 0x68 - Tile status, indexed as [x][y] (see `GetTileStatus`)
    int16             field_3EC;            // 0x3EC
    int16             field_3EE;            // 0x3EE
    CNodeAddress      m_nodeAddress;        // 0x3F0
    int16             field_3F4;            // 0x3F4
    int16             field_3F6;            // 0x3F6
    int32             field_3F8;            // 0x3F8
    int32             field_3FC;            // 0x3FC
    CVector           m_position;           // 0x400
    int8              m_gotoPtsCount;       // 0x40C
    int8              m_interiorInfosCount; // 0x40D
    GotoPt_t          m_gotoPts[16];        // 0x410
    ExitPt_t          m_exitPts[4];         // 0x510 - Bottom, left, top, right (see `CalcExitPts`)
    InteriorInfo_t    m_interiorInfos[16];  // 0x590
    int8              m_furnitureGroupId;   // 0x790
    int8              m_furnitureId;        // 0x791
    int8              m_furnitureId2;       // 0x792 - Used by the office (second furniture id)
    int8              field_793;            // 0x793

public:
    static void InjectHooks();

    Interior_c() = default;
    ~Interior_c() = default; // 0x591360

    int32 Init(const CVector& pos);
    void Exit();

    CObject* Bedroom_AddTableItem(int32 groupId, int32 subGroupId, int32 rotation, int32 tileX, int32 tileY, int32 rotationIdx);
    void FurnishBedroom();
    void Kitchen_FurnishEdges();
    void FurnishKitchen();
    CObject* Lounge_AddTV(int32 rotation, int32 unused1, int32 unused2, int32 unused3);
    CObject* Lounge_AddHifi(int32 rotation, int32 tileX, int32 tileY, int32 unused);
    void Lounge_AddChairInfo(int32 rotation, int32 offset, CEntity* entityIgnoredCollision);
    void Lounge_AddSofaInfo(int32 rotation, int32 offset, CEntity* entityIgnoredCollision);
    void FurnishLounge();
    int32 Office_PlaceEdgeFillers(int32 subGroupId, int32 tileX, int32 tileY, int32 direction, int32 unused);
    int32 Office_PlaceDesk(int32 tileX, int32 tileY, int32 direction, int32 unused1, uint8 unused2, int32 furnitureId);
    int32 Office_PlaceEdgeDesks(int32 unused, int32 tileX, int32 tileY, int32 direction, int32 side);
    void Office_FurnishEdges();
    int32 Office_PlaceDeskQuad(int32 unused, int32 tileX, int32 tileY, int32 furnitureId);
    void Office_FurnishCenter();
    void FurnishOffice();
    int8 Shop_Place3PieceUnit(int32 subGroupId, int32 tileX, int32 tileY, int32 direction, int32 length);
    int32 Shop_PlaceEdgeUnits(int32 subGroupId, int32 tileX, int32 tileY, int32 direction);
    int32 Shop_PlaceCounter(uint8 isOnRightSide);
    void Shop_PlaceFixedUnits();
    void Shop_FurnishCeiling();
    void Shop_AddShelfInfo(int32 tileX, int32 tileY, int32 direction);
    void Shop_FurnishEdges();
    bool GetBoundingBox(FurnitureEntity_c* entity, CVector* outPoints);
    void ResetTiles();
    CObject* PlaceObject(uint8 isStealable, Furniture_c* furniture, float offsetX, float offsetY, float offsetZ, float rotationZ);
    FurnitureEntity_c* GetFurnitureEntity(CEntity*);
    bool IsPtInside(const CVector& pt, CVector bias = {});
    void CalcMatrix(const CVector* translation);
    void Furnish();
    void Unfurnish();
    int8 CheckTilesEmpty(int32 tileX, int32 tileY, int32 width, int32 depth, uint8 allowStatus9);
    void SetTilesStatus(int32 tileX, int32 tileY, int32 width, int32 depth, int32 status, int8 overwrite);
    void SetCornerTiles(int32 corner, int32 size, int32 status, uint8 overwrite);
    int32 GetTileStatus(int32 x, int32 y);
    int32 GetNumEmptyTiles(int32 tileX, int32 tileY, int32 direction, int32 length);
    void GetRandomTile(int32 status, int32* outTileX, int32* outTileY);
    void Shop_FurnishAisles();
    CVector* GetTileCentre(float offsetX, float offsetY, CVector* pointsIn);
    void AddGotoPt(int32 tileX, int32 tileY, float offsetX, float offsetY);
    bool AddInteriorInfo(int32 actionType, float offsetX, float offsetY, int32 direction, CEntity* entityIgnoredCollision);
    void AddPickups();
    void FindBoundingBox(int32 tileX, int32 tileY, int32* minX, int32* maxX, int32* minY, int32* maxY, int32* visited);
    void CalcExitPts();
    bool IsVisible();
    CEntity* PlaceFurniture(Furniture_c* furniture, int32 tileX, int32 tileY, float offsetZ, int32 checkTiles, int32 rotation, int32* outWidth, int32* outDepth, uint8 a9);
    CEntity* PlaceFurnitureOnWall(int32 furnitureGroupId, int32 furnitureSubgroupId, int32 furnitureId, float offsetZ, int32 checkTiles, int32 rotation, int32 posAlongWall,
                                  int32 distFromWall, int32* outRotation, int32* outPosAlongWall, int32* outTileX, int32* outTileY, int32* outWidth, int32* outDepth);
    CEntity* PlaceFurnitureInCorner(int32 furnitureGroupId, int32 furnitureSubgroupId, int32 id, float offsetZ, int32 checkTiles, int32 rotation, int32 distFromWall,
                                    int32* outRotation, int32* outTileX, int32* outTileY, int32* outWidth, int32* outDepth);
    bool FindEmptyTiles(int32 width, int32 depth, int32* outTileX, int32* outTileY);
    void FurnishShop(int32 furnitureGroupId);

    auto GetNodeAddress() const { return m_nodeAddress; }
};
VALIDATE_SIZE(Interior_c, 0x794);
