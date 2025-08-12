#include <stdio.ah>

type FOO = int16 range -5000 ... 6000;

public void main(void) {
  FOO x;
  FOO y;

  x= 40;    printf("x is %d\n", x);
  y= -7000; printf("y is %d\n", y);
}
