#include <stdio.ah>

type FOO = struct {
  int32 a,
  int8  b,
  bool  c,
  flt32 d
};

public void main(void) {
  int32 i;
  int32 jim[5]= { 1, 2, 2, 2, 4 };
  FOO fred = { 1, 'x', true, 3.2 };

  printf("fred.a is %d\n",   fred.a);
  printf("fred.b is '%c'\n", fred.b);
  printf("fred.c is %d\n",   fred.c);
  printf("fred.d is %f\n",   fred.d);

  for (i=0; i < 5; i++)
    printf("jim[%d] is %d\n", i, jim[i]);
}
