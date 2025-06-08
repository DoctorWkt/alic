// Miscellaneous functions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.h"
#include "proto.h"

// Print out fatal messages
void fatal(const char *fmt, ...) {
  va_list ptr;

  va_start(ptr, fmt);
  fprintf(stderr, "%s line %d: ", Infilename, Line);
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
  exit(1);
}

// Print out a "cannot do" error based on an ASTnode's type
void cant_do(ASTnode * n, Type * t, char *msg) {
  if (n->type == t)
    fatal(msg);
}

// Allocate memory but catch failures
void *Malloc(size_t size) {
  void *ptr;

  ptr = (void *) malloc(size);
  if (ptr == NULL)
    fatal("malloc failure\n");

  return (ptr);
}

void *Calloc(size_t size) {
  void *ptr;

  ptr = (void *) calloc(1, size);
  if (ptr == NULL)
    fatal("calloc failure\n");

  return (ptr);
}
