#include "shared.h"
#include "libs/md5.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

static Shared parse_cli(int argc, char** argv) {
  Shared s = {};
  if (argc != 5 ||
      (argc >= 2 &&
       (streq(argv[1], "--help") || streq(argv[1], "-h")))) {
    die("USAGE: %s [HOST] [PORT] [FILENAME] <tcp|udp>", argv[0]);
  }
  s.hostname = argv[1];
  s.port = (short)atoi(argv[2]);
  s.filename = argv[3];
  s.protocol_name = argv[4];

  int protocol;
  if(streq(s.protocol_name, "udp")) {
    protocol = SOCK_DGRAM;
    s.is_tcp = false;
  } else if(streq(s.protocol_name, "tcp")) {
    protocol = SOCK_STREAM;
    s.is_tcp = true;
  } else {
    die("Expected either `tcp` or `udp`. Got: `%s`", s.protocol_name);
  }
  s.file = fopen(s.filename, "rb");
  if(s.file == NULL) {
    die("Failed to open file %s: %s", s.filename, strerror(errno));
  }

  s.sockfd = socket(AF_INET, protocol, 0);
  int optval = 1;
  setsockopt(s.sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  s.sock_addr.sin_family = AF_INET;
  s.sock_addr.sin_addr.s_addr = inet_addr(s.hostname);
  s.sock_addr.sin_port = htons(s.port);

  s.md5 = malloc(sizeof(struct MD5Context));
  MD5Init(s.md5);

  s.buffer = malloc(BUFSIZE);
  return s;
}


void sh_update_hash(Shared sh, int bytes_read) {
  MD5Update(sh.md5, sh.buffer, bytes_read);
}

char u8tohex(uint8_t u) {
  if(u < 10) {
    return '0' + u;
  } else {
    return 'A' + (u-10);
  }
}

char* sh_finish_hash(Shared sh) {
  uint8_t out[16];
  MD5Final(out, sh.md5);
  MD5Init(sh.md5);
  static char human[33] = {};
  for(int i = 0; i < 16; i++) {
    human[i*2] = u8tohex(out[i] & 0xF);
    human[i*2 + 1] = u8tohex(0xF & (out[i] >> 4));
  }
  return human;
}

static void destroy(Shared sh) {
  fclose(sh.file);
  close(sh.sockfd);
  free(sh.md5);
  free(sh.buffer);
}

int main(int argc, char** argv) {
  Shared sh = parse_cli(argc, argv);
  app(sh);
  destroy(sh);
}
