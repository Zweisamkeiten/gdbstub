#include "common.h"
#include "gdbserver.h"

CPU_State cpu;

int main(int argc, char *argv[]) {

  cpu.pc = RESET_VECTOR;

  cpu.gpr[31] = 0x7fffffff;

  if (gdbserver_start(DEFAULT_GDBSTUB_PORT) < 0) {
    panic("gdbserver start fail");
  };

  return EXIT_SUCCESS;
}
