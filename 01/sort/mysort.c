#include "../global.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "mysort.h"

#define slv() (&(coroThis()->slv[coroThis()->slvCount - 1]))

#define coroReturnWrapper() ({ \
  --(coroThis()->slvCount); \
  coroReturn(); \
})

void getSnapshotOfLocalData(int* a, int n) {
  struct coro* c = coroThis();
  if (c->slvCount + 1 > c->slvCapacity) {
    size_t slvNewCap = (c->slvCapacity + 1) * 2;
    size_t slvNewSize = slvNewCap * sizeof(struct SortLocalVariables);
    c->slv = (struct SortLocalVariables*) realloc(c->slv, slvNewSize);
    if (c->slv == NULL) {
      fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    c->slvCapacity = slvNewCap;
  }
  c->slv[c->slvCount].a = a;
  c->slv[c->slvCount].n = n;
  c->slv[c->slvCount].pe = a[n-1];
  c->slv[c->slvCount].i = 0;
  c->slv[c->slvCount].j = 0;
  ++(c->slvCount);
}

void swap(int* a, int* b) {
  int c = *a;
  *a = *b;
  *b = c;
}

void sort(int* a, int n) {
  if (n < 2) {
    coroReturn();
  }
  getSnapshotOfLocalData(a, n);
  coroYieldWrapper();

  for (; slv()->i < slv()->n - 1; ++slv()->i) {
    if (slv()->a[slv()->i] <= slv()->pe) {
      swap(slv()->a + slv()->j, slv()->a + slv()->i);
      ++slv()->j;
    }
    coroYieldWrapper();
  }
  swap(slv()->a + slv()->j, slv()->a + slv()->n - 1);
  ++slv()->j;
  coroYieldWrapper();

  coroCall(sort, slv()->a, slv()->j - 1);
  coroYieldWrapper();

  coroCall(sort, slv()->a + slv()->j, slv()->n - slv()->j);
  coroReturnWrapper();
}

void mySort() {
  int* a = coroThis()->array->a;
  size_t n = coroThis()->array->size;
  coroCall(sort, a, n);
  coroReturn();
}
