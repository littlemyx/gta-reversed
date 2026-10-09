// P2B-01 unit test: the RW C API matrix / vector / stream shim (source/standalone/rw/{math,stream}.cpp) on librw.
// Built as target `rw_math_stream_test` when GTASA_RW_LIBRW is ON (not part of `all`). Usage: rw_math_stream_test.exe [file.dff ...]
// Expected values for the matrix tests are derived by hand from the RW 3.6 conventions (row vectors, p' = p.x*right + p.y*up + p.z*at + pos)
// and from the exe disassembly noted in math.cpp.
#include <rwcore.h>
#include <cmath>
#include <cstdlib>
#include <float.h>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static bool Near(float a, float b, float e = 1e-5f) { return std::fabs(a - b) <= e; }
static bool NearV(const RwV3d& v, float x, float y, float z, float e = 1e-5f) { return Near(v.x, x, e) && Near(v.y, y, e) && Near(v.z, z, e); }
static void Section(const char* n) { std::printf("[%s]\n", n); }
static const unsigned kIdent = rwMATRIXINTERNALIDENTITY, kOrthoNormal = rwMATRIXTYPEORTHONORMAL;

static void TestMatrix() {
    Section("matrix");
    RwMatrix* m = RwMatrixCreate();
    CHECK(m && m->flags == (kIdent | kOrthoNormal));
    CHECK(NearV(m->right, 1, 0, 0) && NearV(m->up, 0, 1, 0) && NearV(m->at, 0, 0, 1) && NearV(m->pos, 0, 0, 0));

    // Translate: REPLACE gives type 3 without IDENTITY, POST adds to pos, PRE adds translation*3x3
    const RwV3d t123{1, 2, 3};
    CHECK(RwMatrixTranslate(m, &t123, rwCOMBINEREPLACE) == m);
    CHECK(NearV(m->pos, 1, 2, 3) && m->flags == kOrthoNormal);
    RwMatrix rot;
    rot.setIdentity();
    const RwV3d zAxis{0, 0, 1};
    RwMatrixRotate(&rot, &zAxis, 90.0f, rwCOMBINEREPLACE); // right=(0,1,0) up=(-1,0,0) at=(0,0,1)
    CHECK(NearV(rot.right, 0, 1, 0) && NearV(rot.up, -1, 0, 0) && NearV(rot.at, 0, 0, 1) && rot.flags == kOrthoNormal);

    // Multiply: out = A*B means "apply A, then B": origin -> A -> (1,2,3) -> rotate -> (-2,1,3)
    RwMatrix c;
    CHECK(RwMatrixMultiply(&c, m, &rot) == &c);
    CHECK(NearV(c.pos, -2, 1, 3) && NearV(c.right, 0, 1, 0) && NearV(c.up, -1, 0, 0) && c.flags == kOrthoNormal);
    // identity shortcuts copy the other operand (flags included)
    RwMatrix id; id.setIdentity();
    RwMatrixMultiply(&c, &id, &rot);
    CHECK(std::memcmp(&c, &rot, sizeof(RwMatrix)) == 0);
    RwMatrixMultiply(&c, &rot, &id);
    CHECK(std::memcmp(&c, &rot, sizeof(RwMatrix)) == 0);
    // aliasing: m = m*rot
    RwMatrix a = *m;
    RwMatrixMultiply(&a, &a, &rot);
    CHECK(NearV(a.pos, -2, 1, 3));
    // flags of a product = AND of the operand flags
    RwMatrix u = rot; RwMatrixUpdate(&u);
    CHECK(u.flags == 0);
    RwMatrixMultiply(&c, &u, &rot);
    CHECK(c.flags == 0);

    // Invert: orthonormal -> transpose path, flags 3
    RwMatrix inv;
    CHECK(RwMatrixInvert(&inv, m) == &inv);
    CHECK(NearV(inv.pos, -1, -2, -3) && inv.flags == kOrthoNormal); // pure translation
    RwMatrix full; RwMatrixMultiply(&full, &rot, m); // rotate then translate
    RwMatrix finv; RwMatrixInvert(&finv, &full);
    RwMatrix prod; RwMatrixMultiply(&prod, &full, &finv);
    CHECK(NearV(prod.right, 1, 0, 0) && NearV(prod.up, 0, 1, 0) && NearV(prod.at, 0, 0, 1) && NearV(prod.pos, 0, 0, 0, 1e-4f));
    // general matrix: scale (2,3,4) then translate; flags of the inverse are 0 (original: mov [dst+0xc], 0)
    RwMatrix g; g.setIdentity();
    const RwV3d s234{2, 3, 4};
    RwMatrixScale(&g, &s234, rwCOMBINEREPLACE);
    CHECK(g.flags == 0 && Near(g.right.x, 2) && Near(g.up.y, 3) && Near(g.at.z, 4));
    RwMatrixTranslate(&g, &t123, rwCOMBINEPOSTCONCAT);
    RwMatrix gi; gi.flags = kOrthoNormal; // stale flags in the output must not leak
    RwMatrixInvert(&gi, &g);
    CHECK(gi.flags == 0 && Near(gi.right.x, 0.5f) && Near(gi.up.y, 1.0f / 3) && Near(gi.at.z, 0.25f));
    CHECK(NearV(gi.pos, -0.5f, -2.0f / 3, -0.75f));
    RwMatrixInvert(&g, &g); // in place
    CHECK(Near(g.right.x, 0.5f) && NearV(g.pos, -0.5f, -2.0f / 3, -0.75f));
    // identity inverts to a copy
    RwMatrixInvert(&gi, &id);
    CHECK(gi.flags == (kIdent | kOrthoNormal));

    // Scale on an orthonormal matrix drops the type bits (librw alone would keep ORTHONORMAL)
    RwMatrix sc = rot;
    RwMatrixScale(&sc, &s234, rwCOMBINEPRECONCAT);
    CHECK(sc.flags == 0);
    // PRE: rows scaled (scale applied first): right = (0,1,0)*2 ; POST: columns scaled
    CHECK(NearV(sc.right, 0, 2, 0) && NearV(sc.up, -3, 0, 0) && NearV(sc.at, 0, 0, 4));

    // Translate PRE/POST on rot (pos = 0)
    RwMatrix tp = rot, tq = rot;
    RwMatrixTranslate(&tp, &t123, rwCOMBINEPOSTCONCAT);
    CHECK(NearV(tp.pos, 1, 2, 3) && tp.flags == kOrthoNormal);
    RwMatrixTranslate(&tq, &t123, rwCOMBINEPRECONCAT); // 1*right + 2*up + 3*at = (-2,1,3)
    CHECK(NearV(tq.pos, -2, 1, 3) && tq.flags == kOrthoNormal);
    RwMatrix ti; ti.setIdentity();
    RwMatrixTranslate(&ti, &t123, rwCOMBINEPOSTCONCAT);
    CHECK((ti.flags & kIdent) == 0 && (ti.flags & 3) == 3); // type bits of the identity are kept

    // Rotate PRE/POST + Transform
    RwMatrix r1 = *m, r2 = *m;
    RwMatrixRotate(&r1, &zAxis, 90.0f, rwCOMBINEPOSTCONCAT); // translate then rotate -> pos (-2,1,3)
    CHECK(NearV(r1.pos, -2, 1, 3));
    RwMatrixRotate(&r2, &zAxis, 90.0f, rwCOMBINEPRECONCAT);  // rotate then translate -> pos (1,2,3)
    CHECK(NearV(r2.pos, 1, 2, 3) && NearV(r2.right, 0, 1, 0));
    RwMatrix tr = *m;
    RwMatrixTransform(&tr, &rot, rwCOMBINEPOSTCONCAT);
    CHECK(NearV(tr.pos, -2, 1, 3));
    RwMatrixTransform(&tr, &rot, rwCOMBINEREPLACE);
    CHECK(std::memcmp(&tr, &rot, sizeof(RwMatrix)) == 0);
    RwMatrixUpdate(&tr);
    CHECK(tr.flags == 0);
    CHECK(RwMatrixDestroy(m));
}

