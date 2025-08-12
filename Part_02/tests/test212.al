#include <stdio.ah>

int16 foo[2][2][2] = { 1, 2, 3, 4, 5, 6, 7, 8};

public void main(void) {
  int x;
  int y;
  int z;

  foreach x (0 ... 1)
    foreach y (0 ... 1)
      foreach z (0 ... 1)
	printf("%d\n", foo[x][y][z]);
}
