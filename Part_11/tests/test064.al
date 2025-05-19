#include <stdio.ah>

int32 fred;
int16 jim;

void main(void) {
  int32 dave= 23;
  int16 mary= 11;

  printf("Hello world %d %d %d %d\n", fred, jim, dave, mary);

  fred= 3; jim= 4; dave= 5; mary= 6;
  printf("Hello world %d %d %d %d\n", fred, jim, dave, mary);
}
