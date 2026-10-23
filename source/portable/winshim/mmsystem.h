#pragma once
// MAC-W0: the wave-format struct of <mmsystem.h>/<mmreg.h> (plain data, packed like the Windows one) for non-Windows builds.
#include "portable/wintypes.h"

#pragma pack(push, 1)
struct WAVEFORMATEX {
    WORD  wFormatTag;
    WORD  nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD  nBlockAlign;
    WORD  wBitsPerSample;
    WORD  cbSize;
};
#pragma pack(pop)
using LPWAVEFORMATEX  = WAVEFORMATEX*;
using LPCWAVEFORMATEX = const WAVEFORMATEX*;
#define WAVE_FORMAT_PCM 1