static void TestVectors() {
    Section("vectors");
    const RwV2d v2{3, 4};
    // 01r: the exe's lengths come from the sqrt tables (relative error up to ~1e-3), 25 / 169 / 25 are exact table hits for the length
    CHECK(Near(RwV2dLength(&v2), 5, 5e-3f));
    const RwV3d v3{3, 4, 12};
    CHECK(RwV3dLength(&v3) == 13.0f);                       // bits(sqrt(169)) = 0x41500000 through the table (checked against Python in TestExeNumerics)
    RwV3d n;
    const RwV3d a{3, 4, 0};
    CHECK(Near(RwV3dNormalize(&n, &a), 5, 5e-3f) && NearV(n, 0.6f, 0.8f, 0, 2e-3f));
    const RwV3d z{0, 0, 0};
    const float lz = RwV3dNormalize(&n, &z);
    CHECK(lz == 0.0f && n.x == 0 && n.y == 0 && n.z == 0); // no NaN
    RwMatrix m; m.setIdentity();
    const RwV3d t{10, 20, 30}, zAxis{0, 0, 1};
    RwMatrixRotate(&m, &zAxis, 90.0f, rwCOMBINEREPLACE);
    RwMatrixTranslate(&m, &t, rwCOMBINEPOSTCONCAT);
    const RwV3d p{1, 2, 3};
    RwV3d out;
    CHECK(RwV3dTransformPoint(&out, &p, &m) == &out && NearV(out, 8, 21, 33));   // (-2,1,3)+(10,20,30)
    CHECK(RwV3dTransformVector(&out, &p, &m) == &out && NearV(out, -2, 1, 3));
    RwV3d pts[2] = { {1, 0, 0}, {0, 1, 0} }, res[2];
    CHECK(RwV3dTransformPoints(res, pts, 2, &m) == res && NearV(res[0], 10, 21, 30) && NearV(res[1], 9, 20, 30));
    CHECK(RwV3dTransformVectors(res, pts, 2, &m) == res && NearV(res[0], 0, 1, 0) && NearV(res[1], -1, 0, 0));
    RwV3d inplace[1] = { {1, 0, 0} };
    RwV3dTransformPoints(inplace, inplace, 1, &m); // out == in
    CHECK(NearV(inplace[0], 10, 21, 30));
}

