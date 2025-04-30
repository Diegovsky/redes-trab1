#include "shared.h"
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

void app(Shared sh) {
  if(connect(sh.sockfd, (struct sockaddr*)&sh.sock_addr, sizeof(sh.sock_addr))) {
    die("Failed to connect to the server: %s", strerror(errno));
  }
  for(
    int bytes_read;
    (bytes_read = recv(sh.sockfd, sh.buffer, BUFSIZE, 0)) != 0;

  ) {
      printf("Got %d bytes\n", bytes_read);
      sh_update_hash(sh, bytes_read);
    }

    char* f = sh_finish_hash(sh);
    printf("MD5 = %s\n", f);
}
