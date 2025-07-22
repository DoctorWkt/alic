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

// Print out fatal messages with a specific line numbner
void lfatal(int line, const char *fmt, ...) {
  va_list ptr;

  va_start(ptr, fmt);
  fprintf(stderr, "%s line %d: ", Infilename, line);
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
    fatal("Malloc failure\n");

  return (ptr);
}

void *Calloc(size_t size) {
  void *ptr;

  ptr = (void *) calloc(1, size);
  if (ptr == NULL)
    fatal("Calloc failure\n");

  return (ptr);
}

// The djb2 hash function comes from
// http://www.cse.yorku.ca/~oz/hash.html
// No copyright is given for it.
//
// Given a pointer to a string, or NULL,
// return a 64-bit hash value for it.
uint64_t djb2hash(uint8_t * str) {
  uint64_t hash = 5381;
  uint8_t c;

  if (str == NULL)
    return (0);

  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + c;

  return (hash);
}
