#include <stdio.ah>

void main(void) {
  int16 fred= 5;
  int16 mary= 0;
  int16 *jim= NULL;

  jim= &fred;
  mary= *jim;
  printf("fred is %d\n", fred);
  printf("mary is %d\n", mary);
  *jim= 17;
  printf("fred is now %d\n", fred);
}
