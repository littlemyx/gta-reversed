#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
typedef IDirect3D9* (WINAPI *PFN_CREATE)(UINT);
static HMODULE real; static PFN_CREATE realCreate;
static HRESULT (STDMETHODCALLTYPE *origCreateDevice)(IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
static HRESULT (STDMETHODCALLTYPE *origPresent)(IDirect3DDevice9*,const RECT*,const RECT*,HWND,const RGNDATA*);
typedef struct { double ts; float b[8]; unsigned nd, pad; } REC;
enum { B_PRESENT, B_CLEAR, B_DRAW, B_CREATE, B_LOCK, B_UPDATE, B_NB };
static REC g_buf[128]; static int g_bn=0; static LARGE_INTEGER g_fq,g_q0; static double g_wall0=0; static FILE* g_ft=0; static double g_acc[8]; static unsigned g_nd=0; static double g_inv=0;
static double WallMs(void){ FILETIME ft; GetSystemTimeAsFileTime(&ft); return ((double)(((ULONGLONG)ft.dwHighDateTime<<32)|ft.dwLowDateTime))/10000.0; }
static double Now(void){ LARGE_INTEGER q; QueryPerformanceCounter(&q); if(!g_wall0){ QueryPerformanceFrequency(&g_fq); g_q0=q; g_wall0=WallMs(); g_inv=1000.0/(double)g_fq.QuadPart; } return g_wall0+(double)(q.QuadPart-g_q0.QuadPart)*g_inv; }
static void logPresent(void){
  REC* r=&g_buf[g_bn++]; r->ts=Now(); for(int i=0;i<8;i++){ r->b[i]=(float)g_acc[i]; g_acc[i]=0; } r->nd=g_nd; g_nd=0;
  if(g_bn==128){ if(!g_ft) g_ft=fopen("Z:\\tmp\\d3f-out\\present_cur.bin","ab"); if(g_ft){ fwrite(g_buf,sizeof(REC),g_bn,g_ft); fflush(g_ft);} g_bn=0; }
}
static char g_name[128]; static int g_pending=0, g_idx=0; static unsigned g_count=0;
#define REQ "Z:\\tmp\\d3f-tool\\shot.req"
static void dump(IDirect3DDevice9* d){
  IDirect3DSurface9 *bb=0,*sys=0; D3DSURFACE_DESC desc; D3DLOCKED_RECT lr;
  if(FAILED(IDirect3DDevice9_GetBackBuffer(d,0,0,D3DBACKBUFFER_TYPE_MONO,&bb))) return;
  IDirect3DSurface9_GetDesc(bb,&desc);
  if(SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(d,desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&sys,0))){
    if(SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(d,bb,sys)) && SUCCEEDED(IDirect3DSurface9_LockRect(sys,&lr,0,D3DLOCK_READONLY))){
      int w=desc.Width,h=desc.Height; int rowb=(w*3+3)&~3; BITMAPFILEHEADER fh={0}; BITMAPINFOHEADER ih={0};
      fh.bfType=0x4D42; fh.bfOffBits=54; fh.bfSize=54+rowb*h; ih.biSize=40; ih.biWidth=w; ih.biHeight=h; ih.biPlanes=1; ih.biBitCount=24;
      char fn[256]; sprintf(fn,"Z:\\tmp\\d3f-shots\\%s_%d.bmp",g_name,g_idx++); FILE*f=fopen(fn,"wb");
      if(f){ fwrite(&fh,14,1,f); fwrite(&ih,40,1,f); BYTE* row=(BYTE*)malloc(rowb);
        for(int y=h-1;y>=0;y--){ BYTE* s=(BYTE*)lr.pBits+y*lr.Pitch; memset(row,0,rowb);
          for(int x=0;x<w;x++){ BYTE r,g,b; if(desc.Format==D3DFMT_R5G6B5){ WORD p=((WORD*)s)[x]; r=((p>>11)&31)*255/31; g=((p>>5)&63)*255/63; b=(p&31)*255/31; } else if(desc.Format==D3DFMT_X1R5G5B5||desc.Format==D3DFMT_A1R5G5B5){ WORD p=((WORD*)s)[x]; r=((p>>10)&31)*255/31; g=((p>>5)&31)*255/31; b=(p&31)*255/31; } else { b=s[x*4]; g=s[x*4+1]; r=s[x*4+2]; }
            row[x*3]=b; row[x*3+1]=g; row[x*3+2]=r; }
          fwrite(row,rowb,1,f); }
        free(row); fclose(f); }
      IDirect3DSurface9_UnlockRect(sys); }
    IDirect3DSurface9_Release(sys); }
  IDirect3DSurface9_Release(bb);
}

