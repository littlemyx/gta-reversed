// P2B-05b unit test: RpMatFX* / RpMaterialUVAnim* / RtDict (UV-animation dictionary) of the librw shim (source/standalone/rw/{matfx,uvanim}.cpp). Runs under
// Wine; the engine is opened device-less.
// Synthetic part: MatFX effects / env-map setters / MatFXD3D9EnvMapGetData layout / texture reference handling; UV animation built by hand (linear and param key
// frames): Exists / AddAnimTime (incl. the exe's wrap that keeps the overshoot) / ApplyUpdate checked against an independent reference, material clone,
// dictionary read-back / destroy semantic (current dictionary cleared, animations survive through the material's reference).
// SA part (optional paths on the command line): infernus.dff (env-map materials), DFFs that start with a UV-animation dictionary chunk (0x2B), read the way
// Streaming.cpp does it (chunk header, RtDictSchemaStreamReadDict, SetCurrentDict, second stream for the clump, RtDictDestroy).
// Usage: rw_matfx_uvanim_test.exe [file.dff ...]. Exit code 0 = all passed.
#include "fakerw.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Section(const char* n) { std::printf("[%s]\n", n); }
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) <= e; }

static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

static rw::UVAnim* UV(RpMaterial* m) { return PLUGINOFFSET(rw::UVAnim, m, rw::uvAnimOffset); }

// builds a one-node UV animation: key frames at t = 0 .. duration (linear: uv = {m00,m01,m10,m11,x,y}; param: {theta,s0,s1,skew,x,y})
static rw::Animation* MakeAnim(const char* name, int scheme, int nFrames, const float (*uv)[6], const float* times, float duration, int channel = 0) {
    rw::AnimInterpolatorInfo* info = rw::AnimInterpolatorInfo::find(scheme);
    if (!info) return nullptr;
    rw::Animation* a = rw::Animation::create(info, nFrames, 0, duration);
    auto* custom = rw::UVAnimCustomData::get(a);
    std::strncpy(custom->name, name, 31);
    std::memset(custom->nodeToUVChannel, 0, sizeof(custom->nodeToUVChannel));
    custom->nodeToUVChannel[0] = channel;
    custom->refCount = 1;
    auto* f = static_cast<rw::UVAnimKeyFrame*>(a->keyframes);
    for (int i = 0; i < nFrames; i++) {
        f[i].time = times[i];
        std::memcpy(f[i].uv, uv[i], sizeof(f[i].uv));
        f[i].prev = i == 0 ? &f[nFrames - 1] : &f[i - 1];   // placeholder, see the real data
    }
    return a;
}

// puts the animation into slot 0 of the material (what the stream reader does) and gives the material the UVTRANSFORM effect
static void Attach(RpMaterial* m, rw::Animation* a) {
    RpMatFXMaterialSetEffects(m, rpMATFXEFFECTUVTRANSFORM);
    rw::AnimInterpolator* ip = rw::AnimInterpolator::create(a->getNumNodes(), a->interpInfo->interpKeyFrameSize);
    ip->setCurrentAnim(a);
    rw::UVAnimCustomData::get(a)->refCount++;
    UV(m)->interp[0] = ip;
    if (!UV(m)->uv[0]) UV(m)->uv[0] = rw::Matrix::create();
    if (!UV(m)->uv[1]) UV(m)->uv[1] = rw::Matrix::create();
}

