#include <stdio.h>

type fred = struct {
  int8  a,
  int16 b,
  bool  b2,
  int8 *ptr,
  bool  b3,
  union { flt64 x, int16 y, bool z },
  int32 c,
  int64 d,
  bool  b4
};

fred jim;

void main(void) {
  int16 foo;
  bool out;

  jim.z= true; out= jim.z; printf("out is %d %d\n", out, jim.z);
  jim.a = 45; printf("%d\n", jim.a);
  jim.b = 23; foo= jim.b; printf("%d %d\n", foo, jim.b);
  jim.b2= true; printf("%d\n", jim.b2);
  jim.b3= true; printf("%d\n", jim.b3);
  // BUG: jim.z prints as 0 below
  jim.y= 31415; printf("union is %f %d\n", jim.x, jim.y);
  jim.c= 99999; printf("%d\n", jim.c);
  jim.d= 199999; printf("%d\n", jim.d);
  jim.b4= true; printf("%d\n", jim.b4);
}
