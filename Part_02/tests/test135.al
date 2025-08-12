#include <stdio.ah>
#include <sys/types.ah>

int vfprintf(FILE *stream, char *format, ...);

// Print out a message to stderr
void fatal(char *fmt, ...) {
  void *ptr;

  va_start(ptr);
  fprintf(stderr, "err message: ");
  vfprintf(stderr, fmt, ptr);
  va_end(ptr);
}

public void main(void) {
  int32 x= 23;
  flt64 y= 44.5566;
  fatal("I hope this works %d %f\n", x, y);
}
