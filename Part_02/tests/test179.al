#include <stdio.ah>

int32 bar(int8 *name, bool is_iregular);

int32 foo(int8 *name, bool is_iregular) {
  printf("%s %d\n", name, is_iregular);
  return(1);
}

type footype= funcptr int32(int8 *, bool);

public void main(void) {
  int32 x;

  // Function pointer and assignment
  footype fred = foo;

  x= fred("mary", true); printf("x is %d\n", x);
  foo("bill", false);
}
