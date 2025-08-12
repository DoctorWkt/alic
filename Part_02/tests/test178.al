#include <stdio.ah>

int32 bar(int8 *name, bool is_iregular);

int32 foo(int8 *name, bool is_iregular) {
  printf("%s %d\n", name, is_iregular);
  return(1);
}

// Function pointer type
type footype= funcptr int32(int8 *, bool);


public void main(void) {
  int32 x;

  // Function pointer
  footype fred;

  fred= foo;

  x= foo("bill", false); printf("x is %d\n", x);
  x= fred("mary", true); printf("x is %d\n", x);
}
