#pragma once
#include "libs/md5.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define die(fmt, ...)                                                          \
  {                                                                            \
    fprintf(stderr, fmt "\n", ##__VA_ARGS__);                                  \
    exit(1);                                                                   \
  }
#define stdlog stderr
#define print(fmt, ...) printf(fmt "\n", ##__VA_ARGS__);
#define eprint(fmt, ...) fprintf(stdlog, fmt "\n", ##__VA_ARGS__);
#define log(fmt, ...) eprint("[+] "fmt, ##__VA_ARGS__)
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
#define BUFSIZE ((size_t) 4096)
#define UDP_HEADER_SIZE (sizeof(UDPPacket) - BUFSIZE)
#define UDPPacketSize(num) (UDP_HEADER_SIZE + num)

static const char HOST_IP[] = "127.0.0.1";
static const uint16_t PORT = 1025;

typedef enum {
  PACKET_DATA = 0,
  PACKET_ACK,
  PACKET_RETRY,
  PACKET_CLOSE,
  PACKET_INIT,
} PacketType;

typedef struct {
  uint16_t size;
  uint16_t packet_id;
  uint8_t hash[16];
  PacketType type;
  union {
    uint16_t retry_id;
    uint8_t body[BUFSIZE];
  };
} UDPPacket;

typedef struct {
  bool verbose;
  float error_percent;

  struct MD5Context *md5;

  int sockfd;
  struct sockaddr_in sock_addr;

  UDPPacket* packet;
} Shared;

typedef struct {
  socklen_t addr_len;
  struct sockaddr_storage addr;
  int sockfd;
} UDP;

char* hash_to_human(uint8_t hash[16]);

void sh_update_hash(Shared sh, uint8_t *bytes, size_t length);
char *sh_finish_hash(Shared sh, uint8_t out[16]);

UDP udp_new(int sockfd);
bool udp_send(UDP* udp, UDPPacket* packet);
ssize_t udp_recv(UDP* udp, UDPPacket* packet);


void app(Shared sh);
