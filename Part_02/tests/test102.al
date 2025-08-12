#include <stdio.ah>

public void main(void) {
  bool b;
  int32 x;

  printf("Checking if x is OUTSIDE the range 13..19\n");
  for (x= 10; x <= 20; x++) {
    if (x<13 || x>19)
      printf("%d outside the range\n", x);
    else
      printf("%d  inside the range\n", x);
  }

  printf("\nChecking if x is INSIDE the range 13..19\n");
  for (x= 10; x <= 20; x++) {
    if (x >= 13 && x <= 19)
      printf("%d  inside the range\n", x);
    else
      printf("%d outside the range\n", x);
  }
}
