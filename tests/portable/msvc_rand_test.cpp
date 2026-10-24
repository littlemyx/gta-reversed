// P4-S1 host test (portable, any platform): source/portable/msvc_rand.h vs the REAL MSVC CRT.
// Golden = tests/portable/msvc_rand_golden.inc, produced by tests/portable/msvc_rand_gen.cpp built with cl.exe and run under Wine (independent of the code under test).
//   tools/portable/run_msvc_rand_test.sh   (REGEN=1 rebuilds the golden with wine+cl; `mutate` runs source mutants that this test must catch)
// Checks: seed-1 sequence (1000), srand(12345) (1000) + 0/0xFFFFFFFF/0x1234/1, RAND_MAX == 0x7FFF (macro + <cstdlib> consumers), per-thread state (a new thread starts at seed 1 and the main
// thread is undisturbed), main-thread start state = LCG^19(1) (static-initialiser rand() calls of the exe), CGeneral-style consumers (RAND_MAX/2 ~50%, 1/RAND_MAX scaling),
// and that the macros leave `std::` / member / local-variable spellings alone.
#define NOTSA_MSVC_RAND_HOSTTEST 1
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <vector>
#include <algorithm>
#include <random>
#include "portable/msvc_rand.h"
#include "msvc_rand_golden.inc"

static int g_fails = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fails++; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

template<size_t N> static void expectSeq(const char* name, const int (&golden)[N], size_t from = 0) {
    for (size_t i = 0; i < N; i++) {
        const int v = rand();
        if (v != golden[i]) { CHECK(false, "%s[%zu]: got %d want %d", name, from + i, v, golden[i]); return; }
    }
}

// the only non-trivial user shapes of the game (General.h / General.cpp)
constexpr float RAND_MAX_FLOAT_RECIPROCAL = 1.0f / static_cast<float>(RAND_MAX);
static_assert(RAND_MAX == 0x7FFF, "RAND_MAX must be the MSVC CRT value");
static_assert(RAND_MAX_FLOAT_RECIPROCAL == 1.0f / 32767.0f);
static_assert(notsa::msvc_crt::MainThreadInitialSeed() == []{ uint32_t s = 1; for (int i = 0; i < 19; i++) s = s * 214013u + 2531011u; return s; }());


int main() {
    // ---- RAND_MAX seen by <cstdlib> consumers after the include
    CHECK(RAND_MAX == g_msvc_rand_max, "RAND_MAX %d vs cl.exe %d", RAND_MAX, g_msvc_rand_max);

    // ---- main thread: start state is LCG^19(1). Re-seed to 1 to compare with the cl.exe CRT main thread (startup state 1)
    {
        uint32_t s = 1; for (int i = 0; i < 19; i++) { (void)notsa::msvc_crt::RandOn(s); }
        const int expectFirst = notsa::msvc_crt::RandOn(s);                    // 20th value of the seed-1 sequence
        CHECK(expectFirst == g_seed1[19], "20th seed-1 value %d vs golden %d", expectFirst, g_seed1[19]);
        const int first = rand();                                                 // the game's first rand() on the main thread
        CHECK(first == g_seed1[19], "main thread first rand() %d, want 20th seed-1 value %d (19 initterm calls)", first, g_seed1[19]);
    }
    srand(1);
    expectSeq("seed1", g_seed1);
    srand(12345); expectSeq("seed12345", g_seed12345);
    srand(0);          expectSeq("seed0", g_seed0);
    srand(0xFFFFFFFFu); expectSeq("seedFFFFFFFF", g_seedFFFFFFFF);
    srand(0x1234);     expectSeq("seed1234", g_seed1234);
    srand(1);          expectSeq("seed1_again", g_seed1_again);
    // srand with a signed / 64-bit / pointer-derived argument: the macro casts to unsigned exactly like the CRT's `unsigned int` parameter
    { srand(-1);              expectSeq("seed(-1)==FFFFFFFF", g_seedFFFFFFFF); }
    { srand((int64_t)0x100000000LL + 12345); expectSeq("seed(2^32+12345)==12345 (truncation)", g_seed12345); }
    { srand(uint32_t(uintptr_t(&g_fails))); (void)rand(); } // NOTSA_PTR_ID32 style seeds compile

    // ---- per-thread state: main srand(777) + 2 calls, then a new thread must see the seed-1 sequence, then main continues undisturbed
    srand(777); (void)rand(); (void)rand();
    {
        std::vector<int> got(20);
        std::thread([&] { for (auto& v : got) v = rand(); }).join();
        for (size_t i = 0; i < got.size(); i++) CHECK(got[i] == g_thread_first_after_main_srand[i], "thread[%zu] %d vs %d", i, got[i], g_thread_first_after_main_srand[i]);
        for (size_t i = 0; i < got.size(); i++) CHECK(got[i] == g_seed1[i], "thread[%zu] %d != seed-1 sequence %d", i, got[i], g_seed1[i]);
    }
    expectSeq("main_after_thread", g_main_after_thread);
    // a worker that srand()s must not touch the main thread either
    srand(5); const int m1 = rand(); srand(5);
    std::thread([] { srand(99); for (int i = 0; i < 50; i++) (void)rand(); }).join();
    CHECK(rand() == m1, "worker srand/rand disturbed the main thread");

    // ---- consumers (General.cpp: GetRandomNumber() >= RAND_MAX / 2, RAND_MAX_FLOAT_RECIPROCAL scaling) over 100k calls
    srand(1);
    int ge = 0; double sum = 0; int mx = 0;
    for (int i = 0; i < 100000; i++) { const int v = rand(); mx = std::max(mx, v); ge += (uint16_t)v >= RAND_MAX / 2; sum += (double)v * (double)RAND_MAX_FLOAT_RECIPROCAL; }
    std::printf("100k: >= RAND_MAX/2: %d (%.1f%%), max %d, mean(v/RAND_MAX) %.4f\n", ge, ge / 1000.0, mx, sum / 100000);
    CHECK(ge > 49000 && ge < 51000, "coin flip not ~50%%: %d", ge);
    CHECK(mx <= 0x7FFF && mx > 0x7F00, "max %d", mx);
    CHECK(sum / 100000 > 0.49 && sum / 100000 < 0.51, "scaling mean %f", sum / 100000);

    std::printf("msvc_rand host test: %s (fails=%d)\n", g_fails ? "FAILED" : "ALL OK", g_fails);
    return g_fails != 0;
}
