#include <stdio.ah>

void main(void) {
  int32 x;

  for (x=1; x < 10; x++) {
    printf("x is %d\n", x);
  }

  for (x=8; x > 0; x--) {
    printf("x is %d\n", x);
  }
}
