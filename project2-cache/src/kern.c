// kern: cache/TLB kernel. The caller pins it (taskset). Usage:
//   kern <mul|saxpy> <array_bytes:pow2> <seq|stride|rand> <stride_bytes> <4k|2m> <min_s>
// Prints one line: total_bytes,ns_per_elem,GB_s,elems,ok
//   GB_s counts 3 accesses/elem = 24 B (no RFO). elems = all elements processed (incl. warmup).
//   ok = every element verified against the closed-form expected value (exit 1 if not).
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>

static size_t n, m, lm, s, k;
static int pat;  // 0 seq, 1 stride, 2 rand

static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec*1e-9; }

// Both orders are permutations of [0,n): every element is touched exactly once per pass.
static inline size_t idx(size_t j){
  if(pat==1) return (j&(m-1))*s + (j>>lm);   // every s-th element, then next offset
  size_t sh=k/2, mk=n-1;                     // rand: bijective multiply/xorshift mix mod 2^k
  j=(j*0x9E3779B97F4A7C15ull)&mk; j^=j>>sh;
  j=(j*0xBF58476D1CE4E5B9ull)&mk; j^=j>>sh;
  return j;
}
#define LOOP(B) if(pat==0){for(size_t i=0;i<n;i++){B;}}else{for(size_t j=0;j<n;j++){size_t i=idx(j);B;}}

__attribute__((noinline)) static void pass(int op, double*restrict x, double*restrict y, double*restrict z){
  if(op==0){ LOOP(z[i]=x[i]*y[i]) } else { LOOP(y[i]=0.5*x[i]+y[i]) }
}

int main(int c, char**v){
  if(c<7){ fprintf(stderr,"usage: kern <mul|saxpy> <array_bytes> <seq|stride|rand> <stride_B> <4k|2m> <min_s>\n"); return 2; }
  int op = !strcmp(v[1],"saxpy");
  size_t B = strtoull(v[2],0,0);
  pat = !strcmp(v[3],"stride") ? 1 : !strcmp(v[3],"rand") ? 2 : 0;
  size_t sb = strtoull(v[4],0,0);
  int huge = !strcmp(v[5],"2m");
  double mins = atof(v[6]);

  n = B/8; s = pat==1 ? sb/8 : 1;
  if(n<64 || (n&(n-1)) || s<1 || (s&(s-1)) || s>n){ fprintf(stderr,"array_bytes and stride must be powers of 2\n"); return 2; }
  m = n/s; lm = __builtin_ctzll(m); k = __builtin_ctzll(n);

  size_t na = op ? 2 : 3, step = B + 1088;           // pad arrays by 17 lines to avoid set/4K aliasing
  size_t T = (na*step + (2<<20) - 1) & ~(size_t)((2<<20) - 1);
  char *p = mmap(0, T, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|(huge?MAP_HUGETLB:0), -1, 0);
  if(p==MAP_FAILED){ perror("mmap"); return 1; }   // fail loudly: no silent fallback
  if(!huge) madvise(p, T, MADV_NOHUGEPAGE);
  double *x=(double*)p, *y=(double*)(p+step), *z=(double*)(p+2*step);
  for(size_t i=0;i<n;i++){ x[i]=1.0; y[i]=2.0; if(!op) z[i]=0.0; }  // first touch all pages

  size_t reps = (1<<22)/n; if(!reps) reps=1;
  pass(op,x,y,z);                                    // untimed warmup
  size_t passes=1, timed=0; double t=0;
  do { double t0=now(); for(size_t r=0;r<reps;r++) pass(op,x,y,z); t+=now()-t0; passes+=reps; timed+=reps; } while(t<mins);

  int ok=1;
  if(!op){ for(size_t i=0;i<n;i++) if(z[i]!=2.0) ok=0; }
  else { double e=2.0+0.5*(double)passes; for(size_t i=0;i<n;i++) if(y[i]!=e) ok=0; }

  double e = (double)timed*n;
  printf("%zu,%.4f,%.3f,%zu,%d\n", na*B, t/e*1e9, 24.0*e/t/1e9, passes*n, ok);
  return ok ? 0 : 1;
}
