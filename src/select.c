#include "shared.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

void app(Shared sh) {
  listen(sh.sockfd, 10);

  fd_set read_fds;
  FD_ZERO(&read_fds);
  int fdmax = sh.sockfd;

  while (!should_quit) {
    log("Awaiting clients...");
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};
    select(fdmax, &read_fds, NULL, NULL, &timeout);

    int clientfd = accept(sh.sockfd, NULL, NULL);
    if (clientfd == -1) {
      die("Failed to accept client: %s", strerror(errno));
    }
    HandlerArgs args = {
        .sh = &sh, .clientfd = clientfd, .error = false, .err_no = 0};
    handle_request(&args);
    FD_SET(sh.sockfd, &read_fds);
  }
}
