#ifndef __COMMON_H__
#define __COMMON_H__

#define DEFAULT_GDBSTUB_PORT "1234"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"

#define Assert(cond, format, ...)                                              \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "fatal: %s: %d " ANSI_FMT(format, ANSI_FG_RED) "\n",     \
              __FILE__, __LINE__, ##__VA_ARGS__),                              \
          assert(cond);                                                        \
    }                                                                          \
  } while (0)

#define panic(format, ...) Assert(0, format, ##__VA_ARGS__)

typedef union {
  struct {
    uint64_t gpr[32];
    uint64_t pc;
  };
  struct {
    uint64_t array[33];
  };
} CPU_State;

#define RESET_VECTOR 0x80000000

#define PG_ALIGN __attribute((aligned(4096)))
#define CONFIG_MBASE 0x80000000
#define CONFIG_MSIZE 0x8000000

#define EBREAK 0x00100073


#endif
