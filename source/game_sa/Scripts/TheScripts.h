/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Text/Text.h"
#include "ExeZero.h"
#line 10
#include "Ped.h"
#include "Vehicle.h"
#include "Object.h"
#include "MissionCleanup.h"
#include "Building.h"
#include "Sprite2d.h"
#include "StreamedScripts.h"
#include "ScriptResourceManager.h"
#include "StuckCarCheck.h"
#include "UpsideDownCarCheck.h"
#include "ScriptsForBrains.h"
#include "ScriptParam.h"
#include "Font.h"

#include "SCMChunks.hpp"

class CRunningScript;
class CCheckpoint;
enum class eCheckpointType : uint32;

enum class eCrossHairType : uint32 {
    NONE,
    FIXED_DRAW_CIRCLE,
    FIXED_DRAW_1STPERSON_WEAPON,
};

enum eScriptThingType : uint8 {
    SCRIPT_THING_SPHERE         = 0,
    SCRIPT_THING_EFFECT_SYSTEM  = 1,
    SCRIPT_THING_SEARCH_LIGHT   = 2,
    SCRIPT_THING_CHECKPOINT     = 3,
    SCRIPT_THING_SEQUENCE_TASK  = 4,
    SCRIPT_THING_FIRE           = 5,
    SCRIPT_THING_2D_EFFECT      = 6,
    SCRIPT_THING_DECISION_MAKER = 7,
    SCRIPT_THING_PED_GROUP      = 8
};

struct tBuildingSwap {
    CBuilding* m_pCBuilding;
    int32      m_nNewModelIndex;
    int32      m_nOldModelIndex;

    tBuildingSwap() { // 0x469270
        Clear();
    }

    tBuildingSwap(CBuilding* building, int32 nNewModelIndex, int32 nOldModelIndex) {
        m_pCBuilding     = building;
        m_nNewModelIndex = nNewModelIndex;
        m_nOldModelIndex = nOldModelIndex;
    }

    void Clear() { // todo: +-
        m_pCBuilding     = nullptr;
        m_nNewModelIndex = -1;
        m_nOldModelIndex = -1;
    }
};

struct tScriptSwitchCase {
    int32 m_nSwitchValue;
    int32 m_nSwitchLabelAddress;
};
VALIDATE_SIZE(tScriptSwitchCase, 0x8);

struct tScriptCheckpoint {
    bool         m_bUsed;
    char         m_field_1;
    int16        m_nId;
    CCheckpoint* m_Checkpoint;

    tScriptCheckpoint() { // 0x469334
        m_bUsed      = false;
        m_nId        = 1;
        m_Checkpoint = nullptr;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};
VALIDATE_SIZE(tScriptCheckpoint, 0x8);

struct tScriptEffectSystem {
    bool        m_bUsed;
    int16       m_nId;
    FxSystem_c* m_pFxSystem;

    tScriptEffectSystem() {
        m_bUsed     = false;
        m_nId       = 1;
        m_pFxSystem = nullptr;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};
VALIDATE_SIZE(tScriptEffectSystem, 0x8);

struct tScriptSequence {
    bool  m_bUsed;
    int16 m_nId;

