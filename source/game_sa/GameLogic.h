#pragma once

#include <Weapon.h>
#include <Vector.h>
#include <Enums/eLevelName.h>

class CEntity;
class CPed;
class CVehicle;

enum eSkipState {
    SKIP_NONE,
    SKIP_AVAILABLE,
    SKIP_IN_PROGRESS,
    SKIP_FADING_OUT,
    SKIP_AFTER_MISSION,
    SKIP_WAITING_SCRIPT
};

enum eGameLogicState : uint8 { // a BYTE in the exe (0x96A8B0), the next bytes are other variables
    GAMELOGIC_STATE_PLAYING,
    GAMELOGIC_STATE_WASTED,
    GAMELOGIC_STATE_BUSTED,
    GAMELOGIC_STATE_MISSION_FAILED,
    GAMELOGIC_STATE_MISSION_PASSED
};

// Used in CGameLogic::n2PlayerPedInFocus
enum class eFocusedPlayer : int32 {
    PLAYER1,
    PLAYER2,
    NONE
};

class CPed;

class CGameLogic {
public:
    static inline NOTSA_GLOBAL(SavedWeapons, 0x96A9B8, (std::array<CWeapon, NUM_WEAPON_SLOTS>), {}); VALIDATE_SIZE(SavedWeapons, 364);

    static inline NOTSA_GLOBAL(AfterDeathStartPointOrientations, 0x96A850, (std::array<float, 16>), {});
    static inline NOTSA_GLOBAL(AfterDeathStartPoints, 0x96A8E0, (std::array<CVector, 16>), {});

    static inline NOTSA_GLOBAL(SkipPosition, 0x96A8D4, (CVector), {});
    static inline NOTSA_GLOBAL(SkipHeading, 0x96A8A4, (float), {});

    static inline NOTSA_GLOBAL(nPrintFocusHelpCounter, 0x96A8B8, (int32), {});
    static inline NOTSA_GLOBAL(nPrintFocusHelpTimer, 0x96A8B4, (int32), {});
    static inline NOTSA_GLOBAL(f2PlayerStartHeading, 0x96A840, (float), {});
    static inline NOTSA_GLOBAL(vec2PlayerStartLocation, 0x96A9AC, (CVector), {});
    static inline NOTSA_GLOBAL(bPlayersCanBeInSeparateCars, 0x96A8B3, (bool), {});
    static inline NOTSA_GLOBAL(bPlayersCannotTargetEachOther, 0x96A8B2, (bool), {});

    static inline NOTSA_GLOBAL(NumAfterDeathStartPoints, 0x96A890, (int32), {});

    static inline NOTSA_GLOBAL(SkipToBeFinishedByScript, 0x96A894, (bool), {});
    static inline NOTSA_GLOBAL(SkipVehicle, 0x96A898, (CVehicle*), {});
    static inline NOTSA_GLOBAL(SkipTimer, 0x96A89C, (uint32), {});
    static inline NOTSA_GLOBAL(SkipState, 0x96A8A0, (eSkipState), {});

    static inline NOTSA_GLOBAL(bScriptCoopGameGoingOn, 0x96A8A8, (bool), {});
    static inline NOTSA_GLOBAL(TimeOfLastEvent, 0x96A8AC, (int32), {});
    // 0x96A8B0..0x96A8B3 are four separate BYTES in the exe (GameState, ActivePlayers, bPlayersCannotTargetEachOther, bPlayersCanBeInSeparateCars), then `nPrintFocusHelpTimer` (dword at 0x96A8B4)
    static inline NOTSA_GLOBAL(GameState, 0x96A8B0, (eGameLogicState), {});
    static inline NOTSA_GLOBAL(ActivePlayers, 0x96A8B1, (uint8), {});

    static inline NOTSA_GLOBAL(bPenaltyForDeathApplies, 0x8A5E48, (bool), { true });
    static inline NOTSA_GLOBAL(bPenaltyForArrestApplies, 0x8A5E49, (bool), { true });
    static inline NOTSA_GLOBAL(bLimitPlayerDistance, 0x8A5E4A, (bool), { true });
    static inline NOTSA_GLOBAL(MaxPlayerDistance, 0x8A5E4C, (float), { 20.0f });  // default 20.0
    static inline NOTSA_GLOBAL(n2PlayerPedInFocus, 0x8A5E50, (eFocusedPlayer), { eFocusedPlayer::NONE }); // default eFocusedPlayer::NONE

public:
    static void InjectHooks();

    /*!
     * @addr 0x4418E0
     * @brief Calculates the distance to a forbidden train crossing from a given point.
     *
     * @param point The position to calculate distance from.
     * @param moveSpeed The movement velocity vector.
     * @param ignoreMoveSpeed If true, the `moveSpeed` parameter is ignored in the calculation.
     * @param outDistance Output parameter that is updated only when a forbidden crossing is found
     *        (that is, when the returned distance is less than the no-crossing sentinel value).
     *        It contains the 2D displacement vector from `point` to the chosen forbidden crossing,
     *        computed in the XY plane with `z` set to `0`.
     *
     * @return The scalar distance to the forbidden train crossing, or the no-crossing sentinel
     *         value if none was found. `outDistance` is only valid when the returned distance is
     *         less than the no-crossing sentinel value.
     */
    static float CalcDistanceToForbiddenTrainCrossing(CVector point, CVector moveSpeed, bool ignoreMoveSpeed, CVector& outDistance);
    static void ClearSkip(bool afterMission);
    static void DoWeaponStuffAtStartOf2PlayerGame(bool shareWeapons);
    static void StorePedsWeapons(CPed* ped);
    static eLevelName FindCityClosestToPoint(CVector2D point);
    static void ForceDeathRestart();
    static void InitAtStartOfGame();
    static bool IsCoopGameGoingOn();
    static bool IsPlayerAllowedToGoInThisDirection(CPed* ped, CVector moveDirection, float distanceLimit);
    static bool IsPlayerUse2PlayerControls(CPed* ped);
    static bool IsPointWithinLineArea(const CVector* points, uint32 numPoints, float x, float y);
    static bool IsSkipWaitingForScriptToFadeIn();
    static bool LaRiotsActiveHere();
    static void Save();
    static void Load();
    static void PassTime(uint32 minutes);
    static void Remove2ndPlayerIfPresent();
    static void ResetStuffUponResurrection();
    static void RestorePedsWeapons(CPed* ped);
    static void RestorePlayerStuffDuringResurrection(CPlayerPed* player, CVector posn, float playerStartHeading);
    static void SetPlayerWantedLevelForForbiddenTerritories(bool immediately);
    static void SetUpSkip(CVector coors, float angle, bool afterMission, CEntity* vehicle, bool finishedByScript);
    static bool SkipCanBeActivated();
    static void SortOutStreamingAndMemory(const CVector& translation, float angle);
    static void StopPlayerMovingFromDirection(int32 playerId, CVector direction);
    static void Update();
    static void UpdateSkip();

    // @notsa
    static bool IsAPlayerInFocusOn2PlayerGame();

    // @notsa
    static CPlayerPed* GetFocusedPlayerPed();

    // @notsa
    static bool CanPlayerTripSkip();

    // @notsa
    static void SetMissionFailed();
};
