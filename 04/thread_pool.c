#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

struct thread_task {
  thread_task_f function;
  void* arg;
  void* result;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  bool in_pool;
  bool is_finished;
  bool is_joined;
};

struct task_queue {
  int size;
  int capacity;
  struct thread_task** data;
};

struct thread_pool {
  pthread_t* threads;
  int max_thread_count;
  int count;
  int active;
  struct task_queue* task_queue;
  pthread_cond_t cond;
  pthread_mutex_t lock;
  bool is_finished;
};

void*
checked_malloc(void* ptr, int n) {
  void* new_ptr = realloc(ptr, n);
  if (new_ptr == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  return new_ptr;
}

void
free_arguments(void* tp, void* th, void* tq) {
  free(tp);
  free(th);
  free(tq);
}

int
thread_pool_new(int max_thread_count, struct thread_pool** pool) {
  if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS) {
    return TPOOL_ERR_INVALID_ARGUMENT;
  }
  struct thread_pool* tp = checked_malloc(NULL, sizeof(struct thread_pool));
  pthread_t* threads = checked_malloc(NULL, max_thread_count * sizeof(pthread_t));
  tp->threads = threads;
  tp->max_thread_count = max_thread_count;
  tp->count = 0;
  tp->active = 0;

  struct task_queue* tq = checked_malloc(NULL, sizeof(struct task_queue));
  tq->size = 0;
  tq->capacity = 0;
  tq->data = NULL;
  tp->task_queue = tq;

  int err = pthread_cond_init(&tp->cond, NULL);
  if (err != 0) {
    free_arguments(tp, threads, tq);
    return -1; 
  }
  err = pthread_mutex_init(&tp->lock, NULL);
  if (err != 0) {
    pthread_cond_destroy(&tp->cond);
    free_arguments(tp, threads, tq);
    return -1; 
  }
  tp->is_finished = false;
  *pool = tp;
  return 0;
}

int
thread_pool_thread_count(const struct thread_pool* pool) {
  return pool->count;
}

void
destroy_pool(struct thread_pool* pool) {
  for (int i = 0; i < pool->count; ++i) {
    pthread_join(pool->threads[i], NULL);
  }
  free(pool->threads);

  free(pool->task_queue->data);
  free(pool->task_queue);

  pthread_cond_destroy(&pool->cond);
  pthread_mutex_destroy(&pool->lock);
  free(pool);
}

int
thread_pool_delete(struct thread_pool* pool) {
  pthread_mutex_lock(&pool->lock);
  if (pool->active != 0 || pool->task_queue->size != 0) {
    pthread_mutex_unlock(&pool->lock);
    return TPOOL_ERR_HAS_TASKS;
  }
  pool->is_finished = true;
  pthread_cond_broadcast(&pool->cond);
  pthread_mutex_unlock(&pool->lock);
  destroy_pool(pool); 
  return 0;
}

struct thread_task* 
get_next_task(struct thread_pool* pool) {
  struct task_queue* tq = pool->task_queue;
  return tq->data[--tq->size];
}

void* 
run_worker(void* arg) {
  struct thread_pool* pool = (struct thread_pool*) arg;
  while (true) {
    pthread_mutex_lock(&pool->lock);
    while (pool->task_queue->size == 0 && !pool->is_finished) {
      pthread_cond_wait(&pool->cond, &pool->lock); 
    }

    if (pool->is_finished) {
      pthread_mutex_unlock(&pool->lock);
      break;
    }

    struct thread_task* task = get_next_task(pool); 
    ++pool->active;
    pthread_mutex_unlock(&pool->lock);

    task->result = task->function(task->arg);
    pthread_mutex_lock(&task->lock);
    task->is_finished = true;
    pthread_mutex_lock(&pool->lock);
    --pool->active;
    pthread_cond_signal(&task->cond);
    pthread_mutex_unlock(&pool->lock);
    pthread_mutex_unlock(&task->lock);
  }
  return NULL;
}

struct task_queue*
resize_queue_if_needed(struct task_queue* tq) {
  if (tq->size + 1 > tq->capacity) {
    int new_cap = (tq->capacity + 1) * 2; 
    int new_size = new_cap * sizeof(struct thread_task*);
    struct thread_task** data = checked_malloc(tq->data, new_size);
    tq->data = data; 
  }
  return tq;
}

int
thread_pool_push_task(struct thread_pool* pool, struct thread_task* task) {
  pthread_mutex_lock(&pool->lock);
  struct task_queue* tq = resize_queue_if_needed(pool->task_queue);
  tq->data[tq->size++] = task; 
  task->in_pool = true;
  task->is_finished = false;
  task->is_joined = false;

  if (pool->count == pool->active && pool->count < pool->max_thread_count) {
    pthread_create(&pool->threads[pool->count++], NULL, run_worker, pool);
  }
  pthread_cond_signal(&pool->cond);
  pthread_mutex_unlock(&pool->lock);
  return 0;
}

int
thread_task_new(struct thread_task** task, thread_task_f function, void* arg) {
  struct thread_task* ts = checked_malloc(NULL, sizeof(struct thread_task));
  ts->function = function;
  ts->arg = arg;
  ts->in_pool = false;
  ts->is_joined = false;
  ts->is_finished = false;
  int err = pthread_cond_init(&ts->cond, NULL);
  if (err != 0) {
    free(ts);
    return -1; 
  }
  err = pthread_mutex_init(&ts->lock, NULL);
  if (err != 0) {
    pthread_cond_destroy(&ts->cond);
    free(ts);
    return -1; 
  }
  *task = ts;
  return 0;
}

bool
thread_task_is_finished(const struct thread_task* task) {
  return task->is_finished;
}

bool
thread_task_is_running(const struct thread_task* task) {
  return task->in_pool;
}

int
thread_task_join_with_timeout(struct thread_task* task, double timeout, void** result) {
  if (!task->in_pool) {
    return TPOOL_ERR_TASK_NOT_PUSHED;
  }
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout;
  int rt = 0;
  pthread_mutex_lock(&task->lock);
  while (!task->is_finished && rt == 0) {
    rt = pthread_cond_timedwait(&task->cond, &task->lock, &ts);
  }
  if (rt != 0) {
    pthread_mutex_unlock(&task->lock);
    return TPOOL_ERR_TIMEOUT;
  }
  *result = task->result;
  task->is_joined = true;
  pthread_mutex_unlock(&task->lock);
  return 0;
}

int
thread_task_join(struct thread_task* task, void** result) {
  if (!task->in_pool) {
    return TPOOL_ERR_TASK_NOT_PUSHED;
  }
  pthread_mutex_lock(&task->lock);
  while (!task->is_finished) {
    pthread_cond_wait(&task->cond, &task->lock);
  }
  *result = task->result;
  task->is_joined = true;
  pthread_mutex_unlock(&task->lock);
  return 0;
}

void 
destroy_task(struct thread_task* task) {
  pthread_cond_destroy(&task->cond);
  pthread_mutex_destroy(&task->lock);
  free(task);
}

int
thread_task_delete(struct thread_task* task) {
  if (!task->in_pool) {
    destroy_task(task);
    return 0;
  }
  pthread_mutex_lock(&task->lock);
  if (!(task->is_joined && task->is_finished)) {
    pthread_mutex_unlock(&task->lock);
    return TPOOL_ERR_TASK_IN_POOL;
  }
  pthread_mutex_unlock(&task->lock);
  destroy_task(task);
  return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task* task) {
  pthread_mutex_lock(&task->lock);
  task->is_joined = true;
  pthread_mutex_unlock(&task->lock);
  return 0;
}

#endif