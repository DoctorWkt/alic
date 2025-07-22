#include <stdio.ah>

type FOO = flt32;

type BAR = struct {
  int32 x,
  flt64 y,
  bool z
};

public void main(void) {
  // These are now zeroed
  int8  a;
  int16 b;
  int32 c;
  int64 d;
  FOO   e;
  BAR   f;
  int32 g[100];

  int8  h = 'c';
  int16 i = 21;
  int32 j = 3333;
  int64 k = 444444;
  flt32 l = 3.14;


  printf("zeroes: %d %d %d %d %f\n", a, b, c, d, e);
  printf("zeroes: %d %f %d\n", f.x, f.y, f.z);
  printf("zeroes: %d %d %d\n", g[1], g[50], g[99]);
  printf("not zero: %d %d %d %d %f\n", h, i, j, k, l);
}
