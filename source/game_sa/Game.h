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
    static inline NOTSA_GLOBAL(aDatFile, 0xB728EC, (char[32]), {});
    static inline NOTSA_GLOBAL(currLevel, 0xB7290C, (int32), {});
    static inline NOTSA_GLOBAL(bMissionPackGame, 0xB72910, (uint8), {});
    static inline NOTSA_GLOBAL(currArea, 0xB72914, (eAreaCodesS32), {});
    static inline NOTSA_GLOBAL(m_pWorkingMatrix1, 0xB72920, (RwMatrix*), {});
    static inline NOTSA_GLOBAL(m_pWorkingMatrix2, 0xB72924, (RwMatrix*), {});


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

NOTSA_GLOBAL_HDR(gameTxdSlot, 0xB728E8, (int32), {});
NOTSA_GLOBAL_HDR(gbLARiots, 0xB72958, (bool), {});
NOTSA_GLOBAL_HDR(gbLARiots_NoPoliceCars, 0xB72959, (bool), {});
// The collision model of the fight strike (CTaskSimpleFight::FightSetUpCol) is ONE CColModel (0x30 bytes at 0xC17824, ctor 0x40FB60).
// The 0x30 bytes behind it (0xC17854) are the CCollisionData it points to (ctor 0x40F030), NOT a second CColModel.
#ifdef NOTSA_GLOBALS_DETACHED
extern CColModel col1;         // defined at the end of Game.cpp (both types are incomplete here)
extern CCollisionData col1Data;
#else
static inline auto& col1     = StaticRef<CColModel>(0xC17824);
static inline auto& col1Data = StaticRef<CCollisionData>(0xC17854);
#endif
#line 65

bool MoveMem(void **pMem);
bool MoveColModelMemory(CColModel &colModel, bool a2);
RpGeometry *MoveGeometryMemory(RpGeometry *geometry);
bool TidyUpModelInfo2(CEntity *entity, bool a2);
void ValidateVersion();
void D3DDeviceRestoreCallback();
bool DoHaspChecks();
