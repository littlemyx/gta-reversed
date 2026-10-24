/* REPLAY-DIFF probe v0: in-process frame tracer for the ORIGINAL gta_sa.exe (v1.0 US compact), loaded as a vorbisFile.dll proxy (the exe imports vorbisFile statically, so
 * DllMain runs before the exe's entry point; the 7 imports are forwarded to vorbisFile_real.dll, see rd_orig.def). Reads the game's globals at their original addresses.
 * Env: RD_OUT=<trace file>  RD_PIN=<bits> 1 = time-/pointer-derived srand() seeds replaced by a constant (GameInit, LoadSplashes, CEntity::CreateEffects, CPostEffects::Grain), 2 = CTimer clock (fn at 0x5617A0) = anchor + frame*RD_STEP ms,
 *      8 = RNG re-seeded to a constant at the first traced frame = the ARM POINT (the loading screen consumes a real-time dependent number of rand() calls),
 *      16 = frame counter (0xB7CB4C) jumps FORWARD to the next multiple of 0x100000 at the arm point (game logic uses fc % n; the menu length differs per run), the pinned clock anchor is shifted back by the same number of slots (continuous clock), 4 = psTimer (0x745270, RsTimer/OS_TimeMS) = 1000 + frame*RD_STEP once the frame counter is > 0     RD_STEP=<ms, default 16>   RD_MAXF=<frames, default 20000>
 * Hooks (whole-function replacement by jmp, never calling the original): CRT rand 0x821B1E / srand 0x821B11 (one global LCG like source/standalone/GameRand.h), CTimer fn 0x5617A0, psTimer 0x745270.
 * Tick = the CTimer::Update call of the timer function (return address in 0x561B10..0x561C10): one trace line per frame, taken at the start of CTimer::Update.
 * Line: "F fc tms rng ncalls px py pz hp peds vehs scrhash"   (floats as hex bit patterns);  "S fc ra arg" for every srand() call. */