    tScriptSequence() {
        m_bUsed = false;
        m_nId   = 1;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};
VALIDATE_SIZE(tScriptSequence, 0x4);

struct tScriptText {
    // Values from 0x4690A8 (the defaults CTheScripts::Process stores; see WrapX/CentreSize for the two that differ in Init)
    CVector2D     Scale{ 0.48f, 1.12f };
    CRGBA         Color{ 225, 225, 225, 255 };
    bool          Justify{ false };
    bool          IsCentered{ false }; //!< `HasRightJustify` takes precedence over it. If true, uses `eFontAlignment::ALIGN_CENTER` otherwise `eFontAlignment::ALIGN_LEFT`.
    bool          HasBg{ false };
    bool          HasBgTextOnly{ false }; //!< Unused
    float         WrapX{ 182.0f };      //!< exe 0x46A0A0 (CTheScripts::Process refill): immediate 0x43360000. CTheScripts::Init (0x4690B7) stores `RsGlobal.maximumWidth` instead
    float         CentreSize{ 640.0f }; //!< exe 0x46A0A0: immediate 0x44200000 (Init: `RsGlobal.maximumWidth`)
    CRGBA         BgColor{ 128, 128, 128, 128 };
    bool          IsProportional{ true };
    CRGBA         DropShadowColor{ 0, 0, 0, 255 };
    int8          DropShadow{ 2 };
    int8          TextEdge{ 0 };
    bool          IsDrawBeforeFade{ false };
    bool          HasRightJustify{ false }; //!< Takes priority over `IsCentered`. If true `eFontAlignment::ALIGN_RIGHT` is used.
    eFontStyleS32 FontStyle{ FONT_SUBTITLES };
    CVector2D     Pos{};
    char          GXTKey[8]{};
    int32         NumberToInsert1{ -1 };
    int32         NumberToInsert2{ -1 };
};
VALIDATE_SIZE(tScriptText, 0x44);

enum class eScriptRectangleType : int32 {
    INACTIVE,           //!< This entry is inactive (Not drawn)
    TITLE_AND_MESSAGE,
    TEXT,
    MONOCOLOR,          //!< Mono-color rect
    TEXTURED,           //!< Textured rect (basically a sprite)
    MONOCOLOR_ANGLED,   //!< Angled mono-color rect
};

struct tScriptRectangle {
    eScriptRectangleType m_nType;
    bool                 m_bDrawBeforeFade;
    char                 field_5;
    int16                m_nTextureId;
    CVector2D            cornerA; //!< Supposed to be: Top left corner (min x, y) - Sometimes this isn't the case though, for example in scripted videogames...
    CVector2D            cornerB; //!< Supposed to be: Bottom right corner (max x, y) - Sometimes this isn't the case though, for example in scripted videogames...
    float                m_nAngle;
    CRGBA                m_nTransparentColor;
    char                 gxt1[8];
    int16                field_28;
    char                 gxt2[8];
    int16                field_32;
    eFontAlignment       m_Alignment;
    uint32               m_nTextboxStyle;

    tScriptRectangle() { // 0x4691C8
        m_nType             = eScriptRectangleType::INACTIVE;
        m_bDrawBeforeFade   = false;
        m_nTextureId        = -1;
        cornerA             = CVector2D();
        cornerB             = CVector2D();
        m_nAngle            = 0;
        m_nTransparentColor = CRGBA(255, 255, 255, 255);
        gxt1[0]             = '\0';
        m_nTextboxStyle     = 3;
    }
};
VALIDATE_SIZE(tScriptRectangle, 0x3C);

struct tScriptAttachedAnimGroup {
    int32 m_nModelID;
    char  m_IfpName[16];

    tScriptAttachedAnimGroup() {
        m_nModelID = -1;
        m_IfpName[0] = 0;
    }
};

enum class eScriptSearchLightState : uint8 {
    STATE_0,
    STATE_1,
    STATE_2,
    STATE_3,
    STATE_4,
};

struct tScriptSearchlight {
    bool                    m_bUsed{};
    bool                    m_bClipIfColliding{};
    bool                    m_bEnableShadow{};
    eScriptSearchLightState m_nCurrentState : 7{};
    bool                    m_SomethingFlag : 1{};
    int16                   m_nId{};
    CVector                 m_Origin{};
    CVector                 m_Target{};
    float                   m_fTargetRadius{};
    float                   m_fBaseRadius{};
    CVector                 m_PathCoord1{};
    CVector                 m_PathCoord2{};
    float                   m_fPathSpeed{};
    notsa::EntityRef<>      m_AttachedEntity{};
    notsa::EntityRef<>      m_FollowingEntity{};
    notsa::EntityRef<>      m_Tower{};
    notsa::EntityRef<>      m_Housing{};
    notsa::EntityRef<>      m_Bulb{};
    CVector                 m_TargetSpot{};
    CVector                 vf64{};
    CVector                 vf70{};

    //! Script thing ID
    auto GetId() { return m_nId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};
VALIDATE_SIZE(tScriptSearchlight, 0x7C);

struct tUsedObject {
    char  szModelName[24];
    int32 nModelIndex;

    tUsedObject() = default; // 0x468F20
};
VALIDATE_SIZE(tUsedObject, 0x1C);

struct tScriptSphere {
    bool    m_bUsed;
    char    m_f1;
    int16   m_nUniqueId;
    uint32  m_nId;
    CVector m_vCoords;
    float   m_fRadius;

    tScriptSphere() { // 0x469060
        m_vCoords   = CVector();
        m_bUsed     = false;
        m_nUniqueId = 1;
        m_nId       = 0;
        m_fRadius   = 0.0f;
    }

    //! Get script thing ID
    auto GetId()    const { return m_nUniqueId; }

    //! If `*this` is currently in use
    auto IsActive() const { return m_bUsed; }
};
VALIDATE_SIZE(tScriptSphere, 0x18);

struct tStoredLine {
    CVector vecInf;
    CVector vecSup;
    uint32  color1;
    uint32  color2;
};
VALIDATE_SIZE(tStoredLine, 0x20);

struct tScriptBrainWaitEntity {
    notsa::EntityRef<> m_pEntity{};
    int16              m_ScriptBrainIndex{ -1 };
    int16              field_6{};

