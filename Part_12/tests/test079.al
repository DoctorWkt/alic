#include <stdio.ah>

void main(void) {
  int32 x=0;

  for (x= 0; x < 100; x= x + 1) {
    if (x < 50) { continue; }
    printf("x is %d\n", x);
  }
}
