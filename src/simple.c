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
  listen(sh.sockfd, 1);
  while (!should_quit) {
    log("Awaiting client...");
    int clientfd = accept(sh.sockfd, NULL, NULL);
    if (clientfd == -1) {
      die("Failed to accept client: %s", strerror(errno));
    }
    HandlerArgs args = ha_new(&sh, clientfd, NULL);
    handle_request(&args);
  }
}