#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned long long u64; typedef unsigned u32;
static FILE* g_f; static u32 g_pin, g_step = 16, g_maxf = 20000;
#define FC_GRANULE 0x100000u
static u32 g_rfrom, g_rlog, g_nf; /* RD_RFROM=<n>: R lines only from the n-th traced frame on */ /* pin bit 32 + RD_RLOG=<n>: "R fc ra" for every rand() call of the first n traced frames */
static volatile u32 g_seed = 1, g_calls; static int g_started; static u64 g_anchor; static int g_anchor_set;
#define U32(a) (*(volatile u32*)(size_t)(a))
static u64 realq(void){ LARGE_INTEGER q; QueryPerformanceCounter(&q); return (u64)q.QuadPart; }
#include "rd_audio_tab.h"
static u32 g_aseed = 0x5EED5EED, g_acalls; /* pin bit 128: separate RNG stream for rand() calls made from audio code (plan row 6); Lockstep.cpp does the same for the port */
static int in_audio(u32 ra){ int i, in = 0; for(i = 0; i < g_audio_tab_n; i++){ if(ra >= g_audio_tab[i]) in = !in; else break; } return in; }
static int __cdecl my_rand(void){ if((g_pin & 128) && g_started && in_audio((u32)(size_t)_ReturnAddress())){ if((g_pin & 32) && g_rlog && g_f && g_nf <= g_rlog && g_nf >= g_rfrom) fprintf(g_f, "R %u %08xA\n", U32(0xB7CB4C), (u32)(size_t)_ReturnAddress()); g_aseed = g_aseed * 214013u + 2531011u; g_acalls++; return (int)((g_aseed >> 16) & 0x7FFF); } if((g_pin & 32) && g_rlog && g_f && g_started && g_nf <= g_rlog && g_nf >= g_rfrom) fprintf(g_f, "R %u %08x\n", U32(0xB7CB4C), (u32)(size_t)_ReturnAddress()); g_seed = g_seed * 214013u + 2531011u; g_calls++; return (int)((g_seed >> 16) & 0x7FFF); }
static void __cdecl my_srand(u32 s){
  u32 a = (u32)(size_t)_ReturnAddress();
  /* time-derived seeds: GameInit 0x5BF3B0 (srand(RsTimer())), CLoadingScreen::LoadSplashes 0x5900B0 (srand(QPC)) */
  if((g_pin & 1) && ((a >= 0x5BF3B0 && a < 0x5BF3E0) || (a >= 0x5900B0 && a < 0x590300) || (a >= 0x5339F0 && a < 0x533A00) /* CEntity::CreateEffects: this + days */ || (a >= 0x7037C0 && a < 0x703900) /* CPostEffects::Grain: OS_TimeMS */)) s = 0x13572468;
  if(g_f && g_started) fprintf(g_f, "S %u %08x %08x\n", U32(0xB7CB4C), a, s);   /* fc, return address, seed AFTER the pin */
  g_seed = s;
}
static u32 fnv(u32 h, const void* p, int n){ const unsigned char* b = (const unsigned char*)p; while(n--){ h ^= *b++; h *= 16777619u; } return h; }
static void tick(void){
  u32 nf = g_nf; if(!g_f || nf >= g_maxf) return;
  if(!g_started){ __try { if(!U32(0xB7CD98)) return; } __except(1) { return; } g_started = 1; if(g_pin & 8) g_seed = 0x2468ACE1;
    if(g_pin & 16){ u32 fc0 = U32(0xB7CB4C), to = (fc0 / FC_GRANULE + 1) * FC_GRANULE; if(g_pin & 2) g_anchor -= (u64)(to - fc0) * g_step * U32(0xB7CB2C); U32(0xB7CB4C) = to; }
    g_calls = 0; g_aseed = 0x5EED5EED; g_acalls = 0; }   /* trace from the first frame with a player ped (game started); pin bit 8 re-seeds the RNG there */
  if(g_pin & 256) { /* pin bit 256: CdStream tick barrier (plan row 7), same as Lockstep.cpp: wait until no CdStream channel has a read queued or in flight (gCdStreams 0x8E3FFC, gStreamCount 0x8E4090, struct 0x30: +4 nSectorsToRead, +0xE bInUse) */
    DWORD t0 = GetTickCount();
    for(;;){ int busy = 0; __try { u32 st = U32(0x8E3FFC), cnt = U32(0x8E4090), k; if(U32(0x8E3FE4) && st) for(k = 0; k < cnt && k < 16; k++) if(U32(st + k*0x30 + 4) || *(unsigned char*)(size_t)(st + k*0x30 + 0xE)) busy = 1; } __except(1) { busy = 0; }
      if(!busy || GetTickCount() - t0 > 10000) break; Sleep(0); }
  }
  g_nf = ++nf;
  u32 fc = U32(0xB7CB4C), tms = U32(0xB7CB84), ped = 0, hp = 0, np = 0, nv = 0, sh = 2166136261u; float pos[3] = {0, 0, 0};
  __try {
    ped = U32(0xB7CD98);
    if(ped){ u32 m = U32(ped + 0x14); memcpy(pos, (void*)(size_t)(m ? m + 0x30 : ped + 4), 12); hp = U32(ped + 0x540); }
    u32 pp = U32(0xB74490), vp = U32(0xB74494);
    if(pp){ u32 fl = U32(pp + 4), n = U32(pp + 8); for(u32 i = 0; i < n; i++) if(!(*(unsigned char*)(size_t)(fl + i) & 0x80)) np++; }
    if(vp){ u32 fl = U32(vp + 4), n = U32(vp + 8); for(u32 i = 0; i < n; i++) if(!(*(unsigned char*)(size_t)(fl + i) & 0x80)) nv++; }
    sh = fnv(sh, (void*)0xA49960, 269000);
    { static const char* dp; static int dpi; if(!dpi){ dpi = 1; dp = getenv("RD_DUMPSCR"); } if(dp && nf <= 4){ char fn[512]; FILE* df; sprintf(fn, "%s.%u", dp, nf); if((df = fopen(fn, "wb"))){ fwrite((void*)0xA49960, 1, 269000, df); fclose(df); } } }   /* RD_DUMPSCR=<prefix>: raw script space at traced frames 1..4 (A-B frame-1 hash hunt) */
  } __except(1) { }
  if(g_pin & 64) { /* pin bit 64: "D fc ..." debugging probes, same values as source/standalone/Lockstep.cpp (ignored by rd_diff.py) */
    fprintf(g_f, "D %u run=%d cd=%d cars=%08x,%08x,%08x,%08x perc=%08x,%08x,%08x,%08x zt=%d tw=%d ti=%d pk=%d,%d,%d bc=%d,%d,%d\n", fc, (int)*(unsigned char*)0xB5F851, (int)*(signed char*)0x9690C0,
            U32(0xC0BC30), U32(0xC0BC34), U32(0xC0BC38), U32(0xC0BC3C), U32(0xC0BC4C), U32(0xC0BC50), U32(0xC0BC54), U32(0xC0BC58), (int)U32(0xC0BC6C), (int)U32(0xC0BC70), (int)U32(0xC0BC74), (int)*(short*)0x9788DA, (int)*(short*)(0x9788DA + 3 * 0x20), (int)*(short*)(0x9788DA + 0x23 * 0x20), (int)*(unsigned short*)0xBA8704, (int)*(unsigned short*)(0xBA8704 + 3 * 40), (int)*(unsigned short*)(0xBA8704 + 0x23 * 40));
  }
  fprintf(g_f, "F %u %u %08x %u %08x %08x %08x %u %u %u %08x\n", fc, tms, g_seed, g_calls, *(u32*)&pos[0], *(u32*)&pos[1], *(u32*)&pos[2], hp, np, nv, sh);
  g_calls = 0; if((nf & 63) == 0) fflush(g_f);
}
/* Pinned CTimer clock. The Idle() frame lock (app_game.cpp, 0x53E930) spins until 14 ms of GetCurrentTimeInCycles() passed, so a clock frozen inside a frame would hang:
 * the reads made by CTimer::Update / CTimer::Initialise (the "frame reads") return anchor + fc*step, every other read anchor + (fc+1)*step ("end of the frame's slot").
 * Consecutive frames therefore differ by exactly `step` (>= 14) in both kinds, and the very first Idle of a game session (g_LastTickTime = 0) sees 16 ms at fc = 0. */