static void MatrixOf(RpMaterial* m, rw::Matrix** base, rw::Matrix** dual = nullptr) {
    rw::MatFX::get(m)->getUVTransformMatrices(base, dual);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void MatFXTests() {
    Section("matfx: effects / env map / pass data");
    RpMaterial* m = RpMaterialCreate();
    CHECK(RpMatFXMaterialGetEffects(m) == rpMATFXEFFECTNULL);
    CHECK(RpMatFXMaterialGetEnvMapTexture(m) == nullptr);
    CHECK(MatFXD3D9EnvMapGetData(m, 0) == nullptr);                                     // no plugin data yet
    CHECK(RpMatFXMaterialSetEnvMapCoefficient(m, 0.5f) == m);                           // no-op without an env pass (the exe would crash)
    CHECK(RpMatFXMaterialSetEffects(m, rpMATFXEFFECTENVMAP) == m);
    CHECK(RpMatFXMaterialGetEffects(m) == rpMATFXEFFECTENVMAP);
    RwTexture* tex = RwTextureCreate(nullptr);
    std::strcpy(tex->name, "envtex");
    rw::MatFX::get(m)->setEnvTexture(tex);
    CHECK(tex->refCount == 2 && RpMatFXMaterialGetEnvMapTexture(m) == tex);
    RwFrame* frame = RwFrameCreate();
    CHECK(RpMatFXMaterialSetEnvMapFrame(m, frame) == m);
    CHECK(RpMatFXMaterialSetEnvMapCoefficient(m, 0.75f) == m);
    rw::MatFX::get(m)->setEnvFBAlpha(1);
    MatFXEnvMapData* d = MatFXD3D9EnvMapGetData(m, rpSECONDPASS);
    CHECK(d != nullptr && d->texture == tex && d->frame == frame && d->coef == 0.75f && d->useFrameBufferAlpha == 1);
    CHECK(MatFXD3D9EnvMapGetData(m, rpMAXPASS) == nullptr && MatFXD3D9EnvMapGetData(m, -1) == nullptr);
    CHECK(tex->refCount == 2);                                                           // SetEnvMapFrame takes no reference
    CHECK(RpMatFXMaterialSetEffects(m, rpMATFXEFFECTENVMAP) == m && RpMatFXMaterialGetEnvMapTexture(m) == tex);   // same type: data kept
    CHECK(RpMatFXMaterialSetEffects(m, rpMATFXEFFECTBUMPENVMAP) == m);                  // type change: pass data cleared, env texture released
    CHECK(RpMatFXMaterialGetEffects(m) == rpMATFXEFFECTBUMPENVMAP && tex->refCount == 1 && RpMatFXMaterialGetEnvMapTexture(m) == nullptr);
    rw::MatFX::get(m)->setEnvTexture(tex);                                              // env is pass 1 of BUMPENVMAP
    CHECK(RpMatFXMaterialGetEnvMapTexture(m) == tex && MatFXD3D9EnvMapGetData(m, 1)->texture == tex && MatFXD3D9EnvMapGetData(m, 0)->texture != tex);
    RpMatFXMaterialSetEnvMapCoefficient(m, 0.25f);
    CHECK(MatFXD3D9EnvMapGetData(m, 1)->coef == 0.25f);
    CHECK(RpMatFXMaterialSetEffects(m, rpMATFXEFFECTNULL) == m && RpMatFXMaterialGetEffects(m) == rpMATFXEFFECTNULL && tex->refCount == 1);   // Automobile.cpp:4089
    CHECK(RpMatFXMaterialGetEnvMapTexture(m) == nullptr);
    RpMatFXMaterialSetEffects(m, rpMATFXEFFECTDUALUVTRANSFORM);
    CHECK(RpMatFXMaterialGetEffects(m) == rpMATFXEFFECTDUALUVTRANSFORM && rw::MatFX::get(m)->fx[0].type == rw::MatFX::UVTRANSFORM && rw::MatFX::get(m)->fx[1].type == rw::MatFX::DUAL);
    RpMaterialDestroy(m);
    RwTextureDestroy(tex);
    RwFrameDestroy(frame);

    Section("matfx: atomic flag");
    RpAtomic* a = RpAtomicCreate();
    CHECK(RpMatFXAtomicQueryEffects(a) == FALSE);
    rw::MatFX::enableEffects(a);
    CHECK(RpMatFXAtomicQueryEffects(a) == TRUE);
    RpAtomicDestroy(a);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void UVAnimSynthetic() {
    Section("uvanim: linear animation, AddAnimTime / ApplyUpdate vs reference");
    // one node, channel 0; key frames at 0, 1, 2 (duration 2): pos.x 0 -> 1 -> 3, pos.y 0 -> 0.5 -> 0.5; scale 1 -> 2 -> 2
    const float uvs[3][6] = { {1, 0, 0, 1, 0, 0}, {2, 0, 0, 2, 1, 0.5f}, {2, 0, 0, 2, 3, 0.5f} };
    const float times[3] = { 0, 1, 2 };
    rw::Animation* anim = MakeAnim("lin", 0x1C0, 3, uvs, times, 2.0f);
    CHECK(anim != nullptr);
    RpMaterial* m = RpMaterialCreate();
    CHECK(RpMaterialUVAnimExists(m) == FALSE);
    // numFrames 3 with one node: SetCurrentAnim uses frames 0 and 1 as the first pair
    Attach(m, anim);
    CHECK(RpMaterialUVAnimExists(m) == TRUE);
    CHECK(RpMaterialUVAnimApplyUpdate(m) == m);
    rw::Matrix *base = nullptr, *dual = nullptr;
    MatrixOf(m, &base, &dual);
    CHECK(base == UV(m)->uv[0] && dual == UV(m)->uv[1]);
    CHECK(Near(base->right.x, 1) && Near(base->up.y, 1) && Near(base->pos.x, 0) && Near(base->pos.y, 0));
    CHECK(Near(dual->right.x, 1) && Near(dual->up.y, 1) && Near(dual->pos.x, 0));              // channel 1 untouched: identity
    CHECK(RpMaterialUVAnimAddAnimTime(m, 0.5f) == m && RpMaterialUVAnimApplyUpdate(m) == m);
    CHECK(Near(base->right.x, 1.5f) && Near(base->up.y, 1.5f) && Near(base->pos.x, 0.5f) && Near(base->pos.y, 0.25f) && Near(base->right.y, 0) && Near(base->at.z, 0));
    CHECK(RpMaterialUVAnimAddAnimTime(m, 0.75f) == m && RpMaterialUVAnimApplyUpdate(m) == m);   // t = 1.25: second pair
    CHECK(Near(base->right.x, 2) && Near(base->pos.x, 1.5f) && Near(base->pos.y, 0.5f));
    CHECK(RpMaterialUVAnimAddAnimTime(m, 0.0f) == m && RpMaterialUVAnimAddAnimTime(m, -1.0f) == m);   // non-positive deltas are ignored
    RpMaterialUVAnimApplyUpdate(m);
    CHECK(Near(base->pos.x, 1.5f) && Near(UV(m)->interp[0]->currentTime, 1.25f));
    // wrap: 1.25 + 1.5 = 2.75 > 2 -> 0.75 (the exe keeps the overshoot), key frames restart
    RpMaterialUVAnimAddAnimTime(m, 1.5f);
    CHECK(Near(UV(m)->interp[0]->currentTime, 0.75f));
    RpMaterialUVAnimApplyUpdate(m);
    CHECK(Near(base->right.x, 1.75f) && Near(base->pos.x, 0.75f) && Near(base->pos.y, 0.375f));
    // several periods at once: 0.75 + 4.5 = 5.25 -> 1.25
    RpMaterialUVAnimAddAnimTime(m, 4.5f);
    CHECK(Near(UV(m)->interp[0]->currentTime, 1.25f));
    RpMaterialUVAnimApplyUpdate(m);
    CHECK(Near(base->pos.x, 1.5f) && Near(base->pos.y, 0.5f));
    // ApplyUpdate rebuilds from identity every time
    RpMaterialUVAnimApplyUpdate(m);
    CHECK(Near(base->pos.x, 1.5f) && Near(base->right.x, 2.0f));

    Section("uvanim: material clone owns its matrices and interpolators");
    RpMaterial* c = RpMaterialClone(m);
    CHECK(c != nullptr && RpMaterialUVAnimExists(c) == TRUE);
    CHECK(UV(c)->uv[0] != nullptr && UV(c)->uv[0] != UV(m)->uv[0] && UV(c)->interp[0] != UV(m)->interp[0]);
    CHECK(UV(c)->interp[0]->currentAnim == anim && rw::UVAnimCustomData::get(anim)->refCount == 3);
    RpMatFXMaterialSetEffects(c, rpMATFXEFFECTUVTRANSFORM);
    RpMaterialUVAnimAddAnimTime(c, 0.5f);
    RpMaterialUVAnimApplyUpdate(c);
    rw::Matrix* cb = nullptr;
    MatrixOf(c, &cb);
    CHECK(cb == UV(c)->uv[0] && Near(cb->pos.x, 0.5f) && Near(base->pos.x, 1.5f));          // independent clocks
    RpMaterialDestroy(c);
    CHECK(rw::UVAnimCustomData::get(anim)->refCount == 2);

    Section("uvanim: dictionary read / current / destroy");
    rw::UVAnimDictionary* dict = rw::UVAnimDictionary::create();
    dict->add(anim);                                                                        // the dictionary takes the creation reference
    rw::Animation* other = MakeAnim("other", 0x1C0, 2, uvs, times, 1.0f);
    dict->add(other);
    CHECK(dict->count() == 2 && dict->find("LIN") == anim && dict->find("other") == other && dict->find("nope") == nullptr);
    // stream the dictionary out and back in through the shim entry points
    uint32_t sz = dict->streamGetSize();
    std::vector<uint8_t> buf(sz + 64);
    rw::StreamMemory ws; ws.open(buf.data(), 0, (uint32_t)buf.size());
    CHECK(dict->streamWrite(&ws));
    CHECK(ws.length == sz + 12);                                                            // streamGetSize excludes the 12-byte chunk header
    rw::StreamMemory rs; rs.open(buf.data(), ws.length);
    rw::ChunkHeaderInfo hdr{};
    CHECK(rw::readChunkHeaderInfo(&rs, &hdr) && hdr.type == rw::ID_UVANIMDICT);              // the game consumes the header first
    extern RtDictSchema RpUVAnimDictSchema;
    RtDict* rd = RtDictSchemaStreamReadDict(&RpUVAnimDictSchema, &rs);
    CHECK(rd != nullptr && rd->count() == 2);
    rw::Animation* ra = rd ? rd->find("lin") : nullptr;
    CHECK(ra != nullptr && ra->numFrames == 3 && Near(ra->duration, 2.0f) && ra->interpInfo->id == 0x1C0);
    if (ra) {
        auto* f = static_cast<rw::UVAnimKeyFrame*>(ra->keyframes);
        CHECK(Near(f[1].time, 1) && Near(f[1].uv[4], 1) && Near(f[2].uv[4], 3) && f[2].prev == &f[1] && f[0].prev == &f[2]);
    }
    CHECK(RtDictSchemaSetCurrentDict(&RpUVAnimDictSchema, rd) == &RpUVAnimDictSchema && rw::currentUVAnimDictionary == rd);
    // a material stream whose animation is found in the current dictionary shares it
    RpMaterial* sm = RpMaterialCreate();
    RpMatFXMaterialSetEffects(sm, rpMATFXEFFECTUVTRANSFORM);
    UV(sm)->uv[0] = rw::Matrix::create(); UV(sm)->uv[1] = rw::Matrix::create();
    if (ra) {
        rw::AnimInterpolator* ip = rw::AnimInterpolator::create(1, ra->interpInfo->interpKeyFrameSize);
        ip->setCurrentAnim(ra);
        rw::UVAnimCustomData::get(ra)->refCount++;
        UV(sm)->interp[0] = ip;
    }
    CHECK(RtDictDestroy(rd) == TRUE);
    CHECK(rw::currentUVAnimDictionary == nullptr);                                           // RtDictDestroy clears the schema's current dictionary
    CHECK(ra && rw::UVAnimCustomData::get(ra)->refCount == 1 && RpMaterialUVAnimExists(sm));   // still referenced by the material: alive
    RpMaterialUVAnimAddAnimTime(sm, 1.5f); RpMaterialUVAnimApplyUpdate(sm);
    rw::Matrix* sb = nullptr; MatrixOf(sm, &sb);
    CHECK(sb && Near(sb->pos.x, 2.0f));                                                      // 1.5 on 0 -> 1 -> 3
    CHECK(RtDictDestroy(nullptr) == FALSE);
    RpMaterialDestroy(sm);                                                                   // frees the last reference
    dict->destroy();                                                                         // anim + other
    RpMaterialDestroy(m);
}

static void UVAnimParam() {
    Section("uvanim: param animation (theta wrap, rotation about the centre)");
    const float kPi = 3.14159265358979f;
    // {theta, s0, s1, skew, x, y}: theta 3.0 -> -3.0 (shortest way = +0.2832 through pi), scales 1 -> 2
    const float uvs[2][6] = { {3.0f, 1, 1, 0, 0, 0}, {-3.0f, 2, 2, 0, 0.5f, 0} };
    const float times[2] = { 0, 1 };
    rw::Animation* anim = MakeAnim("par", 0x1C1, 2, uvs, times, 1.0f);
    CHECK(anim != nullptr);
    RpMaterial* m = RpMaterialCreate();
    Attach(m, anim);
    RpMaterialUVAnimAddAnimTime(m, 0.5f);
    RpMaterialUVAnimApplyUpdate(m);
    rw::Matrix* b = nullptr; MatrixOf(m, &b);
    // reference (double): interpolated theta = 3 + 0.5 * (2*pi - 6 -> wrapped diff -6 + 2pi = 0.2832) ; s = 1.5 ; pos = (0.25, 0)
    const double dth = -6.0 + 2.0 * kPi;
    const double th = 3.0 + 0.5 * dth, s = 1.5, px = 0.25;
    // matrix = S(s) (right.x = s0, up.y = s1) with translation (px, 0), then post-concat: T(-.5) R(th) T(+.5)
    const double c = std::cos(th), sn = std::sin(th);
    double r00 = s * c, r01 = s * sn;      // right = (s0*c, s0*sin) after rotating the right vector
    double u0 = -s * sn, u1 = s * c;
    double tx = px - 0.5, ty = 0 - 0.5;    // translate(-.5)
    double ttx = tx * c - ty * sn + 0.5, tty = tx * sn + ty * c + 0.5;
    CHECK(Near(b->right.x, (float)r00, 2e-3f) && Near(b->right.y, (float)r01, 2e-3f) && Near(b->up.x, (float)u0, 2e-3f) && Near(b->up.y, (float)u1, 2e-3f));
    CHECK(Near(b->pos.x, (float)ttx, 2e-3f) && Near(b->pos.y, (float)tty, 2e-3f));
    RpMaterialDestroy(m);
    rw::UVAnimCustomData::get(anim)->destroy(anim);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static RpAtomic* CollectAtomicCB(RpAtomic* a, void* data) { static_cast<std::vector<RpAtomic*>*>(data)->push_back(a); return a; }
static RpMaterial* CollectMatCB(RpMaterial* mat, void* data) { static_cast<std::vector<RpMaterial*>*>(data)->push_back(mat); return mat; }

static void ModelTests(const char* path) {
    Section((std::string("SA model: ") + path).c_str());
    auto bytes = ReadFile(path);
    if (bytes.empty()) { CHECK(false); return; }
    // Streaming.cpp:466-486 -- first stream: dictionary (if the file starts with one); second stream: the clump
    RtDict* dict = nullptr;
    {
        rw::StreamMemory s1; s1.open(bytes.data(), (uint32_t)bytes.size());
        rw::ChunkHeaderInfo hdr{};
        CHECK(rw::readChunkHeaderInfo(&s1, &hdr));
        if (hdr.type == rw::ID_UVANIMDICT) {
            dict = RtDictSchemaStreamReadDict(&RpUVAnimDictSchema, &s1);
            CHECK(dict != nullptr);
            RtDictSchemaSetCurrentDict(&RpUVAnimDictSchema, dict);
            std::printf("  dictionary: %d animation(s)\n", dict ? dict->count() : -1);
            if (dict) for (rw::LLLink* lnk = dict->animations.link.next; lnk != &dict->animations.link; lnk = lnk->next) {
                rw::Animation* an = rw::UVAnimDictEntry::fromDict(lnk)->anim;
                auto* cd = rw::UVAnimCustomData::get(an);
                auto* kf = static_cast<rw::UVAnimKeyFrame*>(an->keyframes);
                std::printf("    '%s' scheme 0x%X frames %d duration %.3f nodes %d channels", cd->name, an->interpInfo->id, an->numFrames, an->duration, an->getNumNodes());
                for (int i = 0; i < an->getNumNodes(); i++) std::printf(" %d", cd->nodeToUVChannel[i]);
                std::printf("  kf0 prev %d kf1 prev %d t=%.2f uv=(%.3f %.3f %.3f %.3f %.3f %.3f)\n", (int)(kf[0].prev - kf), (int)(kf[1].prev - kf), kf[0].time, kf[0].uv[0], kf[0].uv[1], kf[0].uv[2], kf[0].uv[3], kf[0].uv[4], kf[0].uv[5]);
                CHECK(an->interpInfo->id == 0x1C0 || an->interpInfo->id == 0x1C1);
                CHECK(an->numFrames >= an->getNumNodes() * 2 && an->duration > 0);
            }
        }
    }
    rw::StreamMemory s2; s2.open(bytes.data(), (uint32_t)bytes.size());
    uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&s2, rw::ID_CLUMP, &len, &ver));
    RpClump* clump = RpClumpStreamRead(&s2);
    CHECK(clump != nullptr);
    if (dict) { CHECK(RtDictDestroy(dict) == TRUE); CHECK(rw::currentUVAnimDictionary == nullptr); }
    if (!clump) return;
    std::vector<RpAtomic*> atomics;
    RpClumpForAllAtomics(clump, CollectAtomicCB, &atomics);
    int nMat = 0, nEnv = 0, nUV = 0, nAtomFx = 0;
    std::vector<RpMaterial*> uvMats;
    for (RpAtomic* a : atomics) {
        nAtomFx += RpMatFXAtomicQueryEffects(a) ? 1 : 0;
        std::vector<RpMaterial*> mats;
        RpGeometryForAllMaterials(RpAtomicGetGeometry(a), CollectMatCB, &mats);
        for (RpMaterial* mt : mats) {
            nMat++;
            const RpMatFXMaterialFlags fx = RpMatFXMaterialGetEffects(mt);
            if (fx == rpMATFXEFFECTENVMAP) {
                nEnv++;
                RwTexture* et = RpMatFXMaterialGetEnvMapTexture(mt);
                MatFXEnvMapData* d = MatFXD3D9EnvMapGetData(mt, 0);
                CHECK(d != nullptr && d->texture == et);
                if (nEnv == 1) std::printf("  env material: tex %p coef %.3f frame %p fbAlpha %d\n", (void*)et, d->coef, (void*)d->frame, d->useFrameBufferAlpha);
                CHECK(d->coef >= 0.0f && d->coef <= 4.0f);
            }
            if (RpMaterialUVAnimExists(mt)) { nUV++; uvMats.push_back(mt); }
        }
    }
    std::printf("  atomics %zu (matfx flag %d), materials %d, env-map %d, uv-anim %d\n", atomics.size(), nAtomFx, nMat, nEnv, nUV);
    for (RpMaterial* mt : uvMats) {
        CHECK(RpMatFXMaterialGetEffects(mt) == rpMATFXEFFECTUVTRANSFORM || RpMatFXMaterialGetEffects(mt) == rpMATFXEFFECTDUALUVTRANSFORM);
        RpMaterialUVAnimApplyUpdate(mt);
        rw::Matrix* b0 = nullptr; MatrixOf(mt, &b0);
        CHECK(b0 != nullptr);
        if (!b0) continue;
        const float before[6] = { b0->right.x, b0->right.y, b0->up.x, b0->up.y, b0->pos.x, b0->pos.y };
        RpMaterialUVAnimAddAnimTime(mt, 0.37f);
        RpMaterialUVAnimApplyUpdate(mt);
        const float after[6] = { b0->right.x, b0->right.y, b0->up.x, b0->up.y, b0->pos.x, b0->pos.y };
        bool changed = false;
        for (int i = 0; i < 6; i++) changed |= !Near(before[i], after[i], 1e-6f);
        std::printf("  uv material: t0 (%.3f %.3f | %.3f %.3f | %.3f %.3f)  +0.37s (%.3f %.3f | %.3f %.3f | %.3f %.3f) changed=%d\n", before[0], before[1], before[2], before[3], before[4], before[5],
                    after[0], after[1], after[2], after[3], after[4], after[5], changed);
        CHECK(changed);
        for (int i = 0; i < 6; i++) CHECK(std::isfinite(after[i]));
        // run a few seconds in 1/30 s steps: stays finite, the animation clock stays inside [0, duration]
        bool ok = true;
        for (int i = 0; i < 300; i++) {
            RpMaterialUVAnimAddAnimTime(mt, 1.0f / 30.0f);
            RpMaterialUVAnimApplyUpdate(mt);
            ok &= std::isfinite(b0->pos.x) && std::isfinite(b0->right.x);
            for (int k = 0; k < 8; k++) if (UV(mt)->interp[k]) ok &= UV(mt)->interp[k]->currentTime >= 0 && UV(mt)->interp[k]->currentTime <= UV(mt)->interp[k]->currentAnim->duration + 1e-3f;
        }
        CHECK(ok);
    }
    RpClumpDestroy(clump);
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    HWND wnd = CreateWindowA("STATIC", "rw_matfx_uvanim_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    // the game's PluginAttach order (app.cpp:140-152): world, skin, hanim, ..., matfx, uvanim -- all before RwEngineOpen
    CHECK(RpWorldPluginAttach() == TRUE);
    CHECK(RpMatFXPluginAttach() == TRUE);
    CHECK(RpUVAnimPluginAttach() == TRUE);
    CHECK(RpMatFXPluginAttach() == TRUE && RpUVAnimPluginAttach() == TRUE);                 // idempotent
    CHECK(rw::Material::s_plglist.getPluginOffset(rw::ID_MATFX) > 0 && rw::Material::s_plglist.getPluginOffset(rw::ID_UVANIMATION) > 0);
    rw::EngineOpenParams params{};
    params.window = wnd;
    rw::d3d::renderdevice.system = [](rw::DeviceReq, void*, int32_t) -> int32_t { return 1; };   // device-less (see rw_world_geometry_test)
    CHECK(rw::Engine::open(&params));
    rw::Engine::s_plglist.construct(rw::engine);
    rw::Driver::s_plglist[rw::PLATFORM_NULL].construct(rw::engine->driver[rw::PLATFORM_NULL]);
    CHECK(rw::AnimInterpolatorInfo::find(0x1C0) != nullptr && rw::AnimInterpolatorInfo::find(0x1C1) != nullptr);   // uvanim's engine plugin registered the two schemes at open

    MatFXTests();
    UVAnimSynthetic();
    UVAnimParam();
    for (int i = 1; i < argc; i++) ModelTests(argv[i]);

    rw::Driver::s_plglist[rw::PLATFORM_NULL].destruct(rw::engine->driver[rw::PLATFORM_NULL]);
    rw::Engine::s_plglist.destruct(rw::engine);
    DestroyWindow(wnd);
    std::printf("\nrw_matfx_uvanim_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}
