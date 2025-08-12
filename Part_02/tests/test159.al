#include <stdio.ah>
#include <stdlib.ah>

type FOO = struct {
  int32 a,
  int16 b,
  bool c
};

public void main(void) {
  const FOO *fred = malloc(sizeof(FOO));
  fred.a= 32;
  printf("%d\n", fred.a);
}
