#include "shared.h"
#include "libs/md5.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef CLIENT
#define ARGC 4
#else
#define ARGC 5
#endif

static Shared parse_cli(int argc, char **argv) {
  Shared s;
  memset(&s, 0, sizeof(s));
  // if (argc != ARGC ||
  //     (argc >= 2 && (streq(argv[1], "--help") || streq(argv[1], "-h")))) {
  // }

  s.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  int optval = 1;
  setsockopt(s.sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  s.sock_addr.sin_family = AF_INET;
  s.sock_addr.sin_addr.s_addr = inet_addr(HOST_IP);

  // https://stackoverflow.com/questions/2438471/raw-socket-sendto-failure-in-os-x
  // - ip_len and ip_off must be in host byte order
  // Amo muito quando a apple decide não ser compatível com a speficiação S2
  #ifndef __APPLE__
  s.sock_addr.sin_port = htons(PORT);
  #else
  s.sock_addr.sin_port = PORT;
  #endif

  s.md5 = malloc(sizeof(struct MD5Context));
  MD5Init(s.md5);

  s.packet = malloc(sizeof(UDPPacket));
  return s;
}

void sh_update_hash(Shared sh, uint8_t *bytes, size_t length) {
  MD5Update(sh.md5, bytes, length);
}

char u8tohex(uint8_t u) {
  if (u < 10) {
    return '0' + u;
  } else {
    return 'A' + (u - 10);
  }
}

char* hash_to_human(uint8_t hash[16]) {
  static char human[33] = {[32] = 0};
  for (int i = 0; i < 16; i++) {
    human[i * 2] = u8tohex(hash[i] & 0xF);
    human[i * 2 + 1] = u8tohex(0xF & (hash[i] >> 4));
  }
  return human;
  
}

char *sh_finish_hash(Shared sh, uint8_t out[16]) {
  MD5Final(out, sh.md5);
  MD5Init(sh.md5);
}

static void destroy(Shared sh) {
  close(sh.sockfd);
  free(sh.md5);
  free(sh.packet);
}

bool udp_send(UDP* udp, UDPPacket* packet) {
  sendto(udp->sockfd, packet, packet->size, 0, (void*)&udp->addr, sizeof(udp->addr_len));
}
ssize_t udp_recv(UDP* udp, UDPPacket* packet) {
  size_t byres_read = recvfrom(udp->sockfd, packet, packet->size, 0, (void*)&udp->addr, &udp->addr_len);
  assert(byres_read == packet->size);
}


UDP udp_new(int sockfd) {
  UDP udp;
  bzero(&udp, sizeof(udp));
  udp.sockfd = sockfd;
  udp.addr_len = sizeof(udp.addr);
  return udp;
}

int main(int argc, char **argv) {
  Shared sh = parse_cli(argc, argv);
  app(sh);
  destroy(sh);
}

