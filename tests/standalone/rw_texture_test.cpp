// P2B-03b unit test: RwTexture* / RwTexDictionary* shim over librw/D3D9 (texture.cpp, texdict.cpp, with raster.cpp / image.cpp underneath).
//  1. Texture object: defaults, name/mask truncation, filter/address packing, ref counts, raster ownership, SetRaster, callbacks get/set
//  2. Dictionary: front insertion order, FindNamedTexture (ASCII case-insensitive, full string), ForAllTextures (early stop, destroy in callback),
//     GetFirstTexture, Add moves between dictionaries, Remove, Destroy with a still referenced texture, current dictionary, the game's rwLLLink walk
//  3. RwTextureRead through the find / read callbacks (hit = refCount++, miss = read callback, result goes to the FRONT of the current dictionary)
//  4. dictionary plugin registration (before the engine opens) + stream round trip through a memory stream
//  5. optional argv[1] = SA `models` directory: reads generic/vehicle.txd (DXT), particle.txd (roadsignfont is RGBA), hud.txd, txd/outro.txd (PAL8,
//     512x512, expanded to 32 bit): lists name/mask/format/levels/filter/address, compares level 0 with the file bytes, locks a level and checks pixels
//     against the palette, writes + re-reads the dictionary and compares every level
// Needs a D3D9 HAL adapter (works under Wine/wined3d). Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static RwGlobals s_RwGlobals{};
RwGlobals* RwEngineInstance = &s_RwGlobals;
bool RwInitialized = false;

static int g_fail = 0;
#define CHECK(c) do { const bool ok_ = !!(c); std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); if (!ok_) ++g_fail; } while (0)
#define CHECKV(c, fmt, ...) do { const bool ok_ = !!(c); std::printf("%-4s %s   " fmt "\n", ok_ ? "ok" : "FAIL", #c, __VA_ARGS__); if (!ok_) ++g_fail; } while (0)

static int g_pluginOffset = -1;
static const unsigned kPluginId = 0x0A0B0C0Du;
struct PluginData { int value; };
static int g_plugCtor = 0, g_plugDtor = 0;
static void* PlugCtor(void* obj, int off, int) { ((PluginData*)((char*)obj + off))->value = 0x1234; ++g_plugCtor; return obj; }
static void* PlugDtor(void* obj, int, int) { ++g_plugDtor; return obj; }
static void* PlugCopy(void* dst, const void*, int, int) { return dst; }
static RwStream* PlugRead(RwStream* s, int, void* obj, int off, int) { s->read8((char*)obj + off, 4); return s; }
static RwStream* PlugWrite(RwStream* s, int, const void* obj, int off, int) { s->write8((const char*)obj + off, 4); return s; }
static int PlugSize(const void*, int, int) { return 4; }

static RwRaster* NewRaster(int w, int h, int format = rwRASTERFORMAT8888, int extra = rwRASTERTYPETEXTURE) {
    return RwRasterCreate(w, h, 32, format | extra);
}
static RwTexture* NewTex(const char* name, int w = 8, int h = 8) {
    RwTexture* t = RwTextureCreate(NewRaster(w, h));
    if (t) RwTextureSetName(t, name);
    return t;
}
static std::string Names(RwTexDictionary* d) {
    std::string s;
    for (rw::LLLink* l = rwLinkListGetFirstLLLink(&d->textures); l != rwLinkListGetTerminator(&d->textures); l = rwLLLinkGetNext(l)) {
        RwTexture* t = rwLLLinkGetData(l, RwTexture, inDict);
        if (!s.empty()) s += ",";
        s += t->name;
    }
    return s;
}

//--------------------------------------------------------------------------------------------------
static int g_readCalls = 0, g_findCalls = 0;
static RwTexture* TestFind(const char* name) {
    ++g_findCalls;
    RwTexDictionary* cur = RwTexDictionaryGetCurrent();
    return cur ? RwTexDictionaryFindNamedTexture(cur, name) : nullptr;
}
static RwTexture* TestRead(const char* name, const char* mask) {
    ++g_readCalls;
    if (!std::strcmp(name, "missing")) return nullptr;
    RwTexture* t = NewTex(name, 4, 4);
    if (t && mask) RwTextureSetMaskName(t, mask);
    return t;
}
static RwTexture* CountCB(RwTexture*, void* d) { ++*(int*)d; return (RwTexture*)1; }
static RwTexture* StopAtSecond(RwTexture* t, void* d) { auto* v = (std::vector<std::string>*)d; v->push_back(t->name); return v->size() < 2 ? t : nullptr; }
static RwTexture* DestroyCB(RwTexture* t, void*) { RwTextureDestroy(t); return t; }

