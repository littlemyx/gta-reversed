#include "StdInc.h"

#include <cstdio>
#include <filesystem>

#include "GenericGameStorage.h"
#include "Messages.h"
#include "C_PcSave.h"
#include "SimpleVariablesSaveStructure.h"
#include "GxtChar.h"

void C_PcSave::InjectHooks() {
    RH_ScopedClass(C_PcSave);
    RH_ScopedCategoryGlobal();

    // See note in CGenericGameStorage::InjectHooks as to why GenerateGameFilename is unhooked by default
    RH_ScopedInstall(SetSaveDirectory, 0x619040);
    RH_ScopedInstall(GenerateGameFilename, 0x6190A0);
    RH_ScopedInstall(PopulateSlotInfo, 0x619140);
    RH_ScopedInstall(SaveSlot, 0x619060);
    RH_ScopedInstall(DeleteSlot, 0x6190D0);
}

// 0x619040
void C_PcSave::SetSaveDirectory(const char* path) {
    sprintf_s(DefaultPCSaveFileName, "%s\\%s", path, "GTASAsf");
}

// 0x6190A0
void C_PcSave::GenerateGameFilename(int32 slot, char* out) {
    assert(slot < MAX_SAVEGAME_SLOTS);

    // BUG: The original code uses an unbounded sprintf. Callers pass buffers of different sizes (e.g. the 104 byte
    // `ms_LoadFileNameWithPath`), so `sprintf_s` with a fixed size would (in debug CRT) fill memory past the
    // actual buffer with 0xFE. Kept unbounded to match the original behaviour.
    sprintf(out, "%s%i%s", DefaultPCSaveFileName, slot + 1, ".b");
}

// 0x619140
void C_PcSave::PopulateSlotInfo() {
    s_PcSaveHelper.error = eErrorCode::NONE;

    for (auto i = 0u; i < std::size(CGenericGameStorage::ms_Slots); ++i) {
        CGenericGameStorage::ms_Slots[i] = eSlotState::SLOT_FREE;
        CGenericGameStorage::ms_SlotFileName[i][0] = 0;
        CGenericGameStorage::ms_SlotSaveDate[i][0] = 0;
    }


    for (auto i = 0u; i < std::size(CGenericGameStorage::ms_Slots); ++i) {
        char path[MAX_PATH]{};
        CSimpleVariablesSaveStructure vars{};

        GenerateGameFilename(i, path);
        auto file = CFileMgr::OpenFile(path, "rb");
        if (file) {
            CFileMgr::Seek(file, strlen(CGenericGameStorage::ms_BlockTagName), SEEK_SET);
            CFileMgr::Read(file, &vars, sizeof(CSimpleVariablesSaveStructure));

            // TODO: This is stupid
            if (strncmp((const char*)vars.m_szSaveName, TopLineEmptyFile, 26) != 0) { // exe 0x61920b: strncmp(name, "THIS FILE IS NOT VALID YET", 0x1a)
                memcpy(CGenericGameStorage::ms_SlotFileName[i], vars.m_szSaveName, 48); // TODO: why 48?
                CGenericGameStorage::ms_Slots[i] = eSlotState::SLOT_FILLED;
                CGenericGameStorage::ms_SlotFileName[i][24] = 0; // Truncate the name at 24th character
            }
            CFileMgr::CloseFile(file);
        }

        if (CGenericGameStorage::ms_Slots[i] != eSlotState::SLOT_FILLED) {
            continue;
        }

        if (CGenericGameStorage::CheckDataNotCorrupt(i, path)) {
            const auto& time = vars.m_systemTime;

            assert(time.wMonth - 1 < 12); // NOTSA
            
            char monthGXTKey[64]{};
            sprintf_s(monthGXTKey, "MONTH%d", (uint32)time.wMonth);
            assert(time.wMonth - 1 < 12); // NOTSA

            char date[128];
            sprintf_s(date, "%02d %s %04d %02d:%02d:%02d", time.wDay, GxtCharToUTF8(TheText.Get(monthGXTKey)), time.wYear, time.wHour, time.wMinute, time.wSecond);
            AsciiToGxtChar(date, CGenericGameStorage::ms_SlotSaveDate[i]);
        } else {
            CMessages::InsertNumberInString(TheText.Get("FEC_SLC"), i + 1 /* exe 0x619282: esi = slot + 1 */, -1, -1, -1, -1, -1, CGenericGameStorage::ms_SlotFileName[i]);
#ifdef NOTSA_STANDALONE_RUN
            // NOTSA-AUDIT: the exe (0x619140, 0x619268..0x61928C) does not set SLOT_CORRUPTED for a slot whose data fails CheckDataNotCorrupt: it only formats the "corrupted" text
#else
#line 87
            CGenericGameStorage::ms_Slots[i] = eSlotState::SLOT_CORRUPTED; // NOTSA: Missing corrupted slot
#endif
#line 88
        }
    }
}

// 0x619060
uint32 C_PcSave::SaveSlot(int32 slot) {
    CGenericGameStorage::MakeValidSaveName(slot);
    s_PcSaveHelper.error = eErrorCode::NONE;
    InitUserDirectories(); // exe 0x619075: creates the user dirs only, it does NOT chdir (SetDirMyDocuments would)
    CGenericGameStorage::DoGameSpecificStuffBeforeSave();
    return CGenericGameStorage::GenericSave() ? 0 : 2;
}

// 0x6190D0
bool C_PcSave::DeleteSlot(int32 slot) {
    assert(slot < MAX_SAVEGAME_SLOTS);

    char path[MAX_PATH]{};
    s_PcSaveHelper.error = eErrorCode::NONE;
    GenerateGameFilename(slot, path);
    std::error_code ec;
    std::filesystem::remove(path, ec); // exe 0x6190fe: DeleteFileA(path)
    // exe 0x619113-0x61913b: the result is "the file can no longer be opened" (true also when there was no file), not DeleteFile's result
    if (const auto f = CFileMgr::OpenFile(path, "rb")) {
        CFileMgr::CloseFile(f);
        return false;
    }
    return true;
}
