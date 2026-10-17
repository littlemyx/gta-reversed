/* WIN-PERF: pure D3D9 Present-rate probe (clear + present, no game). usage: present_probe w|f WIDTH HEIGHT imm|one|def [refresh_hz (0=default)] [seconds=2] [fmt: x8r8g8b8|a8r8g8b8|a2r10g10b10|r5g6b5]
 * Prints frames, mean/p50/max ms between Presents. Takes the GPU lock yourself; fullscreen variants change the desktop mode for a moment. */
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int cmpd(const void*a,const void*b){ double x=*(double*)a,y=*(double*)b; return x<y?-1:x>y; }
int main(int argc,char**argv){
  if(argc<5){ puts("usage: present_probe w|f W H imm|one|def [hz] [sec] [fmt]"); return 2; }
  int full=argv[1][0]=='f', W=atoi(argv[2]), H=atoi(argv[3]); const char*iv=argv[4]; int hz=argc>5?atoi(argv[5]):0; double sec=argc>6?atof(argv[6]):2.0;
  D3DFORMAT fmt=(argc>7&&!strcmp(argv[7],"r5g6b5"))?D3DFMT_R5G6B5:(argc>7&&!strcmp(argv[7],"a2r10g10b10"))?D3DFMT_A2R10G10B10:(argc>7&&!strcmp(argv[7],"a8r8g8b8"))?D3DFMT_A8R8G8B8:D3DFMT_X8R8G8B8;
  WNDCLASSA wc={0}; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="pp"; RegisterClassA(&wc);
  HWND w=CreateWindowA("pp","pp",full?WS_POPUP|WS_VISIBLE:WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,W,H,0,0,wc.hInstance,0); ShowWindow(w,SW_SHOW);
  IDirect3D9*d=Direct3DCreate9(D3D_SDK_VERSION); D3DPRESENT_PARAMETERS pp={0};
  pp.BackBufferWidth=W; pp.BackBufferHeight=H; pp.BackBufferFormat=fmt; pp.BackBufferCount=1; pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow=w; pp.Windowed=!full;
  pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D24S8; pp.FullScreen_RefreshRateInHz=full?hz:0;
  pp.PresentationInterval=!strcmp(iv,"imm")?D3DPRESENT_INTERVAL_IMMEDIATE:!strcmp(iv,"one")?D3DPRESENT_INTERVAL_ONE:D3DPRESENT_INTERVAL_DEFAULT;
  IDirect3DDevice9*dev; HRESULT hr=IDirect3D9_CreateDevice(d,0,D3DDEVTYPE_HAL,w,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&dev);
  if(FAILED(hr)){ printf("%s %dx%d %s hz=%d: CreateDevice failed 0x%08lx\n",argv[1],W,H,iv,hz,hr); return 1; }
  D3DDISPLAYMODE dm; IDirect3DDevice9_GetDisplayMode(dev,0,&dm);
  LARGE_INTEGER fq,a,b; QueryPerformanceFrequency(&fq); static double ft[100000]; int n=0; QueryPerformanceCounter(&a); LARGE_INTEGER last=a;
  for(;;){ MSG m; while(PeekMessageA(&m,0,0,0,PM_REMOVE)){ TranslateMessage(&m); DispatchMessageA(&m); }
    IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,D3DCOLOR_XRGB(n&255,0,0),1.f,0); IDirect3DDevice9_BeginScene(dev); IDirect3DDevice9_EndScene(dev);
    IDirect3DDevice9_Present(dev,0,0,0,0); QueryPerformanceCounter(&b); if(n<100000) ft[n++]=(b.QuadPart-last.QuadPart)*1000.0/fq.QuadPart; last=b;
    if((b.QuadPart-a.QuadPart)>sec*fq.QuadPart) break; }
  double mean=0; for(int i=1;i<n;i++) mean+=ft[i]; mean/=(n>1?n-1:1); qsort(ft+1,n-1,sizeof(double),cmpd);
  printf("%s %dx%d %s req_hz=%d fmt=%s -> display mode %ux%u@%uHz: n=%d mean=%.2f ms (%.1f fps) p50=%.2f max=%.2f\n",argv[1],W,H,iv,hz,fmt==D3DFMT_R5G6B5?"r5g6b5":fmt==D3DFMT_A2R10G10B10?"a2r10g10b10":fmt==D3DFMT_A8R8G8B8?"a8r8g8b8":"x8r8g8b8",dm.Width,dm.Height,dm.RefreshRate,n,mean,1000/mean,ft[n/2],ft[n-1]);
  IDirect3DDevice9_Release(dev); IDirect3D9_Release(d); DestroyWindow(w); return 0; }
