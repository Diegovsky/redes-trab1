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

const int NUM_THREADS = 16;

static int handle_request_trampoline(HandlerArgs *args) {
  handle_request(args);
  atomic_store(args->filled, false);
  free(args->buffer);
  free(args);
  return 0;
}

void app(Shared sh) {
  listen(sh.sockfd, NUM_THREADS);
  _Atomic bool slots[NUM_THREADS];
  memset(slots, 0, sizeof(slots));

  while (!should_quit) {
    log("Awaiting clients...");
    int clientfd = accept(sh.sockfd, NULL, NULL);
    if (clientfd == -1) {
      die("Failed to accept client: %s", strerror(errno));
    }
    int free_slot = -1;
    for (int i = 0; i < NUM_THREADS; i++) {
      if (!atomic_load(&slots[i])) {
        free_slot = i;
        atomic_store(&slots[i], true);
        break;
      }
    }
    if(free_slot == -1) {
      log("No threads available.");
      continue;
    }
    HandlerArgs* args = malloc(sizeof(HandlerArgs));
    *args = ha_new(&sh, clientfd, &slots[free_slot]);

    pthread_t th;
    pthread_create(&th, NULL, (void*)handle_request_trampoline, args);
  }
}
