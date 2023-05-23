#include "common.h"
#include "gdbserver.h"

int main(int argc, char *argv[]) {
  if (gdbserver_start(DEFAULT_GDBSTUB_PORT) < 0) {
    panic("gdbserver start fail");
  };

  return EXIT_SUCCESS;
}
