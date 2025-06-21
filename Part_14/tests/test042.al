#include <stdio.ah>

public void main(void) {
  int32 fred= 5;
  int32 mary= 0;
  int32 *jim= NULL;

  jim= &fred;
  mary= *jim;
  printf("fred is %d\n", fred);
  printf("mary is %d\n", mary);
}
