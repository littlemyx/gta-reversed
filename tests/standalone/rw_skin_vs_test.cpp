// P2B-24a: differential test of the skin vertex-shader composer (source/standalone/rw/skin_vs.cpp) against the exe's own composer (exe-oracle, see game_oracle.h).
//
// The test maps gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original VAs and runs the exe's machine code next to the port:
//   * text: the exe's 0x75F0B0 with D3DXAssembleShader (0x7652AE) patched to capture the text (buffer 0xC93AF8) and fail, the sprintf pointer ([[0xC97B24]+0xF0]) = a host vsprintf
//   * NumLightConstants 0x75EDD0, the init 0x760CF0 (model / constant table), cache 0x75EED0 with fake assemble/CreateVertexShader (0x7652AE / 0x7FAC60) and fake COM objects
//   * assembler: the exe's statically linked D3DXAssembleShader (0x7652AE, run in the oracle with the IAT bound to the host) vs d3dcompiler_47 D3DAssemble on the composed texts, bytecode compared
//   * device test (D3D9 HAL; Wine/wined3d is fine): the port's GetVertexShader over a representative key set for the three shader models (assembler: d3dcompiler_47 D3DAssemble)
// usage: rw_skin_vs_test.exe [-n randomKeys] [-q] [--no-device] [--only-device] [--only-asm] [--dump key32hex]   (exit 0 = no mismatch / no device failure; 77 = oracle not available)
#include "game_oracle.h"

#include <d3d9.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "standalone/rw/skin_vs.h"

namespace rw::d3d { extern IDirect3DDevice9* d3ddevice; }
using namespace notsa::skinvs;

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { ++g_fail; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) { for (int i = 0; i < 4; ++i) u32(); }
    uint32_t u32() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    int below(int n) { return (int)(u32() % (uint32_t)n); }
};

// ---------------------------------------------------------------------------------------------------------------- exe side
static uint8_t g_engine[0x400];                                     // fake RwEngineInstance: +8 u16 timestamp, +0xF0 sprintf
static int __cdecl HostSprintf(char* buf, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    const int n = std::vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}
static std::string g_capText;
static int         g_capCalls = 0;
static bool        g_cacheMode = false;
static bool        g_asmFail = true;                                // fake assembler: fail (text capture only) / succeed (cache test)

struct Log { std::vector<std::string> ev; };
static Log g_logExe, g_logPort;
struct FakeShader : IUnknown {
    int serial; Log* log; long refs = 1;
    FakeShader(int s, Log* l) : serial(s), log(l) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { char b[32]; std::snprintf(b, sizeof b, "R%d", serial); log->ev.push_back(b); return 0; }   // never freed: pointers stay comparable
};
struct FakeBlob : IUnknown {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override { delete this; return 0; }
    virtual void* STDMETHODCALLTYPE GetBufferPointer() { return this; }
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() { return 4; }
};
static int g_serialExe = 0, g_serialPort = 0;
static uint32_t Hash(const std::string& s) { uint32_t h = 2166136261u; for (unsigned char c : s) h = (h ^ c) * 16777619u; return h; }

static HRESULT __stdcall FakeAssemble(const char* src, UINT len, void*, void*, UINT, void** out, void*) {
    ++g_capCalls;
    g_capText.assign(src, len);
    if (out) *out = nullptr;
    if (g_asmFail) { if (g_cacheMode) { char b[48]; std::snprintf(b, sizeof b, "A%08X", Hash(g_capText)); g_logExe.ev.push_back(b); } return -1; }
    char b[48]; std::snprintf(b, sizeof b, "A%08X", Hash(g_capText)); g_logExe.ev.push_back(b);
    *out = new FakeBlob;
    return 0;
}
static int __cdecl FakeCreateVS(const void*, void** out) { *out = new FakeShader(++g_serialExe, &g_logExe); return 1; }
static IDirect3DVertexShader9* PortHook(const std::string& text) {
    char b[48]; std::snprintf(b, sizeof b, "A%08X", Hash(text)); g_logPort.ev.push_back(b);
    return reinterpret_cast<IDirect3DVertexShader9*>(new FakeShader(++g_serialPort, &g_logPort));
}

