/* WIN-PERF d3d9 proxy: logs one record per IDirect3DDevice9::Present (QPC timestamp, time spent inside the real Present, Draw* calls since the last Present)
 * into the binary file named by env D3D9PX_OUT (default d3d9px.bin in the cwd). Record = struct REC { double t_ms; float present_ms; unsigned ndraw; } = 16 bytes.
 * t_ms is the ABSOLUTE QPC clock in ms (same clock as inputdrv.exe scene lines). The file is flushed every 16 records (the game is normally killed, not exited).
 * Works for the original exe and ours alike: drop d3d9.dll (this proxy) next to the exe; it loads the real one from System32.
 * Build: tools/standalone/windows/d3d9px/build.sh (cl x86). Analyse: tools/standalone/windows/frame_stats.py */
#include <windows.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef IDirect3D9* (WINAPI *PFN_CREATE)(UINT);
typedef struct { double t_ms; float present_ms; unsigned ndraw; } REC;
static HMODULE g_real; static PFN_CREATE g_create;
static HRESULT (STDMETHODCALLTYPE *oCreateDevice)(IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
static HRESULT (STDMETHODCALLTYPE *oPresent)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
static HRESULT (STDMETHODCALLTYPE *oDP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
static HRESULT (STDMETHODCALLTYPE *oDIP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
static HRESULT (STDMETHODCALLTYPE *oDPUP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
static HRESULT (STDMETHODCALLTYPE *oDIPUP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);
static LARGE_INTEGER g_fq, g_q0; static REC g_buf[16]; static int g_n; static FILE* g_f; static unsigned g_nd;
static double Now(void){ LARGE_INTEGER q; QueryPerformanceCounter(&q); return (double)q.QuadPart*1000.0/(double)g_fq.QuadPart; }
static void* hook(void** vt,int i,void* fn){ DWORD old; void* o=vt[i]; VirtualProtect(&vt[i],4,PAGE_EXECUTE_READWRITE,&old); vt[i]=fn; VirtualProtect(&vt[i],4,old,&old); return o; }
static HRESULT STDMETHODCALLTYPE hDP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT a,UINT b){ g_nd++; return oDP(d,t,a,b); }
static HRESULT STDMETHODCALLTYPE hDIP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,INT a,UINT b,UINT c,UINT e,UINT f){ g_nd++; return oDIP(d,t,a,b,c,e,f); }
static HRESULT STDMETHODCALLTYPE hDPUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT a,const void* p,UINT s){ g_nd++; return oDPUP(d,t,a,p,s); }
static HRESULT STDMETHODCALLTYPE hDIPUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT a,UINT b,UINT c,const void* i,D3DFORMAT f,const void* v,UINT s){ g_nd++; return oDIPUP(d,t,a,b,c,i,f,v,s); }
/* optional external frame cap (sleep, not busy-wait except the last 0.4 ms): the integer in the text file named by env D3D9PX_CAPFILE (fps, 0 = uncapped), re-read every 32 Presents.
 * The game's own limiter is tied to vsync here (psCameraShowRaster), and vsync is broken on some displays (12.7 Hz in exclusive mode), so frame-time runs use limiter OFF + this cap. */
static double g_capms, g_nextdl; static unsigned g_capn;
static void capwait(void){
  if((g_capn++&31)==0){ const char* p=getenv("D3D9PX_CAPFILE"); FILE* f=p?fopen(p,"r"):0; if(f){ int v=0; if(fscanf(f,"%d",&v)==1) g_capms=v>0?1000.0/v:0; fclose(f);} }
  if(g_capms<=0){ g_nextdl=0; return; }
  double t=Now(); if(g_nextdl==0||t>g_nextdl+g_capms*4) g_nextdl=t; g_nextdl+=g_capms;
  while((t=Now())<g_nextdl){ double r=g_nextdl-t; if(r>1.5) Sleep((DWORD)(r-1.0)); else if(r<0.4) YieldProcessor(); else Sleep(0); }
}

