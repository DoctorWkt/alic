#include <stdio.ah>

int32 bar(int8 *name, bool is_iregular);

int32 foo(int8 *name, bool is_iregular) {
  printf("Name %s flag %d\n", name, is_iregular);
  return(1);
}

// Function pointer
int32 fred *(int8 *name, bool is_iregular);

public void main(void) {
  fred= foo;

  foo("bill", false);
  fred("mary", true);
}
