// MAC-W0a: POSIX replacement of extensions/debug.cpp (DbgHelp symbolication), which is excluded from the portable target. Portable-only file.
#include "StdInc.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <format>
#include <string>

#include <extensions/debug.hpp>

namespace notsa {
namespace debug {
// dladdr: image (module) + nearest exported/debug symbol; no source line (that needs DWARF + atos/dsymutil: W-A13)
std::string GetFunctionInfoAtAddress(uintptr_t address, bool compact, HANDLE) {
    Dl_info info{};
    if (!::dladdr(reinterpret_cast<void*>(address), &info) || !info.dli_fname) {
        return "<unknown>";
    }

    std::string module = info.dli_fname;
    if (const auto slash = module.find_last_of('/'); slash != std::string::npos) {
        module.erase(0, slash + 1);
    }

    std::string name = "<unknown>";
    uintptr_t   symAddr = address;
    if (info.dli_sname) {
        int status = 0;
        if (char* dem = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status); dem && status == 0) {
            name = dem;
            std::free(dem);
        } else {
            name = info.dli_sname;
        }
        symAddr = reinterpret_cast<uintptr_t>(info.dli_saddr);
    }
    const bool hasSym = info.dli_sname != nullptr;

    if (compact) {
        return hasSym ? std::format("{}!{}:0", module, name) : std::format("{}!{}", module, address);
    }
    return std::format("0x{:010x}+0x{:04x}: {}!{}:0", symAddr, address - symAddr, module, name);
}

void LoadSymbols() {
    // nothing to load: dladdr reads the dynamic symbol tables of the loaded images
}

void UnloadSymbols() {
}
}; // namespace debug
}; // namespace notsa
