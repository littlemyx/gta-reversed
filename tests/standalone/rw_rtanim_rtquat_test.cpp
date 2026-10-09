// P2B-05c unit test: RtQuat* (source/standalone/rw/rtquat.cpp) and RtAnim* (rtanim.cpp). Runs under Wine; the engine is opened device-less.
//   rtquat: quaternion <-> matrix round trips in all four RtQuatConvertFromMatrix branches (trace, x, y, z), matrix flags / position, RtQuatRotate in the three
//   combine modes against a double-precision Hamilton reference (+ vector rotation consistency, mode swap mutation), RtQuatTransformVectors, slerp cache + slerp
//   against a double-precision slerp (t = 0, 0.25, 0.5, 0.75, 1; acos branches |c| < 0.5, >= 0.5, == 0, ~1, opposite).
//   rtanim: scheme registration (adapter into librw's table, duplicate refused), AnimationCreate layout (header + key frames, customData), SetCurrentAnim order
//   (interpolate callbacks BEFORE the key-frame pointers; mutation: a callback that wipes the frame), interpolation of the standard hanim scheme (id 1) while the
//   time is stepped, stream write / read of a game-registered scheme through the trampolines.
//   Optional SA part: male01.dff hierarchy with the game's scheme and the standard scheme.
// Usage: rw_rtanim_rtquat_test.exe [skinned.dff ...]. Exit code 0 = all passed.
#include "fakerw.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Section(const char* n) { std::printf("[%s]\n", n); }
static bool Near(double a, double b, double e = 1e-5) { return std::fabs(a - b) <= e; }

