/* WIN-PERF input driver for BOTH exes (the original has no injector): SendInput scancodes + ReadProcessMemory probes of the game state (same VAs in the original and in
 * our exe: the data image is mapped at 0x858000..). Native-Windows port of .notes/reports/FPS_tools/sk3.c (Wine/Mac), with a FOREGROUND GUARD: a key-down is sent only while the
 * foreground window belongs to the game process; otherwise (after 10 s) the script aborts, so a user who alt-tabs never gets stray keys.
 * usage: inputdrv <script> <exe-name>      script = ';' or newline separated commands, '#' comments:
 *   wait:MS  shot:PATH.bmp  cap:FPS  skipintro:MAXMS  mouse:X,Y  key:K  down:K  up:K  slow:K  cheat:STRING  mark:TAG  until:control|invehicle|menu|started|gstate=N[:MAXMS]  pressuntil:K:GSTATE  skipif:invehicle|!invehicle:LABEL  label:L
 *   scene:TAG:SEC   prints "[scene TAG] qpc_ms0 qpc_ms1 cpu_ms=..": qpc_ms is the ABSOLUTE QPC clock in ms = the d3d9px.bin t_ms clock (frame_stats.py windows)
 * Probes run only in until/pressuntil (every 250 ms), never faster. Build: tools/standalone/windows/build_tools.sh */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static struct { const char* n; WORD sc; int ext; } K[] = {
 {"return",0x1C,0},{"lctrl",0x1D,0},{"escape",1,0},{"space",0x39,0},{"lshift",0x2A,0},{"up",0x48,1},{"down",0x50,1},{"left",0x4B,1},{"right",0x4D,1},{"tab",0x0F,0},
 {"q",0x10,0},{"w",0x11,0},{"e",0x12,0},{"r",0x13,0},{"t",0x14,0},{"y",0x15,0},{"u",0x16,0},{"i",0x17,0},{"o",0x18,0},{"p",0x19,0},
 {"a",0x1E,0},{"s",0x1F,0},{"d",0x20,0},{"f",0x21,0},{"g",0x22,0},{"h",0x23,0},{"j",0x24,0},{"k",0x25,0},{"l",0x26,0},
 {"z",0x2C,0},{"x",0x2D,0},{"c",0x2E,0},{"v",0x2F,0},{"b",0x30,0},{"n",0x31,0},{"m",0x32,0},{0,0,0}};
static HANDLE H; static DWORD g_pid;
static int fg_ok(void){ DWORD p=0; GetWindowThreadProcessId(GetForegroundWindow(),&p); return p==g_pid; }
static BOOL CALLBACK enumw(HWND w,LPARAM l){ DWORD p=0; GetWindowThreadProcessId(w,&p); if(p==g_pid && IsWindowVisible(w)){ /* the main window is hidden, the visible one is the D3D device window */ *(HWND*)l=w; return FALSE; } return TRUE; }
/* the game is ours to drive (the user knows a game run owns the screen): bring ITS window to the foreground with the usual ALT trick */
static void focus_game(void){ HWND w=0; EnumWindows(enumw,(LPARAM)&w); if(!w) return; HWND fg=GetForegroundWindow(); DWORD ft=fg?GetWindowThreadProcessId(fg,0):0, me=GetCurrentThreadId();
  keybd_event(VK_MENU,0,0,0); if(ft&&ft!=me) AttachThreadInput(me,ft,TRUE); SetForegroundWindow(w); if(ft&&ft!=me) AttachThreadInput(me,ft,FALSE); keybd_event(VK_MENU,0,KEYEVENTF_KEYUP,0); }
