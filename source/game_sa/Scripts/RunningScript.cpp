#include "StdInc.h"

#include "RunningScript.h"
#include "TheScripts.h"
#include "CarGenerator.h"
#include "Hud.h"
#include "ShotInfo.h"
#include "PedScriptedTaskRecord.h"
#include "TaskSequences.h"
#include "TaskSimpleFinishBrain.h"
#include "TaskSimpleRunNamedAnim.h"
#include "TaskSimpleAffectSecondaryBehaviour.h"
#include "TaskSimpleHoldEntity.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <numbers>

static notsa::log_ptr logger;

//static auto logger = NOTSA_MAKE_LOGGER("script");

//! Define it to dump out all commands that don't have a custom handler (that is, they aren't reversed)
//! Makes compilation slow, so don't enable unless necessary!
//#define DUMP_CUSTOM_COMMAND_HANDLERS_TO_FILE

#ifdef DUMP_CUSTOM_COMMAND_HANDLERS_TO_FILE
#include <fstream>
#endif

#include "CommandParser/Parser.hpp"
#include "CommandParser/LUTGenerator.hpp"
#include "reversiblehooks/ReversibleHook/ScriptCommandHook.h"

#include "Commands/Commands.hpp"
#ifdef NOTSA_WITH_CLEO_SCRIPT_COMMANDS // TODO: Add premake/cmake option for this define
#include "Commands/CLEO/Commands.hpp"
#include "Commands/CLEO/Extensions/Commands.hpp"
#endif

// https://library.sannybuilder.com/#/sa

//! Holds all custom command handlers (or null for commands with no custom handler)
static inline std::array<notsa::script::CommandHandlerFunction, (size_t)(COMMAND_HIGHEST_ID_TO_HOOK) + 1> s_CustomCommandHandlerTable{};

std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers        = {};
uint8                                                                                CRunningScript::ScriptArgCharNextFreeBuffer = 0;

