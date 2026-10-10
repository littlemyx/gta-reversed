/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "RenderWare.h"
#include <extensions/utility.hpp>

class CEntity;
class CObject;
class CDirectory;
class CCutsceneObject;
class CAnimBlendAssocGroup;

struct tCutsceneParticleEffect {
    char        m_szEffectName[32];
    FxSystem_c* m_pFxSystem;
    int32       m_nStartTime;
    int32       m_nEndTime;
    int32       m_nObjectId;
    char        m_szObjectPart[32];
    CVector     m_vecPosn;
    CVector     m_vecDirection;
    bool        m_bPlaying;
    bool        m_bStopped;
};
VALIDATE_SIZE(tCutsceneParticleEffect, 0x6C);

struct tCutsceneAttachment {
    int32 m_nCutscenePedObjectId;
    int32 m_nCutsceneAttachmentObjectId;
    int32 m_nBoneId;
};

struct tCutsceneRemoval {
    CVector m_vecPosn;
    char    m_szObjectName[32];
};

extern uint32 MAX_NUM_CUTSCENE_OBJECTS; // default: 50
extern uint32 MAX_NUM_CUTSCENE_PARTICLE_EFFECTS; // default: 8
extern uint32 MAX_NUM_CUTSCENE_ITEMS_TO_HIDE; // default: 50
extern uint32 MAX_NUM_CUTSCENE_ATTACHMENTS; // default: 50


class CCutsceneMgr {
public:
    enum class LoadStatus {
        NOT_LOADED,
        LOADING,
        LOADED,
    };

    enum class PlayStatus {
        S0,
        STARTING = 1,
        S2,
        S3,
        S4
    };

public:
    static inline NOTSA_GLOBAL(ms_useCutsceneShadows, 0x8AC158, (int8), { 1 });
    static inline NOTSA_GLOBAL(numPlayerWeaponsToRestore, 0xB5EB58, (int32), {});

    static inline NOTSA_GLOBAL(playerWeaponsToRestore_Ammo, 0xB5EB5C, (std::array<int32, 13>), {}); // TODO: Where does the 13 number come from? 
    static inline NOTSA_GLOBAL(playerWeaponsToRestore_Type, 0xB5EB90, (std::array<int32, 13>), {}); // TODO: Where does the 13 number come from?

    static inline NOTSA_GLOBAL(ms_cAppendAnimName, 0xB5EBC8, (std::array<char[32], 50>), {});
    static inline NOTSA_GLOBAL(ms_cAppendObjectName, 0xB5F208, (std::array<char[32], 50>), {});

    static inline NOTSA_GLOBAL(ms_pCutsceneDir, 0xB5F848, (CDirectory*), {});
    static inline NOTSA_GLOBAL(ms_cutsceneLoadStatus, 0xB5F84C, (LoadStatus), {});
    static inline NOTSA_GLOBAL(ms_running, 0xB5F851, (int8), {});
    static inline NOTSA_GLOBAL(ms_cutsceneProcessing, 0xB5F852, (bool), {});
    static inline NOTSA_GLOBAL(ms_useLodMultiplier, 0xB5F853, (bool), {});
    static inline NOTSA_GLOBAL(ms_wasCutsceneSkipped, 0xB5F854, (bool), {});
    static inline NOTSA_GLOBAL(ms_hasFileInfo, 0xB5F855, (int8), {});
    static inline NOTSA_GLOBAL(ms_numAppendObjectNames, 0xB5F858, (int32), {});
    static inline NOTSA_GLOBAL(restoreEverythingAfterCutscene, 0xB5F85C, (bool), {});
    static inline NOTSA_GLOBAL(m_fPrevCarDensity, 0xBC1D68, (float), {});
    static inline NOTSA_GLOBAL(m_fPrevPedDensity, 0xBC1D6C, (float), {});
    static inline NOTSA_GLOBAL(ms_pParticleEffects, 0xBC1D70, (std::array<tCutsceneParticleEffect, 8>), {});
    static inline NOTSA_GLOBAL(ms_crToHideItems, 0xBC20D0, (std::array<tCutsceneRemoval, 50>), {});
    static inline NOTSA_GLOBAL(ms_pHiddenEntities, 0xBC2968, (std::array<CEntity*, 50>), {});
    static inline NOTSA_GLOBAL(ms_numAttachObjectToBones, 0xBC2A30, (int32), {});
    static inline NOTSA_GLOBAL(ms_bRepeatObject, 0xBC2A34, (std::array<char, 50>), {});
    static inline NOTSA_GLOBAL(ms_iAttachObjectToBone, 0xBC2A68, (std::array<tCutsceneAttachment, 50>), {});
    static inline NOTSA_GLOBAL(ms_aUncompressedCutsceneAnims, 0xBC2CC0, (std::array<char[32], 8>), {}); 
    static inline NOTSA_GLOBAL(ms_iTextDuration, 0xBC2DC0, (std::array<int32, 64>), {});
    static inline NOTSA_GLOBAL(ms_iTextStartTime, 0xBC2EC0, (std::array<int32, 64>), {});
    static inline NOTSA_GLOBAL(ms_cTextOutput, 0xBC2FC0, (std::array<char[8], 64>), {});
    static inline NOTSA_GLOBAL(ms_iModelIndex, 0xBC31C0, (std::array<eModelID, 50>), {});
    static inline NOTSA_GLOBAL(ms_cLoadAnimName, 0xBC3288, (std::array<char[32], 50>), {});
    static inline NOTSA_GLOBAL(ms_cLoadObjectName, 0xBC38C8, (std::array<char[32], 50>), {});
    static inline NOTSA_GLOBAL(ms_cutsceneTimerS, 0xBC3F08, (float), {}); // In seconds
    static inline NOTSA_GLOBAL(ms_cutsceneName, 0xBC3F0C, (char[8]), {});
    static inline NOTSA_GLOBAL(ms_pCutsceneObjects, 0xBC3F18, (std::array<CCutsceneObject*, 50>), {});
    static inline NOTSA_GLOBAL(ms_cutscenePlayStatus, 0xBC3FE0, (PlayStatus), {});
    static inline NOTSA_GLOBAL(ms_numCutsceneObjs, 0xBC3FE4, (int32), {});
    static inline NOTSA_GLOBAL(ms_numLoadObjectNames, 0xBC3FE8, (int32), {});
    static inline NOTSA_GLOBAL(ms_numTextOutput, 0xBC3FEC, (int32), {});
    static inline NOTSA_GLOBAL(ms_currTextOutput, 0xBC3FF0, (int32), {});
    static inline NOTSA_GLOBAL(ms_numUncompressedCutsceneAnims, 0xBC3FF4, (uint32), {});
    static inline NOTSA_GLOBAL(ms_iNumHiddenEntities, 0xBC3FF8, (uint32), {});
    static inline NOTSA_GLOBAL(ms_iNumParticleEffects, 0xBC3FFC, (uint32), {});
    static inline NOTSA_GLOBAL(m_PrevExtraColour, 0xBC4000, (int32), {});
    static inline NOTSA_GLOBAL(m_PrevExtraColourOn, 0xBC4004, (bool), {});
    static inline NOTSA_GLOBAL(m_bDontClearZone, 0xBC4005, (bool), {});

