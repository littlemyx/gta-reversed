#pragma once
/*
    01r: bit-exact ports of the exe's RenderWare 3.6 math kernels (gta_sa_compact.exe), header-only so that every translation unit of the shim (math.cpp,
    frame.cpp, the unit tests) can use them without extra link dependencies. Expressions follow the asm, not librw:
      * _rwSqrt 0x7EDB30 / _rwInvSqrt 0x7EDB90: lookup-table approximations (relative error ~1e-4), tables built by the engine init at 0x7EDE90.
        Entry index = (bits(x) + 0x800) >> 12 & 0xFFF (exponent LSB + 11 mantissa bits), result = table + exponent fix-up; x == +0 returns +0.
      * RwV3dLength 0x7EDAC0 / RwV3dNormalize 0x7ED9B0 / RwV2dLength 0x7EDBF0: sqrt / inverse sqrt of the float-rounded squared length via the tables.
      * RwV3dTransform{Point,Vector}(s) 0x7ED730 / 0x7ED880 (+ loops 0x7ED670 / 0x7ED7D0): x87 evaluation order with the float spill points of the asm.
      * Matrix multiply kernel 0x7F12F0 (sum order differs from librw's), RwMatrixInvert 0x7F2070 (+ general case 0x7F2160), RwMatrixRotate 0x7F1FD0 +
        RwMatrixRotateOneMinusCosineSine 0x7F1D00, RwMatrixOrthoNormalize 0x7F1930, Scale 0x7F22C0, Translate 0x7F2450, Transform 0x7F25A0, Multiply 0x7F18B0.
    Float discipline: x87 intermediates that the asm keeps on the FPU stack are `double` here (the project's rule: products of two floats are exact in
    double, only long sums can differ from the 64 bit mantissa by an ulp of the double); every `fstp dword` of the asm is a `(float)` cast here.
    Matrix flags use RW's values: IDENTITY 0x20000, TYPENORMAL 1, TYPEORTHOGONAL 2, TYPEORTHONORMAL 3, TYPEMASK 3 (identical in librw).
*/
#include <cstdint>
#include <cstring>
#include <cmath>