static void TextureObjectTests() {
    std::printf("-- texture object\n");
    const int texturesBefore = rw::Texture::numAllocated;
    const int d3dTexBefore = rw::d3d::d3d9Globals.numTextures;
    RwRaster* r = NewRaster(16, 8);
    RwTexture* t = RwTextureCreate(r);
    CHECK(t && t->raster == r && RwTextureGetRaster(t) == r);
    CHECK(t->refCount == 1 && t->dict == nullptr);
    CHECK(RwTextureGetFilterMode(t) == rwFILTERNEAREST);
    CHECK(RwTextureGetAddressingU(t) == rwTEXTUREADDRESSWRAP && RwTextureGetAddressingV(t) == rwTEXTUREADDRESSWRAP);
    CHECKV(t->filterAddressing == 0x1101u, "filterAddressing=%04x", t->filterAddressing);
    CHECK(RwTextureGetName(t)[0] == 0 && RwTextureGetMaskName(t)[0] == 0);
    RwTextureSetFilterMode(t, rwFILTERLINEARMIPLINEAR);
    RwTextureSetAddressingU(t, rwTEXTUREADDRESSCLAMP);
    RwTextureSetAddressingV(t, rwTEXTUREADDRESSMIRROR);
    CHECKV(t->filterAddressing == 0x2306u, "filterAddressing=%04x (V=mirror 2, U=clamp 3, filter 6)", t->filterAddressing);
    CHECK(RwTextureGetFilterMode(t) == rwFILTERLINEARMIPLINEAR && RwTextureGetAddressingU(t) == rwTEXTUREADDRESSCLAMP && RwTextureGetAddressingV(t) == rwTEXTUREADDRESSMIRROR);
    RwTextureSetAddressing(t, rwTEXTUREADDRESSBORDER);
    CHECK(RwTextureGetAddressingU(t) == rwTEXTUREADDRESSBORDER && RwTextureGetAddressingV(t) == rwTEXTUREADDRESSBORDER);
    CHECK(RwTextureSetName(t, "Hello") == t && !std::strcmp(t->name, "Hello"));
    RwTextureSetName(t, "0123456789012345678901234567890123456789");
    CHECK(std::strlen(t->name) == 31 && !std::strncmp(t->name, "0123456789012345678901234567890", 31) && t->name[31] == 0);
    RwTextureSetName(t, "ab");
    CHECK(t->name[2] == 0 && t->name[3] == 0 && t->name[31] == 0);   // fully overwritten (strncpy pads)
    CHECK(RwTextureSetMaskName(t, "mk") == t && !std::strcmp(t->mask, "mk"));
    CHECK(RwTextureAddRef(t) == t && t->refCount == 2);
    CHECK(RwTextureDestroy(t) == TRUE && t->refCount == 1);
    CHECK(rw::Texture::numAllocated == texturesBefore + 1);
    // SetRaster stores the pointer and does not free the old one
    RwRaster* r2 = NewRaster(4, 4);
    CHECK(RwTextureSetRaster(t, r2) == t && t->raster == r2);
    CHECK(RwTextureSetRaster(t, nullptr) == t && t->raster == nullptr);
    RwTextureSetRaster(t, r);
    RwRasterDestroy(r2);
    CHECK(RwTextureDestroy(t) == TRUE);
    CHECK(rw::Texture::numAllocated == texturesBefore);
    CHECKV(rw::d3d::d3d9Globals.numTextures == d3dTexBefore, "D3D textures %d -> %d (raster destroyed with the texture)", d3dTexBefore, rw::d3d::d3d9Globals.numTextures);

    // callbacks
    auto oldFind = RwTextureGetFindCallBack();
    auto oldRead = RwTextureGetReadCallBack();
    CHECK(RwTextureSetFindCallBack(TestFind) == TRUE && RwTextureGetFindCallBack() == TestFind);
    CHECK(RwTextureSetReadCallBack(TestRead) == TRUE && RwTextureGetReadCallBack() == TestRead);
    RwTextureSetFindCallBack(oldFind);
    RwTextureSetReadCallBack(oldRead);
    CHECK(RwTextureGetFindCallBack() == oldFind && RwTextureGetReadCallBack() == oldRead);
    CHECK(RwTextureSetMipmapping(TRUE) && RwTextureGetMipmapping() == TRUE && RwTextureSetMipmapping(FALSE) && RwTextureGetMipmapping() == FALSE);
    CHECK(RwTextureSetAutoMipmapping(TRUE) && RwTextureGetAutoMipmapping() == TRUE && RwTextureSetAutoMipmapping(FALSE) && RwTextureGetAutoMipmapping() == FALSE);
}

