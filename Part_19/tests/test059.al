#include <stdio.ah>

enum { a, b, c=25, d, e= -34, f, g };

public void main(void) {
  printf("We have a=%d, b=%d, c=%d, d=%d\n", a, b, c, d);
  printf("We have e=%d, f=%d, g=%d\n", e, f, g);
}
