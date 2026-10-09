// P2B-01 unit test: the RW C API matrix / vector / stream shim (source/standalone/rw/{math,stream}.cpp) on librw.
// Built as target `rw_math_stream_test` when GTASA_RW_LIBRW is ON (not part of `all`). Usage: rw_math_stream_test.exe [file.dff ...]
// Expected values for the matrix tests are derived by hand from the RW 3.6 conventions (row vectors, p' = p.x*right + p.y*up + p.z*at + pos)
// and from the exe disassembly noted in math.cpp.
#include <rwcore.h>
#include <cmath>
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
    CHECK(Near(RwV2dLength(&v2), 5));
    const RwV3d v3{3, 4, 12};
    CHECK(Near(RwV3dLength(&v3), 13));
    RwV3d n;
    const RwV3d a{3, 4, 0};
    CHECK(Near(RwV3dNormalize(&n, &a), 5) && NearV(n, 0.6f, 0.8f, 0));
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

int main(int argc, char** argv) {
    rw::Engine::init(); // memory functions (rwMalloc / RwFree)
    TestMatrix();
    TestVectors();
    TestStreams(argc, argv);
    std::printf("\nrw_math_stream_test: %d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
