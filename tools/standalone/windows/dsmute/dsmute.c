/* WIN-PERF dsound proxy that keeps the ORIGINAL exe silent (it has no mute switch; ours has NOTSA_STANDALONE_MUTE). Same idea as source/game_sa/Audio/Hardware/AEDirectSoundMute.h:
 * every DirectSound buffer gets SetVolume(DSBVOLUME_MIN) forever, GetVolume returns the volume the game asked for (the game's fade code reads it back).
 * Exports DirectSoundCreate8 (ordinal 11, what gta_sa.exe imports) and DirectSoundCreate (ordinal 1). Drop the built dsound.dll next to gta_sa.exe.
 * Build: tools/standalone/windows/dsmute/build.sh. Verified silent by the 'muted N SetVolume calls' count in dsmute.log (cwd) - there is no way to hear it by design. */
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
typedef HRESULT (WINAPI *PFN_CREATE8)(LPCGUID, LPDIRECTSOUND8*, LPUNKNOWN);
typedef HRESULT (WINAPI *PFN_CREATE)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
static HMODULE g_real; static CRITICAL_SECTION g_cs; static int g_init;
static HRESULT (STDMETHODCALLTYPE *oCreateBuf)(IDirectSound*, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN);
static HRESULT (STDMETHODCALLTYPE *oDupBuf)(IDirectSound*, LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER*);
static HRESULT (STDMETHODCALLTYPE *oSetVol)(IDirectSoundBuffer*, LONG);
static HRESULT (STDMETHODCALLTYPE *oSetVolB)(IDirectSoundBuffer*, LONG); /* second vtable, when IDirectSoundBuffer8 has its own */
static void** g_vt1, **g_vt2;
static struct { void* p; LONG v; } g_map[8192]; static unsigned g_nset, g_nbuf; static LONG g_max = DSBVOLUME_MIN;
static void* hook(void** vt, int i, void* fn) { DWORD old; void* o = vt[i]; VirtualProtect(&vt[i], 4, PAGE_EXECUTE_READWRITE, &old); vt[i] = fn; VirtualProtect(&vt[i], 4, old, &old); return o; }
static void remember(void* p, LONG v) {
  unsigned h = ((unsigned)(ULONG_PTR)p >> 4) & 8191;
  EnterCriticalSection(&g_cs);
  for (unsigned n = 0; n < 8192; n++, h = (h + 1) & 8191) { if (g_map[h].p == p || !g_map[h].p) { g_map[h].p = p; g_map[h].v = v; break; } }
  g_nset++; if (v > g_max) g_max = v;
  LeaveCriticalSection(&g_cs);
}
static HRESULT STDMETHODCALLTYPE mSetVol(IDirectSoundBuffer* b, LONG v) {
  HRESULT (STDMETHODCALLTYPE *real)(IDirectSoundBuffer*, LONG) = (*(void***)b == g_vt2 && g_vt2 != g_vt1) ? oSetVolB : oSetVol;
  if (v > DSBVOLUME_MAX || v < DSBVOLUME_MIN) return DSERR_INVALIDPARAM;
  remember(b, v); return real(b, DSBVOLUME_MIN);
}
static HRESULT STDMETHODCALLTYPE mGetVol(IDirectSoundBuffer* b, LPLONG out) {
  unsigned h = ((unsigned)(ULONG_PTR)b >> 4) & 8191; LONG v = 0;
  EnterCriticalSection(&g_cs);
  for (unsigned n = 0; n < 8192; n++, h = (h + 1) & 8191) { if (g_map[h].p == b) { v = g_map[h].v; break; } if (!g_map[h].p) break; }
  LeaveCriticalSection(&g_cs);
  *out = v; return DS_OK;
}
static void patchBuf(IDirectSoundBuffer* b) {
  void** vt = *(void***)b; g_nbuf++;
  if (vt != g_vt1 && vt != g_vt2) {
    if (!g_vt1) { g_vt1 = vt; oSetVol = hook(vt, 15, mSetVol); hook(vt, 6, mGetVol); }
    else if (!g_vt2) { g_vt2 = vt; oSetVolB = hook(vt, 15, mSetVol); hook(vt, 6, mGetVol); }
  }
  { void* b8 = 0; if (SUCCEEDED(IDirectSoundBuffer_QueryInterface(b, &IID_IDirectSoundBuffer8, &b8)) && b8) { void** v8 = *(void***)b8; if (v8 != g_vt1 && v8 != g_vt2 && !g_vt2) { g_vt2 = v8; oSetVolB = hook(v8, 15, mSetVol); hook(v8, 6, mGetVol); } IDirectSoundBuffer_Release((IDirectSoundBuffer*)b8); } }
  { HRESULT (STDMETHODCALLTYPE *real)(IDirectSoundBuffer*, LONG) = (*(void***)b == g_vt2 && g_vt2 != g_vt1) ? oSetVolB : oSetVol; real(b, DSBVOLUME_MIN); }
}
static HRESULT STDMETHODCALLTYPE hCreateBuf(IDirectSound* d, LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER* out, LPUNKNOWN u) {
  HRESULT hr = oCreateBuf(d, desc, out, u); if (SUCCEEDED(hr) && out && *out) patchBuf(*out); return hr;
}
static HRESULT STDMETHODCALLTYPE hDupBuf(IDirectSound* d, LPDIRECTSOUNDBUFFER src, LPDIRECTSOUNDBUFFER* out) {
  HRESULT hr = oDupBuf(d, src, out); if (SUCCEEDED(hr) && out && *out) patchBuf(*out); return hr;
}
static void attach(IDirectSound* ds) {
  if (oCreateBuf) return; void** vt = *(void***)ds; oCreateBuf = hook(vt, 3, hCreateBuf); oDupBuf = hook(vt, 5, hDupBuf);
}
static void load(void) {
  if (g_real) return; InitializeCriticalSection(&g_cs); char p[MAX_PATH]; GetSystemDirectoryA(p, MAX_PATH); lstrcatA(p, "\\dsound.dll"); g_real = LoadLibraryA(p);
}
__declspec(dllexport) HRESULT WINAPI MyCreate8(LPCGUID g, LPDIRECTSOUND8* out, LPUNKNOWN u) {
  load(); PFN_CREATE8 f = (PFN_CREATE8)GetProcAddress(g_real, "DirectSoundCreate8"); HRESULT hr = f(g, out, u);
  if (SUCCEEDED(hr) && out && *out) attach((IDirectSound*)*out); return hr;
}
__declspec(dllexport) HRESULT WINAPI MyCreate(LPCGUID g, LPDIRECTSOUND* out, LPUNKNOWN u) {
  load(); PFN_CREATE f = (PFN_CREATE)GetProcAddress(g_real, "DirectSoundCreate"); HRESULT hr = f(g, out, u);
  if (SUCCEEDED(hr) && out && *out) attach(*out); return hr;
}
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p) {
  if (r == DLL_PROCESS_DETACH && g_nbuf) { FILE* f = fopen("dsmute.log", "w"); if (f) { fprintf(f, "dsmute: %u buffers patched, %u SetVolume calls muted, loudest requested %d (1/100 dB)\n", g_nbuf, g_nset, (int)g_max); fclose(f); } }
  return TRUE;
}