#define T0 double t0=Now()
#define T1(b) g_acc[b]+=Now()-t0
static void* hookvt(void** vt,int idx,void* fn){ DWORD old; void* o=vt[idx]; VirtualProtect(&vt[idx],4,PAGE_EXECUTE_READWRITE,&old); vt[idx]=fn; VirtualProtect(&vt[idx],4,old,&old); return o; }
typedef HRESULT (STDMETHODCALLTYPE *F_Clear)(IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD); static F_Clear oClear;
static HRESULT STDMETHODCALLTYPE hClear(IDirect3DDevice9* d,DWORD n,const D3DRECT* r,DWORD f,D3DCOLOR c,float z,DWORD s){ T0; HRESULT h=oClear(d,n,r,f,c,z,s); T1(B_CLEAR); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_DP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT); static F_DP oDP;
static HRESULT STDMETHODCALLTYPE hDP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT a,UINT b){ T0; HRESULT h=oDP(d,t,a,b); T1(B_DRAW); g_nd++; return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_DIP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT); static F_DIP oDIP;
static HRESULT STDMETHODCALLTYPE hDIP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,INT a,UINT b,UINT c,UINT e,UINT f){ T0; HRESULT h=oDIP(d,t,a,b,c,e,f); T1(B_DRAW); g_nd++; return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_DPUP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT); static F_DPUP oDPUP;
static HRESULT STDMETHODCALLTYPE hDPUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT a,const void* p,UINT s){ T0; HRESULT h=oDPUP(d,t,a,p,s); T1(B_DRAW); g_nd++; return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_DIPUP)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT); static F_DIPUP oDIPUP;
static HRESULT STDMETHODCALLTYPE hDIPUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE t,UINT a,UINT b,UINT c,const void* i,D3DFORMAT f,const void* v,UINT s){ T0; HRESULT h=oDIPUP(d,t,a,b,c,i,f,v,s); T1(B_DRAW); g_nd++; return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_LockRect)(IDirect3DTexture9*,UINT,D3DLOCKED_RECT*,const RECT*,DWORD); static F_LockRect oLockRect;
static HRESULT STDMETHODCALLTYPE hLockRect(IDirect3DTexture9* t,UINT l,D3DLOCKED_RECT* r,const RECT* q,DWORD f){ T0; HRESULT h=oLockRect(t,l,r,q,f); T1(B_LOCK); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_UnlockRect)(IDirect3DTexture9*,UINT); static F_UnlockRect oUnlockRect;
static HRESULT STDMETHODCALLTYPE hUnlockRect(IDirect3DTexture9* t,UINT l){ T0; HRESULT h=oUnlockRect(t,l); T1(B_LOCK); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_VBLock)(void*,UINT,UINT,void**,DWORD); static F_VBLock oVBLock;
static HRESULT STDMETHODCALLTYPE hVBLock(void* t,UINT o,UINT s,void** p,DWORD f){ T0; HRESULT h=oVBLock(t,o,s,p,f); T1(B_LOCK); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_IBLock)(void*,UINT,UINT,void**,DWORD); static F_IBLock oIBLock;
static HRESULT STDMETHODCALLTYPE hIBLock(void* t,UINT o,UINT s,void** p,DWORD f){ T0; HRESULT h=oIBLock(t,o,s,p,f); T1(B_LOCK); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_CTex)(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE); static F_CTex oCTex;
static HRESULT STDMETHODCALLTYPE hCTex(IDirect3DDevice9* d,UINT w,UINT h_,UINT l,DWORD u,D3DFORMAT f,D3DPOOL p,IDirect3DTexture9** o,HANDLE hh){ T0; HRESULT h=oCTex(d,w,h_,l,u,f,p,o,hh); T1(B_CREATE);
  if(SUCCEEDED(h)&&o&&*o&&!oLockRect){ void** vt=*(void***)*o; oLockRect=(F_LockRect)hookvt(vt,19,hLockRect); oUnlockRect=(F_UnlockRect)hookvt(vt,20,hUnlockRect); } return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_CVB)(IDirect3DDevice9*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9**,HANDLE); static F_CVB oCVB;
static HRESULT STDMETHODCALLTYPE hCVB(IDirect3DDevice9* d,UINT l,DWORD u,DWORD f,D3DPOOL p,IDirect3DVertexBuffer9** o,HANDLE hh){ T0; HRESULT h=oCVB(d,l,u,f,p,o,hh); T1(B_CREATE);
  if(SUCCEEDED(h)&&o&&*o&&!oVBLock){ void** vt=*(void***)*o; oVBLock=(F_VBLock)hookvt(vt,11,hVBLock); } return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_CIB)(IDirect3DDevice9*,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DIndexBuffer9**,HANDLE); static F_CIB oCIB;
static HRESULT STDMETHODCALLTYPE hCIB(IDirect3DDevice9* d,UINT l,DWORD u,D3DFORMAT f,D3DPOOL p,IDirect3DIndexBuffer9** o,HANDLE hh){ T0; HRESULT h=oCIB(d,l,u,f,p,o,hh); T1(B_CREATE);
  if(SUCCEEDED(h)&&o&&*o&&!oIBLock){ void** vt=*(void***)*o; oIBLock=(F_IBLock)hookvt(vt,11,hIBLock); } return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_CVS)(IDirect3DDevice9*,const DWORD*,IDirect3DVertexShader9**); static F_CVS oCVS;
