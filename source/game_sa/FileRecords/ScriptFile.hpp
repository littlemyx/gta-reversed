#pragma once
// P-2g: on-disk (x86 exe) layout of CRunningScript. Standalone POD header (no StdInc): usable from tests/portable/p2g_*.cpp.
//
// CTheScripts::Save dumps every non-external running script raw (sizeof(CRunningScript) == 0xE0 on the exe) and follows it with `uint32 ipOffset` and 8 x `uint32 ipStackOffset`
// (relative to ScriptSpace). CTheScripts::Load starts the script by index, reads the 0xE0 bytes over it, restores m_pPrev/m_pNext from the freshly started script and then sets
// m_IP / m_IPStack[] from the trailing offsets. So of the 10 pointer slots in the record (next, prev, baseIP, ip, ipStack[8]) the loader only ever uses ip/ipStack through the offsets
// after the record; `next`/`prev` are discarded, `baseIP` is kept verbatim but only read by a debug log (CommandParser/Parser.hpp NotImplemented) and set at MissionCommands.cpp:36.
// On an LP64 host those slots are 8 bytes wide (and tScriptParam used to be 8), so every later offset moved: the record fixes them at u32.
#include <cstddef>
#include <cstdint>

//! VA of CTheScripts::ScriptSpace in the original exe (0xA49960; MissionBlock follows at +MAIN_SCRIPT_SIZE = 0xA7A6A0). The port writes exe-style VAs into the record's pointer slots
//! so a save made by the port still carries values of the same kind the exe wrote; they are never dereferenced on load.
inline constexpr uint32_t ScriptSpaceExeVA = 0xA49960;

struct CRunningScript_File {
    uint32_t next;               // 0x00 m_pNext : heap garbage in exe saves, 0 written by the port; discarded by the loader
    uint32_t prev;               // 0x04 m_pPrev : likewise
    char     name[8];            // 0x08 m_szName
    uint32_t baseIP;             // 0x10 m_BaseIP : ScriptSpaceExeVA + offset (0 if not inside ScriptSpace); loader rebuilds it
    uint32_t ip;                 // 0x14 m_IP : ScriptSpaceExeVA + offset or 0; the u32 offset after the record wins
    uint32_t ipStack[8];         // 0x18 m_IPStack : same
    uint16_t stackDepth;         // 0x38 m_StackDepth
    uint8_t  pad3A[2];           // 0x3A written 0
    uint32_t localVars[34];      // 0x3C m_LocalVars (32 locals + 2 timers), tScriptParam (union, 4 bytes) copied bit-exact
    uint8_t  isActive;           // 0xC4
    uint8_t  condResult;         // 0xC5
    uint8_t  usesMissionCleanup; // 0xC6
    uint8_t  isExternal;         // 0xC7
    uint8_t  isTextBlockOverride; // 0xC8
    int8_t   externalType;       // 0xC9
    uint8_t  padCA[2];           // 0xCA written 0
    int32_t  wakeTime;           // 0xCC
    uint16_t andOrState;         // 0xD0
    uint8_t  notFlag;            // 0xD2
    uint8_t  isDeathArrestCheckEnabled; // 0xD3
    uint8_t  doneDeathArrest;    // 0xD4
    uint8_t  padD5[3];           // 0xD5 written 0
    int32_t  sceneSkipIP;        // 0xD8
    uint8_t  thisMustBeTheOnlyMissionRunning; // 0xDC
    uint8_t  padDD[3];           // 0xDD written 0
};
static_assert(sizeof(CRunningScript_File) == 0xE0 && alignof(CRunningScript_File) == 4, "Invalid structure size of CRunningScript_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)
static_assert(offsetof(CRunningScript_File, name) == 0x08 && offsetof(CRunningScript_File, baseIP) == 0x10 && offsetof(CRunningScript_File, ip) == 0x14 && offsetof(CRunningScript_File, ipStack) == 0x18
           && offsetof(CRunningScript_File, stackDepth) == 0x38 && offsetof(CRunningScript_File, localVars) == 0x3C && offsetof(CRunningScript_File, isActive) == 0xC4
           && offsetof(CRunningScript_File, usesMissionCleanup) == 0xC6 && offsetof(CRunningScript_File, isTextBlockOverride) == 0xC8 && offsetof(CRunningScript_File, externalType) == 0xC9
           && offsetof(CRunningScript_File, wakeTime) == 0xCC && offsetof(CRunningScript_File, andOrState) == 0xD0 && offsetof(CRunningScript_File, notFlag) == 0xD2
           && offsetof(CRunningScript_File, doneDeathArrest) == 0xD4 && offsetof(CRunningScript_File, sceneSkipIP) == 0xD8 && offsetof(CRunningScript_File, thisMustBeTheOnlyMissionRunning) == 0xDC);

namespace notsa::script_file {
//! Pointer inside [space, space + size) -> exe VA of the same byte; anything else (null, outside) -> 0.
inline uint32_t ToExeVA(const uint8_t* p, const uint8_t* space, size_t size) {
    const auto d = reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(space); // wraps for p < space => rejected below
    return (p && d < size) ? ScriptSpaceExeVA + (uint32_t)d : 0u;
}
//! Exe VA inside ScriptSpace -> pointer into `space`; anything else -> nullptr.
inline uint8_t* FromExeVA(uint32_t va, uint8_t* space, size_t size) {
    return (va >= ScriptSpaceExeVA && va - ScriptSpaceExeVA < size) ? space + (va - ScriptSpaceExeVA) : nullptr;
}
} // namespace notsa::script_file
