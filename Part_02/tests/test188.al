#include <stdio.ah>

type FOO = int8 range 45 ... 3000;

public void main(void) {
  FOO x;
  FOO y;

  x= 40;    printf("x is %d\n", x);
  y= -7000; printf("y is %d\n", y);
}
