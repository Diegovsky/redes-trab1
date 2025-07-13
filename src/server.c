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

static void receive_files(Shared* sh) {
  UDPPacket *pack = sh->udp.packet;
  UDP udp_original = sh->udp;
  while (1) {
    UDP udp_copy = udp_original;
    UDP* udp = &udp_copy;
    // wait for client
    log("Waiting for client to connect");
    switch(udp_listen(udp)) {
      case RECV_ERR:
        log("Recv Error: %s", strerror(errno));
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

    while (1) {
      log("Waiting for client's chunk");
      RecvResult res = udp_recv(udp);
      if(res != RECV_OK) {
        switch(res) {
          case RECV_ERR:
            log("Failed to read from client. Giving up");
            break;
          case RECV_TIMEOUT:
            log("Timed out waiting for response");
            break;
          case RECV_CLOSE:
            log("Server closed the connection");
          default:
            break;
        }
        log("Server gave up on client");
        log("-----------");
        break;
      }
      log("Got %hd bytes from client", pack->size);
    }
    udp_close(udp);
    udp_print_stats(udp);
  }
}

void app(Shared sh) {
  if (bind(sh.sockfd, (struct sockaddr *)&sh.sock_addr, sizeof(sh.sock_addr)) == -1) {
    die("Failed to bind to address %s: %s", inet_to_human(&sh.sock_addr), strerror(errno));
  }
  receive_files(&sh);
  udp_print_stats(&sh.udp);
}
