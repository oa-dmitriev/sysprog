#include <string.h>
#include <stdio.h>

#include "global.h"

#include "./file/myfile.h"
#include "./sort/mysort.h"

double latency = 1000;
int coroCount = 0;
int curCoro = 0;
struct coro* coros;

void worker(const char* filename) {
  coroCall(readFromFileToBuf, filename);
  coroYieldWrapper();
  
  coroCall(readFromBufToArray);
  coroYieldWrapper();

  coroCall(mySort);
  coroYieldWrapper();

  coroFinishWrapper();
  coroReturn();
  exit(0);
}

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "no input files\n");
    exit(0);
  }

  coroCount = argc - 2;
  coros = malloc(coroCount * sizeof(struct coro));
  if (coros == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  char** fileNames = malloc(sizeof(char*) * (argc - 2));
  if (fileNames == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  sscanf(argv[1], "%lf", &latency);
  for (size_t i = 2; i < argc; ++i) {
    char* filename = malloc(strlen(argv[i]) + 1);
    if (filename == NULL) {
      fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
    
    memcpy(filename, argv[i], strlen(argv[i]) + 1);
    fileNames[i - 2] = filename;
  }
  
  clock_t mainStart = clock();

  for (size_t i = 0; i < coroCount; ++i) {
    if (coroInitWrapper(&coros[i]) != 0) {
      break;
    }
  }

  coroCall(worker, fileNames[curCoro]);

  coroWaitGroup();

  printf("Latency: %lfµs\n", latency);
  for (size_t i = 0; i < coroCount; ++i) {
    printf("Coroutine %ld ran for %fµs\n", i, coros[i].runningTime);
  }
  double t = (double) (clock() - mainStart) / CLOCKS_PER_SEC * 1000000;
  printf("Whole program ran for %fµs\n", t);

  struct Array* finalArray = finalMerge(coros);
  writeToFile("mergedFile", finalArray);

  freeCoros();

  for (size_t i = 0; i < coroCount; ++i) {
    free(fileNames[i]);
  }
  free(fileNames);
  free(finalArray->a);
  free(finalArray);
  return 0;
} 

void merge(int* a, size_t na, int* b, size_t nb) {
  int* arr = malloc(sizeof(int) * (na + nb));
  if (arr == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  size_t i = 0, j = 0, k = 0;
  while (i < na && j < nb) {
    if (a[i] <= b[j]) {
      arr[k++] = a[i++];
    } else {
      arr[k++] = b[j++];
    }
  }
  if (i < na) {
    memcpy(arr + k, a + i, sizeof(int) * (na - i));
  }
  if (j < nb) {
    memcpy(arr + k, b + j, sizeof(int) * (nb - j));
  }
  memcpy(a, arr, sizeof(int) * (na + nb));
  free(arr);
}

struct Array* finalMerge(struct coro* coros) {
  struct Array* arr = malloc(sizeof(struct Array));
  if (arr == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  arr->size = 0;
  for (int i = 0; i < coroCount; ++i) {
    arr->size += coros[i].array->size;
  }

  arr->a = malloc(sizeof(int) * arr->size);
  if (arr->a == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  size_t leftSize = coros[0].array->size;
  memcpy(arr->a, coros[0].array->a, sizeof(int) * coros[0].array->size);

  for (int i = 1; i < coroCount; ++i) {
    merge(arr->a, leftSize, coros[i].array->a, coros[i].array->size);
    leftSize += coros[i].array->size;
  }
  return arr;
}

void freeCoros() {
  for (int i = 0; i < coroCount; ++i) {
    free(coros[i].array->a);
    free(coros[i].array);

    free(coros[i].file->buf);
    free(coros[i].file->st);
    free(coros[i].file->cb);
    free(coros[i].file);

    free(coros[i].slv);
    free(coros[i].retPoints);
  }
  free(coros);
}
