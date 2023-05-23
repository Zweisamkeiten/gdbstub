#ifndef __COMMON_H__
#define __COMMON_H__

#define DEFAULT_GDBSTUB_PORT "1234"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#endif
