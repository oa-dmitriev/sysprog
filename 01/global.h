#pragma once
#include <time.h>

extern double latency;

#define CoroLocalData \
  struct Array* array; \
  struct File* file; \
  struct SortLocalVariables* slv; \
  int slvCount; \
  int slvCapacity; \
  clock_t start; \
  double runningTime;
  
#include "coro.h"

#define coroInitWrapper(coro) ({ \
  (coro)->array = NULL; \
  (coro)->file = NULL; \
  (coro)->slv = NULL; \
  (coro)->slvCount = 0; \
  (coro)->slvCapacity = 0; \
  (coro)->start = clock(); \
  (coro)->runningTime = 0; \
  coroInit(coro); \
})

#define coroFinishWrapper() ({ \
  clock_t cur = clock(); \
  double t = (double) (cur - coroThis()->start) / CLOCKS_PER_SEC * 1000000; \
  coroThis()->runningTime += t; \
  coroFinish(); \
})

#define coroYieldWrapper(coro) ({ \
  clock_t cur = clock(); \
  double t = (double) (cur - coroThis()->start) / CLOCKS_PER_SEC * 1000000; \
  if (coroThis()->isFinished) { \
    coroYield(); \
  } else if (t >= latency) { \
    coroThis()->runningTime += t; \
    coroYield(); \
    coroThis()->start = clock(); \
  } \
})

void merge(int*, size_t, int*, size_t);
struct Array* finalMerge(struct coro* coros);
void freeCoros();
