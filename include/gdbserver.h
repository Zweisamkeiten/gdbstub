#ifndef __GDBSERVER_H__
#define __GDBSERVER_H__

#include <stdint.h>

extern int gdbserver_start(const char *port);

typedef struct {
  char *str;
  uint8_t checksum;
} Pack_match;

#endif
