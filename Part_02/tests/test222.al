#include <stdio.ah>

enum { five=5, six=6, ten=10, twenty=20 };

int32 fred[ six ] = { 1, 2, 3, 4, 5, 6 };

type FOO = int32 range five ... twenty;

public void main(void) {
  FOO jim;
  int32 x;

  jim = five;
  printf("jim is %d\n", jim);

  foreach x (fred) { printf("%d\n", x); }
}
