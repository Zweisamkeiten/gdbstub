#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#define BUFFER_SIZE 1024

static bool gdb_accept_tcp(int gdb_fd);
static int gdbserver_open_port(int port);
static void gdb_accept_loop(int client_fd);

static void gdb_accept_loop(int client_fd) {
  char recv_buffer[BUFFER_SIZE];

  while (1) {
    memset(recv_buffer, 0, BUFFER_SIZE);

    ssize_t recv_len = recv(client_fd, recv_buffer, BUFFER_SIZE - 1, 0);

    if (recv_len < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        close(client_fd);
        perror("recv");
        break;
      }
    } else if (recv_len == 0) {
      Log("Client disconnected");
      close(client_fd);
      break;
    } else {
      TCPRecvLog("Received: %s", recv_buffer);
      const char *send_buffer = "+$#00";
      TCPSendLog("Send: %s", send_buffer);
      send(client_fd, send_buffer, recv_len, 0);
    }
  }
}

static bool gdb_accept_tcp(int gdb_fd) {
  struct sockaddr_in client_addr = {};
  socklen_t client_len;
  int client_fd;

  for (;;) {
    // 接受客户端连接
    client_len = sizeof(client_addr);
    client_fd = accept(gdb_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0 && errno != EINTR) {
      perror("accept");
      return false;
    } else if (client_fd >= 0) {
      // 成功连接

      // 打印客户端连接信息
      Log("Connection accepted: %d", client_fd);

      break;
    }
  }

  // 设置套接字为非阻塞
  int flags = fcntl(client_fd, F_GETFL, 0);
  fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

  gdb_accept_loop(client_fd);
  close(gdb_fd);
  return true;
}

static int gdbserver_open_port(int port) {
  struct sockaddr_in server_addr;
  int server_fd;

  // 创建服务器套接字
  server_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket error");
    return -1;
  }

  // 绑定服务器地址
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind error");
    close(server_fd);
    return -1;
  }

  // 监听客户端连接
  if (listen(server_fd, 5) < 0) {
    perror("listen error");
    close(server_fd);
    return -1;
  }
  Log("Started a server at %d", port);

  return server_fd;
}

int gdbserver_start(const char *port_s) {
  int port = strtoull(port_s, NULL, 10);
  int gdb_fd;

  if (port > 0) {
    gdb_fd = gdbserver_open_port(port);
  } else {
    panic("port <= 0");
  }

  if (gdb_fd < 0) {
    return -1;
  }

  if (port > 0 && gdb_accept_tcp(gdb_fd)) {
    return 0;
  }

  /* gone wrong */
  close(gdb_fd);
  return -1;
}