static void PutChunk(RwStream* s, unsigned type, unsigned len, unsigned libId) {
    const unsigned h[3] = { type, len, libId };
    CHECK(RwStreamWrite(s, h, sizeof(h)) == s);
}

static void TestStreams(int argc, char** argv) {
    Section("stream: memory round trip");
    // write: framelist(0x0E) len 16 | struct(1) len 4 | 4 bytes payload | clump(0x10) | struct with a 3.2 library id   (0x1803FFFF == 3.6.0.3 build 0xFFFF)
    RwStream* w = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMWRITE, nullptr);
    CHECK(w != nullptr);
    PutChunk(w, 0x0E, 16, 0x1803FFFF);                   // frame list: struct header + payload
    PutChunk(w, 0x01, 4, 0x1803FFFF);
    const unsigned payload = 0xCAFEBABE;
    CHECK(RwStreamWrite(w, &payload, 4) == w);
    PutChunk(w, 0x10, 4, 0x1803FFFF);                    // clump
    PutChunk(w, 0x01, 0, 0x0800FFFF);                    // 3.2 header (old numbering has no build? 0x0800FFFF)
    RwMemory mem{};
    CHECK(RwStreamClose(w, &mem));
    CHECK(mem.start != nullptr && mem.length == 12 * 4 + 4);

    RwStream* r = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
    CHECK(r != nullptr);
    RwChunkHeaderInfo ci{};
    CHECK(RwStreamReadChunkHeaderInfo(r, &ci) == r);
    std::printf("  chunk0: type=0x%X length=%u version=0x%X build=0x%X isComplex=%d\n", ci.type, ci.length, ci.version, ci.buildNum, ci.isComplex);
    CHECK(ci.type == 0x0E && ci.length == 16 && ci.version == 0x36003 && ci.buildNum == 0xFFFF && ci.isComplex == 1);
    CHECK(RwStreamReadChunkHeaderInfo(r, &ci) == r);
    CHECK(ci.type == 1 && ci.length == 4 && ci.isComplex == 0);
    unsigned got = 0;
    CHECK(RwStreamRead(r, &got, 4) == 4 && got == payload);
    // FindChunk: skips the rest of what is not asked for
    unsigned len = 0, ver = 0;
    CHECK(RwStreamFindChunk(r, 0x10, &len, &ver) == TRUE && len == 4 && ver == 0x36003);
    // the next chunk has the 3.2 header (0x0800FFFF -> version 0x31000?): rejected by the version window
    CHECK(RwStreamFindChunk(r, 1, nullptr, nullptr) == FALSE);
    CHECK(RwStreamClose(r, &mem));

    r = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
    // NULL out pointers are fine
    CHECK(RwStreamFindChunk(r, 0x10, nullptr, nullptr) == TRUE);
    // short read / skip behaviour of a memory stream
    char buf[8];
    CHECK(RwStreamSkip(r, 0) == r);
    CHECK(RwStreamSkip(r, 4) != nullptr);                 // reaches the end of the 3.2 header? 12 bytes remain: pos 4 -> in range
    CHECK(RwStreamSkip(r, 1000) == nullptr);              // past the end: NULL, stops at the end
    CHECK(RwStreamRead(r, buf, 8) == 0);                  // at the end
    CHECK(RwStreamReadChunkHeaderInfo(r, &ci) == nullptr);
    CHECK(RwStreamClose(r, &mem));
    rwFree(mem.start);                                    // write-stream buffers belong to the caller

    Section("stream: 3.7 header is refused, 3.4 accepted");
    unsigned raw[6] = { 0x10, 0, 0x1C020037u, 0x10, 0, 0x1003FFFFu };
    RwMemory rm{ reinterpret_cast<RwUInt8*>(raw), sizeof(raw) };
    r = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &rm);
    CHECK(RwStreamFindChunk(r, 0x10, &len, &ver) == FALSE);   // 0x1C020037 -> version 0x37002 > 0x36003
    RwStreamClose(r, nullptr);
    rm.start += 12; rm.length -= 12;
    r = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &rm);
    CHECK(RwStreamFindChunk(r, 0x10, &len, &ver) == TRUE && ver == 0x34003);
    RwStreamClose(r, nullptr);

    Section("stream: file (write, append, read)");
    const char* tmp = "rw_math_stream_test.tmp";
    RwStream* f = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMWRITE, tmp);
    CHECK(f != nullptr);
    PutChunk(f, 0x16, 8, 0x1803FFFF);
    CHECK(RwStreamWrite(f, "ABCDEFGH", 8) == f);
    CHECK(RwStreamClose(f, nullptr));
    f = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, tmp);
    CHECK(f && RwStreamFindChunk(f, 0x16, &len, &ver) && len == 8);
    char tx[9] = {};
    CHECK(RwStreamRead(f, tx, 8) == 8 && std::strcmp(tx, "ABCDEFGH") == 0);
    CHECK(RwStreamRead(f, tx, 8) == 0);
    CHECK(RwStreamClose(f, nullptr));
    CHECK(RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, "no_such_file.dff") == nullptr);
    // wrap an open FILE
    RwFileFunctions* fi = RwOsGetFileInterface();
    CHECK(fi && fi->rwfexist(tmp) && !fi->rwfexist("no_such_file.dff"));
    void* fp = fi->rwfopen(tmp, "rb");
    f = RwStreamOpen(rwSTREAMFILE, rwSTREAMREAD, fp);
    CHECK(f && RwStreamSkip(f, 12) == f && RwStreamRead(f, tx, 4) == 4 && std::memcmp(tx, "ABCD", 4) == 0);
    CHECK(RwStreamClose(f, nullptr));
    std::remove(tmp);
    // _rwStreamInitialize ignores the caller storage but behaves like Open
    char storage[64];
    RwMemory im{ reinterpret_cast<RwUInt8*>(raw), 12 };
    RwStream* is = _rwStreamInitialize(reinterpret_cast<RwStream*>(storage), 0, rwSTREAMMEMORY, rwSTREAMREAD, &im);
    CHECK(is && RwStreamReadChunkHeaderInfo(is, &ci) == is && ci.type == 0x10 && ci.version == 0x37002);
    CHECK(RwStreamClose(is, &im));

    Section("stream: real SA DFF files");
    for (int i = 1; i < argc; ++i) {
        RwStream* s = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, argv[i]);
        if (!s) { std::printf("  cannot open %s\n", argv[i]); ++g_fail; continue; }
        unsigned l = 0, v = 0;
        const bool found = RwStreamFindChunk(s, 0x10 /*rwID_CLUMP*/, &l, &v);
        std::printf("  %s\n    FindChunk(CLUMP): %d length=%u version=0x%X\n", argv[i], found, l, v);
        CHECK(found && l > 0 && (v == 0x36003 || v == 0x35000 || v == 0x34003));
        RwChunkHeaderInfo h{};
        if (found && RwStreamReadChunkHeaderInfo(s, &h)) { // the clump's first child: STRUCT (numAtomics, numLights, numCameras)
            std::printf("    first child: type=0x%X length=%u version=0x%X build=0x%X complex=%d\n", h.type, h.length, h.version, h.buildNum, h.isComplex);
            CHECK(h.type == 1 && h.length >= 4);
        }
        CHECK(RwStreamFindChunk(s, 0x1234567, nullptr, nullptr) == FALSE); // runs through the rest of the file (skips by length) and fails
        RwStreamClose(s, nullptr);
        // cross-check against librw's own findChunk on the same bytes
        if (FILE* fh = std::fopen(argv[i], "rb")) {
            std::vector<uint8_t> bytes(1 << 20);
            bytes.resize(std::fread(bytes.data(), 1, bytes.size(), fh));
            std::fclose(fh);
            rw::StreamMemory sm;
            sm.open(bytes.data(), (uint32_t)bytes.size());
            uint32_t ll = 0, lv = 0;
            CHECK(rw::findChunk(&sm, 0x10, &ll, &lv) && ll == l && lv == v);
        }
    }
}

