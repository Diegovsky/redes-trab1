#include "shared.h"
#include "sds.h"

#include <dirent.h>
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

#define ENDL "\r\n"

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
  sds cmd = sdscatprintf(sdsempty(), "file -ib %s", filename);
  FILE *stream = popen(cmd, "r");
  sdsfree(cmd);
  if (stream == NULL)
    return NULL;

  char *buf = sdsgrowzero(sdsempty(), 64);
  fgets(buf, 64, stream);
  buf[strcspn(buf, "\n")] = '\0';
  sdsupdatelen(buf);
  log("mime: '%s'\n", buf);
  pclose(stream);
  return buf;
}

static void sendall(int fd, char* msg, int len) {
  while(len > 0) {
    int sent = send(fd, msg, len, 0);
    msg += sent;
    len -= sent;
  }
}
static void sendstr(int fd, char *msg) { sendall(fd, msg, strlen(msg)); }

typedef struct {
  ssize_t size;
  bool is_dir;
} FileInfo;

static bool get_file_info(FILE *file, FileInfo* out) {
  int fd = fileno(file);

  struct stat stat;
  if (fstat(fd, &stat) != 0) return false;
  out->is_dir = S_ISDIR(stat.st_mode);
  out->size = stat.st_size;
  return true;
}

static void send_index(int clientfd, sds path) {
  sds msg = sdsempty();
  msg = sdscatprintf(msg, "<html><head><title>Listagem de %s</title></head><body style=\"display: flexbox\"><h1>Listagem de %s</h1>", path, path);

  sds buf = sdsempty();
  DIR* dir = opendir(path);
  if (dir) {
      struct dirent* entry;
      while ((entry = readdir(dir)) != NULL) {
          if (streq(entry->d_name, ".") || streq(entry->d_name, "..")) continue;
          sdsclear(buf);
          if(sdslen(path) != 0)  {
            buf = sdscatprintf(buf, "%s/", path);
          }
          buf = sdscat(buf, entry->d_name);
          msg = sdscatprintf(msg, "<a href=\"%s\">%s</a><br>\n", buf, entry->d_name);
      }
      closedir(dir);
  } else {
      msg = sdscat(msg, "<p>Erro ao listar diretório.</p>");
  }

  sdsfree(buf);
  msg = sdscat(msg, "</body></html>");

  sendall(clientfd, msg, sdslen(msg));
  sdsfree(msg);
}

static void send_error(int clientfd, int error) {
  sds msg = sdsnew("HTTP/1.1 ");
  switch(error) {
    case 404:
      msg = sdscat(msg, "404 Not Found");
      break;
    case 401:
      msg = sdscat(msg, "401 Bad Request");
      break;
    default:
      die("Unsupported error code %d", error);
  }
  msg = sdscat(msg, ENDL);
  msg = sdscat(msg, "Server: trab2-server" ENDL);
  msg = sdscat(msg, "Content-Type: text/plain"ENDL);
  msg = sdscat(msg, "Server: trab2-server"ENDL);
  const char* text = "Erro";
  msg = sdscatprintf(msg, "Content-Length: %ld"ENDL, strlen(text));
  msg = sdscat(msg, ENDL);
  msg = sdscat(msg, text);
  sendall(clientfd, msg, sdslen(msg));
  sdsfree(msg);
}

void handle_request(HandlerArgs *args) {
  char *buf = (char *)args->buffer;
  sds path = NULL, mime = NULL;
  sds *lines = NULL;
  FILE *file = NULL;
  ssize_t bytes_read = recv(args->clientfd, buf, BUFSIZE - 1, 0);
  if (bytes_read == -1) {
    goto error;
  }

  buf[bytes_read] = '\0';

  int lines_len = 0;
  lines = sdssplitlen(buf, bytes_read, ENDL, 2, &lines_len);

  char method[16] = {0};
  path = sdsgrowzero(sdsempty(), 256);
  char *line = lines[0];

  sscanf(line, "%s /%[^ ]s", method, path);
  path = sdstrim(path, " ");
  if(sdslen(path) == 0) {
    path = sdscat(path, "./");
  }
  log("%s: %s", method, path);
  if (!streq(method, "GET")) {
    send_error(args->clientfd, 404);
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
    send_error(args->clientfd, 404);
    goto end;
  }

  FileInfo info;
  if(!get_file_info(file, &info)) {
    send_error(args->clientfd, 404);
    goto end;
  }

  mime = mime_type(path);
  if(info.is_dir) {
    sdsclear(mime);
    mime = sdscat(mime, "text/html; charset=utf-8");
  }

  sds msg = sdsnew("HTTP/1.1 200 OK"ENDL);
  msg = sdscatprintf(msg, "Content-Type: %s"ENDL, mime);
  msg = sdscat(msg, "Server: trab2-server"ENDL);
  if(!info.is_dir) msg = sdscatprintf(msg, "Content-Length: %ld"ENDL, info.size);
  msg = sdscat(msg, ""ENDL);
  sendstr(args->clientfd, msg);
  sdsfree(msg);

  if(info.is_dir) {
    send_index(args->clientfd, path);
    goto end;
  }

  for (;;) {
    ssize_t read = fread(buf, 1, BUFSIZE, file);
    fwrite(buf, read, 1, stdout);
    if (read == 0)
      break;
    else if (read == -1) {
      errno = ferror(file);
      goto error;
    }

    sendall(args->clientfd, buf, read);
  }
  


end:
  if (path != NULL) sdsfree(path);
  if (mime != NULL) sdsfree(mime);
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
