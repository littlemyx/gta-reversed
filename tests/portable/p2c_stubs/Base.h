#pragma once
// P-2c host-test stub of source/Base.h: only the typedefs / VALIDATE_* macros / NOTSA_GLOBAL_EXTERN that the real Vector.h, Matrix.h, VehicleSaveStructure.h, ReplayBlockType.h need
// (the real Base.h drags in the whole engine, whose known non-P-2c errors make the TU unlinkable on the Mac).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <type_traits>
#include <rwplcore.h>
using uint8 = uint8_t; using uint16 = uint16_t; using uint32 = uint32_t; using uint64 = uint64_t;
using int8 = int8_t; using int16 = int16_t; using int32 = int32_t; using int64 = int64_t;
using GxtChar = uint16;
#define PLUGIN_API
#define NOTSA_UNPAREN(...) __VA_ARGS__
#define NOTSA_GLOBAL_EXTERN(name, type) extern std::type_identity_t<NOTSA_UNPAREN type> name
#define VALIDATE_SIZE_FILE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)
#define VALIDATE_OFFSET_FILE(struc, member, offset) static_assert(offsetof(struc, member) == offset, "The offset of " #member " in " #struc " is not " #offset "...")
#define VALIDATE_LAYOUT_FILE(...) static_assert(__VA_ARGS__)
#ifdef NOTSA_PORTABLE
#define VALIDATE_SIZE(struc, size) static_assert(true)
#define VALIDATE_OFFSET(struc, member, offset) static_assert(true)
#define VALIDATE_LAYOUT(...) static_assert(true)
#else
#define VALIDATE_SIZE(struc, size) VALIDATE_SIZE_FILE(struc, size)
#define VALIDATE_OFFSET(struc, member, offset) VALIDATE_OFFSET_FILE(struc, member, offset)
#define VALIDATE_LAYOUT(...) static_assert(__VA_ARGS__)
#endif
#include <ranges>
#include <span>
#include <numeric>
#include <initializer_list>
#include <cstdlib>
#include <cassert>
namespace rng = std::ranges;
#define NOTSA_UNREACHABLE(...) std::abort()
#define NLOHMANN_DEFINE_TYPE_INTRUSIVE(...)