using ExeCompose = void* (__cdecl*)(const uint8_t*, uint8_t*);       // 0x75F0B0 and 0x75EED0 (key*, layout*) -> shader
using ExeNum     = int  (__cdecl*)(const uint8_t*);                   // 0x75EDD0
using ExeVoid    = void (__cdecl*)();                                  // 0x760CF0 init, 0x75EE60 free
static ExeCompose ExeComposeFn() { return reinterpret_cast<ExeCompose>(0x75F0B0); }
static ExeCompose ExeGetFn()     { return reinterpret_cast<ExeCompose>(0x75EED0); }
static ExeNum     ExeNumFn()     { return reinterpret_cast<ExeNum>(0x75EDD0); }
static ExeVoid    ExeInitFn()    { return reinterpret_cast<ExeVoid>(0x760CF0); }
static ExeVoid    ExeFreeFn()    { return reinterpret_cast<ExeVoid>(0x75EE60); }
static uint32_t& W(unsigned va) { return *reinterpret_cast<uint32_t*>(va); }

// caps words the exe's init reads (its private D3DCAPS9 copy at 0xC9BF00): VertexShaderVersion 0xC9BFC4, MaxVertexShaderConst 0xC9BFC8, DeclTypes 0xC9BFEC, DynamicFlowControlDepth 0xC9BFFC
static void ExeSetCaps(unsigned vsVer, unsigned maxConst, unsigned flowDepth) { W(0xC9BFC4) = vsVer; W(0xC9BFC8) = maxConst; W(0xC9BFFC) = flowDepth; }

static void SetEngineStamp(unsigned stamp) { *reinterpret_cast<uint16_t*>(g_engine + 8) = (uint16_t)stamp; SetFrameStamp((uint16_t)stamp); }

// ---------------------------------------------------------------------------------------------------------------- key generators
static Key MakeKey(uint32_t v) { Key k; std::memcpy(k.b, &v, 4); return k; }
static uint32_t KeyU(const Key& k) { uint32_t v; std::memcpy(&v, k.b, 4); return v; }
// biased towards combinations that occur in the game: few lights, N in 0..4, normals mostly on, M in 0..12, fog 0..3
static uint32_t RandomKey(Rng& r, bool wild) {
    uint8_t b[4];
    if (wild) { uint32_t v = r.u32() ^ (r.u32() << 16); std::memcpy(b, &v, 4); return v; }
    const int a = r.below(3) ? r.below(4) : r.below(16);
    const int bb = r.below(3) ? r.below(3) : r.below(16);
    const int c = r.below(4) ? 0 : r.below(5);
    b[0] = (uint8_t)(a | (bb << 4));
    b[1] = (uint8_t)(c | ((r.below(4) ? r.below(4) : r.below(16)) << 4));
    int N = r.below(3) ? 0 : (r.below(4) ? r.below(5) : r.below(8));
    b[2] = (uint8_t)((r.below(8) == 0) | (N << 1) | ((r.below(3) == 0) << 4) | ((r.below(5) != 0) << 5) | ((r.below(3) == 0) << 6) | ((r.below(3) == 0) << 7));
    int M = r.below(2) ? 0 : (r.below(8) ? r.below(13) : r.below(64));
    b[3] = (uint8_t)((r.below(3) == 0 ? r.below(4) : 0) | (M << 2));
    uint32_t v; std::memcpy(&v, b, 4); return v;
}

// ---------------------------------------------------------------------------------------------------------------- text differential
struct TextStats { long compared = 0, mismatches = 0, skippedBig = 0; };
static bool g_quiet = false;