void CRunningScript::InjectHooks() {
    logger = NOTSA_MAKE_LOGGER("script");

    InjectCustomCommandHooks();

    RH_ScopedClass(CRunningScript);
    RH_ScopedCategory("Scripts");

    RH_ScopedInstall(Init, 0x4648E0);
    RH_ScopedInstall(GetCorrectPedModelIndexForEmergencyServiceType, 0x464F50);

    RH_ScopedInstall(PlayAnimScriptCommand, 0x470150);
    RH_ScopedInstall(LocateCarCommand, 0x487A20);
    RH_ScopedInstall(LocateCharCommand, 0x486D80);
    RH_ScopedInstall(LocateObjectCommand, 0x487D10);
    RH_ScopedInstall(LocateCharCarCommand, 0x487420);
    RH_ScopedInstall(LocateCharCharCommand, 0x4870F0);
    RH_ScopedInstall(LocateCharObjectCommand, 0x487720);
    RH_ScopedInstall(CarInAreaCheckCommand, 0x488EC0);
    RH_ScopedInstall(CharInAreaCheckCommand, 0x488B50);
    RH_ScopedInstall(ObjectInAreaCheckCommand, 0x489150);
    RH_ScopedInstall(CharInAngledAreaCheckCommand, 0x487F60);
    RH_ScopedInstall(FlameInAngledAreaCheckCommand, 0x488780);
    RH_ScopedInstall(ObjectInAngledAreaCheckCommand, 0x4883F0);
    RH_ScopedInstall(CollectParameters, 0x464080, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(CollectNextParameterWithoutIncreasingPC, 0x464250, { .StackArgumentsToPreserve = 0, .PreserveRegisters = true });
    RH_ScopedInstall(StoreParameters, 0x464370, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ReadArrayInformation, 0x463CF0, { .StackArgumentsToPreserve = 3, .PreserveRegisters = true });
    RH_ScopedInstall(ReadParametersForNewlyStartedScript, 0x464500, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ReadTextLabelFromScript, 0x463D50, { .StackArgumentsToPreserve = 2, .PreserveRegisters = true });
    RH_ScopedInstall(GetIndexOfGlobalVariable, 0x464700, { .StackArgumentsToPreserve = 0, .PreserveRegisters = true });
    RH_ScopedInstall(GetPadState, 0x485B10);
    RH_ScopedInstall(GetPointerToLocalVariable, 0x463CA0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(GetPointerToLocalArrayElement, 0x463CC0, { .StackArgumentsToPreserve = 3, .PreserveRegisters = true });
    RH_ScopedInstall(GetPointerToScriptVariable, 0x464790, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(DoDeathArrestCheck, 0x485A50);
    RH_ScopedInstall(SetCharCoordinates, 0x464DC0);
    RH_ScopedInstall(AddScriptToList, 0x464C00, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(RemoveScriptFromList, 0x464BD0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ShutdownThisScript, 0x465AA0);
    RH_ScopedInstall(IsPedDead, 0x464D70);
    RH_ScopedInstall(ThisIsAValidRandomPed, 0x489490);
    RH_ScopedInstall(ScriptTaskPickUpObject, 0x46AF50);
    RH_ScopedInstall(UpdateCompareFlag, 0x4859D0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(UpdatePC, 0x464DA0, { .StackArgumentsToPreserve = 1, .PreserveRegisters = true });
    RH_ScopedInstall(ProcessOneCommand, 0x469EB0);
    RH_ScopedInstall(Process, 0x469F00);
    RH_ScopedOverloadedInstall(GivePedScriptedTask, "OG", 0x465C20, void(CRunningScript::*)(int32, CTask*, int32));
}

//! Register our custom script command handlers
void CRunningScript::InjectCustomCommandHooks() {
    // Uncommenting any call will prevent it from being hooked, so
    // feel free to do so when debugging (Just don't forget to undo the changes!)

    namespace c = ::notsa::script::commands;
    c::basic::RegisterHandlers();
    c::camera::RegisterHandlers();
    c::character::RegisterHandlers();
    c::clock::RegisterHandlers();
    c::comparasion::RegisterHandlers();
    c::game::RegisterHandlers();
    c::generic::RegisterHandlers();
    c::math::RegisterHandlers();
    c::mission::RegisterHandlers();
    c::object::RegisterHandlers();
    c::pad::RegisterHandlers();
    c::ped::RegisterHandlers();
    c::player::RegisterHandlers();
    c::script::RegisterHandlers();
    c::sequence::RegisterHandlers();
    c::text::RegisterHandlers();
    c::unused::RegisterHandlers();
    c::utility::RegisterHandlers();
    c::vehicle::RegisterHandlers();
    c::zone::RegisterHandlers();
    c::stat::RegisterHandlers();
    c::conversation::RegisterHandlers();
    c::ported::g01_04::RegisterHandlers();
    c::ported::g05_08::RegisterHandlers();
    c::ported::g09_12::RegisterHandlers();
    c::ported::g13_15::RegisterHandlers();
    c::ported::g20_21::RegisterHandlers();
    c::ported::g16::RegisterHandlers();
    c::ported::g17b::RegisterHandlers();
    c::ported::g17c::RegisterHandlers();
    c::ported::g18::RegisterHandlers();
    c::ported::g19::RegisterHandlers();
    c::ported::g22::RegisterHandlers();

#ifdef NOTSA_WITH_CLEO_SCRIPT_COMMANDS
    cleo::audiostream::RegisterHandlers();
    cleo::character::RegisterHandlers();
    cleo::dynamiclibrary::RegisterHandlers();
    cleo::fs::RegisterHandlers();
    cleo::game::RegisterHandlers();
    cleo::generic::RegisterHandlers();
    cleo::memory::RegisterHandlers();
    cleo::pad::RegisterHandlers();
    cleo::script::RegisterHandlers();
    cleo::vehicle::RegisterHandlers();
    cleo::world::RegisterHandlers();

    cleo::extensions::cleoplus::RegisterHandlers();
    cleo::extensions::clipboard::RegisterHandlers();
    cleo::extensions::fs::RegisterHandlers();
    cleo::extensions::imgui::RegisterHandlers();
    cleo::extensions::intoperations::RegisterHandlers();
#endif

#ifdef DUMP_CUSTOM_COMMAND_HANDLERS_TO_FILE
    auto reversed{0}, total{0};
    std::ofstream ofsrev{ "reversed_script_command_handlers.txt" }, ofsnotrev{ "NOT_reversed_script_command_handlers.txt" };
    for (auto&& [idx, handler] : rngv::enumerate(s_CustomCommandHandlerTable)) {
        const auto id = (eScriptCommands)(idx);
        ++total;
        if (handler) ++reversed;
        (handler ? ofsrev : ofsnotrev) << ::notsa::script::GetScriptCommandName(id) << '\n';
    }
    NOTSA_LOG_DEBUG("Script cmds dumped! Find them in `<GTA Directory>/Scripts`!");
    NOTSA_LOG_DEBUG("Script cmds reverse progress: {}/{} ({:.2f}% done)", reversed, total, 100.0f * ((float)reversed / (float)total));
#endif
}

// 0x4648E0
void CRunningScript::Init() {
    SetName("noname");
    rng::fill(m_IPStack, nullptr);
    rng::fill(m_LocalVars, tScriptParam{ 0 });
    m_BaseIP                          = nullptr;
    m_pPrev                           = nullptr;
    m_pNext                           = nullptr;
    m_IP                              = nullptr;
    m_StackDepth                      = 0;
    m_WakeTime                        = 0;
    m_IsActive                        = false;
    m_CondResult                      = false;
    m_UsesMissionCleanup              = false;
    m_IsExternal                      = false;
    m_IsTextBlockOverride             = false;
    m_ExternalType                    = -1;
    m_AndOrState                      = 0;
    m_NotFlag                         = false;
    m_DoneDeathArrest                 = false;
    m_SceneSkipIP                     = 0;
    m_ThisMustBeTheOnlyMissionRunning = false;
    m_IsDeathArrestCheckEnabled       = true;
}

/*!
 * Adds script to list
 * @addr 0x464C00
 */
void CRunningScript::AddScriptToList(CRunningScript** queueList) {
    m_pNext = *queueList;
    m_pPrev = nullptr;
    if (*queueList)
        (*queueList)->m_pPrev = this;
    *queueList = this;
}

/*!
 * Removes script from list
 * @addr 0x464BD0
 */
void CRunningScript::RemoveScriptFromList(CRunningScript** queueList) {
    if (m_pPrev)
        m_pPrev->m_pNext = m_pNext;
    else
        *queueList = m_pNext;

    if (m_pNext)
        m_pNext->m_pPrev = m_pPrev;
}

/*!
 * Terminates a script
 * @addr 0x465AA0
 */
void CRunningScript::ShutdownThisScript() {
    m_IsActive = false;

    if (m_IsExternal) {
        const auto idx = CTheScripts::StreamedScripts.GetStreamedScriptWithThisStartAddress(m_BaseIP);
        CTheScripts::StreamedScripts.m_aScripts[idx].m_NumberOfUsers--;
    }

    switch (m_ExternalType) {
    case 0:
    case 2:
    case 3:
    case 5: {
        const auto pedRef = m_ThisMustBeTheOnlyMissionRunning
            ? CTheScripts::LocalVariablesForCurrentMission[0].iParam
            : m_LocalVars[0].iParam;
        if (auto* const ped = GetPedPool()->GetAtRef(pedRef)) {
            ped->bHasAScriptBrain = false;
            if (m_ExternalType == 5) {
                CScriptedBrainTaskStore::SetTask(ped, new CTaskSimpleFinishBrain{});
            }
        }
        break;
    }
    case 1:
    case 4: {
        const auto objRef = m_ThisMustBeTheOnlyMissionRunning
            ? CTheScripts::LocalVariablesForCurrentMission[0].iParam
            : m_LocalVars[0].iParam;
        if (auto* const obj = GetObjectPool()->GetAtRef(objRef)) {
            obj->objectFlags.b0x100000_0x200000 = 1; // Clears 0x200000, sets 0x100000
        }
        break;
    }
    default:
        break;
    }
}

// 0x465C20
void CRunningScript::GivePedScriptedTask(int32 pedHandle, CTask* task, int32 opcode) {
    if (pedHandle == -1) {
        CTaskSequences::AddTaskToActiveSequence(task);
        return;
    }

    CPed* ped = GetPedPool()->GetAtRef(pedHandle);
    assert(ped);
    CPedGroup* pedGroup = CPedGroups::GetPedsGroup(ped);

    CPed* otherPed = nullptr;
    if (m_ExternalType == 5 || m_ExternalType == 2 || !m_ExternalType || m_ExternalType == 3) {
        auto* localVariable = reinterpret_cast<int32*>(GetPointerToLocalVariable(0));
        otherPed = GetPedPool()->GetAtRef(*localVariable);
    }

    if (ped->bHasAScriptBrain && otherPed != ped) {
        delete task;
    } else if (otherPed && m_ExternalType == 5) {
        if (CScriptedBrainTaskStore::SetTask(ped, task)) {
            const int32 slot = CPedScriptedTaskRecord::GetVacantSlot();
            CPedScriptedTaskRecord::ms_scriptedTasks[slot].SetAsAttractorScriptTask(ped, opcode, task);
        }
    } else if (!pedGroup || ped->IsPlayer()) {
        CEventScriptCommand eventScriptCommand(TASK_PRIMARY_PRIMARY, task, false);
        auto* event = static_cast<CEventScriptCommand*>(ped->GetEventGroup().Add(&eventScriptCommand, false));
        if (event) {
            const int32 slot = CPedScriptedTaskRecord::GetVacantSlot();
            CPedScriptedTaskRecord::ms_scriptedTasks[slot].Set(ped, opcode, event);
        }
    } else {
        pedGroup->GetIntelligence().SetScriptCommandTask(ped, *task);
        CTask* scriptedTask = pedGroup->GetIntelligence().GetTaskScriptCommand(ped);
        const int32 slot = CPedScriptedTaskRecord::GetVacantSlot();
        CPedScriptedTaskRecord::ms_scriptedTasks[slot].SetAsGroupTask(ped, opcode, scriptedTask);
        delete task;
    }
}

void CRunningScript::GivePedScriptedTask(CPed* ped, CTask* task, int32 opcode) {
    GivePedScriptedTask(GetPedPool()->GetRef(ped), task, opcode); // Must do it like this, otherwise unhooking of the original `GivePedScriptedTask` will do nothing
}

namespace {
//! Calculates the corners of an angled area: `C` (next to `B`) and `D` (next to `A`)
//! Used by all of the `*InAngledAreaCheckCommand`s
void CalculateAngledAreaCorners(float x1, float y1, float x2, float y2, float width, CVector2D& c, CVector2D& d) {
    constexpr float TWO_PI_F = 2.f * std::numbers::pi_v<float>; // 0x858CBC

    float angle = CGeneral::GetRadianAngleBetweenPoints(x1, y1, x2, y2) + std::numbers::pi_v<float> / 2.f; // 0x858FE4
    while (angle < 0.f) {
        angle += TWO_PI_F;
    }
    while (angle > TWO_PI_F) {
        angle -= TWO_PI_F;
    }
    const auto s = std::sin(angle);
    const auto co = std::cos(angle);
    c = CVector2D{ x2 + s * width, -(co * width) + y2 };
    d = CVector2D{ s * width + x1, -(co * width) + y1 };
}

//! Checks if the point is within the angled area (ignoring Z)
//! NOTE: Doesn't use `notsa::shapes::AngledRect` on purpose, as the original has a few quirks.
bool IsPointInAngledArea2D(float px, float py, float x1, float y1, float x2, float y2, CVector2D d) {
    CVector2D ab{ x2 - x1, y2 - y1 };
    CVector2D ad{ d.x - x1, d.y - y1 };
    const float abLen = std::sqrt(ab.y * ab.y + ab.x * ab.x);
    const float adLen = std::sqrt(ad.y * ad.y + ad.x * ad.x);
    const CVector2D rel{ px - x1, py - y1 };

    ab.Normalise(); // NOTE: In the original this is done on a copy
    const float dotAB = rel.y * ab.y + rel.x * ab.x;
    if (dotAB < 0.f || dotAB > abLen) {
        return false;
    }

    ad.Normalise();
    const float dotAD = rel.y * ad.y + rel.x * ad.x;
    return dotAD >= 0.f && dotAD <= adLen;
}

//! The id used to identify the highlighted area (original: `m_IP + this`)
int32 GetHighlightId(const CRunningScript* s) {
    return reinterpret_cast<int32>(s) + reinterpret_cast<int32>(s->m_IP);
}
} // namespace

// 0x470150
void CRunningScript::PlayAnimScriptCommand(int32 commandId) {
    CollectParameters(1);
    const auto pedHandle = ScriptParams[0].iParam;

    char animName[24];
    char animGroup[16];
    ReadTextLabelFromScript(animName, sizeof(animName));
    ReadTextLabelFromScript(animGroup, sizeof(animGroup));

    bool interruptable = true; // Original: `bVar6` (the inverse of `dontInterrupt`)
    bool offsetPed     = false;
    switch (commandId) {
    case 0x88A: // 8 params
        CollectParameters(8);
        interruptable = ScriptParams[6].iParam != 0;
        offsetPed     = ScriptParams[7].iParam != 0;
        break;
    case 0x812:
        interruptable = false;
        [[fallthrough]];
    case 0x605:
    case 0xA1A:
        CollectParameters(6);
        break;
    default: // NOTE: No more params are collected here (original behaviour)
        break;
    }

    const float blendDelta = ScriptParams[0].fParam;
    const int32 loop       = ScriptParams[1].iParam;
    const int32 lockX      = ScriptParams[2].iParam;
    const int32 lockY      = ScriptParams[3].iParam;
    const int32 lockF      = ScriptParams[4].iParam;
    const int32 time       = ScriptParams[5].iParam;

    uint32 animFlags = ANIMATION_IS_PARTIAL; // 0x10
    if (loop || (time > 0 && !lockF)) {
        animFlags = ANIMATION_IS_PARTIAL | ANIMATION_IS_LOOPED; // 0x12
    }
    if (lockX) {
        animFlags |= ANIMATION_CAN_EXTRACT_VELOCITY; // 0x40
    }
    if (lockY) {
        animFlags |= ANIMATION_CAN_EXTRACT_X_VELOCITY; // 0x80
    }
    if (!lockF) {
        animFlags |= ANIMATION_IS_FINISH_AUTO_REMOVE; // 0x8
    }
    if (commandId == 0xA1A) {
        animFlags |= ANIMATION_DONT_ADD_TO_PARTIAL_BLEND; // 0x400
    }

    const bool runInSequence = CTaskSequences::ms_iActiveSequence >= 0;

    CTask* task = new CTaskSimpleRunNamedAnim(
        animName,
        animGroup,
        animFlags,
        blendDelta,
        time > 0 ? time : -1,
        !interruptable,
        runInSequence,
        offsetPed,
        false
    );
    if (commandId == 0xA1A) {
        task = new CTaskSimpleAffectSecondaryBehaviour(true, TASK_SECONDARY_PARTIAL_ANIM, task);
    }
    GivePedScriptedTask(pedHandle, task, commandId);
}

// 0x487A20
void CRunningScript::LocateCarCommand(int32 commandId) {
    const bool is3D = commandId >= 0x1AF && commandId <= 0x1B0;
    CollectParameters(is3D ? 8 : 6);

    auto* const veh = GetVehiclePool()->GetAtRef(ScriptParams[0].iParam);
    assert(veh);

    bool notStopped = false;
    if (commandId == 0x1AE || commandId == 0x1B0) { // Stopped variants
        if (!CTheScripts::IsVehicleStopped(veh)) {
            notStopped = true;
        }
    }

    const float x = ScriptParams[1].fParam;
    const float y = ScriptParams[2].fParam;
    float       z = 0.f, rx, ry, rz = 0.f;
    int32       highlight;
    if (is3D) {
        z         = ScriptParams[3].fParam;
        rx        = ScriptParams[4].fParam;
        ry        = ScriptParams[5].fParam;
        rz        = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
    } else {
        rx        = ScriptParams[3].fParam;
        ry        = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }

    bool result = false;
    if (!notStopped) {
        const auto& pos = veh->GetPosition();
        if (!(x - rx > pos.x || x + rx < pos.x || y - ry > pos.y || y + ry < pos.y)) {
            if (!is3D || !(z - rz > pos.z || z + rz < pos.z)) {
                result = true;
            }
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ x - rx, y - ry }, CVector2D{ x + rx, y + ry }, is3D ? z : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(x - rx, y - ry, x + rx, y + ry);
    }
}

// 0x486D80
void CRunningScript::LocateCharCommand(int32 commandId) {
    const bool is3D = commandId >= 0xFE && commandId <= 0x103;
    CollectParameters(is3D ? 8 : 6);

    auto* const ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    assert(ped);

    const auto pos = ped->GetRealPosition();

    bool notStopped = false;
    switch (commandId) {
    case 0xEF:
    case 0xF0:
    case 0xF1:
    case 0x101:
    case 0x102:
    case 0x103: // Stopped variants
        if (!CTheScripts::IsPedStopped(ped)) {
            notStopped = true;
        }
        break;
    default:
        break;
    }

    const float x = ScriptParams[1].fParam;
    const float y = ScriptParams[2].fParam;
    float       z = 0.f, rx, ry, rz = 0.f;
    int32       highlight;
    if (is3D) {
        z         = ScriptParams[3].fParam;
        rx        = ScriptParams[4].fParam;
        ry        = ScriptParams[5].fParam;
        rz        = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
    } else {
        rx        = ScriptParams[3].fParam;
        ry        = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }

    bool result = false;
    if (!notStopped) {
        const bool inVehicle = ped->bInVehicle;
        if (!(x - rx > pos.x || x + rx < pos.x || y - ry > pos.y || y + ry < pos.y)
            && (!is3D || !(z - rz > pos.z || z + rz < pos.z))) {
            switch (commandId) {
            case 0xEC:
            case 0xEF:
            case 0xFE:
            case 0x101: // Any means
                result = true;
                break;
            case 0xED:
            case 0xF0:
            case 0xFF:
            case 0x102: // On foot
                result = !inVehicle;
                break;
            case 0xEE:
            case 0xF1:
            case 0x100:
            case 0x103: // In car
                result = inVehicle;
                break;
            default:
                break;
            }
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ x - rx, y - ry }, CVector2D{ x + rx, y + ry }, is3D ? z : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(x - rx, y - ry, x + rx, y + ry);
    }
}

// 0x487D10
void CRunningScript::LocateObjectCommand(int32 commandId) {
    const bool is3D = commandId == 0x4E6;
    CollectParameters(is3D ? 8 : 6);

    const float x = ScriptParams[1].fParam;
    const float y = ScriptParams[2].fParam;

    auto* const obj = GetObjectPool()->GetAtRef(ScriptParams[0].iParam);
    assert(obj);

    float z = 0.f, rx, ry, rz = 0.f;
    int32 highlight;
    if (is3D) {
        z         = ScriptParams[3].fParam;
        rx        = ScriptParams[4].fParam;
        ry        = ScriptParams[5].fParam;
        rz        = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
    } else {
        rx        = ScriptParams[3].fParam;
        ry        = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }

    const auto& pos = obj->GetPosition();
    bool result = false;
    if (!(pos.x < x - rx || x + rx < pos.x || y - ry > pos.y || y + ry < pos.y)
        && (!is3D || !(z - rz > pos.z || z + rz < pos.z))) {
        result = true;
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ x - rx, y - ry }, CVector2D{ x + rx, y + ry }, is3D ? z : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(x - rx, y - ry, x + rx, y + ry);
    }
}

// 0x487420
void CRunningScript::LocateCharCarCommand(int32 commandId) {
    const bool is3D = commandId >= 0x205 && commandId <= 0x207;
    CollectParameters(is3D ? 6 : 5);

    auto* const ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    auto* const veh = GetVehiclePool()->GetAtRef(ScriptParams[1].iParam);
    assert(ped && veh);

    const float rx = ScriptParams[2].fParam;
    const float ry = ScriptParams[3].fParam;
    float       rz = 0.f;
    int32       highlight;
    if (is3D) {
        rz        = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    } else {
        highlight = ScriptParams[4].iParam;
    }

    const bool  inVehicle = ped->bInVehicle;
    const auto  pedPos    = ped->GetRealPosition();
    const auto& vehPos    = veh->GetPosition();

    const float minX = vehPos.x - rx;
    bool        result = false;
    if (!(pedPos.x < minX || vehPos.x + rx < pedPos.x || vehPos.y - ry > pedPos.y || vehPos.y + ry < pedPos.y)
        && (!is3D || !(vehPos.z - rz > pedPos.z || vehPos.z + rz < pedPos.z))) {
        switch (commandId) {
        case 0x202:
        case 0x205: // Any means
            result = true;
            break;
        case 0x203:
        case 0x206: // On foot
            result = !inVehicle;
            break;
        case 0x204:
        case 0x207: // In car
            result = inVehicle;
            break;
        default:
            break;
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ minX, vehPos.y - ry }, CVector2D{ vehPos.x + rx, vehPos.y + ry }, is3D ? vehPos.z : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(minX, vehPos.y - ry, vehPos.x + rx, vehPos.y + ry);
    }
}

// 0x4870F0
void CRunningScript::LocateCharCharCommand(int32 commandId) {
    const bool is3D = commandId >= 0x104 && commandId <= 0x106;
    CollectParameters(is3D ? 6 : 5);

    auto* const ped1 = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    auto* const ped2 = GetPedPool()->GetAtRef(ScriptParams[1].iParam);
    assert(ped1 && ped2);

    const float rx = ScriptParams[2].fParam;
    const float ry = ScriptParams[3].fParam;
    float       rz = 0.f;
    int32       highlight;
    if (is3D) {
        rz        = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    } else {
        highlight = ScriptParams[4].iParam;
    }

    const bool inVehicle = ped1->bInVehicle;
    const auto pos1      = ped1->GetRealPosition();
    const auto pos2      = ped2->GetRealPosition();

    const float minX = pos2.x - rx;
    bool        result = false;
    if (!(pos1.x < minX || pos2.x + rx < pos1.x || pos2.y - ry > pos1.y || pos2.y + ry < pos1.y)
        && (!is3D || !(pos2.z - rz > pos1.z || pos2.z + rz < pos1.z))) {
        switch (commandId) {
        case 0xF2:
        case 0x104: // Any means
            result = true;
            break;
        case 0xF3:
        case 0x105: // On foot
            result = !inVehicle;
            break;
        case 0xF4:
        case 0x106: // In car
            result = inVehicle;
            break;
        default:
            break;
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ minX, pos2.y - ry }, CVector2D{ pos2.x + rx, pos2.y + ry }, is3D ? pos2.z : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(minX, pos2.y - ry, pos2.x + rx, pos2.y + ry);
    }
}

// 0x487720
void CRunningScript::LocateCharObjectCommand(int32 commandId) {
    const bool is3D = commandId >= 0x474 && commandId <= 0x476;
    CollectParameters(is3D ? 6 : 5);

    auto* const ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    auto* const obj = GetObjectPool()->GetAtRef(ScriptParams[1].iParam);
    assert(ped && obj);

    const float rx = ScriptParams[2].fParam;
    const float ry = ScriptParams[3].fParam;
    float       rz = 0.f;
    int32       highlight;
    if (is3D) {
        rz        = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    } else {
        highlight = ScriptParams[4].iParam;
    }

    const bool  inVehicle = ped->bInVehicle;
    const auto  pedPos    = ped->GetRealPosition();
    const auto& objPos    = obj->GetPosition();

    const float minX = objPos.x - rx;
    bool        result = false;
    if (!(pedPos.x < minX || objPos.x + rx < pedPos.x || objPos.y - ry > pedPos.y || objPos.y + ry < pedPos.y)
        && (!is3D || !(objPos.z - rz > pedPos.z || objPos.z + rz < pedPos.z))) {
        switch (commandId) {
        case 0x471:
        case 0x474: // Any means
            result = true;
            break;
        case 0x472:
        case 0x475: // On foot
            result = !inVehicle;
            break;
        case 0x473:
        case 0x476: // In car
            result = inVehicle;
            break;
        default:
            break;
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ minX, objPos.y - ry }, CVector2D{ objPos.x + rx, objPos.y + ry }, is3D ? objPos.z : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(minX, objPos.y - ry, objPos.x + rx, objPos.y + ry);
    }
}

// 0x488EC0
void CRunningScript::CarInAreaCheckCommand(int32 commandId) {
    const bool is3D = commandId == 0xB1 || commandId == 0x1AC;
    CollectParameters(is3D ? 8 : 6);

    auto* const veh = GetVehiclePool()->GetAtRef(ScriptParams[0].iParam);
    assert(veh);

    bool notStopped = false;
    if (commandId > 0x1AA && commandId < 0x1AD) { // Stopped variants
        if (!CTheScripts::IsVehicleStopped(veh)) {
            notStopped = true;
        }
    }

    float minX = ScriptParams[1].fParam;
    float minY = ScriptParams[2].fParam;
    float maxX, maxY;
    float minZ = 0.f, maxZ = 0.f;
    int32 highlight;
    if (is3D) {
        minZ      = ScriptParams[3].fParam;
        maxX      = ScriptParams[4].fParam;
        maxY      = ScriptParams[5].fParam;
        maxZ      = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
        if (maxZ < minZ) {
            std::swap(minZ, maxZ);
        }
    } else {
        maxX      = ScriptParams[3].fParam;
        maxY      = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }
    if (maxX < minX) {
        std::swap(minX, maxX);
    }
    if (maxY < minY) {
        std::swap(minY, maxY);
    }

    bool result = false;
    if (!notStopped) {
        const auto& pos = veh->GetPosition();
        if (!(pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY)
            && (!is3D || !(pos.z < minZ || pos.z > maxZ))) {
            result = true;
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ minX, minY }, CVector2D{ maxX, maxY }, is3D ? (maxZ + minZ) * 0.5f : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(minX, minY, maxX, maxY);
    }
}

// 0x488B50
void CRunningScript::CharInAreaCheckCommand(int32 commandId) {
    const bool is3D = commandId == 0xA4 || (commandId > 0x1A5 && commandId < 0x1AB);
    CollectParameters(is3D ? 8 : 6);

    auto* const ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    assert(ped);

    const bool inVehicle = ped->bInVehicle;
    const auto pos       = ped->GetRealPosition();

    bool notStopped = false;
    switch (commandId) {
    case 0x1A3:
    case 0x1A4:
    case 0x1A5:
    case 0x1A8:
    case 0x1A9:
    case 0x1AA: // Stopped variants
        if (!CTheScripts::IsPedStopped(ped)) {
            notStopped = true;
        }
        break;
    default:
        break;
    }

    float minX = ScriptParams[1].fParam;
    float minY = ScriptParams[2].fParam;
    float maxX, maxY;
    float minZ = 0.f, maxZ = 0.f;
    int32 highlight;
    if (is3D) {
        minZ      = ScriptParams[3].fParam;
        maxX      = ScriptParams[4].fParam;
        maxY      = ScriptParams[5].fParam;
        maxZ      = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
        if (maxZ < minZ) {
            std::swap(minZ, maxZ);
        }
    } else {
        maxX      = ScriptParams[3].fParam;
        maxY      = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }
    if (maxX < minX) {
        std::swap(minX, maxX);
    }
    if (maxY < minY) {
        std::swap(minY, maxY);
    }

    bool result = false;
    if (!notStopped) {
        if (!(pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY)
            && (!is3D || !(pos.z < minZ || pos.z > maxZ))) {
            switch (commandId) {
            case 0xA3:
            case 0xA4:
            case 0x1A3:
            case 0x1A8: // Any means
                result = true;
                break;
            case 0x1A1:
            case 0x1A4:
            case 0x1A6:
            case 0x1A9: // On foot
                result = !inVehicle;
                break;
            case 0x1A2:
            case 0x1A5:
            case 0x1A7:
            case 0x1AA: // In car
                result = inVehicle;
                break;
            default:
                break;
            }
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ minX, minY }, CVector2D{ maxX, maxY }, is3D ? (maxZ + minZ) * 0.5f : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(minX, minY, maxX, maxY);
    }
}

// 0x489150
void CRunningScript::ObjectInAreaCheckCommand(int32 commandId) {
    const bool is3D = commandId == 0x4EA;
    CollectParameters(is3D ? 8 : 6);

    auto* const obj = GetObjectPool()->GetAtRef(ScriptParams[0].iParam);
    assert(obj);

    float minX = ScriptParams[1].fParam;
    float minY = ScriptParams[2].fParam;
    float maxX, maxY;
    float minZ = 0.f, maxZ = 0.f;
    int32 highlight;
    if (is3D) {
        minZ      = ScriptParams[3].fParam;
        maxX      = ScriptParams[4].fParam;
        maxY      = ScriptParams[5].fParam;
        maxZ      = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
        if (maxZ < minZ) {
            std::swap(minZ, maxZ);
        }
    } else {
        maxX      = ScriptParams[3].fParam;
        maxY      = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }
    if (maxX < minX) {
        std::swap(minX, maxX);
    }
    if (maxY < minY) {
        std::swap(minY, maxY);
    }

    const auto& pos = obj->GetPosition();
    bool result = false;
    if (!(pos.x < minX || pos.x > maxX || pos.y < minY || pos.y > maxY)
        && (!is3D || !(pos.z < minZ || pos.z > maxZ))) {
        result = true;
    }
    UpdateCompareFlag(result);

    if (highlight) {
        HighlightImportantArea(CVector2D{ minX, minY }, CVector2D{ maxX, maxY }, is3D ? (maxZ + minZ) * 0.5f : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugSquare(minX, minY, maxX, maxY);
    }
}

// 0x487F60
void CRunningScript::CharInAngledAreaCheckCommand(int32 commandId) {
    const bool is3D = commandId >= 0x5FC && commandId <= 0x601;
    CollectParameters(is3D ? 9 : 7);

    auto* const ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam);
    assert(ped);

    bool notStopped = false;
    switch (commandId) {
    case 0x5F9:
    case 0x5FA:
    case 0x5FB:
    case 0x5FF:
    case 0x600:
    case 0x601: // Stopped variants
        if (!CTheScripts::IsPedStopped(ped)) {
            notStopped = true;
        }
        break;
    default:
        break;
    }

    const float x1 = ScriptParams[1].fParam;
    const float y1 = ScriptParams[2].fParam;
    float       x2, y2, width;
    float       minZ = 0.f, maxZ = 0.f;
    int32       highlight;
    if (is3D) {
        minZ      = ScriptParams[3].fParam;
        x2        = ScriptParams[4].fParam;
        y2        = ScriptParams[5].fParam;
        maxZ      = ScriptParams[6].fParam;
        width     = ScriptParams[7].fParam;
        highlight = ScriptParams[8].iParam;
        if (maxZ < minZ) {
            std::swap(minZ, maxZ);
        }
    } else {
        x2        = ScriptParams[3].fParam;
        y2        = ScriptParams[4].fParam;
        width     = ScriptParams[5].fParam;
        highlight = ScriptParams[6].iParam;
    }

    CVector2D c, d;
    CalculateAngledAreaCorners(x1, y1, x2, y2, width, c, d);

    bool result = false;
    if (!notStopped) {
        const bool inVehicle = ped->bInVehicle;
        const auto pos       = ped->GetRealPosition();
        if (IsPointInAngledArea2D(pos.x, pos.y, x1, y1, x2, y2, d)
            && (!is3D || (pos.z >= minZ && pos.z <= maxZ))) {
            switch (commandId) {
            case 0x5F7:
            case 0x5FA:
            case 0x5FD:
            case 0x600: // On foot
                result = !inVehicle;
                break;
            case 0x5F8:
            case 0x5FB:
            case 0x5FE:
            case 0x601: // In car
                result = inVehicle;
                break;
            case 0x5F6:
            case 0x5F9:
            case 0x5FC:
            case 0x5FF: // Any means
                result = true;
                break;
            default:
                break;
            }
        }
    }
    UpdateCompareFlag(result);

    if (highlight) {
        CTheScripts::HighlightImportantAngledArea(GetHighlightId(this), x1, y1, x2, y2, c.x, c.y, d.x, d.y, is3D ? (maxZ + minZ) * 0.5f : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugAngledSquare({ x1, y1 }, { x2, y2 }, c, d);
    }
}

// 0x488780
void CRunningScript::FlameInAngledAreaCheckCommand(int32 commandId) {
    const bool is3D = commandId == 0x72E;
    CollectParameters(is3D ? 8 : 6);

    const float x1 = ScriptParams[0].fParam;
    const float y1 = ScriptParams[1].fParam;
    float       x2, y2, width;
    float       minZ = 0.f, maxZ = 0.f;
    int32       highlight;
    if (is3D) {
        minZ      = ScriptParams[2].fParam;
        x2        = ScriptParams[3].fParam;
        y2        = ScriptParams[4].fParam;
        maxZ      = ScriptParams[5].fParam;
        width     = ScriptParams[6].fParam;
        highlight = ScriptParams[7].iParam;
        if (maxZ < minZ) {
            std::swap(minZ, maxZ);
        }
    } else {
        x2        = ScriptParams[2].fParam;
        y2        = ScriptParams[3].fParam;
        width     = ScriptParams[4].fParam;
        highlight = ScriptParams[5].iParam;
    }

    CVector2D c, d;
    CalculateAngledAreaCorners(x1, y1, x2, y2, width, c, d);

    CVector2D   ab{ x2 - x1, y2 - y1 };
    CVector2D   ad{ d.x - x1, d.y - y1 };
    const float abLen = std::sqrt(ab.x * ab.x + ab.y * ab.y);
    const float adLen = std::sqrt(ad.x * ad.x + ad.y * ad.y);

    bool result = false;
    for (uint16 i = 0; !result && i < 100u; i++) {
        CVector shotPos;
        if (!CShotInfo::GetFlameThrowerShotPosn((uint8)i, shotPos)) {
            continue;
        }

        ab.Normalise(); // NOTSA: Done in-place (like in the original), so it's re-normalised each iteration
        const float dotAB = (shotPos.x - x1) * ab.x + (shotPos.y - y1) * ab.y;
        if (dotAB < 0.f || dotAB > abLen) {
            continue;
        }

        ad.Normalise(); // Same as above
        const float dotAD = (shotPos.x - x1) * ad.x + (shotPos.y - y1) * ad.y;
        if (dotAD < 0.f || dotAD > adLen) {
            continue;
        }

        if (is3D && (minZ > shotPos.z || shotPos.z > maxZ)) {
            continue;
        }

        result = true;
    }
    UpdateCompareFlag(result);

    if (highlight) {
        CTheScripts::HighlightImportantAngledArea(GetHighlightId(this), x1, y1, x2, y2, c.x, c.y, d.x, d.y, is3D ? (maxZ + minZ) * 0.5f : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugAngledSquare({ x1, y1 }, { x2, y2 }, c, d);
    }
}

// 0x4883F0
void CRunningScript::ObjectInAngledAreaCheckCommand(int32 commandId) {
    const bool is3D = commandId == 0x8E4;
    CollectParameters(is3D ? 9 : 7);

    const float x1 = ScriptParams[1].fParam;
    const float y1 = ScriptParams[2].fParam;

    auto* const obj = GetObjectPool()->GetAtRef(ScriptParams[0].iParam);
    assert(obj);

    float x2, y2, width;
    float minZ = 0.f, maxZ = 0.f;
    int32 highlight;
    if (is3D) {
        minZ      = ScriptParams[3].fParam;
        x2        = ScriptParams[4].fParam;
        y2        = ScriptParams[5].fParam;
        maxZ      = ScriptParams[6].fParam;
        width     = ScriptParams[7].fParam;
        highlight = ScriptParams[8].iParam;
        if (maxZ < minZ) {
            std::swap(minZ, maxZ);
        }
    } else {
        x2        = ScriptParams[3].fParam;
        y2        = ScriptParams[4].fParam;
        width     = ScriptParams[5].fParam;
        highlight = ScriptParams[6].iParam;
    }

    CVector2D c, d;
    CalculateAngledAreaCorners(x1, y1, x2, y2, width, c, d);

    const auto& pos    = obj->GetPosition();
    bool        result = false;
    if (IsPointInAngledArea2D(pos.x, pos.y, x1, y1, x2, y2, d)
        && (!is3D || (pos.z >= minZ && pos.z <= maxZ))) {
        result = true;
    }
    UpdateCompareFlag(result);

    if (highlight) {
        CTheScripts::HighlightImportantAngledArea(GetHighlightId(this), x1, y1, x2, y2, c.x, c.y, d.x, d.y, is3D ? (maxZ + minZ) * 0.5f : -100.f);
    }
    if (CTheScripts::DbgFlag && !is3D) {
        CTheScripts::DrawDebugAngledSquare({ x1, y1 }, { x2, y2 }, c, d);
    }
}

// 0x464D70
bool CRunningScript::IsPedDead(CPed* ped) const {
    ePedState pedState = ped->m_nPedState;
    return pedState == PEDSTATE_DEAD || pedState == PEDSTATE_DIE || pedState == PEDSTATE_DIE_BY_STEALTH;
}

// 0x489490
bool CRunningScript::ThisIsAValidRandomPed(ePedType pedType, bool civilian, bool gang, bool criminal) {
    switch (pedType) {
    case PED_TYPE_CIVMALE:
    case PED_TYPE_CIVFEMALE:
        return civilian;
    case PED_TYPE_GANG1:
    case PED_TYPE_GANG2:
    case PED_TYPE_GANG3:
    case PED_TYPE_GANG4:
    case PED_TYPE_GANG5:
    case PED_TYPE_GANG6:
    case PED_TYPE_GANG7:
    case PED_TYPE_GANG8:
    case PED_TYPE_GANG9:
        return gang;
    case PED_TYPE_CRIMINAL:
    case PED_TYPE_PROSTITUTE:
        return criminal;
    default:
        return false;
    }
}

// 0x485A50
void CRunningScript::DoDeathArrestCheck() {
    if (!m_IsDeathArrestCheckEnabled) {
        return;
    }

    if (!CTheScripts::IsPlayerOnAMission()) {
        return;
    }

    if (const auto& pi = FindPlayerInfo(); !pi.IsRestartingAfterDeath() && !pi.IsRestartingAfterArrest()) {
        return;
    }

    CMessages::ClearSmallMessagesOnly();
    memset(&CTheScripts::ScriptSpace[CTheScripts::OnAMissionFlag], 0, sizeof(uint32));
    ResetIP();

    m_DoneDeathArrest = true;
    m_WakeTime        = 0;
}

// 0x464F50
void CRunningScript::GetCorrectPedModelIndexForEmergencyServiceType(ePedType pedType, uint32* typeSpecificModelId) {
    switch (*typeSpecificModelId) {
    case MODEL_LAPD1:
    case MODEL_SFPD1:
    case MODEL_LVPD1:
    case MODEL_LAPDM1:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_CITYCOP;
        }
        break;
    case MODEL_CSHER:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_CSHER;
        }
        break;
    case MODEL_SWAT:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_SWAT1;
        }
        break;
    case MODEL_FBI:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_FBI;
        }
        break;
    case MODEL_ARMY:
        if (pedType == PED_TYPE_COP) {
            *typeSpecificModelId = COP_TYPE_ARMY;
        }
        break;
    default:
        return;
    }
}

// Returns state of pad button
// 0x485B10
int16 CRunningScript::GetPadState(uint16 playerIndex, eButtonId buttonId) {
    const auto* pad = CPad::GetPad(playerIndex);
    switch (buttonId) {
    case BUTTON_LEFT_STICK_X:    return pad->NewState.LeftStickX;
    case BUTTON_LEFT_STICK_Y:    return pad->NewState.LeftStickY;
    case BUTTON_RIGHT_STICK_X:   return pad->NewState.RightStickX;
    case BUTTON_RIGHT_STICK_Y:   return pad->NewState.RightStickY;
    case BUTTON_LEFT_SHOULDER1:  return pad->NewState.LeftShoulder1;
    case BUTTON_LEFT_SHOULDER2:  return pad->NewState.LeftShoulder2;
    case BUTTON_RIGHT_SHOULDER1: return pad->NewState.RightShoulder1;
    case BUTTON_RIGHT_SHOULDER2: return pad->NewState.RightShoulder2;
    case BUTTON_DPAD_UP:         return pad->NewState.DPadUp;
    case BUTTON_DPAD_DOWN:       return pad->NewState.DPadDown;
    case BUTTON_DPAD_LEFT:       return pad->NewState.DPadLeft;
    case BUTTON_DPAD_RIGHT:      return pad->NewState.DPadRight;
    case BUTTON_START:           return pad->NewState.Start;
    case BUTTON_SELECT:          return pad->NewState.Select;
    case BUTTON_SQUARE:          return pad->NewState.ButtonSquare;
    case BUTTON_TRIANGLE:        return pad->NewState.ButtonTriangle;
    case BUTTON_CROSS:           return pad->NewState.ButtonCross;
    case BUTTON_CIRCLE:          return pad->NewState.ButtonCircle;
    case BUTTON_LEFTSHOCK:       return pad->NewState.ShockButtonL;
    case BUTTON_RIGHTSHOCK:      return pad->NewState.ShockButtonR;
    default:                     return OR_CONTINUE;
    }
}

// 0x46AF50
void CRunningScript::ScriptTaskPickUpObject(int32 commandId) {
    CollectParameters(7);

    const auto  pedHandle = ScriptParams[0].iParam;
    auto* const obj       = GetObjectPool()->GetAtRef(ScriptParams[1].iParam);
    const CVector offset{ ScriptParams[2].fParam, ScriptParams[3].fParam, ScriptParams[4].fParam };
    const auto  boneFrameId = ScriptParams[5].u8Param;
    const auto  boneFlags   = ScriptParams[6].u8Param;

    char animName[24];
    char animBlock[16];
    ReadTextLabelFromScript(animName, sizeof(animName));
    ReadTextLabelFromScript(animBlock, sizeof(animBlock));

    const bool noAnim = strncmp(animName, "NULL", 5) == 0 || strncmp(animBlock, "NULL", 5) == 0;

    CollectParameters(1);
    const auto animFlags = static_cast<eAnimationFlags>(ScriptParams[0].iParam == 0 ? 0x18 : 0x10);

    CTask* task;
    if (noAnim) {
        task = new CTaskSimpleHoldEntity(obj, &offset, boneFrameId, boneFlags, ANIM_ID_NO_ANIMATION_SET, ANIM_GROUP_DEFAULT, false);
    } else {
        task = new CTaskSimpleHoldEntity(obj, &offset, boneFrameId, boneFlags, animName, animBlock, animFlags);
    }

    if (pedHandle == -1) {
        if (commandId == 0x70A) {
            task = new CTaskSimpleAffectSecondaryBehaviour(true, TASK_SECONDARY_PARTIAL_ANIM, task);
            CTaskSequences::AddTaskToActiveSequence(task);
        }
    } else {
        auto* const ped = GetPedPool()->GetAtRef(pedHandle);
        assert(ped);
        ped->GetTaskManager().SetTaskSecondary(task, commandId == 0x70A ? TASK_SECONDARY_PARTIAL_ANIM : TASK_SECONDARY_ATTACK);
        CPedScriptedTaskRecord::ms_scriptedTasks[CPedScriptedTaskRecord::GetVacantSlot()].Set(ped, commandId, task);
    }
}

// 0x464DC0
void CRunningScript::SetCharCoordinates(CPed& ped, CVector posn, bool warpGang, bool offset) {
    CWorld::PutToGroundIfTooLow(posn);

    CVehicle* vehicle = ped.GetVehicleIfInOne();
    if (vehicle) {
        posn.z += vehicle->GetDistanceFromCentreOfMassToBaseOfModel();
        vehicle->Teleport(posn, false);
        CTheScripts::ClearSpaceForMissionEntity(posn, vehicle);
    } else {
        posn.z += offset ? ped.GetDistanceFromCentreOfMassToBaseOfModel() : 0.0f;
        CTheScripts::ClearSpaceForMissionEntity(posn, &ped);
        auto* group = CPedGroups::GetPedsGroup(&ped);
        if (group && group->GetMembership().IsLeader(&ped) && warpGang) {
            group->Teleport(posn);
        } else {
            ped.Teleport(posn, false);
        }
    }
}

// 0x463CA0
tScriptParam* CRunningScript::GetPointerToLocalVariable(int32 loc) {
    return &GetLocal<tScriptParam>(loc);
}

/*!
 * @addr 0x463CC0
 * @brief Returns pointer to a local script variable.
 *
 * @param arrayBaseOffset          The offset of the array
 * @param index                    Index of the variable inside the array
 * @param arrayEntriesSizeAsParams Size of 1 variable in the array (In terms of `tScriptParam`'s - So for a regular `int` (or float, etc) variable this will be `1`, for long strings it's `4` and for short one's it's `2`)
 */
tScriptParam* CRunningScript::GetPointerToLocalArrayElement(int32 arrayBaseOffset, uint16 index, uint8 arrayEntriesSizeAsParams) {
    return &GetArrayLocal<tScriptParam>(arrayBaseOffset, index, arrayEntriesSizeAsParams);
}

/*!
 * Returns pointer to script variable of any type.
 * @addr 0x464790
 */
tScriptParam* CRunningScript::GetPointerToScriptVariable(eScriptVariableType) {
    uint8  arrElemSize;
    uint16 arrVarOffset;
    int32  arrElemIdx;

    int8 type = CTheScripts::Read1ByteFromScript(m_IP);
    switch (type) {
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE:
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[index]);
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE:
    case SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        return GetPointerToLocalVariable(index);
    }

    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY)
            return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[LONG_STRING_SIZE * arrElemIdx + arrVarOffset]);
        else if (type == SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY)
            return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[SHORT_STRING_SIZE * arrElemIdx + arrVarOffset]);
        else // SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY
            return reinterpret_cast<tScriptParam*>(&CTheScripts::ScriptSpace[4 * arrElemIdx + arrVarOffset]);

    case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
    case SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY)
            arrElemSize = 4;
        else if (type == SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY)
            arrElemSize = 2;
        else // SCRIPT_PARAM_LOCAL_NUMBER_ARRAY
            arrElemSize = 1;
        return GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, arrElemSize);

    default:
        NOTSA_UNREACHABLE();
    }
}