//--------------------------------------------------------------------------------------------------
// 01r: exe numerics. (1) values of the table sqrt computed by an independent transcription of the asm (Python, float32 rounding at every store),
// (2) structural checks of RwMatrixOrthoNormalize, (3) differential run against the REAL RW code of gta_sa_compact.exe: the exe's image is mapped at its
// own addresses (the test is linked at a different base), a fake RwEngineInstance is built and the exe's own functions are called through function
// pointers; results must be bit-identical to the shim (needs env RW_EXE_ORACLE=<path of gta_sa_compact.exe>, skipped otherwise).
//--------------------------------------------------------------------------------------------------
static bool SameBits(float a, float b) { return std::memcmp(&a, &b, 4) == 0; }
static bool SameMat(const RwMatrix& a, const RwMatrix& b, bool flags = true) {
    return std::memcmp(&a.right, &b.right, 12) == 0 && std::memcmp(&a.up, &b.up, 12) == 0 && std::memcmp(&a.at, &b.at, 12) == 0 &&
           std::memcmp(&a.pos, &b.pos, 12) == 0 && (!flags || a.flags == b.flags);
}

static void TestExeNumerics() {
    Section("exe numerics (01r): table sqrt");
    struct { float x; unsigned sq, isq; } kv[] = {
        {1.0f, 0x3f800000, 0x3f800000}, {2.0f, 0x3fb504f3, 0x3f3504f3}, {3.0f, 0x3fddb3d7, 0x3f13cd3a}, {4.0f, 0x40000000, 0x3f000000},
        {10.0f, 0x404a62c2, 0x3ea1e89b}, {169.0f, 0x41500000, 0x3d9d89d9}, {0.5f, 0x3f3504f3, 0x3fb504f3}, {0.001f, 0x3d0185af, 0x41fcfdcb},
        {12345.678f, 0x42de3505, 0x3c13774e},
    };
    for (const auto& k : kv) {
        unsigned a, b;
        const float s = _rwSqrt(k.x), i = _rwInvSqrt(k.x);
        std::memcpy(&a, &s, 4); std::memcpy(&b, &i, 4);
        CHECK(a == k.sq && b == k.isq);
    }
    CHECK(_rwSqrt(0.0f) == 0.0f && _rwInvSqrt(0.0f) == 0.0f);
    // structural: the approximation is NOT sqrtf (the exe's error ~7e-4 at 12345.678), and RwV3dNormalize's vector is not unit length exactly
    CHECK(_rwSqrt(12345.678f) != std::sqrt(12345.678f));

    Section("exe numerics (01r): RwMatrixOrthoNormalize keeps the most orthogonal pair");
    {
        RwMatrix m; m.setIdentity();
        m.right = {1, 0, 0}; m.up = {0, 1, 0}; m.at = {0.2f, 0, 1}; m.pos = {5, 6, 7}; m.flags = 0;
        RwMatrix o = m;
        CHECK(RwMatrixOrthoNormalize(&o, &m) == &o);
        CHECK(NearV(o.right, 1, 0, 0, 2e-3f) && NearV(o.up, 0, 1, 0, 2e-3f) && NearV(o.at, 0, 0, 1, 2e-3f)); // at rebuilt as right x up
        CHECK(NearV(o.pos, 5, 6, 7, 0) && o.flags == kOrthoNormal);
        RwMatrix k = m; k.right = {1, 0.1f, 0}; k.at = {0, 0, 1}; k.flags = kIdent;
        RwMatrixOrthoNormalize(&k, &k);                        // in place; identity flag cleared
        // |A.U| = |A.R| = 0 < |U.R|: right and at are kept, up is rebuilt as at x right = (-0.0995, 0.995, 0)
        CHECK(NearV(k.up, -0.0995f, 0.995f, 0, 2e-3f) && NearV(k.right, 0.995f, 0.0995f, 0, 2e-3f) && k.flags == kOrthoNormal);
        RwMatrix z = m; z.right = {0, 0, 0};                   // zero axis: rebuilt from the other two
        RwMatrixOrthoNormalize(&z, &z);
        CHECK(NearV(z.right, 0.9806f, 0, -0.1961f, 2e-3f));    // right = up x at (at = (0.2, 0, 1) normalised)
    }
}