static bool CompareText(uint32_t keyv, TextStats& st, int model, unsigned maxConst) {
    const Key key = MakeKey(keyv);
    Layout portLay;
    const std::string portText = ComposeText(key, portLay, model, maxConst);
    if (portText.size() > 0xF00) { ++st.skippedBig; return true; }                  // the exe has no bound check on its 4 KB buffer: skip keys that would overrun it
    uint8_t exeLay[8];
    g_capText.clear(); g_capCalls = 0;
    ExeComposeFn()(key.b, exeLay);
    ++st.compared;
    bool ok = true;
    if (model == 0) { ok = g_capCalls == 0 && portText.empty(); }
    else ok = g_capCalls == 1 && g_capText == portText;
    ok = ok && std::memcmp(exeLay, portLay.reg, 8) == 0;
    if (!ok) {
        ++st.mismatches;
        if (st.mismatches <= 5) {
            std::printf("  MISMATCH key=%08X model=0x%X maxConst=%u calls=%d layoutEq=%d\n", keyv, model, maxConst, g_capCalls, std::memcmp(exeLay, portLay.reg, 8) == 0);
            size_t i = 0; while (i < g_capText.size() && i < portText.size() && g_capText[i] == portText[i]) ++i;
            std::printf("    first difference at %zu\n    exe : %.90s\n    port: %.90s\n", i, g_capText.c_str() + (i > 40 ? i - 40 : 0), portText.c_str() + (i > 40 ? i - 40 : 0));
        }
    }
    return ok;
}

static void SetModelBoth(int model, unsigned maxConst) {
    // exe: through its own init (0x760CF0) so the init is tested too
    const unsigned ver = model == kModelVS2x ? 0x300 : model == kModelVS20 ? 0x200 : model == kModelVS11 ? 0x101 : 0;
    ExeSetCaps(ver, maxConst, 0);
    if (model == kModelVS2x && ver != 0x300) W(0xC9BFFC) = 1;
    ExeInitFn()();
    SetModel(model, maxConst < 0x100 ? maxConst : 0x100);
}

static void TextSection(int model, unsigned maxConst, int randomKeys, bool full) {
    SetModelBoth(model, maxConst);
    CHECK((int)W(0xC94C00) == model, "exe model %u != %d", W(0xC94C00), model);
    if (model) CHECK(W(0xC94C04) == (maxConst < 0x100 ? maxConst : 0x100), "exe maxConst %u", W(0xC94C04));
    const unsigned mc = (unsigned)W(0xC94C04);
    TextStats st;
    std::vector<uint32_t> keys;
    keys.push_back(0);
    for (int i = 0; i < 32; ++i) keys.push_back(1u << i);                                            // every single bit
    for (int i = 0; i < 32; ++i) for (int j = i + 1; j < 32; ++j) keys.push_back((1u << i) | (1u << j));   // every pair
    for (int by = 0; by < 4; ++by) for (int v = 0; v < 256; ++v) keys.push_back((uint32_t)v << (8 * by));  // every value of every byte alone
    for (uint32_t k : keys) CompareText(k, st, model, mc);
    const long sweepKeys = st.compared;
    long sweep2 = 0;
    // (b2, b3) exhaustive for a set of light descriptors (b0, b1)
    static const uint16_t kLights[] = { 0x0000, 0x0001, 0x0004, 0x0010, 0x0030, 0x0050, 0x0100, 0x0400, 0x3000, 0x1221, 0x2443, 0x1111, 0x3114, 0x2301 };
    const int nl = full ? (int)(sizeof kLights / sizeof kLights[0]) : 3;
    for (int li = 0; li < nl; ++li) {
        const uint32_t l = kLights[li];
        // kLights = (b1 << 8) | b0
        for (uint32_t b23 = 0; b23 < 0x10000; ++b23) { CompareText(l | (b23 << 16), st, model, mc); ++sweep2; }
    }
    Rng r(0xABCDEF + model);
    for (int i = 0; i < randomKeys; ++i) CompareText(RandomKey(r, false), st, model, mc);
    for (int i = 0; i < randomKeys / 4; ++i) CompareText(RandomKey(r, true), st, model, mc);
    std::printf("  text model=0x%02X maxConst=%3u : compared %ld (sweep %ld + (b2,b3) exhaustive %ld + random), mismatches %ld, skipped(>3.8KB) %ld\n", model, mc, st.compared, sweepKeys, sweep2, st.mismatches, st.skippedBig);
    CHECK(st.mismatches == 0, "text mismatches %ld (model 0x%X)", st.mismatches, model);
}

