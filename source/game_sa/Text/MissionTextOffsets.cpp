#include "StdInc.h"

#include "MissionTextOffsets.h"

CMissionTextOffsets::CMissionTextOffsets() {
    m_size = 0;
}

// 0x69F670
bool CMissionTextOffsets::Load(uint32 length, FILESTREAM file, uint32* offset, int32 unknown) {
    // 0x69F670: count = length / 12; an empty chunk reads nothing (the port always read one entry), read results are not checked and the count is stored at the end
    const auto count = length / sizeof(tMissionTextOffset);
    m_size = 0;
    for (uint32 i = 0; i < count; i++) {
        CFileMgr::Read(file, &m_offsets[i], sizeof(tMissionTextOffset));
        *offset += sizeof(tMissionTextOffset);
    }
    m_size = count;
    return true;
}