static HRESULT STDMETHODCALLTYPE hCVS(IDirect3DDevice9* d,const DWORD* a,IDirect3DVertexShader9** o){ T0; HRESULT h=oCVS(d,a,o); T1(B_CREATE); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_CPS)(IDirect3DDevice9*,const DWORD*,IDirect3DPixelShader9**); static F_CPS oCPS;
static HRESULT STDMETHODCALLTYPE hCPS(IDirect3DDevice9* d,const DWORD* a,IDirect3DPixelShader9** o){ T0; HRESULT h=oCPS(d,a,o); T1(B_CREATE); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_UT)(IDirect3DDevice9*,IDirect3DBaseTexture9*,IDirect3DBaseTexture9*); static F_UT oUT;
static HRESULT STDMETHODCALLTYPE hUT(IDirect3DDevice9* d,IDirect3DBaseTexture9* a,IDirect3DBaseTexture9* b){ T0; HRESULT h=oUT(d,a,b); T1(B_UPDATE); return h; }
typedef HRESULT (STDMETHODCALLTYPE *F_SR)(IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const RECT*,D3DTEXTUREFILTERTYPE); static F_SR oSR;
static HRESULT STDMETHODCALLTYPE hSR(IDirect3DDevice9* d,IDirect3DSurface9* a,const RECT* b,IDirect3DSurface9* c,const RECT* e,D3DTEXTUREFILTERTYPE f){ T0; HRESULT h=oSR(d,a,b,c,e,f); T1(B_UPDATE); return h; }
static void hookDevice(IDirect3DDevice9* dev){
  void** vt=*(void***)dev;
  oCTex=(F_CTex)hookvt(vt,23,hCTex); oCVB=(F_CVB)hookvt(vt,26,hCVB); oCIB=(F_CIB)hookvt(vt,27,hCIB); oUT=(F_UT)hookvt(vt,31,hUT); oSR=(F_SR)hookvt(vt,34,hSR);
  oClear=(F_Clear)hookvt(vt,43,hClear); oDP=(F_DP)hookvt(vt,81,hDP); oDIP=(F_DIP)hookvt(vt,82,hDIP); oDPUP=(F_DPUP)hookvt(vt,83,hDPUP); oDIPUP=(F_DIPUP)hookvt(vt,84,hDIPUP);
  oCVS=(F_CVS)hookvt(vt,91,hCVS); oCPS=(F_CPS)hookvt(vt,106,hCPS);
}
static HRESULT STDMETHODCALLTYPE hkPresent(IDirect3DDevice9* d,const RECT*a,const RECT*b,HWND c,const RGNDATA*e){
  g_count++;
  if(!g_pending && (g_count%32)==0){ FILE*f=fopen(REQ,"r"); if(f){ char nm[128]; int n=0; if(fscanf(f,"%127s %d",nm,&n)==2){ strcpy(g_name,nm); g_pending=n; g_idx=0; } fclose(f); DeleteFileA(REQ); CreateDirectoryA("Z:\\tmp\\d3f-shots",0); } }
  if(g_pending>0){ dump(d); g_pending--; }
  { double t0=Now(); HRESULT hr=origPresent(d,a,b,c,e); g_acc[B_PRESENT]+=Now()-t0; logPresent(); return hr; }
}
static HRESULT STDMETHODCALLTYPE hkCreateDevice(IDirect3D9* t,UINT ad,D3DDEVTYPE ty,HWND w,DWORD fl,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice9** out){
  HRESULT hr=origCreateDevice(t,ad,ty,w,fl,pp,out);
  if(SUCCEEDED(hr)&&out&&*out&&!origPresent){ void** vt=*(void***)*out; DWORD old; VirtualProtect(&vt[17],4,PAGE_EXECUTE_READWRITE,&old); origPresent=(void*)vt[17]; vt[17]=hkPresent; VirtualProtect(&vt[17],4,old,&old); hookDevice(*out); }
  return hr;
}
__declspec(dllexport) IDirect3D9* WINAPI MyCreate9(UINT v){
  if(!real){ char p[MAX_PATH]; GetSystemDirectoryA(p,MAX_PATH); strcat(p,"\\d3d9.dll"); real=LoadLibraryA(p); realCreate=(PFN_CREATE)GetProcAddress(real,"Direct3DCreate9"); }
  IDirect3D9* d=realCreate(v);
  if(d&&!origCreateDevice){ void** vt=*(void***)d; DWORD old; VirtualProtect(&vt[16],4,PAGE_EXECUTE_READWRITE,&old); origCreateDevice=(void*)vt[16]; vt[16]=hkCreateDevice; VirtualProtect(&vt[16],4,old,&old); }
  return d;
}
