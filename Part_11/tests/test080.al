#include <stdio.ah>

void main(void) {
  int32 x=0;

  while (true) {
    printf("x is %d\n", x);
    if (x >= 50) { break; }
    x= x + 1;
  }
}
