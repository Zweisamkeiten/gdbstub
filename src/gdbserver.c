#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <regex.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "gdbserver.h"

#define BUFFER_SIZE 1024
#define CMP(ptr, str) strncmp((ptr), (str), strlen(str)) == 0

extern void pmem_write(uint64_t addr, int len, uint64_t data);
extern uint64_t pmem_read(uint64_t addr, int len);
extern bool in_pmem(uint64_t addr);
extern uint64_t bp_addr;
extern bool cpu_stop;
bool bp_trap = false;

static bool gdb_accept_tcp(int gdb_fd);
static int gdbserver_open_port(int port);
static void gdb_accept_loop(int client_fd);
static unsigned char computeChecksum(const char *packet);
static void gdb_reply(int client_fd, Pack_match *pack_recv);

typedef void (*handle)(void *params);

extern CPU_State cpu;

static unsigned char computeChecksum(const char *packet) {
  size_t len = strlen(packet);
  unsigned char sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += packet[i];
  }
  return sum & 0xFF;
}

static Pack_match *gdb_match(int client_fd, const char *buf, int size) {
  if (buf[0] == '-') {
    gdb_reply(client_fd, NULL);
  } else if (buf[0] == '+') {
    return NULL;
  } else if (buf[0] == '$') {
    regex_t regex;
    regcomp(&regex, "^\\$([^#]*)#([0-9a-zA-Z]{2})$", REG_EXTENDED);

    regmatch_t match[3];

    if (regexec(&regex, buf, 3, match, 0) == 0) {

      Pack_match *pack_recv = (Pack_match *)malloc(sizeof(Pack_match));
      pack_recv->str = malloc(match[1].rm_eo - match[1].rm_so + 1);
      memcpy(pack_recv->str, buf + match[1].rm_so,
             match[1].rm_eo - match[1].rm_so);
      pack_recv->str[match[1].rm_eo - match[1].rm_so] = '\0';

      sscanf(buf + match[2].rm_so, "%2hhx", &(pack_recv->checksum));

      Log("checksum: %x, Str: %s", pack_recv->checksum, pack_recv->str);

      if (pack_recv->checksum == computeChecksum(pack_recv->str)) {
        return pack_recv;
      } else {
        free(pack_recv->str);
        free(pack_recv);
        return NULL;
      }
    } else {
      // 匹配失败
      send(client_fd, "-", 1, 0);
      return NULL;
    }
  }

  return NULL;
}

static void generateReply(const char *packet, char *reply) {
  unsigned char checksum = computeChecksum(packet);
  sprintf(reply, "$%s#%02x", packet, checksum);
}

static uint8_t hex_nibble(uint8_t hex) {
  return isdigit(hex) ? hex - '0' : tolower(hex) - 'a' + 10;
}

static uint16_t gdb_decode_hex(uint8_t msb, uint8_t lsb) {
  if (!isxdigit(msb) || !isxdigit(lsb))
    return UINT16_MAX;
  return 16 * hex_nibble(msb) + hex_nibble(lsb);
}

static uint64_t gdb_decode_hex_str(uint8_t *bytes) {
  uint64_t value = 0;
  uint64_t weight = 1;
  while (isxdigit(bytes[0]) && isxdigit(bytes[1])) {
    value += weight * gdb_decode_hex(bytes[0], bytes[1]);
    bytes += 2;
    weight *= 16 * 16;
  }
  return value;
}

static uint64_t hex_to_dec(uint8_t *hex_str) {
  uint8_t *p = hex_str;
  uint64_t dec = 0;
  while (*p != '\0') {
    dec = dec * 16 + hex_nibble(*p);
    p++;
  }
  return dec;
}