/*!
 * Returns offset of global variable
 * @addr 0x464700
 */
uint16 CRunningScript::GetIndexOfGlobalVariable() {
    switch (const auto t = GetAtIPAs<uint8>()) {
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        return GetAtIPAs<uint16>();
    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY: {
        uint16 base;
        int32  idx;
        ReadArrayInformation(true, &base, &idx);
        return base + sizeof(tScriptParam) * idx;
    }
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x464080
void CRunningScript::CollectParameters(int16 count) {
    uint16 arrVarOffset;
    int32  arrElemIdx;

    for (auto i = 0; i < count; i++) {
        switch (CTheScripts::Read1ByteFromScript(m_IP)) {
        case SCRIPT_PARAM_STATIC_INT_32BITS:
            ScriptParams[i].iParam = CTheScripts::Read4BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            ScriptParams[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            ScriptParams[i] = *GetPointerToLocalVariable(index);
            break;
        }
        case SCRIPT_PARAM_STATIC_INT_8BITS:
            ScriptParams[i].iParam = CTheScripts::Read1ByteFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_INT_16BITS:
            ScriptParams[i].iParam = CTheScripts::Read2BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_FLOAT:
            ScriptParams[i].fParam = CTheScripts::ReadFloatFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            ScriptParams[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]);
            break;
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            ScriptParams[i] = *GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1);
            break;
        }
    }
}

