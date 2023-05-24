#include "common.h"
#include "gdbserver.h"

CPU_State cpu;
uint64_t bp_addr;
bool cpu_stop = true;
static uint8_t pmem[0x8000000] PG_ALIGN = {};
uint8_t *guest_to_host(uint64_t paddr) { return pmem + paddr - CONFIG_MBASE; }

static inline void host_write(void *addr, int len, uint64_t data) {
  switch (len) {
  case 1:
    *(uint8_t *)addr = data;
    return;
  case 2:
    *(uint16_t *)addr = data;
    return;
  case 4:
    *(uint32_t *)addr = data;
    return;
  case 8:
    *(uint64_t *)addr = data;
    return;
  default:
    assert(0);
  }
}

static inline uint64_t host_read(void *addr, int len) {
  switch (len) {
  case 1:
    return *(uint8_t *)addr;
  case 2:
    return *(uint16_t *)addr;
  case 4:
    return *(uint32_t *)addr;
  case 8:
    return *(uint64_t *)addr;
  default:
    assert(0);
  }
}

uint64_t pmem_read(uint64_t addr, int len) {
  uint64_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

void pmem_write(uint64_t addr, int len, uint64_t data) {
  host_write(guest_to_host(addr), len, data);
}

bool in_pmem(uint64_t addr) { return addr - CONFIG_MBASE < CONFIG_MSIZE; }

static const uint32_t img[] = {
    0x00009117, // auipc sp, 0x9
    0x00100513, // li      a0,1
    0x00200593, // li      a1,2
    0x00200613, // li      a2,2
    0x00200693, // li      a3,2
    0x00c58933, // add     s2,a1,a2
    0x00a13423, // sd      a0,8(sp)
    0x00100073, // ebreak (used as nemu_trap)
    0xdeadbeef, // some data
};

int main(int argc, char *argv[]) {

  cpu.pc = RESET_VECTOR;

  cpu.gpr[31] = 0x7fffffff;

  memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));

  if (gdbserver_start(DEFAULT_GDBSTUB_PORT) < 0) {
    panic("gdbserver start fail");
  };

  return EXIT_SUCCESS;
}