static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// double precision reference (Hamilton, w first)
struct Qd { double w, x, y, z; };
static Qd FromAxisAngleDeg(double ax, double ay, double az, double deg) {
    const double l = std::sqrt(ax * ax + ay * ay + az * az), h = deg * 3.14159265358979323846 / 360.0, s = std::sin(h) / l;
    return { std::cos(h), ax * s, ay * s, az * s };
}
static Qd Ham(const Qd& a, const Qd& b) {
    return { a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
             a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
             a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
             a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w };
}
static Qd ToQd(const RtQuat& q) { return { q.real, q.imag.x, q.imag.y, q.imag.z }; }
static RtQuat ToRt(const Qd& q) { RtQuat r; r.real = (float)q.w; r.imag = { (float)q.x, (float)q.y, (float)q.z }; return r; }
static bool QNear(const Qd& a, const Qd& b, double e = 1e-5) { return Near(a.w, b.w, e) && Near(a.x, b.x, e) && Near(a.y, b.y, e) && Near(a.z, b.z, e); }
static bool QNearSign(const Qd& a, const Qd& b, double e = 1e-5) { return QNear(a, b, e) || QNear(a, { -b.w, -b.x, -b.y, -b.z }, e); }
static void RotateD(const Qd& q, const double v[3], double out[3]) {   // q v q^-1 (unit q)
    const Qd p = { 0, v[0], v[1], v[2] }, qc = { q.w, -q.x, -q.y, -q.z }, r = Ham(Ham(q, p), qc);
    out[0] = r.x; out[1] = r.y; out[2] = r.z;
}
// rotation matrix columns = images of the basis vectors; RW stores them as right / up / at
static void MatrixD(const Qd& q, double m[3][3]) {   // m[i] = image of basis vector i
    const double e[3][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };
    for (int i = 0; i < 3; i++) RotateD(q, e[i], m[i]);
}
static bool MatNearQ(const RwMatrix& m, const Qd& q, double e = 1e-5) {
    double r[3][3];
    MatrixD(q, r);
    return Near(m.right.x, r[0][0], e) && Near(m.right.y, r[0][1], e) && Near(m.right.z, r[0][2], e) &&
           Near(m.up.x, r[1][0], e) && Near(m.up.y, r[1][1], e) && Near(m.up.z, r[1][2], e) &&
           Near(m.at.x, r[2][0], e) && Near(m.at.y, r[2][1], e) && Near(m.at.z, r[2][2], e);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void QuatMatrixTests() {
    Section("RtQuat: ConvertToMatrix / UnitConvertToMatrix / ConvertFromMatrix (all four branches)");
    struct Case { double ax, ay, az, deg; const char* name; };
    const Case cases[] = {
        { 1, 2, 3, 40, "trace" }, { 0, 0, 1, 10, "trace small" }, { 1, 0, 0, 170, "x" }, { 0, 1, 0, 170, "y" }, { 0, 0, 1, 170, "z" },
        { 1, 1, 0, 175, "x/y" }, { 0.3, -1, 0.2, 200, "y neg" }, { -1, 0.2, 0.1, 250, "x neg" }, { 0.1, 0.2, -1, 190, "z neg" }, { 1, 0, 0, 180, "x 180" },
    };
    int branchCount[4] = {};
    for (const Case& c : cases) {
        const Qd qd = FromAxisAngleDeg(c.ax, c.ay, c.az, c.deg);
        RtQuat q = ToRt(qd);
        RwMatrix m{};
        m.flags = 0x20000;
        m.pos = { 7, 8, 9 };
        RtQuatUnitConvertToMatrix(&q, &m);
        CHECK(MatNearQ(m, ToQd(q), 2e-6));
        CHECK(m.flags == 3 && m.pos.x == 0.0f && m.pos.y == 0.0f && m.pos.z == 0.0f);
        // non-unit quaternion: RtQuatConvertToMatrix divides by the squared modulus
        RtQuat q2 = q;
        q2.real *= 2.5f; q2.imag.x *= 2.5f; q2.imag.y *= 2.5f; q2.imag.z *= 2.5f;
        RwMatrix m2{};
        m2.flags = 0x20000;
        m2.pos = { 1, 1, 1 };
        RtQuatConvertToMatrix(&q2, &m2);
        CHECK(MatNearQ(m2, ToQd(q), 2e-6));
        CHECK(m2.flags == 3 && m2.pos.x == 0.0f && m2.pos.z == 0.0f);
        // back
        RtQuat back{};
        CHECK(RtQuatConvertFromMatrix(&back, &m) == TRUE);
        CHECK(QNearSign(ToQd(back), ToQd(q), 5e-4));   // the exe's table sqrt (~1e-4 relative error) is used since 01r2
        const double len = std::sqrt(back.real * back.real + back.imag.x * back.imag.x + back.imag.y * back.imag.y + back.imag.z * back.imag.z);
        CHECK(Near(len, 1.0, 5e-4));
        RwMatrix m3{};
        RtQuatUnitConvertToMatrix(&back, &m3);
        CHECK(MatNearQ(m3, ToQd(q), 1e-3));
        // which branch did the matrix select (RW's rule: trace > 0, else the largest diagonal element)
        const double trace = (double)m.at.z + m.right.x + m.up.y;
        int br;
        if (trace > 0) br = 0;
        else if (m.right.x > m.up.y) br = m.right.x > m.at.z ? 1 : 3;
        else br = m.up.y > m.at.z ? 2 : 3;
        branchCount[br]++;
        if (br == 0) CHECK(back.real > 0.0f);                       // trace branch: real part positive
        else if (br == 1) CHECK(back.imag.x > 0.0f);                // largest-element branches: that component positive
        else if (br == 2) CHECK(back.imag.y > 0.0f);
        else CHECK(back.imag.z > 0.0f);
    }
    std::printf("  branches hit: trace %d, x %d, y %d, z %d\n", branchCount[0], branchCount[1], branchCount[2], branchCount[3]);
    CHECK(branchCount[0] > 0 && branchCount[1] > 0 && branchCount[2] > 0 && branchCount[3] > 0);
    // identity
    {
        RtQuat id; id.real = 1; id.imag = { 0, 0, 0 };
        RwMatrix m{};
        RtQuatUnitConvertToMatrix(&id, &m);
        CHECK(m.right.x == 1 && m.up.y == 1 && m.at.z == 1 && m.right.y == 0 && m.at.x == 0 && m.up.z == 0);
        RtQuat b{};
        CHECK(RtQuatConvertFromMatrix(&b, &m) == TRUE && b.real == 1.0f && b.imag.x == 0 && b.imag.y == 0 && b.imag.z == 0);
    }
    // NULL arguments are refused
    RtQuat dummy{};
    RwMatrix dm{};
    CHECK(RtQuatConvertFromMatrix(nullptr, &dm) == FALSE && RtQuatConvertFromMatrix(&dummy, nullptr) == FALSE);
}

static void RotateTests() {
    Section("RtQuatRotate: replace / preconcat / postconcat");
    const RwV3d axis = { 0.0f, 2.0f, 0.0f };   // not normalised
    RtQuat q{};
    RtQuat* r = RtQuatRotate(&q, &axis, 33.0f, rwCOMBINEREPLACE);
    CHECK(r == &q);
    const Qd R = FromAxisAngleDeg(0, 1, 0, 33.0);
    CHECK(QNear(ToQd(q), R));
    // angle 0: identity
    q.real = 5; q.imag = { 1, 2, 3 };
    RtQuatRotate(&q, &axis, 0.0f, rwCOMBINEREPLACE);
    CHECK(q.real == 1.0f && q.imag.x == 0.0f && q.imag.z == 0.0f);

    const Qd q0 = FromAxisAngleDeg(1, -2, 0.5, 71.0);
    for (int mode : { (int)rwCOMBINEPRECONCAT, (int)rwCOMBINEPOSTCONCAT }) {
        RtQuat t = ToRt(q0);
        const RwV3d ax2 = { 0.3f, 0.4f, -1.0f };
        CHECK(RtQuatRotate(&t, &ax2, -47.0f, (RwOpCombineType)mode) == &t);
        const Qd Rd = FromAxisAngleDeg(0.3, 0.4, -1.0, -47.0);
        const Qd q0f = ToQd(ToRt(q0));
        const Qd expect = mode == rwCOMBINEPOSTCONCAT ? Ham(Rd, q0f) : Ham(q0f, Rd);
        const Qd other  = mode == rwCOMBINEPOSTCONCAT ? Ham(q0f, Rd) : Ham(Rd, q0f);
        CHECK(QNear(ToQd(t), expect, 2e-6));
        CHECK(!QNear(ToQd(t), other, 1e-3));                                                  // mutation: the two modes are not interchangeable
        // vector view: POST = rotate by q0 first then by R; PRE = rotate by R first then by q0
        const double v[3] = { 1.0, -2.0, 0.5 };
        double a[3], b[3], e[3];
        if (mode == rwCOMBINEPOSTCONCAT) { RotateD(q0f, v, a); RotateD(Rd, a, b); }
        else { RotateD(Rd, v, a); RotateD(q0f, a, b); }
        RwV3d in = { 1.0f, -2.0f, 0.5f }, out{};
        RtQuatTransformVectors(&out, &in, 1, &t);
        RotateD(ToQd(t), v, e);
        CHECK(Near(out.x, b[0], 1e-5) && Near(out.y, b[1], 1e-5) && Near(out.z, b[2], 1e-5));
        CHECK(Near(out.x, e[0], 1e-5) && Near(out.y, e[1], 1e-5) && Near(out.z, e[2], 1e-5));
    }
    CHECK(RtQuatRotate(nullptr, &axis, 1.0f, rwCOMBINEREPLACE) == nullptr);
    CHECK(RtQuatRotate(&q, nullptr, 1.0f, rwCOMBINEREPLACE) == nullptr);
    CHECK(RtQuatRotate(&q, &axis, 1.0f, (RwOpCombineType)9) == nullptr);
}

static void TransformTests() {
    Section("RtQuatTransformVectors");
    const Qd qd = FromAxisAngleDeg(0.2, 1, -0.7, 123.0);
    for (double scale : { 1.0, 2.0 }) {
        RtQuat q = ToRt(qd);
        q.real *= (float)scale; q.imag.x *= (float)scale; q.imag.y *= (float)scale; q.imag.z *= (float)scale;
        RwV3d in[5] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1.5f, -2.0f, 3.25f }, { -10.0f, 4.0f, 0.5f } }, out[5]{};
        CHECK(RtQuatTransformVectors(out, in, 5, &q) == out);
        bool ok = true;
        for (int i = 0; i < 5; i++) {
            const double v[3] = { in[i].x, in[i].y, in[i].z };
            double e[3];
            RotateD(ToQd(ToRt(qd)), v, e);                                                     // |q| = scale -> the result is scaled by |q|^2
            ok &= Near(out[i].x, e[0] * scale * scale, 2e-5 * scale * scale) && Near(out[i].y, e[1] * scale * scale, 2e-5 * scale * scale) && Near(out[i].z, e[2] * scale * scale, 2e-5 * scale * scale);
        }
        CHECK(ok);
    }
    RtQuat q = ToRt(qd);
    RwV3d in{ 1, 2, 3 }, out{ -1, -1, -1 };
    CHECK(RtQuatTransformVectors(&out, &in, 0, &q) == &out && out.x == -1.0f);                // no points: untouched, returns the output pointer
    // matrix view: M(q) maps basis vector i to the i-th matrix row
    RwMatrix m{};
    RtQuatUnitConvertToMatrix(&q, &m);
    RwV3d ex = { 1, 0, 0 }, ey = { 0, 1, 0 }, o1{}, o2{};
    RtQuatTransformVectors(&o1, &ex, 1, &q);
    RtQuatTransformVectors(&o2, &ey, 1, &q);
    CHECK(Near(o1.x, m.right.x, 2e-5) && Near(o1.y, m.right.y, 2e-5) && Near(o1.z, m.right.z, 2e-5));
    CHECK(Near(o2.x, m.up.x, 2e-5) && Near(o2.y, m.up.y, 2e-5) && Near(o2.z, m.up.z, 2e-5));
}

