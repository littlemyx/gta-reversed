/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "RenderWare.h"
#include <Enums/eAreaCodes.h>

class CColModel;
class CCollisionData;
class CEntity;

class CGame {
public:
    //! Current number of area
    static inline auto& aDatFile = StaticRef<char[32]>(0xB728EC);
    static inline auto& currLevel = StaticRef<int32>(0xB7290C);
    static inline auto& bMissionPackGame = StaticRef<uint8>(0xB72910);
    static inline auto& currArea = StaticRef<eAreaCodesS32>(0xB72914);
    static inline auto& m_pWorkingMatrix1 = StaticRef<RwMatrix*>(0xB72920);
    static inline auto& m_pWorkingMatrix2 = StaticRef<RwMatrix*>(0xB72924);


public:
    static void InjectHooks();

    static bool CanSeeOutSideFromCurrArea();
    static bool CanSeeWaterFromCurrArea();

    static void GenerateTempPedAtStartOfNetworkGame();

    static bool Init1(const char* datFile);
    static bool Init2(const char* datFile);
    static bool Init3(const char* datFile);
    static void Initialise(char const *datFile);
    static bool InitialiseCoreDataAfterRW();
    static bool InitialiseEssentialsAfterRW();
    static void InitialiseOnceBeforeRW();
    static bool InitialiseRenderWare();
    static void InitialiseWhenRestarting();
    static void Process();
    static void ReInitGameObjectVariables();
    static void ReloadIPLs();

    static void ShutDownForRestart();
    static bool Shutdown();
    static void ShutdownRenderWare();
    static void DrasticTidyUpMemory(bool a1);
    static void FinalShutdown();
    static void TidyUpMemory(bool a1, bool clearD3Dmem);
    static eAreaCodes GetCurrentAreaCode() { return currArea; }
    static eAreaCodes GetPlayerOrCurrentAreaCode();
};

static inline auto& gameTxdSlot = StaticRef<int32>(0xB728E8);
static inline auto& gbLARiots = StaticRef<bool>(0xB72958);
static inline auto& gbLARiots_NoPoliceCars = StaticRef<bool>(0xB72959);
// The collision model of the fight strike (CTaskSimpleFight::FightSetUpCol) is ONE CColModel (0x30 bytes at 0xC17824, ctor 0x40FB60).
// The 0x30 bytes behind it (0xC17854) are the CCollisionData it points to (ctor 0x40F030), NOT a second CColModel.
static inline auto& col1     = StaticRef<CColModel>(0xC17824);
static inline auto& col1Data = StaticRef<CCollisionData>(0xC17854);

bool MoveMem(void **pMem);
bool MoveColModelMemory(CColModel &colModel, bool a2);
RpGeometry *MoveGeometryMemory(RpGeometry *geometry);
bool TidyUpModelInfo2(CEntity *entity, bool a2);
void ValidateVersion();
void D3DDeviceRestoreCallback();
bool DoHaspChecks();
