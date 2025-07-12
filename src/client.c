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

size_t udp(Shared sh) {
  UDPPacket *pack = sh.packet;

  UDP udp = udp_new(sh.sockfd);
  udp.addr_len = sizeof(sh.sock_addr);
  memcpy(&udp.addr, &sh.sock_addr, udp.addr_len);

  log("Telling server I'm connected");
  pack->packet_id = 0;
  pack->size = UDPPacketSize(0);
  udp_send(&udp, pack);

  ssize_t bytes_read = 0, total_bytes_read = 0;
  uint16_t expected_packet = 0, packets_lost = 0;
  while (1) {
    log("Reading chunk from server");
    bytes_read = udp_recv(&udp, pack);
    log("Read from the server");

    if (bytes_read == -1) {
      die("Error reading from server: %s", strerror(errno));
    }

    if (pack->type == PACKET_CLOSE) {
      log("Server closed the connection");
      break;
    }

    total_bytes_read += bytes_read;
    bytes_read -= UDP_HEADER_SIZE;

    fwrite(pack->body, bytes_read, 1, stdlog);
    sh_update_hash(sh, pack->body, bytes_read);
    expected_packet++;
  }

  print("Packets Lost: %d", packets_lost);

  if (packets_lost == 0) {
    char *f = sh_finish_hash(sh);
    print("MD5: %s", f);
  } else {
    print("MD5: null");
  }

  return (size_t)total_bytes_read;
}

void app(Shared sh) {
  if (connect(sh.sockfd, (struct sockaddr *)&sh.sock_addr,
              sizeof(sh.sock_addr))) {
    die("Failed to connect to the server: %s", strerror(errno));
  }

  struct timespec before;
  clock_gettime(CLOCK_MONOTONIC, &before);

  size_t total_bytes_read = udp(sh);
  print("Total bytes: %ld", total_bytes_read);

  double total_time = time_elapsed(&before);
  print_download(total_bytes_read / total_time);
}