static Qd SlerpD(Qd a, Qd b, double t) {
    double dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (dot < 0) { b = { -b.w, -b.x, -b.y, -b.z }; dot = -dot; }
    if (dot > 0.9999999) return { a.w * (1 - t) + b.w * t, a.x * (1 - t) + b.x * t, a.y * (1 - t) + b.y * t, a.z * (1 - t) + b.z * t };
    const double om = std::acos(std::min(dot, 1.0)), s = std::sin(om);
    const double f = std::sin((1 - t) * om) / s, g = std::sin(t * om) / s;
    return { a.w * f + b.w * g, a.x * f + b.x * g, a.y * f + b.y * g, a.z * f + b.z * g };
}

static void SlerpTests() {
    Section("RtQuatSetupSlerpCache / RtQuatSlerp");
    struct Pair { Qd a, b; const char* name; };
    const Qd base = FromAxisAngleDeg(0.3, 1, -0.2, 20);
    const Pair pairs[] = {
        { base, Ham(FromAxisAngleDeg(0, 1, 0, 20), base), "20 deg" },
        { base, Ham(FromAxisAngleDeg(0.5, 0.5, 1, 100), base), "100 deg (c >= 0.5)" },
        { base, Ham(FromAxisAngleDeg(0.5, 0.5, 1, 200), base), "200 deg (dot < 0)" },
        { base, Ham(FromAxisAngleDeg(1, 0, 1, 140), base), "140 deg (c < 0.5)" },
        { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, "orthogonal (c = 0)" },
        { base, Ham(FromAxisAngleDeg(1, 1, 1, 0.01), base), "0.01 deg (nearly parallel)" },
        { base, { -base.w, -base.x, -base.y, -base.z }, "opposite signs, same rotation" },
        { { 1, 0, 0, 0 }, FromAxisAngleDeg(0, 0, 1, 90), "90 deg z" },
    };
    for (const Pair& p : pairs) {
        RtQuat a = ToRt(p.a), b = ToRt(p.b);
        RtQuatSlerpCache cache{};
        const RtQuat a0 = a, b0 = b;
        RtQuatSetupSlerpCache(&a, &b, &cache);
        CHECK(std::memcmp(&a, &a0, sizeof(a)) == 0 && std::memcmp(&b, &b0, sizeof(b)) == 0);   // inputs untouched
        const Qd fa = ToQd(a), fb = ToQd(b);
        double dot = fa.w * fb.w + fa.x * fb.x + fa.y * fb.y + fa.z * fb.z;
        const double om = std::acos(std::min(std::fabs(dot), 1.0));
        std::printf("  %-28s omega %.6f (ref %.6f) nearlyZero %d\n", p.name, cache.omega, om, cache.nearlyZeroOm);
        CHECK(cache.nearlyZeroOm ? cache.omega < 1e-3 : Near(cache.omega, om, 2e-6));   // near 1 the float dot has no resolution left
        CHECK(cache.nearlyZeroOm == (std::fabs(dot) >= 0.99999 ? 1 : 0));
        if (!cache.nearlyZeroOm) {
            const double inv = 1.0 / std::sin(om), sg = dot < 0 ? -1.0 : 1.0;
            CHECK(Near(cache.raFrom.real, fa.w * inv, 2e-5) && Near(cache.raFrom.imag.y, fa.y * inv, 2e-5));
            CHECK(Near(cache.raTo.real, sg * fb.w * inv, 2e-5) && Near(cache.raTo.imag.x, sg * fb.x * inv, 2e-5));
        } else {
            CHECK(cache.raFrom.real == a.real && cache.raFrom.imag.x == a.imag.x);               // unscaled
        }
        for (float t : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
            RtQuat res{};
            RtQuatSlerp(&res, &a, &b, t, &cache);
            if (t == 0.0f) CHECK(std::memcmp(&res, &a, sizeof(res)) == 0);                     // before the start: the start quaternion itself
            else if (t == 1.0f) CHECK(std::memcmp(&res, &b, sizeof(res)) == 0);                // after the end: the end quaternion itself
            else {
                const Qd ref = SlerpD(fa, fb, t);
                const bool ok = QNear(ToQd(res), ref, 3e-5);
                CHECK(ok);
                if (!ok) std::printf("    t=%g got (%g %g %g %g) want (%g %g %g %g)\n", t, res.real, res.imag.x, res.imag.y, res.imag.z, ref.w, ref.x, ref.y, ref.z);
                // mutation: t and 1-t are different except at the midpoint of a symmetric pair
                if (t == 0.25f && std::fabs(om) > 0.1) {
                    RtQuat res2{};
                    RtQuatSlerp(&res2, &a, &b, 0.75f, &cache);
                    CHECK(!QNear(ToQd(res), ToQd(res2), 1e-3));
                }
            }
        }
    }
    // t outside the range (before / after)
    RtQuat a = ToRt(pairs[1].a), b = ToRt(pairs[1].b), res{};
    RtQuatSlerpCache cache{};
    RtQuatSetupSlerpCache(&a, &b, &cache);
    RtQuatSlerp(&res, &a, &b, -3.0f, &cache);
    CHECK(std::memcmp(&res, &a, sizeof(res)) == 0);
    RtQuatSlerp(&res, &a, &b, 2.0f, &cache);
    CHECK(std::memcmp(&res, &b, sizeof(res)) == 0);
    // midpoint of two rotations about the same axis = half angle
    const Qd z0 = FromAxisAngleDeg(0, 0, 1, 20), z1 = FromAxisAngleDeg(0, 0, 1, 80);
    RtQuat za = ToRt(z0), zb = ToRt(z1);
    RtQuatSetupSlerpCache(&za, &zb, &cache);
    RtQuatSlerp(&res, &za, &zb, 0.5f, &cache);
    CHECK(QNear(ToQd(res), FromAxisAngleDeg(0, 0, 1, 50), 2e-6));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// RtAnim
static void ZeroInterp(void* out, void*, void*, float, void*) { std::memset(out, 0, 0x1C); }   // what the game's RpAnimBlendKeyFrameInterpolate does (RpHAnimBlendInterpFrame = 0x1C)
static void ApplyDummy(void*, void*) {}
constexpr int kGameScheme = 0x253F2FB;   // rwID_RPANIMBLENDPLUGIN

static void SchemeTests() {
    Section("RtAnim: scheme registration / animation create / destroy");
    CHECK(RtAnimInitialize() == TRUE);
    CHECK(rw::AnimInterpolatorInfo::find(kGameScheme) == nullptr);
    RtAnimInterpolatorInfo info{};
    info.typeID = kGameScheme;
    info.interpKeyFrameSize = 0x1C;
    info.animKeyFrameSize = sizeof(RpHAnimKeyFrame);
    info.keyFrameApplyCB = ApplyDummy;
    info.keyFrameBlendCB = RpHAnimKeyFrameBlend;
    info.keyFrameInterpolateCB = ZeroInterp;
    info.keyFrameAddCB = RpHAnimKeyFrameAdd;
    info.keyFrameMulRecipCB = RpHAnimKeyFrameMulRecip;
    info.keyFrameStreamReadCB = RpHAnimKeyFrameStreamRead;
    info.keyFrameStreamWriteCB = RpHAnimKeyFrameStreamWrite;
    info.keyFrameStreamGetSizeCB = RpHAnimKeyFrameStreamGetSize;
    info.customDataSize = 0;
    CHECK(RtAnimRegisterInterpolationScheme(&info) == TRUE);
    rw::AnimInterpolatorInfo* ri = rw::AnimInterpolatorInfo::find(kGameScheme);
    CHECK(ri != nullptr && ri->id == kGameScheme && ri->interpKeyFrameSize == 0x1C && ri->animKeyFrameSize == 0x24 && ri->customDataSize == 0);
    CHECK(ri->applyCB == ApplyDummy && ri->blendCB == RpHAnimKeyFrameBlend && ri->interpCB == ZeroInterp && ri->addCB == RpHAnimKeyFrameAdd && ri->mulRecipCB == RpHAnimKeyFrameMulRecip);
    CHECK(RtAnimRegisterInterpolationScheme(&info) == FALSE);                                    // duplicate id
    RtAnimInterpolatorInfo std1 = info;
    std1.typeID = 1;
    CHECK(RtAnimRegisterInterpolationScheme(&std1) == FALSE);                                    // librw's own hanim scheme already holds the id

    // AnimationCreate: header + key frames, no custom data
    RtAnimAnimation* a = RtAnimAnimationCreate(kGameScheme, 6, 5, 2.5f);
    CHECK(a != nullptr);
    CHECK(a->interpInfo == ri && a->numFrames == 6 && a->flags == 5 && a->duration == 2.5f);
    CHECK(a->keyframes == reinterpret_cast<char*>(a) + 0x18 && a->customData == nullptr && sizeof(rw::Animation) == 0x18);
    std::memset(a->keyframes, 0xAB, 6 * 0x24);                                                  // the whole key-frame array is the game's to write
    CHECK(RtAnimAnimationDestroy(a) == TRUE);
    // numFrames 0 (RpAnimBlendCreateAnimationForHierarchy), the game raises numFrames afterwards without growing the block
    a = RtAnimAnimationCreate(kGameScheme, 0, 0, 0.0f);
    CHECK(a != nullptr && a->numFrames == 0 && a->keyframes == reinterpret_cast<char*>(a) + 0x18);
    a->numFrames = 8;
    CHECK(RtAnimAnimationDestroy(a) == TRUE);
    CHECK(RtAnimAnimationCreate(0x7777, 4, 0, 1.0f) == nullptr);                                // unknown scheme
    // custom data
    static RtAnimInterpolatorInfo cinfo = info;
    cinfo.typeID = 0x254;
    cinfo.customDataSize = 12;
    CHECK(RtAnimRegisterInterpolationScheme(&cinfo) == TRUE);
    a = RtAnimAnimationCreate(0x254, 3, 0, 1.0f);
    CHECK(a != nullptr && a->customData == static_cast<char*>(a->keyframes) + 3 * 0x24);
    RtAnimAnimationDestroy(a);
}

static void MakeStdKeyFrames(rw::HAnimKeyFrame* kf, int N, int stages) {
    // stage s (0 .. stages-1): time s, frame index s*N + i, previous key frame of the node = (s-1)*N + i (stage 0: itself)
    for (int s = 0; s < stages; s++) {
        for (int i = 0; i < N; i++) {
            const float ang = 0.2f + 0.3f * i + 0.7f * s;
            rw::V3d axis = { 0.1f * i, 1.0f, 0.4f * s - 0.3f };
            rw::Quat q = rw::Quat::rotation(ang, axis);
            kf[s * N + i] = { &kf[(s ? s - 1 : 0) * N + i], float(s), q, { float(i) + 3.0f * s, 0.5f * i, -1.0f + s } };
        }
    }
}

static void InterpolatorChecks(rw::AnimInterpolator* ip, RtAnimAnimation* anim, int N) {
    CHECK(ip->currentAnim == anim && ip->currentTime == 0.0f);
    CHECK(ip->currentInterpKeyFrameSize == anim->interpInfo->interpKeyFrameSize && ip->currentAnimKeyFrameSize == anim->interpInfo->animKeyFrameSize);
    CHECK(ip->applyCB == anim->interpInfo->applyCB && ip->interpCB == anim->interpInfo->interpCB && ip->blendCB == anim->interpInfo->blendCB && ip->addCB == anim->interpInfo->addCB);
    const int sz = anim->interpInfo->animKeyFrameSize;
    CHECK(ip->nextFrame == static_cast<char*>(anim->keyframes) + 2 * N * sz);
    bool ptrs = true;
    for (int i = 0; i < N; i++) {
        rw::InterpFrameHeader* f = ip->getInterpFrame(i);
        ptrs &= f->keyFrame1 == reinterpret_cast<rw::KeyFrameHeader*>(static_cast<char*>(anim->keyframes) + i * sz) &&
                f->keyFrame2 == reinterpret_cast<rw::KeyFrameHeader*>(static_cast<char*>(anim->keyframes) + (N + i) * sz);
    }
    CHECK(ptrs);
}

static void InterpolationTests() {
    Section("RtAnim: SetCurrentAnim + stepping the standard scheme (id 1)");
    constexpr int N = 4;
    rw::AnimInterpolator* ip = rw::AnimInterpolator::create(N, 36);
    CHECK(ip != nullptr && ip->maxInterpKeyFrameSize == 36 && ip->numNodes == N);
    RtAnimAnimation* anim = RtAnimAnimationCreate(1, 3 * N, 0, 2.0f);
    CHECK(anim != nullptr && anim->interpInfo == rw::AnimInterpolatorInfo::find(1) && anim->customData == nullptr);
    rw::HAnimKeyFrame* kf = static_cast<rw::HAnimKeyFrame*>(anim->keyframes);
    MakeStdKeyFrames(kf, N, 3);
    CHECK(RtAnimInterpolatorSetCurrentAnim(ip, anim) == TRUE);
    InterpolatorChecks(ip, anim, N);
    // t = 0: interpolated frame == first key frame of the node
    {
        bool ok = true;
        for (int i = 0; i < N; i++) {
            const rw::HAnimInterpFrame* f = reinterpret_cast<const rw::HAnimInterpFrame*>(ip->getInterpFrame(i));
            ok &= QNear({ f->q.w, f->q.x, f->q.y, f->q.z }, { kf[i].q.w, kf[i].q.x, kf[i].q.y, kf[i].q.z }, 1e-6) && Near(f->t.x, kf[i].t.x) && Near(f->t.y, kf[i].t.y);
        }
        CHECK(ok);
    }
    // step: 0.5 (between stage 0 and 1), then to 1.5 (between stage 1 and 2)
    for (float t : { 0.5f, 1.5f }) {
        ip->addTime(t == 0.5f ? 0.5f : 1.0f);
        CHECK(Near(ip->currentTime, t));
        const int s = (int)std::floor(t);
        bool ok = true;
        for (int i = 0; i < N; i++) {
            const rw::HAnimInterpFrame* f = reinterpret_cast<const rw::HAnimInterpFrame*>(ip->getInterpFrame(i));
            const rw::HAnimKeyFrame& k1 = kf[s * N + i];
            const rw::HAnimKeyFrame& k2 = kf[(s + 1) * N + i];
            const double u = t - s;
            const Qd ref = SlerpD({ k1.q.w, k1.q.x, k1.q.y, k1.q.z }, { k2.q.w, k2.q.x, k2.q.y, k2.q.z }, u);
            ok &= QNear({ f->q.w, f->q.x, f->q.y, f->q.z }, ref, 2e-5) && Near(f->t.x, k1.t.x + (k2.t.x - k1.t.x) * u, 1e-5) && Near(f->t.z, k1.t.z + (k2.t.z - k1.t.z) * u, 1e-5);
            ok &= ip->getInterpFrame(i)->keyFrame1 == reinterpret_cast<const rw::KeyFrameHeader*>(&k1) && ip->getInterpFrame(i)->keyFrame2 == reinterpret_cast<const rw::KeyFrameHeader*>(&k2);
        }
        CHECK(ok);
    }
    // restart: the animation is set again
    CHECK(RtAnimInterpolatorSetCurrentAnim(ip, anim) == TRUE);
    InterpolatorChecks(ip, anim, N);
    // an interpolation frame bigger than the interpolator can hold is refused (the exe would overflow)
    rw::AnimInterpolator* tiny = rw::AnimInterpolator::create(N, 16);
    CHECK(RtAnimInterpolatorSetCurrentAnim(tiny, anim) == FALSE && tiny->currentAnim == nullptr);
    tiny->destroy();
    RtAnimAnimationDestroy(anim);
    ip->destroy();

    Section("RtAnim: SetCurrentAnim order with the game's scheme (interpolate wipes the frame, THEN the key-frame pointers are stored)");
    rw::AnimInterpolator* gi = rw::AnimInterpolator::create(5, 0x24);
    // poison the frames first so that a missing interpolate call would be seen
    std::memset(gi->getFrames(), 0x5A, 5 * 0x24);
    RtAnimAnimation* ga = RtAnimAnimationCreate(kGameScheme, 0, 0, 0.0f);
    ga->numFrames = 2 * 5;                                                                       // RpAnimBlendCreateAnimationForHierarchy
    CHECK(RtAnimInterpolatorSetCurrentAnim(gi, ga) == TRUE);
    InterpolatorChecks(gi, ga, 5);
    {
        bool tail = true;
        for (int i = 0; i < 5; i++) {
            const uint8_t* f = reinterpret_cast<const uint8_t*>(gi->getInterpFrame(i));
            for (int b = 8; b < 0x1C; b++) tail &= f[b] == 0;                                    // zero-filled by the interpolate callback (0x5A before)
        }
        CHECK(tail);
        CHECK(reinterpret_cast<uint8_t*>(gi->getInterpFrame(1)) == reinterpret_cast<uint8_t*>(gi->getFrames()) + 0x1C);   // frames are packed at the scheme's size
    }
    RtAnimAnimationDestroy(ga);
    gi->destroy();
}

static void StreamTests() {
    Section("RtAnim: game scheme key frames through librw's Animation stream (trampolines to the RpHAnimKeyFrame* callbacks)");
    constexpr int N = 3;
    RtAnimAnimation* a = RtAnimAnimationCreate(kGameScheme, 2 * N, 3, 1.25f);
    rw::HAnimKeyFrame* kf = static_cast<rw::HAnimKeyFrame*>(a->keyframes);
    MakeStdKeyFrames(kf, N, 2);
    CHECK(a->streamGetSize() == 20 + 2 * N * 36);                                                // 5 header dwords + RpHAnimKeyFrameStreamGetSize
    rw::uint8 buf[4096] = {};
    rw::StreamMemory out;
    out.open(buf, 0, sizeof(buf));
    CHECK(a->streamWrite(&out));
    const rw::uint32 written = out.tell();
    CHECK(written == 12 + a->streamGetSize());                                                   // chunk header + payload
    out.close();
    rw::StreamMemory in;
    in.open(buf, written);
    rw::uint32 len = 0, ver = 0;
    CHECK(rw::findChunk(&in, rw::ID_ANIMANIMATION, &len, &ver) && len == (rw::uint32)a->streamGetSize());
    rw::Animation* b = rw::Animation::streamRead(&in);
    CHECK(b != nullptr && b->interpInfo == a->interpInfo && b->numFrames == 2 * N && b->flags == 3 && b->duration == 1.25f);
    if (b) {
        const rw::HAnimKeyFrame* k2 = static_cast<const rw::HAnimKeyFrame*>(b->keyframes);
        bool same = true;
        for (int i = 0; i < 2 * N; i++) {
            same &= k2[i].time == kf[i].time && k2[i].q.x == kf[i].q.x && k2[i].q.w == kf[i].q.w && k2[i].t.y == kf[i].t.y && (k2[i].prev - k2) == (kf[i].prev - kf);
        }
        CHECK(same);
        RtAnimAnimationDestroy(b);
    }
    in.close();
    RtAnimAnimationDestroy(a);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void CollectFrames(RwFrame* f, std::vector<RwFrame*>& out) {
    out.push_back(f);
    for (RwFrame* c = f->child; c; c = c->next) CollectFrames(c, out);
}
static RpHAnimHierarchy* HierarchyOf(RpClump* clump) {
    std::vector<RwFrame*> fs;
    CollectFrames(RpClumpGetFrame(clump), fs);
    for (RwFrame* f : fs) if (RpHAnimFrameGetHierarchy(f)) return RpHAnimFrameGetHierarchy(f);
    return nullptr;
}

static void ModelTests(const char* path) {
    Section((std::string("SA model hierarchy: ") + path).c_str());
    auto bytes = ReadFile(path);
    if (bytes.empty()) { CHECK(false); return; }
    rw::StreamMemory ms;
    ms.open(bytes.data(), (uint32_t)bytes.size());
    uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&ms, rw::ID_CLUMP, &len, &ver));
    RpClump* clump = RpClumpStreamRead(&ms);
    CHECK(clump != nullptr);
    if (!clump) return;
    RpHAnimHierarchy* h = HierarchyOf(clump);
    CHECK(h != nullptr);
    if (!h) { RpClumpDestroy(clump); return; }
    const int N = h->numNodes;
    std::printf("  hierarchy: %d nodes, max interp key frame %d\n", N, h->interpolator->maxInterpKeyFrameSize);

    // 1) the game's way (ClumpModelInfo.cpp:99): empty animation of the game's scheme, numFrames = 2 * nodes
    RtAnimAnimation* ga = RtAnimAnimationCreate(kGameScheme, 0, 0, 0.0f);   // RpAnimBlendCreateAnimationForHierarchy
    CHECK(ga != nullptr);
    ga->numFrames = 2 * N;
    CHECK(RtAnimInterpolatorSetCurrentAnim(h->interpolator, ga) == TRUE);
    InterpolatorChecks(h->interpolator, ga, N);
    CHECK(h->interpolator->currentInterpKeyFrameSize == 0x1C);
    RtAnimAnimationDestroy(ga);
    h->interpolator->currentAnim = nullptr;

    // 2) standard scheme with real data stepping: 3 stages, node i
    RtAnimAnimation* sa = RtAnimAnimationCreate(1, 3 * N, 0, 2.0f);
    rw::HAnimKeyFrame* kf = static_cast<rw::HAnimKeyFrame*>(sa->keyframes);
    MakeStdKeyFrames(kf, N, 3);
    CHECK(RtAnimInterpolatorSetCurrentAnim(h->interpolator, sa) == TRUE);
    InterpolatorChecks(h->interpolator, sa, N);
    h->interpolator->addTime(0.75f);
    h->interpolator->addTime(0.75f);
    bool ok = true;
    for (int i = 0; i < N; i++) {
        const rw::HAnimInterpFrame* f = reinterpret_cast<const rw::HAnimInterpFrame*>(h->interpolator->getInterpFrame(i));
        const double u = 0.5;
        const rw::HAnimKeyFrame& k1 = kf[N + i];
        const rw::HAnimKeyFrame& k2 = kf[2 * N + i];
        const Qd ref = SlerpD({ k1.q.w, k1.q.x, k1.q.y, k1.q.z }, { k2.q.w, k2.q.x, k2.q.y, k2.q.z }, u);
        ok &= QNear({ f->q.w, f->q.x, f->q.y, f->q.z }, ref, 2e-5) && Near(f->t.x, k1.t.x + (k2.t.x - k1.t.x) * u, 1e-5);
    }
    CHECK(ok);
    // the destroyed animation must not stay set on the hierarchy (the clump destroy path reads currentAnim)
    h->interpolator->currentAnim = nullptr;
    RtAnimAnimationDestroy(sa);
    CHECK(RpClumpDestroy(clump) == TRUE);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Bit-exact differential against the exe: tools/standalone/quat_unicorn.py runs the exe's RtQuat* / RpHAnimKeyFrame* / RtAnimBlendKeyFrameApply code under
// Unicorn with the x87 precision control at 24 bits (the state the game runs in after RwEngineStart) and records the output bits. The shim must give the same
// bits under the same precision control. Deliberate: the exe's RwSqrt is table based (replaced by fsqrt in the recording), RtQuatRotate cases use axes of
// squared length exactly 1 (RwV3dNormalize is table based too).
#include "rw_quat_cases.inc"
#include <float.h>

static uint32_t FBits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
static float    BitsF(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
// exact float equality, NaN == NaN; +-0 differ (the exe's stores are compared as bits)
static bool BitsEq(float a, uint32_t expected) { return FBits(a) == expected; }
static int g_exeMismatch = 0;
static void Mism(const char* what, int i, int k, float got, uint32_t exp) {
    if (g_exeMismatch++ < 12) std::printf("  MISMATCH %s case %d field %d: shim %.9g (%08x) exe %.9g (%08x)\n", what, i, k, got, FBits(got), BitsF(exp), exp);
}
template <class T, size_t N, size_t W>
static constexpr size_t Rows(const T (&)[N][W]) { return N; }

struct Pc24 {
    unsigned old = 0;
    Pc24() { _controlfp_s(&old, 0, 0); unsigned cur; _controlfp_s(&cur, _PC_24, _MCW_PC); }
    ~Pc24() { unsigned cur; _controlfp_s(&cur, old & _MCW_PC, _MCW_PC); }
};

static void ExeDifferentialTests() {
    Pc24 pc;
    auto cmp = [&](const char* what, int i, const float* got, const uint32_t* exp, int n) {
        int bad = 0;
        for (int k = 0; k < n; k++) { if (!BitsEq(got[k], exp[k])) { bad++; Mism(what, i, k, got[k], exp[k]); } }
        CHECK(bad == 0);
    };
    Section("exe differential: RtQuatConvertFromMatrix 0x7EB5C0 (all four branches)");
    for (size_t i = 0; i < Rows(kConvFromMat); i++) {
        const uint32_t* r = kConvFromMat[i];
        RwMatrix m{}; RtQuat q{};
        m.right = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) };
        m.up    = { BitsF(r[3]), BitsF(r[4]), BitsF(r[5]) };
        m.at    = { BitsF(r[6]), BitsF(r[7]), BitsF(r[8]) };
        CHECK(RtQuatConvertFromMatrix(&q, &m) == TRUE);
        const float got[4] = { q.imag.x, q.imag.y, q.imag.z, q.real };
        cmp("ConvertFromMatrix", (int)i, got, r + 9, 4);
    }
    Section("exe differential: RtQuatTransformVectors 0x7EBBB0");
    for (size_t i = 0; i < Rows(kTransformVec); i++) {
        const uint32_t* r = kTransformVec[i];
        RtQuat q; q.imag = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) }; q.real = BitsF(r[3]);
        RwV3d in[3], out[3];
        for (int k = 0; k < 3; k++) in[k] = { BitsF(r[4 + 3 * k]), BitsF(r[5 + 3 * k]), BitsF(r[6 + 3 * k]) };
        RtQuatTransformVectors(out, in, 3, &q);
        float got[9]; for (int k = 0; k < 3; k++) { got[3 * k] = out[k].x; got[3 * k + 1] = out[k].y; got[3 * k + 2] = out[k].z; }
        cmp("TransformVectors", (int)i, got, r + 13, 9);
    }
    Section("exe differential: RtQuatSetupSlerpCache 0x7EC220");
    int nearlyZero = 0;
    for (size_t i = 0; i < Rows(kSlerpCache); i++) {
        const uint32_t* r = kSlerpCache[i];
        RtQuat a, b; a.imag = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) }; a.real = BitsF(r[3]); b.imag = { BitsF(r[4]), BitsF(r[5]), BitsF(r[6]) }; b.real = BitsF(r[7]);
        RtQuatSlerpCache c; std::memset(&c, 0xcc, sizeof c);
        RtQuatSetupSlerpCache(&a, &b, &c);
        const float got[8] = { c.raFrom.imag.x, c.raFrom.imag.y, c.raFrom.imag.z, c.raFrom.real, c.raTo.imag.x, c.raTo.imag.y, c.raTo.imag.z, c.raTo.real };
        cmp("SlerpCache", (int)i, got, r + 8, 8);
        const float om[1] = { c.omega };
        cmp("SlerpCache.omega", (int)i, om, r + 16, 1);
        CHECK(c.nearlyZeroOm == (int32_t)r[17]);
        nearlyZero += c.nearlyZeroOm;
    }
    CHECK(nearlyZero > 20 && nearlyZero < (int)Rows(kSlerpCache) - 20);   // both paths covered
    Section("exe differential: RtQuatRotate 0x7EB7C0 (replace / preconcat / postconcat)");
    for (size_t i = 0; i < Rows(kRotate); i++) {
        const uint32_t* r = kRotate[i];
        RtQuat q; q.imag = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) }; q.real = BitsF(r[3]);
        const RwV3d ax = { BitsF(r[4]), BitsF(r[5]), BitsF(r[6]) };
        RtQuatRotate(&q, &ax, BitsF(r[7]), (RwOpCombineType)r[8]);
        const float got[4] = { q.imag.x, q.imag.y, q.imag.z, q.real };
        cmp("Rotate", (int)i, got, r + 9, 4);
    }
    Section("exe differential: RtQuatSetupSlerpCache + RtQuatSlerp (inlined macro of BoneNode_c::BlendKeyframe 0x616E30, blend in (0, 1))");
    for (size_t i = 0; i < Rows(kBoneBlend); i++) {
        const uint32_t* r = kBoneBlend[i];
        RtQuat a, b, res; a.imag = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) }; a.real = BitsF(r[3]); b.imag = { BitsF(r[4]), BitsF(r[5]), BitsF(r[6]) }; b.real = BitsF(r[7]);
        RtQuatSlerpCache c;
        RtQuatSetupSlerpCache(&a, &b, &c);
        res = a;
        RtQuatSlerp(&res, &a, &b, BitsF(r[8]), &c);
        const float got[4] = { res.imag.x, res.imag.y, res.imag.z, res.real };
        cmp("Slerp", (int)i, got, r + 9, 4);
    }
    Section("exe differential: RtQuatUnitConvertToMatrix (inlined in RtAnimBlendKeyFrameApply 0x4D5FA0)");
    for (size_t i = 0; i < Rows(kUnitToMat); i++) {
        const uint32_t* r = kUnitToMat[i];
        RtQuat q; q.imag = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) }; q.real = BitsF(r[3]);
        RwMatrix m; std::memset(&m, 0xcc, sizeof m);
        RtQuatUnitConvertToMatrix(&q, &m);
        const float got[10] = { m.right.x, m.right.y, m.right.z, m.up.x, m.up.y, m.up.z, m.at.x, m.at.y, m.at.z, 0 };
        const uint32_t exp[9] = { r[4 + 0], r[4 + 1], r[4 + 2], r[4 + 4], r[4 + 5], r[4 + 6], r[4 + 8], r[4 + 9], r[4 + 10] };
        cmp("UnitConvertToMatrix", (int)i, got, exp, 9);
        CHECK(m.flags == r[4 + 3]);
    }
    Section("exe differential: RtQuatConvertToMatrix (inlined in CPhysical::PositionAttachedEntity 0x5471CF..0x547352)");
    for (size_t i = 0; i < Rows(kQuatToMat); i++) {
        const uint32_t* r = kQuatToMat[i];
        RtQuat q; q.imag = { BitsF(r[0]), BitsF(r[1]), BitsF(r[2]) }; q.real = BitsF(r[3]);
        RwMatrix m; std::memset(&m, 0xcc, sizeof m);
        RtQuatConvertToMatrix(&q, &m);
        const float got[12] = { m.right.x, m.right.y, m.right.z, m.up.x, m.up.y, m.up.z, m.at.x, m.at.y, m.at.z, m.pos.x, m.pos.y, m.pos.z };
        const uint32_t exp[12] = { r[4], r[5], r[6], r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15], r[16] };
        cmp("ConvertToMatrix", (int)i, got, exp, 12);
        CHECK(m.flags == r[7]);
    }
    Section("exe differential: RpHAnimKeyFrameBlend 0x7C60C0 / Add 0x7C6720 / MulRecip 0x7C65C0");
    auto load = [](const uint32_t* r, rw::HAnimKeyFrame& f) {
        std::memset(&f, 0, sizeof f); f.time = BitsF(r[1]);
        f.q.x = BitsF(r[2]); f.q.y = BitsF(r[3]); f.q.z = BitsF(r[4]); f.q.w = BitsF(r[5]); f.t.x = BitsF(r[6]); f.t.y = BitsF(r[7]); f.t.z = BitsF(r[8]);
    };
    auto fvals = [](const rw::HAnimKeyFrame& f, float* o) { o[0] = f.q.x; o[1] = f.q.y; o[2] = f.q.z; o[3] = f.q.w; o[4] = f.t.x; o[5] = f.t.y; o[6] = f.t.z; };
    for (size_t i = 0; i < Rows(kBlend); i++) {
        const uint32_t* r = kBlend[i];
        rw::HAnimKeyFrame a, b, out; load(r, a); load(r + 9, b); std::memset(&out, 0, sizeof out);
        RpHAnimKeyFrameBlend(&out, &a, &b, BitsF(r[18]));
        float g[7]; fvals(out, g);
        cmp("KeyFrameBlend", (int)i, g, r + 19 + 2, 7);
        fvals(b, g);   // in2 is negated in place when the quaternions face away from each other
        cmp("KeyFrameBlend.in2", (int)i, g, r + 28 + 2, 7);
    }
    for (size_t i = 0; i < Rows(kAdd); i++) {
        const uint32_t* r = kAdd[i];
        rw::HAnimKeyFrame a, b, out; load(r, a); load(r + 9, b); std::memset(&out, 0, sizeof out);
        RpHAnimKeyFrameAdd(&out, &a, &b);
        float g[7]; fvals(out, g);
        cmp("KeyFrameAdd", (int)i, g, r + 18 + 2, 7);
    }
    for (size_t i = 0; i < Rows(kMulRecip); i++) {
        const uint32_t* r = kMulRecip[i];
        rw::HAnimKeyFrame f, s; load(r, f); load(r + 9, s);
        RpHAnimKeyFrameMulRecip(&f, &s);
        float g[7]; fvals(f, g);
        cmp("KeyFrameMulRecip", (int)i, g, r + 18 + 2, 7);
    }
    CHECK(g_exeMismatch == 0);
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    HWND wnd = CreateWindowA("STATIC", "rw_rtanim_rtquat_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    CHECK(RpWorldPluginAttach() == TRUE);
    CHECK(RpSkinPluginAttach() == TRUE);
    CHECK(RtAnimInitialize() == TRUE);
    CHECK(RpHAnimPluginAttach() == TRUE);
    rw::registerAnisotropyPlugin();
    rw::EngineOpenParams params{};
    params.window = wnd;
    rw::d3d::renderdevice.system = [](rw::DeviceReq, void*, int32_t) -> int32_t { return 1; };   // device-less
    CHECK(rw::Engine::open(&params));
    rw::Engine::s_plglist.construct(rw::engine);
    rw::Driver::s_plglist[rw::PLATFORM_NULL].construct(rw::engine->driver[rw::PLATFORM_NULL]);
    CHECK(rw::AnimInterpolatorInfo::find(1) != nullptr);

    ExeDifferentialTests();
    QuatMatrixTests();
    RotateTests();
    TransformTests();
    SlerpTests();
    SchemeTests();
    InterpolationTests();
    StreamTests();
    for (int i = 1; i < argc; i++) ModelTests(argv[i]);

    rw::Driver::s_plglist[rw::PLATFORM_NULL].destruct(rw::engine->driver[rw::PLATFORM_NULL]);
    rw::Engine::s_plglist.destruct(rw::engine);
    DestroyWindow(wnd);
    std::printf("\nrw_rtanim_rtquat_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}