// ---------------------------------------------------------------------------------------------------------------- NumLightConstants
static void NumConstSection(int count) {
    Rng r(777);
    long bad = 0;
    for (int i = 0; i < count; ++i) {
        uint32_t v = (i < 0x10000) ? (uint32_t)i << 16 | (uint32_t)(i * 2654435761u >> 16) : (r.u32() ^ (r.u32() << 16));
        const Key k = MakeKey(v);
        const int e = ExeNumFn()(k.b), p = NumLightConstants(k);
        if (e != p) { if (++bad <= 5) std::printf("  NumLightConstants key=%08X exe=%d port=%d\n", v, e, p); }
    }
    std::printf("  NumLightConstants: %d keys, mismatches %ld\n", count, bad);
    CHECK(bad == 0, "NumLightConstants mismatches");
    // the init table at 0xC94AF8 against the port's
    const Key k0 = MakeKey(0);
    (void)k0;
}

// ---------------------------------------------------------------------------------------------------------------- cache behaviour
static int SerialOf(IDirect3DVertexShader9* s) { return s ? static_cast<FakeShader*>(reinterpret_cast<IUnknown*>(s))->serial : 0; }
static void CacheSection(int distinct, int calls, bool assembleFails, uint64_t seed) {
    // reset both: free releases (logged), init zeroes the exe cache
    g_asmFail = false;
    SetModelBoth(kModelVS2x, 0x100);
    ExeFreeFn()(); ExeInitFn()(); Shutdown();
    SetModel(kModelVS2x, 0x100);
    g_logExe.ev.clear(); g_logPort.ev.clear(); g_serialExe = g_serialPort = 0;
    g_asmFail = assembleFails; g_cacheMode = true;
    SetAssembleHook(assembleFails ? [](const std::string& t) -> IDirect3DVertexShader9* { char b[48]; std::snprintf(b, sizeof b, "A%08X", Hash(t)); g_logPort.ev.push_back(b); return nullptr; } : PortHook);
    Rng r(seed);
    std::vector<uint32_t> pool;
    while ((int)pool.size() < distinct) { uint32_t k = RandomKey(r, false); if (k != 0xFFFFFFFFu) pool.push_back(k); }
    unsigned stamp = r.u32() & 0xFFFF;
    long bad = 0, hits = 0, repl = 0;
    for (int i = 0; i < calls; ++i) {
        if (r.below(4) == 0) { stamp = (stamp + 1 + r.below(3)) & 0xFFFF; SetEngineStamp(stamp); }
        else if (i == 0) SetEngineStamp(stamp);
        // zipf-ish selection: favour a hot subset so that hits and LRU decisions both occur
        const uint32_t kv = pool[r.below(4) ? r.below(distinct < 20 ? distinct : 20) : r.below(distinct)];
        const Key key = MakeKey(kv);
        const int cBefore = W(0xC94BF8); const size_t evBefore = g_logExe.ev.size();
        uint8_t exeLay[8]; Layout portLay;
        void* es = ExeGetFn()(key.b, exeLay);
        IDirect3DVertexShader9* ps = GetVertexShader(key, portLay);
        const bool ok = SerialOf((IDirect3DVertexShader9*)es) == SerialOf(ps) && std::memcmp(exeLay, portLay.reg, 8) == 0 && g_logExe.ev == g_logPort.ev && (int)W(0xC94BF8) == CacheCount();
        bool composed = false;
        for (size_t q = evBefore; q < g_logExe.ev.size(); ++q) composed |= g_logExe.ev[q][0] == 'A';
        if (composed) ++repl; else ++hits;
        if (!ok) { if (++bad <= 5) std::printf("  CACHE mismatch at call %d key=%08X exeSerial=%d portSerial=%d count %u/%d\n", i, kv, SerialOf((IDirect3DVertexShader9*)es), SerialOf(ps), W(0xC94BF8), CacheCount()); break; }
    }
    // whole array
    long arrBad = 0;
    const int cnt = (int)W(0xC94BF8);
    for (int i = 0; i < cnt && i < 0x100; ++i) {
        const uint8_t* e = reinterpret_cast<const uint8_t*>(0xC926F8 + 0x14 * i);
        uint32_t key, st2; IDirect3DVertexShader9* sh; uint8_t lay[8];
        CacheEntryAt(i, key, st2, sh, lay);
        const bool eq = *(const uint32_t*)e == key && *(const uint32_t*)(e + 4) == st2 && SerialOf(*(IDirect3DVertexShader9**)(e + 8)) == SerialOf(sh) && std::memcmp(e + 0xC, lay, 8) == 0;
        if (!eq) ++arrBad;
    }
    std::printf("  cache distinct=%d calls=%d assembleFails=%d: final count %d, hits %ld, composes %ld, serials %d, releases %ld, mismatches %ld, array diffs %ld, cur==%s\n", distinct, calls, assembleFails, cnt, hits, repl, g_serialExe,
                (long)std::count_if(g_logExe.ev.begin(), g_logExe.ev.end(), [](const std::string& s) { return s[0] == 'R'; }), bad, arrBad, W(0xC94BFC) == 0xC926F8 + 0x14 * 0 ? "arr0" : "other");
    CHECK(bad == 0 && arrBad == 0, "cache mismatch");
    SetAssembleHook(nullptr); g_cacheMode = false;
}

