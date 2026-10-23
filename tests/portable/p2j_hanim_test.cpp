// P-2j host test (portable arm64): the RtAnim key-frame stream callbacks of source/standalone/rw/hanim.cpp (REAL file, compiled into this test; undefined librw symbols are
// resolved lazily and never called) against hand-built RtAnim key-frame blobs.
//   tools/portable/run_p2j_test.sh [mutate]
// On-disk record = 0x24 bytes: {float time, float q[4], float t[3]} (0x20) + uint32 byte offset of the previous key frame of the same node (0x24 stride, NOT the LP64 in-memory stride).
#include "fakerw.h"
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)

// data symbols of librw that hanim.cpp's (never called) hierarchy code references; functions stay lazily unresolved
rw::PluginList rw::Frame::s_plglist(0);
rw::Engine* rw::engine = nullptr;

// ---- memory stream behind the two RwStream calls hanim.cpp makes ---------------------------------------------------------------------------------------------
struct MemStream { const uint8_t* rd; size_t rdLen, pos; std::vector<uint8_t> wr; bool failWrite; };
static RwStream* S(MemStream* m) { return reinterpret_cast<RwStream*>(m); }
RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length) {
    auto* m = reinterpret_cast<MemStream*>(stream);
    const size_t n = std::min<size_t>(length, m->rdLen - m->pos);
    std::memcpy(buffer, m->rd + m->pos, n); m->pos += n; return (RwUInt32)n;
}
RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length) {
    auto* m = reinterpret_cast<MemStream*>(stream);
    if (m->failWrite) return nullptr;
    auto* p = static_cast<const uint8_t*>(buffer); m->wr.insert(m->wr.end(), p, p + length); return stream;
}

// ---- hand-built blob (independent of hanim.cpp): frame i = time, q, t, prevOffset --------------------------------------------------------------------------------
struct Spec { float time, q[4], t[3]; uint32_t prevOffset; };
static std::vector<uint8_t> blob(const std::vector<Spec>& fr) {
    std::vector<uint8_t> b(fr.size() * 0x24);
    for (size_t i = 0; i < fr.size(); ++i) {
        uint8_t* p = &b[i * 0x24];
        std::memcpy(p + 0x00, &fr[i].time, 4); std::memcpy(p + 0x04, fr[i].q, 16); std::memcpy(p + 0x14, fr[i].t, 12); std::memcpy(p + 0x20, &fr[i].prevOffset, 4);
    }
    return b;
}
// an RtAnimAnimation over caller-owned key-frame storage (what RtAnimAnimationCreate hands the callbacks)
struct Anim { std::vector<rw::HAnimKeyFrame> kf; rw::Animation a{}; explicit Anim(size_t n) : kf(n) { a.numFrames = (int32_t)n; a.keyframes = kf.data(); } };

static void checkDecode(const std::vector<Spec>& fr, const char* what) {
    const auto b = blob(fr);
    Anim an(fr.size());
    MemStream in{ b.data(), b.size(), 0, {}, false };
    CHECK(RpHAnimKeyFrameStreamGetSize(&an.a) == (RwInt32)b.size());
    CHECK(RpHAnimKeyFrameStreamRead(S(&in), &an.a) == &an.a);
    CHECK(in.pos == b.size());
    for (size_t i = 0; i < fr.size(); ++i) {
        const auto& k = an.kf[i];
        const bool ok = k.time == fr[i].time && k.q.x == fr[i].q[0] && k.q.y == fr[i].q[1] && k.q.z == fr[i].q[2] && k.q.w == fr[i].q[3] && k.t.x == fr[i].t[0] && k.t.y == fr[i].t[1] && k.t.z == fr[i].t[2]
                     && k.prev == &an.kf[fr[i].prevOffset / 0x24];
        if (!ok) { std::printf("FAIL %s: frame %zu decode\n", what, i); fails++; }
    }
    MemStream out{ nullptr, 0, 0, {}, false };
    CHECK(RpHAnimKeyFrameStreamWrite(&an.a, S(&out)) == TRUE);
    if (out.wr != b) { std::printf("FAIL %s: write != blob (%zu vs %zu bytes)\n", what, out.wr.size(), b.size()); fails++; }
}

