#include <stdio.ah>

type FOO= struct {
  int32 a,
  int8  b,
  flt32 c
};

void main(void) {
  FOO fred[10];
  printf("fred has %d elements\n", sizeof(fred));
  fred[1].c= 3.1;  printf("fred[1].c is %f\n", fred[1].c);
  fred[2].b= 12;   printf("fred[2].b is %d\n", fred[2].b);
  fred[3].a= 100;  printf("fred[3].a is %d\n", fred[3].a);
}