// representative keys: the shapes of the game's peds and vehicles (skinned peds with 1-4 bones, lights, prelit, fog, env mapping, morph, tangent space), within the constant budget
static std::vector<uint32_t> RealisticKeys(size_t count, uint64_t seed, unsigned maxc) {
    Rng r(seed);
    std::vector<uint32_t> keys;
    static const uint32_t kFixed[] = { 0x00000000, 0x0000102A & 0xFFFFFFFF, 0x20003011 & 0, 0x00002A01 };   // N=1 skin + normals + lights; plain
    for (uint32_t k : kFixed) keys.push_back(k);
    while (keys.size() < count) {
        uint32_t k = RandomKey(r, false);
        const Key kk = MakeKey(k);
        const unsigned N = (kk.b[2] >> 1) & 7;
        if (N > 4 || (kk.b[1] >> 4) > 4) continue;
        { Layout l0; if (ComposeText(kk, l0, kModelVS2x, 0x100).find("(null)") != std::string::npos) continue; }   // normal-dependent code without normals: the exe emits garbage too
        if ((unsigned)NumLightConstants(kk) + 3 * N > maxc) continue;                              // the exe's light budget loop (0x7C8221) guarantees this
        keys.push_back(k);
    }
    return keys;
}

// ---------------------------------------------------------------------------------------------------------------- assembler differential
// The exe assembles the composed text with its statically linked D3DX (D3DXAssembleShader 0x7652AE, flags 4); we use d3dcompiler_47's D3DAssemble.
// Here the EXE's assembler runs in the oracle (IAT bound to the host kernel32, CRT heap = process heap) on the same text and the bytecode is compared.
static void __cdecl HostMathErr(int, int, int, int) {}
static uint32_t g_fakePtd[0x100];
static void* __cdecl HostGetPtd() { return g_fakePtd; }
using ExeAsmFn = HRESULT(__stdcall*)(const char*, UINT, const void*, const void*, DWORD, void**, void**);
struct ExeBlobVt { void* qi; void* addref; ULONG(__stdcall* release)(void*); void*(__stdcall* ptr)(void*); SIZE_T(__stdcall* size)(void*); };

