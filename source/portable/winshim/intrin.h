// Portable build: MSVC <intrin.h> placeholder. The x86 intrinsics the game used are replaced by the soft x87 / SIMD shims; nothing is provided here
// (clang's own <intrin.h> include_next's the MSVC one and fails on arm64).
#pragma once