namespace rwx {

inline uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
inline float    FromBits(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
inline double   D(float f) { return static_cast<double>(f); }
inline float    F(double d) { return static_cast<float>(d); }

//--------------------------------------------------------------------------------------------------
// Sqrt / inverse sqrt tables (exe: allocated by the engine init 0x7EDE90, 2 x 4096 dwords)
//--------------------------------------------------------------------------------------------------
struct SqrtTables {
    uint32_t sq[4096];   // sqrt:      [2048 + i] = bits(sqrt(f)) - 0x1FC00000 for f = [1,2) step 2^12 ulp; [i] = bits(sqrt(f)) - 0x20000000 for f in [2,4)
    uint32_t isq[4096];  // 1/sqrt:    [2048 + i] = bits(1/sqrt(f)) - 0x20000000; [i] = bits(1/sqrt(f)) - 0x1FC00000
};

inline const SqrtTables& Tables() {
    static const SqrtTables t = [] {
        SqrtTables r{};
        uint32_t   fb = 0x3F800000u; // 1.0f; the exe walks the float bit pattern in steps of 0x1000 across [1, 4)
        for (int i = 0; i < 2048; i++, fb += 0x1000u) {
            const double f = D(FromBits(fb));
            r.sq[2048 + i]  = Bits(F(std::sqrt(f))) - 0x1FC00000u;
            r.isq[2048 + i] = Bits(F(1.0 / std::sqrt(f))) - 0x20000000u;
        }
        for (int i = 0; i < 2048; i++, fb += 0x1000u) {
            const double f = D(FromBits(fb));
            r.sq[i]  = Bits(F(std::sqrt(f))) - 0x20000000u;
            r.isq[i] = Bits(F(1.0 / std::sqrt(f))) - 0x1FC00000u;
        }
        return r;
    }();
    return t;
}

// 0x7EDB30
inline float Sqrt(float x) {
    const uint32_t b = Bits(x);
    if (b == 0) {
        return x;
    }
    const uint32_t v = b + 0x800u;
    return FromBits(Tables().sq[(v >> 12) & 0xFFFu] + ((v >> 1) & 0x3FC00000u));
}

// 0x7EDB90
inline float InvSqrt(float x) {
    const uint32_t b = Bits(x);
    if (b == 0) {
        return x;
    }
    const uint32_t v = b + 0x800u;
    return FromBits(Tables().isq[(v >> 12) & 0xFFFu] + ((~v >> 1) & 0x3FC00000u));
}

//--------------------------------------------------------------------------------------------------
// Vectors
//--------------------------------------------------------------------------------------------------
inline float LengthSq3(float x, float y, float z) { return F((D(x) * x + D(y) * y) + D(z) * z); }   // x, y, then z (0x7EDAC0)

inline float V3dLength(const RwV3d* in) { return Sqrt(LengthSq3(in->x, in->y, in->z)); }

inline float V2dLength(const RwV2d* in) { return Sqrt(F(D(in->x) * in->x + D(in->y) * in->y)); }

// 0x7ED9B0: returns the length; out = in * (approximate 1/length); zero vector -> zeros and 0 (the exe additionally raises an RW error)
inline float V3dNormalize(RwV3d* out, const RwV3d* in) {
    const float s   = LengthSq3(in->x, in->y, in->z);
    const float len = Sqrt(s);
    const float rcp = InvSqrt(s);
    const RwV3d i   = *in;
    out->x = F(D(i.x) * rcp);
    out->y = F(D(i.y) * rcp);
    out->z = F(D(i.z) * rcp);
    return len;
}

// 0x7ED730 (point) / 0x7ED880 (vector), looped by 0x7ED670 / 0x7ED7D0
inline void TransformPoint(RwV3d* out, const RwV3d* in, const RwMatrix* m) {
    const float x = in->x, y = in->y, z = in->z;
    const float xry = F(D(x) * m->right.y), xrz = F(D(x) * m->right.z);
    const double A  = D(y) * m->up.x + D(x) * m->right.x;
    const double B  = D(y) * m->up.y + xry;
    const double C  = D(y) * m->up.z + xrz;
    const float by  = F(D(z) * m->at.y + B);
    out->x = F((D(z) * m->at.x + A) + m->pos.x);
    out->y = F(D(m->pos.y) + by);
    out->z = F(D(m->pos.z) + (D(z) * m->at.z + C));
}
inline void TransformVector(RwV3d* out, const RwV3d* in, const RwMatrix* m) {
    const float x = in->x, y = in->y, z = in->z;
    const float xry = F(D(x) * m->right.y), xrz = F(D(x) * m->right.z);
    const double A  = D(y) * m->up.x + D(x) * m->right.x;
    const double B  = D(y) * m->up.y + xry;
    const float  Cf = F(D(y) * m->up.z + xrz);
    out->x = F(D(z) * m->at.x + A);
    out->y = F(D(z) * m->at.y + B);
    out->z = F(D(z) * m->at.z + Cf);
}

//--------------------------------------------------------------------------------------------------
// Matrices
//--------------------------------------------------------------------------------------------------
constexpr uint32_t kIdentity = 0x20000u;
constexpr uint32_t kTypeMask = 3u;

// 0x7F12F0: d = a * b (apply a, then b). Must not alias.
inline void MultKernel(RwMatrix* d, const RwMatrix* a, const RwMatrix* b) {
    d->right.x = F((D(a->right.x) * b->right.x + D(a->right.z) * b->at.x) + D(b->up.x) * a->right.y);
    d->right.y = F((D(b->up.y) * a->right.y + D(b->right.y) * a->right.x) + D(b->at.y) * a->right.z);
    d->right.z = F((D(b->up.z) * a->right.y + D(b->right.z) * a->right.x) + D(b->at.z) * a->right.z);
    d->up.x    = F((D(a->up.z) * b->at.x + D(a->up.x) * b->right.x) + D(a->up.y) * b->up.x);
    d->up.y    = F((D(a->up.x) * b->right.y + D(a->up.z) * b->at.y) + D(a->up.y) * b->up.y);
    d->up.z    = F((D(a->up.x) * b->right.z + D(a->up.z) * b->at.z) + D(a->up.y) * b->up.z);
    d->at.x    = F((D(a->at.y) * b->up.x + D(a->at.z) * b->at.x) + D(a->at.x) * b->right.x);
    d->at.y    = F((D(b->at.y) * a->at.z + D(b->up.y) * a->at.y) + D(a->at.x) * b->right.y);
    d->at.z    = F((D(b->at.z) * a->at.z + D(b->up.z) * a->at.y) + D(a->at.x) * b->right.z);
    d->pos.x   = F(((D(a->pos.z) * b->at.x + D(a->pos.x) * b->right.x) + D(a->pos.y) * b->up.x) + b->pos.x);
    d->pos.y   = F(((D(a->pos.x) * b->right.y + D(a->pos.z) * b->at.y) + D(a->pos.y) * b->up.y) + b->pos.y);
    d->pos.z   = F(((D(a->pos.x) * b->right.z + D(a->pos.z) * b->at.z) + D(a->pos.y) * b->up.z) + b->pos.z);
}

// 0x7F18B0 (identity shortcuts copy the other operand with its flags, else kernel and flags = a & b); safe when `out` aliases an input
inline RwMatrix* Multiply(RwMatrix* out, const RwMatrix* a, const RwMatrix* b) {
    if (a->flags & kIdentity) {
        const RwMatrix t = *b;
        *out = t;
    } else if (b->flags & kIdentity) {
        const RwMatrix t = *a;
        *out = t;
    } else {
        RwMatrix t;
        MultKernel(&t, a, b);
        t.flags = a->flags & b->flags;
        *out    = t;
    }
    return out;
}

// 0x7F2160 general inverse (flags = 0)
inline void InvertGeneral(RwMatrix* dst, const RwMatrix* s) {
    RwMatrix t = *dst; // keeps the pad dwords of dst like the exe (only xyz / pos / flags are written)
    t.right.x = F(D(s->at.z) * s->up.y - D(s->at.y) * s->up.z);
    t.right.y = F(-(D(s->at.z) * s->right.y - D(s->at.y) * s->right.z));
    const double Drz = D(s->up.z) * s->right.y - D(s->up.y) * s->right.z; // kept unrounded on the x87 stack
    t.right.z = F(Drz);
    const float detf = F(D(s->up.x) * t.right.y + D(s->at.x) * Drz + D(t.right.x) * s->right.x);
    const double inv = Bits(detf) == 0 ? 1.0 : 1.0 / D(detf);
    const double RXp = inv * D(t.right.x);
    t.right.x = F(RXp);
    t.right.y = F(inv * D(t.right.y));
    t.right.z = F(inv * D(t.right.z));
    t.up.x = F(-((D(s->at.z) * s->up.x - D(s->at.x) * s->up.z) * inv));
    t.up.y = F((D(s->at.z) * s->right.x - D(s->at.x) * s->right.z) * inv);
    t.up.z = F(-((D(s->up.z) * s->right.x - D(s->up.x) * s->right.z) * inv));
    t.at.x = F((D(s->at.y) * s->up.x - D(s->at.x) * s->up.y) * inv);
    t.at.y = F(-((D(s->at.y) * s->right.x - D(s->at.x) * s->right.y) * inv));
    t.at.z = F((D(s->up.y) * s->right.x - D(s->up.x) * s->right.y) * inv);
    t.pos.x = F(-((D(t.at.x) * s->pos.z + D(s->pos.y) * t.up.x) + D(s->pos.x) * RXp));
    t.pos.y = F(-((D(t.at.y) * s->pos.z + D(s->pos.y) * t.up.y) + D(s->pos.x) * t.right.y));
    t.pos.z = F(-((D(s->pos.y) * t.up.z + D(s->pos.x) * t.right.z) + D(t.at.z) * s->pos.z));
    t.flags = 0;
    *dst    = t;
}

// 0x7F2070
inline RwMatrix* Invert(RwMatrix* out, const RwMatrix* in) {
    if (in->flags & kIdentity) {
        const RwMatrix t = *in;
        *out = t;
    } else if ((in->flags & kTypeMask) == 3u) {
        RwMatrix t = *out;
        t.right.x = in->right.x; t.right.y = in->up.x;    t.right.z = in->at.x;
        t.up.x    = in->right.y; t.up.y    = in->up.y;    t.up.z    = in->at.y;
        t.at.x    = in->right.z; t.at.y    = in->up.z;    t.at.z    = in->at.z;
        t.pos.x = F(-((D(in->pos.y) * in->right.y + D(in->pos.x) * in->right.x) + D(in->right.z) * in->pos.z));
        t.pos.y = F(-((D(in->pos.x) * in->up.x + D(in->pos.y) * in->up.y) + D(in->pos.z) * in->up.z));
        t.pos.z = F(-((D(in->at.x) * in->pos.x + D(in->pos.y) * in->at.y) + D(in->at.z) * in->pos.z));
        t.flags = 3u;
        *out    = t;
    } else {
        InvertGeneral(out, in);
    }
    return out;
}

// 0x7F1D00: builds the rotation (type orthonormal) and combines it. combineOp: 0 replace, 1 pre-concat (rot * m), 2 post-concat (m * rot).
// Returns false for an invalid op (the exe raises an RW error and returns NULL).
inline bool RotateOneMinusCosineSine(RwMatrix* m, const RwV3d* axis, float omc, float sn, int combineOp) {
    const float x = axis->x, y = axis->y, z = axis->z;
    const double t = omc;
    const double X = 1.0 - D(x) * x;
    const double Y = 1.0 - D(y) * y;
    const float  Zf = F(1.0 - D(z) * z);
    const float  Xtf = F(X * t);
    const double Yt = Y * t;
    const double Zt = D(Zf) * t;
    const float  xyf = F(D(y) * x);
    const float  yztf = F((D(y) * z) * t);
    const double zxt = (D(z) * x) * t;
    const double xyt = D(xyf) * t;
    const float  sxf = F(D(sn) * x);
    const float  syf = F(D(y) * sn);
    const double sz  = D(z) * sn;
    RwMatrix r;
    std::memset(&r, 0, sizeof(r));
    r.right.x = F(1.0 - D(Xtf));
    r.right.y = F(sz + xyt);
    r.right.z = F(zxt - D(syf));
    r.up.x    = F(xyt - sz);
    r.up.y    = F(1.0 - Yt);
    r.up.z    = F(D(sxf) + D(yztf));
    r.at.x    = F(D(syf) + zxt);
    r.at.y    = F(D(yztf) - D(sxf));
    r.at.z    = F(1.0 - Zt);
    r.flags   = 3u;
    switch (combineOp) {
    case 0:
        *m = r;
        return true;
    case 1: case 2: {
        const uint32_t mf = m->flags;
        if (mf & kIdentity) {
            *m = r;
        } else {
            RwMatrix tmp;
            if (combineOp == 1) {
                MultKernel(&tmp, &r, m);
            } else {
                MultKernel(&tmp, m, &r);
            }
            tmp.flags = mf & 3u;
            *m        = tmp;
        }
        return true;
    }
    default:
        return false;
    }
}

// 0x7F1FD0: angle in degrees
inline RwMatrix* Rotate(RwMatrix* m, const RwV3d* axis, float angle, int combineOp) {
    const float ang = F(D(angle) * D(0.01745329238474369f));
    const float s2  = LengthSq3(axis->x, axis->y, axis->z);
    const float rcp = InvSqrt(s2);
    RwV3d an;
    an.x = F(D(rcp) * axis->x);
    an.y = F(D(rcp) * axis->y);
    an.z = F(D(rcp) * axis->z);
    const float sn  = F(std::sin(D(ang)));
    const float omc = F(1.0 - std::cos(D(ang)));
    RotateOneMinusCosineSine(m, &an, omc, sn, combineOp);
    return m;
}

// 0x7F22C0 (the exe dereferences NULL for an invalid op; nullptr is returned here)
inline RwMatrix* Scale(RwMatrix* m, const RwV3d* s, int combineOp) {
    switch (combineOp) {
    case 0: // replace: diagonal matrix, then flags &= ~(IDENTITY | TYPEMASK)
        m->right = {s->x, 0.0f, 0.0f};
        m->up    = {0.0f, s->y, 0.0f};
        m->at    = {0.0f, 0.0f, s->z};
        m->pos   = {0.0f, 0.0f, 0.0f};
        break;
    case 1: // pre-concat: rows scaled, pos untouched
        m->right.x = F(D(m->right.x) * s->x); m->right.y = F(D(m->right.y) * s->x); m->right.z = F(D(m->right.z) * s->x);
        m->up.x    = F(D(m->up.x) * s->y);    m->up.y    = F(D(m->up.y) * s->y);    m->up.z    = F(D(m->up.z) * s->y);
        m->at.x    = F(D(m->at.x) * s->z);    m->at.y    = F(D(m->at.y) * s->z);    m->at.z    = F(D(m->at.z) * s->z);
        break;
    case 2: // post-concat: columns scaled (pos included)
        m->right.x = F(D(m->right.x) * s->x); m->up.x = F(D(m->up.x) * s->x); m->at.x = F(D(m->at.x) * s->x); m->pos.x = F(D(m->pos.x) * s->x);
        m->right.y = F(D(m->right.y) * s->y); m->up.y = F(D(m->up.y) * s->y); m->at.y = F(D(m->at.y) * s->y); m->pos.y = F(D(m->pos.y) * s->y);
        m->right.z = F(D(m->right.z) * s->z); m->up.z = F(D(m->up.z) * s->z); m->at.z = F(D(m->at.z) * s->z); m->pos.z = F(D(m->pos.z) * s->z);
        break;
    default:
        return nullptr;
    }
    m->flags &= ~(kIdentity | kTypeMask);
    return m;
}

// 0x7F2450
inline RwMatrix* Translate(RwMatrix* m, const RwV3d* t, int combineOp) {
    switch (combineOp) {
    case 0: // replace: identity rotation, pos = t; flags |= type 3, identity cleared
        m->right = {1.0f, 0.0f, 0.0f};
        m->up    = {0.0f, 1.0f, 0.0f};
        m->at    = {0.0f, 0.0f, 1.0f};
        m->pos   = *t;
        m->flags = (m->flags | 0x20003u) & ~kIdentity;
        break;
    case 1: // pre-concat: pos += t * (3x3 part)
        m->pos.x = F(((D(m->at.x) * t->z + D(m->up.x) * t->y) + D(m->right.x) * t->x) + m->pos.x);
        m->pos.y = F(((D(m->at.y) * t->z + D(m->up.y) * t->y) + D(m->right.y) * t->x) + m->pos.y);
        m->pos.z = F(((D(m->at.z) * t->z + D(m->up.z) * t->y) + D(m->right.z) * t->x) + m->pos.z);
        m->flags &= ~kIdentity;
        break;
    case 2: // post-concat
        m->pos.x = F(D(m->pos.x) + t->x);
        m->pos.y = F(D(t->y) + m->pos.y);
        m->pos.z = F(D(t->z) + m->pos.z);
        m->flags &= ~kIdentity;
        break;
    default:
        return nullptr;
    }
    return m;
}

// 0x7F25A0
inline RwMatrix* Transform(RwMatrix* m, const RwMatrix* tr, int combineOp) {
    switch (combineOp) {
    case 0:
        *m = *tr;
        return m;
    case 1: // m = tr * m
        if (tr->flags & kIdentity) {
            return m;
        }
        if (m->flags & kIdentity) {
            *m = *tr;
            return m;
        } else {
            RwMatrix t;
            MultKernel(&t, tr, m);
            t.flags = m->flags & tr->flags;
            *m      = t;
            return m;
        }
    case 2: // m = m * tr
        if (m->flags & kIdentity) {
            *m = *tr;
            return m;
        }
        if (tr->flags & kIdentity) {
            return m;
        } else {
            RwMatrix t;
            MultKernel(&t, m, tr);
            t.flags = m->flags & tr->flags;
            *m      = t;
            return m;
        }
    default:
        return nullptr;
    }
}

// 0x7F1930 (dst may alias src). Normalises all three axes, then rebuilds the two axes that are the least orthogonal to the third.
inline RwMatrix* OrthoNormalize(RwMatrix* dst, const RwMatrix* src) {
    float R[3] = {src->right.x, src->right.y, src->right.z};
    float U[3] = {src->up.x, src->up.y, src->up.z};
    float A[3] = {src->at.x, src->at.y, src->at.z};
    const RwV3d pos = src->pos;

    // 1/length of every axis (squared length summed y, x, z as in the asm), then the axis scaled component by component
    const float rR = InvSqrt(F((D(R[1]) * R[1] + D(R[0]) * R[0]) + D(R[2]) * R[2]));
    R[0] = F(D(rR) * R[0]); R[1] = F(D(rR) * R[1]); R[2] = F(D(rR) * R[2]);
    const float rU = InvSqrt(F((D(U[1]) * U[1] + D(U[0]) * U[0]) + D(U[2]) * U[2]));
    U[0] = F(D(rU) * U[0]); U[1] = F(D(rU) * U[1]); U[2] = F(D(rU) * U[2]);
    const float rA = InvSqrt(F((D(A[1]) * A[1] + D(A[0]) * A[0]) + D(A[2]) * A[2]));
    A[0] = F(D(rA) * A[0]); A[1] = F(D(rA) * A[1]); A[2] = F(D(rA) * A[2]);

    float *b, *s, *d; // b stays, d = b x s, then s = d x b
    if (!(rR > 0.0f)) {
        b = U; s = A; d = R;
    } else if (!(rU > 0.0f)) {
        b = A; s = R; d = U;
    } else if (!(rA > 0.0f)) {
        b = R; s = U; d = A;
    } else {
        double dAU = F((D(A[1]) * U[1] + D(A[0]) * U[0]) + D(A[2]) * U[2]);
        if (dAU < 0.0) dAU = -dAU;
        double dAR = F((D(A[1]) * R[1] + D(A[0]) * R[0]) + D(A[2]) * R[2]);
        if (dAR < 0.0) dAR = -dAR;
        double dUR = (D(U[1]) * R[1] + D(U[0]) * R[0]) + D(U[2]) * R[2]; // stays unrounded on the x87 stack
        if (dUR < 0.0) dUR = -dUR;
        if (dAU < dAR) {
            if (dAU < dUR) { b = U; s = A; d = R; } else { b = R; s = U; d = A; }
        } else {
            if (dAR < dUR) { b = A; s = R; d = U; } else { b = R; s = U; d = A; }
        }
    }
    // d = b x s
    d[0] = F(D(s[2]) * b[1] - D(s[1]) * b[2]);
    d[1] = F(D(b[2]) * s[0] - D(s[2]) * b[0]);
    const double Dz = D(s[1]) * b[0] - D(b[1]) * s[0];
    d[2] = F(Dz);
    {
        const float r = InvSqrt(F((D(d[0]) * d[0] + D(d[1]) * d[1]) + Dz * Dz));
        d[0] = F(D(d[0]) * r);
        const double Dy2 = D(d[1]) * r;
        d[1] = F(Dy2);
        const double Dz2 = D(d[2]) * r;
        d[2] = F(Dz2);
        // s = d x b (the first component uses the unrounded scaled y / z of d)
        s[0] = F(D(b[2]) * Dy2 - D(b[1]) * Dz2);
    }
    s[1] = F(D(d[2]) * b[0] - D(b[2]) * d[0]);
    const double Sz = D(b[1]) * d[0] - D(d[1]) * b[0];
    s[2] = F(Sz);
    {
        const float r = InvSqrt(F((D(s[0]) * s[0] + D(s[1]) * s[1]) + Sz * Sz));
        s[0] = F(D(r) * s[0]);
        s[1] = F(D(s[1]) * r);
        s[2] = F(D(s[2]) * r);
    }
    dst->right = {R[0], R[1], R[2]};
    dst->up    = {U[0], U[1], U[2]};
    dst->at    = {A[0], A[1], A[2]};
    dst->pos   = pos;
    dst->flags = (dst->flags & ~kIdentity) | 3u;
    return dst;
}

//--------------------------------------------------------------------------------------------------
// Bounding spheres
//--------------------------------------------------------------------------------------------------
// 0x749330 (RpAtomicGetWorldBoundingSphere), the radius: unless the LTM is orthonormal (type bits == 3) the local radius is scaled by the table sqrt of the
// largest squared axis length. Axis squares are (x^2 + y^2) + z^2 (extended in the exe: `double`; the first axis stays unrounded on the x87 stack and is only
// compared with the float-rounded maximum of the other two, the others are spilled to float); the comparison is `s0 < max(s1, s2)` with NaN picking s0 / s1.
inline float WorldSphereRadius(const RwMatrix* ltm, float radius) {
    if ((ltm->flags & kTypeMask) == 3u) {
        return radius;
    }
    const double s0 = (D(ltm->right.x) * ltm->right.x + D(ltm->right.y) * ltm->right.y) + D(ltm->right.z) * ltm->right.z;
    const float  s1 = F((D(ltm->up.x) * ltm->up.x + D(ltm->up.y) * ltm->up.y) + D(ltm->up.z) * ltm->up.z);
    const float  s2 = F((D(ltm->at.x) * ltm->at.x + D(ltm->at.y) * ltm->at.y) + D(ltm->at.z) * ltm->at.z);
    const float  m12 = (s1 < s2) ? s2 : s1;
    const float  mx  = (s0 < D(m12)) ? m12 : F(s0);
    return F(D(Sqrt(mx)) * D(radius));
}

// 0x74C200 (RpMorphTargetCalcBoundingSphere): bounding box (0x808F60: maximum / minimum start at vertex 0, strict ordered compares) -> centre = (max + min) * 0.5
// (x, y: the extended sum is not rounded before the multiplication, z: spilled to float first), radius = table sqrt of the largest squared distance (extended
// subtraction, (dy^2 + dx^2) + dz^2, kept as float when it grows) * 1.001f; a squared radius <= 0 / NaN skips the sqrt.
inline void MorphTargetSphere(const RwV3d* v, int32_t n, RwV3d* centre, float* radius) {
    RwV3d hi{0, 0, 0}, lo{0, 0, 0};
    if (n > 0 && v) {
        hi = lo = v[0];
        for (int32_t i = 1; i < n; i++) {
            if (lo.x > v[i].x) lo.x = v[i].x;
            if (lo.y > v[i].y) lo.y = v[i].y;
            if (lo.z > v[i].z) lo.z = v[i].z;
            if (hi.x < v[i].x) hi.x = v[i].x;
            if (hi.y < v[i].y) hi.y = v[i].y;
            if (hi.z < v[i].z) hi.z = v[i].z;
        }
    }
    const RwV3d c{F((D(hi.x) + D(lo.x)) * 0.5), F((D(hi.y) + D(lo.y)) * 0.5), F(D(F(D(hi.z) + D(lo.z))) * 0.5)};
    float maxSq = 0.0f;
    for (int32_t i = 0; v && i < n; i++) {
        const double dx = D(v[i].x) - D(c.x), dy = D(v[i].y) - D(c.y), dz = D(v[i].z) - D(c.z);
        const double d  = (dy * dy + dx * dx) + dz * dz;
        if (d > D(maxSq)) {
            maxSq = F(d);
        }
    }
    if (maxSq > 0.0f) {
        maxSq = Sqrt(maxSq);
    }
    *centre = c;
    *radius = F(D(maxSq) * D(FromBits(0x3F8020C5u)));   // 1.001f (.rdata 0x872490)
}

} // namespace rwx
