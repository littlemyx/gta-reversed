#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
static long cnt=0,tot=0; __attribute__((destructor)) static void fin(){{FILE*f=fopen("/private/tmp/claude-501/race1/slowio.log","a");fprintf(f,"slowio: %ld delayed preads, %ld bytes\n",cnt,tot);fclose(f);}}
__attribute__((constructor)) static void ini(){FILE*f=fopen("/private/tmp/claude-501/race1/slowio.log","a");fprintf(f,"loaded\n");fclose(f);}
static double mbps = -1; static int jit=0;
static ssize_t my_pread(int fd, void* b, size_t n, off_t o){
  if (mbps < 0){ const char* e=getenv("SLOWIO_MBPS"); mbps = e?atof(e):0; e=getenv("SLOWIO_JITTER_MS"); jit=e?atoi(e):0; srand(getpid()); }
  if (!pthread_main_np() && (mbps>0||jit)){
    cnt++;tot+=n; double us = mbps>0 ? (double)n/(mbps*1e6)*1e6 : 0; if (jit) us += (rand()%(jit*1000+1));
    struct timespec ts={(time_t)(us/1e6),(long)(((long long)us%1000000)*1000)}; nanosleep(&ts,0);
  }
  return pread(fd,b,n,o);
}
__attribute__((used)) static struct { const void* r; const void* o; } interposers[] __attribute__((section("__DATA,__interpose"))) = { {(const void*)my_pread,(const void*)pread} };
