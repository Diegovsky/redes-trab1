#pragma once
#include "libs/md5.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define die(fmt, ...)                                                          \
  {                                                                            \
    fprintf(stderr, fmt "\n", ##__VA_ARGS__);                                  \
    exit(1);                                                                   \
  }
#define print(fmt, ...) printf(fmt "\n", ##__VA_ARGS__);
#define c(a, b) a b
#define dbg(arg)                                                               \
  {                                                                            \
    printf(#arg " = ");                                                        \
    printf(_Generic(arg,                                                       \
               int: "%d",                                                      \
               size_t: "%ld",                                                  \
               ssize_t: "%ld",                                                 \
               double: "%lf",                                                  \
               uint16_t: "%hd"),                                               \
           arg);                                                               \
    printf("\n");                                                              \
  }
#define streq(a, b) (strcmp(a, b) == 0)

static const size_t BUFSIZE = 4096;
#define UDPPacketSize(num) (sizeof(UDPPacket) + num)

typedef struct {
  char *hostname;
  short port;
  char *filename;
  char *protocol_name;

  struct MD5Context *md5;
  bool is_udp;
  FILE *file;
  int sockfd;
  uint8_t *buffer;
  struct sockaddr_in sock_addr;
} Shared;

typedef struct {
  uint16_t packet_id;
  uint8_t body[];
} UDPPacket;

void sh_update_hash(Shared sh, uint8_t *bytes, size_t length);
char *sh_finish_hash(Shared sh);

void app(Shared sh);
