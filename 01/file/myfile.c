#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <aio.h>
#include <unistd.h>

#include "myfile.h"

#define coroFile() (coroThis()->file)

void readFromFileToBuf(const char* filename) {
  coroFile() = malloc(sizeof(struct File));
  if (coroFile() == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  coroFile()->fd = open(filename, O_RDONLY);
  if (coroFile()->fd == -1) {
    fprintf(stderr, "%s: %s\n", strerror(errno), filename); 
    exit(EXIT_FAILURE);
  }
  coroYieldWrapper();

  coroFile()->st = malloc(sizeof(struct stat));
  if (coroFile()->st == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  fstat(coroFile()->fd, coroFile()->st);
  coroYieldWrapper();

  coroFile()->buf = malloc(coroFile()->st->st_size);
  if (coroFile()->buf == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  coroYieldWrapper();

  coroFile()->cb = malloc(sizeof(struct aiocb));
  if (coroFile()->cb == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  coroFile()->cb->aio_nbytes = coroFile()->st->st_size;
  coroFile()->cb->aio_fildes = coroFile()->fd;
  coroFile()->cb->aio_offset = 0;
  coroFile()->cb->aio_buf = coroFile()->buf;
  coroFile()->cb->aio_reqprio = 0;
  coroYieldWrapper();

  if (aio_read(coroFile()->cb) == -1) {
    fprintf(stderr, "Failed to create a request: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  while (aio_error(coroFile()->cb) == EINPROGRESS) {
    coroYieldWrapper();
  }
  
  if(coroFile()->st->st_size != aio_return(coroFile()->cb)) {
    fprintf(stderr, "Failed to read everything: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (close(coroFile()->fd) != 0) {
    fprintf(stderr, "Failed to close file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  coroReturn();
}

void readFromBufToArray() {
  size_t size = 0, cap = 10;
  int* arr = malloc(cap * sizeof(int));
  if (arr == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  const char* p = coroThis()->file->buf;
  int a, b;
  while (sscanf(p, "%d%n", &a, &b)==1) {
    if (size + 1 > cap) {
      cap = (cap + 1) * 2;
      int* newArr = realloc(arr, cap * sizeof(int));
      if (newArr == NULL) {
        free(arr);
        fprintf(stderr, "couldn't realloc at %d\n", __LINE__);
        exit(EXIT_FAILURE);
      }
      arr = newArr;
    }
    arr[size] = a;
    size++;
    p += b; 
  }
  coroThis()->array = malloc(sizeof(struct Array));
  if (coroThis()->array == NULL) {
    fprintf(stderr, "Error allocating space: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  coroThis()->array->a = arr;
  coroThis()->array->size = size;
  coroReturn();
}

void writeToFile(const char* filename, struct Array* arr) {  
  FILE* f = fopen(filename, "w");
  if (f == NULL) {
    fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  for (size_t i = 0; i < arr->size; ++i) {
    fprintf(f, "%d ", arr->a[i]);
  }

  if (fclose(f) != 0) {
    fprintf(stderr, "Failed to close file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
}
