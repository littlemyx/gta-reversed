/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include <cstddef>
#include <cstdint>

#include "Base.h"
#include "standalone/Fixups.h"

namespace plugin {
int GetBaseAddress();

int GetGlobalAddress(int address);

// meta template for functions
#if (defined(__GNUC__) || defined(__GNUG__) || defined(__clang__))
template <auto Func> struct meta;
#elif (defined(_MSC_VER))
template <typename FuncType, FuncType Func> struct meta;
#else

#endif

// Gets the virtual method table from the object @self
inline void** GetVMT(const void* self) {
    return *(void***)(self);
}

// In the standalone build there is no original code at `address`: resolve to our reimplementation (hook table) or to a trap
#ifdef NOTSA_STANDALONE_RUN
#define NOTSA_PLUGIN_ADDR(address) (notsa::standalone::ResolveCallTarget(address))
#else
#define NOTSA_PLUGIN_ADDR(address) (address)
#endif

// Detached run build (NOTSA_GLOBALS_DETACHED): there is no original code at `address`, so ANY instantiation of the by-address calls below is a compile error
// (the DLL build and the address-mode oracle tests do not define it and keep working). Convert the call to a direct call of our own function.
#ifdef NOTSA_GLOBALS_DETACHED
template <unsigned int> inline constexpr bool kCallByAddressForbidden = false;
#define NOTSA_PLUGIN_CALL_FORBIDDEN(address) static_assert(::plugin::kCallByAddressForbidden<address>, "plugin::Call*<0xADDR>: call by address of original code is not allowed in the detached run build - call our own function directly")
#else
#define NOTSA_PLUGIN_CALL_FORBIDDEN(address)
#endif

template <unsigned int address, typename... Args> void Call(Args... args) {
    NOTSA_PLUGIN_CALL_FORBIDDEN(address);
    reinterpret_cast<void(__cdecl*)(Args...)>(NOTSA_PLUGIN_ADDR(address))(args...);
}

template <typename Ret, unsigned int address, typename... Args>  Ret CallAndReturn(Args... args) {
    NOTSA_PLUGIN_CALL_FORBIDDEN(address);
    return reinterpret_cast<Ret(__cdecl*)(Args...)>(NOTSA_PLUGIN_ADDR(address))(args...);
}

template <typename Ret, unsigned int address, typename C, typename... Args>
Ret CallMethodAndReturn(C _this, Args... args) requires std::is_class_v<std::remove_pointer_t<C>> {
    NOTSA_PLUGIN_CALL_FORBIDDEN(address);
    return reinterpret_cast<Ret(__thiscall*)(C, Args...)>(NOTSA_PLUGIN_ADDR(address))(_this, args...);
}

template <unsigned int address, typename C, typename... Args>
void CallMethod(C _this, Args... args) requires std::is_class_v<std::remove_pointer_t<C>> {
    NOTSA_PLUGIN_CALL_FORBIDDEN(address);
    return reinterpret_cast<void(__thiscall*)(C, Args...)>(NOTSA_PLUGIN_ADDR(address))(_this, args...);
}

template <unsigned int tableIndex, typename C, typename... Args>
void CallVirtualMethod(C _this, Args... args) requires std::is_class_v<std::remove_pointer_t<C>> {
    reinterpret_cast<void(__thiscall*)(C, Args...)>((*reinterpret_cast<void***>(_this))[tableIndex])(_this, args...);
}

template <typename Ret, unsigned int tableIndex, typename C, typename... Args>
Ret CallVirtualMethodAndReturn(C _this, Args... args) requires std::is_class_v<std::remove_pointer_t<C>> {
    return reinterpret_cast<Ret(__thiscall*)(C, Args...)>((*reinterpret_cast<void***>(_this))[tableIndex])(_this, args...);
}

} // namespace plugin
