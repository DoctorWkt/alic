#include <stdio.ah>

public void main(void) {
  int32 x=0;

  while (x < 100) {
    x= x + 1;
    if (x < 50) { continue; }
    printf("x is %d\n", x);
  }
}
