#include "StdInc.h"
#include "VMTInfo.h"
#include "dllmain.h"
#include "HooksUtility.hpp"
#ifdef NOTSA_STANDALONE_RUN
#include "standalone/Fixups.h"
#endif

namespace {
// Really fucking simple name mangling for msvc
// Check this out: https://en.m.wikiversity.org/wiki/Visual_C%2B%2B_name_mangling
void MangleClassNameMSVC(CHAR* out, std::string_view name) {
    if (const auto openerPos = name.find('<'); openerPos != std::string_view::npos) { // Templated class, this only works for single templated classes (for now)
        // ??_7?$CTaskComplexSeekEntity@VCEntitySeekPosCalculatorStandard@@@@6B@
        const auto closerPos = name.rfind('>');
        *std::format_to(out, "??_7?${}@V{}@@@@6B@", name.substr(0, openerPos), name.substr(openerPos + 1, closerPos - openerPos - 1)) = 0;
    } else {
        // ??_7CTaskSimple@@6B@
        *std::format_to(out, "??_7{}@@6B@", name) = 0;
    }
}

HMODULE GetVtableModule() {
#ifdef NOTSA_STANDALONE_RUN
    return GetModuleHandleA(nullptr); // the exe itself exports the vtables (NOTSA_EXPORT_VTABLE = dllexport)
#else
    return notsa::GetDLLHandle();
#endif
}

void** FindVMTAddressByClassName(std::string_view className) {
    // The VTable is exported as a symbol, in the format `??_7<class name>@@6B@` where `<class name>` is the name of the class.
    // In order for this to work the class has to be exported (So the `NOTSA_EXPORT_VTABLE` macro has to be used)
    CHAR mangledName[1024];
    MangleClassNameMSVC(mangledName, className);
    if (const auto address = reinterpret_cast<void**>(GetProcAddress(GetVtableModule(), mangledName))) {
        return address;
    }
    throw std::runtime_error{ std::format("Failed to find VMT for class '{}'", className) };
}
};

auto ReversibleHooks::Utility::VMTInfo::FindByClassName(const char* name, size_t size) -> VMTInfo {
#ifdef NOTSA_STANDALONE_RUN
    // A class without `NOTSA_EXPORT_VTABLE` must not abort the whole startup: its virtual hooks are skipped (the exe vtable slots stay trapped)
    try {
        return VMTInfo{ FindVMTAddressByClassName(name), size };
    } catch (const std::exception& e) {
        notsa::standalone::Fixups::Log("VMT export missing, virtual hooks of this class are skipped: %s", e.what());
        return VMTInfo{};
    }
#else
    return VMTInfo{ FindVMTAddressByClassName(name), size };
#endif
}

size_t ReversibleHooks::Utility::VMTInfo::FindIndexOf(void* fn) {
    ReversibleHooks::Utility::ScopedVirtualProtectModify g{ m_Table, m_Size * sizeof(*m_Table) };
    for (size_t i = 0; i < m_Size; i++) {    
        if (m_Table[i] == fn) {
            return i;
        }
    }
    throw std::runtime_error{ std::format("Failed to find function `{}` in VMT", fn) };
}
