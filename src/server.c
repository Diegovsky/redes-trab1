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

void udp(Shared sh) {
  UDPPacket *pack = (UDPPacket *)sh.packet;
  while (1) {
    UDP udp = udp_new(sh.sockfd);

    // wait for client
    log("Waiting for client to connect");
    if (udp_recv(&udp, pack)) {
      print("Recv Error: %s", strerror(errno));
      break;
    }

    uint16_t packet = 0;
    size_t bytes_read = 0;
    while (1) {
      log("Reading file");
      bytes_read = fread(pack->body, 1, sizeof(UDPPacket), stdin);
      log("Read");
      if (bytes_read == 0) {
        break;
      }

      pack->size = UDPPacketSize(bytes_read);
      sh_update_hash(sh, pack->body, bytes_read);

      pack->packet_id = packet;
      ssize_t result;

      log("Sending file");
      result = udp_send(&udp, pack);
      if (result == -1) {
        print("Send Error: %s", strerror(errno));
        break;
      }
      packet++;
    }
    pack->packet_id = -1;
    pack->size = UDPPacketSize(0);
    udp_send(&udp, pack);

    char *f = sh_finish_hash(sh);
    fseek(stdin, 0, SEEK_SET);
    printf("MD5: %s\n", f);
  }
}

void app(Shared sh) {
  if (bind(sh.sockfd, (struct sockaddr *)&sh.sock_addr, sizeof(sh.sock_addr))) {
    char adr[24];
    inet_ntop(AF_INET, (void*)&sh.sock_addr, adr, sizeof(adr)-1);
    die("Failed to bind to address %s:%hd: %s", adr, htons(sh.sock_addr.sin_port),
        strerror(errno));
  }
  udp(sh);
}
