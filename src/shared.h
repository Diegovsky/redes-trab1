#pragma once
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
               PacketType: "%d", \
               uint16_t: "%hd"),                                               \
           arg);                                                               \
    printf("\n");                                                              \
  }
#define streq(a, b) (strcmp(a, b) == 0)
#define BUFSIZE ((size_t) 4096)
#define UDP_HEADER_SIZE (sizeof(UDPPacket) - BUFSIZE)
#define UDPPacketSize(num) (UDP_HEADER_SIZE + num)

typedef uint16_t u16;
typedef uint8_t u8;

static const char HOST_IP[] = "127.0.0.1";
static const u16 PORT = 1025;

typedef enum {
  PACKET_DATA = 0,
  PACKET_ACK,
  PACKET_RETRY,
  PACKET_CLOSE,
  PACKET_INIT,
} PacketType;

typedef enum {
  RECV_OK = 0,
  RECV_CLOSE,
  RECV_TIMEOUT,
  RECV_ERR,
} RecvResult;

typedef struct {
  u16 size;
  u16 packet_id;
  u16 checksum;
  PacketType type;
  u8 body[BUFSIZE];
} UDPPacket;

typedef struct {
  socklen_t addr_len;
  struct sockaddr_in addr;
  UDPPacket* packet;
  u16 packet_id;
  int sockfd;
} UDP;

char* inet_to_human(struct sockaddr_in* addr);
bool udp_connect(UDP* udp);
RecvResult udp_listen(UDP* udp);
void udp_close(UDP* udp);
bool udp_send(UDP* udp, u16 size);
RecvResult udp_recv(UDP* udp);

typedef struct {
  bool verbose;
  float error_percent;

  struct MD5Context *md5;

  int sockfd;
  struct sockaddr_in sock_addr;

  UDPPacket* packet;
  UDP udp;
} Shared;


void app(Shared sh);
