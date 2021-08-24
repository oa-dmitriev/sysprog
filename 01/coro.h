#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CoroLocalData
#error "No local data for coroutines"
#endif 

struct coro {
  jmp_buf env;
  bool isFinished;
  jmp_buf* retPoints;
  int retCount;
  int retCapacity;
  
  CoroLocalData;
};

extern int coroCount;

extern int curCoro;

extern struct coro* coros;

#define coroThis() (&coros[curCoro])

#define coroInit(coro) ({ \
  (coro)->isFinished = false; \
  (coro)->retCount = 0; \
  (coro)->retCapacity = 0; \
  (coro)->retPoints = NULL; \
  setjmp((coro)->env); \
})


#define coroYield() ({ \
  int old_i = curCoro; \
  curCoro = (curCoro + 1) % coroCount; \
  if (setjmp(coros[old_i].env) == 0) \
      longjmp(coros[curCoro].env, 1); \
})

#define coroCall(func, ...) ({ \
  struct coro* c = coroThis(); \
  if (c->retCount+1 > c->retCapacity) { \
    int newCap = (c->retCapacity + 1) * 2; \
    int newSize = newCap * sizeof(jmp_buf); \
    jmp_buf* dummy = realloc(c->retPoints, newSize); \
    if (dummy == NULL) { \
      fprintf(stderr, "Error allocating space: %s\n", strerror(errno)); \
      exit(EXIT_FAILURE); \
    } \
    c->retPoints = dummy; \
    c->retCapacity = newCap; \
  } \
  if (setjmp(c->retPoints[c->retCount]) == 0) { \
    ++c->retCount; \
    func(__VA_ARGS__); \
  } \
})

#define coroFinish() ({ \
  coroThis()->isFinished = true; \
})

#define coroReturn() ({ \
  struct coro* c = coroThis(); \
  longjmp(c->retPoints[--(c->retCount)], 1); \
})

#define coroWaitGroup() \
  do { \
    bool isAllfinished = true; \
    for (int i = 0; i < coroCount; ++i) { \
      if (!coros[i].isFinished) { \
        fprintf(stderr, "Coro %d is still active, rescheduling...\r", i); \
        isAllfinished = false; \
        break; \
      } \
    } \
    if (isAllfinished) { \
      fprintf(stderr, "\nNo more active coros\n"); \
      break; \
    } \
    coroYield(); \
  } while(true);
