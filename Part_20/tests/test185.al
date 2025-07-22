#include <stdio.ah>

type FOO = int8 range 23 ... 45;

public void main(void) {
  FOO x;
  FOO y;

  x= 40;  printf("x is %d\n", x);
  y= 200; printf("y is %d\n", y);
}
