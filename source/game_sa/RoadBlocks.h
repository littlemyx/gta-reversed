#pragma once

#include "Vector.h"
#include "NodeAddress.h"

class CVehicle;
class CPed;

struct CScriptRoadBlock {
    CVector CornerA;
    CVector CornerB;
    bool    IsActive;
    bool    IsSafeToCreate;
    bool    IsGangRoadBlock;
};
VALIDATE_SIZE(CScriptRoadBlock, 0x1C);

static constexpr size_t MAX_ROADBLOCKS{ 325 };
static constexpr size_t MAX_SCRIPT_ROADBLOCKS{ 16 };

class CRoadBlocks {
public:
    static void Init();
    static void ClearScriptRoadBlocks();
    static void CreateRoadBlockBetween2Points(CVector a, CVector b, bool isGangRoadBlock);
    static void GenerateRoadBlockPedsForCar(CVehicle* vehicle, int32 orientationToCar, ePedType pedType);
    static void GenerateRoadBlocks();
    static void RegisterScriptRoadBlock(CVector cornerA, CVector cornerB, bool isGangRoadBlock);

private:
    friend void InjectHooksMain();
    static void InjectHooks();

    static bool GetRoadBlockNodeInfo(CNodeAddress nodeAddress, float& outWidth, CVector& outDir);
    static bool ClearSpaceForRoadBlockObject(CVector cornerA, CVector cornerB);

private:
    static inline NOTSA_GLOBAL(InOrOut, 0xA43438, (std::array<bool, MAX_ROADBLOCKS>), {});
    static inline auto& RoadBlockNodes            = StaticRef<std::array<CNodeAddress, MAX_ROADBLOCKS>>(0xA435A0);
    static inline NOTSA_GLOBAL(aScriptRoadBlocks, 0xA43AB8, (std::array<CScriptRoadBlock, MAX_SCRIPT_ROADBLOCKS>), {});
    static inline NOTSA_GLOBAL(GenerateDynamicRoadBlocks, 0xA43584, (bool), {});
    static inline NOTSA_GLOBAL(NumRoadBlocks, 0xA43580, (int32), {});
};
