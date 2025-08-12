#include <stdio.ah>
#include <stdlib.ah>

public void main(void) {
  int32 **base;
  int32 i;
  int32 j;

  base= calloc(5, sizeof(int32 *));
  foreach i (0 ... 4)
    base[i]= calloc(5, sizeof(int32));

  base[3][2]= 45; base[4][1]= 77;
  base[0][3]= 11; base[1][4]= 88;

  foreach i (0 ... 4) {
    foreach j (0 ... 4)
      printf("%2d ", base[i][j]);
    printf("\n");
  }
}