#ifdef _WIN32
// ---- the real RW code of the exe -----------------------------------------------------------------------------------
namespace oracle {
using VecLen   = float(__cdecl*)(const RwV3d*);
using V3dNorm  = float(__cdecl*)(RwV3d*, const RwV3d*);
using V2dLen   = float(__cdecl*)(const RwV2d*);
using F1       = float(__cdecl*)(float);
using Xform    = RwV3d*(__cdecl*)(RwV3d*, const RwV3d*, const RwMatrix*);
using XformN   = RwV3d*(__cdecl*)(RwV3d*, const RwV3d*, int, const RwMatrix*);
using Mat3     = RwMatrix*(__cdecl*)(RwMatrix*, const RwMatrix*, const RwMatrix*);
using Mat2     = RwMatrix*(__cdecl*)(RwMatrix*, const RwMatrix*);
using MatRot   = RwMatrix*(__cdecl*)(RwMatrix*, const RwV3d*, float, int);
using MatV     = RwMatrix*(__cdecl*)(RwMatrix*, const RwV3d*, int);
using MatM     = RwMatrix*(__cdecl*)(RwMatrix*, const RwMatrix*, int);
using Init     = void*(__cdecl*)(void*, int, int);

static uint8_t g_engine[0x1000];
static void* __cdecl FakeAlloc(size_t n, unsigned) { return std::calloc(1, n); }
static void  __cdecl FakeFree(void* p) { std::free(p); }

#include "rw_exe_oracle_fixups.inc"   // offsets of the absolute addresses inside the two copied .text windows (tools/gen_oracle_fixups.py)
static uint8_t* g_data;     // copy of the exe's .rdata + .data (+ zeroed .bss) = VA 0x858000..0xCB0000
static uint8_t* g_code;   // copy of the exe .text span [kSpanVA, kSpanVA + kSpanSize)
static uint8_t* G(unsigned va) { return g_data + (va - 0x858000); }
static void* Fn(unsigned va) { return g_code + (va - kSpanVA); }

static bool Map(const char* path) {
    FILE* fh = std::fopen(path, "rb");
    if (!fh) { std::printf("  cannot open %s\n", path); return false; }
    std::vector<uint8_t> exe;
    exe.resize(0x600000);
    exe.resize(std::fread(exe.data(), 1, exe.size(), fh));
    std::fclose(fh);
    if (exe.size() < 0x4E1C00) return false;
    g_data = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0xCB0000 - 0x858000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    g_code = static_cast<uint8_t*>(VirtualAlloc(nullptr, kSpanSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!g_data || !g_code) return false;
    std::memcpy(g_data, exe.data() + 0x456800, 0x4B400);                          // .rdata  (VA 0x858000)
    std::memcpy(G(0x8A4000), exe.data() + 0x4A1C00, 0x40000);                     // .data   (VA 0x8A4000), the rest up to 0xCB0000 is .bss
    std::memcpy(g_code, exe.data() + (kSpanVA - 0x401000 + 0x400), kSpanSize);
    for (const auto& f : kFix) {
        uint32_t* p = reinterpret_cast<uint32_t*>(g_code + f[0]);
        *p += f[1] ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_code)) - kSpanVA : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_data)) - 0x858000u;
    }
    *reinterpret_cast<void**>(G(0xC97B24)) = g_engine;                                             // RwEngineInstance
    *reinterpret_cast<void**>(g_engine + 0x134) = reinterpret_cast<void*>(&FakeAlloc);
    *reinterpret_cast<void**>(g_engine + 0x138) = reinterpret_cast<void*>(&FakeFree);
    // the exe's own plugin constructor builds the sqrt tables (0x7EDE90 stores the plugin offset into 0xC97934)
    std::printf("  mapped: data %p code %p\n", g_data, g_code);
    reinterpret_cast<Init>(Fn(0x7EDE90))(nullptr, 0x400, 0);
    std::printf("  plugin constructor done\n");
    // matrix plugin data (RwMatrixOpen 0x7F16C0): identity mask, multiply kernel, tolerances
    *reinterpret_cast<int*>(G(0xC979BC)) = 0x600;
    *reinterpret_cast<uint32_t*>(g_engine + 0x604) = 0x20000;
    *reinterpret_cast<void**>(g_engine + 0x608) = Fn(0x7F12F0);
    return true;
}
} // namespace oracle

