#include "shared.h"
#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <semaphore.h>

const int NUM_THREADS = 16;

typedef struct {
  HandlerArgs args;
  sem_t* semaphore;
}TrampArgs; 

static void handle_request_trampoline(TrampArgs *args) {
  handle_request(&args->args);
  sem_post(args->semaphore);
  free(args->args.buffer);
  free(args);
  pthread_exit(NULL);
}

void app(Shared sh) {
  listen(sh.sockfd, NUM_THREADS);
  
  sem_t semaphore;
  sem_init(&semaphore, false, NUM_THREADS);
  
  while (!should_quit) {
    log("Awaiting clients...");
    int clientfd = accept(sh.sockfd, NULL, NULL);
    if (clientfd == -1) {
      die("Failed to accept client: %s", strerror(errno));
    }
    sem_wait(&semaphore);
    TrampArgs* args = malloc(sizeof(TrampArgs));
    args->args = ha_new(&sh, clientfd);
    args->semaphore = &semaphore;

    pthread_t t;
    pthread_create(&t, NULL, (void*)handle_request_trampoline, args);
    pthread_detach(t);
  }
  for(int i = 0; i < NUM_THREADS; i++) {
    sem_wait(&semaphore);
  }
  sem_destroy(&semaphore);
}
