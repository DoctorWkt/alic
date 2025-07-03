#include <stdio.ah>

void fred(int8 *fmt, ...) {
  int32 x;
  int32 y;
  flt64 z;
  void *va_ptr;

  va_start(va_ptr);
  x= va_arg(va_ptr, int32);
  y= va_arg(va_ptr, int32);
  z= va_arg(va_ptr, flt64);
  va_end(va_ptr);

  printf("fred has %d %d %f %s\n", x, y, z, fmt);
}

public void main(void) {
  int32 a= 2;
  int32 b= 33;
  flt64 d= 100.3;

  fred("foo", a, b, d);
}