    tScriptBrainWaitEntity() = default; // 0x468E12

    // NOTSA
    void Clear() {
        m_pEntity = nullptr;
        m_ScriptBrainIndex = -1;
    }
};
VALIDATE_SIZE(tScriptBrainWaitEntity, 0x8);

struct tScriptConnectLodsObject {
    int32 a;
    int32 b;

    tScriptConnectLodsObject() {
        a = -1;
        b = -1;
    }
};
VALIDATE_SIZE(tScriptBrainWaitEntity, 0x8);

enum {
    MAX_NUM_SCRIPTS                             = 96,
    MAX_NUM_SCRIPT_SPRITES                      = 128,
    MAX_NUM_SCRIPT_SPHERES                      = 16,
    MAX_NUM_SCRIPT_RECTANGLES                   = 128,
    MAX_NUM_SCRIPT_SEARCH_LIGHT                 = 8,
    MAX_NUM_SCRIPT_SEQUENCE_TASKS               = 64,
    MAX_NUM_SCRIPT_CHECKPOINTS                  = 20,
    MAX_NUM_SCRIPT_EFFECT_SYSTEMS               = 32,
    MAX_NUM_SCRIPT_CONNECT_LODS_OBJECTS         = 10,
    MAX_NUM_SCRIPT_ATTACHED_ANIM_GROUPS         = 8,
    MAX_NUM_ENTITIES_WAITING_FOR_SCRIPT_BRAIN   = 150,
    MAX_NUM_VEHICLE_MODELS_BLOCKED_BY_SCRIPT    = 20,
    MAX_NUM_MISSION_SCRIPTS                     = 200,
    MAX_NUM_LOCAL_VARIABLES_FOR_CURRENT_MISSION = 1024,
    MAX_NUM_USED_OBJECTS                        = 395,
    MAX_NUM_BUILDING_SWAPS                      = 25,
    MAX_NUM_INVISIBILITY_SETTINGS               = 20,
    MAX_NUM_INTRO_TEXT_LINES                    = 96,
    MAX_NUM_STORED_LINES                        = 1024,
    MAX_NUM_SwitchJumpTable                     = 75,
    MAX_NUM_CARDS                               = 312,
    MAX_NUM_SUPPRESSED_VEHICLE_MODELS           = 40,
};

enum class ScriptSavedObjectType : uint32 {
    NONE = 0,
    INVISIBLE = 1, // ?
    BUILDING = 2,
    OBJECT = 3,
    DUMMY = 4,
};

static constexpr uint32 SCRIPT_VAR_TIMERA = 32, SCRIPT_VAR_TIMERB = 33;
static constexpr uint32 MISSION_SCRIPT_SIZE = 69000;

NOTSA_GLOBAL_HDR(gAllowScriptedFixedCameraCollision, 0xB6EC2C, (bool), {}); // The original keeps it in .bss (read by CCam::Process_Fixed 0x51D6D9, reset by CMissionCleanup, set by ALLOW_FIXED_CAMERA_COLLISION)

class CTheScripts {
public:
    static constexpr uint32 MAIN_SCRIPT_SIZE         = 200'000;
    static constexpr uint32 SCRIPT_SPACE_SIZE        = MAIN_SCRIPT_SIZE + MISSION_SCRIPT_SIZE;
    static constexpr uint32 MAX_SAVED_GVAR_PART_SIZE = 51'200;

    //! Lower `MAIN_SCRIPT_SIZE` is where MAIN.SCM is, remaining `MISSION_SCRIPT_SIZE` is for other loaded scripts.
    // static inline auto& ScriptSpace = StaticRef<uint8[SCRIPT_SPACE_SIZE]>(0xA49960);
    static inline NOTSA_GLOBAL(ScriptSpace, 0xA49960, (std::array<uint8, SCRIPT_SPACE_SIZE>), {});

    //! Reference to ScriptSpace's lower portion for MAIN.SCM - Prefer this over `&ScriptSpace[0]`
    static inline std::span<uint8, MAIN_SCRIPT_SIZE> MainSCMBlock{ ScriptSpace.data() + 0, MAIN_SCRIPT_SIZE };

    //! Reference to ScriptSpace's upper portion for other scripts - Prefer this over `&ScriptSpace[MAIN_SCRIPT_SIZE]`
    static inline std::span<uint8, MISSION_SCRIPT_SIZE> MissionBlock{ ScriptSpace.data() + MainSCMBlock.size(), MISSION_SCRIPT_SIZE };

