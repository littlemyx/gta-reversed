#pragma once

// Slice A-1 / C-0 of .notes/STREAM_C_PLAN.md: the portable build (CMake option GTASA_PORTABLE, preset PortableMac, NOTSA_PORTABLE=1) has no hook registry.
//
// ReversibleHooks.h includes this header INSTEAD of RHManager.h / HooksUtility.hpp / VMTInfo.h (VirtualProtect, GetModuleHandle, exe addresses ...). Together with
// NoHookRegistry.h (NOTSA_PORTABLE => NOTSA_NO_HOOK_REGISTRY) every RH_Scoped* macro is `((void)0)` / empty, so the ~896 hook-registration files compile unchanged and
// reference nothing of the registry; the only names they still resolve are the ones below. The registry sources (reversiblehooks/*.cpp, the hook UI) are not part of the
// target (source/CMakeLists.txt). Never included when NOTSA_PORTABLE is not defined: the Windows builds do not see this file.
#ifndef NOTSA_PORTABLE
#error "reversiblehooks/PortableStub.h is only for the portable build (NOTSA_PORTABLE)"
#endif

#include <filesystem>
#include <optional>

#include "ReversibleHook/Enums/TwoWayHookState.h"

namespace ReversibleHooks {
//! `using HS = ReversibleHooks::HookInstallOptions::HS;` is the only thing the (dropped) macro arguments `{.State = HS::...}` of the prologue macros need
struct HookInstallOptions {
    using HS = ReversibleHook::TwoWayHookState;
};

//! No registry: the callers that survive in the portable tree (run WinMain creating / destroying it, UIRenderer::PreRenderUpdate, CommandLine::DumpHooks) get no-ops.
class RHManager {
public:
    using HookInstallOptions = ::ReversibleHooks::HookInstallOptions;

    static RHManager& GetInstance() noexcept { static RHManager s_Instance; return s_Instance; }
    static void CreateInstance() noexcept {}
    static void DestroyInstance() noexcept {}

    void CheckAll() noexcept {}
    void WriteHooksToFile(const std::filesystem::path&) noexcept {}
};
} // namespace ReversibleHooks