static void DictionaryTests() {
    std::printf("-- dictionary\n");
    const int dictsBefore = rw::TexDictionary::numAllocated;
    const int texBefore = rw::Texture::numAllocated;
    RwTexDictionary* d = RwTexDictionaryCreate();
    CHECK(d && d->object.type == rw::TexDictionary::ID);
    CHECK(rw::TexDictionary::numAllocated == dictsBefore + 1);
    CHECK(g_plugCtor >= 1 && ((PluginData*)((char*)d + g_pluginOffset))->value == 0x1234);   // plugin constructor ran
    CHECK(GetFirstTexture(d) == nullptr);
    RwTexture *a = NewTex("Alpha"), *b = NewTex("beta"), *c = NewTex("GAMMA");
    CHECK(RwTexDictionaryAddTexture(d, a) == a && a->dict == d);
    RwTexDictionaryAddTexture(d, b);
    RwTexDictionaryAddTexture(d, c);
    CHECKV(Names(d) == "GAMMA,beta,Alpha", "order=%s (newest first)", Names(d).c_str());
    CHECK(GetFirstTexture(d) == c);
    CHECK(d->count() == 3);
    CHECK(RwTexDictionaryFindNamedTexture(d, "alpha") == a && RwTexDictionaryFindNamedTexture(d, "ALPHA") == a && RwTexDictionaryFindNamedTexture(d, "Beta") == b);
    CHECK(RwTexDictionaryFindNamedTexture(d, "gamma") == c);
    CHECK(RwTexDictionaryFindNamedTexture(d, "alph") == nullptr && RwTexDictionaryFindNamedTexture(d, "alphaa") == nullptr && RwTexDictionaryFindNamedTexture(d, "") == nullptr);
    int n = 0;
    CHECK(RwTexDictionaryForAllTextures(d, CountCB, &n) == d && n == 3);
    std::vector<std::string> seen;
    CHECK(RwTexDictionaryForAllTextures(d, StopAtSecond, &seen) == d);
    CHECK(seen.size() == 2 && seen[0] == "GAMMA" && seen[1] == "beta");
    // texture list entry state seen by the game's hand written walk
    CHECK(rwLLLinkGetData(rwLinkListGetFirstLLLink(&d->textures), RwTexture, inDict) == c);
    CHECK(rwLLLinkGetNext(rwLinkListGetLastLLLink(&d->textures)) == rwLinkListGetTerminator(&d->textures));

    // Add into another dictionary moves it
    RwTexDictionary* d2 = RwTexDictionaryCreate();
    CHECK(RwTexDictionaryAddTexture(d2, b) == b && b->dict == d2);
    CHECKV(Names(d) == "GAMMA,Alpha" && Names(d2) == "beta", "d=%s d2=%s", Names(d).c_str(), Names(d2).c_str());
    // Remove
    CHECK(RwTexDictionaryRemoveTexture(b) == b && b->dict == nullptr && Names(d2).empty());
    CHECK(RwTexDictionaryRemoveTexture(b) == b);      // not in a dictionary: unchanged
    CHECK(b->refCount == 1);

    // current dictionary
    RwTexDictionary* oldCur = RwTexDictionaryGetCurrent();
    CHECK(RwTexDictionarySetCurrent(d) == d && RwTexDictionaryGetCurrent() == d);

    // Destroy with a texture that still has another reference: it survives, dict cleared
    RwTextureAddRef(a);
    const int texMid = rw::Texture::numAllocated;
    CHECK(RwTexDictionaryDestroy(d) == TRUE);
    CHECK(RwTexDictionaryGetCurrent() == nullptr);                     // destroying the current dictionary clears it
    CHECKV(rw::Texture::numAllocated == texMid - 1, "textures %d -> %d (c freed, a survives)", texMid, rw::Texture::numAllocated);
    CHECK(a->refCount == 1 && a->dict == nullptr);
    RwTextureDestroy(a);
    CHECK(RwTexDictionaryAddTexture(d2, b) == b);
    CHECK(RwTexDictionaryDestroy(d2) == TRUE);
    CHECK(rw::TexDictionary::numAllocated == dictsBefore && rw::Texture::numAllocated == texBefore);
    CHECK(g_plugDtor >= 2);
    RwTexDictionarySetCurrent(oldCur);

    // destroying textures from inside the walk
    RwTexDictionary* d3 = RwTexDictionaryCreate();
    for (int i = 0; i < 5; ++i) RwTexDictionaryAddTexture(d3, NewTex("t"));
    RwTexDictionaryForAllTextures(d3, DestroyCB, nullptr);
    CHECK(d3->count() == 0 && rw::Texture::numAllocated == texBefore);
    RwTexDictionaryDestroy(d3);
}

static void ReadTests() {
    std::printf("-- RwTextureRead\n");
    auto oldFind = RwTextureGetFindCallBack();
    auto oldRead = RwTextureGetReadCallBack();
    RwTextureSetFindCallBack(TestFind);
    RwTextureSetReadCallBack(TestRead);
    RwTexDictionary* oldCur = RwTexDictionaryGetCurrent();
    RwTexDictionary* d = RwTexDictionaryCreate();
    RwTexDictionarySetCurrent(d);
    g_readCalls = g_findCalls = 0;
    RwTexture* t1 = RwTextureRead("first", "firstm");
    CHECK(t1 && g_findCalls == 1 && g_readCalls == 1 && t1->dict == d && !std::strcmp(t1->mask, "firstm") && t1->refCount == 1);
    RwTexture* t1b = RwTextureRead("FIRST", nullptr);
    CHECK(t1b == t1 && g_readCalls == 1 && t1->refCount == 2);                 // found: refCount++, no read callback
    RwTexture* t2 = RwTextureRead("second", nullptr);
    CHECK(t2 && Names(d) == "second,first");                                    // new texture goes to the FRONT
    CHECK(RwTextureRead("missing", nullptr) == nullptr && g_readCalls == 3 && d->count() == 2);
    // without a current dictionary the new texture is not added anywhere
    RwTexDictionarySetCurrent(nullptr);
    RwTexture* t3 = RwTextureRead("third", nullptr);
    CHECK(t3 && t3->dict == nullptr);
    RwTextureDestroy(t3);
    RwTexDictionarySetCurrent(oldCur);
    RwTextureDestroy(t1b);
    RwTexDictionaryDestroy(d);
    RwTextureSetFindCallBack(oldFind);
    RwTextureSetReadCallBack(oldRead);
}

