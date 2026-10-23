// Host test of source/portable/crt.h + stdcompat.h (non-Windows only). Not wired into CMake. Build and run:
//   /opt/homebrew/opt/llvm/bin/clang++ -isysroot $(xcrun --show-sdk-path) -std=c++23 -Isource -o /tmp/crt_shim_test tests/portable/crt_shim_test.cpp && /tmp/crt_shim_test   (prints fails=0)
#include <cassert>
#include <cassert>
#include "portable/crt.h"
#include "portable/stdcompat.h"
static int fails=0;
#define CHECK(x) do{ if(!(x)){ printf("FAIL line %d: %s\n",__LINE__,#x); fails++; } }while(0)
int main(){
  static_assert(sizeof(LONG)==4 && sizeof(DWORD)==4 && sizeof(HRESULT)==4 && sizeof(WORD)==2 && sizeof(LARGE_INTEGER)==8 && sizeof(GUID)==16);
  char b[8];
  CHECK(strcpy_s(b,"abc")==0 && !strcmp(b,"abc"));
  notsa::portable::InvalidParameterHandler()=[](const wchar_t*,const wchar_t*,const wchar_t*,unsigned,uintptr_t){};
  CHECK(strcpy_s(b,"abcdefgh")==ERANGE && b[0]==0);
  CHECK(strcpy_s(b,"abcdefg")==0);
  CHECK(strcat_s(b,"x")==ERANGE && b[0]==0);
  strcpy_s(b,"ab"); CHECK(strcat_s(b,"cd")==0 && !strcmp(b,"abcd"));
  CHECK(strncpy_s(b,"abcdefghij",_TRUNCATE)==STRUNCATE && !strcmp(b,"abcdefg"));
  CHECK(strncpy_s(b,sizeof(b),"abcdefghij",3)==0 && !strcmp(b,"abc"));
  CHECK(strncpy_s(b,sizeof(b),"abcdefghij",9)==ERANGE && b[0]==0);
  CHECK(sprintf_s(b,"%d",12345)==5 && !strcmp(b,"12345"));
  CHECK(sprintf_s(b,sizeof(b),"%d",123456789)==-1 && b[0]==0);
  char n1[16], n2[16]; int i; float f; char c; unsigned char u;
  CHECK(sscanf_s("hello world 42 1.5", "%s %s %d %f", n1, (unsigned)sizeof(n1), n2, std::size(n2), &i, &f)==4 && !strcmp(n1,"hello") && !strcmp(n2,"world") && i==42 && f==1.5f);
  CHECK(sscanf_s("A7 name","%c%d %s", &c, 1u, &i, n1, sizeof(n1))==3 && c=='A' && i==7 && !strcmp(n1,"name"));
  CHECK(sscanf_s("abcdefghijklmnop rest","%4s", n1, sizeof(n1))==1 && !strcmp(n1,"abcd"));
  CHECK(sscanf_s("abcdefghijklmnop","%s", n1, 5u)==1 && !strcmp(n1,"abcd"));
  CHECK(sscanf_s("1.5, 2, foo bar\tbaz","%f, %d, %[^\t\n]", &f,&i,n1,sizeof(n1))==3 && !strcmp(n1,"foo bar") && i==2);
  CHECK(sscanf_s("x 5 y","%*s %d %s", &i, n1, sizeof(n1))==2 && i==5 && !strcmp(n1,"y"));
  CHECK(sscanf_s("200","%hhu",&u)==1 && u==200);
  CHECK(sscanf_s("carrec12","carrec%d",&i)==1 && i==12);
  CHECK(sscanf_s("1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32","%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
     &i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i,&i)==32 && i==32);
  CHECK(_stricmp("AbC","aBc")==0 && _strnicmp("ABCd","abcE",3)==0);
  char s[]="MiXed"; _strlwr(s); CHECK(!strcmp(s,"mixed")); _strupr_s(s); CHECK(!strcmp(s,"MIXED"));
  char t[]="a,b;c"; char*ctx; CHECK(!strcmp(strtok_s(t,",;",&ctx),"a")); CHECK(!strcmp(strtok_s(nullptr,",;",&ctx),"b"));
  volatile LONG l=5; CHECK(InterlockedIncrement(&l)==6 && InterlockedExchange(&l,9)==6 && InterlockedCompareExchange(&l,1,9)==9 && l==1);
  double d=0; auto r=notsa::compat::from_chars("2.5",("2.5")+3,d); CHECK(d==2.5 && r.ec==std::errc{});
  int ii=0; auto r2=notsa::compat::from_chars("17x","17x"+3,ii); CHECK(ii==17);
  FILE*fp=nullptr; CHECK(fopen_s(&fp,"/nonexistent/x","r")!=0 && fp==nullptr);
  char cwd[512]; CHECK(_getcwd(cwd,512)!=nullptr);
  char dt[12]; CHECK(_strdate_s(dt,12u)==0 && strlen(dt)==8); CHECK(_strtime_s(dt)==0 && strlen(dt)==8);
  char m1[4]; CHECK(memcpy_s(m1,4,"abc",3)==0 && memcpy_s(m1,2,"abc",3)==ERANGE && m1[0]==0); CHECK(_memicmp("ABC","abd",3)<0);
  wchar_t wc[512]; CHECK(_wgetcwd(wc,512)!=nullptr);
  printf("fails=%d\n",fails); return fails;
}
