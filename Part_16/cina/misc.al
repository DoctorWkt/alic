// Miscellaneous functions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Print out fatal messages
public void fatal(const char *fmt, ...) {
  void *ptr;

  va_start(ptr);
  fprintf(stderr, "%s line %d: ", Infilename, Line);
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
  exit(1);
}


// Print out fatal messages with known line number
public void lfatal(const int line, const char *fmt, ...) {
  void *ptr;

  va_start(ptr);
  fprintf(stderr, "%s line %d: ", Infilename, line);
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
  exit(1);
}

// Print out a "cannot do" error based on an ASTnode's type
public void cant_do(const ASTnode * n, const Type * t, const char *msg) {
  if (n.ty == t)
    fatal(msg);
}

// Allocate memory but catch failures
public void *Malloc(const size_t size) {
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL)
    fatal("Malloc failure\n");

  return (ptr);
}

public void *Calloc(const size_t size) {
  void *ptr;

  ptr = calloc(1, size);
  if (ptr == NULL)
    fatal("Calloc failure\n");

  return (ptr);
}
