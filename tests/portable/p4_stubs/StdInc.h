#pragma once
// P-4b host-test stub of source/StdInc.h for tests/portable/p4_keyarray_test.cpp: just enough for the REAL KeyArray.cpp / KeyArray.h / Core/KeyGen.h / GxtChar.h.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <type_traits>
using uint8 = uint8_t; using uint16 = uint16_t; using uint32 = uint32_t; using uint64 = uint64_t;
using int8 = int8_t; using int16 = int16_t; using int32 = int32_t; using int64 = int64_t;
#define RH_ScopedInstall(...)
#define RH_ScopedOverloadedInstall(...)
#define VALIDATE_SIZE_FILE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)
using FILESTREAM = FILE*;
struct CFileMgr {
    static size_t Read(FILESTREAM file, void* buf, size_t size) { return fread(buf, 1, size, file); }
};
#include "Core/KeyGen.h"
