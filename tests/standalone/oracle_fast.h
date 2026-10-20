// Optimised helpers of the exe-oracle harness, implemented in oracle_fast.cpp (opt in with -DORACLE_FAST_HELPERS, see there).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace oraclefast {
bool BytesDiffer(const void* a, const void* b, size_t n);                           // memcmp(a, b, n) != 0
bool FillGarbage(uint64_t* rngState, int sp, void* p, size_t n);                    // the harness FillGarbage; true = a special value was produced
// Delta snapshot of a large region (the 4 MB data image): the 64-byte blocks that differ from the pre-call snapshot instead of a second full copy.
struct Blk { uint32_t off; uint8_t d[64]; };
void Capture(const uint8_t* before, const uint8_t* now, size_t n, std::vector<Blk>& out);   // out = blocks of `now` that differ from `before` (n % 64 == 0)
void Restore(uint8_t* now, const uint8_t* before, const std::vector<Blk>& chg);              // puts the `before` bytes back into the blocks listed in chg
bool Equal(const uint8_t* before, const uint8_t* now, size_t n, const std::vector<Blk>& chg); // now == (before with the blocks of chg applied)
void Apply(uint8_t* buf, const std::vector<Blk>& chg);                                       // buf (a copy of `before`) += chg
}
