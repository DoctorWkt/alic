#include <stdio.ah>

public void main(void) {
  uint32 mary= 23;
  uint16 fred= 7;
  int32 foo= 33;
  int16 bar= 7;
  mary = mary + fred;
  printf("%d\n", mary);
  foo = bar + foo;
  printf("%d\n", foo);
}
