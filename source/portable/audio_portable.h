#pragma once
// MAC-W0: small platform helpers for the audio layer on non-Windows hosts (header only; included under NOTSA_PORTABLE).
#ifndef _WIN32
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <utility>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace notsa::portable {
//! [begin, end) of the main executable image in memory (all its Mach-O segments, i.e. text + static data/bss): the straight equivalent of
//! GetModuleHandle(nullptr) + IMAGE_NT_HEADERS::OptionalHeader.SizeOfImage
inline std::pair<uintptr_t, uintptr_t> MainImageRange() {
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
}
} // namespace notsa::portable
#endif