    //! If the camera splines were loaded (See `LoadCutsceneData_postload`)
    //! from the .DAT file of the cutscene found in CUTS.IMG
    static inline NOTSA_GLOBAL(dataFileLoaded, 0xBC4006, (bool), {});

    //! If the anims were loaded (From the cutscene's .IFP file found in CUTS.IMG)
    static inline NOTSA_GLOBAL(ms_animLoaded, 0xB5F850, (bool), {});

    static inline auto& ms_cutsceneAssociations = StaticRef<CAnimBlendAssocGroup>(0xBC4020);
    static inline NOTSA_GLOBAL(ms_cutsceneOffset, 0xBC4034, (CVector), {});

    static void InjectHooks();

    static int32 AddCutsceneHead(CObject* object, int32 arg1);
    static void AppendToNextCutscene(const char* objectName, const char* animName);
    static void AttachObjectToBone(CCutsceneObject* attachment, CCutsceneObject* object, int32 boneId);
    static void AttachObjectToFrame(CCutsceneObject* attachment, CEntity* object, const char* frameName);
    static void AttachObjectToParent(CCutsceneObject* attachment, CEntity* object);
    static void BuildCutscenePlayer();
    static void UpdateCutsceneObjectBoundingBox(RpClump* clump, eModelID modellId);
    static CCutsceneObject* CreateCutsceneObject(eModelID modelId);
    static void DeleteCutsceneData();
    static void DeleteCutsceneData_overlay();
    static void FinishCutscene();
    static uint64 GetCutsceneTimeInMilleseconds();
    static bool HasCutsceneFinished();
    static void HideRequestedObjects();
    static void Initialise();
    static bool IsCutsceneSkipButtonBeingPressed();
    static void LoadAnimationUncompressed(const char* animName);
    static void LoadCutsceneData(const char* cutsceneName);
    static void LoadCutsceneData_loading();
    static void LoadCutsceneData_overlay(const char* cutsceneName);
    static void LoadCutsceneData_postload();
    static bool LoadCutSceneFile(const char* csFileName);
    static void LoadCutsceneData_preload();
    static void LoadEverythingBecauseCutsceneDeletedAllOfIt();
    static void RemoveCutscenePlayer();
    static void RemoveEverythingBecauseCutsceneDoesntFitInMemory();
    static void SetCutsceneAnim(const char* animName, CObject* object);
    static void SetCutsceneAnimToLoop(const char* animName);
    static void SetHeadAnim(const char* animName, CObject* headObject);
    static void SetupCutsceneToStart();
    static void Shutdown();
    static void SkipCutscene();
    static void StartCutscene();
    static void Update();
    static void Update_overlay();


    static bool IsRunning() { return ms_running; }
    static bool IsCutsceneProcessing() { return ms_cutsceneProcessing; }
    static bool HasLoaded() { return ms_cutsceneLoadStatus == LoadStatus::LOADED; }
    static bool IsLoading() { return ms_cutsceneLoadStatus == LoadStatus::LOADING; }
    static bool IsPlayingCSTheFinale();
};

int16 FindCutsceneAudioTrackId(const char* cutsceneName);
void UpdateCutsceneObjectBoundingBox(RpClump* clump, int32 modelId);
