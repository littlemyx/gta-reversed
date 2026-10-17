#pragma once

static constexpr auto MAX_SAVEGAME_SLOTS{ 8u };

enum class eSlotState {
    SLOT_FILLED,
    SLOT_FREE,
    SLOT_CORRUPTED,
    SLOT_DATA_OLD,
};

class CGenericGameStorage {
    static constexpr auto BUFFER_SIZE{ (uint32)(50u * 1024u) };

    using tSlotSaveDate = GxtChar[70];
    using tSlotFileName = GxtChar[MAX_PATH];

    enum class eBlocks {
        SIMPLE_VARIABLES,
        SCRIPTS,
        POOLS,
        GARAGES,
        GAMELOGIC,
        PATHS,
        PICKUPS,
        PHONEINFO,
        RESTART,
        RADAR,
        ZONES,
        GANGS,
        CAR_GENERATORS,
        PED_GENERATORS,
        AUDIO_SCRIPT_OBJECT,
        PLAYERINFO,
        STATS,
        SET_PIECES,
        STREAMING,
        PED_TYPES,
        TAGS,
        IPLS,
        SHOPPING,
        GANGWARS,
        STUNTJUMPS,
        ENTRY_EXITS,
        RADIOTRACKS,
        USER3DMARKERS,

        TOTAL,
    };

    enum class eSaveLoadError {
        SYNC,
        LOADING,
        SAVING
    };
        
public:
    static inline NOTSA_GLOBAL(ms_WorkBufferSize, 0x8D2BE0, (uint32), { 51200 });
    static inline NOTSA_GLOBAL(ms_SaveFileNameJustSaved, 0xC16030, (char[MAX_PATH]), {});
    static inline NOTSA_GLOBAL(ms_SaveFileName, 0xC16DB8, (char[256]), {});
    static inline NOTSA_GLOBAL(ms_LoadFileName, 0xC15FC8, (char[104]), {});
    static inline NOTSA_GLOBAL(ms_LoadFileNameWithPath, 0xC15F60, (char[104]), {});
    static inline NOTSA_GLOBAL(ms_CheckSum, 0xC16134, (uint32), {});
    static inline NOTSA_GLOBAL(ms_WorkBuffer, 0xC16EE8, (uint8*), {});
    static inline NOTSA_GLOBAL(ms_WorkBufferPos, 0xC16EEC, (int32), {});
    static inline NOTSA_GLOBAL(ms_FileHandle, 0xC16EF0, (FILE*), {});
    static inline NOTSA_GLOBAL(ms_FilePos, 0xC16EF4, (uint32), {});
    static inline NOTSA_GLOBAL(ms_FileSize, 0xC16EF8, (uint32), {});
    static inline NOTSA_GLOBAL(ms_bFailed, 0xC16EFC, (bool), {});
    static inline NOTSA_GLOBAL(ms_bLoading, 0xC16EFD, (bool), {});
    static inline const char ms_BlockTagName[] = "BLOCK";
    static inline NOTSA_GLOBAL(ms_SlotSaveDate, 0xC16138, (std::array<tSlotSaveDate, MAX_SAVEGAME_SLOTS>), {});
    static inline NOTSA_GLOBAL(ms_SlotFileName, 0xC16368, (std::array<tSlotFileName, MAX_SAVEGAME_SLOTS>), {});
    static inline NOTSA_GLOBAL(ms_Slots, 0xC16EBC, (std::array<eSlotState, MAX_SAVEGAME_SLOTS>), {});

public:
    static void InjectHooks();

    static void ReportError(eBlocks nBlock, eSaveLoadError nError);
    static void DoGameSpecificStuffBeforeSave();
    static void DoGameSpecificStuffAfterSucessLoad();
    static void InitRadioStationPositionList();
    static bool GenericLoad(bool& outbVariablesLoaded);
    static bool GenericSave();
    static bool CheckSlotDataValid(int32 slot);
    static bool LoadDataFromWorkBuffer(void* data, int32 size);
    static bool SaveDataToWorkBuffer(void* data, int32 Size);
    static bool LoadWorkBuffer();
    static bool SaveWorkBuffer(bool bIncludeChecksum);
    static uint32 GetCurrentVersionNumber();
    static void MakeValidSaveName(int32 saveNum);
    static bool CloseFile();
    static bool OpenFileForWriting();
    static bool OpenFileForReading(const char* fileName, int32 slot);
    static bool CheckDataNotCorrupt(int32 slot, const char* fileName);
    static bool RestoreForStartLoad();

    // NOTSA
    template<typename T>
    static bool LoadDataFromWorkBuffer(T& data) { return LoadDataFromWorkBuffer((void*)&data, sizeof(T)); }

    template<typename T>
    static T LoadDataFromWorkBuffer() { T data; LoadDataFromWorkBuffer((void*)&data, sizeof(T)); return data; }

    template<typename T>
    static bool SaveDataToWorkBuffer(const T& data) { return SaveDataToWorkBuffer(const_cast<void*>((const void*)&data), sizeof(T)); }
private:
    static const char* GetBlockName(eBlocks);
};

const GxtChar* GetSavedGameDateAndTime(int32 slot);

 // Maybe NOTSA
 inline const GxtChar* GetNameOfSavedGame(int32 slot) {
    assert(slot < MAX_SAVEGAME_SLOTS);
    return CGenericGameStorage::ms_SlotFileName[slot];
}

 // Maybe NOTSA
inline const eSlotState GetSavedGameState(int32 slot) {
    assert(slot < MAX_SAVEGAME_SLOTS);
    return CGenericGameStorage::ms_Slots[slot];
}
