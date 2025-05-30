#include <stdio.ah>

       int32 fred;
public int32 jim;
extern int32 mary;

public int foo(void) {
  return(7);
}

void main(void) {
  fred= 3; jim= 4;
  printf("fred is %d and jim is %d\n", fred, jim);
  printf("foo() returns %d\n", foo());
}