static bool ExeAssemble(const std::string& text, std::vector<uint8_t>& out, std::string* err) {
    void* shader = nullptr; void* errors = nullptr;
    const HRESULT hr = reinterpret_cast<ExeAsmFn>(0x7652AE)(text.data(), (UINT)text.size(), nullptr, nullptr, 4, &shader, nullptr);
    (void)errors;
    if (hr < 0 || !shader) { if (err) *err = "exe D3DXAssembleShader hr=" + std::to_string((unsigned)hr); return false; }
    const ExeBlobVt* vt = *reinterpret_cast<ExeBlobVt**>(shader);
    const uint8_t* p = static_cast<const uint8_t*>(vt->ptr(shader)); const SIZE_T n = vt->size(shader);
    out.assign(p, p + n);
    vt->release(shader);
    return true;
}


static void AssemblerSection(const char* exePath, const uint8_t origAsm[5]) {
    std::printf("[assembler bytecode: exe D3DX vs d3dcompiler_47]\n");
    std::memcpy(reinterpret_cast<void*>(0x7652AE), origAsm, 5);                    // undo the text-capture patch: the real exe assembler
    oracle::g_where = "exe D3DXAssembleShader";
    std::printf("  IAT slots bound to the host: %d\n", oracle::BindImports(exePath));
    { static CRITICAL_SECTION lockTabLock; InitializeCriticalSection(&lockTabLock); W(0x8E31C0 + 8 * 0xA) = (uint32_t)(uintptr_t)&lockTabLock; }   // _mtinitlocks: lock 0xA (_LOCKTAB_LOCK) exists before any lazily created lock
    W(0xC9C2F8) = 1;                                                                 // CRT heap type 1 = plain HeapAlloc/HeapFree on the heap handle at 0xC9C2F4 (what the startup code set up)
    W(0xC9C2F4) = (uint32_t)(uintptr_t)GetProcessHeap();
    oracle::Patch(0x82872C, reinterpret_cast<void*>(&HostMathErr));                  // as in the other oracle tests: errno / _getptd need TLS state
    oracle::Patch(0x827B3D, reinterpret_cast<void*>(&HostGetPtd));
    // sanity: a hand-written shader assembles in both and decodes to the same bytes
    {
        std::vector<uint8_t> a, b; std::string e;
        const std::string t = "vs_1_1\ndcl_position v0\nm4x4 oPos, v0, c0\nmov oD0, c4\n";
        const std::string t11 = "vs_1_1\nm4x4 oPos, v0, c0\nmov oD0, c4\n";
        const bool okE = ExeAssemble(t11, a, &e), okP = AssembleToBytes(t11, b);
        std::printf("  smoke: exe %s (%zu bytes), d3dcompiler %s (%zu bytes), %s\n", okE ? "ok" : e.c_str(), a.size(), okP ? "ok" : "FAILED", b.size(), a == b ? "identical" : "DIFFERENT");
        (void)t;
        CHECK(okE, "exe assembler failed on the smoke text: %s", e.c_str());
        if (!okE) return;
    }
    struct { int model; const char* name; } models[3] = { { kModelVS2x, "vs_2_x" }, { kModelVS20, "vs_2_0" }, { kModelVS11, "vs_1_1" } };
    long total = 0, identical = 0, differ = 0, bothFail = 0, onlyOne = 0, shown = 0;
    long sizeDiff = 0, instrDiff = 0;
    Rng r(9001);
    std::vector<uint32_t> keys = RealisticKeys(400, 4242, 0x100);                    // the 400 keys of the device section ...
    { std::vector<uint32_t> more = RealisticKeys(3000, 777, 0x100); keys.insert(keys.end(), more.begin() + 400, more.end()); }   // ... plus 2600 more
    for (auto& m : models) {
        long mi = 0, md = 0;
        for (uint32_t kv : keys) {
            const Key k = MakeKey(kv);
            Layout lay;
            const std::string text = ComposeText(k, lay, m.model, 0x100);
            if (text.empty() || text.size() > 0xF00) continue;
            ++total;
            std::vector<uint8_t> ex, po; std::string err;
            const bool oe = ExeAssemble(text, ex, &err), op = AssembleToBytes(text, po);
            if (!oe && !op) { ++bothFail; continue; }
            if (oe != op) { ++onlyOne; ++md; if (shown++ < 4) std::printf("  key %08X %s: exe %s, d3dcompiler %s\n", kv, m.name, oe ? "ok" : "FAILS", op ? "ok" : "FAILS"); continue; }
            if (ex == po) { ++identical; ++mi; continue; }
            ++differ; ++md;
            if (ex.size() != po.size()) ++sizeDiff; else ++instrDiff;
            if (shown++ < 4) {
                size_t i = 0; while (i < ex.size() && i < po.size() && ex[i] == po[i]) ++i;
                std::printf("  DIFF key %08X %s: exe %zu bytes, d3dcompiler %zu bytes, first difference at byte %zu\n    exe  :", kv, m.name, ex.size(), po.size(), i);
                for (size_t j = i & ~3u; j < ex.size() && j < (i & ~3u) + 24; ++j) std::printf(" %02X", ex[j]);
                std::printf("\n    d3dc :");
                for (size_t j = i & ~3u; j < po.size() && j < (i & ~3u) + 24; ++j) std::printf(" %02X", po[j]);
                std::printf("\n");
            }
        }
        std::printf("  %s: %ld identical, %ld different\n", m.name, mi, md);
    }
    (void)r;
    std::printf("  assembler: %ld texts, %ld byte-identical, %ld different (%ld size, %ld same size), %ld accepted by only one, %ld rejected by both\n", total, identical, differ, sizeDiff, instrDiff, onlyOne, bothFail);
    std::printf("TOTAL: %ld texts, %ld mismatches\n", total, differ + onlyOne);
    CHECK(differ + onlyOne == 0, "assembler bytecode mismatches: %ld different + %ld accepted by one only", differ, onlyOne);
}

