#include <stdio.ah>

public void main(void) {
  int32 x = 3;
  int8 *y = NULL;
  int8 **z = y;

  if (y == z) {
    printf("y and z match\n", 0);
  }
}
