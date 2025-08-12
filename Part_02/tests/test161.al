#include <stdio.ah>

public void main(void) {
  flt64  a= -3000.76;
  flt32  b;
  int64  c;
  int32  d;
  int16  e;
  int8   f;
  uint64 p= 99;
  uint32 q;
  uint16 r;
  uint8  s;

  printf("a is %f\n", a);
  b= cast(a, flt32); printf("b is %f\n",  b);
  c= cast(a, int64); printf("c is %ld\n", c);
  d= cast(a, int32); printf("d is %d\n",  d);
  e= cast(a, int16); printf("e is %d\n",  e);
  printf("\n");

  c= 27;
  d= cast(c, int32); printf("d is %d\n",  d);
  e= cast(c, int16); printf("e is %d\n",  e);
  f= cast(c, int8);  printf("f is %d\n",  f);
  printf("\n");

  q= cast(p, uint32); printf("q is %d\n",  q);
  r= cast(p, uint16); printf("r is %d\n",  r);
  s= cast(p, uint8);  printf("s is %d\n",  s);
  
}