/*!
 * Collects parameter and returns it.
 * @addr 0x464250
 */
int32 CRunningScript::CollectNextParameterWithoutIncreasingPC() {
    uint16 arrVarOffset;
    int32  arrElemIdx;
    uint8* ip = m_IP;
    int32  result = -1;

    switch (CTheScripts::Read1ByteFromScript(m_IP)) {
    case SCRIPT_PARAM_STATIC_INT_32BITS:
    case SCRIPT_PARAM_STATIC_FLOAT:
        result = CTheScripts::Read4BytesFromScript(m_IP);
        break;
    case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        result = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]);
        break;
    }
    case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        result = GetPointerToLocalVariable(index)->iParam;
        break;
    }
    case SCRIPT_PARAM_STATIC_INT_8BITS:
        result = CTheScripts::Read1ByteFromScript(m_IP);
        break;
    case SCRIPT_PARAM_STATIC_INT_16BITS:
        result = CTheScripts::Read2BytesFromScript(m_IP);
        break;
    case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
        ReadArrayInformation(false, &arrVarOffset, &arrElemIdx);
        result = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]);
        break;
    case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
        ReadArrayInformation(false, &arrVarOffset, &arrElemIdx);
        result = GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1)->iParam;
        break;
    }

    m_IP = ip;
    return result;
}

