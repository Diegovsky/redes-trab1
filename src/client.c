#define _GNU_SOURCE
#include "shared.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

const double SECOND_IN_NANOSECONDS = 1000000000;

double time_elapsed(struct timespec *before) {

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  double time = now.tv_sec - before->tv_sec;
  time += (now.tv_nsec - before->tv_nsec) / SECOND_IN_NANOSECONDS;
  *before = now;
  return time;
}

void print_download(double speed) {
  char *units[] = {"", "K", "M", "G", "T"};
  char *postfix;
  for (int i = 0; i < (int)(sizeof(units) / sizeof(units[0])); i++) {
    postfix = units[i];
    if (speed < 1024) {
      break;
    }
    speed /= 1024;
  }
  print("Download Speed: %.2lf%sb/s", speed, postfix);
}

void udp(Shared sh) {
  UDPPacket *pack = sh.packet;
  UDP* udp = &sh.udp;
  log("Telling server I'm connected");
  udp_connect(udp);

  while (1) {
    log("Reading chunk from server");
    RecvResult res = udp_recv(udp);
    switch(res) {
      case RECV_ERR:
        die("Failed to read from the server");
      case RECV_TIMEOUT:
        log("Timed out waiting for response");
        continue;
      case RECV_CLOSE:
        log("Server closed the connection");
        goto exit;
      case RECV_OK:
        break;
    }
    log("Read from the server:");
    fwrite(pack->body, pack->size, 1, stdlog);
  }

  exit:
  return;
}

void app(Shared sh) {
  if (connect(sh.sockfd, (struct sockaddr *)&sh.sock_addr,
              sizeof(sh.sock_addr))) {
    die("Failed to connect to the server: %s", strerror(errno));
  }

  struct timespec before;
  clock_gettime(CLOCK_MONOTONIC, &before);

  udp(sh);
  // print("Total bytes: %ld", total_bytes_read);

  // double total_time = time_elapsed(&before);
  // print_download(total_bytes_read / total_time);
}
