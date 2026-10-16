#pragma once
// TEMPORARY frame-time instrumentation (agent FPS). QPC scopes, no thread suspension. Log: fprof.log in the cwd, written only for frames > 50 ms.
#if defined(NOTSA_STANDALONE_RUN)
#include <windows.h>
#include <cstdio>
namespace fprof {
enum P { P_CGAMEPROC, P_AUDIO, P_STREAM_UPD, P_STREAM_LOADALL, P_CONVERT, P_CDSYNC, P_CDREAD, P_PRESENT, P_TXD, P_RASTER, P_COUNT };
inline const char* kNames[P_COUNT] = { "CGame::Process", "Audio.Service", "CStreaming::Update", "LoadAllRequested", "ConvertBufferToObject", "CdStreamSync", "CdStreamRead", "Present", "TxdStreamRead", "RasterUnlock" };
inline double Now() {
    static const double inv = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return 1000.0 / (double)f.QuadPart; }();
    LARGE_INTEGER q; QueryPerformanceCounter(&q); return (double)q.QuadPart * inv;
}
inline double g_acc[P_COUNT]; inline double g_lastTop = 0, g_idleStart = 0, g_idleDur = 0; inline unsigned g_frame = 0; inline FILE* g_f = nullptr;
struct Scope { int id; double t0; explicit Scope(int i) : id(i), t0(Now()) {} ~Scope() { g_acc[id] += Now() - t0; } };
inline void FrameTop() {
    const double t = Now();
    if (g_lastTop > 0) {
        const double period = t - g_lastTop;
        if (period > 50.0) {
            if (!g_f) g_f = fopen("fprof.log", "a");
            if (g_f) {
                SYSTEMTIME st; GetLocalTime(&st);
                double acc = 0; for (int i = 0; i < P_COUNT; i++) if (i != P_CONVERT && i != P_STREAM_LOADALL && i != P_CDSYNC && i != P_CDREAD && i != P_TXD && i != P_RASTER) acc += g_acc[i];
                fprintf(g_f, "%02d:%02d:%02d.%03d frame %u period %.1f ms: inIdle %.1f outsideIdle %.1f |", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, g_frame, period, g_idleDur, period - g_idleDur);
                for (int i = 0; i < P_COUNT; i++) fprintf(g_f, " %s=%.1f", kNames[i], g_acc[i]);
                fprintf(g_f, " | idle-minus(Process+Audio+Present)=%.1f\n", g_idleDur - g_acc[P_CGAMEPROC] - g_acc[P_AUDIO] - g_acc[P_PRESENT]);
                fflush(g_f);
            }
        }
    }
    for (auto& a : g_acc) a = 0;
    g_lastTop = t; g_idleStart = t; g_frame++;
}
struct IdleGuard { IdleGuard() { FrameTop(); } ~IdleGuard() { g_idleDur = Now() - g_idleStart; } };
}
#define FPROF(id) ::fprof::Scope _fps_##id(::fprof::id)
#define FPROF_IDLE() ::fprof::IdleGuard _fpg
#else
#define FPROF(id)
#define FPROF_IDLE()
#endif
