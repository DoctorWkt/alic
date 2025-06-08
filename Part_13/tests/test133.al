#include <stdio.ah>

public void main(void) {
  uint32 fred;
  int32 mary= -5;

  fred= unsigned(mary);
  printf("fred is %u\n", fred);
}
