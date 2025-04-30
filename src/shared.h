#pragma once
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "libs/md5.h"
#include <stdlib.h>

#define die(fmt, ...) { fprintf(stderr, fmt "\n" __VA_OPT__(,) __VA_ARGS__); exit(1); }
#define streq(a, b) (strcmp(a, b) == 0)

#define BUFSIZE 1024

typedef struct {
  char* hostname;
  short port;
  char* filename;
  char* protocol_name;

  struct MD5Context* md5;
  bool is_tcp;
  FILE* file;
  int sockfd;
  uint8_t* buffer;
  struct sockaddr_in sock_addr;
} Shared;

void sh_update_hash(Shared sh, int bytes_read);
char* sh_finish_hash(Shared sh);

void app(Shared sh);


