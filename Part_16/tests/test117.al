#include <stdio.ah>

public void main(void) {
  int32 fred[5];
  int32 i;

  fred[0]= 10;
  fred[1]= 20;
  fred[2]= 30;
  fred[3]= 40;
  fred[4]= 50;

  for (i=0; i < 10; i++)
    printf("%d\n", fred[i]);
}
