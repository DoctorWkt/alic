#include <stdio.ah>

type FLOP = struct {
  int32 a,
  int32 b,
  int32 c
};

public void main(void) {
  FLOP fred;
  fred.b= 3;
  printf("fred.b is %d\n", fred.b);
}
