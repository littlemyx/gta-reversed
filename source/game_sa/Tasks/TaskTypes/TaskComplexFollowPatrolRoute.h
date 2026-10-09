#pragma once

#include "TaskComplex.h"
#include "Vector.h"
#include "eMoveState.h"

class CPatrolRoute;

//! Makes the ped walk along a patrol route (see `CPatrolRoute`), playing the anim of each point (if any) once reached
class NOTSA_EXPORT_VTABLE CTaskComplexFollowPatrolRoute : public CTaskComplex {
public:
    //! What to do once the end of the route has been reached
    enum class eMode : int16 {
        ONCE,         //< Finish
        BACK_ONCE,    //< Walk back along the route (once), then finish
        PING_PONG,    //< Walk back and forth forever
        LOOP,         //< Start from the beginning, forever
    };

    static constexpr auto Type = TASK_COMPLEX_FOLLOW_PATROL_ROUTE;

    static void InjectHooks();

    /*!
    * @param moveState      The move state to use (Walk, run, ...)
    * @param route          The route to follow (This is copied)
    * @param mode           What to do when the end of the route is reached
    * @param radius         The target radius (for the points)
    * @param moveStateRadius The radius from which the final point is approached with the given move state (`CTaskComplexGoToPointAndStandStill`)
    */
    CTaskComplexFollowPatrolRoute(int16 moveState, const CPatrolRoute* route, eMode mode, float radius, float moveStateRadius); // 0x674930
    ~CTaskComplexFollowPatrolRoute() override; // 0x6709F0 (scalar deleting dtor: 0x672BB0)

    eTaskType GetTaskType() const override { return Type; } // 0x670A60
    CTask*    Clone() const override; // 0x674BB0
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x66BA90
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x670D80
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x670E20
    CTask*    ControlSubTask(CPed* ped) override; // 0x66BB00

    //! @return The type of the sub-task to create next
    eTaskType ComputeNextSubTaskType(); // 0x670A70

    CTask* CreateSubTask(eTaskType taskType); // 0x670B00

private:
    CTaskComplexFollowPatrolRoute* Constructor(int16 moveState, const CPatrolRoute* route, eMode mode, float radius, float moveStateRadius) {
        this->CTaskComplexFollowPatrolRoute::CTaskComplexFollowPatrolRoute(moveState, route, mode, radius, moveStateRadius);
        return this;
    }

private:
    eMode         m_Mode{};              // 0x0C
    int16         m_NumLoops{};          // 0x0E - How many times the end of the route has been reached
    int16         m_MoveState{};         // 0x10
    int16         m_CurNodeIdx{};        // 0x12
    float         m_Radius{};            // 0x14
    float         m_MoveStateRadius{};   // 0x18
    CPatrolRoute* m_Route{};             // 0x1C - Own copy
    union {                              // 0x20
        uint32 m_Flags{};
        struct {
            bool m_bRestart : 1;         //< Create the first sub-task again (Set at start)
            bool m_bFollowNodeRoute : 1; //< Go back to the route using a node route (Set if aborted)
        };
    };
    CVector       m_PedPos{};            // 0x24 - Position of the ped when aborted
};
VALIDATE_SIZE(CTaskComplexFollowPatrolRoute, 0x30);
