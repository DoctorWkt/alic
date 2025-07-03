#include <stdio.ah>

int32 x[5] = { 1, 3, 5, 7, 9 };

public void main(void) {
  x[3] = 100;
  x = const;
  x[2] = 50;

}