    static inline NOTSA_GLOBAL(SwitchJumpTable, 0xA43CF8, (std::array<tScriptSwitchCase, MAX_NUM_SwitchJumpTable>), {});
    static inline NOTSA_GLOBAL(NumberOfEntriesInSwitchTable, 0xA43F50, (uint16), {});
    static inline NOTSA_GLOBAL(NumberOfEntriesStillToReadForSwitch, 0xA43F60, (uint16), {});

    static inline NOTSA_GLOBAL(CardStack, 0xA44218, (std::array<int16, MAX_NUM_CARDS>), {});
    static inline NOTSA_GLOBAL(MultiScriptArray, 0xA444C8, (std::array<int32, MAX_NUM_MISSION_SCRIPTS>), {});
    static inline NOTSA_GLOBAL(ScriptConnectLodsObjects, 0xA44800, (std::array<tScriptConnectLodsObject, MAX_NUM_SCRIPT_CONNECT_LODS_OBJECTS>), = ExeZero<std::array<tScriptConnectLodsObject, MAX_NUM_SCRIPT_CONNECT_LODS_OBJECTS>>()); static_assert(sizeof(ScriptConnectLodsObjects) == 80); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(ScriptAttachedAnimGroups, 0xA44850, (std::array<tScriptAttachedAnimGroup, MAX_NUM_SCRIPT_ATTACHED_ANIM_GROUPS>), = ExeZero<std::array<tScriptAttachedAnimGroup, MAX_NUM_SCRIPT_ATTACHED_ANIM_GROUPS>>()); static_assert(sizeof(ScriptAttachedAnimGroups) == 160); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(VehicleModelsBlockedByScript, 0xA448F0, (std::array<eModelID, MAX_NUM_VEHICLE_MODELS_BLOCKED_BY_SCRIPT>), {});
    static inline NOTSA_GLOBAL(SuppressedVehicleModels, 0xA44940, (std::array<eModelID, MAX_NUM_SUPPRESSED_VEHICLE_MODELS>), {});
    static inline NOTSA_GLOBAL(InvisibilitySettingArray, 0xA449E0, (std::array<CEntity*, MAX_NUM_INVISIBILITY_SETTINGS>), {});

    static inline NOTSA_GLOBAL(LocalVariablesForCurrentMission, 0xA48960, (std::array<tScriptParam, MAX_NUM_LOCAL_VARIABLES_FOR_CURRENT_MISSION>), {});
    static inline NOTSA_GLOBAL(LargestNumberOfMissionScriptLocalVariables, 0xA444B4, (uint32), {});

    static inline NOTSA_GLOBAL(BuildingSwapArray, 0xA44A30, (std::array<tBuildingSwap, MAX_NUM_BUILDING_SWAPS>), = ExeZero<std::array<tBuildingSwap, MAX_NUM_BUILDING_SWAPS>>()); VALIDATE_SIZE(BuildingSwapArray, 300); // zero .bss in the exe (not the repo NSDMI defaults)

    static inline NOTSA_GLOBAL(UsedObjectArray, 0xA44B70, (std::array<tUsedObject, MAX_NUM_USED_OBJECTS>), {});
    static inline NOTSA_GLOBAL(NumberOfUsedObjects, 0xA44B6C, (uint16), {});

    static inline NOTSA_GLOBAL(EntitiesWaitingForScriptBrain, 0xA476B0, (std::array<tScriptBrainWaitEntity, MAX_NUM_ENTITIES_WAITING_FOR_SCRIPT_BRAIN>), = [] { std::array<tScriptBrainWaitEntity, MAX_NUM_ENTITIES_WAITING_FOR_SCRIPT_BRAIN> a{}; for (auto& e : a) { e.m_ScriptBrainIndex = 0; } return a; }()); VALIDATE_SIZE(EntitiesWaitingForScriptBrain, 1200); // zero .bss in the exe (the repo NSDMI is -1; EntityRef is not trivially copyable, so no ExeZero)
    static inline NOTSA_GLOBAL(IntroTextLines, 0xA913E8, (std::array<tScriptText, MAX_NUM_INTRO_TEXT_LINES>), = ExeZero<std::array<tScriptText, MAX_NUM_INTRO_TEXT_LINES>>()); static_assert(sizeof(IntroTextLines) == 6528); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(NumberOfIntroTextLinesThisFrame, 0xA44B68, (uint16), {});

