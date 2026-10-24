#pragma once
// MAC-W0: small platform helpers for the audio layer on non-Windows hosts (header only; included under NOTSA_PORTABLE).
#ifndef _WIN32
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#else
extern "C" char __executable_start, _end; // ELF linker symbols
#endif

namespace notsa::portable {
//! [begin, end) of the main executable image in memory (all its Mach-O segments, i.e. text + static data/bss): the straight equivalent of
//! GetModuleHandle(nullptr) + IMAGE_NT_HEADERS::OptionalHeader.SizeOfImage
inline std::pair<uintptr_t, uintptr_t> MainImageRange() {
#ifndef __APPLE__
    // ELF: linker-provided symbols (GNU ld / lld) bound the main program image: text + rodata + data + bss. (<link.h> / dl_iterate_phdr is shadowed by game_sa/Core/link.h on the include path)
    static const auto range = std::pair{ reinterpret_cast<uintptr_t>(&__executable_start), reinterpret_cast<uintptr_t>(&_end) };
    return range;
#else
    static const auto range = [] {
        const auto hdr   = reinterpret_cast<const mach_header_64*>(_dyld_get_image_header(0));
        const auto slide = (uintptr_t)_dyld_get_image_vmaddr_slide(0);
        uintptr_t  lo = ~uintptr_t{}, hi = 0;
        auto       cmd = reinterpret_cast<const load_command*>(hdr + 1);
        for (uint32_t i = 0; i < hdr->ncmds; i++, cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(cmd) + cmd->cmdsize)) {
            if (cmd->cmd != LC_SEGMENT_64)
                continue;
            const auto seg = reinterpret_cast<const segment_command_64*>(cmd);
            if (seg->vmsize == 0 || std::string_view{seg->segname} == "__PAGEZERO")
                continue;
            lo = std::min<uintptr_t>(lo, seg->vmaddr + slide);
            hi = std::max<uintptr_t>(hi, seg->vmaddr + slide + seg->vmsize);
        }
        return std::pair{ lo, hi };
    }();
    return range;
#endif
}
} // namespace notsa::portable
#else
// MAC-WIN64: Windows x64 portable build: the PE image of the exe (what IsInExeImage / the audio stats read through GetModuleHandle in the Win32 build)
#include <cstdint>
#include <utility>
namespace notsa::portable {
inline std::pair<uintptr_t, uintptr_t> MainImageRange() {
    static const auto range = [] {
        const auto base = reinterpret_cast<uintptr_t>(::GetModuleHandleW(nullptr));
        const auto nt   = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
        return std::pair{ base, base + (uintptr_t)nt->OptionalHeader.SizeOfImage };
    }();
    return range;
}
} // namespace notsa::portable
#endif
