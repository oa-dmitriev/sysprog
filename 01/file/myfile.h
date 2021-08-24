#include "../global.h"
#include "../sort/mysort.h"
#include <errno.h>

struct File { 
  int fd; 
  char* buf;
  struct stat* st;
  struct aiocb* cb;
};

void readFromFileToBuf(const char* filename);
void readFromBufToArray();
void writeToFile(const char*, struct Array*);

