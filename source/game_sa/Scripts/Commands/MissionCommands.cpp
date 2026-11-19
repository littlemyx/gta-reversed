#include <StdInc.h>

#include "Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <FireManager.h>

/*!
* Various mission commands
*/

namespace {
void LoadAndLaunchMissionInternal(int32 missionId) {
    // Mostly CP from StartMission @ MissionDebugModule
    if (CTheScripts::NumberOfExclusiveMissionScripts > 0) {
        if (missionId <= (0xFFFF - 3)) {
            return;
        }
        missionId = 0xFFFF - missionId;
    }

    CTimer::Suspend();

    const auto StartScriptFromFile = [missionId](const char* filePath) {
        if (auto* file = CFileMgr::OpenFile(filePath, "rb")) {
            const int32 offsetToMission = CTheScripts::MultiScriptArray[missionId];

            CFileMgr::Seek(file, offsetToMission, 0);
            const auto bytesRead = CFileMgr::Read(file, &CTheScripts::MissionBlock[0], MISSION_SCRIPT_SIZE);
            CFileMgr::CloseFile(file);

            CTheScripts::WipeLocalVariableMemoryForMissionScript();
            CRunningScript* script = CTheScripts::StartNewScript(&CTheScripts::MissionBlock[0]);
            script->m_UsesMissionCleanup = true;
            script->m_ThisMustBeTheOnlyMissionRunning = true;
            script->m_BaseIP = &CTheScripts::MissionBlock[0];
            CTheScripts::bAlreadyRunningAMissionScript = true;
            CGameLogic::ClearSkip(false);

            return bytesRead;
        }
        return size_t{0};
    };

    if (CGame::bMissionPackGame) {
        CFileMgr::ChangeDir("\\");
        size_t bytesRead = 0;
        while (FrontEndMenuManager.CheckMissionPackValidMenu()) {
            CFileMgr::SetDirMyDocuments();
            sprintf_s(gString, "MPACK//MPACK%d//SCR.SCM", CGame::bMissionPackGame);
            if (StartScriptFromFile(gString) >= 1) {
                break;
            }
        }
    } else {
        CFileMgr::SetDir("");
        StartScriptFromFile("data\\script\\main.scm");
    }

    CTimer::Resume();
}

void ScriptName(CRunningScript* S, std::string_view name) {
    // exe 0x4846A1: ReadTextLabel(8), ASCII 'A'-'Z' += 0x20 over all 8 bytes, 8-byte copy into script+8 (no terminator)
    char lowered[9]{};
    for (size_t i = 0; i < 8 && i < name.size(); i++) {
        const char c = name[i];
        lowered[i] = (c >= 'A' && c <= 'Z') ? char(c + 0x20) : c;
    }
    S->SetName(lowered);
}

int32 StartScriptFire(CVector pos, int8 propagation, int32 size) {
    return gFireManager.StartScriptFire(pos, nullptr, 0.8f, 1, propagation, size);
}

void LaunchMission(uint32 label) {
    CTheScripts::StartNewScript(&CTheScripts::ScriptSpace[label]);
}
};

void notsa::script::commands::mission::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Mission");

    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_AND_LAUNCH_MISSION_INTERNAL, LoadAndLaunchMissionInternal);
    REGISTER_COMMAND_HANDLER(COMMAND_SCRIPT_NAME, ScriptName);
    REGISTER_COMMAND_HANDLER(COMMAND_START_SCRIPT_FIRE, StartScriptFire);
    REGISTER_COMMAND_HANDLER(COMMAND_LAUNCH_MISSION, LaunchMission);

    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_CREATE_CUTSCENE_OBJECT);
    REGISTER_COMMAND_UNIMPLEMENTED(COMMAND_SET_CUTSCENE_ANIM);
}
