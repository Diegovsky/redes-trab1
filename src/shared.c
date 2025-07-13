#include "shared.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

char* inet_to_human(struct sockaddr_in* addr) {
  static char adr[64];
  inet_ntop(AF_INET, (void*)&addr->sin_addr, adr, sizeof(adr)-1);
  sprintf(adr, "%s:%hd", adr, ntohs(addr->sin_port));
  return adr;

}

UDP udp_new(int sockfd, struct sockaddr_in addr, socklen_t addr_len, UDPPacket* packet) {
  UDP udp;
  bzero(&udp, sizeof(udp));
  udp.packet = packet;
  udp.packet_id = 0;
  udp.sockfd = sockfd;
  udp.addr = addr;
  udp.addr_len = addr_len;
  return udp;
}
static Shared parse_cli(int argc, char **argv) {
  Shared s;
  memset(&s, 0, sizeof(s));
  s.sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  struct timeval timeout;
  timeout.tv_sec = 1;
  // 5 ms
  timeout.tv_usec = 5000;
  setsockopt(s.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  s.sock_addr.sin_family = AF_INET;
  s.sock_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, HOST_IP, &s.sock_addr.sin_addr);

  s.packet = calloc(sizeof(UDPPacket), 1);
  s.udp = udp_new(s.sockfd, s.sock_addr, sizeof(s.sock_addr), s.packet);
  return s;
}


static void destroy(Shared sh) {
  close(sh.sockfd);
  free(sh.packet);
}

static void checksum(UDPPacket *packet) {
  // otherwise could give wrong results;
  packet->checksum = 0;
  int16_t sum = 0;
  for(size_t i = 0; i < packet->size; i++) {
    sum += ((u8*)packet)[i];
  }
  packet->checksum = sum;
}

static bool checksum_match(UDPPacket* packet) {
  u16 expeted_checksum = packet->checksum;
  checksum(packet);
  return expeted_checksum == packet->checksum;
  
}

static bool send2(UDP* udp, const UDPPacket* pack) {
  assert(pack->size >= UDP_HEADER_SIZE);
  log("Sending payload of %hd bytes", pack->size);
  return sendto(udp->sockfd, pack, pack->size, 0, (void*)&udp->addr, udp->addr_len) != -1;
}
static bool recv2(UDP* udp, UDPPacket* pack) {
  log("Recv: %s", inet_to_human(&udp->addr));
  udp->addr_len = sizeof(udp->addr);
  ssize_t bytes_read = recvfrom(udp->sockfd, pack, sizeof(UDPPacket), 0, (void*)&udp->addr, &udp->addr_len);
  if(bytes_read == -1) return false;
  assert(bytes_read == pack->size);
  return true;
}

static void packet_prepare(UDPPacket* pack, PacketType type, u16 id, u16 size) {
  assert(size >= UDP_HEADER_SIZE);
  assert(size <= sizeof(UDPPacket));
  pack->type = type;
  pack->packet_id = id;
  pack->size = size;
  checksum(pack);
}

static RecvResult raw_recv(UDP* udp, u16 expected_id, u16 expected_type) {
  #define retry() { retries++; log("Dropping packet and retrying..."); goto retry; }
  int retries = 0;
  UDPPacket* packet = udp->packet;
  retry:
  if(retries >= 2) return RECV_ERR;
  if(!recv2(udp, packet)) {
    if(errno == EAGAIN) return RECV_TIMEOUT;
    else return RECV_ERR;
  }

  if(!checksum_match(packet)) {
    log("Checksum does not match!");
    retry();
  }

  if(packet->packet_id == 0 && packet->type == PACKET_CLOSE) {
    log("Connection closed");
    return RECV_CLOSE;
  }

  if(packet->packet_id < expected_id) {
    log("Got duplicate packet");
    retry();
  }

  if(packet->packet_id != expected_id) {
    log("Expected packet id %hd, got %hd", expected_id, packet->packet_id);
    retry();
  }

  if(packet->type != expected_type) {
    log("Got unexpected packet type");
    retry();
  }

  UDPPacket* ack = alloca(UDP_HEADER_SIZE);
  packet_prepare(ack, PACKET_ACK, packet->packet_id, UDP_HEADER_SIZE);
  if(!send2(udp, ack)) {
    return RECV_ERR;
  };

  #undef retry
  return RECV_OK;
}

static bool raw_send(UDP* udp, PacketType type, u16 id, u16 size) {
  #define retry() { log("Retrying..."); goto retry; }
  UDPPacket* packet = udp->packet;
  retry:
  packet_prepare(packet, type, id, size);
  // send packet
  if(!send2(udp, packet)) return false;

  // Make sure it arrived
  u16 pack_id = packet->packet_id; 
  if(!recv2(udp, packet)) {
    // timer expired, try again;
    if(errno == EAGAIN) {
      log("Window expired");
      retry();
    }
    // error occoured
    else return false;
  }

  if(!checksum_match(packet)) {
    log("Checksum does not match");
    retry();
  }
  if(packet->type != PACKET_ACK) {
    log("Expected ACK, got something else");
    retry();
  }
  if(packet->packet_id != pack_id) {
    log("Expecting ACK for packet id %hd, got %hd", pack_id, packet->packet_id);
    retry();
  }

  #undef retry
  return true;
}

bool udp_connect(UDP* udp) {
  return raw_send(udp, PACKET_INIT, 0, UDP_HEADER_SIZE);
}

RecvResult udp_listen(UDP *udp) {
  return raw_recv(udp, 0, PACKET_INIT);
}

void udp_close(UDP* udp) {
  packet_prepare(udp->packet, PACKET_CLOSE, 0, UDP_HEADER_SIZE);
  send2(udp, udp->packet);
  udp->packet_id = 0;
};

bool udp_send(UDP *udp, u16 size) {
  bool success = raw_send(udp, PACKET_DATA, udp->packet_id, size);
  if(success) {
    udp->packet_id++;
  }

  return success;
}


RecvResult udp_recv(UDP *udp) {
  RecvResult res = raw_recv(udp, udp->packet_id, PACKET_DATA);
  if(res == RECV_OK) {
    udp->packet_id++;
  }
  return res;
}

int main(int argc, char **argv) {
  Shared sh = parse_cli(argc, argv);
  app(sh);
  destroy(sh);
}

