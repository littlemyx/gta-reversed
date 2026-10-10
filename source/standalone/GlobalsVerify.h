#pragma once

// NOTSA_VERIFY_GLOBALS (slice A2 of .notes/DETACH_DATA_PLAN.md): every `NOTSA_GLOBAL*` global converted to a real C++ variable (detached mode) registers
// {name, file:line, original VA, size, address of its storage} here at static-init time. `DumpAtBoot()` (WinMain, after Fixups::ApplyToDataImage and
// before any game code ran) writes one line per global to the file named by the environment variable NOTSA_VERIFY_GLOBALS:
//     <VA hex>\t<size>\t<name>\t<file:line>\t<our bytes hex>\t<bytes of the data image at VA hex (post Fixups::ApplyToDataImage)>
// `tools/standalone/verify_globals.py` compares these against `original_data.bin`.
// This header is included from Base.h (PCH) ONLY when NOTSA_VERIFY_GLOBALS is defined; it must stay dependency free.
#include <cstddef>
#include <cstdint>

namespace notsa::globals {
//! Registration record; the constructor lives in source/standalone/GlobalsVerify.cpp
struct Reg {
    Reg(const char* name, const char* file, int line, uint32_t exeAddr, size_t size, const void* storage) noexcept;
};

//! Environment variables (all optional, read at boot):
//!   NOTSA_VERIFY_GLOBALS=<file>        write the dump of the boot state to <file>
//!   NOTSA_VERIFY_GLOBALS_EXIT=1        exit(0) right after the boot dump (no need to start the game to verify)
//!   NOTSA_VERIFY_GLOBALS_AFTER=<s>     additionally write <file>.late <s> seconds after boot (informational: compare two builds, not against the image)
void DumpAtBoot();
} // namespace notsa::globals
