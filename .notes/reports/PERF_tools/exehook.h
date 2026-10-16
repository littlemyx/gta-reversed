// exehook.h: QPC inclusive/exclusive timers on functions of the ORIGINAL exe (agent PERF). Included by d3d9px.c; hooks the entries listed in g_hk.
// Integer-only; the main thread's x87 stack/flags/regs are preserved around the timer code.
typedef struct { DWORD addr; const char* name; int nbytes; BYTE* tramp; long long incl, excl; unsigned long long calls; } HK;
static HK g_hk[] = {
 {0x53BEE0,"CGame::Process",5},{0x40E670,"CStreaming::Update",5},{0x5684A0,"CWorld::Process",7},{0x5556E0,"CRenderer::ConstructRenderList",6},
 {0x553910,"CRenderer::PreRender",5},{0x53DF40,"RenderScene",6},{0x53E230,"Render2dStuff",5},{0x706AB0,"CRealTimeShadowManager::Update",5},
 {0x4185C0,"CCollision::ProcessColModels",5},{0x5DCFA0,"CPlantMgr::Update",9},{0x507750,"CAudioEngine::Service",9},{0x46A000,"CTheScripts::Process",5},
 {0x6FAEC0,"CCoronas::Render",6},{0x7046E0,"CPostEffects::Render",5},{0x553A10,"CRenderer::RenderRoads",5},{0x553AA0,"CRenderer::RenderEverythingBarRoads",6},
 {0x41B000,"CCollision::CameraConeCastVsWorldCollision",6},{0x41AC40,"CCollision::BuildCacheOfCameraCollision",8},{0x41A820,"CCollision::CheckCameraCollisionBuildings",6},{0x419F00,"CCollision::SphereCastVsEntity",7},{0x52B730,"CCamera::Process",6},{0x6FADF0,"CCoronas::Update",6},{0x72CAE0,"CWorldScan::ScanWorld",6},{0x5534B0,"CRenderer::AddEntityToRenderList",5},{0x553770,"CRenderer::ProcessLodRenderLists",5},{0x7201C0,"COcclusion::ProcessBeforeRendering",6},{0x72D9F0,"CCullZones::FindTunnelAttributesForCoors",8},{0x70C950,"CShadows::UpdatePermanentShadows",5},{0x711D90,"CStencilShadows::Process",5},{0x4D9870,"CAEAudioHardware::Service",5},{0x417730,"CCollision::TestLineOfSight",8},{0x554230,"CRenderer::SetupEntityVisibility",8},
 {0x4165B0,"CCollision::TestSphereTriangle",6},{0x5E8CD0,"CPed::ProcessControl",5},{0x60EA90,"CPlayerPed::ProcessControl",5},{0x6B1880,"CAutomobile::ProcessControl",6},{0x5485E0,"CPhysical::ProcessControl",6},{0x54DFB0,"CPhysical::ProcessCollision",8},{0x4341C0,"CCarCtrl::GenerateRandomCars",5},{0x72B850,"CWeather::Update",5},
 {0x561760,"CTimeCycle::Update",5},{0x6997E0,"CCover::Update",6},{0x541DD0,"CPad::UpdatePads",7},{0x6EF650,"CWaterLevel::RenderWater",5},{0x417950,"CCollision::ProcessLineOfSight",5},
 {0x417BF0,"CCollision::ProcessVerticalLine",7},{0x563430,"CWorld::ProcessPedsAfterPreRender",5},{0x5DAD00,"CGrassRenderer::DrawTriPlants",7},{0x70A960,"CShadows::RenderStoredShadows",6},
 {0x727140,"CMirrors::BeforeMainRender",6},
};
#define NHK (int)(sizeof(g_hk)/sizeof(g_hk[0]))
typedef struct { int idx; DWORD ret; long long t0, child; } FR;
static DWORD g_tr[64]; static FR g_stk[512]; static int g_sp=0; static DWORD g_mainTid=0; static long long g_hq0=0;
static LARGE_INTEGER g_hqf; static long long g_idleI=0, g_idleN=0, g_lastP=0;
static void exit_thunk(void);
static long long HNow(void){ LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart; }
static DWORD __cdecl enter_c(int i, DWORD ret){
  if(GetCurrentThreadId()!=g_mainTid || g_sp>=500) return 0;
  FR* f=&g_stk[g_sp++]; f->idx=i; f->ret=ret; f->child=0; f->t0=HNow(); return (DWORD)exit_thunk;
}
static DWORD __cdecl leave_c(void){
  long long t=HNow(); FR* f=&g_stk[--g_sp]; long long d=t-f->t0; HK* h=&g_hk[f->idx]; h->incl+=d; h->excl+=d-f->child; h->calls++; if(g_sp) g_stk[g_sp-1].child+=d; return f->ret;
}
static __declspec(naked) void exit_thunk(void){
  __asm {
    push 0
    pushfd
    push eax
    push edx
    push ecx
    call leave_c
    mov [esp+0x10], eax
    pop ecx
    pop edx
    pop eax
    popfd
    ret
  }
}
static __declspec(naked) void common_entry(void){
  __asm {
    pushfd
    push eax
    push ecx
    push edx
    mov eax,[esp+0x10]      ; hook index
    mov edx,[esp+0x14]      ; return address
    push edx
    push eax
    call enter_c
    add esp,8
    test eax,eax
    jz skip
    mov [esp+0x14], eax
  skip:
    mov eax,[esp+0x10]      ; index -> trampoline address
    mov eax, dword ptr g_tr[eax*4]
    mov [esp+0x10], eax
    pop edx
    pop ecx
    pop eax
    popfd
    ret
  }
}
static long long g_hw0=0,g_hqq0=0;
static void PatchExe(void){
  g_mainTid=GetCurrentThreadId(); { FILETIME ft; GetSystemTimeAsFileTime(&ft); g_hw0=(long long)(((ULONGLONG)ft.dwHighDateTime<<32)|ft.dwLowDateTime)/10000; g_hqq0=HNow(); QueryPerformanceFrequency(&g_hqf); }
  for(int i=0;i<NHK;i++){
    HK* h=&g_hk[i]; if(h->nbytes<5) continue;
    BYTE* code=(BYTE*)h->addr;
    BYTE* mem=(BYTE*)VirtualAlloc(0,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    // entry stub at mem[0..): push i ; jmp common_entry ; trampoline at mem+16: saved bytes ; jmp back
    mem[0]=0x68; *(DWORD*)(mem+1)=i; mem[5]=0xE9; *(DWORD*)(mem+6)=(DWORD)common_entry-(DWORD)(mem+10);
    BYTE* tr=mem+16; memcpy(tr,code,h->nbytes); tr[h->nbytes]=0xE9; *(DWORD*)(tr+h->nbytes+1)=(DWORD)(code+h->nbytes)-(DWORD)(tr+h->nbytes+5);
    h->tramp=tr; g_tr[i]=(DWORD)tr;
    DWORD old; VirtualProtect(code,16,PAGE_EXECUTE_READWRITE,&old);
    code[0]=0xE9; *(DWORD*)(code+1)=(DWORD)mem-(DWORD)(code+5); for(int k=5;k<h->nbytes;k++) code[k]=0x90;
    VirtualProtect(code,16,old,&old);
  }
}
static FILE* g_hf=0; static long long g_hlastDump=0; static unsigned long long g_hframes=0;
static void HookFrame(void){ // called at every Present (main thread)
  if(!g_mainTid) return; long long t=HNow();
  if(g_lastP){ g_idleI+=t-g_lastP; g_idleN++; } g_lastP=t; g_hframes++;
  if(t-g_hlastDump>g_hqf.QuadPart){ g_hlastDump=t;
    if(!g_hf) g_hf=fopen("Z:\\tmp\\perf\\out\\exe_perf_cur.log","a"); if(!g_hf) return;
    fprintf(g_hf,"T %lld %llu\n", g_hw0+(t-g_hqq0)*1000/g_hqf.QuadPart, g_hframes);
    fprintf(g_hf,"S Present:0 Idle %lld %lld %llu\n", g_idleI*1000000/g_hqf.QuadPart, g_idleI*1000000/g_hqf.QuadPart, g_idleN);
    for(int i=0;i<NHK;i++) if(g_hk[i].calls) fprintf(g_hf,"S exe:%x %s %lld %lld %llu\n", g_hk[i].addr, g_hk[i].name, g_hk[i].incl*1000000/g_hqf.QuadPart, g_hk[i].excl*1000000/g_hqf.QuadPart, g_hk[i].calls);
    fflush(g_hf);
  }
}