// ---------------------------------------------------------------------------------------------------------------- device test
static bool CreateDevice(IDirect3DDevice9** out, IDirect3D9** d3dOut, HWND* wndOut) {
    HWND wnd = CreateWindowA("STATIC", "rw_skin_vs_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::printf("  device: Direct3DCreate9 failed\n"); return false; }
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.BackBufferWidth = 320; pp.BackBufferHeight = 240;
    pp.hDeviceWindow = wnd; pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, out);
    if (FAILED(hr)) hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, out);
    if (FAILED(hr)) { std::printf("  device: CreateDevice failed 0x%08lX\n", (unsigned long)hr); return false; }
    *d3dOut = d3d; *wndOut = wnd;
    return true;
}

static void DeviceSection() {
    IDirect3DDevice9* dev = nullptr; IDirect3D9* d3d = nullptr; HWND wnd = nullptr;
    if (!CreateDevice(&dev, &d3d, &wnd)) { CHECK(false, "no D3D9 device"); return; }
    D3DCAPS9 caps{}; dev->GetDeviceCaps(&caps);
    std::printf("  device caps: VS 0x%04lX, PS 0x%04lX, MaxVertexShaderConst %lu, VS20 flow depth %lu\n", caps.VertexShaderVersion & 0xFFFF, caps.PixelShaderVersion & 0xFFFF, caps.MaxVertexShaderConst, caps.VS20Caps.DynamicFlowControlDepth);
    rw::d3d::d3ddevice = dev;
    SetAssembleHook(nullptr);
    Init();
    std::printf("  Init(): model 0x%X, maxConst %u\n", Model(), MaxConstants());
    CHECK(Model() != 0, "no shader model from the device caps");
    const std::vector<uint32_t> keys = RealisticKeys(400, 4242, MaxConstants());
    const int models[3] = { kModelVS2x, kModelVS20, kModelVS11 };
    for (int model : models) {
        Shutdown();
        SetModel(model, MaxConstants() ? MaxConstants() : 0x100);
        long okc = 0, failc = 0, shown = 0;
        for (uint32_t kv : keys) {
            const Key k = MakeKey(kv);
            Layout lay;
            IDirect3DVertexShader9* s = GetVertexShader(k, lay);
            if (s) ++okc; else {
                ++failc;
                if (shown++ < 2) { Layout l2; const std::string t = ComposeText(k, l2, model, MaxConstants()); std::printf("  model 0x%X key %08X rejected; text:\n%s\n", model, kv, t.c_str()); }
            }
        }
        std::printf("  device model=0x%02X: %zu keys, accepted %ld, rejected %ld\n", model, keys.size(), okc, failc);
        CHECK(failc == 0, "device rejected %ld shaders (model 0x%X)", failc, model);
    }
    Shutdown();
    rw::d3d::d3ddevice = nullptr;
    dev->Release(); d3d->Release(); DestroyWindow(wnd);
}

