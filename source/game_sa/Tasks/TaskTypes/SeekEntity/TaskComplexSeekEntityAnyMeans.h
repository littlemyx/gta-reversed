#pragma once

#include <type_traits>
#include <concepts>

#include "Base.h"
#include "TaskComplex.h"
#include "PosCalculators/EntitySeekPosCalculator.h"
#include "TaskComplexDriveToPoint.h"
#include "TaskComplexLeaveAnyCar.h"
#include "TaskTimer.h"
#include "Entity.h"

class CPed;

/*!
* Follows an entity using a car.
* When the entity is destroyed the ped leaves the car.
* Do not use directly, use the derived classes instead, 
* such as `CTaskComplexSeekEntityAnyMeansXYOffset`
*
* @tparam T The position calculator to use
*/
template<std::derived_from<CEntitySeekPosCalculator> T>
class NOTSA_EXPORT_VTABLE CTaskComplexSeekEntityAnyMeans : public CTaskComplex {
    CEntity*   m_entity{};        /// The entity to seek
    int32      m_scanInterval{};  /// Entity pos seek interval 
    float      m_targetRadius{};  /// The "tolerance"
    CTaskTimer m_scanTimer{};     /// Timer which on expiration updates the subtasks' goto point and is re-started (Uses `m_scanInterval` as the interval)
    T          m_posCalculator{}; /// The position calculator
public:
    static constexpr auto Type = eTaskType::TASK_COMPLEX_SEEK_ENTITY_ANY_MEANS;

protected:
    CTaskComplexSeekEntityAnyMeans(CEntity* entity, int32 period, float radius, T posCalculator) :
        m_targetRadius{ radius },
        m_entity{ entity },
        m_scanInterval{ period },
        m_posCalculator{ posCalculator }
    {
        CEntity::SafeRegisterRef(m_entity);
    }

public:
    CTaskComplexSeekEntityAnyMeans(const CTaskComplexSeekEntityAnyMeans& o) :
        m_targetRadius{ 8.f },
        m_entity{ o.m_entity },
        m_scanInterval{ 1000 },
        m_posCalculator{ o.m_posCalculator }
    {
        CEntity::SafeRegisterRef(m_entity); // 0x696063-0x696075: Clone runs the real constructor (0x695600), which registers the reference
    }

    ~CTaskComplexSeekEntityAnyMeans() {
        CEntity::SafeCleanUpRef(m_entity);
    }

    CTask* Clone() const override {
        return new CTaskComplexSeekEntityAnyMeans{ *this };
    }

    eTaskType GetTaskType() const override {
        return Type;
    }

    CTask*    CreateNextSubTask(CPed* ped) override {
        return nullptr;
    }

    CTask* CreateFirstSubTask(CPed* ped) override {
        const auto seekPos = GetSeekPos(ped); // 0x6960D2-0x6960DF: the position is computed first
        m_scanTimer.StartUnguarded(m_scanInterval); // 0x6960EE-0x6960F9: starts the scan timer (no >= 0 test); the port never started it, so ControlSubTask never re-aimed the drive
        return new CTaskComplexDriveToPoint{ nullptr, seekPos, 30.f, 0, MODEL_INVALID, m_targetRadius, DRIVING_STYLE_STOP_FOR_CARS };
    }

    CTask* ControlSubTask(CPed* ped) override {
        if (m_entity) {
            if (m_scanTimer.IsOutOfTime()) {
                m_scanTimer.StartUnguarded(m_scanInterval); // 0x6961F3-0x696223: no >= 0 test
                notsa::cast<CTaskComplexDriveToPoint>(m_pSubTask)->GoToPoint(GetSeekPos(ped));
            }
            return m_pSubTask;
        }
        return new CTaskComplexLeaveAnyCar{ 0, true, false };
    }

    CVector GetSeekPos(CPed* ped) {
        CVector seekPos{};
        m_posCalculator.ComputeEntitySeekPos(*ped, *m_entity, seekPos);
        return seekPos;
    }
};