/*!
 * @addr 0x464370
 */
void CRunningScript::StoreParameters(int16 count) {
    uint16 arrVarOffset;
    int32  arrElemIdx;

    for (auto i = 0; i < count; i++) {
        switch (CTheScripts::Read1ByteFromScript(m_IP)) {
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]) = ScriptParams[i].iParam;
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            *GetPointerToLocalVariable(index) = ScriptParams[i];
            break;
        }
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]) = ScriptParams[i].iParam;
            break;
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            *GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1) = ScriptParams[i];
            break;
        }
    }
}

// Reads array var base offset and element index from index variable.
// 0x463CF0
void CRunningScript::ReadArrayInformation(int32 updateIP, uint16* outArrayBase, int32* outArrayIndex) {
    const auto op = GetAtIPAs<scm::ArrayAccess>(updateIP);
    *outArrayIndex = op.IdxVarIsGlobal
        ? GetGlobal<int32>(op.IdxVarLoc)
        : GetLocal<int32>(op.IdxVarLoc);
    *outArrayBase = op.ArrayBase;
}

// Collects parameters and puts them to local variables of new script
// 0x464500
void CRunningScript::ReadParametersForNewlyStartedScript(CRunningScript* newScript) {
    uint16 arrVarOffset;
    int32  arrElemIdx;
    int8   type = CTheScripts::Read1ByteFromScript(m_IP);

    for (int i = 0; type != SCRIPT_PARAM_END_OF_ARGUMENTS; type = CTheScripts::Read1ByteFromScript(m_IP), i++) {
        switch (type) {
        case SCRIPT_PARAM_STATIC_INT_32BITS:
            newScript->m_LocalVars[i].iParam = CTheScripts::Read4BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            newScript->m_LocalVars[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[index]);
            break;
        }
        case SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE:
        {
            uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
            newScript->m_LocalVars[i] = *GetPointerToLocalVariable(index);
            break;
        }
        case SCRIPT_PARAM_STATIC_INT_8BITS:
            newScript->m_LocalVars[i].iParam = CTheScripts::Read1ByteFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_INT_16BITS:
            newScript->m_LocalVars[i].iParam = CTheScripts::Read2BytesFromScript(m_IP);
            break;
        case SCRIPT_PARAM_STATIC_FLOAT:
            newScript->m_LocalVars[i].fParam = CTheScripts::ReadFloatFromScript(m_IP);
            break;
        case SCRIPT_PARAM_GLOBAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            newScript->m_LocalVars[i].iParam = *reinterpret_cast<int32*>(&CTheScripts::ScriptSpace[arrVarOffset + 4 * arrElemIdx]);
            break;
        case SCRIPT_PARAM_LOCAL_NUMBER_ARRAY:
            ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
            newScript->m_LocalVars[i] = *GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 1);
            break;
        default:
            break;
        }
    }
}

