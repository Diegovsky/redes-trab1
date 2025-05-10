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

void tcp(Shared sh) {
  listen(sh.sockfd, 1);
  while (1) {
    int bytes_read;
    int client = accept(sh.sockfd, NULL, NULL);
    while ((bytes_read = fread(sh.buffer, 1, BUFSIZE, sh.file)) != 0) {
      ssize_t h = send(client, sh.buffer, bytes_read, 0);
      if (h == 0) {
        print("Client disconnected");
        break;
      } else if (h == -1) {
        die("Send Error: %s", strerror(errno));
      }
      sh_update_hash(sh, sh.buffer, bytes_read);
    }
    fseek(sh.file, 0, SEEK_SET);
    close(client);
    char *f = sh_finish_hash(sh);
    fseek(sh.file, 0, SEEK_SET);
    printf("MD5: %s\n", f);
  }
}

void udp(Shared sh) {
  UDPPacket *pack = (UDPPacket *)sh.buffer;
  while (1) {
    size_t bytes_read = 0;
    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    bzero(&client_addr, client_size);

    uint16_t packet = 0;

    while (1) {
      if (recvfrom(sh.sockfd, NULL, 0, 0, (void *)&client_addr, &client_size) ==
          -1) {
        print("Recv Error: %s", strerror(errno));
        break;
      }

      bytes_read = fread(pack->body, 1, BUFSIZE, sh.file);
      if (bytes_read == 0) {
        break;
      }

      sh_update_hash(sh, pack->body, bytes_read);

      pack->packet_id = packet;
      ssize_t result;

      result = sendto(sh.sockfd, pack, UDPPacketSize(bytes_read), 0,
                      (void *)&client_addr, client_size);
      if (result == -1) {
        print("Send Error: %s", strerror(errno));
        break;
      }
      packet++;
    }
    pack->packet_id = -1;
    sendto(sh.sockfd, pack, UDPPacketSize(0), 0, (void *)&client_addr,
           client_size);

    char *f = sh_finish_hash(sh);
    fseek(sh.file, 0, SEEK_SET);
    printf("MD5: %s\n", f);
  }
}

void app(Shared sh) {
  if (bind(sh.sockfd, (struct sockaddr *)&sh.sock_addr, sizeof(sh.sock_addr))) {
    die("Failed to bind to address %s:%hd: %s", sh.hostname, sh.port,
        strerror(errno));
  }
  if (sh.is_udp) {
    udp(sh);
  } else {
    tcp(sh);
  }
}
