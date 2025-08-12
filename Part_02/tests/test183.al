#include <stdio.ah>

int32 fred(inout int a, int b) {
  a++;
  return(a + b);
}

public void main(void) {
  int x=5;
  int y=6;
  int z=0;

  printf("x %d y %d z %d\n", x, y, z);
  z= fred(x, y);
  printf("x %d y %d z %d\n", x, y, z);
}
