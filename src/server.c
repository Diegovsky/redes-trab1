#include "shared.h"
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

void app(Shared sh) {
  if(bind(sh.sockfd, (struct sockaddr*)&sh.sock_addr, sizeof(sh.sock_addr))) {
    die("Failed to bind to address %s:%hd: %s", sh.hostname, sh.port, strerror(errno));
  }
  listen(sh.sockfd, 1);
  int bytes_read;
  while(1) {
    
    int client;
    for(client = accept(sh.sockfd, nullptr, nullptr);
        (bytes_read = fread(sh.buffer, 1, BUFSIZE, sh.file)) != 0;
      )

    {
      printf("Sending %d bytes\n", bytes_read);
      sh_update_hash(sh, bytes_read);
      send(client, sh.buffer, bytes_read, 0);
    }
    char* f = sh_finish_hash(sh);
    fseek(sh.file, 0, SEEK_SET);
    printf("MD5 = %s\n", f);
    close(client);
  }
}