static void ev(const char* n,int up){
  for(int i=0;K[i].n;i++) if(!strcmp(K[i].n,n)){
    if(!up && !fg_ok()){ for(int t=0;t<40&&!fg_ok();t++){ if(t%4==0) focus_game(); Sleep(250); } if(!fg_ok()){ { char t[80]={0}; DWORD fp=0; HWND f=GetForegroundWindow(); GetWindowThreadProcessId(f,&fp); GetWindowTextA(f,t,80); printf("[ABORT] game window is not in the foreground (10 s; foreground = pid %lu '%s'): refusing to send keys\n",fp,t); } fflush(stdout); ExitProcess(3); } }
    INPUT in; memset(&in,0,sizeof in); in.type=INPUT_KEYBOARD; in.ki.wScan=K[i].sc;
    in.ki.dwFlags=KEYEVENTF_SCANCODE|(K[i].ext?KEYEVENTF_EXTENDEDKEY:0)|(up?KEYEVENTF_KEYUP:0);
    SendInput(1,&in,sizeof in); return; }
  printf("unknown key %s\n",n);
}
static DWORD rd32(DWORD a){ DWORD v=0; SIZE_T n; if(!H||!ReadProcessMemory(H,(LPCVOID)(ULONG_PTR)a,&v,4,&n)) return 0; return v;}
static BYTE rd8(DWORD a){ BYTE v=0; SIZE_T n; if(!H||!ReadProcessMemory(H,(LPCVOID)(ULONG_PTR)a,&v,1,&n)) return 0; return v;}
static WORD rd16(DWORD a){ WORD v=0; SIZE_T n; if(!H||!ReadProcessMemory(H,(LPCVOID)(ULONG_PTR)a,&v,2,&n)) return 0xFFFF; return v;}
static DWORD ped(){ return rd32(0xB7CD98); }
static int invehicle(){ DWORD p=ped(); return p && (rd8(p+0x46D)&1) && rd32(p+0x58C); }
static int control(){ return ped() && !rd8(0xBA67A4) && !rd8(0xB5F851) && rd32(0xC8D4C0)==9 && rd16(0xB73564)==0; }
static int pedpos(float*p){ DWORD e=ped(); if(!e) return 0; DWORD m=rd32(e+0x14); SIZE_T n; return ReadProcessMemory(H,(LPCVOID)(ULONG_PTR)(m?m+0x30:e+4),p,12,&n); }
static void findproc(const char* exe){
  HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32 pe; pe.dwSize=sizeof pe;
  for(BOOL ok=Process32First(s,&pe);ok;ok=Process32Next(s,&pe)) if(!_stricmp(pe.szExeFile,exe)){ g_pid=pe.th32ProcessID; H=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,g_pid); printf("pid %lu handle %p\n",g_pid,H); }
  CloseHandle(s);
}
static double cpums(void){ FILETIME c,e,k,u; if(!H||!GetProcessTimes(H,&c,&e,&k,&u)) return -1; return (((double)(((ULONGLONG)k.dwHighDateTime<<32)|k.dwLowDateTime))+((double)(((ULONGLONG)u.dwHighDateTime<<32)|u.dwLowDateTime)))/10000.0; }
static double qpcms(void){ LARGE_INTEGER f,q; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&q); return q.QuadPart*1000.0/f.QuadPart; }
static void status(const char* tag){
  printf("[%s] qpc=%.1f gstate=%lu menu=%u cut=%u fc=%lu ped=%08lx inveh=%d padoff=%04x\n",tag,qpcms(),rd32(0xC8D4C0),rd8(0xBA67A4),rd8(0xB5F851),rd32(0xB7CB4C),ped(),invehicle(),rd16(0xB73564));
}
int main(int argc,char**argv){
  if(argc<3){ puts("usage: inputdrv <script> <exe-name>"); return 2; }
  findproc(argv[2]); if(!H){ puts("process not found"); return 2; }
  static char* tok[4000]; int nt=0; FILE* f=fopen(argv[1],"r"); static char line[512]; if(!f){ puts("script not found"); return 2; }
  while(fgets(line,512,f)){ char* p=strchr(line,'#'); if(p)*p=0; for(char*q=strtok(line,";\r\n");q;q=strtok(0,";\r\n")){ while(*q==' ')q++; if(*q) tok[nt++]=_strdup(q);} }
  for(int pc=0;pc<nt;pc++){ char buf[256]; strcpy(buf,tok[pc]); char* q=buf; char* c=strchr(q,':'); char* a=""; if(c){*c=0;a=c+1;}
    DWORD ex=0; if(!GetExitCodeProcess(H,&ex)||ex!=STILL_ACTIVE){ printf("[exit] game process ended (code %lu)\n",ex); return 4; }
    if(!strcmp(q,"wait")) Sleep(atoi(a));
    else if(!strcmp(q,"key")){ev(a,0);Sleep(80);ev(a,1);Sleep(70);}
    else if(!strcmp(q,"slow")){ev(a,0);Sleep(300);ev(a,1);Sleep(300);}
    else if(!strcmp(q,"cheat")){ /* types each key and confirms it through CCheat::m_CheatString (0x969110); retries a lost key */
      int ok=1; for(const char*k=a;*k;k++){ char kn[2]={*k,0}; int got=0; for(int t=0;t<6&&!got;t++){ char b[32]={0}; SIZE_T n; ev(kn,0);Sleep(250);ev(kn,1);Sleep(350); ReadProcessMemory(H,(LPCVOID)(ULONG_PTR)0x969110,b,30,&n);
          got=(b[0]==*k||b[0]==(*k>='a'&&*k<='z'?*k-32:*k)); }
        if(!got) ok=0; }
      printf("[cheat] typed %s %s\n",a,ok?"ok":"FAILED"); }
    else if(!strcmp(q,"mouse")){ int x=0,y=0; sscanf(a,"%d,%d",&x,&y); if(!fg_ok()) Sleep(2000); /* absolute move in desktop pixels (the game runs exclusive at the desktop mode, window at 0,0); the menu follows the pointer */
      INPUT in; memset(&in,0,sizeof in); in.type=INPUT_MOUSE; in.mi.dx=(LONG)((x+0.5)*65535.0/GetSystemMetrics(SM_CXSCREEN)); in.mi.dy=(LONG)((y+0.5)*65535.0/GetSystemMetrics(SM_CYSCREEN)); in.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE; SendInput(1,&in,sizeof in); Sleep(300); }
    else if(!strcmp(q,"cap")){ const char* cf=getenv("D3D9PX_CAPFILE"); FILE* cfh=cf?fopen(cf,"w"):0; if(cfh){ fprintf(cfh,"%d\n",atoi(a)); fclose(cfh); } Sleep(400); } /* frame cap for the d3d9px proxy (fps, 0 = uncapped) */
    else if(!strcmp(q,"shot")){ const char* sf=getenv("D3D9PX_SHOTFILE"); FILE* sh=sf?fopen(sf,"w"):0; if(sh){ fprintf(sh,"%s\n",a); fclose(sh); } Sleep(2500); } /* d3d9px screenshot: shot:C:/path/name.bmp */
    else if(!strcmp(q,"down")) ev(a,0); else if(!strcmp(q,"up")) ev(a,1);
    else if(!strcmp(q,"mark")||!strcmp(q,"status")) status(a);
    else if(!strcmp(q,"scene")){ char tg[64]; int sec=10; strncpy(tg,a,63); tg[63]=0; char* m=strchr(tg,':'); if(m){*m=0; sec=atoi(m+1);} double w0=qpcms(),c0=cpums(); Sleep(sec*1000); printf("[scene %s] %.1f %.1f cpu_ms=%.0f\n",tg,w0,qpcms(),cpums()-c0); }
    else if(!strcmp(q,"pressuntil")){ char k[32]; strncpy(k,a,31); k[31]=0; char* m=strchr(k,':'); *m=0; int g=atoi(m+1); for(int i=0;i<60&&rd32(0xC8D4C0)!=(DWORD)g;i++){ ev(k,0);Sleep(80);ev(k,1);Sleep(2000);} printf("[pressuntil] gstate=%lu qpc=%.1f\n",rd32(0xC8D4C0),qpcms()); }
    else if(!strcmp(q,"skipif")){ char* l=strchr(a,':'); *l=0; l++; if((!strcmp(a,"invehicle")&&invehicle())||(!strcmp(a,"!invehicle")&&!invehicle())){ for(int j=pc+1;j<nt;j++){ if(!strncmp(tok[j],"label:",6)&&!strcmp(tok[j]+6,l)){pc=j;break;} } } }
    else if(!strcmp(q,"label")){}
    else if(!strcmp(q,"skipintro")){ /* skipintro:MAXMS - press ESC every 2.5 s while a cutscene runs, until the player has control for 3 s; the position is only reported */
      DWORD mx=atoi(a),t0=GetTickCount(),gs=0; int ok=0; while(GetTickCount()-t0<mx){ if(control()){ if(!gs) gs=GetTickCount(); if(GetTickCount()-gs>=3000){ ok=1; break; } } else { gs=0; if(rd8(0xB5F851)){ ev("escape",0);Sleep(100);ev("escape",1); } } Sleep(2500); }
      float p[3]={0,0,0}; pedpos(p); printf("[skipintro] %s after %lu ms, ped at %.1f %.1f %.1f\n",ok?"ok":"TIMEOUT",GetTickCount()-t0,p[0],p[1],p[2]); }
    else if(!strcmp(q,"until")){
      char cond[64]; strncpy(cond,a,63); cond[63]=0; char* m=strchr(cond,':'); DWORD mx=60000; if(m){*m=0;mx=atoi(m+1);}
      DWORD t0=GetTickCount(),cs=0; int ok=0;
      while(GetTickCount()-t0<mx){ if(!strcmp(cond,"control")){ if(control()){ if(!cs) cs=GetTickCount(); ok=(GetTickCount()-cs>=2500);} else cs=0; } else if(!strcmp(cond,"invehicle")) ok=invehicle(); else if(!strncmp(cond,"gstate=",7)) ok=rd32(0xC8D4C0)==(DWORD)atoi(cond+7); else if(!strcmp(cond,"menu")) ok=rd8(0xBA67A4); else if(!strcmp(cond,"started")) ok=rd32(0xC8D4C0)>=2; if(ok)break; Sleep(250);}
      printf("[until] %s %s after %lu ms\n",cond,ok?"ok":"TIMEOUT",GetTickCount()-t0); }
    else printf("unknown command %s\n",q);
    fflush(stdout); }
  return 0; }
