#include <stdio.ah>

public int32 fred(const int32 x) {
  x = x + 1;
  return(x);
}

public void main(void) {
  int32 result;
  result= fred(5);
}
