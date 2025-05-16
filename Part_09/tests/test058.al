#include <stdio.h>

void main(void) {
  int32 a= 4;
  int32 b= 3;

  if (a > b) {
    // We now have nested local scopes
    int32 c = 100;
    printf("We have a %d b %d c %d\n", a, b, c);
  }

  // c should no longer exist
  printf("We have a %d b %d c %d\n", a, b, c);
}