    static inline NOTSA_GLOBAL(IntroRectangles, 0xA92D68, (std::array<tScriptRectangle, MAX_NUM_SCRIPT_RECTANGLES>), = ExeZero<std::array<tScriptRectangle, MAX_NUM_SCRIPT_RECTANGLES>>()); static_assert(sizeof(IntroRectangles) == 7680); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(NumberOfIntroRectanglesThisFrame, 0xA44B5C, (uint16), {});

    static inline NOTSA_GLOBAL(ScriptSprites, 0xA94B68, (std::array<CSprite2d, MAX_NUM_SCRIPT_SPRITES>), {}); VALIDATE_SIZE(ScriptSprites, 512);

    static inline NOTSA_GLOBAL(NumberOfExclusiveMissionScripts, 0xA444B8, (uint16), {});
    static inline NOTSA_GLOBAL(NumberOfMissionScripts, 0xA444BC, (uint16), {});

    //
    // Script things
    //

    static inline NOTSA_GLOBAL(ScriptSphereArray, 0xA91268, (std::array<tScriptSphere, MAX_NUM_SCRIPT_SPHERES>), = ExeZero<std::array<tScriptSphere, MAX_NUM_SCRIPT_SPHERES>>()); static_assert(sizeof(ScriptSphereArray) == 384); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(ScriptEffectSystemArray, 0xA44110, (std::array<tScriptEffectSystem, MAX_NUM_SCRIPT_EFFECT_SYSTEMS>), = ExeZero<std::array<tScriptEffectSystem, MAX_NUM_SCRIPT_EFFECT_SYSTEMS>>()); VALIDATE_SIZE(ScriptEffectSystemArray, 256); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(ScriptSearchLightArray, 0xA94D68, (std::array<tScriptSearchlight, MAX_NUM_SCRIPT_SEARCH_LIGHT>), {});

    static inline NOTSA_GLOBAL(ScriptSequenceTaskArray, 0xA43F68, (std::array<tScriptSequence, MAX_NUM_SCRIPT_SEQUENCE_TASKS>), = ExeZero<std::array<tScriptSequence, MAX_NUM_SCRIPT_SEQUENCE_TASKS>>()); static_assert(sizeof(ScriptSequenceTaskArray) == 256); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(NumberOfScriptSearchLights, 0xA90830, (uint16), {});

    static inline NOTSA_GLOBAL(ScriptCheckpointArray, 0xA44070, (std::array<tScriptCheckpoint, MAX_NUM_SCRIPT_CHECKPOINTS>), = ExeZero<std::array<tScriptCheckpoint, MAX_NUM_SCRIPT_CHECKPOINTS>>()); VALIDATE_SIZE(ScriptCheckpointArray, 160); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(NumberOfScriptCheckpoints, 0xA44068, (uint16), {});
    static inline NOTSA_GLOBAL(UnknownDebugStuff, 0xA95190, (uint32), {}); // used in DO_DEBUG_STUFF command

