#include "userfs.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdbool.h>

enum {
  BLOCK_SIZE = 512,
  MAX_FILE_SIZE = 1024 * 1024 * 1024,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
  /** Block memory. */
  char *memory;
  /** How many bytes are occupied. */
  int occupied;
  /** Next block in the file. */
  struct block *next;
  /** Previous block in the file. */
  struct block *prev;
};

struct file {
  /** Double-linked list of file blocks. */
  struct block *block_list;
  /**
   * Last block in the list above for fast access to the end
   * of file.
   */
  struct block *last_block;
  /** How many file descriptors are opened on the file. */
  int refs;
  /** File name. */
  const char *name;
  /** Files are stored in a double-linked list. */
  struct file *next;
  struct file *prev;

  int size;
};

/** List of all files. */
static struct file *file_list = NULL;

struct file* find_file(const char* name) {
  struct file* head = file_list;
  while (head != NULL) {
    if (strcmp(head->name, name)==0) {
      return head;
    }
    head = head->next;
  }
  return head;
}

struct filedesc {
  struct file *file;

  int id;
  int offset;
  int flag;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
  return ufs_error_code;
}

struct filedesc* create_filedesc(struct file* file) {
  struct filedesc* fd = malloc(sizeof(struct filedesc));
  if (fd == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  fd->file = file;
  fd->offset = 0;

  int id = 0;
  for (int i = 0; i < file_descriptor_count; ++i) {
    if (file_descriptors[i] != NULL) {
      ++id;
      continue;
    } 
    break;
  }
  fd->id = id;
  if (file_descriptor_count == id) {
    if (file_descriptor_count + 1 > file_descriptor_capacity) { 
      int new_cap = (file_descriptor_capacity + 1) * 2;
      int new_size = new_cap * sizeof(struct filedesc*);
      struct filedesc** new_filedesc = realloc(file_descriptors, new_size);
      if (new_filedesc == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
      }
      file_descriptors = new_filedesc;
      file_descriptor_capacity = new_cap;
    }
    ++file_descriptor_count;
  }
  file_descriptors[id] = fd;
  return fd;
}

struct file* create_file(const char* filename) {
  struct file* file = malloc(sizeof(struct file));
  if (file == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  file->block_list = NULL;
  file->last_block = NULL;
  file->refs = 0;

  char* name = malloc(strlen(filename) + 1);
  if (name == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memcpy(name, filename, strlen(filename) + 1);
  file->name = name;

  file->prev = NULL;
  if (file_list == NULL) {
    file->next = NULL;
  } else {
    file->next = file_list;
    file_list->prev = file;
  }
  file->size = 0;
  file_list = file;
  return file;
}

int
ufs_open(const char *filename, int flags)
{
  struct file* file = find_file(filename);
  if (file == NULL) {
    if (flags == UFS_CREATE) {
      file = create_file(filename);
      if (file == NULL) {
        return -1;
      }
    } else {
      ufs_error_code = UFS_ERR_NO_FILE;
      return -1;
    }
  }
  struct filedesc* fd = create_filedesc(file);
  if (fd == NULL) {
    return -1;
  }
  fd->flag = flags == 0 ? UFS_READ_WRITE : flags;
  ++file->refs;
  return fd->id;
}

struct filedesc* find_filedesc(int fd) {
  for (int i = 0; i < file_descriptor_count; ++i) {
    if (file_descriptors[i] != NULL &&
        file_descriptors[i]->id == fd) {
      return file_descriptors[i]; 
    }
  }
  return NULL;
}

struct block* create_block() {
  struct block* block = malloc(sizeof(struct block));
  if (block == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  block->memory = malloc(BLOCK_SIZE);
  if (block->memory == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  block->occupied = 0;
  block->next = NULL;
  block->prev = NULL;
  return block;
}

ssize_t
write_to_block(
    struct block* block, 
    struct file* file,
    int offset,
    const char* buf, size_t size
) {
  if (size < 1) {
    return size;
  }
  if (offset == MAX_FILE_SIZE) {
    ufs_error_code = UFS_ERR_NO_MEM; 
    return -1; 
  }

  int mem_left = BLOCK_SIZE - offset % BLOCK_SIZE;
  if (mem_left < size) {
    memcpy(block->memory + offset % BLOCK_SIZE, buf, mem_left);
    file->size += mem_left;
    block->occupied = BLOCK_SIZE;
    int next_size = size - mem_left;
    if (block->next == NULL) {
      struct block* nblock = create_block();
      block->next = nblock;
      nblock->prev = block;
      block = nblock;
      file->last_block = block;
    } else {
      block = block->next;
    }
    return mem_left + write_to_block(block, 
        file, offset + mem_left, buf + mem_left, next_size);
  }
  memcpy(block->memory + offset % BLOCK_SIZE, buf, size);
  file->size += size;
  if (offset % BLOCK_SIZE + size > block->occupied) {
    block->occupied = offset % BLOCK_SIZE + size;
  }
  return size;
}

ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
  struct filedesc* filedesc = find_filedesc(fd); 
  if (filedesc == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1; 
  }
  if ((filedesc->flag & (UFS_CREATE | UFS_WRITE_ONLY | UFS_READ_WRITE)) == 0) {
    ufs_error_code = UFS_ERR_NO_PERMISSION;
    return -1;
  }
  struct file* file = filedesc->file;
  int block_id = filedesc->offset / BLOCK_SIZE;
  int offset = filedesc->offset;

  if (file->block_list == NULL) {
    struct block* block = create_block();
    file->block_list = block;
    file->last_block = block;
  }
  struct block* block = file->block_list;
  for (int i = 0; i < block_id && block != NULL; ++i) {
    block = block->next; 
  }
  if (block == NULL) {
    block = create_block();
    block->prev = file->last_block;
    file->last_block->next = block;
    file->last_block = block;
  }
  ssize_t bytes_read = write_to_block(block, file, offset, buf, size); 
  if (bytes_read != -1) {
    filedesc->offset += bytes_read;
  }
  return bytes_read;
}

ssize_t 
read_from_block(struct block* block, int offset, char* buf, size_t size) {
  if (block == NULL) {
    return 0;
  }
  int eff_size = block->occupied - offset;
  if (eff_size >= size) {
    memcpy(buf, block->memory + offset, size);
    return size;
  }
  memcpy(buf, block->memory + offset, eff_size);
  if (block->occupied == BLOCK_SIZE) {
    int bytes_left = size - eff_size;
    return eff_size + read_from_block(block->next, 0, buf + eff_size, bytes_left);
  }
  return eff_size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
  struct filedesc* filedesc = find_filedesc(fd); 
  if (filedesc == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1; 
  }
  if ((filedesc->flag & (UFS_READ_WRITE | UFS_READ_ONLY | UFS_CREATE)) == 0) {
    ufs_error_code = UFS_ERR_NO_PERMISSION;
    return -1;
  }
  struct file* file = filedesc->file;
  int block_id = filedesc->offset / BLOCK_SIZE;
  int offset = filedesc->offset % BLOCK_SIZE;
  
  struct block* block = file->block_list;
  for (int i = 0; i < block_id && block != NULL; ++i) {
    block = block->next;
  }
  if (block == NULL) {
    return 0;
  }
  ssize_t read_bytes = read_from_block(block, offset, buf, size);
  if (read_bytes != -1) {
    filedesc->offset += read_bytes;
  }
  return read_bytes;
}

void free_blocks(struct block* head) {
  struct block* next;
  while (head != NULL) {
    next = head ->next;
    free(head->memory);
    free(head);
    head = next;
  }
}

void free_file(struct file* file) {
  struct block* block = file->block_list;
  free_blocks(block);
  free((char*) file->name);
  free(file);
}


bool
exists(struct file* file) {
  struct file* cur = file_list;
  while (cur != NULL) {
    if (cur == file) {
      return true;
    }
    cur = cur->next;
  }
  return false;
}

int
ufs_close(int fd)
{
  for (int i = 0; i < file_descriptor_count; ++i) {
    if (file_descriptors[i] != NULL && file_descriptors[i]->id == fd) {
      struct file* file = file_descriptors[i]->file;
      if(--file->refs == 0 && !exists(file)) {
        free_file(file); 
      }
      free(file_descriptors[i]);
      file_descriptors[i] = NULL;
      return 0;
    }
  }
  ufs_error_code = UFS_ERR_NO_FILE;
  return -1;
}


int
ufs_delete(const char *filename)
{
  struct file* file = find_file(filename);
  if (file != NULL) {
    if (file->next != NULL) {
      file->next->prev = file->prev;
    }
    if (file->prev != NULL) {
      file->prev->next = file->next;
    }
    if (file == file_list) {
      file_list = file->next;
    }
    if (file->refs == 0) {
      free_file(file);
    }
    return 0;
  }
  ufs_error_code = UFS_ERR_NO_FILE;
  return -1;
}

void update_fildescs(struct file* file) {
  int size = file->size;
  struct filedesc* fd = NULL;
  for (int i = 0; i < file_descriptor_count; ++i) {
    fd = file_descriptors[i];
    if (fd != NULL && fd->file == file) {
      fd->offset = fd->offset > size ? size : fd->offset;
    }
  }
}

int 
ufs_resize(int fd, size_t new_size) {
  if (new_size < 0) {
    return -1;
  }
  struct filedesc* filedesc = find_filedesc(fd);
  if (filedesc == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }
  struct file* file = filedesc->file;
  int new_blocks = new_size / BLOCK_SIZE - file->size / BLOCK_SIZE;
  int offset = new_size % BLOCK_SIZE;
  struct block* block = file->last_block;
  if (new_blocks > 0) {
    for (int i = 0; i < new_blocks; ++i) {
      struct block* nblock = create_block();
      nblock->prev = block;
      block->next = nblock;
      block = nblock;
      file->size += BLOCK_SIZE;
    }
    file->last_block = block;
    return 0;
  } 
  
  for (int i = -1; i > new_blocks && block != NULL; --i) { 
    block = block->prev;
    file->size -= BLOCK_SIZE;
  }
  if (block != NULL) {
    block->occupied = offset;
    file->size += offset - BLOCK_SIZE;
    update_fildescs(file);
  }
  file->last_block = block;
  return 0;
}
