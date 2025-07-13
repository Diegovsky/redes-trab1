#include "shared.h"

#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define VRGCLI
#include "vrg.h"

bool verbose = false;
float fail_percent = 0.0;

char* inet_to_human(struct sockaddr_in* addr) {
  static char adr[25];
  inet_ntop(AF_INET, (void*)&addr->sin_addr, adr, sizeof(adr)-1);
  static char buf[sizeof(adr)+8];
  snprintf(buf, sizeof(buf), "%s:%hd", adr, ntohs(addr->sin_port));
  return buf;

}

UDP udp_new(int sockfd, struct sockaddr_in addr, socklen_t addr_len) {
  UDP udp;
  bzero(&udp, sizeof(udp));
  udp.packet = calloc(1, sizeof(UDPPacket));
  udp.ack_packet = calloc(1, sizeof(UDPPacket));
  udp.packet_id = 0;
  udp.sockfd = sockfd;
  udp.addr = addr;
  udp.addr_len = addr_len;
  return udp;
}
static Shared parse_cli(int argc, char **argv) {
  Shared s;
  bzero(&s, sizeof(s));
  vrgcli("Trabalho de Redes 3") {
    vrgarg("-h, --help\tMostrar informações de uso") {
      vrgusage();
    }
    vrgarg("-v, --verbose\tAtivar modo verboso") {
      verbose = true;
    }
    vrgarg("-f, --fail percent\tProbabilidade do pacote se perder (0.0-1.0) [PADRÃO: 0.0]") {
      fail_percent = atof(vrgarg);
    }
    vrgarg() {
      if (vrgarg[0] == '-') {
        vrgusage("ERROR: flag desconhecida '%s'\n", vrgarg);
      } else {
        vrgusage("ERROR: não esperava o parâmetro '%s'\n", vrgarg);
        
      }
    }
  }
  s.sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  struct timeval timeout;
  timeout.tv_sec = 0;
  // 50 ms
  timeout.tv_usec = 50000;
  setsockopt(s.sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  s.sock_addr.sin_family = AF_INET;
  s.sock_addr.sin_port = htons(PORT);
  inet_pton(AF_INET, HOST_IP, &s.sock_addr.sin_addr);

  s.udp = udp_new(s.sockfd, s.sock_addr, sizeof(s.sock_addr));
  return s;
}


static void destroy(Shared sh) {
  close(sh.sockfd);
  free(sh.udp.packet);
  free(sh.udp.ack_packet);
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

static bool synthetic_fail() {
  const double RESOLUTION = 10000;
  if(fail_percent == 0.0) return false;
  double r = ((rand() % (int) (RESOLUTION+1)) / RESOLUTION);
  dbg(r);
  return r <= fail_percent;
}

static bool send2(UDP* udp, const UDPPacket* pack) {
  assert(pack->size >= UDP_HEADER_SIZE);
  udp->total_bytes_sent += pack->size;
  if(synthetic_fail()) {
    log("Synthetically failing to send packet %hd", pack->packet_id);
    return true;
  }
  log("Sending packet %hd of type %d", pack->packet_id, pack->type);
  return sendto(udp->sockfd, pack, pack->size, 0, (void*)&udp->addr, udp->addr_len) != -1;
}
static bool recv2(UDP* udp, UDPPacket* pack) {
  udp->addr_len = sizeof(udp->addr);
  ssize_t bytes_read = recvfrom(udp->sockfd, pack, sizeof(UDPPacket), 0, (void*)&udp->addr, &udp->addr_len);
  if(bytes_read == -1) return false;
  assert(bytes_read == pack->size);
  udp->total_bytes_received += pack->size;
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

static bool send_ack(UDP* udp) {
  packet_prepare(udp->ack_packet, PACKET_ACK, udp->packet->packet_id, UDP_HEADER_SIZE);
  return send2(udp, udp->ack_packet);
}

static RecvResult raw_recv(UDP* udp, u16 expected_id, u16 expected_type) {
  #define retry(stat) { retries++; log("Dropping packet and retrying..."); udp->stat++; goto retry; }
  int retries = 0;
  UDPPacket* packet = udp->packet;
  log("----raw_recv----");
  retry:
  if(retries >= 10) return RECV_ERR;
  if(!recv2(udp, packet)) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) return RECV_TIMEOUT;
    else return RECV_ERR;
  }

  if(!checksum_match(packet)) {
    log("Checksum does not match!");
    retry(packets_lost);
  }

  if(packet->packet_id == 0 && packet->type == PACKET_CLOSE) {
    log("Connection closed");
    return RECV_CLOSE;
  }
  if(packet->packet_id == expected_id-1) {
    // Packet is duplicate
    log("Got duplicate packet");
    send_ack(udp);
    retry(packets_duplicated);
  }

  if(packet->packet_id != expected_id) {
    log("Expected packet id %hd, got %hd", expected_id, packet->packet_id);
    retry(packets_lost);
  }

  if(packet->type != expected_type) {
    log("Got unexpected packet type: %d, expected %d", packet->type, expected_type);
    retry(packets_lost);
  }

  if(!send_ack(udp)) {
    return RECV_ERR;
  }
  log("----");
  // Store a copy of last packet in case ACK is not reached;
  *udp->ack_packet = *udp->packet;

  #undef retry
  return RECV_OK;
}

static bool raw_send(UDP* udp, PacketType type, u16 id, u16 size) {
  #define retry(stat) { log("Retrying..."); retries++; udp->stat++; goto retry; }
  int retries = 0;
  UDPPacket* packet = udp->packet;
  log("----raw_send----");
  retry:
  if(retries >= 10) return false;
  packet_prepare(packet, type, id, size);
  // send packet
  if(!send2(udp, packet)) return false;

  // Wait for ACK
  u16 pack_id = packet->packet_id; 
  if(!recv2(udp, packet)) {
    // timer expired, try again;
    if(errno == EAGAIN) {
      log("Window expired");
      udp->packets_resent++;
      retry(packets_lost);
    }
    // error occoured
    else return false;
  }

  if(!checksum_match(packet)) {
    log("Checksum does not match");
    retry(packets_lost);
  }
  if(packet->type != PACKET_ACK) {
    log("Expected ACK, got something else");
    retry(packets_lost);
  }
  if(packet->packet_id == pack_id-1) {
    log("Got ACK for previous packet");
    retry(packets_resent);
  }
  if(packet->packet_id != pack_id) {
    log("Expecting ACK for packet id %hd, got %hd", pack_id, packet->packet_id);
    retry(packets_lost);
  }
  log("ACK for %hd acknowledged!", pack_id);
  log("----");

  #undef retry
  return true;
}

bool udp_connect(UDP* udp) {
  udp->packet_id = 0;
  return raw_send(udp, PACKET_INIT, 0, UDP_HEADER_SIZE);
}

RecvResult udp_listen(UDP *udp) {
  udp->packet_id = 0;
  return raw_recv(udp, 0, PACKET_INIT);
}

void udp_close(UDP* udp) {
  packet_prepare(udp->packet, PACKET_CLOSE, 0, UDP_HEADER_SIZE);
  send2(udp, udp->packet);
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

char* bytes_to_human(u32 byte_count) {
  char *units[] = {"", "K", "M", "G", "T"};
  char *postfix;
  static char buf[32];
  double bytes = byte_count;
  for (int i = 0; i < (int)(sizeof(units) / sizeof(units[0])); i++) {
    postfix = units[i];
    if (bytes < 1024) {
      break;
    }
    bytes /= 1024;
  }
  snprintf(buf, sizeof(buf), "%.2lf%s", bytes, postfix);
  return buf;
}

void udp_print_stats(UDP* udp) {
  print("Pacotes perdidos: %d", udp->packets_lost);
  print("Pacotes duplicados: %d", udp->packets_duplicated);
  print("Pacotes retransmitidos: %d", udp->packets_resent);
  print("Quantidade de dados enviados: %s", bytes_to_human(udp->total_bytes_sent));
  print("Quantidade de dados recebidos: %s", bytes_to_human(udp->total_bytes_received));
  print("Quantidade de pacotes comutados: %hd", udp->packet_id);
  print("Taxa de sucesso: %.2lf%%", (1.0 - udp->packets_lost/(double)udp->packet_id)*100.0);
}

int main(int argc, char **argv) {
  Shared sh = parse_cli(argc, argv);
  srand(time(NULL));
  app(sh);
  destroy(sh);
}