static void StreamTests() {
    std::printf("-- stream round trip (synthetic)\n");
    RwTexDictionary* d = RwTexDictionaryCreate();
    RwTexture* a = NewTex("tex_a", 16, 16);
    RwTexture* b = NewTex("tex_b", 8, 4);
    RwTextureSetMaskName(b, "tex_b_m");
    RwTextureSetFilterMode(a, rwFILTERLINEAR);
    RwTextureSetAddressingU(a, rwTEXTUREADDRESSCLAMP);
    // fill a with a pattern
    unsigned char* p = RwRasterLock(a->raster, 0, rwRASTERLOCKWRITE);
    CHECK(p != nullptr);
    if (p) {
        const int stride = RwRasterGetStride(a->raster);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                unsigned char* px = p + y * stride + x * 4;
                px[0] = (unsigned char)(x * 9); px[1] = (unsigned char)(y * 7); px[2] = (unsigned char)(x ^ y); px[3] = (unsigned char)(200 + x);
            }
        RwRasterUnlock(a->raster);
    }
    RwTexDictionaryAddTexture(d, a);
    RwTexDictionaryAddTexture(d, b);
    ((PluginData*)((char*)d + g_pluginOffset))->value = 0xBEEF;
    a->filterAddressing |= 0x30000u;   // bits above the 16 filter / addressing bits are not read back (exe native reader 0x4CD820 merges 16 bits only)
    const unsigned size = RwTexDictionaryStreamGetSize(d);
    std::vector<unsigned char> buf(size + 4096);
    rw::StreamMemory ms;
    ms.open(buf.data(), 0, (unsigned)buf.size());
    CHECK(RwTexDictionaryStreamWrite(d, &ms) == d);
    CHECKV(ms.length == size + 12, "written %u bytes, GetSize %u + chunk header 12", ms.length, size);
    ms.position = 0;
    ms.length = ms.length;
    RwTexDictionary* d2 = nullptr;
    if (rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr)) {
        d2 = RwTexDictionaryStreamRead(&ms);
    }
    CHECK(d2 != nullptr);
    if (d2) {
        // read order: textures come back in reverse (front insertion) = the order they were written reversed
        CHECKV(Names(d2) == "tex_a,tex_b", "names=%s", Names(d2).c_str());
        RwTexture* a2 = RwTexDictionaryFindNamedTexture(d2, "tex_a");
        RwTexture* b2 = RwTexDictionaryFindNamedTexture(d2, "tex_b");
        CHECK(a2 && b2 && !std::strcmp(b2->mask, "tex_b_m"));
        CHECK(a2 && RwTextureGetFilterMode(a2) == rwFILTERLINEAR && RwTextureGetAddressingU(a2) == rwTEXTUREADDRESSCLAMP);
        CHECKV(a2 && (a2->filterAddressing & ~0xFFFFu) == 0, "filterAddressing=%08x: upper 16 bits dropped on read", a2 ? a2->filterAddressing : 0u);
        CHECK(a2 && RwRasterGetWidth(a2->raster) == 16 && RwRasterGetHeight(a2->raster) == 16 && RwRasterGetFormat(a2->raster) == (rwRASTERFORMAT8888 | 0));
        CHECK(((PluginData*)((char*)d2 + g_pluginOffset))->value == 0xBEEF);   // dictionary plugin data survived the stream
        if (a2) {
            unsigned char* q = RwRasterLock(a2->raster, 0, rwRASTERLOCKREAD);
            CHECK(q != nullptr);
            if (q) {
                const int stride = RwRasterGetStride(a2->raster);
                bool same = true;
                for (int y = 0; y < 16 && same; ++y)
                    for (int x = 0; x < 16; ++x) {
                        unsigned char* px = q + y * stride + x * 4;
                        if (px[0] != (unsigned char)(x * 9) || px[1] != (unsigned char)(y * 7) || px[2] != (unsigned char)(x ^ y) || px[3] != (unsigned char)(200 + x)) { same = false; break; }
                    }
                CHECK(same);
                RwRasterUnlock(a2->raster);
            }
        }
        RwTexDictionaryDestroy(d2);
    }
    RwTexDictionaryDestroy(d);
    // a truncated stream fails without leaking
    const int dicts = rw::TexDictionary::numAllocated, texs = rw::Texture::numAllocated;
    ms.open(buf.data(), 60, 60);
    ms.position = 0;
    RwTexDictionary* bad = nullptr;
    if (rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr)) bad = RwTexDictionaryStreamRead(&ms);
    CHECKV(bad == nullptr && rw::TexDictionary::numAllocated == dicts && rw::Texture::numAllocated == texs, "dicts %d textures %d", rw::TexDictionary::numAllocated, rw::Texture::numAllocated);
}


static bool SameDicts(RwTexDictionary* x, RwTexDictionary* y, bool ignoreFilter = false);
extern bool rwshim_AnisotropySupportedByGFX;
extern int  rwshim_FxQuality;

