#include <stdio.ah>

public void main(void) {
  int32 x;

  for (x=0; x < 15; x= x + 1) {
    printf("%d mod 7 is %d\n", x, x % 7);
  }
}
