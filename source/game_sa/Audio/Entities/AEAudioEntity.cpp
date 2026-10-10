/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"
#include "AEAudioEntity.h"

constexpr auto EVENT_VOLUMES_BUFFER_SIZE = 45401;

NOTSA_GLOBAL(m_pAudioEventVolumes, 0xBD00F8, (int8*), {}); // Use `GetDefaultVolume` to access!

// The exe's destructors (e.g. 0x6D0A10 `mov [esi], 0x859988` = CAEAudioEntity vftable) reset the vptr down the chain, so a stale `CAESound::m_AudioEntity`
// of a destroyed owner dispatches to `CAEAudioEntity::UpdateParameters` (empty) instead of the derived override. With the trivial inline destructor MSVC /O2
// drops the dead base-vptr store and the stale call reaches the derived UpdateParameters (null `m_Entity` after `Terminate`) -> crash. Keep it out of line.
CAEAudioEntity::~CAEAudioEntity() { }

// NOTSA | INLINED | REFACTORED
bool CAEAudioEntity::StaticInitialise() {
    m_pAudioEventVolumes = new int8[EVENT_VOLUMES_BUFFER_SIZE];
    auto file = CFileMgr::OpenFile("AUDIO\\CONFIG\\EVENTVOL.DAT", "r");
    if (!file) {
        NOTSA_LOG_WARN("[AudioEngine] Failed to open EVENTVOL.DAT");
        CFileMgr::CloseFile(file);
        return false;
    }
    if (CFileMgr::Read(file, m_pAudioEventVolumes, EVENT_VOLUMES_BUFFER_SIZE) != EVENT_VOLUMES_BUFFER_SIZE) {
        NOTSA_LOG_WARN("[AudioEngine] Failed to read EVENTVOL.DAT");
        CFileMgr::CloseFile(file);
        return false;
    }
    CFileMgr::CloseFile(file);
    return true;
}

// NOTSA | INLINED | REFACTORED
void CAEAudioEntity::Shutdown() {
    delete[] std::exchange(m_pAudioEventVolumes, nullptr);
}

float CAEAudioEntity::GetDefaultVolume(eAudioEvents event) {
    return static_cast<float>(m_pAudioEventVolumes[event]);
}
