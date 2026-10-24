// MAC-RR2 host test (portable arm64): RtAnimInterpolatorSetCurrentAnim of source/standalone/rw/rtanim.cpp (REAL file) with the game's RpAnimBlend scheme.
//   tools/portable/run_rr2_anim_test.sh [mutate]
// The game's interpolated frame is RpHAnimBlendInterpFrame = {q, t} = 0x1C bytes WITHOUT a header. The exe's SetCurrentAnim stores the two key-frame pointers over the first 8
// bytes (x86: q.x/q.y = heap addresses, ~1e-38 as floats, so the frame stays an identity rotation). On LP64 the 16 pointer bytes cover the whole quaternion: the first skeleton
// update of every new ped then read address-dependent garbage (NaN / 1e18 / ...) = the "last self-nondeterminism" of the native build (MAC-RR2). Requirement: after
// SetCurrentAnim every interpolated frame of the game's scheme is exactly what the interpolate callback wrote (q = 0 0 0 1, t = 0), whatever the key-frame addresses are.
#include "fakerw.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)

namespace { struct GameFrame { float q[4]; float t[3]; }; static_assert(sizeof(GameFrame) == 0x1C); }
// what the game's RpAnimBlendKeyFrameInterpolate does: `*out = {}` with RtQuat q{0,0,0,1}, CVector t{0,0,0}
static void GameInterp(void* out, void*, void*, float, void*) { *static_cast<GameFrame*>(out) = GameFrame{ { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } }; }
static void NoopApply(void*, void*) {}

int main() {
    rw::Engine::memfuncs = rw::defaultMemfuncs;
    RtAnimInterpolatorInfo info{};
    info.typeID = 0x0253F2FB;   // rwID_RPANIMBLENDPLUGIN
    info.interpKeyFrameSize = sizeof(GameFrame);
    info.animKeyFrameSize = 40;                              // sizeof(RpHAnimKeyFrame) on LP64
    info.keyFrameApplyCB = NoopApply;
    info.keyFrameInterpolateCB = GameInterp;
    CHECK(RtAnimRegisterInterpolationScheme(&info) == TRUE);

    for (int round = 0; round < 64; round++) {
        // vary the heap: junk blocks of different sizes so the animation / interpolator land at different addresses with different low/high address words
        std::vector<void*> junk;
        for (int j = 0; j < round * 7 + 1; j++) junk.push_back(std::malloc(16 + 64 * (j % 11)));
        constexpr int N = 32;
        rw::AnimInterpolator* ip = rw::AnimInterpolator::create(N, sizeof(GameFrame));
        RtAnimAnimation* a = RtAnimAnimationCreate(info.typeID, 0, 0, 0.0f);   // RpAnimBlendCreateAnimationForHierarchy: numFrames 0 ...
        CHECK(ip && a);
        a->numFrames = 2 * N;                                                  // ... raised afterwards
        std::memset(ip->getFrames(), 0xA5, N * sizeof(GameFrame));
        CHECK(RtAnimInterpolatorSetCurrentAnim(ip, a) == TRUE);
        CHECK(ip->currentAnim == a && ip->currentInterpKeyFrameSize == (int)sizeof(GameFrame));
        bool identity = true;
        for (int i = 0; i < N; i++) {
            const GameFrame* f = reinterpret_cast<const GameFrame*>(ip->getInterpFrame(i));
            const GameFrame want{ { 0.f, 0.f, 0.f, 1.f }, { 0.f, 0.f, 0.f } };
            identity &= std::memcmp(f, &want, sizeof want) == 0;
        }
        CHECK(identity);
        RtAnimAnimationDestroy(a);
        ip->destroy();
        for (void* p : junk) std::free(p);
    }
    std::printf(fails ? "RR2 RESULT FAIL (%d)\n" : "RR2 RESULT OK\n", fails);
    return fails != 0;
}
