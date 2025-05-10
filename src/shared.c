#include "shared.h"
#include "libs/md5.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
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
  if (argc != ARGC ||
      (argc >= 2 && (streq(argv[1], "--help") || streq(argv[1], "-h")))) {
#ifdef CLIENT
    die("USAGE: %s [IP] [PORT] <tcp|udp>", argv[0]);
#else
    die("USAGE: %s [IP] [PORT] <tcp|udp> [FILENAME]", argv[0]);
#endif
  }
  s.hostname = argv[1];
  s.port = (short)atoi(argv[2]);
  s.protocol_name = argv[3];

#ifndef CLIENT
  s.filename = argv[4];
#endif

  int protocol;
  if (streq(s.protocol_name, "udp")) {
    protocol = SOCK_DGRAM;
    s.is_udp = true;
  } else if (streq(s.protocol_name, "tcp")) {
    protocol = SOCK_STREAM;
    s.is_udp = false;
  } else {
    die("Expected either `tcp` or `udp`. Got: `%s`", s.protocol_name);
  }
#ifndef CLIENT
  s.file = fopen(s.filename, "rb");
  if (s.file == NULL) {
    die("Failed to open file %s: %s", s.filename, strerror(errno));
  }
#endif

  s.sockfd = socket(AF_INET, protocol, 0);
  int optval = 1;
  setsockopt(s.sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  s.sock_addr.sin_family = AF_INET;
  s.sock_addr.sin_addr.s_addr = inet_addr(s.hostname);

  // https://stackoverflow.com/questions/2438471/raw-socket-sendto-failure-in-os-x
  // - ip_len and ip_off must be in host byte order
  // Amo muito quando a apple decide não ser compatível com a speficiação S2
  #ifndef __APPLE__
  s.sock_addr.sin_port = htons(s.port);
  #else
  s.sock_addr.sin_port = s.port;
  #endif

  s.md5 = malloc(sizeof(struct MD5Context));
  MD5Init(s.md5);

  size_t bufsize = BUFSIZE;
  if (s.is_udp) {
    bufsize += sizeof(UDPPacket);
  }
  s.buffer = malloc(bufsize);
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

char *sh_finish_hash(Shared sh) {
  uint8_t out[16];
  MD5Final(out, sh.md5);
  MD5Init(sh.md5);
  static char human[33] = {[32] = 0};
  for (int i = 0; i < 16; i++) {
    human[i * 2] = u8tohex(out[i] & 0xF);
    human[i * 2 + 1] = u8tohex(0xF & (out[i] >> 4));
  }
  return human;
}

static void destroy(Shared sh) {
#ifndef CLIENT
  fclose(sh.file);
#endif
  close(sh.sockfd);
  free(sh.md5);
  free(sh.buffer);
}

int main(int argc, char **argv) {
  Shared sh = parse_cli(argc, argv);
  app(sh);
  destroy(sh);
}
