#include "shared.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

const int SLEEP = 2;

void udp(Shared sh) {
  UDPPacket *pack = (UDPPacket *)sh.packet;
  UDP udp_original = sh.udp;
  while (1) {
    UDP udp_copy = udp_original;
    UDP* udp = &udp_copy;
    // wait for client
    log("Waiting for client to connect");
    switch(udp_listen(udp)) {
      case RECV_ERR:
        die("Recv Error: %s", strerror(errno));
        break;
      case RECV_CLOSE:
        log("Client closed the connection");
        continue;
      case RECV_TIMEOUT:
        log("No client, sleeping for %d second(s)", SLEEP);
        sleep(SLEEP);
        continue;
      case RECV_OK:
        log("Client connected!");
        break;
    }

    size_t bytes_read = 0;
    while (1) {
      log("Reading file");
      bytes_read = fread(pack->body, 1, BUFSIZE, stdin);
      log("Read %ld bytes", bytes_read);
      if (bytes_read == 0) {
        break;
      }

      log("Sending file");
      if(!udp_send(udp, UDPPacketSize(bytes_read))) {
        log("Send Error: %s", strerror(errno));
        break;
      }
    }

    udp_close(udp);
  }
}

void app(Shared sh) {
  if (bind(sh.sockfd, (struct sockaddr *)&sh.sock_addr, sizeof(sh.sock_addr)) == -1) {
    
    die("Failed to bind to address %s:%hd: %s", inet_to_human(&sh.sock_addr), htons(sh.sock_addr.sin_port),
        strerror(errno));
  }
  udp(sh);
}