// Collects string parameter
// 0x463D50
void CRunningScript::ReadTextLabelFromScript(char* buffer, uint8 nBufferLength) {
    uint16 arrVarOffset;
    int32  arrElemIdx;

    int8 type = CTheScripts::Read1ByteFromScript(m_IP);
    switch (type) {
    case SCRIPT_PARAM_STATIC_SHORT_STRING:
        for (auto i = 0; i < SHORT_STRING_SIZE; i++)
            buffer[i] = CTheScripts::Read1ByteFromScript(m_IP);
        break;

    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, SHORT_STRING_SIZE, (char*)&CTheScripts::ScriptSpace[index], SHORT_STRING_SIZE);
        break;
    }

    case SCRIPT_PARAM_LOCAL_SHORT_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, SHORT_STRING_SIZE, (char*) GetPointerToLocalVariable(index), SHORT_STRING_SIZE);
        break;
    }

    case SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_GLOBAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_GLOBAL_SHORT_STRING_ARRAY)
            strncpy_s(buffer, SHORT_STRING_SIZE, (char*) & CTheScripts::ScriptSpace[SHORT_STRING_SIZE * arrElemIdx + arrVarOffset], SHORT_STRING_SIZE);
        else
            strncpy_s(buffer, SHORT_STRING_SIZE, (char*) & CTheScripts::ScriptSpace[LONG_STRING_SIZE * arrElemIdx + arrVarOffset], std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        break;

    case SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY:
    case SCRIPT_PARAM_LOCAL_LONG_STRING_ARRAY:
        ReadArrayInformation(true, &arrVarOffset, &arrElemIdx);
        if (type == SCRIPT_PARAM_LOCAL_SHORT_STRING_ARRAY)
            strncpy_s(buffer, SHORT_STRING_SIZE, (char*) GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 2), SHORT_STRING_SIZE);
        else {
            const auto bufferLength = std::min<uint8>(nBufferLength, LONG_STRING_SIZE);
            strncpy_s(buffer, bufferLength, (char*)GetPointerToLocalArrayElement(arrVarOffset, arrElemIdx, 4), bufferLength);
        }
        break;

    case SCRIPT_PARAM_STATIC_PASCAL_STRING:
    {
        int16 nStringLen = CTheScripts::Read1ByteFromScript(m_IP); // sign extension. max size = 127, not 255
        for (auto i = 0; i < nStringLen; i++)
            buffer[i] = CTheScripts::Read1ByteFromScript(m_IP);

        if (nStringLen < nBufferLength)
            memset(&buffer[(uint8)nStringLen], 0, (uint8)(nBufferLength - nStringLen));
        break;
    }

    case SCRIPT_PARAM_STATIC_LONG_STRING:
        // slightly changed code: original code is a bit messy and calls Read1ByteFromScript
        // in a loop and does some additional checks to ensure that buffer can hold the data
        strncpy_s(buffer, LONG_STRING_SIZE, (char*)m_IP, std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        m_IP += LONG_STRING_SIZE;
        break;

    case SCRIPT_PARAM_GLOBAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, LONG_STRING_SIZE, (char*) & CTheScripts::ScriptSpace[index], std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        break;
    }

    case SCRIPT_PARAM_LOCAL_LONG_STRING_VARIABLE:
    {
        uint16 index = CTheScripts::Read2BytesFromScript(m_IP);
        strncpy_s(buffer, LONG_STRING_SIZE, (char*) GetPointerToLocalVariable(index), std::min<uint8>(nBufferLength, LONG_STRING_SIZE));
        break;
    }

    default:
        break;
    }
}