    enum class eUseTextCommandState : uint8 {
        DISABLED,
        DISABLE_NEXT_FRAME,
        ENABLED_BY_SCRIPT
    };
    static inline NOTSA_GLOBAL(UseTextCommands, 0xA44B67, (eUseTextCommandState), {});
    static inline NOTSA_GLOBAL(DbgFlag, 0x859CF8, (bool), {});
    static inline NOTSA_GLOBAL(SwitchDefaultAddress, 0xA43F54, (int32), {});
    static inline NOTSA_GLOBAL(SwitchDefaultExists, 0xA43F58, (bool), {});
    static inline NOTSA_GLOBAL(ValueToCheckInSwitchStatement, 0xA43F5C, (int32), {});
    static inline NOTSA_GLOBAL(CardStackPosition, 0xA44210, (int16), {});
    static inline NOTSA_GLOBAL(bDrawSubtitlesBeforeFade, 0xA44488, (bool), {});
    static inline NOTSA_GLOBAL(bDrawOddJobTitleBeforeFade, 0xA44489, (bool), {});
    static inline NOTSA_GLOBAL(bScriptHasFadedOut, 0xA4448A, (bool), {});
    static inline NOTSA_GLOBAL(bAddNextMessageToPreviousBriefs, 0xA4448B, (bool), {});
    static inline NOTSA_GLOBAL(ForceRandomCarModel, 0xA4448C, (int32), {});
    static inline NOTSA_GLOBAL(bDrawCrossHair, 0xA44490, (eCrossHairType), {});
    static inline NOTSA_GLOBAL(bEnableCraneRelease, 0xA44494, (bool), {});
    static inline NOTSA_GLOBAL(bEnableCraneLower, 0xA44495, (bool), {});
    static inline NOTSA_GLOBAL(bEnableCraneRaise, 0xA44496, (bool), {});
    static inline NOTSA_GLOBAL(fCameraHeadingStepWhenPlayerIsAttached, 0xA44498, (float), {});
    static inline NOTSA_GLOBAL(fCameraHeadingWhenPlayerIsAttached, 0xA4449C, (float), {});
    static inline NOTSA_GLOBAL(bDisplayHud, 0xA444A0, (bool), {});
    static inline NOTSA_GLOBAL(HideAllFrontEndMapBlips, 0xA444A1, (bool), {});
    static inline NOTSA_GLOBAL(RadarShowBlipOnAllLevels, 0xA444A2, (bool), {});
    static inline NOTSA_GLOBAL(RadarZoomValue, 0xA444A3, (uint8), {});
    static inline NOTSA_GLOBAL(bPlayerIsOffTheMap, 0xA444A4, (bool), {});
    static inline NOTSA_GLOBAL(RiotIntensity, 0xA444A5, (char), {});
    static inline NOTSA_GLOBAL(bPlayerHasMetDebbieHarry, 0xA444A6, (bool), {});
    static inline NOTSA_GLOBAL(bDisplayNonMiniGameHelpMessages, 0xA444A7, (bool), {});
    static inline NOTSA_GLOBAL(bMiniGameInProgress, 0xA444A8, (bool), {});
    static inline NOTSA_GLOBAL(ScriptPickupCycleIndex, 0xA444AC, (int32), {});
    static inline NOTSA_GLOBAL(FailCurrentMission, 0xA444B0, (int8), {});
    static inline NOTSA_GLOBAL(bAlreadyRunningAMissionScript, 0xA444B1, (bool), {});
    static inline NOTSA_GLOBAL(LargestMissionScriptSize, 0xA444C0, (uint32), {});
    static inline NOTSA_GLOBAL(MainScriptSize, 0xA444C4, (uint32), {});
    static inline NOTSA_GLOBAL(bUsingAMultiScriptFile, 0xA447E8, (bool), {});
    static inline NOTSA_GLOBAL(StoreVehicleIndex, 0xA447EC, (int32), {});
    static inline NOTSA_GLOBAL(StoreVehicleWasRandom, 0xA447F0, (bool), {});
    static inline NOTSA_GLOBAL(CommandsExecuted, 0xA447F4, (uint16), {});
    static inline NOTSA_GLOBAL(ScriptsUpdated, 0xA447F8, (uint16), {});
    static inline NOTSA_GLOBAL(MessageWidth, 0xA44B60, (uint16), {});
    static inline NOTSA_GLOBAL(MessageCentre, 0xA44B64, (uint16), {});
    static inline NOTSA_GLOBAL(bUseMessageFormatting, 0xA44B66, (bool), {});
    static inline NOTSA_GLOBAL(LastRandomPedId, 0xA476A4, (int32), {});
    static inline NOTSA_GLOBAL(LastMissionPassedTime, 0xA476A8, (uint32), {});
    static inline NOTSA_GLOBAL(OnAMissionFlag, 0xA476AC, (int32), {}); // Refers to the offset of OM flag in script space.
    static inline NOTSA_GLOBAL(StreamedScripts, 0xA47B60, (CStreamedScripts), = ExeZero<CStreamedScripts>()); VALIDATE_SIZE(StreamedScripts, 2632); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(ScriptResourceManager, 0xA485A8, (CScriptResourceManager), = ExeZero<CScriptResourceManager>()); VALIDATE_SIZE(ScriptResourceManager, 900); // all-zero like the exe image (its ctor is deleted, tScriptResource would give -1 ids: CScriptResourceManager::Initialise() fills them at game init)
    static inline NOTSA_GLOBAL(UpsideDownCars, 0xA4892C, (CUpsideDownCarCheck), = ExeZero<CUpsideDownCarCheck>()); static_assert(sizeof(UpsideDownCars) == 48); // zero .bss in the exe (not the repo NSDMI defaults)
    static inline NOTSA_GLOBAL(pIdleScripts, 0xA8B428, (CRunningScript*), {});
    static inline NOTSA_GLOBAL(pActiveScripts, 0xA8B42C, (CRunningScript*), {});
    static inline NOTSA_GLOBAL(MissionCleanUp, 0xA90850, (CMissionCleanup), {});
    static inline NOTSA_GLOBAL(StuckCars, 0xA90AB0, (CStuckCarCheck), {});
    static inline NOTSA_GLOBAL(ScriptsForBrains, 0xA90CF0, (CScriptsForBrains), {}); static_assert(sizeof(ScriptsForBrains) == 1400);

public:
    static void InjectHooks();

