#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "shared.h"

const int MAX_FDS = 128;

void app(Shared sh) {
  listen(sh.sockfd, MAX_FDS);

  int maxfd = sh.sockfd;
  fd_set active_fds, read_fds;

  FD_ZERO(&active_fds);
  FD_SET(sh.sockfd, &active_fds);

  HandlerArgs ha_args = ha_new(&sh, -1);
  while (!should_quit) {
    read_fds = active_fds;

    int ready = select(maxfd + 1, &read_fds, NULL, NULL, NULL);
    if (ready == -1) {
      if (errno == EINTR) continue;
      die("select() failed: %s", strerror(errno));
    }

    for (int fd = 0; fd <= maxfd; fd++) {
      if (!FD_ISSET(fd, &read_fds)) continue;

      if (fd == sh.sockfd) {
        int clientfd = accept(sh.sockfd, NULL, NULL);
        if (clientfd == -1) {
          die("accept() failed: %s", strerror(errno));
          continue;
        }
        FD_SET(clientfd, &active_fds);
        if (clientfd > maxfd) maxfd = clientfd;
      } else {
        ha_args.clientfd = fd;
        handle_request(&ha_args);

        close(fd);
        FD_CLR(fd, &active_fds);
      }
    }
  }
  free(ha_args.buffer);

  for (int fd = 0; fd <= maxfd; fd++) {
    if (FD_ISSET(fd, &active_fds)) close(fd);
  }
}