// Updates comparement flag, used in conditional commands
// 0x4859D0
void CRunningScript::UpdateCompareFlag(bool state) {
    if (m_NotFlag)
        state = !state;

    if (m_AndOrState == ANDOR_NONE) {
        m_CondResult = state;
        return;
    }

    if (m_AndOrState >= ANDS_1 && m_AndOrState <= ANDS_8) {
        m_CondResult &= state;
        if (m_AndOrState == ANDS_1)
            m_AndOrState = ANDOR_NONE;
        else
            m_AndOrState--;

        return;
    }

    if (m_AndOrState >= ORS_1 && m_AndOrState <= ORS_8) {
        m_CondResult |= state;
        if (m_AndOrState == ORS_1)
            m_AndOrState = ANDOR_NONE;
        else
            m_AndOrState--;

        return;
    }
}

// Sets instruction pointer, used in GOTO-like commands
// 0x464DA0
void CRunningScript::UpdatePC(int32 newIP) {
    m_IP = newIP >= 0
        ? &CTheScripts::ScriptSpace[newIP]
        : m_BaseIP + std::abs(newIP);
}

// 0x469EB0, inlined
OpcodeResult CRunningScript::ProcessOneCommand() {
    ++CTheScripts::CommandsExecuted;

    const auto op = GetAtIPAs<scm::Instruction>();

    // Check if IP is valid pre-return
    notsa::ScopeGuard guardIP{[this]() {
        const auto next{ GetAtIPAs<scm::Instruction>(false) };
        VERIFY(next.Command <= COMMAND_HIGHEST_VANILLA_ID);
    }};

#ifdef NOTSA_SCRIPT_TRACING
    // snprintf is faster (in debug at least) - Gotta stick to it for now
    char msg[4096];
    sprintf_s(msg, "[%s][IP: 0x%X + 0x%X]: %s [0x%X]", m_szName, LOG_PTR(m_pBaseIP), LOG_PTR(m_IP - m_pBaseIP), notsa::script::GetScriptCommandName((eScriptCommands)op.Command).data(), (size_t)op.Command);
    SPDLOG_LOGGER_TRACE(logger, msg);
    //SPDLOG_LOGGER_TRACE(logger, "[{}][IP: {:#x} + {:#x}]: {} [{:#x}]", BaseFilename, LOG_PTR(m_pBaseIP), LOG_PTR(m_IP - m_pBaseIP), notsa::script::GetScriptCommandName((eScriptCommands)op.command), (size_t)op.command);
#endif
    
    m_NotFlag = op.NotFlag;

    if (const auto handler = CustomCommandHandlerOf((eScriptCommands)(op.Command))) {
        return std::invoke(handler, this);
    } else {
        return std::invoke(s_OriginalCommandHandlerTable[(size_t)op.Command / 100], this, (eScriptCommands)(op.Command));
    }
}

