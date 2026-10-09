#pragma once

// Standalone build: the original exe's data image (.rdata/.data/BSS/_TEXT_HA/_rwdseg) mapped at its ORIGINAL virtual address,
// so that `StaticRef<T>(0xADDR)` and every absolute pointer stored inside the data stay valid. See .notes/P2A_DESIGN.md

#ifdef NOTSA_STANDALONE_RUN
#include <cstddef>
#include <cstdint>

namespace notsa::standalone::DataImage {
struct Info {
    uint32_t ImageBase{};     // original exe image base (0x400000)
    uint32_t CodeLo{}, CodeHi{}; // original code range [CodeLo, CodeHi) (0x401000, 0x858000); reserved PAGE_NOACCESS
    uint32_t DataBase{}, DataEnd{}; // committed data range (0x858000, 0xCB0000), BSS included
    uint32_t InitializedSize{};     // bytes copied from original_data.bin (the rest is zero = BSS)
};

//! Maps the image. Idempotent. Normally called by the `.CRT$XIB` initializer (before any C++ dynamic initializer),
//! and again, harmlessly, from the standalone WinMain. Terminates the process with a message if it cannot succeed.
void Load();
bool IsLoaded();
const Info& GetInfo();
} // namespace notsa::standalone::DataImage
#endif
