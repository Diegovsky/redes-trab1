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
  UDPPacket *pack = (UDPPacket *)sh.buffer;
  pack->packet_id = 0;

  send(sh.sockfd, NULL, 0, 0);

  ssize_t bytes_read = 0, total_bytes_read = 0;
  uint16_t expected_packet = 0, packets_lost = 0;
  while (1) {
    bytes_read = recv(sh.sockfd, pack, UDPPacketSize(BUFSIZE), 0);
    if (pack->packet_id == (uint16_t)-1) {
      break;
    }
    if (bytes_read == -1) {
      die("Error: %s", strerror(errno));
    }
    total_bytes_read += bytes_read;
    bytes_read -= UDPPacketSize(0);

    if (pack->packet_id != expected_packet) {
      packets_lost++;
      expected_packet = pack->packet_id;
    }
    if (packets_lost == 0) {
      sh_update_hash(sh, pack->body, bytes_read);
    }
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

size_t tcp(Shared sh) {
  ssize_t bytes_read, total_bytes_read = 0;

  while (1) {
    bytes_read = recv(sh.sockfd, sh.buffer, BUFSIZE, 0);
    if (bytes_read == 0)
      break;
    if (bytes_read == -1) {
      print("Server error");
      break;
    }
    total_bytes_read += bytes_read;

    sh_update_hash(sh, sh.buffer, bytes_read);
  }
  char *f = sh_finish_hash(sh);
  print("MD5: %s", f);

  return total_bytes_read;
}

void app(Shared sh) {
  if (connect(sh.sockfd, (struct sockaddr *)&sh.sock_addr,
              sizeof(sh.sock_addr))) {
    die("Failed to connect to the server: %s", strerror(errno));
  }

  struct timespec before;
  clock_gettime(CLOCK_MONOTONIC, &before);

  size_t total_bytes_read = sh.is_udp ? udp(sh) : tcp(sh);
  print("Total bytes: %ld", total_bytes_read);

  double total_time = time_elapsed(&before);
  print_download(total_bytes_read / total_time);
}