    static void Init();
    static void InitialiseAllConnectLodObjects();
    static void InitialiseConnectLodObjects(uint16 index);
    static void InitialiseSpecialAnimGroup(uint16 index);
    static void InitialiseSpecialAnimGroupsAttachedToCharModels();
    static void ReadObjectNamesFromScript();
    static void UpdateObjectIndices();
    static void ReadMultiScriptFileOffsetsFromScript();

    static uint32 AddScriptCheckpoint(CVector at, CVector pointTo, float radius, eCheckpointType type);
    static uint32 AddScriptEffectSystem(FxSystem_c* system);
    static uint32 AddScriptSearchLight(CVector start, CEntity* entity, CVector target, float targetRadius, float baseRadius);
    static uint32 AddScriptSphere(uint32 id, CVector posn, float radius);

    static void AddToBuildingSwapArray(CBuilding* building, int32 oldModelId, int32 newModelId);
    static void AddToInvisibilitySwapArray(CEntity* entity, bool bVisible);
    static void AddToListOfConnectedLodObjects(CObject* obj1, CObject* obj2);
    static void AddToListOfSpecialAnimGroupsAttachedToCharModels(int32 modelId, Const char* ifpName);
    static void AddToSwitchJumpTable(int32 switchValue, int32 switchLabelLocalAddress);
    static void AddToVehicleModelsBlockedByScript(eModelID modelIndex);
    static void AddToWaitingForScriptBrainArray(CEntity* entity, int16 arg2);

    static void AttachSearchlightToSearchlightObject(int32 searchLightId, CObject* tower, CObject* housing, CObject* bulb, CVector offset);
    static bool CheckStreamedScriptVersion(RwStream* stream, const char* filename);
    static void CleanUpThisObject(CObject* obj);
    static void CleanUpThisPed(CPed* ped);
    static void CleanUpThisVehicle(CVehicle* vehicle);
    static void ClearAllSuppressedCarModels();
    static void ClearAllVehicleModelsBlockedByScript();
    static void ClearSpaceForMissionEntity(const CVector& pos, CEntity* entity);
    static void DoScriptSetupAfterPoolsHaveLoaded();

    static int32 GetActualScriptThingIndex(int32 ref, eScriptThingType type);
    static int32 GetNewUniqueScriptThingIndex(int32 index, eScriptThingType type);
    static int32 GetScriptIndexFromPointer(CRunningScript* thread);
    static int32 GetUniqueScriptThingIndex(int32 playerGroup, eScriptThingType type);

    static bool IsEntityWithinAnySearchLight(CEntity* entity, int32* pIndex);
    static bool IsEntityWithinSearchLight(uint32 index, CEntity* entity);
    static bool IsPedStopped(CPed* ped);
    static bool IsPlayerOnAMission();
    static bool IsPointWithinSearchLight(const CVector& pointPosn, int32 index);
    static bool IsVehicleStopped(CVehicle* veh);

    static void Load();
    static void Save();

    static void MoveSearchLightBetweenTwoPoints(int32 index, float x1, float y1, float z1, float x2, float y2, float z2, float pathSpeed);
    static void MoveSearchLightToEntity(int32 index, CEntity* entity, float pathSpeed);
    static void MoveSearchLightToPointAndStop(int32 index, float x, float y, float z, float pathSpeed);

    static void Process();
    static void ProcessAllSearchLights();
    static void ProcessWaitingForScriptBrainArray();

    static void ReinitialiseSwitchStatementData();

    static void RemoveFromVehicleModelsBlockedByScript(int32 modelIndex);
    static void RemoveFromWaitingForScriptBrainArray(CEntity* entity, int16 modelIndex);
    static void RemoveScriptCheckpoint(int32 scriptIndex);
    static void RemoveScriptEffectSystem(int32 scriptIndex);
    static void RemoveScriptSearchLight(int32 scriptIndex);
    static void RemoveScriptSphere(int32 scriptIndex);
    static void RemoveScriptTextureDictionary();
    static void RemoveThisPed(CPed* ped);

    static void RenderAllSearchLights();
    static bool ScriptAttachAnimGroupToCharModel(int32 modelId, const char* ifpName);
    static void ScriptConnectLodsFunction(int32 objectHandle1, int32 objectHandle2);
    static void ScriptDebugCircle2D(float x, float y, float width, float height, CRGBA color);
    static CRunningScript* StartNewScript(uint8* startIP);
    static CRunningScript* StartNewScript(uint8* startIP, uint16 index);
    static void StartTestScript();
    static void UndoBuildingSwaps();
    static void UndoEntityInvisibilitySettings();
    static void UseSwitchJumpTable(int32& switchLabelAddress);
    static void WipeLocalVariableMemoryForMissionScript();

