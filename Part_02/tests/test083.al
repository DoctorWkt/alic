#include <stdio.ah>
#include <stdlib.ah>

public void main(void) {
  int8 *ptr;

  ptr= "Hello there\n";
  printf("%c", *ptr); ptr= ptr + 1;
  printf("%c", *ptr); ptr= ptr + 1;
  printf("%c", *ptr); ptr= ptr + 1;
  printf("%c", *ptr); ptr= ptr + 1;
  printf("%c", *ptr); ptr= ptr + 1;
  printf("%c\n", *ptr);
}