int main() {
    // the required case: 3 frames, prev offsets 0 / 36 / 72 => prev -> frames 0 / 1 / 2 (each frame is its own predecessor)
    checkDecode({ { 0.f, { 0.1f, 0.2f, 0.3f, 0.9f }, { 1.f, 2.f, 3.f }, 0 }, { 0.5f, { -0.1f, 0.25f, -0.75f, 0.5f }, { 4.f, 5.f, 6.f }, 36 }, { 1.f, { 0.5f, 0.5f, 0.5f, 0.5f }, { -7.f, 8.f, 9.5f }, 72 } }, "3 frames 0/36/72");
    {   // and the explicit pointer expectation, independent of the helper above
        const auto b = blob({ { 0.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 0 }, { 1.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 36 }, { 2.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 72 } });
        Anim an(3); MemStream in{ b.data(), b.size(), 0, {}, false };
        CHECK(RpHAnimKeyFrameStreamRead(S(&in), &an.a) == &an.a);
        CHECK(an.kf[0].prev == &an.kf[0] && an.kf[1].prev == &an.kf[1] && an.kf[2].prev == &an.kf[2]);
        CHECK(RpHAnimKeyFrameStreamGetSize(&an.a) == 108 && b.size() == 108);
        // LP64 trap: in-memory stride is not the file stride; a reader dividing by sizeof(HAnimKeyFrame) would map offset 72 to frame 1 (72/40) or 1.8
        static_assert(sizeof(void*) == 4 || sizeof(rw::HAnimKeyFrame) != 0x24);
    }
    // 6 frames, predecessor chain per node (two nodes interleaved: 0,1,2 then 3->0, 4->1, 5->2), non-trivial floats
    checkDecode({ { 0.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 0 }, { 0.f, { 0, 0, 0, 1 }, { 1, 1, 1 }, 36 }, { 0.f, { 0, 0, 0, 1 }, { 2, 2, 2 }, 72 },
                  { 1.5f, { 0.4f, 0.5f, 0.6f, 0.7f }, { 0, 20, 30 }, 0 }, { 1.5f, { 0.4f, 0.5f, 0.6f, 0.7f }, { 1, 20, 30 }, 36 }, { 1.5f, { 0.4f, 0.5f, 0.6f, 0.7f }, { 2, 20, 30 }, 72 } }, "6 frames chained");
    // random: 40 frames, any predecessor index (forward references included); floats are random bit patterns incl. NaN/inf/denormals (copied, never interpreted)
    {
        std::mt19937 rng(0xA11CE);
        for (int round = 0; round < 25; ++round) {
            const size_t n = 1 + rng() % 60;
            std::vector<Spec> fr(n);
            for (auto& f : fr) {
                uint32_t bits[8]; for (auto& x : bits) x = rng();
                std::memcpy(&f.time, &bits[0], 4); std::memcpy(f.q, &bits[1], 16); std::memcpy(f.t, &bits[5], 12);
                f.prevOffset = 0x24u * (rng() % n);
            }
            // NaN payloads: compare bytes, not floats
            const auto b = blob(fr);
            Anim an(n); MemStream in{ b.data(), b.size(), 0, {}, false };
            CHECK(RpHAnimKeyFrameStreamRead(S(&in), &an.a) == &an.a);
            bool ok = true; for (size_t i = 0; i < n; ++i) ok &= an.kf[i].prev == &an.kf[fr[i].prevOffset / 0x24] && std::memcmp(&an.kf[i].time, &b[i * 0x24], 0x20) == 0;
            CHECK(ok);
            MemStream out{ nullptr, 0, 0, {}, false };
            CHECK(RpHAnimKeyFrameStreamWrite(&an.a, S(&out)) == TRUE && out.wr == b);
            CHECK(RpHAnimKeyFrameStreamGetSize(&an.a) == (RwInt32)(n * 0x24));
        }
    }
    // truncated / failing streams
    {
        const auto b = blob({ { 0.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 0 }, { 1.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 36 }, { 2.f, { 0, 0, 0, 1 }, { 0, 0, 0 }, 72 } });
        for (size_t len : { size_t(0), size_t(31), size_t(32), size_t(35), size_t(36), size_t(71), size_t(107) }) {   // every cut inside the payload or the offset word
            Anim an(3); MemStream in{ b.data(), len, 0, {}, false };
            CHECK(RpHAnimKeyFrameStreamRead(S(&in), &an.a) == nullptr);
        }
        Anim an(3); MemStream out{ nullptr, 0, 0, {}, true };
        CHECK(RpHAnimKeyFrameStreamWrite(&an.a, S(&out)) == FALSE);
        Anim zero(0); MemStream e{ nullptr, 0, 0, {}, false };
        CHECK(RpHAnimKeyFrameStreamRead(S(&e), &zero.a) == &zero.a && RpHAnimKeyFrameStreamGetSize(&zero.a) == 0 && RpHAnimKeyFrameStreamWrite(&zero.a, S(&e)) == TRUE && e.wr.empty());
    }
    std::printf(fails ? "p2j FAILED (%d)\n" : "p2j host test: ALL OK (fails=%d)\n", fails);
    return fails ? 1 : 0;
}
