#include <stdio.ah>


public void main(void) {
  int32 fred[char *];
  int32 x;
  bool see;

  printf("hi\n");
  fred["hello"]= 5;
  x= fred["hello"];
  printf("x is %d\n", x);
  see= exists(fred["hello"]);
  printf("see is %d\n", see);
  undef(fred["hello"]);
  see= exists(fred["hello"]);
  printf("see is %d\n", see);
}
