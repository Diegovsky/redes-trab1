#include "shared.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>

const int NUM_THREADS = 16;
const int MAX_FDS = 128;

typedef struct {
  pthread_mutex_t lock;
  pthread_cond_t empty_cond;
  int* queue;
  int len;
} ClientQueue;

bool cq_push(ClientQueue* cq, int clientfd) {
  pthread_mutex_lock(&cq->lock);
  if(cq->len == MAX_FDS) {
    pthread_mutex_unlock(&cq->lock);
    return false;
  }
  if(cq->len == 0) pthread_cond_signal(&cq->empty_cond);
  cq->queue[cq->len++] = clientfd;
  pthread_mutex_unlock(&cq->lock);
  return true;
}

int cq_pop(ClientQueue* cq) {
  pthread_mutex_lock(&cq->lock);
  if(cq->len == 0) {
    pthread_cond_wait(&cq->empty_cond, &cq->lock);
    // If it is still 0, the program ended
    if(cq->len == 0) {
      pthread_mutex_unlock(&cq->lock);
      return -1;
    }
  }
  int fd = cq->queue[0];
  cq->len--;
  memmove(cq->queue, cq->queue+1, cq->len*sizeof(int));
  pthread_mutex_unlock(&cq->lock);
  return fd;
}

typedef struct {
  const Shared* sh;
  ClientQueue* queue;
} WorkerArgs;

static void worker_thread(WorkerArgs* args) {
  HandlerArgs ha_args = ha_new(args->sh, -1);
  while(!should_quit) {
    int clientfd = cq_pop(args->queue);
    if(clientfd == -1) break;
    ha_args.clientfd = clientfd;
    handle_request(&ha_args);
  }
  free(ha_args.buffer);
  free(args);
}

void app(Shared sh) {
  listen(sh.sockfd, NUM_THREADS);
  pthread_t threads[NUM_THREADS];
  ClientQueue queue = {
    .len = 0,
    .queue = calloc(MAX_FDS, sizeof(int)),
    .empty_cond = PTHREAD_COND_INITIALIZER,
    .lock = PTHREAD_MUTEX_INITIALIZER,
  };
  
  printf("Spawning %d threads\n", NUM_THREADS);
  for(int i = 0; i < NUM_THREADS; i++) {
    WorkerArgs* args = malloc(sizeof(WorkerArgs));
    args->sh = &sh;
    args->queue = &queue;
    pthread_create(&threads[i], NULL, (void*)worker_thread, args);
  }
  
  while (!should_quit) {
    log("Awaiting clients...");
    int clientfd = accept(sh.sockfd, NULL, NULL);
    if (clientfd == -1) {
      die("Failed to accept client: %s", strerror(errno));
    }
    cq_push(&queue, clientfd);
  }
  for(int i = 0; i < NUM_THREADS; i++) {
    pthread_cond_broadcast(&queue.empty_cond);
    printf("Joining on %d\n", i);
    pthread_join(threads[i], NULL);
  }
  free(queue.queue);
}
