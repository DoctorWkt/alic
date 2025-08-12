#include <stdio.ah>

public void main(void) {
  bool a = true;
  bool b = false;
  flt32 d = a + b;
  int32 c = a - b;

  printf("%d\n", c);
  printf("%f\n", d);
}