// 03c: SA's RW patches RwTexDictionaryGtaStreamRead / Read1 + Read2
static void GtaStreamTests() {
    std::printf("-- GtaStreamRead (synthetic)\n");
    RwTexDictionary* d = RwTexDictionaryCreate();
    const char* names[5] = {"t0", "t1", "t2", "t3", "t4"};
    const int filters[5] = {rwFILTERNEAREST, rwFILTERMIPNEAREST, rwFILTERLINEAR, rwFILTERMIPLINEAR, rwFILTERLINEARMIPLINEAR};
    for (int i = 0; i < 5; ++i) {
        RwTexture* t = NewTex(names[i], 4 << (i % 3), 4);
        RwTextureSetFilterMode(t, (RwTextureFilterMode)filters[i]);
        RwTexDictionaryAddTexture(d, t);   // list order in memory: t4,t3,t2,t1,t0 ; written in that order
    }
    const unsigned size = RwTexDictionaryStreamGetSize(d);
    std::vector<unsigned char> buf(size + 64);
    rw::StreamMemory ms;
    ms.open(buf.data(), 0, (unsigned)buf.size());
    RwTexDictionaryStreamWrite(d, &ms);
    const unsigned total = ms.length;
    RwTexDictionary* ref = nullptr;
    ms.open(buf.data(), total, total);
    if (rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr)) ref = RwTexDictionaryStreamRead(&ms);
    CHECK(ref && ref->count() == 5);

    // whole read
    ms.open(buf.data(), total, total);
    RwTexDictionary* g = nullptr;
    CHECK(rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr));
    g = RwTexDictionaryGtaStreamRead(&ms);
    CHECK(g && g->count() == 5);
    if (g) {
        CHECKV(Names(g) == Names(ref), "order g=%s ref=%s", Names(g).c_str(), Names(ref).c_str());
        CHECK(RwTextureGetFilterMode(RwTexDictionaryFindNamedTexture(g, "t4")) == rwFILTERLINEARMIPLINEAR);
    }
    if (g) {
        // t0 NEAREST->LINEAR, t1 MIPNEAREST->MIPLINEAR, others unchanged
        CHECK(RwTextureGetFilterMode(RwTexDictionaryFindNamedTexture(g, "t0")) == rwFILTERLINEAR);
        CHECK(RwTextureGetFilterMode(RwTexDictionaryFindNamedTexture(g, "t1")) == rwFILTERMIPLINEAR);
        CHECK(RwTextureGetFilterMode(RwTexDictionaryFindNamedTexture(g, "t2")) == rwFILTERLINEAR);
        CHECK(RwTextureGetFilterMode(RwTexDictionaryFindNamedTexture(g, "t3")) == rwFILTERMIPLINEAR);
        CHECK(SameDicts(g, ref, true));
    }
    // split read: Read1 reads n - n/2 = 3 textures, Read2 (a NEW stream over the same memory) the other 2
    ms.open(buf.data(), total, total);
    CHECK(rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr));
    RwTexDictionary* s = RwTexDictionaryGtaStreamRead1(&ms);
    CHECK(s && s->count() == 3);
    const unsigned posAfter1 = ms.position;
    rw::StreamMemory ms2;
    ms2.open(buf.data(), total, total);
    CHECK(rw::findChunk(&ms2, rw::ID_TEXDICTIONARY, nullptr, nullptr));   // a fresh stream: Read2 skips to the remembered position
    RwTexDictionary* s2 = RwTexDictionaryGtaStreamRead2(&ms2, s);
    CHECK(s2 == s && s->count() == 5);
    CHECKV(ms2.position > posAfter1 && ms2.position <= total, "stream pos after Read1 %u, after Read2 %u (of %u)", posAfter1, ms2.position, total);
    if (g && s) {
        CHECKV(Names(s) == Names(g), "split order %s", Names(s).c_str());
        CHECK(SameDicts(g, s));
    }
    if (s) RwTexDictionaryDestroy(s);
    // n = 1: Read1 reads the single texture (n - n/2 = 1), Read2 nothing
    RwTexDictionary* one = RwTexDictionaryCreate();
    RwTexDictionaryAddTexture(one, NewTex("only"));
    std::vector<unsigned char> b1(RwTexDictionaryStreamGetSize(one) + 64);
    rw::StreamMemory m1;
    m1.open(b1.data(), 0, (unsigned)b1.size());
    RwTexDictionaryStreamWrite(one, &m1);
    const unsigned l1 = m1.length;
    m1.open(b1.data(), l1, l1);
    rw::findChunk(&m1, rw::ID_TEXDICTIONARY, nullptr, nullptr);
    RwTexDictionary* o1 = RwTexDictionaryGtaStreamRead1(&m1);
    CHECK(o1 && o1->count() == 1);
    rw::StreamMemory m1b;
    m1b.open(b1.data(), l1, l1);
    rw::findChunk(&m1b, rw::ID_TEXDICTIONARY, nullptr, nullptr);
    CHECK(RwTexDictionaryGtaStreamRead2(&m1b, o1) == o1 && o1->count() == 1);
    if (o1) RwTexDictionaryDestroy(o1);
    RwTexDictionaryDestroy(one);

    // truncated inside the first texture -> NULL, nothing leaked
    const int dicts = rw::TexDictionary::numAllocated, texs = rw::Texture::numAllocated;
    ms.open(buf.data(), 100, 100);
    rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr);
    CHECK(RwTexDictionaryGtaStreamRead(&ms) == nullptr);
    ms.open(buf.data(), 100, 100);
    rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr);
    RwTexDictionary* half = RwTexDictionaryGtaStreamRead1(&ms);
    CHECKV(half == nullptr && rw::TexDictionary::numAllocated == dicts && rw::Texture::numAllocated == texs, "dicts %d tex %d", rw::TexDictionary::numAllocated, rw::Texture::numAllocated);

    // anisotropy patch: only with GPU support, plugin value >= 1, FX quality >= HIGH
    rwshim_AnisotropySupportedByGFX = true;
    rwshim_FxQuality = 1;
    ms.open(buf.data(), total, total);
    rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr);
    RwTexDictionary* a1 = RwTexDictionaryGtaStreamRead(&ms);
    const int caps = rw::getMaxSupportedMaxAnisotropy();
    CHECKV(a1 && GetFirstTexture(a1)->getMaxAnisotropy() == 1, "FX quality 1: aniso=%d (device max %d)", a1 ? GetFirstTexture(a1)->getMaxAnisotropy() : -1, caps);
    rwshim_FxQuality = 2;
    ms.open(buf.data(), total, total);
    rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr);
    RwTexDictionary* a2 = RwTexDictionaryGtaStreamRead(&ms);
    CHECKV(a2 && GetFirstTexture(a2)->getMaxAnisotropy() == caps, "FX quality 2: aniso=%d (device max %d)", a2 ? GetFirstTexture(a2)->getMaxAnisotropy() : -1, caps);
    rwshim_AnisotropySupportedByGFX = false;
    if (a1) RwTexDictionaryDestroy(a1);
    if (a2) RwTexDictionaryDestroy(a2);
    if (g) RwTexDictionaryDestroy(g);
    if (ref) RwTexDictionaryDestroy(ref);
    RwTexDictionaryDestroy(d);
}

