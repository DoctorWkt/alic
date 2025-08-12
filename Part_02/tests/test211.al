#include <stdio.ah>

type FOO;

int fred[5][4][3];

public void main(void) {
  int val=1;
  int x;
  int y;
  int z;
  int *ptr;

  // Initialise the array
  foreach x (0 ... 4)
    foreach y (0 ... 3)
      foreach z (0 ... 2) {
	fred[x][y][z]= val; val++;
      }
  
  // Print out all of fred
  foreach val (fred) printf("%d\n", val); printf("\n");

  // Print out some of fred
  foreach val (fred[1]) printf("%d\n", val); printf("\n");

  // Print out even less of
  foreach val (fred[1][1]) printf("%d\n", val); printf("\n");
}