static uint32_t g_rng = 12345;
static float Rnd(float lo, float hi) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return lo + (hi - lo) * ((g_rng >> 8) * (1.0f / 16777216.0f));
}
static RwMatrix RndMat(bool ortho) {
    RwMatrix m; m.setIdentity();
    m.right = {Rnd(-3, 3), Rnd(-3, 3), Rnd(-3, 3)};
    m.up    = {Rnd(-3, 3), Rnd(-3, 3), Rnd(-3, 3)};
    m.at    = {Rnd(-3, 3), Rnd(-3, 3), Rnd(-3, 3)};
    m.pos   = {Rnd(-500, 500), Rnd(-500, 500), Rnd(-500, 500)};
    m.flags = 0;
    if (ortho) {
        RwMatrixOrthoNormalize(&m, &m);
    }
    return m;
}

static void TestAgainstExe(const char* exePath) {
    Section("exe oracle (01r): the exe's own RW code vs the shim, bit-exact");
    if (!oracle::Map(exePath)) {
        std::printf("  (skipped: cannot map %s at its addresses)\n", exePath);
        return;
    }
    std::printf("  x87 control word: 0x%04x\n", _controlfp(0, 0));
    // tables: the exe built them in its own plugin constructor; every table cell is hit by the grid below (all exponent parities, 4096 cells per decade)
    // compare outputs on a dense grid instead: every table cell is hit by x = bits(i << 12 ...) for both exponent parities
    int sqBad = 0, isqBad = 0, total = 0;
    for (uint32_t e = 100; e < 140; e++) {
        for (uint32_t m = 0; m < 0x800000; m += 0x1001) {
            const uint32_t bits = (e << 23) | m;
            float x; std::memcpy(&x, &bits, 4);
            ++total;
            if (!SameBits(_rwSqrt(x), reinterpret_cast<oracle::F1>(oracle::Fn(0x7EDB30))(x))) ++sqBad;
            if (!SameBits(_rwInvSqrt(x), reinterpret_cast<oracle::F1>(oracle::Fn(0x7EDB90))(x))) ++isqBad;
        }
    }
    std::printf("  sqrt / invsqrt mismatches: %d / %d of %d\n", sqBad, isqBad, total);
    CHECK(sqBad == 0 && isqBad == 0);

    int bad[16] = {};
    const char* names[] = {"V3dLength", "V3dNormalize", "V2dLength", "Point", "Vector", "Multiply", "Invert(ortho)", "Invert(gen)", "Rotate", "Scale", "Translate", "Transform", "OrthoNormalize", "Normalize.len", "", ""};
    for (int it = 0; it < 3000; it++) {
        const RwV3d v{Rnd(-50, 50), Rnd(-50, 50), Rnd(-50, 50)};
        const RwV2d v2{Rnd(-50, 50), Rnd(-50, 50)};
        if (!SameBits(RwV3dLength(&v), reinterpret_cast<oracle::VecLen>(oracle::Fn(0x7EDAC0))(&v))) bad[0]++;
        RwV3d a, b;
        const float la = RwV3dNormalize(&a, &v), lb = reinterpret_cast<oracle::V3dNorm>(oracle::Fn(0x7ED9B0))(&b, &v);
        if (std::memcmp(&a, &b, 12) != 0) bad[1]++;
        if (!SameBits(la, lb)) bad[13]++;
        if (!SameBits(RwV2dLength(&v2), reinterpret_cast<oracle::V2dLen>(oracle::Fn(0x7EDBF0))(&v2))) bad[2]++;

        const RwMatrix m = RndMat(it & 1), n = RndMat(it & 2);
        RwV3d pa, pb;
        RwV3dTransformPoint(&pa, &v, &m); reinterpret_cast<oracle::Xform>(oracle::Fn(0x7EDD60))(&pb, &v, &m);
        if (std::memcmp(&pa, &pb, 12) != 0) bad[3]++;
        RwV3dTransformVector(&pa, &v, &m); reinterpret_cast<oracle::Xform>(oracle::Fn(0x7EDDC0))(&pb, &v, &m);
        if (std::memcmp(&pa, &pb, 12) != 0) bad[4]++;

        RwMatrix x, y; x = y = RndMat(false);
        RwMatrixMultiply(&x, &m, &n); reinterpret_cast<oracle::Mat3>(oracle::Fn(0x7F18B0))(&y, &m, &n);
        if (!SameMat(x, y)) bad[5]++;
        RwMatrixInvert(&x, &m); reinterpret_cast<oracle::Mat2>(oracle::Fn(0x7F2070))(&y, &m);
        if (!SameMat(x, y)) bad[(it & 1) ? 6 : 7]++;
        const RwV3d axis{Rnd(-1, 1), Rnd(-1, 1), Rnd(-1, 1)};
        const float ang = Rnd(-360, 360);
        const int op = it % 3;
        x = y = m;
        RwMatrixRotate(&x, &axis, ang, static_cast<RwOpCombineType>(op)); reinterpret_cast<oracle::MatRot>(oracle::Fn(0x7F1FD0))(&y, &axis, ang, op);
        if (!SameMat(x, y)) bad[8]++;
        x = y = m;
        RwMatrixScale(&x, &axis, static_cast<RwOpCombineType>(op)); reinterpret_cast<oracle::MatV>(oracle::Fn(0x7F22C0))(&y, &axis, op);
        if (!SameMat(x, y)) bad[9]++;
        x = y = m;
        RwMatrixTranslate(&x, &axis, static_cast<RwOpCombineType>(op)); reinterpret_cast<oracle::MatV>(oracle::Fn(0x7F2450))(&y, &axis, op);
        if (!SameMat(x, y)) bad[10]++;
        x = y = m;
        RwMatrixTransform(&x, &n, static_cast<RwOpCombineType>(op)); reinterpret_cast<oracle::MatM>(oracle::Fn(0x7F25A0))(&y, &n, op);
        if (!SameMat(x, y)) bad[11]++;
        x = y = m;
        RwMatrixOrthoNormalize(&x, &m); reinterpret_cast<oracle::Mat2>(oracle::Fn(0x7F1920))(&y, &m);
        if (!SameMat(x, y)) bad[12]++;
    }
    for (int i = 0; i < 14; i++) {
        std::printf("  %-16s mismatches: %d / 3000\n", names[i], bad[i]);
        CHECK(bad[i] == 0);
    }
}
#endif

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    rw::Engine::init(); // memory functions (rwMalloc / RwFree)
    TestExeNumerics();
#ifdef _WIN32
    if (const char* exe = std::getenv("RW_EXE_ORACLE")) {
        TestAgainstExe(exe);
    }
#endif
    TestMatrix();
    TestVectors();
    TestStreams(argc, argv);
    std::printf("\nrw_math_stream_test: %d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