//--------------------------------------------------------------------------------------------------
// real files
//--------------------------------------------------------------------------------------------------
static std::vector<unsigned char> ReadAll(const std::string& path) {
    std::vector<unsigned char> v;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return v;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    v.resize(n);
    if (n) std::fread(v.data(), 1, n, f);
    std::fclose(f);
    return v;
}
static unsigned U32(const unsigned char* p) { unsigned v; std::memcpy(&v, p, 4); return v; }

struct RawTex {
    std::string name;
    unsigned format, d3d; int w, h, depth, levels, type, flags;
    unsigned palOff, lvl0SizeOff;
};
static std::vector<RawTex> ParseRaw(const std::vector<unsigned char>& d) {
    std::vector<RawTex> out;
    if (d.size() < 28 || U32(d.data()) != 0x16) return out;
    unsigned off = 12;
    const unsigned structLen = U32(&d[off + 4]);
    const int n = d[off + 12] | (d[off + 13] << 8);
    off += 12 + structLen;
    for (int i = 0; i < n; ++i) {
        if (off + 12 > d.size() || U32(&d[off]) != 0x15) break;
        const unsigned len = U32(&d[off + 4]);
        const unsigned s = off + 24;
        RawTex t;
        t.name = (const char*)&d[s + 8];
        t.format = U32(&d[s + 72]); t.d3d = U32(&d[s + 76]);
        t.w = d[s + 80] | (d[s + 81] << 8); t.h = d[s + 82] | (d[s + 83] << 8);
        t.depth = d[s + 84]; t.levels = d[s + 85]; t.type = d[s + 86]; t.flags = d[s + 87];
        t.palOff = s + 88;
        const unsigned pal = (t.format & 0x2000) ? 1024 : (t.format & 0x4000) ? 128 : 0;
        t.lvl0SizeOff = s + 88 + pal;
        out.push_back(t);
        off += 12 + len;
    }
    return out;
}
static std::string Fourcc(unsigned v) {
    if (v < 0x1000) { char b[16]; std::snprintf(b, sizeof b, "%u", v); return b; }
    return std::string((const char*)&v, 4);
}

static RwTexDictionary* LoadTxd(const std::string& path, std::vector<unsigned char>& raw) {
    raw = ReadAll(path);
    if (raw.empty()) { std::printf("cannot read %s\n", path.c_str()); return nullptr; }
    rw::StreamFile sf;
    if (!sf.open(path.c_str(), "rb")) return nullptr;
    RwTexDictionary* d = nullptr;
    if (rw::findChunk(&sf, rw::ID_TEXDICTIONARY, nullptr, nullptr)) d = RwTexDictionaryStreamRead(&sf);
    sf.close();
    return d;
}

// Compare every mip level of every texture of two dictionaries (same names).
static bool SameDicts(RwTexDictionary* x, RwTexDictionary* y, bool ignoreFilter) {
    for (rw::LLLink* l = x->textures.link.next; l != &x->textures.link; l = l->next) {
        RwTexture* tx = rw::Texture::fromDict(l);
        RwTexture* ty = RwTexDictionaryFindNamedTexture(y, tx->name);
        if (!ty || (ignoreFilter ? (tx->filterAddressing & ~0xFFu) != (ty->filterAddressing & ~0xFFu) : tx->filterAddressing != ty->filterAddressing) || std::strcmp(tx->mask, ty->mask)) return false;
        RwRaster *rx = tx->raster, *ry = ty->raster;
        if (rx->width != ry->width || rx->height != ry->height || rx->format != ry->format || rx->getNumLevels() != ry->getNumLevels()) return false;
        for (int lv = 0; lv < rx->getNumLevels(); ++lv) {
            unsigned char* px = RwRasterLock(rx, lv, rwRASTERLOCKREAD);
            const int wx = rx->width, hx = rx->height, sx = rx->stride;
            unsigned char* py = RwRasterLock(ry, lv, rwRASTERLOCKREAD);
            const int sy = ry->stride;
            if (!px || !py || wx != ry->width || hx != ry->height || sx != sy) { if (px) RwRasterUnlock(rx); if (py) RwRasterUnlock(ry); return false; }
            const bool dxt = GETD3DRASTEREXT(rx)->customFormat != 0;
            const int rows = dxt ? (hx + 3) / 4 : hx;
            const bool eq = std::memcmp(px, py, (size_t)rows * sx) == 0;
            RwRasterUnlock(rx);
            RwRasterUnlock(ry);
            if (!eq) return false;
        }
    }
    return true;
}

