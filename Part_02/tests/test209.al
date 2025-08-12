#include <stdio.ah>

int32 ary[7][5][4];

public void main(void) {
  int32 foo[7][5][4];
  int32 cnt=1;
  int64 x=3;
  int64 y=2;
  int64 z=1;

  foo[x][y][z]= 23; printf("%d\n", foo[x][y][z]);
  x= 4; y= 1; z= 3;
  foo[x][y][z]= 123; printf("%d\n", foo[x][y][z]);
  x=3; y=2; z=1; printf("%d\n", foo[x][y][z]);

  // Fill the array with values
  foreach x (0 ... 6) {
    foreach y (0 ... 4) {
      foreach z (0 ... 3) {
	foo[x][y][z]= cnt; cnt++;
      }
    }
  }

  // Print the values in reverse order
  for (x=6; x>=0; x--) {
    for (y=4; y>=0; y--) {
      for (z=3; z>=0; z--) {
	printf("%d\n", foo[x][y][z]);
      }
    }
  }
}
