#include "StdInc.h"
#include "AEMP3TrackLoader.h"
#include "app.h"
#include "FileMgr.h"
#include "MemoryMgr.h"
#ifdef NOTSA_PORTABLE
#include "portable/path_resolve.h"
#endif

void CAEMP3TrackLoader::InjectHooks() {
    RH_ScopedClass(CAEMP3TrackLoader);
    RH_ScopedCategory("Audio/Loaders");

    RH_ScopedInstall(Constructor, 0x4E0930);
    RH_ScopedInstall(Deconstructor, 0x4E0940);
    RH_ScopedInstall(Initialise, 0x4E0C50);
    RH_ScopedInstall(LoadStreamPackTable, 0x4E0970);
    RH_ScopedInstall(LoadTrackLookupTable, 0x4E09F0);
    RH_ScopedInstall(GetTrackInfo, 0x4E0A70);
    RH_ScopedInstall(IsCurrentAudioStreamAvailable, 0x4E0BD0);
}

// 0x4E0930
CAEMP3TrackLoader::CAEMP3TrackLoader() {
    m_bInitialised = false;
    m_pszDvdDrivePath = nullptr;
}

// 0x4E0940
CAEMP3TrackLoader::~CAEMP3TrackLoader() {
    if (m_bInitialised) {
        CMemoryMgr::Free(m_paTrackLookups);
        CMemoryMgr::Free(m_paStreamPacks);
    }

    if (m_pszDvdDrivePath) {
        delete m_pszDvdDrivePath;
    }
}

// 0x4E0C50
bool CAEMP3TrackLoader::Initialise() {
    if (!LoadStreamPackTable() || !LoadTrackLookupTable())
        return false;

    m_bStreamingFromDVD = false;

    // NOTSA: Originally used a heap allocated string and strcpy/strcat to make paths.

    // Firstly try loading locally.
    const auto localPath = std::format("AUDIO\\STREAMS\\{}", m_paStreamPacks[0].m_szName);
    if (auto file = CFileMgr::OpenFile(localPath.c_str(), "r")) {
        m_bInitialised = true;
        m_bStreamingFromDVD = false;

        CFileMgr::CloseFile(file);
        return true;
    }

    // Try loading from a DVD.
    // 0x4E0CEE: the exe sets `m_bInitialised` here (IsCurrentAudioStreamAvailable does not)
    if (IsCurrentAudioStreamAvailable()) {
        m_bInitialised = true;
        return true;
    }
    return false;
}

