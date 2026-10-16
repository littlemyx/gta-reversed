// sk2 <script> <exename> : SendInput driver + game memory probe (ReadProcessMemory, no thread suspension)
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
static struct { const char* n; WORD sc; int ext; } K[] = {
 {"return",0x1C,0},{"escape",1,0},{"space",0x39,0},{"lshift",0x2A,0},{"up",0x48,1},{"down",0x50,1},{"left",0x4B,1},{"right",0x4D,1},{"tab",0x0F,0},
 {"q",0x10,0},{"w",0x11,0},{"e",0x12,0},{"r",0x13,0},{"t",0x14,0},{"y",0x15,0},{"u",0x16,0},{"i",0x17,0},{"o",0x18,0},{"p",0x19,0},
 {"a",0x1E,0},{"s",0x1F,0},{"d",0x20,0},{"f",0x21,0},{"g",0x22,0},{"h",0x23,0},{"j",0x24,0},{"k",0x25,0},{"l",0x26,0},
 {"z",0x2C,0},{"x",0x2D,0},{"c",0x2E,0},{"v",0x2F,0},{"b",0x30,0},{"n",0x31,0},{"m",0x32,0},{0,0,0}};
static void ev(const char* n,int up){
  for(int i=0;K[i].n;i++) if(!strcmp(K[i].n,n)){
    INPUT in; memset(&in,0,sizeof in); in.type=INPUT_KEYBOARD; in.ki.wScan=K[i].sc;
    in.ki.dwFlags=KEYEVENTF_SCANCODE|(K[i].ext?KEYEVENTF_EXTENDEDKEY:0)|(up?KEYEVENTF_KEYUP:0);
    SendInput(1,&in,sizeof in); return; }
  printf("unknown key %s\n",n);
}
static HANDLE H;
static DWORD rd32(DWORD a){ DWORD v=0; SIZE_T n; if(!H||!ReadProcessMemory(H,(LPCVOID)a,&v,4,&n)) return 0; return v;}
static BYTE rd8(DWORD a){ BYTE v=0; SIZE_T n; if(!H||!ReadProcessMemory(H,(LPCVOID)a,&v,1,&n)) return 0; return v;}
static WORD rd16(DWORD a){ WORD v=0; SIZE_T n; if(!H||!ReadProcessMemory(H,(LPCVOID)a,&v,2,&n)) return 0xFFFF; return v;}
static DWORD ped(){ return rd32(0xB7CD98); }
static int invehicle(){ DWORD p=ped(); return p && (rd8(p+0x46D)&1) && rd32(p+0x58C); }
static int control(){ return ped() && !rd8(0xBA67A4) && !rd8(0xB5F851) && rd32(0xC8D4C0)==9 && rd16(0xB73564)==0; }
static void findproc(const char* exe){
  HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32 pe; pe.dwSize=sizeof pe;
  for(BOOL ok=Process32First(s,&pe);ok;ok=Process32Next(s,&pe)) if(!_stricmp(pe.szExeFile,exe)){ H=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pe.th32ProcessID); printf("pid %lu handle %p\n",pe.th32ProcessID,H); break; }
  CloseHandle(s);
}

