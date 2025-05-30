#include <stdio.ah>

void main(void) {
  int32 x=0;

  while (x < 100) {
    printf("x is %d\n", x);
    if (x >= 50) { break; }
    x= x + 1;
  }
}