static u64 __cdecl my_timer(void){
  u32 ra = (u32)(size_t)_ReturnAddress();
  int upd = ra >= 0x561B10 && ra < 0x561C10, ini = ra >= 0x5617E0 && ra < 0x561890;
  if(upd) tick();   /* may jump fc / shift the anchor (pin 16), so fc is read after it */
  u32 fc = U32(0xB7CB4C), div = U32(0xB7CB2C);
  if(!(g_pin & 2)) return realq();
  if(g_started) { /* anchor fixed from the arm point on */ } else if(fc) g_anchor_set = 0; else if(!g_anchor_set){ g_anchor = realq(); g_anchor_set = 1; }
  return g_anchor + (u64)(fc + ((upd || ini) ? 0 : 1)) * g_step * div;
}
static u32 __cdecl my_pstimer(void){
  u32 fc = U32(0xB7CB4C);
  if((g_pin & 4) && fc) return 1000 + fc * g_step;
  { LARGE_INTEGER q, f; QueryPerformanceCounter(&q); QueryPerformanceFrequency(&f); return (u32)(q.QuadPart * 1000 / f.QuadPart); }
}
/* forwarding stubs: the 7 vorbisFile imports of the exe go to vorbisFile_real.dll (a .def forwarder does not link under x86 here) */
static FARPROC g_fw[7];
static const char* const g_fwn[7] = {"ov_open_callbacks","ov_clear","ov_time_total","ov_time_tell","ov_read","ov_info","ov_time_seek"};
__declspec(naked) void __cdecl ov_open_callbacks(void){ __asm jmp dword ptr [g_fw+0] }
__declspec(naked) void __cdecl ov_clear(void){ __asm jmp dword ptr [g_fw+4] }
__declspec(naked) void __cdecl ov_time_total(void){ __asm jmp dword ptr [g_fw+8] }
__declspec(naked) void __cdecl ov_time_tell(void){ __asm jmp dword ptr [g_fw+12] }
__declspec(naked) void __cdecl ov_read(void){ __asm jmp dword ptr [g_fw+16] }
__declspec(naked) void __cdecl ov_info(void){ __asm jmp dword ptr [g_fw+20] }
__declspec(naked) void __cdecl ov_time_seek(void){ __asm jmp dword ptr [g_fw+24] }
static void patch(u32 at, void* to){ DWORD o; unsigned char* p = (unsigned char*)(size_t)at; VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &o); p[0] = 0xE9; *(int*)(p + 1) = (int)((size_t)to - (at + 5)); VirtualProtect(p, 5, o, &o); }
BOOL WINAPI DllMain(HINSTANCE h, DWORD why, LPVOID r){
  if(why == DLL_PROCESS_ATTACH){
    const char* e; HMODULE rv = LoadLibraryA("vorbisFile_real.dll"); int i; for(i = 0; i < 7; i++) g_fw[i] = rv ? GetProcAddress(rv, g_fwn[i]) : 0;
    if((e = getenv("RD_PIN"))) g_pin = strtoul(e, 0, 0);
    if((e = getenv("RD_STEP"))) g_step = atoi(e);
    if((e = getenv("RD_MAXF"))) g_maxf = atoi(e);
    if((e = getenv("RD_RLOG"))) g_rlog = atoi(e);
    if((e = getenv("RD_RFROM"))) g_rfrom = atoi(e);
    if((e = getenv("RD_OUT"))) g_f = fopen(e, "w");
    if(GetModuleHandleA(0) != (HMODULE)0x400000) return TRUE; /* not the original image */
    patch(0x821B1E, my_rand); patch(0x821B11, my_srand); patch(0x5617A0, my_timer); patch(0x745270, my_pstimer);
    if(g_pin & 512){ DWORD o; unsigned char* p = (unsigned char*)0x507750; VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &o); p[0] = 0xC3; VirtualProtect(p, 1, o, &o); } /* pin bit 512: CAudioEngine::Service() (0x507750, thiscall, no stack args) = ret, as Lockstep.cpp NoAudio() in the port (= NOTSA_RR_NOAUDIO of the RR harness) */
  } else if(why == DLL_PROCESS_DETACH && g_f) fflush(g_f);
  return TRUE;
}
