// Miscellaneous functions for the alic compiler.
// (c) 2025 Warren Toomey, GPL3

#include "alic.ah"
#include "proto.ah"

// Print out fatal messages
public void fatal(const string fmt, ...) {
  void *ptr;

  va_start(ptr);
  fprintf(stderr, "%s line %d: ", Infilename, Line);
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
  exit(1);
}


// Print out fatal messages with known line number
public void lfatal(const int line, const string fmt, ...) {
  void *ptr;

  va_start(ptr);
  fprintf(stderr, "%s line %d: ", Infilename, line);
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
  exit(1);
}

// Print out a "cannot do" error based on an ASTnode's type
public void cant_do(const ASTnode * n, const Type * t, const string msg) {
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

// The next function needs unsigned chars
// but we need to be able to call it with
// a pointer to signed chars
type uschar = struct {
  union { int8 sc, uint8 uc }
};

// The djb2 hash function comes from
// http://www.cse.yorku.ca/~oz/hash.html
// No copyright is given for it.
//
// Given a pointer to a string, or NULL,
// return a 64-bit hash value for it.
public uint64 djb2hash(int8 * str) {
  uint64 hash = 5381;
  uschar c;

  if (str == NULL)
    return (0);

  c.sc= *str;
  while (c.uc != 0) {
    hash = ((hash << 5) + hash) + c.uc;
    str++;
    c.sc= *str;
  }

  return (hash);
}
