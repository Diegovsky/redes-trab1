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
#define fprint(file, fmt, ...) fprintf(file, fmt "\n", ##__VA_ARGS__);
#define print(fmt, ...) fprint(stdout, fmt, ##__VA_ARGS__);
#define log(fmt, ...) if(verbose) fprint(stdlog, "[+] "fmt, ##__VA_ARGS__)
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

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

static const char HOST_IP[] = "127.0.0.1";
static const u16 PORT = 1025;

extern float fail_percent;
extern bool verbose;


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
  UDPPacket* ack_packet;
  UDPPacket* packet;
  u16 packet_id;
  int sockfd;
  // stats
  u32 total_bytes_sent;
  u32 total_bytes_received;
  u32 packets_lost;
  u32 packets_duplicated;
  u32 packets_resent;
} UDP;

char* bytes_to_human(u32 byte_count);
char* inet_to_human(struct sockaddr_in* addr);
bool udp_connect(UDP* udp);
RecvResult udp_listen(UDP* udp);
void udp_close(UDP* udp);
bool udp_send(UDP* udp, u16 size);
RecvResult udp_recv(UDP* udp);
void udp_print_stats(UDP* udp);

typedef struct {
  float error_percent;

  int sockfd;
  struct sockaddr_in sock_addr;

  UDP udp;
} Shared;


void app(Shared sh);
