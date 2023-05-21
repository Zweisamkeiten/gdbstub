#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>
#include <stdbool.h>

#define TODO() assert(0)

#define Debug(...) if (debug == true) \
                             printf(__VA_ARGS__)

extern bool debug;

#endif