/* screenshot on request: the text file named by env D3D9PX_SHOTFILE (checked every 8 Presents) holds an output .bmp path; the back buffer is written there (24 bit) and the request file deleted */
static void shot(IDirect3DDevice9* d, const char* path){
  IDirect3DSurface9 *bb=0,*sys=0; D3DSURFACE_DESC dc; D3DLOCKED_RECT lr;
  if(FAILED(IDirect3DDevice9_GetBackBuffer(d,0,0,D3DBACKBUFFER_TYPE_MONO,&bb))) return;
  IDirect3DSurface9_GetDesc(bb,&dc);
  if(SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(d,dc.Width,dc.Height,dc.Format,D3DPOOL_SYSTEMMEM,&sys,0))){
    if(SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(d,bb,sys)) && SUCCEEDED(IDirect3DSurface9_LockRect(sys,&lr,0,D3DLOCK_READONLY))){
      int w=dc.Width,h=dc.Height,rb=(w*3+3)&~3; BITMAPFILEHEADER fh; BITMAPINFOHEADER ih; ZeroMemory(&fh,sizeof fh); ZeroMemory(&ih,sizeof ih);
      fh.bfType=0x4D42; fh.bfOffBits=54; fh.bfSize=54+rb*h; ih.biSize=40; ih.biWidth=w; ih.biHeight=h; ih.biPlanes=1; ih.biBitCount=24;
      FILE* f=fopen(path,"wb"); if(f){ BYTE* row=(BYTE*)malloc(rb); fwrite(&fh,14,1,f); fwrite(&ih,40,1,f);
        for(int y=h-1;y>=0;y--){ BYTE* sp=(BYTE*)lr.pBits+y*lr.Pitch; ZeroMemory(row,rb);
          for(int x=0;x<w;x++){ BYTE r,g,b; if(dc.Format==D3DFMT_R5G6B5){ WORD v=((WORD*)sp)[x]; r=((v>>11)&31)*255/31; g=((v>>5)&63)*255/63; b=(v&31)*255/31; } else { b=sp[x*4]; g=sp[x*4+1]; r=sp[x*4+2]; } row[x*3]=b; row[x*3+1]=g; row[x*3+2]=r; }
          fwrite(row,rb,1,f); }
        free(row); fclose(f); }
      IDirect3DSurface9_UnlockRect(sys); }
    IDirect3DSurface9_Release(sys); }
  IDirect3DSurface9_Release(bb);
}
static unsigned g_shn;
static void shotcheck(IDirect3DDevice9* d){
  if((g_shn++&7)!=0) return; const char* rf=getenv("D3D9PX_SHOTFILE"); if(!rf) return; FILE* f=fopen(rf,"r"); if(!f) return;
  char path[600]={0}; if(fgets(path,sizeof path,f)){ size_t n=strlen(path); while(n&&(path[n-1]=='\n'||path[n-1]=='\r')) path[--n]=0; } fclose(f); DeleteFileA(rf); if(path[0]) shot(d,path);
}
static HRESULT STDMETHODCALLTYPE hPresent(IDirect3DDevice9* d,const RECT*a,const RECT*b,HWND c,const RGNDATA*e){
  shotcheck(d);
  capwait();
  double t0=Now(); HRESULT hr=oPresent(d,a,b,c,e); double t1=Now();
  REC* r=&g_buf[g_n++]; r->t_ms=t1; r->present_ms=(float)(t1-t0); r->ndraw=g_nd; g_nd=0;
  if(g_n==16){ if(!g_f){ const char* p=getenv("D3D9PX_OUT"); g_f=fopen(p?p:"d3d9px.bin","wb"); } if(g_f){ fwrite(g_buf,sizeof(REC),16,g_f); fflush(g_f);} g_n=0; }
  return hr;
}
static HRESULT (STDMETHODCALLTYPE *oReset)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
static void logpp(const char* what, const D3DPRESENT_PARAMETERS* pp){
  char fn[600]; const char* p=getenv("D3D9PX_OUT"); wsprintfA(fn,"%s.txt",p?p:"d3d9px.bin"); FILE* f=fopen(fn,"a");
  if(f){ fprintf(f,"%.1f ms %s: %ux%u fmt=%d backbuffers=%u windowed=%d swap=%d hz=%u interval=0x%x depth=%d\n",Now(),what,pp->BackBufferWidth,pp->BackBufferHeight,(int)pp->BackBufferFormat,pp->BackBufferCount,pp->Windowed,(int)pp->SwapEffect,pp->FullScreen_RefreshRateInHz,pp->PresentationInterval,(int)pp->AutoDepthStencilFormat); fclose(f); }
}
/* env D3D9PX_NOVSYNC=1: the real CreateDevice/Reset get a COPY of the parameters with PresentationInterval = IMMEDIATE (the caller's struct, and so its own bookkeeping, stays untouched;
 * the driver's output fields are copied back). Measurement tool only: removes the display's vsync pacing (12.7 Hz in exclusive mode on the Windows host) from both exes alike. */
static int novsync(void){ static int v=-1; if(v<0) v=getenv("D3D9PX_NOVSYNC")?1:0; return v; }
static HRESULT STDMETHODCALLTYPE hReset(IDirect3DDevice9* d,D3DPRESENT_PARAMETERS* pp){
  logpp("Reset",pp); if(!novsync()) return oReset(d,pp);
  D3DPRESENT_PARAMETERS c=*pp; c.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE; HRESULT hr=oReset(d,&c); return hr; }
static HRESULT STDMETHODCALLTYPE hCreateDevice(IDirect3D9* t,UINT ad,D3DDEVTYPE ty,HWND w,DWORD fl,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice9** out){
  logpp("CreateDevice",pp);
  HRESULT hr; if(novsync()){ D3DPRESENT_PARAMETERS c=*pp; c.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE; hr=oCreateDevice(t,ad,ty,w,fl,&c,out); pp->BackBufferWidth=c.BackBufferWidth; pp->BackBufferHeight=c.BackBufferHeight; pp->BackBufferFormat=c.BackBufferFormat; pp->BackBufferCount=c.BackBufferCount; }
  else hr=oCreateDevice(t,ad,ty,w,fl,pp,out);
  if(SUCCEEDED(hr)&&out&&*out&&!oPresent){ void** vt=*(void***)*out;
    oPresent=hook(vt,17,hPresent); oReset=hook(vt,16,hReset); oDP=hook(vt,81,hDP); oDIP=hook(vt,82,hDIP); oDPUP=hook(vt,83,hDPUP); oDIPUP=hook(vt,84,hDIPUP); }
  return hr;
}
__declspec(dllexport) IDirect3D9* WINAPI MyCreate9(UINT v){
  if(!g_real){ timeBeginPeriod(1); char p[MAX_PATH]; GetSystemDirectoryA(p,MAX_PATH); lstrcatA(p,"\\d3d9.dll"); g_real=LoadLibraryA(p); g_create=(PFN_CREATE)GetProcAddress(g_real,"Direct3DCreate9");
    QueryPerformanceFrequency(&g_fq); QueryPerformanceCounter(&g_q0); }
  IDirect3D9* d=g_create(v);
  if(d&&!oCreateDevice){ void** vt=*(void***)d; oCreateDevice=hook(vt,16,hCreateDevice); }
  return d;
}
