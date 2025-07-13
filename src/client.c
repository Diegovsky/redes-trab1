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


static void send_file(Shared* sh) {
  UDPPacket *pack = sh->udp.packet;
  UDP* udp = &sh->udp;
  log("Telling server I'm connected");
  udp_connect(udp);

  while (1) {
    u16 bytes_read = fread(pack->body, 1, BUFSIZE, stdin);
    if(bytes_read == 0 && feof(stdin)) break;
    if(!udp_send(udp, UDPPacketSize(bytes_read))) {
      log("Error trying to talk to server: %s", strerror(errno));
      break;
    }
  }

  udp_close(udp);
}

void app(Shared sh) {
  if (connect(sh.sockfd, (struct sockaddr *)&sh.sock_addr,
              sizeof(sh.sock_addr))) {
    die("Failed to connect to the server: %s", strerror(errno));
  }

  struct timespec before;
  clock_gettime(CLOCK_MONOTONIC, &before);

  send_file(&sh);
  udp_print_stats(&sh.udp);

  double total_time = time_elapsed(&before);
  print("Tempo total: %lfs", total_time);
  print("Velocidade de Upload: %sb/s", bytes_to_human(sh.udp.total_bytes_sent / total_time));
}
