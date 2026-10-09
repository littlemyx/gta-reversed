#include "StdInc.h"

#ifdef NOTSA_STANDALONE_RUN
#include "DataImage.h"

// Placeholder inside the standalone exe's own image (linked at /BASE:0x400000, merged into .text by /MERGE:_TEXT=.text).
// It makes the *main image* cover the original data VA range [0x858000, 0xCB0000): only the main image is mapped before the process heap /
// NLS sections, so (unlike VirtualAlloc at startup) this range can never be taken by someone else. DataImage.cpp makes the data part RW and
// copies original_data.bin into it. See .notes/P2A_DESIGN.md section 4.
//
// The pad MUST be the very first code of .text, so that `notsa_orig_pad == 0x401000` == the original code start: the whole range
// [0x401000, 0x858000) is then PAGE_NOACCESS, and a raw jump/call to any original address faults with exactly that address instead of
// silently running unrelated code of ours. The section is `.text$00`: the linker sorts the `.text$<x>` group alphabetically and the
// compiler's own code is `.text$mn`. The link uses /INCREMENTAL:NO (incremental thunks would precede the first object), and
// DataImage::Load fails loudly if the pad is not at the code start.
#pragma section(".text$00", read, execute)
extern "C" __declspec(allocate(".text$00")) const unsigned char notsa_orig_pad[notsa::standalone::DataImage::ORIG_PAD_SIZE] = {};
#endif
