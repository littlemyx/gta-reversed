#pragma once

#include <array>
#include <span>

#include "Enums/eDirection.h"
#include "Vector.h"
#include "Base.h"
#include "Route.hpp"

class CVector;
class CEntity;
class CPed;
class CColSphere;
class CPhysical;

class CPointRoute : public notsa::Route<CVector> {
public:
    static void* operator new(uint32 size);
    static void operator delete(void* ptr, size_t sz);
};
VALIDATE_SIZE(CPointRoute, 0x64);

class CPedGeometryAnalyser {
public:
    static void InjectHooks();
    
    static bool CanPedJumpObstacle(const CPed& ped, const CEntity& entity);
    static bool CanPedJumpObstacle(const CPed& ped, const CEntity& entity, const CVector& contactNormal, const CVector& contactPos);

    static bool CanPedTargetPed(CPed& ped, CPed& targetPed, bool checkDirection);
    static bool CanPedTargetPoint(const CPed& ped, const CVector& point, bool checkDirection);

    static int32 ComputeBuildingHitPoints(const CVector& a1, const CVector& a2);
    
    static void ComputeClearTarget(const CPed& ped, const CVector& target, CVector& outTarget);

    static bool ComputeClosestSurfacePoint(const CPed& ped, CEntity& entity, CVector& point);
    static bool ComputeClosestSurfacePoint(const CVector& posn, CEntity& entity, CVector& point);
    static bool ComputeClosestSurfacePoint(const CVector& posn, const CVector* corners, CVector& point);

    static void ComputeEntityBoundingBoxCentre(float zPos, CEntity& entity, CVector& center);
    static void ComputeEntityBoundingBoxCentreUncached(float zPos, const CVector* corners, CVector& center);
    static void ComputeEntityBoundingBoxCentreUncachedAll(float zPos, CEntity& entity, CVector& center);
    static void ComputeEntityBoundingBoxCorners(float zPos, CEntity& entity, CVector* corners);
    static void ComputeEntityBoundingBoxCornersUncached(float zPos, CEntity& entity, CVector* corners);
    static void ComputeEntityBoundingBoxPlanes(float zPos, CEntity& entity, CVector(*outPlanes)[4], float* outPlanesDot);
    static void ComputeEntityBoundingBoxPlanesUncached(float zPos, const CVector* corners, CVector(*outPlanes)[4], float* outPlanesDot);
    static void ComputeEntityBoundingBoxPlanesUncachedAll(float zPos, CEntity& entity, CVector (*outPlanes)[4], float* outPlanesDot);
    static void ComputeEntityBoundingBoxSegmentPlanes(float zPos, CEntity& entity, CVector* normals, float* dots);
    static CVector* ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector* corners, CVector& center, CVector* normals, float* dots);
    static CVector* ComputeEntityBoundingBoxSegmentPlanesUncachedAll(float zPos, CEntity& entity, CVector* a3, float* a4);
    static void ComputeEntityBoundingSphere(const CPed& ped, CEntity& entity, CColSphere& sphere);

    static void ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, CVector& outDirToAvoidEntity); // NOTE: The original has no meaningful return value

    static CVector ComputeEntityDir(const CEntity& entity, eDirection dir);
    static CVector* ComputeEntityDirs(const CEntity& entity, CVector* outDirs); // Writes 4 directions: forward, left, backward, right (see `eDirection`)
    static int32 ComputeEntityHitSide(const CPed& ped, CEntity& entity); // Returns `eDirection`
    static int32 ComputeEntityHitSide(const CVector& point, const CVector* planes, const float* dots); // Returns `eDirection`
    static int32 ComputeEntityHitSide(const CVector& point, CEntity& entity); // Returns `eDirection`
    static int32 ComputePedHitSide(const CPed& ped, const CPhysical& physical);
    static int32 ComputePedHitSide(const CPed& ped, const CVector& velocity);
    static int32 ComputePedShotSide(const CPed& ped, const CVector& posn);

    static int32 ComputeRouteRoundEntityBoundingBox(const CPed& ped, CEntity& entity, const CVector& to, CPointRoute& route, int32 mode);
    static int32 ComputeRouteRoundEntityBoundingBox(const CPed& ped, const CVector& from, CEntity& entity, const CVector& to, CPointRoute& route, int32 mode); // `mode`: 0 = choose the best path, 1 = path A, otherwise path B. Returns the path chosen (1 or 2), or 0 if no route is needed

    static bool ComputeRouteRoundSphere(const CPed& ped, const CColSphere& sphere, const CVector& a3, const CVector& a4, CVector& a5, CVector& a6);

    static bool GetIsLineOfSightClear(const CPed& ped, const CVector& target, CEntity& entity, float& outDist);
    static bool GetIsLineOfSightClear(const CVector& start, const CVector& end, CEntity& entity);
    static CPed* GetNearestPed(const CVector& point);

    static bool   IsEntityBlockingTarget(CEntity* entity, const CVector& point, float distance);
    static bool   IsInAir(const CPed& ped);

    enum class WanderPathClearness : uint32 {
        BLOCKED_HEIGHT,
        BLOCKED_LOS,
        BLOCKED_WATER,
        BLOCKED_SHARP_DROP,
        CLEAR,
    };
    static WanderPathClearness IsWanderPathClear(const CVector& from, const CVector& to, float maxHeightChange, int32 maxSamples);

    static bool LiesInsideBoundingBox(const CPed& ped, const CVector& posn, CEntity& entity);
};
