#pragma once
// MAC-W0: Media Foundation interface NAMES for non-Windows builds (forward declarations only: CAEMFDecoder keeps pointers to them; its portable
// implementation, portable/audio_decoders_portable.cpp, never creates one).
#include "objidl.h"
struct IMFAttributes;
struct IMFMediaType;
struct IMFSourceReader;