static uint8_t g_origAsm[5];

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int randomKeys = 200000;
    bool device = true, onlyDevice = false, onlyAsm = false;
    unsigned dumpKey = 0; bool dump = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-n") && i + 1 < argc) randomKeys = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-q")) g_quiet = true;
        else if (!std::strcmp(argv[i], "--no-device")) device = false;
        else if (!std::strcmp(argv[i], "--only-device")) onlyDevice = true;
        else if (!std::strcmp(argv[i], "--only-asm")) { onlyAsm = true; device = false; }
        else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) { dump = true; dumpKey = (unsigned)std::strtoul(argv[++i], nullptr, 16); }
    }
    const char* exePath = std::getenv("RW_EXE_ORACLE");
    const bool haveOracle = !onlyDevice && exePath && oracle::Map(exePath);
    if (!haveOracle) std::printf("rw_skin_vs_test: exe oracle not available (set RW_EXE_ORACLE, link with the placeholder image): oracle sections skipped\n");
    if (haveOracle) {
        W(0xC97B24) = (uint32_t)(uintptr_t)g_engine;
        *reinterpret_cast<void**>(g_engine + 0xF0) = reinterpret_cast<void*>(&HostSprintf);
        std::memcpy(g_origAsm, reinterpret_cast<void*>(0x7652AE), 5);
        oracle::Patch(0x7652AE, reinterpret_cast<void*>(&FakeAssemble));
        oracle::Patch(0x7FAC60, reinterpret_cast<void*>(&FakeCreateVS));
        oracle::g_where = "composer";
        if (dump) {
            SetModelBoth(kModelVS2x, 0x100);
            uint8_t lay[8]; g_capText.clear();
            const Key k = MakeKey(dumpKey);
            ExeComposeFn()(k.b, lay);
            std::printf("exe text for key %08X:\n%s\nlayout:", dumpKey, g_capText.c_str());
            for (int i = 0; i < 8; ++i) std::printf(" %02X", lay[i]);
            Layout pl; const std::string pt = ComposeText(k, pl, kModelVS2x, 0x100);
            std::printf("\nport equal: %d\n", pt == g_capText);
            return 0;
        }
        if (!onlyAsm) {
        std::printf("[text differential]\n");
        g_asmFail = true;
        TextSection(kModelVS2x, 0x100, randomKeys, true);
        TextSection(kModelVS2x, 96, randomKeys / 4, false);
        TextSection(kModelVS20, 0x100, randomKeys / 2, false);
        TextSection(kModelVS11, 0x100, randomKeys / 2, false);
        TextSection(0, 0x100, 100, false);
        std::printf("[NumLightConstants]\n");
        NumConstSection(1500000);
        std::printf("[cache]\n");
        CacheSection(40, 20000, false, 1);
        CacheSection(300, 60000, false, 2);
        CacheSection(700, 60000, false, 3);
        CacheSection(300, 20000, true, 4);
        }
    }
    if (device) {
        std::printf("[device]\n");
        DeviceSection();
    }
    if (haveOracle && !dump) AssemblerSection(exePath, g_origAsm);
    std::printf("rw_skin_vs_test: %s (%d failure%s)\n", g_fail ? "FAILED" : "OK", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
