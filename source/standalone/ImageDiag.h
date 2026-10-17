#pragma once

// DEV-ONLY (CMake GTASA_IMAGE_DIAG=ON -> NOTSA_IMAGE_DIAG, the StandaloneDetached preset): the original exe's data image (.rdata/.data/BSS/_TEXT_HA/_rwdseg) mapped at
// its ORIGINAL virtual address, for the stray-access census / poison diagnostics (ImageDiag.cpp) that PROVE the run build touches none of it.
// The run build (StandaloneRelease / StandaloneRW) has no image at all (slice F3 of .notes/DETACH_DATA_PLAN.md): no loader, no pad, no original_data.* next to the exe.
// Nothing here is a runtime dependency of the game; the image is never patched (no code-pointer rewrite, no trap stubs): a stray execution of original code faults at the
// exact exe address (ImageLoad.cpp `ExeAccessVEH`).

#ifdef NOTSA_IMAGE_DIAG
#include <cstddef>
#include <cstdint>

namespace notsa::standalone::ImageDiag {
constexpr uint32_t ORIG_PAD_SIZE = 0x8B0000; // size of the in-image placeholder (tools/standalone/make_orig_pad.py, keep in sync): >= 0xCB1000 - 0x401000

struct Info {
    uint32_t ImageBase{};     // original exe image base (0x400000)
    uint32_t CodeLo{}, CodeHi{}; // original code range [CodeLo, CodeHi) (0x401000, 0x858000); PAGE_NOACCESS
    uint32_t DataBase{}, DataEnd{}; // committed data range (0x858000, 0xCB0000), BSS included
    uint32_t InitializedSize{};     // bytes copied from original_data.bin (the rest is zero = BSS)
    uint32_t RdataLo{}, RdataHi{};  // original .rdata [RdataLo, RdataHi) (0x858000, 0x8A4000): made read-only by ArmDiagnostics (0 = unknown)
};

//! Maps the image. Idempotent. Called by the `.CRT$XIB` initializer (before any C++ dynamic initializer). Terminates the process with a message if it cannot succeed.
void Load();
bool IsLoaded();
const Info& GetInfo();
//! Called from WinMain: .rdata -> PAGE_READONLY, a VEH that names the exe address of any stray execution / read of the original code range, then the env-driven diagnostics
//! NOTSA_POISON_CONVERTED=<file> (0xDD over converted globals) and NOTSA_IMAGE_CENSUS=1 (page-wise first-touch log). See ImageDiag.cpp.
void ArmDiagnostics();
} // namespace notsa::standalone::ImageDiag
#define NOTSA_IMAGE_DIAG_ARM() ::notsa::standalone::ImageDiag::ArmDiagnostics()
#else
#define NOTSA_IMAGE_DIAG_ARM() ((void)0)
#endif