    static bool HasCarModelBeenSuppressed(eModelID carModelId);
    static bool HasVehicleModelBeenBlockedByScript(eModelID carModelId);

    // DEBUG
    static void ScriptDebugLine3D(const CVector& start, const CVector& end, uint32 color1, uint32 color2);
    static void RenderTheScriptDebugLines();

    static void PrintListSizes();

    static void DrawScriptSpheres();
    static void HighlightImportantArea(uint32 markerId, float fromX, float fromY, float toX, float toY, float height);
    static void HighlightImportantArea(uint32 markerId, const CRect& area, float height) { HighlightImportantArea(markerId, area.left, area.top, area.right, area.bottom, height); } // NOTSA
    static void HighlightImportantAngledArea(uint32 markerId, float fromX, float fromY, float toX, float toY, float angledToX, float angledToY, float angledFromX, float angledFromY, float height);
    static void DrawDebugSquare(float, float, float, float);
    static void DrawDebugSquare(const CRect& area) { DrawDebugSquare(area.left, area.top, area.right, area.bottom); }
    static void DrawDebugAngledSquare(const CVector2D& inf, const CVector2D& sup, const CVector2D& rotSup, const CVector2D& rotInf);
    static void DrawDebugCube(const CVector& inf, const CVector& sup);
    static void DrawDebugAngledCube(const CVector& inf, const CVector& sup, const CVector2D& rotSup, const CVector2D& rotInf);
    static void DrawScriptSpritesAndRectangles(bool drawBeforeFade);

    // NOTSA
    template<typename ChunkT = tSCMChunkHeader>
        requires std::is_base_of_v<tSCMChunkHeader, ChunkT>
    static const ChunkT* GetSCMChunk() {
        // A SCM file can have any amount of header chunks before the main script,
        // under these conditions:
        //
        // 1. Vanilla EXE expects at least 2 chunks to be available. Having less than that
        // will crash the game.
        //
        // 2. The last chunk must set the main script offset as next chunk offset. So virtual
        // machine will just jump through all chunks to the main script.
        constexpr auto NUM_VANILLA_MAIN_CHUNKS = 6u;
        const auto* header = reinterpret_cast<tSCMChunkHeader*>(&ScriptSpace[0]);
        for (auto i = 0u; i < NUM_VANILLA_MAIN_CHUNKS; i++) {
            static constexpr uint8 GoToInst[] = { 0x02, 0x00, 0x01 };

            if (header->m_ChunkIndex == ChunkT::Index)
                return header->As<ChunkT>();

            assert(!memcmp(header->m_InstrGoTo, GoToInst, sizeof(GoToInst)));
            header = reinterpret_cast<tSCMChunkHeader*>(&ScriptSpace[header->m_NextChunkOffset]);
        }

        NOTSA_UNREACHABLE_RET(nullptr);
    }

    static int32* GetPointerToScriptVariable(uint32 offset) {
        // TODO: find out how this method changed between re3 and GTA:SA
        assert(offset >= 8 && offset < GetSizeOfVariableSpace());
        return (int32*)&ScriptSpace[offset];
    }

    static int8 Read1ByteFromScript(uint8*& ip) {
        int8 retval = *reinterpret_cast<int8*>(ip);
        ip += 1;
        return retval;
    }

    static int16 Read2BytesFromScript(uint8*& ip) {
        int16 retval = *reinterpret_cast<int16*>(ip);
        ip += 2;
        return retval;
    }

    static int32 Read4BytesFromScript(uint8*& ip) {
        int32 retval = *reinterpret_cast<int32*>(ip); // big-endian unfriendly/unaligned mem access
        ip += 4;
        return retval;
    }

    static float ReadFloatFromScript(uint8*& ip) {
        int32 retval = Read4BytesFromScript(ip);
        return *reinterpret_cast<float*>(&retval);
    }

#define KEY_LENGTH_IN_SCRIPT (8)

    static void ReadTextLabelFromScript(uint8*& ip, char* buf) {
        strncpy_s(buf, KEY_LENGTH_IN_SCRIPT, (const char*) & ScriptSpace[*ip], KEY_LENGTH_IN_SCRIPT);
    }

    static auto * GetTextByKeyFromScript(uint8*& ip) {
        auto* text = TheText.Get((const char*)&ScriptSpace[*ip]);
        ip += KEY_LENGTH_IN_SCRIPT;
        return text;
    }

    static uint32 GetSizeOfVariableSpace() {
        uint8* tmp = &MainSCMBlock[3];
        return Read4BytesFromScript(tmp);
    }
};