static void gdb_reply(int client_fd, Pack_match *pack_recv) {
  static char send_buffer[BUFFER_SIZE];
  char *p = send_buffer;

  // '-' 重传
  if (pack_recv == NULL) {
    TCPSendLog("Send: %s", send_buffer);
    send(client_fd, send_buffer, strlen(send_buffer), 0);
    return;
  }

  p += sprintf(p, "+");

  switch (pack_recv->str[0]) {
  case 'q': {
    if (CMP(pack_recv->str + 1, "Supported")) {
      generateReply("hwbreak+", p);
    } else if (CMP(pack_recv->str + 1, "fThreadInfo")) {
      generateReply("", p);
    } else if (CMP(pack_recv->str + 1, "L")) {
      generateReply("", p);
    } else if (CMP(pack_recv->str + 1, "C")) {
      generateReply("", p);
    } else if (CMP(pack_recv->str + 1, "Attached")) {
      // 必须响应“1”，表示我们的远程服务器已附加到现有进程
      // 或者带有“0”表示远程服务器自己创建了一个新进程
      // 根据我们在这里的回答，我们在调用“退出”时会得到一个 kill 或 detach
      // 命令 想在退出 GDB 时让程序继续运行，所以是“1”
      generateReply("1", p);
    } else if (CMP(pack_recv->str + 1, "Symbol::")) {
      generateReply("OK", p);
    } else {
      generateReply("", p);
    }
    break;
  }
  case 'v': {
    if (CMP(pack_recv->str + 1, "MustReplyEmpty")) {
      generateReply("", p);
    } else if (CMP(pack_recv->str + 1, "Cont?")) {
      generateReply("", p);
    }
    break;
  }
  case 'H': {
    if (CMP(pack_recv->str + 1, "g0")) {
      generateReply("", p);
    } else if (CMP(pack_recv->str + 1, "c-1")) {
      generateReply("", p);
    } else if (CMP(pack_recv->str + 1, "c0")) {
      generateReply("", p);
    }
    break;
  }
  case 'g': {
    char regs[16 * (sizeof(CPU_State) / sizeof(uint64_t)) + 1];
    char *pt = regs;
    for (int i = 0; i < sizeof(CPU_State) / sizeof(uint64_t); i++) {
      for (int j = 0; j < 8; j++) {
        uint8_t hex = (cpu.array[i] >> (j * 8)) & 0xff;
        pt += sprintf(pt, "%02x", hex);
      }
    }

    generateReply(regs, p);
    break;
  }
  case 'G': {
    uint8_t *pt = (uint8_t *)pack_recv->str + 1;

    uint8_t c;
    for (int i = 0; i < sizeof(CPU_State) / sizeof(uint64_t); i++) {
      c = pt[16];
      pt[16] = '\0';
      cpu.array[i] = gdb_decode_hex_str(pt);
      pt[16] = c;
      pt += 16;
    }

    generateReply("OK", p);
    break;
  }
  case 'P': {
    char *pt = pack_recv->str + 1;
    char *equal_p = strchr(pt, '=');
    *equal_p = '\0';
    uint8_t reg_to_write = hex_to_dec((uint8_t *)pt);
    cpu.array[reg_to_write] = gdb_decode_hex_str((uint8_t *)equal_p + 1);
    *equal_p = '=';
    generateReply("OK", p);
    break;
  }
  case 'm': {
    char *pt = pack_recv->str + 1;
    char *comm_p = strchr(pt, ',');
    *comm_p = '\0';
    uint64_t raddr = hex_to_dec((uint8_t *)pt);
    uint64_t length = strtol(comm_p + 1, NULL, 16);

    if (in_pmem(raddr)) {
      char tmp[length * 2 + 1];
      for (int i = 0; i < length; i++) {
        sprintf(tmp + 2 * i, "%02x", (uint8_t)pmem_read(raddr + i, 1));
      }
      generateReply(tmp, p);
    } else {
      generateReply("", p);
    }
    break;
  }
  case 'M': {
    char *pt = pack_recv->str + 1;
    char *comm_p = strchr(pt, ',');
    *comm_p = '\0';
    uint64_t waddr = hex_to_dec((uint8_t *)pt);
    char *colon_p = strchr(comm_p + 1, ':');
    *colon_p = '\0';
    uint64_t length = strtol(comm_p + 1, NULL, 16);

    char *data_str = colon_p + 1;
    if (in_pmem(waddr)) {
      uint8_t c;
      for (int i = 0; i < length; i++) {
        c = data_str[2];
        data_str[2] = '\0';
        pmem_write(waddr + i, 1, gdb_decode_hex_str((uint8_t *)data_str));
        data_str[2] = c;
        data_str += 2;
      }
      generateReply("OK", p);
    } else {
      generateReply("", p);
    }
    break;
  }
  case 'c': {
    cpu_stop = false;
    while (!cpu_stop) {
      cpu.pc += 4;
      if (cpu.pc == bp_addr) {
        cpu_stop = true;
        bp_trap = true;
      }
    }
    generateReply("OK", p);
    break;
  }
  case 'Z': {
    typedef enum { SWBREAK, HWBREAK } bp_type;
    char *pt = pack_recv->str + 1;
    char *comm_p = strchr(pt, ',');
    *comm_p = '\0';
    char *comm_p2 = strchr(comm_p + 1, ',');
    *comm_p2 = '\0';
    bp_type type = atoi(pt);
    // int kind = atoi(comm_p2 + 1);
    switch (type) {
    case SWBREAK:
    case HWBREAK: {
      bp_addr = hex_to_dec((uint8_t *)comm_p + 1);
      // bp.addrs[bp.bp_nums++] = hex_to_dec((uint8_t *)comm_p + 1);
    }
    }
    generateReply("OK", p);
    break;
  }
  case 'z': {
    typedef enum { SWBREAK, HWBREAK } bp_type;
    char *pt = pack_recv->str + 1;
    char *comm_p = strchr(pt, ',');
    *comm_p = '\0';
    char *comm_p2 = strchr(comm_p + 1, ',');
    *comm_p2 = '\0';
    bp_type type = atoi(pt);
    // int kind = atoi(comm_p2 + 1);
    switch (type) {
    case SWBREAK:
    case HWBREAK: {
      bp_addr = 0;
    }
    }
    generateReply("OK", p);
    break;
  }
  case '?': {
    generateReply("S05", p);
    break;
  }
  default: {
    generateReply("", p);
    break;
  }
  }

  TCPSendLog("Send: %s", send_buffer);
  send(client_fd, send_buffer, strlen(send_buffer), 0);

  free(pack_recv->str);
  free(pack_recv);
  return;
}

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

      Pack_match *ma =
          gdb_match(client_fd, (const char *)recv_buffer, recv_len);

      if (ma != NULL) {
        gdb_reply(client_fd, ma);
      }

      if (cpu_stop && bp_trap) {
        bp_trap = false;
        char send_buffer[9];
        const char *str = "S05";
        generateReply(str, send_buffer);
        send(client_fd, send_buffer, strlen(send_buffer), 0);
      }
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
