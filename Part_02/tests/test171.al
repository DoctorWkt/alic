#include <stdio.ah>

public void main(void) {
  int32 x;
  int32 y;
  int32 z;

  for ({ x= 1; y= 3; z= 5;}; x < 10; {x++; y= y+2; z= z+3;})
    printf("%d\n", x + y + z);
}