// 0x469F00
OpcodeResult CRunningScript::Process() {
    if (m_SceneSkipIP && CCutsceneMgr::IsCutsceneSkipButtonBeingPressed()) {
        CHud::m_BigMessage[1][0] = 0;
        UpdatePC(std::exchange(m_SceneSkipIP, 0));
        m_WakeTime = 0;
    }

    if (m_UsesMissionCleanup) {
        DoDeathArrestCheck();
    }

    if (m_ThisMustBeTheOnlyMissionRunning && CTheScripts::FailCurrentMission == 1) {
        if (m_StackDepth > 0) {
            ResetIP();
        }
    }

    CTheScripts::ReinitialiseSwitchStatementData();

    if (CTimer::GetTimeInMS() >= (uint32)m_WakeTime) {
        while (ProcessOneCommand() == OR_CONTINUE)
            ; // Process commands
    }

    return OR_CONTINUE;
}

void CRunningScript::HighlightImportantArea(CVector2D from, CVector2D to, float z) {
    CTheScripts::HighlightImportantArea(reinterpret_cast<int32>(this) + reinterpret_cast<int32>(m_IP), from.x, from.y, to.x, to.y, z);
}

void CRunningScript::HighlightImportantArea(CRect area, float z) {
    HighlightImportantArea(area.GetTopLeft(), area.GetBottomRight(), z);
}

void CRunningScript::HighlightImportantArea(CVector from, CVector to) {
    HighlightImportantArea(CVector2D{ from }, CVector2D{ to }, (from.z + to.z) / 2.f);
}

void CRunningScript::HighlightImportantAngledArea(uint32 id, CVector2D a, CVector2D b, CVector2D c, CVector2D d) {
    NOTSA_UNREACHABLE(); // Fuck this, we dont need it!
}

notsa::script::CommandHandlerFunction& CRunningScript::CustomCommandHandlerOf(eScriptCommands command) {
    return s_CustomCommandHandlerTable[(size_t)(command)];
}

void CRunningScript::ResetIP() {
    assert(m_StackDepth > 0); // Original bug...
    do {
        m_IP = std::exchange(m_IPStack[m_StackDepth--], nullptr); // NOTSA: Also clear the stack, we don't need it anymore
    } while (m_StackDepth);
}