static int epos(DWORD e,float*p){ DWORD m=rd32(e+0x14); SIZE_T n; if(m){ if(!ReadProcessMemory(H,(LPCVOID)(m+0x30),p,12,&n)) return 0; } else { if(!ReadProcessMemory(H,(LPCVOID)(e+4),p,12,&n)) return 0; } return 1; }
static void cs(){ char b[32]={0}; SIZE_T n; ReadProcessMemory(H,(LPCVOID)0x969110,b,30,&n); printf("  cheatstr='%s' fg=%d\n",b,0); }
static void vehs(const char* tag){ cs();
  float pp[3]={0,0,0}; DWORD pe=ped(); if(!pe||!epos(pe,pp)){ printf("[%s] no ped\n",tag); return; }
  DWORD pool=rd32(0xB74494); DWORD objs=rd32(pool), map=rd32(pool+4), size=rd32(pool+8); int used=0; printf("[%s] ped at %.1f %.1f %.1f; vehicle pool size %lu:",tag,pp[0],pp[1],pp[2],size);
  if(size>0&&size<1000){ for(DWORD i=0;i<size;i++){ BYTE fl=rd8(map+i); if(fl&0x80) continue; used++; DWORD v=objs+i*0xA18; float vp[3]; if(!epos(v,vp)) continue; float d=(float)sqrt((vp[0]-pp[0])*(vp[0]-pp[0])+(vp[1]-pp[1])*(vp[1]-pp[1])+(vp[2]-pp[2])*(vp[2]-pp[2])); if(d<30) printf(" [model %u d=%.1f]",(unsigned)((rd32(v+0x20)>>16)&0xFFFF),d); } }
  printf(" used=%d\n",used);
}
static double cpums(void){ FILETIME c,e,k,u; if(!H||!GetProcessTimes(H,&c,&e,&k,&u)) return -1; return (((double)(((ULONGLONG)k.dwHighDateTime<<32)|k.dwLowDateTime))+((double)(((ULONGLONG)u.dwHighDateTime<<32)|u.dwLowDateTime)))/10000.0; }
static double wallms(void){ FILETIME ft; GetSystemTimeAsFileTime(&ft); return ((double)(((ULONGLONG)ft.dwHighDateTime<<32)|ft.dwLowDateTime))/10000.0; }
static void status(const char* tag){ printf("[wall %s] %.1f\n",tag,wallms());
  printf("[%s] tick=%lu gstate=%lu menu=%u cut=%u fc=%lu ped=%08lx inveh=%d padoff=%04x\n",tag,GetTickCount(),rd32(0xC8D4C0),rd8(0xBA67A4),rd8(0xB5F851),rd32(0xB7CB4C),ped(),invehicle(),rd16(0xB73564));
}
int main(int argc,char**argv){
  findproc(argv[2]);
  static char* tok[4000]; int nt=0; FILE* f=fopen(argv[1],"r"); static char line[512];
  while(fgets(line,512,f)){ char* p=strchr(line,'#'); if(p)*p=0; for(char*q=strtok(line,";\r\n");q;q=strtok(0,";\r\n")){ while(*q==' ')q++; if(*q) tok[nt++]=_strdup(q);} }
  for(int pc=0;pc<nt;pc++){ char buf[256]; strcpy(buf,tok[pc]); char* q=buf; char* c=strchr(q,':'); char* a=""; if(c){*c=0;a=c+1;}
    if(!strcmp(q,"wait")) Sleep(atoi(a));
    else if(!strcmp(q,"key")){ev(a,0);Sleep(80);ev(a,1);Sleep(70);}
    else if(!strcmp(q,"cheat")){ // cheat:jqntdmh  types each key with confirmation via CCheat::m_CheatString (0x969110), retries a lost key
      int ok=1; for(const char*k=a;*k;k++){ char kn[2]={*k,0}; int got=0; for(int t=0;t<6&&!got;t++){ char b[32]={0}; SIZE_T n; ev(kn,0);Sleep(250);ev(kn,1);Sleep(350); ReadProcessMemory(H,(LPCVOID)0x969110,b,30,&n); if(b[0]==(*k-32)||(k[1]==0&&b[0]==0)) got=1; } if(!got) ok=0; }
      printf("[cheat] typed %s %s\n",a,ok?"ok":"FAILED"); }
    else if(!strcmp(q,"shot")){ // shot:name:N  -> d3d9 proxy dumps N consecutive back buffers as Z:\\tmp\\d3f-shots\\name_i.bmp
      char nm[64]; strncpy(nm,a,63); nm[63]=0; char* m=strchr(nm,':'); int n=3; if(m){*m=0;n=atoi(m+1);} FILE* f=fopen("Z:\\tmp\\d3f-tool\\shot.req","w"); if(f){ fprintf(f,"%s %d\n",nm,n); fclose(f); } Sleep(4000); printf("[shot] %s x%d requested\n",nm,n); }
    else if(!strcmp(q,"frames")){ // frames:SEC:TAG  per-frame times from CTimer::m_FrameCounter polled with QPC (no thread suspension)
      char tg[64]; int sec=10; strncpy(tg,a,63); tg[63]=0; char* m=strchr(tg,':'); if(m){*m=0; sec=atoi(tg); memmove(tg,m+1,strlen(m+1)+1);} 
      static double ft[40000],ts[40000]; static char st[40000]; int nf=0; LARGE_INTEGER fq,qn; QueryPerformanceFrequency(&fq);
      QueryPerformanceCounter(&qn); double t0=qn.QuadPart*1000.0/fq.QuadPart, lastT=t0, lastP=t0; DWORD lastfc=rd32(0xB7CB4C); int stallflag=0, stalls=0, multi=0;
      for(;;){ QueryPerformanceCounter(&qn); double t=qn.QuadPart*1000.0/fq.QuadPart; if(t-t0>=sec*1000.0) break;
        if(t-lastP>12.0){ stalls++; stallflag=1; } lastP=t; DWORD fc=rd32(0xB7CB4C);
        if(fc!=lastfc){ if(fc-lastfc>1) multi++; if(nf<40000){ ft[nf]=t-lastT; ts[nf]=t-t0; st[nf]=stallflag; nf++; } lastfc=fc; lastT=t; stallflag=0; }
        Sleep(1); }
      char fn[256]; sprintf(fn,"Z:\\tmp\\d3f-out\\%s.ft",tg); FILE* fo=fopen(fn,"w"); for(int i=0;i<nf;i++) fprintf(fo,"%.1f %.2f %d\n",ts[i],ft[i],st[i]); fclose(fo);
      static double srt[40000]; memcpy(srt,ft,nf*sizeof(double)); for(int i=1;i<nf;i++){ double v=srt[i]; int j=i-1; while(j>=0&&srt[j]>v){srt[j+1]=srt[j];j--;} srt[j+1]=v; }
      int g50=0,g100=0,g50s=0; for(int i=0;i<nf;i++){ if(ft[i]>50){g50++; if(st[i])g50s++;} if(ft[i]>100)g100++; }
      if(nf>0) printf("[frames %s] %ds n=%d fps=%.2f p50=%.1f p95=%.1f p99=%.1f max=%.1f ms  >50ms:%d (sampler-stall-affected %d) >100ms:%d  polls>12ms:%d multi:%d\n",tg,sec,nf,nf*1000.0/(sec*1000.0),srt[nf/2],srt[(int)(nf*0.95)],srt[(int)(nf*0.99)],srt[nf-1],g50,g50s,g100,stalls,multi);
      else printf("[frames %s] no frames\n",tg);
      printf("  hitch>50ms at(s):"); int c=0; for(int i=0;i<nf&&c<30;i++) if(ft[i]>50){ printf(" %.1f(%.0f)",ts[i]/1000.0,ft[i]); c++; } printf("\n"); }
    else if(!strcmp(q,"scene")){ // scene:TAG:SEC  no polling; just brackets a wall-clock window (same clock as the d3d9 proxy Present log)
      char tg[64]; int sec=10; strncpy(tg,a,63); tg[63]=0; char* m=strchr(tg,':'); if(m){*m=0; sec=atoi(m+1);} double w0=wallms(),c0=cpums(); Sleep(sec*1000); printf("[scene %s] %.1f %.1f cpu_ms=%.0f\n",tg,w0,wallms(),cpums()-c0); }
    else if(!strcmp(q,"slow")){ev(a,0);Sleep(300);ev(a,1);Sleep(300);}
    else if(!strcmp(q,"down")) ev(a,0); else if(!strcmp(q,"up")) ev(a,1);
    else if(!strcmp(q,"mark")||!strcmp(q,"status")) status(a);
    else if(!strcmp(q,"vehs")) vehs(a);
    else if(!strcmp(q,"pressuntil")){ char k[32]; strncpy(k,a,31); k[31]=0; char* m=strchr(k,':'); *m=0; int g=atoi(m+1); for(int i=0;i<30&&rd32(0xC8D4C0)!=(DWORD)g;i++){ ev(k,0);Sleep(80);ev(k,1);Sleep(2000);} printf("[pressuntil] gstate=%lu\n",rd32(0xC8D4C0)); }
    else if(!strcmp(q,"skipif")){ char* l=strchr(a,':'); *l=0; l++; if((!strcmp(a,"invehicle")&&invehicle())||(!strcmp(a,"!invehicle")&&!invehicle())){ for(int j=pc+1;j<nt;j++){ if(!strncmp(tok[j],"label:",6)&&!strcmp(tok[j]+6,l)){pc=j;break;} } } }
    else if(!strcmp(q,"until")){
      char cond[64]; strncpy(cond,a,63); cond[63]=0; char* m=strchr(cond,':'); DWORD mx=60000; if(m){*m=0;mx=atoi(m+1);}
      DWORD t0=GetTickCount(),cs=0; int ok=0;
      while(GetTickCount()-t0<mx){ if(!strcmp(cond,"control")){ if(control()){ if(!cs) cs=GetTickCount(); ok=(GetTickCount()-cs>=2500);} else cs=0; } else if(!strcmp(cond,"invehicle")) ok=invehicle(); else if(!strncmp(cond,"gstate=",7)) ok=rd32(0xC8D4C0)==(DWORD)atoi(cond+7); else if(!strcmp(cond,"menu")) ok=rd8(0xBA67A4); else if(!strcmp(cond,"started")) ok=rd32(0xC8D4C0)>=2; if(ok)break; Sleep(250);} 
      printf("[until] %s %s after %lu ms\n",cond,ok?"ok":"TIMEOUT",GetTickCount()-t0); }
    else if(!strcmp(q,"sample")){
      int n=atoi(a); DWORD f0=rd32(0xB7CB4C),t0=GetTickCount(),fl=f0,tl=t0; printf("sample:");
      for(int i=0;i<n;i++){ Sleep(1000); DWORD fc=rd32(0xB7CB4C),t=GetTickCount(); printf(" %.1f",(fc-fl)*1000.0/(t-tl)); fl=fc;tl=t; }
      printf("\n  avg %.2f fps over %lu ms (%lu frames)\n",(fl-f0)*1000.0/(tl-t0),tl-t0,fl-f0); }
    fflush(stdout); }
  return 0; }
