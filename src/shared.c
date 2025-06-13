#include "shared.h"
#include "sds.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

static Shared parse_cli(int argc, char **argv) {
  Shared s;
  memset(&s, 0, sizeof(s));
  if (argc != 3 ||
      (argc >= 2 && (streq(argv[1], "--help") || streq(argv[1], "-h")))) {
    die("USAGE: %s [IP] [PORT]", argv[0]);
  }
  s.hostname = argv[1];
  s.port = (short)atoi(argv[2]);
  s.sockfd = socket(AF_INET, SOCK_STREAM, 0);
  int optval = 1;
  setsockopt(s.sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  s.sock_addr.sin_family = AF_INET;
  s.sock_addr.sin_addr.s_addr = inet_addr(s.hostname);
  s.sock_addr.sin_port = htons(s.port);

  if (bind(s.sockfd, addr(s.sock_addr)) != 0) {
    die("bind error: %s", strerror(errno));
  }
  return s;
}

static void destroy(Shared sh) {
  close(sh.sockfd);
}

static sds mime_type(const char *filename) {
  sds cmd = sdscatprintf(sdsempty(), "file --mime-type %s", filename);
  FILE *stream = popen(cmd, "r");
  sdsfree(cmd);
  if (stream == NULL)
    return NULL;

  char *buf = sdsgrowzero(sdsempty(), 64);
  fgets(buf, 64, stream);
  pclose(stream);
  return buf;
}

static int sendstr(int fd, char *msg) { return send(fd, msg, strlen(msg), 0); }

static ssize_t get_file_size(FILE *file) {
  int fd = fileno(file);

  struct stat stat;
  if (fstat(fd, &stat) != 0)
    return -1;
  return stat.st_size;
}

/*
GET / HTTP/1.1
Host: localhost:8080
User-Agent: curl/8.14.1
*/
void handle_request(HandlerArgs *args) {
  char *buf = (char *)args->buffer;
  char *path = NULL;
  sds *lines = NULL;
  FILE *file = NULL;
  ssize_t bytes_read = recv(args->clientfd, buf, BUFSIZE - 1, 0);
  if (bytes_read == -1) {
    goto error;
  }

  buf[bytes_read] = '\0';

  int lines_len = 0;
  lines = sdssplitlen(buf, bytes_read, "\r\n", 2, &lines_len);

  char method[16] = {0};
  path = sdsgrowzero(sdsempty(), 256);
  char *line = lines[0];

  sscanf(line, "%s /%[^ ]s", method, path);
  log("%s: %s", method, path);
  if (!streq(method, "GET")) {
    sendstr(args->clientfd, "HTTP/1.1 401 Bad Request\r\n");
    goto end;
  }
  if(streq(path, "_quit")) {
    should_quit = true;
    goto end;
  }

  for (int i = 1; i < lines_len; i++) {
    line = lines[i];
    char *sep = strchr(line, ':');
    if (sep == NULL)
      break;
    *sep = '\0';
    sep++;
    while (*sep == ' ')
      sep++;
    log("%s: %s", line, sep);
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    sendstr(args->clientfd, "HTTP/1.1 404 Not Found\r\n");
    goto end;
  }

  ssize_t file_size = get_file_size(file);
  sds mime = mime_type(path);

  sds msg = sdsnew("HTTP/1.1 200 OK\r\n");
  msg = sdscatprintf(msg, "Content-Type: %s\r\n", mime);
  msg = sdscat(msg, "Server: trab2-server\r\n");
  if (file_size != -1)
    msg = sdscatprintf(msg, "Content-Length: %s\r\n", mime);
  msg = sdscat(msg, "\r\n\r\n");
  sendstr(args->clientfd, msg);

  sdsfree(mime);
  sdsfree(msg);
  for (;;) {
    ssize_t read = fread(buf, 1, BUFSIZE, file);
    if (read == 0)
      break;
    else if (read == -1) {
      errno = ferror(file);
      goto error;
    }

    send(args->clientfd, buf, read, 0);
  }

end:
  if (path != NULL) sdsfree(path);
  if (lines != NULL) sdsfreesplitres(lines, lines_len);
  if(file != NULL) fclose(file);
  close(args->clientfd);
  return;

error:
  args->error = true;
  args->err_no = errno;
  goto end;
}

HandlerArgs ha_new(const Shared *sh, int clientfd) {
  return (HandlerArgs){.sh = sh,
                       .clientfd = clientfd,
                       .buffer = calloc(BUFSIZE, 1),
                       .error = false,
                       .err_no = 0,
                       };
}

bool should_quit = false;

static void sighandler(int sig) {
  (void)sig;
  if (should_quit) {
    exit(-1);
  }
  should_quit = true;
}

int main(int argc, char **argv) {
  Shared sh = parse_cli(argc, argv);
  signal(SIGINT, sighandler);
  app(sh);
  destroy(sh);
}
