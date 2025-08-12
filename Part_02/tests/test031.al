#include <stdio.ah>

void fred(int32 x, int8 y);

void fred(int32 x, int16 y) {
  printf("fred has argument x=%d\n", x);
}
