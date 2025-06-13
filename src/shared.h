#pragma once
#include <arpa/inet.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#define log(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define die(fmt, ...)                                                          \
  {                                                                            \
    fprintf(stderr, fmt "\n", ##__VA_ARGS__);                                  \
    exit(1);                                                                   \
  }
#define print(fmt, ...) printf(fmt "\n", ##__VA_ARGS__);
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
#define addr(sock) ((void *)&sock), sizeof(sock)

static const size_t BUFSIZE = 4096;
#define UDPPacketSize(num) (sizeof(UDPPacket) + num)

extern bool should_quit;

typedef struct {
  char *hostname;
  short port;
  char *filename;

  int sockfd;
  struct sockaddr_in sock_addr;
} Shared;

typedef struct {
  const Shared *sh;
  int clientfd;
  uint8_t *buffer;

  // out
  bool error;
  int err_no;
} HandlerArgs;

void app(Shared sh);

HandlerArgs ha_new(const Shared *sh, int clientfd);
void handle_request(HandlerArgs *args);
