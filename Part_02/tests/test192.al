#include <stdio.ah>

type foo = int8 *;

public void main(void) {
  foo fred = "hello";
  char *mary;
  printf("%s\n", fred);
  mary= fred;
  printf("%s\n", mary);
  fred++;
  printf("%s\n", fred);
}
