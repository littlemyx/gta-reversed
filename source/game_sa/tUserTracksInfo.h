#pragma once

#include "eAudioFileType.h"

struct tUserTracksInfo {
    uint32         startUfilesPath;
    uint32         pathLength;
    eAudioFileType fileType;
};

VALIDATE_SIZE_FILE(tUserTracksInfo, 0xc);