static void RealFile(const std::string& models, const char* rel, bool expectPal8) {
    std::printf("-- %s\n", rel);
    std::vector<unsigned char> raw;
    const std::string path = models + "/" + rel;
    RwTexDictionary* d = LoadTxd(path, raw);
    CHECK(d != nullptr);
    if (!d) return;
    const std::vector<RawTex> rt = ParseRaw(raw);
    CHECKV((int)rt.size() == d->count() && !rt.empty(), "textures: file %d, loaded %d", (int)rt.size(), d->count());
    int shown = 0, pal8 = 0, dxt = 0, mips = 0;
    bool allMatch = true;
    for (const RawTex& r : rt) {
        RwTexture* t = RwTexDictionaryFindNamedTexture(d, r.name.c_str());
        if (!t) { allMatch = false; std::printf("   missing %s\n", r.name.c_str()); continue; }
        RwRaster* ras = t->raster;
        rw::d3d::D3dRaster* ext = GETD3DRASTEREXT(ras);
        const bool isPal = (r.format & 0x2000) != 0;
        const bool isDxt = (r.flags & 8) != 0;
        pal8 += isPal; dxt += isDxt; mips += r.levels > 1;
        // level count / size / format agree with the file
        bool ok = ras->width == r.w && ras->height == r.h && ras->type == r.type;
        if (isPal) ok = ok && !(ras->format & (rwRASTERFORMATPAL8 | rwRASTERFORMATPAL4)) && ras->depth == 32;
        else ok = ok && (ras->format & 0xFF00) == (int)(r.format & 0xFF00);
        // level 0 against the file
        const unsigned sz0 = U32(&raw[r.lvl0SizeOff]);
        unsigned char* px = RwRasterLock(ras, 0, rwRASTERLOCKREAD);
        if (!px) ok = false;
        else {
            const int stride = RwRasterGetStride(ras);
            const unsigned char* src = &raw[r.lvl0SizeOff + 4];
            if (isPal) {
                const unsigned char* pal = &raw[r.palOff];
                for (int y = 0; y < r.h && ok; y += (r.h > 16 ? r.h / 16 : 1))
                    for (int x = 0; x < r.w; x += (r.w > 16 ? r.w / 16 : 1)) {
                        const unsigned char* c = pal + src[y * r.w + x] * 4;
                        const unsigned char* o = px + y * stride + x * 4;
                        if (o[0] != c[2] || o[1] != c[1] || o[2] != c[0]) { ok = false; break; }
                    }
            } else {
                const int rows = isDxt ? (r.h + 3) / 4 : r.h;
                const unsigned rowBytes = sz0 / rows;
                for (int y = 0; y < rows && ok; ++y)
                    if (std::memcmp(px + y * stride, src + y * rowBytes, rowBytes < (unsigned)stride ? rowBytes : stride)) ok = false;
            }
            RwRasterUnlock(ras);
        }
        // every further level against the file (levels follow each other: u32 size + data)
        if (!isPal && r.levels > 1 && ras->getNumLevels() >= r.levels) {
            unsigned off = r.lvl0SizeOff;
            for (int lv = 0; lv < r.levels && ok; ++lv) {
                const unsigned sz = U32(&raw[off]);
                const int lw = r.w >> lv ? r.w >> lv : 1, lh = r.h >> lv ? r.h >> lv : 1;
                unsigned char* q = RwRasterLock(ras, lv, rwRASTERLOCKREAD);
                if (!q) { ok = false; break; }
                const int stride = RwRasterGetStride(ras);
                const int rows = isDxt ? (lh + 3) / 4 : lh;
                const unsigned rowBytes = sz / rows;
                (void)lw;
                for (int y = 0; y < rows && ok; ++y)
                    if (std::memcmp(q + y * stride, &raw[off + 4 + y * rowBytes], rowBytes < (unsigned)stride ? rowBytes : stride)) ok = false;
                RwRasterUnlock(ras);
                off += 4 + sz;
            }
        }
        ok = ok && ras->getNumLevels() >= 1;
        if (r.levels > 1 && !(r.format & rwRASTERFORMATAUTOMIPMAP)) ok = ok && ras->getNumLevels() >= r.levels;
        if (!ok) { allMatch = false; std::printf("   MISMATCH %s fmt=%04x d3d=%s %dx%d lv=%d (raster fmt %04x lv %d)\n", r.name.c_str(), r.format, Fourcc(r.d3d).c_str(), r.w, r.h, r.levels, ras->format, ras->getNumLevels()); }
        if (shown < 6 || isPal) {
            ++shown;
            std::printf("   %-20s mask=%-12s %4dx%-4d file fmt=%04x d3d=%-4s lv=%d flags=%d -> raster fmt=%04x depth=%d lv=%d d3dfmt=%s filter=%d addr=%d/%d\n", t->name, t->mask, r.w, r.h, r.format,
                        Fourcc(r.d3d).c_str(), r.levels, r.flags, ras->format, ras->depth, ras->getNumLevels(), Fourcc(ext->format).c_str(), RwTextureGetFilterMode(t),
                        RwTextureGetAddressingU(t), RwTextureGetAddressingV(t));
        }
    }
    CHECKV(allMatch, "all %d textures match the file (level 0%s); DXT=%d PAL8=%d with mips=%d", (int)rt.size(), pal8 ? " incl. palette lookup" : "", dxt, pal8, mips);
    if (expectPal8) CHECK(pal8 > 0);
    // find by (lower case) name
    CHECK(RwTexDictionaryFindNamedTexture(d, rt[0].name.c_str()) != nullptr);
    std::string lower = rt[0].name;
    for (auto& ch : lower) ch = (char)std::tolower((unsigned char)ch);
    CHECK(RwTexDictionaryFindNamedTexture(d, lower.c_str()) != nullptr);
    CHECK(GetFirstTexture(d) == rw::Texture::fromDict(d->textures.link.next));
    // file order is reversed by the front insertion
    CHECKV(GetFirstTexture(d)->name == rt.back().name, "first=%s last in file=%s", GetFirstTexture(d)->name, rt.back().name.c_str());
    // write + re-read
    const unsigned size = RwTexDictionaryStreamGetSize(d);
    std::vector<unsigned char> buf(size + 64);
    rw::StreamMemory ms;
    ms.open(buf.data(), 0, (unsigned)buf.size());
    RwTexDictionaryStreamWrite(d, &ms);
    const unsigned written = ms.length;
    ms.position = 0;
    RwTexDictionary* d2 = nullptr;
    if (rw::findChunk(&ms, rw::ID_TEXDICTIONARY, nullptr, nullptr)) d2 = RwTexDictionaryStreamRead(&ms);
    CHECKV(d2 && d2->count() == d->count(), "write+reread: %u bytes (GetSize %u + 12)", written, size);
    if (d2) {
        CHECK(SameDicts(d, d2));
        RwTexDictionaryDestroy(d2);
    }
    // SA patched readers on the real file: whole read and Read1 + Read2 give the same dictionary as the plain reader (filters NEAREST->LINEAR aside)
    {
        rw::StreamFile sf;
        RwTexDictionary *g = nullptr, *s = nullptr;
        if (sf.open(path.c_str(), "rb") && rw::findChunk(&sf, rw::ID_TEXDICTIONARY, nullptr, nullptr)) g = RwTexDictionaryGtaStreamRead(&sf);
        sf.close();
        CHECKV(g && g->count() == d->count() && Names(g) == Names(d) && SameDicts(d, g, true), "GtaStreamRead: %d textures, same order and data", g ? g->count() : -1);
        std::vector<unsigned char> mem = ReadAll(path);
        rw::StreamMemory m1, m2;
        m1.open(mem.data(), (unsigned)mem.size(), (unsigned)mem.size());
        m2.open(mem.data(), (unsigned)mem.size(), (unsigned)mem.size());
        if (rw::findChunk(&m1, rw::ID_TEXDICTIONARY, nullptr, nullptr)) s = RwTexDictionaryGtaStreamRead1(&m1);
        const int afterRead1 = s ? s->count() : -1;
        rw::findChunk(&m2, rw::ID_TEXDICTIONARY, nullptr, nullptr);
        if (s) s = RwTexDictionaryGtaStreamRead2(&m2, s);
        CHECKV(s && s->count() == d->count() && afterRead1 == d->count() - d->count() / 2 && Names(s) == Names(d) && SameDicts(d, s, true), "Read1 (%d of %d) + Read2: same dictionary", afterRead1, d->count());
        if (g) RwTexDictionaryDestroy(g);
        if (s) RwTexDictionaryDestroy(s);
    }
    const int texBefore = rw::Texture::numAllocated, d3dBefore = rw::d3d::d3d9Globals.numTextures;
    const int n = d->count();
    RwTexDictionaryDestroy(d);
    CHECKV(rw::Texture::numAllocated == texBefore - n && rw::d3d::d3d9Globals.numTextures == d3dBefore - n, "destroy: textures -%d, D3D textures -%d", texBefore - rw::Texture::numAllocated, d3dBefore - rw::d3d::d3d9Globals.numTextures);
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    HWND wnd = CreateWindowA("STATIC", "rw_texture_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    // the game registers its dictionary plugin (CTxdStore::PluginAttach) between RwEngineInit and RwEngineOpen
    rw::registerAnisotropyPlugin();   // what RpAnisotPluginAttach will do
    g_pluginOffset = RwTexDictionaryRegisterPlugin(sizeof(PluginData), kPluginId, PlugCtor, PlugDtor, PlugCopy);
    CHECK(g_pluginOffset >= 0);
    CHECK(RwTexDictionaryRegisterPluginStream(kPluginId, PlugRead, PlugWrite, PlugSize) >= 0);
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    std::printf("D3D9 object: %s\n", rw::d3d::d3d9Globals.d3d9 ? "yes" : "NO");
    rw::Engine::start();
    IDirect3DDevice9* dev = rw::d3d::d3ddevice;
    std::printf("D3D9 device: %s\n", dev ? "yes" : "NO (nothing testable without a device)");
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;

    if (dev) {
        TextureObjectTests();
        DictionaryTests();
        ReadTests();
        StreamTests();
        GtaStreamTests();
        if (argc > 1) {
            const std::string m = argv[1];
            RealFile(m, "generic/vehicle.txd", false);
            RealFile(m, "particle.txd", false);
            RealFile(m, "hud.txd", false);
            RealFile(m, "txd/outro.txd", true);
            if (argc > 2) RealFile(argv[2], "vgsnbuild07.txd", false);   // DXT1 with 9 level mip chains (extracted from gta3.img)
        }
        rw::Engine::stop();
    }
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
    std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail;
}