// 0x4E0970
bool CAEMP3TrackLoader::LoadStreamPackTable(void) {
    // NOTSA: Originally Win32 file API was used.
#ifdef NOTSA_PORTABLE
    auto* fp = notsa::portable::OpenResolved("AUDIO\\CONFIG\\STRMPAKS.DAT", "rb"); // W-7: case-insensitive; a missing file is not created in the game directory
    if (!fp) {
#else
    auto* fp = fopen("AUDIO\\CONFIG\\STRMPAKS.DAT", "rb");
    if (!fp) {
        // Win32 API creates a file if it doesn't exists, and reads 0 bytes.
        fp = fopen("AUDIO\\CONFIG\\STRMPAKS.DAT", "w");
        fclose(fp);
#endif

        // NOTSA: Originally (StreamPack*)CMemoryMgr::Malloc(0), return value is implementation-dependent.
        m_paStreamPacks = nullptr;
        m_nStreamPackCount = 0;
        return false;
    }

    // Get file size.
    fseek(fp, 0, SEEK_END);
    const auto size = ftell(fp);
    const auto numPacks = size / sizeof(StreamPack);
    rewind(fp);

    m_paStreamPacks = (StreamPack*)CMemoryMgr::Malloc(size);
    m_nStreamPackCount = numPacks;

    // NOTE: Win32 API outputs number of bytes read, most likely checking is unnecessary.
    fread(m_paStreamPacks, sizeof(StreamPack), numPacks, fp);

    fclose(fp);
    return true;
}

// 0x4E09F0
bool CAEMP3TrackLoader::LoadTrackLookupTable(void) {
    // NOTSA: Originally Win32 file API was used.
#ifdef NOTSA_PORTABLE
    auto* fp = notsa::portable::OpenResolved("AUDIO\\CONFIG\\TRAKLKUP.DAT", "rb"); // W-7: case-insensitive; a missing file is not created in the game directory
    if (!fp) {
#else
    auto* fp = fopen("AUDIO\\CONFIG\\TRAKLKUP.DAT", "rb");
    if (!fp) {
        // Win32 API creates a file if it doesn't exists, and reads 0 bytes.
        fp = fopen("AUDIO\\CONFIG\\TRAKLKUP.DAT", "w");
        fclose(fp);
#endif

        // NOTSA: Originally (tTrackLookup*)CMemoryMgr::Malloc(0), return value is implementation-dependent.
        m_paTrackLookups = nullptr;
        m_nTrackCount = 0;
        return false;
    }

    // Get file size.
    fseek(fp, 0, SEEK_END);
    const auto size = ftell(fp);
    const auto numPacks = size / sizeof(tTrackLookup);
    rewind(fp);

    m_paTrackLookups = (tTrackLookup*)CMemoryMgr::Malloc(size);
    m_nTrackCount = numPacks;

    // NOTE: Win32 API outputs number of bytes read, most likely checking is unnecessary.
    fread(m_paTrackLookups, sizeof(tTrackLookup), numPacks, fp);

    fclose(fp);
    return true;
}

// 0x4E0A70
tTrackInfo* CAEMP3TrackLoader::GetTrackInfo(uint32 trackId) {
#ifdef FIX_BUGS
    if (trackId >= m_nTrackCount)
#else
    if (trackId > m_nTrackCount) // 0x4E0A91: `ja` (GetDataStream at 0x4E0D41 uses `jae`); the id == count entry is one past the table
#endif
        return nullptr;

    const auto packId = m_paTrackLookups[trackId].m_nbPackId;
    if (packId > m_nStreamPackCount)
        return nullptr;

    const auto length = [&] {
        if (m_bStreamingFromDVD) {
            return strlen(m_pszDvdDrivePath) + strlen(m_paStreamPacks[packId].m_szName) + 21;
        }
        return strlen(m_paStreamPacks[packId].m_szName) + 21;
    }();

    char* path = new char[length]; // owned by to be ctor'd CAEDataStream.
    strcpy_s(path, length,
        std::format("{}AUDIO\\STREAMS\\{}",
            !m_bStreamingFromDVD ? "" : std::string{m_pszDvdDrivePath},
            m_paStreamPacks[packId].m_szName
        ).c_str()
    );

    auto* stream = new CAEDataStream(
        trackId,
        path,
        m_paTrackLookups[trackId].m_nOffset,
        m_paTrackLookups[trackId].m_nSize,
        true
    );
    auto* trackInfo = new tTrackInfo;

    stream->Initialise();
    stream->FillBuffer(trackInfo, sizeof(tTrackInfo));
    delete stream;

    return trackInfo;
}

// 0x4E0BD0
bool CAEMP3TrackLoader::IsCurrentAudioStreamAvailable() {
    if (m_pszDvdDrivePath = getDvdGamePath()) {
        // 0x4E0BE6: the drive string already ends with a path separator (like "D:\"): strcpy(dvd) + strcat("AUDIO\STREAMS\") + strcat(name)
        const auto dvdPath = std::format("{}AUDIO\\STREAMS\\{}", m_pszDvdDrivePath, m_paStreamPacks[0].m_szName);
        if (auto file = CFileMgr::OpenFile(dvdPath.c_str(), "r")) {
            m_bStreamingFromDVD = true;

            CFileMgr::CloseFile(file);
            return true;
        }
    }

    m_bStreamingFromDVD = false;
    return false;
}
