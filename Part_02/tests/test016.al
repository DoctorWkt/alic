#include <stdio.ah>

public void main(void) {
  int32 a= 0;
  a = 255 & 3;
  printf("%d\n", a);
  a = 15 | 240;
  printf("%d\n", a);
  a = 255 ^ 1;
  printf("%d\n", a);
  a = ~ 0;
  printf("%d\n", a);
}
